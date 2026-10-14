#include "vulkan_context.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace engine {

void framebufferResizeCallback(GLFWwindow *window, int, int) {
	auto *context = static_cast<VulkanContext *>(glfwGetWindowUserPointer(window));
	if (context != nullptr) {
		context->notifyFramebufferResized();
	}
}

VulkanContext::VulkanContext(VulkanDevice &device, Window *window) : device_(device), window_(window) {
	if (window_ == nullptr || window_->handle() == nullptr) {
		throw std::invalid_argument("VulkanContext requires a valid window");
	}

	glfwSetWindowUserPointer(window_->handle(), this);
	glfwSetFramebufferSizeCallback(window_->handle(), framebufferResizeCallback);
	createSurface();
	createSwapchain();
	createCommandResources();
}

VulkanContext::~VulkanContext() {
	destroyWindowResources();
	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(device_.instance(), surface_, nullptr);
	}
	if (window_ != nullptr && window_->handle() != nullptr &&
	    glfwGetWindowUserPointer(window_->handle()) == this) {
		glfwSetWindowUserPointer(window_->handle(), nullptr);
		glfwSetFramebufferSizeCallback(window_->handle(), nullptr);
	}
}

void VulkanContext::createSurface() {
	if (glfwCreateWindowSurface(device_.instance(), window_->handle(), nullptr, &surface_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
}

VulkanContext::SwapchainSupportDetails VulkanContext::querySwapchainSupport(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface) const {
	SwapchainSupportDetails result{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &result.capabilities);

	std::uint32_t count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr);
	result.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, result.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr);
	result.presentModes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, result.presentModes.data());
	return result;
}

void VulkanContext::createSwapchain() {
	const QueueFamilies queues = device_.findQueueFamilies(device_.physicalDevice(), surface_);
	if (!queues.hasGraphics || !queues.hasPresent) {
		throw std::runtime_error("Vulkan device cannot present to this window surface");
	}
	const SwapchainSupportDetails support = querySwapchainSupport(device_.physicalDevice(), surface_);
	if (support.formats.empty() || support.presentModes.empty()) {
		throw std::runtime_error("surface does not support a usable swapchain");
	}

	const auto preferred_format = std::find_if(
	    support.formats.begin(), support.formats.end(), [](const VkSurfaceFormatKHR &format) {
		    return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
		           format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	    });
	const VkSurfaceFormatKHR surface_format =
	    preferred_format == support.formats.end() ? support.formats.front() : *preferred_format;
	const VkPresentModeKHR present_mode =
	    std::find(support.presentModes.begin(), support.presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) !=
	            support.presentModes.end()
	        ? VK_PRESENT_MODE_MAILBOX_KHR
	        : VK_PRESENT_MODE_FIFO_KHR;

	if (support.capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
		swapchain_extent_ = support.capabilities.currentExtent;
	} else {
		int width  = 0;
		int height = 0;
		glfwGetFramebufferSize(window_->handle(), &width, &height);
		swapchain_extent_ = {
			std::clamp(static_cast<std::uint32_t>(std::max(width, 0)), support.capabilities.minImageExtent.width,
			           support.capabilities.maxImageExtent.width),
			std::clamp(static_cast<std::uint32_t>(std::max(height, 0)), support.capabilities.minImageExtent.height,
			           support.capabilities.maxImageExtent.height)
		};
	}

	std::uint32_t image_count = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0) {
		image_count = std::min(image_count, support.capabilities.maxImageCount);
	}

	const bool               separate_queues  = queues.graphics != queues.present;
	const std::uint32_t      queue_families[] = { queues.graphics, queues.present };
	VkSwapchainCreateInfoKHR info{};
	info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface               = surface_;
	info.minImageCount         = image_count;
	info.imageFormat           = surface_format.format;
	info.imageColorSpace       = surface_format.colorSpace;
	info.imageExtent           = swapchain_extent_;
	info.imageArrayLayers      = 1;
	info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode      = separate_queues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = separate_queues ? 2 : 0;
	info.pQueueFamilyIndices   = separate_queues ? queue_families : nullptr;
	info.preTransform          = support.capabilities.currentTransform;
	info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode           = present_mode;
	info.clipped               = VK_TRUE;
	if (vkCreateSwapchainKHR(device_.device(), &info, nullptr, &swapchain_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain");
	}

	vkGetSwapchainImagesKHR(device_.device(), swapchain_, &image_count, nullptr);
	swapchain_images_.resize(image_count);
	vkGetSwapchainImagesKHR(device_.device(), swapchain_, &image_count, swapchain_images_.data());
	swapchain_format_ = surface_format.format;
	swapchain_image_views_.resize(image_count);

	for (std::uint32_t index = 0; index < image_count; ++index) {
		VkImageViewCreateInfo view{};
		view.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image                       = swapchain_images_[index];
		view.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
		view.format                      = swapchain_format_;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device_.device(), &view, nullptr, &swapchain_image_views_[index]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swapchain image view");
		}
	}
}

void VulkanContext::createCommandResources() {
	const QueueFamilies     queues = device_.findQueueFamilies(device_.physicalDevice(), surface_);
	VkCommandPoolCreateInfo pool{};
	pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = queues.graphics;
	if (vkCreateCommandPool(device_.device(), &pool, nullptr, &command_pool_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool");
	}

	VkCommandBufferAllocateInfo allocation{};
	allocation.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocation.commandPool        = command_pool_;
	allocation.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocation.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	if (vkAllocateCommandBuffers(device_.device(), &allocation, command_buffers_.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers");
	}

	VkSemaphoreCreateInfo semaphore{};
	semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fence{};
	fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
	render_finished_.resize(swapchain_images_.size());
	for (std::uint32_t index = 0; index < MAX_FRAMES_IN_FLIGHT; ++index) {
		if (vkCreateSemaphore(device_.device(), &semaphore, nullptr, &image_available_[index]) != VK_SUCCESS ||
		    vkCreateFence(device_.device(), &fence, nullptr, &in_flight_[index]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create frame synchronization resources");
		}
	}
	for (VkSemaphore &signal : render_finished_) {
		if (vkCreateSemaphore(device_.device(), &semaphore, nullptr, &signal) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render-finished semaphore");
		}
	}
}

Frame VulkanContext::beginFrame() {
	if (framebuffer_resized_) {
		framebuffer_resized_ = false;
		recreateSwapchain();
		return {};
	}

	const VkFence fence = in_flight_[current_frame_];
	vkWaitForFences(device_.device(), 1, &fence, VK_TRUE, UINT64_MAX);
	Frame          frame{ command_buffers_[current_frame_], 0, current_frame_ };
	const VkResult result = vkAcquireNextImageKHR(
	    device_.device(), swapchain_, UINT64_MAX, image_available_[current_frame_], VK_NULL_HANDLE, &frame.imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return {};
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swapchain image");
	}

	if (images_in_flight_[frame.imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device_.device(), 1, &images_in_flight_[frame.imageIndex], VK_TRUE, UINT64_MAX);
	}
	images_in_flight_[frame.imageIndex] = fence;
	vkResetFences(device_.device(), 1, &fence);
	if (vkResetCommandBuffer(frame.commandBuffer, 0) != VK_SUCCESS) {
		throw std::runtime_error("failed to reset command buffer");
	}

	VkCommandBufferBeginInfo begin{};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(frame.commandBuffer, &begin) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin command buffer");
	}
	return frame;
}

void VulkanContext::endFrame(const Frame &frame) {
	if (frame.commandBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to end command buffer");
	}

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo         submit{};
	submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.waitSemaphoreCount   = 1;
	submit.pWaitSemaphores      = &image_available_[frame.frameIndex];
	submit.pWaitDstStageMask    = &wait_stage;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &frame.commandBuffer;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &render_finished_[frame.imageIndex];
	if (vkQueueSubmit(device_.graphicsQueue(), 1, &submit, in_flight_[frame.frameIndex]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit command buffer");
	}

	VkPresentInfoKHR present{};
	present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores    = &render_finished_[frame.imageIndex];
	present.swapchainCount     = 1;
	present.pSwapchains        = &swapchain_;
	present.pImageIndices      = &frame.imageIndex;
	const VkResult result      = vkQueuePresentKHR(device_.presentQueue(), &present);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapchain();
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swapchain image");
	}
	current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::destroySwapchain() {
	for (VkImageView view : swapchain_image_views_) {
		vkDestroyImageView(device_.device(), view, nullptr);
	}
	swapchain_image_views_.clear();
	swapchain_images_.clear();
	if (swapchain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device_.device(), swapchain_, nullptr);
		swapchain_ = VK_NULL_HANDLE;
	}
}

void VulkanContext::destroyWindowResources() {
	if (device_.device() == VK_NULL_HANDLE) {
		return;
	}
	vkDeviceWaitIdle(device_.device());
	for (VkSemaphore semaphore : render_finished_) {
		vkDestroySemaphore(device_.device(), semaphore, nullptr);
	}
	for (VkSemaphore semaphore : image_available_) {
		vkDestroySemaphore(device_.device(), semaphore, nullptr);
	}
	for (VkFence fence : in_flight_) {
		vkDestroyFence(device_.device(), fence, nullptr);
	}
	if (command_pool_ != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(device_.device(), command_pool_, MAX_FRAMES_IN_FLIGHT, command_buffers_.data());
		vkDestroyCommandPool(device_.device(), command_pool_, nullptr);
	}
	destroySwapchain();
}

void VulkanContext::waitForValidFramebufferSize() const {
	int width  = 0;
	int height = 0;
	glfwGetFramebufferSize(window_->handle(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window_->handle(), &width, &height);
		glfwWaitEvents();
	}
}

void VulkanContext::recreateSwapchain() {
	if (device_.device() == VK_NULL_HANDLE) {
		return;
	}
	waitForValidFramebufferSize();
	vkDeviceWaitIdle(device_.device());
	destroySwapchain();
	createSwapchain();
	images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);

	for (VkSemaphore semaphore : render_finished_) {
		vkDestroySemaphore(device_.device(), semaphore, nullptr);
	}
	render_finished_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
	VkSemaphoreCreateInfo semaphore{};
	semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (VkSemaphore &signal : render_finished_) {
		if (vkCreateSemaphore(device_.device(), &semaphore, nullptr, &signal) != VK_SUCCESS) {
			throw std::runtime_error("failed to recreate render-finished semaphore");
		}
	}
}

} // namespace engine
