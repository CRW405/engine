#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_device.hpp"

namespace engine {

// ============================================================================
// GPU buffer ownership
// ============================================================================
class Buffer {
  public:
	Buffer(VulkanDevice &device, const void *data, VkDeviceSize size, VkBufferUsageFlags usage);
	~Buffer();

	Buffer(const Buffer &)            = delete;
	Buffer &operator=(const Buffer &) = delete;
	Buffer(Buffer &&other) noexcept;
	Buffer &operator=(Buffer &&)      = delete;

	[[nodiscard]] VkBuffer     buffer() const noexcept { return buffer_; }
	[[nodiscard]] VkDeviceSize size() const noexcept { return size_; }
	void                       bind(VkCommandBuffer commandBuffer, std::uint32_t binding = 0) const;

  protected:
	VulkanDevice  &device_;
	VkBuffer       buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory memory_ = VK_NULL_HANDLE;
	VkDeviceSize   size_   = 0;
};

// ============================================================================
// Vertex and index draw buffers
// ============================================================================
class VertexBuffer : public Buffer {
  public:
	VertexBuffer(VulkanDevice &device, const void *data, VkDeviceSize size)
	    : Buffer(device, data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {}
};

class IndexBuffer : public Buffer {
  public:
	IndexBuffer(VulkanDevice &device, const void *data, VkDeviceSize size, VkIndexType indexType)
	    : Buffer(device, data, size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT), index_type_(indexType) {}

	void bind(VkCommandBuffer commandBuffer) const;
	[[nodiscard]] VkIndexType indexType() const noexcept { return index_type_; }

  private:
	VkIndexType index_type_;
};

}
