#include "vulkan_engine.hpp"

#include <stdexcept>

namespace engine {

void VulkanContext::createCommandResources() {
    const QueueFamilies queues = findQueueFamilies(physicalDevice_);
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = queues.graphics;
    if (vkCreateCommandPool(device_, &pool, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool");
    }

    VkCommandBufferAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = commandPool_;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = maxFramesInFlight;
    if (vkAllocateCommandBuffers(device_, &allocation, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers");
    }

    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    imagesInFlight_.resize(swapchainImages_.size(), VK_NULL_HANDLE);

    for (std::uint32_t index = 0; index < maxFramesInFlight; ++index) {
        if (vkCreateSemaphore(device_, &semaphore, nullptr, &imageAvailable_[index]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence, nullptr, &inFlight_[index]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create frame synchronization resources");
        }
    }

    renderFinished_.resize(swapchainImages_.size());
    for (VkSemaphore& signal : renderFinished_) {
        if (vkCreateSemaphore(device_, &semaphore, nullptr, &signal) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render-finished semaphore");
        }
    }
}

void VulkanContext::destroyDeviceResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(device_);
    for (VkSemaphore semaphore : renderFinished_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    for (VkSemaphore semaphore : imageAvailable_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    for (VkFence fence : inFlight_) {
        vkDestroyFence(device_, fence, nullptr);
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, commandPool_, maxFramesInFlight, commandBuffers_.data());
        vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
    destroySwapchain();
    vkDestroyDevice(device_, nullptr);
}

} // namespace engine
