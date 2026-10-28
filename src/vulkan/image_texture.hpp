#pragma once

#include "vulkan/vulkan_device.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace engine {

class ImageTexture {
  public:
	ImageTexture(VulkanDevice &device, const std::string &imagePath);
	~ImageTexture();

	// disable copying
	ImageTexture(const ImageTexture &)            = delete;
	ImageTexture &operator=(const ImageTexture &) = delete;

	// wrappers
	[[nodiscard]] VkImageView imageView() const { return image_view_; }
	[[nodiscard]] VkSampler   sampler() const { return sample_; }

  private:
	VulkanDevice  &device_;
	VkImage        image_       = VK_NULL_HANDLE;
	VkDeviceMemory image_memory = VK_NULL_HANDLE;
	VkImageView    image_view_  = VK_NULL_HANDLE;
	VkSampler      sample_      = VK_NULL_HANDLE;
};

} // namespace engine
