#include "glfw/glfw_engine.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/pipeline_layout.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cglm/cglm.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <vector>

int vulkan_triangle();

int main() {
	try {
		return vulkan_triangle();
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}
}

// -----------------------------------------------------------------------------
// Triangle application
//
// Create the engine, record a rotating indexed triangle, and present frames
// until the window is closed.
// -----------------------------------------------------------------------------
struct simple_vertex {
	float position[2];
	float color[3];
};
int vulkan_triangle() {
	std::printf("Running Vulkan Triangle\n");
	try {
		// -------------------------------------------------------------------------
		// Step 1: create the process, window, Vulkan device, and presentation
		// context. These objects are declared in dependency order so teardown
		// happens in the reverse order automatically.
		// -------------------------------------------------------------------------
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanDevice  device(window.handle(), "Engine");
		engine::VulkanContext vulkan(device, &window);

		// -------------------------------------------------------------------------
		// Step 2: describe how simple_vertex is laid out in the vertex buffer.
		// The shader locations must match these attribute descriptions.
		// -------------------------------------------------------------------------
		engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

		engine::VertexInputDescription vertexInput;
		vertexInput.addBinding(0, sizeof(simple_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(simple_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(simple_vertex, color));
		pipelineConfig.setVertexInput(vertexInput);
		pipelineConfig.renderPass = vulkan.renderPass();

		// Push constant setup
		engine::PipelineLayout pipelineLayout(device, {
		                                                  { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4) }
        });
		pipelineConfig.pipelineLayout = pipelineLayout.handle();

		// -------------------------------------------------------------------------
		// Step 3: create the pipeline and upload the triangle data. The pipeline
		// owns its shader modules and default layout after construction.
		// -------------------------------------------------------------------------
		engine::Pipeline pipeline(device, "shaders/vulkan_triangle.vert.spv", "shaders/vulkan_triangle.frag.spv", pipelineConfig);

		std::vector<simple_vertex> vulkan_triangle = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.5f },   { 0.0f, 0.0f, 1.0f } }
		};
		const std::vector<std::uint16_t> triangle_indices = { 0, 1, 2 };
		engine::VertexBuffer             vertex_buffer(device, vulkan_triangle.data(), sizeof(simple_vertex) * vulkan_triangle.size());
		engine::IndexBuffer              index_buffer(
		    device,
		    triangle_indices.data(),
		    sizeof(std::uint16_t) * triangle_indices.size(),
		    VK_INDEX_TYPE_UINT16);
		std::printf("Vertex buffer created with size: %zu bytes\n", sizeof(simple_vertex) * vulkan_triangle.size());

		// -------------------------------------------------------------------------
		// Step 4: acquire, record, submit, and present one frame at a time.
		// beginFrame returns an empty frame when the swapchain was recreated.
		// -------------------------------------------------------------------------
		std::uint64_t framesSinceReport = 0;
		double        fpsReportTime     = window.time();

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			// -------------------------------------------------------------------------
			// Begin the render pass against the image acquired for this frame.
			// The swapchain extent is the framebuffer size Vulkan is actually using.
			// -------------------------------------------------------------------------
			vulkan.beginRenderPass(frame, { { { 1.0f, 1.0f, 1.0f, 1.0f } } });

			// Bind state first, then issue the draw commands that use it.
			pipeline.bind(frame.commandBuffer);

			vulkan.setViewportAndScissor(frame.commandBuffer);
			vertex_buffer.bind(frame.commandBuffer);
			index_buffer.bind(frame.commandBuffer);

			// Push constant data
			mat4  Transform    = GLM_MAT4_IDENTITY_INIT;
			float angle        = static_cast<float>(window.time() * glm_rad(90.0f)); // Rotate 90 degrees per second
			vec3  rotationAxis = { 0.0f, 0.0f, 1.0f };
			glm_rotate(Transform, angle, rotationAxis);
			vkCmdPushConstants(
			    frame.commandBuffer,
			    pipelineLayout.handle(),
			    VK_SHADER_STAGE_VERTEX_BIT,
			    0,
			    sizeof(mat4),
			    Transform);

			vkCmdDrawIndexed(
			    frame.commandBuffer,
			    static_cast<std::uint32_t>(triangle_indices.size()),
			    1,
			    0,
			    0,
			    0);

			vulkan.endRenderPass(frame);

			vulkan.endFrame(frame);

			// Report the number of frames presented over the last second.
			++framesSinceReport;
			const double now     = window.time();
			const double elapsed = now - fpsReportTime;
			if (elapsed >= 1.0) {
				const double fps = static_cast<double>(framesSinceReport) / elapsed;
				std::printf("\r\033[KFPS: %.1f", fps);
				std::fflush(stdout);

				framesSinceReport = 0;
				fpsReportTime     = now;
			}
		}

		// Leave the cursor on a fresh line once the loop exits.
		std::printf("\n");
		device.waitIdle(); // Wait for the device to finish before tearing down the context and window
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}
