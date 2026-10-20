#include "render_pipeline.hpp"
#include <fstream>
#include <stdexcept>
#include <utility>

namespace engine {

/*
 * Pipeline creation has two independent inputs: shader modules and the
 * fixed-function configuration. The shader modules are temporary construction
 * resources; the pipeline and its layout remain available for command recording.
 */
PipelineConfigInfo::PipelineConfigInfo(const PipelineConfigInfo &other)
    : vertexInputInfo(other.vertexInputInfo),
      viewportInfo(other.viewportInfo),
      inputAssemblyInfo(other.inputAssemblyInfo),
      rasterizationInfo(other.rasterizationInfo),
      multisampleInfo(other.multisampleInfo),
      colorBlendAttachment(other.colorBlendAttachment),
      colorBlendInfo(other.colorBlendInfo),
      depthStencilInfo(other.depthStencilInfo),
      dynamicStates(other.dynamicStates),
      dynamicStateInfo(other.dynamicStateInfo),
      pipelineLayout(other.pipelineLayout),
      renderPass(other.renderPass),
      subpass(other.subpass) {
	refreshInternalPointers();
}

PipelineConfigInfo &PipelineConfigInfo::operator=(const PipelineConfigInfo &other) {
	if (this != &other) {
		vertexInputInfo      = other.vertexInputInfo;
		viewportInfo         = other.viewportInfo;
		inputAssemblyInfo    = other.inputAssemblyInfo;
		rasterizationInfo    = other.rasterizationInfo;
		multisampleInfo      = other.multisampleInfo;
		colorBlendAttachment = other.colorBlendAttachment;
		colorBlendInfo       = other.colorBlendInfo;
		depthStencilInfo     = other.depthStencilInfo;
		dynamicStates        = other.dynamicStates;
		dynamicStateInfo     = other.dynamicStateInfo;
		pipelineLayout       = other.pipelineLayout;
		renderPass           = other.renderPass;
		subpass              = other.subpass;
		refreshInternalPointers();
	}
	return *this;
}

PipelineConfigInfo::PipelineConfigInfo(PipelineConfigInfo &&other) noexcept
    : vertexInputInfo(other.vertexInputInfo),
      viewportInfo(other.viewportInfo),
      inputAssemblyInfo(other.inputAssemblyInfo),
      rasterizationInfo(other.rasterizationInfo),
      multisampleInfo(other.multisampleInfo),
      colorBlendAttachment(other.colorBlendAttachment),
      colorBlendInfo(other.colorBlendInfo),
      depthStencilInfo(other.depthStencilInfo),
      dynamicStates(std::move(other.dynamicStates)),
      dynamicStateInfo(other.dynamicStateInfo),
      pipelineLayout(other.pipelineLayout),
      renderPass(other.renderPass),
      subpass(other.subpass) {
	refreshInternalPointers();
}

PipelineConfigInfo &PipelineConfigInfo::operator=(PipelineConfigInfo &&other) noexcept {
	if (this != &other) {
		vertexInputInfo      = other.vertexInputInfo;
		viewportInfo         = other.viewportInfo;
		inputAssemblyInfo    = other.inputAssemblyInfo;
		rasterizationInfo    = other.rasterizationInfo;
		multisampleInfo      = other.multisampleInfo;
		colorBlendAttachment = other.colorBlendAttachment;
		colorBlendInfo       = other.colorBlendInfo;
		depthStencilInfo     = other.depthStencilInfo;
		dynamicStates        = std::move(other.dynamicStates);
		dynamicStateInfo     = other.dynamicStateInfo;
		pipelineLayout       = other.pipelineLayout;
		renderPass           = other.renderPass;
		subpass              = other.subpass;
		refreshInternalPointers();
	}
	return *this;
}

void PipelineConfigInfo::refreshInternalPointers() noexcept {
	/*
	 * These pointers refer to members owned by this config. Refreshing them is
	 * required after a copy or move because the object's address may change.
	 */
	colorBlendInfo.pAttachments     = &colorBlendAttachment;
	dynamicStateInfo.pDynamicStates = dynamicStates.data();
}

Pipeline::Pipeline(
    VulkanDevice             &device,
    const std::string        &vertFilepath,
    const std::string        &fragFilepath,
    const PipelineConfigInfo &configInfo)
    : device_(device) {

	/*
	 * Step 1: load shader bytecode and create temporary shader modules. The
	 * modules are needed only while the graphics pipeline is being built.
	 */
	auto vertCode = readFile(vertFilepath);
	auto fragCode = readFile(fragFilepath);

	try {
		createShaderModule(vertCode, &vertShaderModule_);
		createShaderModule(fragCode, &fragShaderModule_);

		if (configInfo.pipelineLayout == VK_NULL_HANDLE) {
			/* Use an empty layout until descriptors or push constants are needed. */
			VkPipelineLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &layout_) != VK_SUCCESS) {
				throw std::runtime_error("failed to create pipeline layout");
			}
			owns_layout_ = true;
		} else {
			layout_ = configInfo.pipelineLayout;
		}

		/*
		 * Step 2: connect shader stages to the fixed-function configuration and
		 * create the graphics pipeline for the context's render pass.
		 */
		VkPipelineShaderStageCreateInfo shaderStages[2]{};
		shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = vertShaderModule_;
		shaderStages[0].pName  = "main";

		shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = fragShaderModule_;
		shaderStages[1].pName  = "main";

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

		pipelineInfo.layout     = layout_;
		pipelineInfo.renderPass = configInfo.renderPass;
		pipelineInfo.subpass    = configInfo.subpass;

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex  = -1;

		if (vkCreateGraphicsPipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline");
		}
	} catch (...) {
		if (fragShaderModule_ != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device_.device(), fragShaderModule_, nullptr);
			fragShaderModule_ = VK_NULL_HANDLE;
		}
		if (vertShaderModule_ != VK_NULL_HANDLE) {
			vkDestroyShaderModule(device_.device(), vertShaderModule_, nullptr);
			vertShaderModule_ = VK_NULL_HANDLE;
		}
		if (owns_layout_ && layout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device_.device(), layout_, nullptr);
			layout_ = VK_NULL_HANDLE;
			owns_layout_ = false;
		}
		throw;
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
		if (owns_layout_ && layout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device_.device(), layout_, nullptr);
		}
	}
}

Pipeline::Pipeline(Pipeline &&other) noexcept
    : device_(other.device_),
      pipeline_(other.pipeline_),
      layout_(other.layout_),
      vertShaderModule_(other.vertShaderModule_),
      fragShaderModule_(other.fragShaderModule_),
      owns_layout_(other.owns_layout_) {
	other.pipeline_         = VK_NULL_HANDLE;
	other.layout_           = VK_NULL_HANDLE;
	other.vertShaderModule_ = VK_NULL_HANDLE;
	other.fragShaderModule_ = VK_NULL_HANDLE;
	other.owns_layout_      = false;
}

void Pipeline::bind(VkCommandBuffer commandBuffer) {
	/* Binding makes this pipeline the active graphics state for later draws. */
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

std::vector<char> Pipeline::readFile(const std::string &filepath) {
	/* Shader binaries are read as raw bytes for VkShaderModule creation. */
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
	/* Vulkan consumes SPIR-V as 32-bit words even though files are byte streams. */
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

	if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module");
	}
}

PipelineConfigInfo Pipeline::defaultPipelineConfigInfo() {
	/*
	 * Establish the smallest useful triangle pipeline: fixed-function defaults,
	 * dynamic viewport/scissor state, and no depth attachment.
	 */
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
	configInfo.depthStencilInfo.depthTestEnable       = VK_FALSE;
	configInfo.depthStencilInfo.depthWriteEnable      = VK_TRUE;
	configInfo.depthStencilInfo.depthCompareOp        = VK_COMPARE_OP_LESS;
	configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	configInfo.depthStencilInfo.stencilTestEnable     = VK_FALSE;

	configInfo.dynamicStates                      = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	configInfo.dynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStates.size());
	configInfo.dynamicStateInfo.pDynamicStates    = configInfo.dynamicStates.data();

	configInfo.vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	configInfo.vertexInputInfo.vertexBindingDescriptionCount   = 0;
	configInfo.vertexInputInfo.pVertexBindingDescriptions      = nullptr;
	configInfo.vertexInputInfo.vertexAttributeDescriptionCount = 0;
	configInfo.vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

	return configInfo;
}

}
