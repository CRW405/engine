#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>

#include "vulkan_device.hpp"

namespace engine {

class ImageTexture;

// ============================================================================
// Sampled image descriptor set
//
// Owns the descriptor set layout, pool, and set needed to sample one
// ImageTexture. The Vulkan handles stay visible so they can be connected to a
// PipelineLayout and bound while recording commands.
//
// The defaults describe the common case: one combined image sampler exposed at
// binding 0 to the fragment stage. Override the binding or stage in the
// constructor when a shader uses a different interface. More complex layouts
// should create their descriptor objects directly with Vulkan.
// ============================================================================
class TextureDescriptorSet {
  public:
	// Defaults used by the simple texture shader.
	static constexpr std::uint32_t      DEFAULT_BINDING      = 0;
	static constexpr VkShaderStageFlags DEFAULT_STAGE_FLAGS  = VK_SHADER_STAGE_FRAGMENT_BIT;
	static constexpr VkDescriptorType   DEFAULT_DESCRIPTOR_TYPE =
	    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	TextureDescriptorSet(
	    VulkanDevice       &device,
	    const ImageTexture &texture,
	    std::uint32_t       binding    = DEFAULT_BINDING,
	    VkShaderStageFlags  stageFlags = DEFAULT_STAGE_FLAGS);
	~TextureDescriptorSet();

	TextureDescriptorSet(const TextureDescriptorSet &)            = delete;
	TextureDescriptorSet &operator=(const TextureDescriptorSet &) = delete;

	[[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return layout_; }
	[[nodiscard]] VkDescriptorSet       descriptorSet() const noexcept { return set_; }

  private:
	VulkanDevice         &device_;
	VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
	VkDescriptorPool      pool_   = VK_NULL_HANDLE;
	VkDescriptorSet       set_    = VK_NULL_HANDLE;
};

}
