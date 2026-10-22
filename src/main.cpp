#include "glfw/glfw_engine.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/vertex_buffer.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

int vulkan_triangle();
int sierpinski_triangle(int n);

int main() {
	try {
		// return vulkan_triangle();
		return sierpinski_triangle(1);
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}
}

struct simple_vertex {
	float position[2];
	float color[3];
};

int vulkan_triangle() {
	try {
		/*
		 * Step 1: create the process, window, Vulkan device, and presentation
		 * context. These objects are declared in dependency order so teardown
		 * happens in the reverse order automatically.
		 */
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanDevice  device(window.handle(), "Engine");
		engine::VulkanContext vulkan(device, &window);

		/*
		 * Step 2: describe how simple_vertex is laid out in the vertex buffer.
		 * The shader locations must match these attribute descriptions.
		 */
		engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding   = 0;
		bindingDescription.stride    = sizeof(simple_vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
		attributeDescriptions[0].binding  = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset   = offsetof(simple_vertex, position);

		attributeDescriptions[1].binding  = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset   = offsetof(simple_vertex, color);

		pipelineConfig.vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineConfig.vertexInputInfo.vertexBindingDescriptionCount   = 1;
		pipelineConfig.vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
		pipelineConfig.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		pipelineConfig.vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();
		pipelineConfig.renderPass                                      = vulkan.renderPass();

		/*
		 * Step 3: create the pipeline and upload the triangle data. The pipeline
		 * owns its shader modules and default layout after construction.
		 */
		engine::Pipeline pipeline(device, "shaders/vulkan_triangle.vert.spv", "shaders/vulkan_triangle.frag.spv", pipelineConfig);

		std::vector<simple_vertex> vulkan_triangle = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.5f },   { 0.0f, 0.0f, 1.0f } }
		};
		engine::VertexBuffer vertex_buffer(device, vulkan_triangle.data(), sizeof(simple_vertex) * vulkan_triangle.size());
		std::printf("Vertex buffer created with size: %zu bytes\n", sizeof(simple_vertex) * vulkan_triangle.size());

		/*
		 * Step 4: acquire, record, submit, and present one frame at a time.
		 * beginFrame returns an empty frame when the swapchain was recreated.
		 */
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

			/*
			 * Begin the render pass against the image acquired for this frame.
			 * The swapchain extent is the framebuffer size Vulkan is actually using.
			 */
			VkClearValue clearColor = { { { 1.0f, 1.0f, 1.0f, 1.0f } } };

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass        = vulkan.renderPass();
			renderPassInfo.framebuffer       = vulkan.framebuffer(frame.imageIndex);
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = vulkan.extent();
			renderPassInfo.clearValueCount   = 1;
			renderPassInfo.pClearValues      = &clearColor;

			vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			/* Bind state first, then issue the draw commands that use it. */
			pipeline.bind(frame.commandBuffer);

			VkViewport viewport{};
			viewport.x        = 0.0f;
			viewport.y        = 0.0f;
			viewport.width    = static_cast<float>(vulkan.extent().width);
			viewport.height   = static_cast<float>(vulkan.extent().height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = vulkan.extent();
			vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

			VkBuffer     vertexBuffers[] = { vertex_buffer.buffer() };
			VkDeviceSize offsets[]       = { 0 };
			vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

			vkCmdEndRenderPass(frame.commandBuffer);

			vulkan.endFrame(frame);

			/* Report the number of frames presented over the last second. */
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

		/* Leave the cursor on a fresh line once the loop exits. */
		std::printf("\n");
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}

int sierpinski_triangle(int n) {
	try {
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanDevice  device(window.handle(), "Engine");
		engine::VulkanContext vulkan(device, &window);

		engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

		// set up the pipeline to take n

		engine::Pipeline pipeline(device, "shaders/sierpinski_triangle.vert.spv", "shaders/sierpinski_triangle.frag.spv", pipelineConfig);

		// set up the pipeline to take n

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			VkClearValue clearColor = { { { 1.0f, 1.0f, 1.0f, 1.0f } } };

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass        = vulkan.renderPass();
			renderPassInfo.framebuffer       = vulkan.framebuffer(frame.imageIndex);
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = vulkan.extent();
			renderPassInfo.clearValueCount   = 1;
			renderPassInfo.pClearValues      = &clearColor;

			vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			pipeline.bind(frame.commandBuffer);

			VkViewport viewport{};
			viewport.x        = 0.0f;
			viewport.y        = 0.0f;
			viewport.width    = static_cast<float>(vulkan.extent().width);
			viewport.height   = static_cast<float>(vulkan.extent().height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = vulkan.extent();
			vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

			// pass n

			vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

			vkCmdEndRenderPass(frame.commandBuffer);

			vulkan.endFrame(frame);
		}
	}

	std::printf("\n");
}
catch (const std::exception &error) {
	std::fprintf(stderr, "Fatal error: %s\n", error.what());
	return 1;
}
return 0;
}
