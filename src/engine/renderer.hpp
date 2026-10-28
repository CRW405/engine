#include "vulkan/render_pipeline.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

namespace engine {
class Renderer {
	Renderer(VulkanDevice &device, VkReenderPass renderPass);
	~Renderer();

	Renderer(const Renderer &)            = delete;
	Renderer &operator=(const Renderer &) = delete;

	VkPipelineLayout pipelineLayout() const { return pipelineLayout_; }
	Pipeline        &pipeline() { return *pipeline_; }

  private:
	void createPipelineLayout(VulkanDevice &device);
	void createPipeline(VulkanDevice &device, VkRenderPass renderPass);

	VulkanDevice             &device_;
	VkPipelineLayout          pipelineLayout_;
	std::unique_ptr<Pipeline> pipeline_;
};
} // namespace engine
