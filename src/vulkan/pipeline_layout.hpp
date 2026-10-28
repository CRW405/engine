#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <initializer_list>

#include "vulkan_device.hpp"

namespace engine {

// ============================================================================
// Descriptor and push-constant layout
// ============================================================================
class PipelineLayout {
  public:
	PipelineLayout(
	    VulkanDevice &device,
	    const VkDescriptorSetLayout *descriptorSetLayouts = nullptr,
	    std::uint32_t descriptorSetLayoutCount = 0,
	    const VkPushConstantRange *pushConstantRanges = nullptr,
	    std::uint32_t pushConstantRangeCount = 0);
	PipelineLayout(
	    VulkanDevice &device,
	    std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts,
	    std::initializer_list<VkPushConstantRange> pushConstantRanges);
	PipelineLayout(
	    VulkanDevice &device,
	    std::initializer_list<VkPushConstantRange> pushConstantRanges);
	~PipelineLayout();

	PipelineLayout(const PipelineLayout &)            = delete;
	PipelineLayout &operator=(const PipelineLayout &) = delete;
	PipelineLayout(PipelineLayout &&)                 = delete;
	PipelineLayout &operator=(PipelineLayout &&)      = delete;

	[[nodiscard]] VkPipelineLayout handle() const noexcept { return layout_; }

	// Bind one descriptor set for the common no-dynamic-offset case.
	void bindDescriptorSet(
	    VkCommandBuffer commandBuffer,
	    VkDescriptorSet descriptorSet,
	    std::uint32_t   firstSet = 0) const;

  private:
	VulkanDevice    &device_;
	VkPipelineLayout layout_ = VK_NULL_HANDLE;
};

}
