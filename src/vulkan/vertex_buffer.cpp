#include "vertex_buffer.hpp"
#include <cstring>
#include <stdexcept>

namespace engine {

/*
 * This first buffer implementation favors clarity over transfer speed. It
 * allocates host-visible memory and copies vertex data directly into it.
 */
uint32_t findMemoryType(VulkanDevice &device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	/* Match Vulkan's allowed type bits with the properties the buffer needs. */
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(device.physicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type");
}

VertexBuffer::VertexBuffer(VulkanDevice &device, const void *data, VkDeviceSize size)
    : device_(device), size_(size) {
	/*
	 * Step 1: create a buffer that can be bound as vertex input. This buffer is
	 * intentionally simple and is not yet backed by a staging transfer.
	 */
	if (data == nullptr || size_ == 0) {
		throw std::invalid_argument("vertex buffer requires non-empty data");
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = size_;
	bufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device_.device(), &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create vertex buffer");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device_.device(), buffer_, &memRequirements);

	/*
	 * Step 2: allocate compatible host-visible memory, bind it to the buffer,
	 * and copy the caller's vertex data into the mapped allocation.
	 */
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize  = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
	    device_,
	    memRequirements.memoryTypeBits,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to allocate vertex buffer memory");
	}

	if (vkBindBufferMemory(device_.device(), buffer_, memory_, 0) != VK_SUCCESS) {
		vkFreeMemory(device_.device(), memory_, nullptr);
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		memory_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to bind vertex buffer memory");
	}

	void *mappedData;
	if (vkMapMemory(device_.device(), memory_, 0, size_, 0, &mappedData) != VK_SUCCESS) {
		vkFreeMemory(device_.device(), memory_, nullptr);
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		memory_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to map vertex buffer memory");
	}
	std::memcpy(mappedData, data, static_cast<size_t>(size_));
	vkUnmapMemory(device_.device(), memory_);
}

VertexBuffer::~VertexBuffer() {
	if (device_.device() != VK_NULL_HANDLE) {
		if (buffer_ != VK_NULL_HANDLE) {
			vkDestroyBuffer(device_.device(), buffer_, nullptr);
		}
		if (memory_ != VK_NULL_HANDLE) {
			vkFreeMemory(device_.device(), memory_, nullptr);
		}
	}
}

VertexBuffer::VertexBuffer(VertexBuffer &&other) noexcept
    : device_(other.device_), buffer_(other.buffer_), memory_(other.memory_), size_(other.size_) {
	other.buffer_ = VK_NULL_HANDLE;
	other.memory_ = VK_NULL_HANDLE;
	other.size_   = 0;
}

}
