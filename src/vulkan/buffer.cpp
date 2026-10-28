#include "buffer.hpp"

#include <cstring>
#include <stdexcept>

namespace engine {

// -----------------------------------------------------------------------------
// Buffer lifetime and data upload
// -----------------------------------------------------------------------------
Buffer::Buffer(VulkanDevice &device, const void *data, VkDeviceSize size, VkBufferUsageFlags usage)
    : device_(device), size_(size) {
	if (data == nullptr || size_ == 0) {
		throw std::invalid_argument("buffer requires non-empty data");
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size        = size_;
	bufferInfo.usage       = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device_.device(), &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device_.device(), buffer_, &memoryRequirements);

	VkMemoryAllocateInfo allocation{};
	allocation.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocation.allocationSize  = memoryRequirements.size;
	allocation.memoryTypeIndex = device_.findMemoryType(
	    memoryRequirements.memoryTypeBits,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device_.device(), &allocation, nullptr, &memory_) != VK_SUCCESS) {
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to allocate buffer memory");
	}

	if (vkBindBufferMemory(device_.device(), buffer_, memory_, 0) != VK_SUCCESS) {
		vkFreeMemory(device_.device(), memory_, nullptr);
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		memory_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to bind buffer memory");
	}

	void *mappedData = nullptr;
	if (vkMapMemory(device_.device(), memory_, 0, size_, 0, &mappedData) != VK_SUCCESS) {
		vkFreeMemory(device_.device(), memory_, nullptr);
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		memory_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to map buffer memory");
	}
	std::memcpy(mappedData, data, static_cast<std::size_t>(size_));
	vkUnmapMemory(device_.device(), memory_);
}

Buffer::~Buffer() {
	if (device_.device() != VK_NULL_HANDLE) {
		vkDestroyBuffer(device_.device(), buffer_, nullptr);
		vkFreeMemory(device_.device(), memory_, nullptr);
	}
}

Buffer::Buffer(Buffer &&other) noexcept
    : device_(other.device_), buffer_(other.buffer_), memory_(other.memory_), size_(other.size_) {
	other.buffer_ = VK_NULL_HANDLE;
	other.memory_ = VK_NULL_HANDLE;
	other.size_   = 0;
}

void Buffer::bind(VkCommandBuffer commandBuffer, std::uint32_t binding) const {
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer, binding, 1, &buffer_, &offset);
}

// -----------------------------------------------------------------------------
// Index buffer binding
// -----------------------------------------------------------------------------
void IndexBuffer::bind(VkCommandBuffer commandBuffer) const {
	vkCmdBindIndexBuffer(commandBuffer, buffer_, 0, index_type_);
}

} // namespace engine
