#include "vulkan_engine.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace engine {

VulkanContext::SwapchainSupport VulkanContext::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport result{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &result.capabilities);

    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr);
    result.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, result.formats.data());

    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr);
    result.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, result.presentModes.data());
    return result;
}

void VulkanContext::createSwapchain() {
    const QueueFamilies queues = findQueueFamilies(physicalDevice_);
    const SwapchainSupport support = querySwapchainSupport(physicalDevice_);
    if (support.formats.empty() || support.presentModes.empty()) {
        throw std::runtime_error("surface does not support a usable swapchain");
    }

    const auto preferredFormat = std::find_if(
        support.formats.begin(), support.formats.end(), [](const auto& format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                   format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
    const VkSurfaceFormatKHR surfaceFormat =
        preferredFormat == support.formats.end() ? support.formats.front() : *preferredFormat;
    const VkPresentModeKHR presentMode =
        std::find(support.presentModes.begin(), support.presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR)
            != support.presentModes.end() ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;

    if (support.capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        swapchainExtent_ = support.capabilities.currentExtent;
    } else {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_.handle(), &width, &height);
        swapchainExtent_ = {
            std::clamp(static_cast<std::uint32_t>(std::max(width, 0)),
                       support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width),
            std::clamp(static_cast<std::uint32_t>(std::max(height, 0)),
                       support.capabilities.minImageExtent.height, support.capabilities.maxImageExtent.height)};
    }

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);
    }

    const bool separateQueues = queues.graphics != queues.present;
    const std::uint32_t families[] = {queues.graphics, queues.present};
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = swapchainExtent_;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = separateQueues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = separateQueues ? 2 : 0;
    info.pQueueFamilyIndices = separateQueues ? families : nullptr;
    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainFormat_ = surfaceFormat.format;
    swapchainImageViews_.resize(imageCount);
    for (std::uint32_t index = 0; index < imageCount; ++index) {
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = swapchainImages_[index];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = swapchainFormat_;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &view, nullptr, &swapchainImageViews_[index]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swapchain image view");
        }
    }
}

void VulkanContext::destroySwapchain() {
    for (VkImageView view : swapchainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::waitForValidFramebufferSize() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_.handle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_.handle(), &width, &height);
        glfwWaitEvents();
    }
}

void VulkanContext::recreateSwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    waitForValidFramebufferSize();
    vkDeviceWaitIdle(device_);
    destroySwapchain();
    createSwapchain();
    imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);

    for (VkSemaphore semaphore : renderFinished_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    renderFinished_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (VkSemaphore& semaphore : renderFinished_) {
        if (vkCreateSemaphore(device_, &info, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to recreate render-finished semaphore");
        }
    }
}

} // namespace engine
