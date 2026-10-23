#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <vector>

#include "glfw/glfw_engine.hpp"
#include "vulkan_device.hpp"

namespace engine {

constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;

struct Frame {
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	std::uint32_t   imageIndex    = 0;
	std::uint32_t   frameIndex    = 0;
};

// ============================================================================
// Window-owned Vulkan rendering resources
//
// Owns the presentation resources for one window. The device is shared, while
// the surface, swapchain, render pass, framebuffers, and frame synchronization
// belong to this context.
// ============================================================================
class VulkanContext {
  public:
	explicit VulkanContext(VulkanDevice &device, Window *window);
	~VulkanContext();

	VulkanContext(const VulkanContext &)            = delete;
	VulkanContext &operator=(const VulkanContext &) = delete;
	VulkanContext(VulkanContext &&)                 = delete;
	VulkanContext &operator=(VulkanContext &&)      = delete;

	[[nodiscard]] VkRenderPass                    renderPass() const noexcept { return render_pass_; }
	[[nodiscard]] VkExtent2D                      extent() const noexcept { return swapchain_extent_; }
	[[nodiscard]] VkFormat                        swapchainFormat() const noexcept { return swapchain_format_; }
	[[nodiscard]] const std::vector<VkImage>     &swapchainImages() const noexcept { return swapchain_images_; }
	[[nodiscard]] const std::vector<VkImageView> &swapchainImageViews() const noexcept {
		return swapchain_image_views_;
	}
	[[nodiscard]] VkFramebuffer framebuffer(std::uint32_t index) const;

	[[nodiscard]] Frame beginFrame();
	void                endFrame(const Frame &frame);
	void                beginRenderPass(const Frame &frame, VkClearValue clearValue);
	void                endRenderPass(const Frame &frame);
	void                setViewportAndScissor(VkCommandBuffer commandBuffer) const;
	void                recreateSwapchain();
	void                notifyFramebufferResized() { framebuffer_resized_ = true; }

  private:
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR        capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR>   presentModes;
	};

	void createSurface();
	void createSwapchain();
	void createRenderPass();
	void createFramebuffers();
	void createCommandResources();

	void destroySwapchain(bool destroy_render_pass = true);
	void destroyWindowResources();

	void waitForValidFramebufferSize() const;

	[[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;

	VulkanDevice &device_;
	Window       *window_;

	VkSurfaceKHR                                      surface_          = VK_NULL_HANDLE;
	VkSwapchainKHR                                    swapchain_        = VK_NULL_HANDLE;
	VkFormat                                          swapchain_format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D                                        swapchain_extent_{};
	std::vector<VkImage>                              swapchain_images_;
	std::vector<VkImageView>                          swapchain_image_views_;
	std::vector<VkFramebuffer>                        swapchain_framebuffers_;
	VkRenderPass                                      render_pass_  = VK_NULL_HANDLE;
	VkCommandPool                                     command_pool_ = VK_NULL_HANDLE;
	std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> command_buffers_{};
	std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     image_available_{};
	std::vector<VkSemaphore>                          render_finished_;
	std::array<VkFence, MAX_FRAMES_IN_FLIGHT>         in_flight_{};
	std::vector<VkFence>                              images_in_flight_;

	std::uint32_t current_frame_       = 0;
	bool          framebuffer_resized_ = false;
};

}
