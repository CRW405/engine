#include "glfw/glfw_engine.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/vertex_buffer.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cstdio>
#include <exception>
#include <memory>
#include <vector>

struct simple_vertex {
	float position[2];
	float color[3];
};

int main() {
	try {
		// main basic setup
		engine::GLFWContext        glfw;
		engine::Window             window({ 500, 500, "Engine", true });
		engine::VulkanDevice       device(window.handle(), "Engine");
		engine::VulkanContext      vulkan(device, &window);
		engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo(500, 500);
		engine::Pipeline           pipeline(device, "shaders/simple.vert.spv", "shaders/simple.frag.spv", pipelineConfig);

		// // test creating a second window and Vulkan context
		// std::unique_ptr<engine::Window> window2 =
		//     std::make_unique<engine::Window>(engine::WindowConfig{ 250, 250, "Window 2", true });
		// std::unique_ptr<engine::VulkanContext> vulkan2 =
		//     std::make_unique<engine::VulkanContext>(device, window2.get());

		// testing vertex buffer
		std::vector<simple_vertex> vulkan_triangle = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.5f },   { 0.0f, 0.0f, 1.0f } }
		};
		engine::VertexBuffer vertex_buffer(device, vulkan_triangle.data(), sizeof(simple_vertex) * vulkan_triangle.size());
		std::printf("Vertex buffer created with size: %zu bytes\n", sizeof(simple_vertex) * vulkan_triangle.size());

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}
			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			pipeline.bind(frame.commandBuffer);
			VkViewport viewport{};
			viewport.x        = 0.0f;
			viewport.y        = 0.0f;
			viewport.width    = static_cast<float>(window.width());
			viewport.height   = static_cast<float>(window.height());
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = { static_cast<uint32_t>(window.width()), static_cast<uint32_t>(window.height()) };
			vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

			VkBuffer     vertexBuffers[] = { vertex_buffer.buffer() };
			VkDeviceSize offsets[]       = { 0 };
			vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

			vulkan.endFrame(frame);

			// if (window2 && vulkan2) {
			// 	if (!window2->shouldClose()) {
			// 		if (window2->keyPressed(GLFW_KEY_ESCAPE) || window2->keyPressed(GLFW_KEY_Q)) {
			// 			glfwSetWindowShouldClose(window2->handle(), GLFW_TRUE);
			// 		}
			// 		const engine::Frame frame2 = vulkan2->beginFrame();
			// 		if (frame2.commandBuffer != VK_NULL_HANDLE) {
			// 			vulkan2->endFrame(frame2);
			// 		}
			// 	} else {
			// 		vulkan2.reset();
			// 		window2.reset();
			// 	}
			// }
		}
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}
