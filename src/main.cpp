#include "glfw/glfw_engine.hpp"
#include "vulkan/vertex_buffer.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cstdio>
#include <exception>
#include <memory>
#include <vector>

struct basic_vertex {
	float position[2];
	float color[3];
};

int main() {
	try {
		// main basic setup
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanDevice  device(window.handle(), "Engine");
		engine::VulkanContext vulkan(device, &window);

		// test creating a second window and Vulkan context
		std::unique_ptr<engine::Window> window2 =
		    std::make_unique<engine::Window>(engine::WindowConfig{ 250, 250, "Window 2", true });
		std::unique_ptr<engine::VulkanContext> vulkan2 =
		    std::make_unique<engine::VulkanContext>(device, window2.get());

		// testing vertex buffer
		std::vector<basic_vertex> vulkan_triangle = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.5f },   { 0.0f, 0.0f, 1.0f } }
		};
		engine::VertexBuffer vertex_buffer(device, vulkan_triangle.data(), sizeof(basic_vertex) * vulkan_triangle.size());
		std::printf("Vertex buffer created with size: %zu bytes\n", vertex_buffer.size());

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}
			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer != VK_NULL_HANDLE) {
				vulkan.endFrame(frame);
			}

			if (window2 && vulkan2) {
				if (!window2->shouldClose()) {
					if (window2->keyPressed(GLFW_KEY_ESCAPE) || window2->keyPressed(GLFW_KEY_Q)) {
						glfwSetWindowShouldClose(window2->handle(), GLFW_TRUE);
					}
					const engine::Frame frame2 = vulkan2->beginFrame();
					if (frame2.commandBuffer != VK_NULL_HANDLE) {
						vulkan2->endFrame(frame2);
					}
				} else {
					vulkan2.reset();
					window2.reset();
				}
			}
		}
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}
