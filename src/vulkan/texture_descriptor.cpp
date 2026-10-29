#include "texture_descriptor.hpp"
#include "image_texture.hpp"

namespace oxide {

// -----------------------------------------------------------------------------
// Texture descriptor lifetime and update
//
// The shared descriptor wrappers create the single combined image sampler
// described in the header. The write below points that binding at the image
// view and sampler owned by ImageTexture.
// -----------------------------------------------------------------------------
TextureDescriptorSet::TextureDescriptorSet(
    VulkanDevice       &device,
    const ImageTexture &texture,
    std::uint32_t       binding,
    VkShaderStageFlags  stageFlags)
    : layout_(device, DescriptorBinding{ binding, DEFAULT_DESCRIPTOR_TYPE, stageFlags }),
      pool_(device, DEFAULT_DESCRIPTOR_TYPE),
      set_(pool_, layout_.handle()) {
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView   = texture.imageView();
	imageInfo.sampler     = texture.sampler();

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet          = set_.handle();
	descriptorWrite.dstBinding      = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType  = DEFAULT_DESCRIPTOR_TYPE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo      = &imageInfo;

	vkUpdateDescriptorSets(device.device(), 1, &descriptorWrite, 0, nullptr);
}

} // namespace oxide
