#include "render_pipeline.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace engine {

Pipeline::Pipeline(
    VulkanDevice             &device,
    const std::string        &vertFilepath,
    const std::string        &fragFilepath,
    const PipelineConfigInfo &configInfo)
    : device_(device) {

	auto vertCode = readFile(vertFilepath);
	auto fragCode = readFile(fragFilepath);

	createShaderModule(vertCode, &vertShaderModule_);
	createShaderModule(fragCode, &fragShaderModule_);

	VkPipelineShaderStageCreateInfo shaderStages[2]{};
	shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertShaderModule_;
	shaderStages[0].pName  = "main";

	shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragShaderModule_;
	shaderStages[1].pName  = "main";

	// Vertex input state (decoupled to accept raw byte layouts or dynamic bindings)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount   = 0;
	vertexInputInfo.pVertexBindingDescriptions      = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &configInfo.vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
	pipelineInfo.pViewportState      = &configInfo.viewportInfo;
	pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
	pipelineInfo.pMultisampleState   = &configInfo.multisampleInfo;
	pipelineInfo.pColorBlendState    = &configInfo.colorBlendInfo;
	pipelineInfo.pDepthStencilState  = &configInfo.depthStencilInfo;
	pipelineInfo.pDynamicState       = &configInfo.dynamicStateInfo;

	pipelineInfo.layout     = configInfo.pipelineLayout;
	pipelineInfo.renderPass = configInfo.renderPass;
	pipelineInfo.subpass    = configInfo.subpass;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex  = -1;

	if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}
}

Pipeline::~Pipeline() {
	if (device_.device() != VK_NULL_HANDLE) {
		if (vertShaderModule_ != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device_.device(), vertShaderModule_, nullptr);
		}
		if (fragShaderModule_ != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device_.device(), fragShaderModule_, nullptr);
		}
		if (pipeline_ != VK_NULL_HANDLE) {
			vkDestroyPipeline(device_.device(), pipeline_, nullptr);
		}
	}
}

Pipeline::Pipeline(Pipeline &&other) noexcept
    : device_(other.device_),
      pipeline_(other.pipeline_),
      layout_(other.layout_),
      vertShaderModule_(other.vertShaderModule_),
      fragShaderModule_(other.fragShaderModule_) {
	other.pipeline_         = VK_NULL_HANDLE;
	other.layout_           = VK_NULL_HANDLE;
	other.vertShaderModule_ = VK_NULL_HANDLE;
	other.fragShaderModule_ = VK_NULL_HANDLE;
}

void Pipeline::bind(VkCommandBuffer commandBuffer) {
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

std::vector<char> Pipeline::readFile(const std::string &filepath) {
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file: " + filepath);
	}

	size_t            fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

void Pipeline::createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

	if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module");
	}
}

PipelineConfigInfo Pipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height) {
	PipelineConfigInfo configInfo{};

	configInfo.inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	configInfo.inputAssemblyInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	configInfo.viewportInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	configInfo.viewportInfo.viewportCount = 1;
	configInfo.viewportInfo.pViewports    = nullptr;
	configInfo.viewportInfo.scissorCount  = 1;
	configInfo.viewportInfo.pScissors     = nullptr;

	configInfo.rasterizationInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	configInfo.rasterizationInfo.depthClampEnable        = VK_FALSE;
	configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
	configInfo.rasterizationInfo.polygonMode             = VK_POLYGON_MODE_FILL;
	configInfo.rasterizationInfo.lineWidth               = 1.0f;
	configInfo.rasterizationInfo.cullMode                = VK_CULL_MODE_NONE;
	configInfo.rasterizationInfo.frontFace               = VK_FRONT_FACE_CLOCKWISE;
	configInfo.rasterizationInfo.depthBiasEnable         = VK_FALSE;

	configInfo.multisampleInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	configInfo.multisampleInfo.sampleShadingEnable  = VK_FALSE;
	configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	configInfo.colorBlendAttachment.colorWriteMask =
	    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	configInfo.colorBlendAttachment.blendEnable = VK_FALSE;

	configInfo.colorBlendInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	configInfo.colorBlendInfo.logicOpEnable   = VK_FALSE;
	configInfo.colorBlendInfo.attachmentCount = 1;
	configInfo.colorBlendInfo.pAttachments    = &configInfo.colorBlendAttachment;

	configInfo.depthStencilInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	configInfo.depthStencilInfo.depthTestEnable       = VK_TRUE;
	configInfo.depthStencilInfo.depthWriteEnable      = VK_TRUE;
	configInfo.depthStencilInfo.depthCompareOp        = VK_COMPARE_OP_LESS;
	configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	configInfo.depthStencilInfo.stencilTestEnable     = VK_FALSE;

	configInfo.dynamicStates                      = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	configInfo.dynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStates.size());
	configInfo.dynamicStateInfo.pDynamicStates    = configInfo.dynamicStates.data();

	return configInfo;
}

} // namespace engine
