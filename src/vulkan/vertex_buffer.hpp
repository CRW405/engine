#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_device.hpp"
#include <cstdint>

namespace engine {

class VertexBuffer {
  public:
	VertexBuffer(VulkanDevice &device, const void *data, VkDeviceSize size);
	~VertexBuffer();

	// Disable copy semantics
	VertexBuffer(const VertexBuffer &)            = delete;
	VertexBuffer &operator=(const VertexBuffer &) = delete;

	// Move semantics
	VertexBuffer(VertexBuffer &&) noexcept;
	VertexBuffer &operator=(VertexBuffer &&) = delete;

	// Getters and handles
	[[nodiscard]] VkBuffer     buffer() const noexcept { return buffer_; }
	[[nodiscard]] VkDeviceSize size() const noexcept { return size_; }

  private:
	VulkanDevice  &device_;
	VkBuffer       buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory memory_ = VK_NULL_HANDLE;
	VkDeviceSize   size_   = 0;
};

} // namespace engine
