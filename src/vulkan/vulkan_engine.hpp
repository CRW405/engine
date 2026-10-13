#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "glfw/glfw_engine.hpp"

namespace engine {

constexpr std::uint32_t maxFramesInFlight = 2;

struct Frame {
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	std::uint32_t   imageIndex    = 0;
	std::uint32_t   frameIndex    = 0;
};

class VulkanContext {
  public:
	explicit VulkanContext(Window &window, std::string applicationName = "Vulkanc Engine");
	~VulkanContext();

	VulkanContext(const VulkanContext &)            = delete;
	VulkanContext &operator=(const VulkanContext &) = delete;
	VulkanContext(VulkanContext &&)                 = delete;
	VulkanContext &operator=(VulkanContext &&)      = delete;

	[[nodiscard]] VkDevice                    device() const noexcept { return device_; }
	[[nodiscard]] VkPhysicalDevice            physicalDevice() const noexcept { return physicalDevice_; }
	[[nodiscard]] VkRenderPass                renderPass() const noexcept { return renderPass_; }
	[[nodiscard]] VkExtent2D                  extent() const noexcept { return swapchainExtent_; }
	[[nodiscard]] VkFormat                    swapchainFormat() const noexcept { return swapchainFormat_; }
	[[nodiscard]] const std::vector<VkImage> &swapchainImages() const noexcept { return swapchainImages_; }

	[[nodiscard]] Frame beginFrame();
	void                endFrame(const Frame &frame);
	void                recreateSwapchain();
	void                notifyFramebufferResized() noexcept { framebufferResized_ = true; }
	void                setRenderPass(VkRenderPass renderPass) noexcept { renderPass_ = renderPass; }

  private:
	struct QueueFamilies {
		std::uint32_t graphics    = 0;
		std::uint32_t present     = 0;
		bool          hasGraphics = false;
		bool          hasPresent  = false;
	};

	struct SwapchainSupport {
		VkSurfaceCapabilitiesKHR        capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR>   presentModes;
	};

	void createInstance(const std::string &applicationName);
	void createSurface();
	void selectPhysicalDevice();
	void createDevice();
	void createSwapchain();
	void createCommandResources();
	void destroySwapchain();
	void destroyDeviceResources();
	void waitForValidFramebufferSize() const;

	[[nodiscard]] QueueFamilies    findQueueFamilies(VkPhysicalDevice device) const;
	[[nodiscard]] SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;
	[[nodiscard]] bool             deviceIsSuitable(VkPhysicalDevice device) const;

	Window                                        &window_;
	std::string                                    applicationName_;
	VkInstance                                     instance_        = VK_NULL_HANDLE;
	VkSurfaceKHR                                   surface_         = VK_NULL_HANDLE;
	VkPhysicalDevice                               physicalDevice_  = VK_NULL_HANDLE;
	VkDevice                                       device_          = VK_NULL_HANDLE;
	VkQueue                                        graphicsQueue_   = VK_NULL_HANDLE;
	VkQueue                                        presentQueue_    = VK_NULL_HANDLE;
	VkSwapchainKHR                                 swapchain_       = VK_NULL_HANDLE;
	VkFormat                                       swapchainFormat_ = VK_FORMAT_UNDEFINED;
	VkExtent2D                                     swapchainExtent_{};
	std::vector<VkImage>                           swapchainImages_;
	std::vector<VkImageView>                       swapchainImageViews_;
	VkRenderPass                                   renderPass_  = VK_NULL_HANDLE;
	VkCommandPool                                  commandPool_ = VK_NULL_HANDLE;
	std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers_{};
	std::array<VkSemaphore, maxFramesInFlight>     imageAvailable_{};
	std::vector<VkSemaphore>                       renderFinished_;
	std::array<VkFence, maxFramesInFlight>         inFlight_{};
	std::vector<VkFence>                           imagesInFlight_;
	std::uint32_t                                  currentFrame_       = 0;
	bool                                           framebufferResized_ = false;
};

} // namespace engine
