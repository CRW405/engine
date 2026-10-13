#include "glfw/glfw_engine.hpp"
#include "vulkan/vulkan_engine.hpp"

#include <cstdio>
#include <exception>
#include <memory>
#include <vector>

int main() {
	try {
		// basic main setup
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanContext vulkan(window);

		// testing multiple windows
		std::unique_ptr<engine::Window>        window2 = std::make_unique<engine::Window>(engine::WindowConfig{ 250, 250, "Window 2", true });
		std::unique_ptr<engine::VulkanContext> vulkan2 = std::make_unique<engine::VulkanContext>(*window2);

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}
			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer != VK_NULL_HANDLE) {
				vulkan.endFrame(frame);
			}

			// testing multiple windows
			if (window2 && vulkan2) {
				if (!window2->shouldClose()) {
					window2->pollEvents();
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
