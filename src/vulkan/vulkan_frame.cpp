#include "vulkan_engine.hpp"

#include <stdexcept>

namespace engine {

Frame VulkanContext::beginFrame() {
    if (framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
        return {};
    }

    const VkFence fence = inFlight_[currentFrame_];
    vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
    Frame frame{commandBuffers_[currentFrame_], 0, currentFrame_};
    const VkResult result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, imageAvailable_[currentFrame_], VK_NULL_HANDLE, &frame.imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return {};
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    if (imagesInFlight_[frame.imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &imagesInFlight_[frame.imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[frame.imageIndex] = fence;
    vkResetFences(device_, 1, &fence);
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

void VulkanContext::endFrame(const Frame& frame) {
    if (frame.commandBuffer == VK_NULL_HANDLE) {
        return;
    }
    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to end command buffer");
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailable_[frame.frameIndex];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_[frame.imageIndex];
    if (vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_[frame.frameIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit command buffer");
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[frame.imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &frame.imageIndex;
    const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swapchain image");
    }
    currentFrame_ = (currentFrame_ + 1) % maxFramesInFlight;
}

} // namespace engine
