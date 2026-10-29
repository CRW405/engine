#include "ubo_descriptor.hpp"
#include "buffer.hpp"

namespace oxide {

// -----------------------------------------------------------------------------
// Uniform buffer descriptor lifetime and update
//
// The shared descriptor wrappers create the single uniform buffer described in
// the header. The write below points that binding at the full range of the
// Buffer, so updating the buffer contents is enough to update the shader input.
// -----------------------------------------------------------------------------
UBODescriptorSet::UBODescriptorSet(
    VulkanDevice       &device,
    const Buffer       &buffer,
    std::uint32_t       binding,
    VkShaderStageFlags  stageFlags)
    : layout_(device, DescriptorBinding{ binding, DEFAULT_DESCRIPTOR_TYPE, stageFlags }),
      pool_(device, DEFAULT_DESCRIPTOR_TYPE),
      set_(pool_, layout_.handle()) {
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer.buffer();
	bufferInfo.offset = 0;
	bufferInfo.range  = buffer.size();

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet          = set_.handle();
	descriptorWrite.dstBinding      = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType  = DEFAULT_DESCRIPTOR_TYPE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo     = &bufferInfo;

	vkUpdateDescriptorSets(device.device(), 1, &descriptorWrite, 0, nullptr);
}

} // namespace oxide
