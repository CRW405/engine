#pragma once

#define GLFW_INCLUDE_VULKAN
#include "vulkan_device.hpp"
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

namespace engine {

struct PipelineConfigInfo {
	VkPipelineVertexInputStateCreateInfo   vertexInputInfo;
	VkPipelineViewportStateCreateInfo      viewportInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationInfo;
	VkPipelineMultisampleStateCreateInfo   multisampleInfo;
	VkPipelineColorBlendAttachmentState    colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo    colorBlendInfo;
	VkPipelineDepthStencilStateCreateInfo  depthStencilInfo;
	std::vector<VkDynamicState>            dynamicStates;
	VkPipelineDynamicStateCreateInfo       dynamicStateInfo;
	VkPipelineLayout                       pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass                           renderPass     = VK_NULL_HANDLE;
	uint32_t                               subpass        = 0;
};

class Pipeline {
  public:
	Pipeline(
	    VulkanDevice             &device,
	    const std::string        &vertFilepath,
	    const std::string        &fragFilepath,
	    const PipelineConfigInfo &configInfo);

	~Pipeline();

	// Disable copy semantics
	Pipeline(const Pipeline &)            = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	// Move semantics
	Pipeline(Pipeline &&) noexcept;
	Pipeline &operator=(Pipeline &&) = delete;

	void bind(VkCommandBuffer commandBuffer);

	[[nodiscard]] VkPipeline       pipeline() const noexcept { return pipeline_; }
	[[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

	// Helper to populate default configuration settings
	static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

  private:
	static std::vector<char> readFile(const std::string &filepath);
	void                     createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

	VulkanDevice    &device_;
	VkPipeline       pipeline_         = VK_NULL_HANDLE;
	VkPipelineLayout layout_           = VK_NULL_HANDLE;
	VkShaderModule   vertShaderModule_ = VK_NULL_HANDLE;
	VkShaderModule   fragShaderModule_ = VK_NULL_HANDLE;
};

} // namespace engine
