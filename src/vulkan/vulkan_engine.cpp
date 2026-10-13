#include "vulkan_engine.hpp"

#include <stdexcept>
#include <utility>

namespace engine {
namespace {

void framebufferResizeCallback(GLFWwindow *window, int, int) {
	auto *context = static_cast<VulkanContext *>(glfwGetWindowUserPointer(window));
	if (context != nullptr) {
		context->notifyFramebufferResized();
	}
}

} // namespace

VulkanContext::VulkanContext(Window &window, std::string applicationName)
    : window_(window), applicationName_(std::move(applicationName)) {
	if (window_.handle() == nullptr) {
		throw std::invalid_argument("VulkanContext requires a valid window");
	}

	glfwSetWindowUserPointer(window_.handle(), this);
	glfwSetFramebufferSizeCallback(window_.handle(), framebufferResizeCallback);
	createInstance(applicationName_);
	createSurface();
	selectPhysicalDevice();
	createDevice();
	createSwapchain();
	createCommandResources();
}

VulkanContext::~VulkanContext() {
	destroyDeviceResources();
	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
	}
	if (instance_ != VK_NULL_HANDLE) {
		vkDestroyInstance(instance_, nullptr);
	}
}

} // namespace engine
