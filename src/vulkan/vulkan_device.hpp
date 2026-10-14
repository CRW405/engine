#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glfw/glfw_engine.hpp"

namespace engine {

// Structure to hold the indices of the graphics and present queue families
struct QueueFamilies {
	std::uint32_t graphics    = 0;
	std::uint32_t present     = 0;
	bool          hasGraphics = false;
	bool          hasPresent  = false;
};

// wrapper class for general vulkan device management
class VulkanDevice {
  public:
	// a window is required to create a Vulkan surface for device selection
	explicit VulkanDevice(GLFWwindow *main_window, std::string app_name = "Engine");
	~VulkanDevice();

	// disable copying and moving
	VulkanDevice(const VulkanDevice &)            = delete;
	VulkanDevice &operator=(const VulkanDevice &) = delete;
	VulkanDevice(VulkanDevice &&)                 = delete;
	VulkanDevice &operator=(VulkanDevice &&)      = delete;

	// Getters and handles
	[[nodiscard]] VkInstance       instance() const noexcept { return instance_; }
	[[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return physical_device_; }
	[[nodiscard]] VkDevice         device() const noexcept { return device_; }
	[[nodiscard]] VkQueue          graphicsQueue() const noexcept { return graphics_queue_; }
	[[nodiscard]] VkQueue          presentQueue() const noexcept { return present_queue_; }
	[[nodiscard]] QueueFamilies    findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;

  private:
	void createInstance(const std::string app_name); // create the actual Vulkan instance
	void selectPhysicalDevice(VkSurfaceKHR surface); // select a suitable physical device (GPU) that supports graphics and presentation
	void createLogicalDevice(VkSurfaceKHR surface);  // create a logical device from the selected physical device and retrieve the graphics and present queues

	// Vulkan handles and properties
	std::string      app_name_;
	VkInstance       instance_        = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice         device_          = VK_NULL_HANDLE;
	VkQueue          graphics_queue_  = VK_NULL_HANDLE;
	VkQueue          present_queue_   = VK_NULL_HANDLE;
};

} // namespace engine
