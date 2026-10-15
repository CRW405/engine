#include "vulkan_device.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace engine {

constexpr bool        enable_validation   = true;
constexpr const char *validation_layer    = "VK_LAYER_KHRONOS_validation";
constexpr const char *swapchain_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

bool validationAvailable() {
	std::uint32_t count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> layers(count);
	vkEnumerateInstanceLayerProperties(&count, layers.data());
	return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties &layer) {
		return std::strcmp(layer.layerName, validation_layer) == 0;
	});
}

bool swapchainExtensionAvailable(VkPhysicalDevice device) {
	std::uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> extensions(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
	return std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties &extension) {
		return std::strcmp(extension.extensionName, swapchain_extension) == 0;
	});
}

VulkanDevice::VulkanDevice(GLFWwindow *main_window, std::string app_name) : app_name_(std::move(app_name)) {
	if (main_window == nullptr) {
		throw std::invalid_argument("VulkanDevice requires a valid window");
	}

	createInstance(app_name_);

	VkSurfaceKHR selection_surface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(instance_, main_window, nullptr, &selection_surface) != VK_SUCCESS) {
		vkDestroyInstance(instance_, nullptr);
		instance_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to create Vulkan surface for device selection");
	}

	try {
		selectPhysicalDevice(selection_surface);
		createLogicalDevice(selection_surface);
	} catch (...) {
		vkDestroySurfaceKHR(instance_, selection_surface, nullptr);
		vkDestroyInstance(instance_, nullptr);
		instance_ = VK_NULL_HANDLE;
		throw;
	}
	vkDestroySurfaceKHR(instance_, selection_surface, nullptr);
}

VulkanDevice::~VulkanDevice() {
	if (device_ != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device_);
		vkDestroyDevice(device_, nullptr);
	}
	if (instance_ != VK_NULL_HANDLE) {
		vkDestroyInstance(instance_, nullptr);
	}
}

void VulkanDevice::createInstance(const std::string app_name) {
	if (enable_validation && !validationAvailable()) {
		throw std::runtime_error("Vulkan validation layer is unavailable");
	}

	VkApplicationInfo app_info{};
	app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName   = app_name.c_str();
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName        = "Engine";
	app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion         = VK_API_VERSION_1_0;

	std::uint32_t extension_count = 0;
	const char  **extensions      = glfwGetRequiredInstanceExtensions(&extension_count);
	if (extensions == nullptr) {
		throw std::runtime_error("GLFW did not provide Vulkan extensions");
	}

	VkInstanceCreateInfo info{};
	info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo        = &app_info;
	info.enabledExtensionCount   = extension_count;
	info.ppEnabledExtensionNames = extensions;
	if (enable_validation) {
		info.enabledLayerCount   = 1;
		info.ppEnabledLayerNames = &validation_layer;
	}
	if (vkCreateInstance(&info, nullptr, &instance_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create Vulkan instance");
	}
}

QueueFamilies VulkanDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const {
	QueueFamilies result{};
	std::uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

	for (std::uint32_t index = 0; index < count; ++index) {
		if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			result.graphics    = index;
			result.hasGraphics = true;
		}

		VkBool32 present = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present);
		if (present == VK_TRUE) {
			result.present    = index;
			result.hasPresent = true;
		}

		if (result.hasGraphics && result.hasPresent) {
			break;
		}
	}
	return result;
}

void VulkanDevice::selectPhysicalDevice(VkSurfaceKHR surface) {
	std::uint32_t count = 0;
	if (vkEnumeratePhysicalDevices(instance_, &count, nullptr) != VK_SUCCESS || count == 0) {
		throw std::runtime_error("no Vulkan devices found");
	}

	std::vector<VkPhysicalDevice> devices(count);
	if (vkEnumeratePhysicalDevices(instance_, &count, devices.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to enumerate Vulkan devices");
	}

	for (VkPhysicalDevice device : devices) {
		const QueueFamilies queues = findQueueFamilies(device, surface);
		if (queues.hasGraphics && queues.hasPresent && swapchainExtensionAvailable(device)) {
			physical_device_ = device;
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(physical_device_, &properties);
			std::cout << "Selected Vulkan device: " << properties.deviceName << std::endl;
			return;
		}
	}
	throw std::runtime_error("no suitable Vulkan device found");
}

void VulkanDevice::createLogicalDevice(VkSurfaceKHR surface) {
	const QueueFamilies                  queues   = findQueueFamilies(physical_device_, surface);
	const float                          priority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queue_infos;

	for (std::uint32_t family : std::set{ queues.graphics, queues.present }) {
		VkDeviceQueueCreateInfo info{};
		info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = family;
		info.queueCount       = 1;
		info.pQueuePriorities = &priority;
		queue_infos.push_back(info);
	}

	VkDeviceCreateInfo info{};
	info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.queueCreateInfoCount    = static_cast<std::uint32_t>(queue_infos.size());
	info.pQueueCreateInfos       = queue_infos.data();
	info.enabledExtensionCount   = 1;
	info.ppEnabledExtensionNames = &swapchain_extension;
	if (vkCreateDevice(physical_device_, &info, nullptr, &device_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device");
	}

	vkGetDeviceQueue(device_, queues.graphics, 0, &graphics_queue_);
	vkGetDeviceQueue(device_, queues.present, 0, &present_queue_);
}

} // namespace engine
