#include "vulkan_engine.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace engine {
namespace {

constexpr const char* swapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

} // namespace

VulkanContext::QueueFamilies VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilies result{};
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (std::uint32_t index = 0; index < count; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            result.graphics = index;
            result.hasGraphics = true;
        }
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &present);
        if (present == VK_TRUE) {
            result.present = index;
            result.hasPresent = true;
        }
        if (result.hasGraphics && result.hasPresent) {
            break;
        }
    }
    return result;
}

bool VulkanContext::deviceIsSuitable(VkPhysicalDevice device) const {
    const QueueFamilies queues = findQueueFamilies(device);
    if (!queues.hasGraphics || !queues.hasPresent) {
        return false;
    }

    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    const bool hasSwapchain = std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) {
        return std::strcmp(extension.extensionName, swapchainExtension) == 0;
    });
    const SwapchainSupport support = querySwapchainSupport(device);
    return hasSwapchain && !support.formats.empty() && !support.presentModes.empty();
}

void VulkanContext::selectPhysicalDevice() {
    std::uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance_, &count, nullptr) != VK_SUCCESS || count == 0) {
        throw std::runtime_error("no Vulkan devices found");
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(instance_, &count, devices.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to enumerate Vulkan devices");
    }

    const auto selected = std::find_if(devices.begin(), devices.end(),
        [this](VkPhysicalDevice device) { return deviceIsSuitable(device); });
    if (selected == devices.end()) {
        throw std::runtime_error("no suitable Vulkan device found");
    }
    physicalDevice_ = *selected;
}

void VulkanContext::createDevice() {
    const QueueFamilies queues = findQueueFamilies(physicalDevice_);
    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    for (std::uint32_t family : std::set{queues.graphics, queues.present}) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
    info.pQueueCreateInfos = queueInfos.data();
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = &swapchainExtension;
    if (vkCreateDevice(physicalDevice_, &info, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device");
    }
    vkGetDeviceQueue(device_, queues.graphics, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queues.present, 0, &presentQueue_);
}

} // namespace engine
