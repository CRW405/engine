#include "vulkan_engine.hpp"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace engine {
namespace {

constexpr bool enableValidation = true;
constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";

bool validationAvailable() {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, validationLayer) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

void VulkanContext::createInstance(const std::string& applicationName) {
    if (enableValidation && !validationAvailable()) {
        throw std::runtime_error("Vulkan validation layer is unavailable");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = applicationName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Vulkanc Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (extensions == nullptr) {
        throw std::runtime_error("GLFW did not provide Vulkan extensions");
    }

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = extensions;
    if (enableValidation) {
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &validationLayer;
    }
    if (vkCreateInstance(&info, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance");
    }
}

void VulkanContext::createSurface() {
    if (glfwCreateWindowSurface(instance_, window_.handle(), nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
}

} // namespace engine
