#pragma once

#include <vulkan/vulkan.h>

#include <string>

#include <oxide/vulkan/vulkan_device.hpp>

namespace oxide {

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
	void destroyResources() noexcept;

	VulkanDevice  &device_;
	VkImage        image_       = VK_NULL_HANDLE;
	VkDeviceMemory image_memory = VK_NULL_HANDLE;
	VkImageView    image_view_  = VK_NULL_HANDLE;
	VkSampler      sample_      = VK_NULL_HANDLE;
};

} // namespace oxide
