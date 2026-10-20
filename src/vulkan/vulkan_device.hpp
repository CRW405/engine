#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glfw/glfw_engine.hpp"

namespace engine {

struct QueueFamilies {
	std::uint32_t graphics    = 0;
	std::uint32_t present     = 0;
	bool          hasGraphics = false;
	bool          hasPresent  = false;
};

/*
 * Owns the Vulkan instance and logical device shared by renderer resources.
 * Device selection uses the window surface because presentation support belongs
 * to a queue family and surface pair.
 */
class VulkanDevice {
  public:
	explicit VulkanDevice(GLFWwindow *main_window, std::string app_name = "Engine");
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &)            = delete;
	VulkanDevice &operator=(const VulkanDevice &) = delete;
	VulkanDevice(VulkanDevice &&)                 = delete;
	VulkanDevice &operator=(VulkanDevice &&)      = delete;

	[[nodiscard]] VkInstance       instance() const noexcept { return instance_; }
	[[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return physical_device_; }
	[[nodiscard]] VkDevice         device() const noexcept { return device_; }
	[[nodiscard]] VkQueue          graphicsQueue() const noexcept { return graphics_queue_; }
	[[nodiscard]] VkQueue          presentQueue() const noexcept { return present_queue_; }
	[[nodiscard]] QueueFamilies    findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;

  private:
	void createInstance(const std::string app_name);
	void setupDebugMessenger();
	void destroyDebugMessenger() noexcept;
	void selectPhysicalDevice(VkSurfaceKHR surface);
	void createLogicalDevice(VkSurfaceKHR surface);

	std::string      app_name_;
	VkInstance       instance_        = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice         device_          = VK_NULL_HANDLE;
	VkQueue          graphics_queue_  = VK_NULL_HANDLE;
	VkQueue          present_queue_   = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
};

}
