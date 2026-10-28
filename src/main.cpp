#include "glfw/glfw_engine.hpp"
#include "tools/fps.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/image_texture.hpp"
#include "vulkan/pipeline_layout.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/texture_descriptor.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cglm/cglm.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <vector>

int vulkan_triangle();
int sierpinski_triangle();
int image_texture_example();

int main(int argc, char *argv[]) {
	int example = 0;
	if (argc > 1) {
		std::string arg1 = argv[1];
		if (arg1 == "all") {
			vulkan_triangle();
			sierpinski_triangle();
			image_texture_example();
			return 0;
		}
	}
	example = std::atoi(argv[1]);
	switch (example) {
	case 0:
		return vulkan_triangle();
	case 1:
		return sierpinski_triangle();
	case 2:
		return image_texture_example();
	default:
		std::fprintf(stderr, "Unknown program %d\n", example);
		return 1;
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
		tools::FPSCounter fps(window.time());

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
			vulkan.beginRenderPass(frame, { 1.0f, 1.0f, 1.0f, 1.0f });

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

			fps.update(window.time());
		}

		// Leave the cursor on a fresh line once the loop exits.
		fps.finish();
		device.waitIdle(); // Wait for the device to finish before tearing down the context and window
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}

struct Camera {
	int   recursion_depth;
	float zoom;
	float aspect_ratio;
};
int sierpinski_triangle() {
	// setup
	engine::GLFWContext   glfw;
	engine::Window        window({ 500, 500, "Sierpinski Triangle", true });
	engine::VulkanDevice  device(window.handle(), "Engine");
	engine::VulkanContext vulkan(device, &window);

	// create pipeline
	engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

	VkPushConstantRange    pushConstantRange{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Camera) };
	engine::PipelineLayout pipelineLayout(device, { pushConstantRange });
	pipelineConfig.pipelineLayout = pipelineLayout.handle();
	pipelineConfig.renderPass     = vulkan.renderPass();
	engine::Pipeline pipeline(device,
	                          "shaders/sierpinski_triangle.vert.spv",
	                          "shaders/sierpinski_triangle.frag.spv",
	                          pipelineConfig);

	// "globals"
	Camera camera{ 0, 1.0f, 1.0f };

	// debug tools
	tools::FPSCounter fps(window.time());

	// main loop
	while (!window.shouldClose()) {
		window.pollEvents();

		// input
		if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
			glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
		}
		if (window.keyPressed(GLFW_KEY_RIGHT)) {
			camera.recursion_depth++;
			std::printf("Recursion depth: %d\n", camera.recursion_depth);
			std::printf("Number of triangles: %lld\n", (long long)pow(3, camera.recursion_depth));
		}
		if (window.keyPressed(GLFW_KEY_LEFT) && camera.recursion_depth > 0) {
			camera.recursion_depth--;
			std::printf("Recursion depth: %d\n", camera.recursion_depth);
			std::printf("Number of triangles: %lld\n", (long long)pow(3, camera.recursion_depth));
		}

		const engine::MousePosition scroll = window.consumeScrollDelta();
		camera.zoom                        = camera.zoom * std::exp(static_cast<float>(scroll.y) * 0.1f);

		// create frame
		const engine::Frame frame = vulkan.beginFrame();
		if (frame.commandBuffer == VK_NULL_HANDLE) {
			continue;
		}
		camera.aspect_ratio = static_cast<float>(vulkan.extent().width) /
		                      static_cast<float>(vulkan.extent().height);
		vulkan.beginRenderPass(frame, { 1.0f, 1.0f, 1.0f, 1.0f });

		// setup draw
		pipeline.bind(frame.commandBuffer);
		vulkan.setViewportAndScissor(frame.commandBuffer);

		// push constant data
		vkCmdPushConstants(
		    frame.commandBuffer,
		    pipelineLayout.handle(),
		    VK_SHADER_STAGE_FRAGMENT_BIT,
		    0,
		    sizeof(camera),
		    &camera);

		// draw
		vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

		// end frame
		vulkan.endRenderPass(frame);
		vulkan.endFrame(frame);
		fps.update(window.time());
	}
	fps.finish();
	device.waitIdle();
	return 0;
}

struct texture_vertex {
	float position[2];
	float color[3];
	float uv[2]; // a uv is a 2D coordinate that maps a point on the texture to a point on the mesh
};
int image_texture_example() {
	std::printf("Running Image Texture Example\n");
	try {
		std::string           app_name = "Image Texture Example";
		engine::GLFWContext   glfw;
		engine::Window        window({ 800, 600, app_name, true });
		engine::VulkanDevice  device(window.handle(), app_name);
		engine::VulkanContext vulkan(device, &window);

		// set up texture and the default sampled-image descriptor set
		engine::ImageTexture         texture(device, "../assets/smiley_0.png");
		engine::TextureDescriptorSet textureDescriptor(device, texture);

		// pipeline setup
		engine::PipelineLayout     pipeline_layout(device, { textureDescriptor.layout() }, {});
		engine::PipelineConfigInfo pipeline_Config = engine::Pipeline::defaultPipelineConfigInfo();

		// vertex input description
		engine::VertexInputDescription vertexInput;
		vertexInput.addBinding(0, sizeof(texture_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(texture_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(texture_vertex, color))
		    .addAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(texture_vertex, uv));
		pipeline_Config.setVertexInput(vertexInput)
		    .setRenderPass(vulkan.renderPass())
		    .setPipelineLayout(pipeline_layout.handle());

		engine::Pipeline pipeline(device,
		                          "shaders/simple_texture.vert.spv",
		                          "shaders/simple_texture.frag.spv",
		                          pipeline_Config);

		std::vector<texture_vertex> vertices = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 0.5f, 0.5f },   { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -0.5f, 0.5f },  { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }
		};
		const std::vector<std::uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
		engine::VertexBuffer             vertex_buffer(device, vertices.data(), sizeof(texture_vertex) * vertices.size());
		engine::IndexBuffer              index_buffer(device, indices.data(), sizeof(std::uint16_t) * indices.size(), VK_INDEX_TYPE_UINT16);

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			vulkan.beginRenderPass(frame, { 0.0f, 0.0f, 0.0f, 0.0f });

			vertex_buffer.bind(frame.commandBuffer);
			index_buffer.bind(frame.commandBuffer);

			pipeline_layout.bindDescriptorSet(frame.commandBuffer, textureDescriptor.descriptorSet());

			pipeline.bind(frame.commandBuffer);
			vulkan.setViewportAndScissor(frame.commandBuffer);

			vkCmdDrawIndexed(frame.commandBuffer,
			                 static_cast<std::uint32_t>(indices.size()),
			                 1,
			                 0,
			                 0,
			                 0);

			vulkan.endRenderPass(frame);
			vulkan.endFrame(frame);
		}
		device.waitIdle();

	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
	}
	return 0;
}
