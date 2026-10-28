#include "texture_descriptor.hpp"
#include "image_texture.hpp"

#include <stdexcept>

namespace engine {

// -----------------------------------------------------------------------------
// Texture descriptor lifetime and update
//
// The layout, pool, and write below implement the single-texture defaults
// declared in the header. Keeping each Vulkan object separate makes it clear
// what the convenience constructor creates and what a custom layout replaces.
// -----------------------------------------------------------------------------
TextureDescriptorSet::TextureDescriptorSet(
    VulkanDevice       &device,
    const ImageTexture &texture,
    std::uint32_t       binding,
    VkShaderStageFlags  stageFlags)
    : device_(device) {
	// Describe one combined image sampler at the requested binding and stage.
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding            = binding;
	layoutBinding.descriptorCount    = 1;
	layoutBinding.descriptorType     = DEFAULT_DESCRIPTOR_TYPE;
	layoutBinding.pImmutableSamplers = nullptr;
	layoutBinding.stageFlags         = stageFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings    = &layoutBinding;
	if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &layout_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}

	// The pool only needs to hold the single descriptor set for this texture.
	VkDescriptorPoolSize poolSize{};
	poolSize.type            = DEFAULT_DESCRIPTOR_TYPE;
	poolSize.descriptorCount = 1;
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes    = &poolSize;
	poolInfo.maxSets       = 1;
	if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
		vkDestroyDescriptorSetLayout(device_.device(), layout_, nullptr);
		layout_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to create descriptor pool");
	}

	// Allocate the set from the layout created above.
	VkDescriptorSetAllocateInfo allocation{};
	allocation.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocation.descriptorPool     = pool_;
	allocation.descriptorSetCount = 1;
	allocation.pSetLayouts        = &layout_;
	if (vkAllocateDescriptorSets(device_.device(), &allocation, &set_) != VK_SUCCESS) {
		vkDestroyDescriptorPool(device_.device(), pool_, nullptr);
		vkDestroyDescriptorSetLayout(device_.device(), layout_, nullptr);
		pool_   = VK_NULL_HANDLE;
		layout_ = VK_NULL_HANDLE;
		throw std::runtime_error("failed to allocate descriptor set");
	}

	// Point the descriptor at the image view and sampler owned by ImageTexture.
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView   = texture.imageView();
	imageInfo.sampler     = texture.sampler();

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet          = set_;
	descriptorWrite.dstBinding      = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType  = DEFAULT_DESCRIPTOR_TYPE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo      = &imageInfo;

	vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);
}

TextureDescriptorSet::~TextureDescriptorSet() {
	// The pool releases the set; the layout is destroyed separately.
	if (device_.device() != VK_NULL_HANDLE) {
		if (pool_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device_.device(), pool_, nullptr);
		}
		if (layout_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device_.device(), layout_, nullptr);
		}
	}
}

}
