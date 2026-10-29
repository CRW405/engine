#include "glfw/glfw_oxide.hpp"
#include "tools/fps.hpp"
#include "tools/log.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/image_texture.hpp"
#include "vulkan/pipeline_layout.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/texture_descriptor.hpp"
#include "vulkan/uniform_buffer_ring.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cglm/cglm.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

// -----------------------------------------------------------------------------
// Full showcase application
//
// This example combines Oxide features in one program: window and input
// handling, swapchain-managed frames, vertex and index buffers, a sampled
// texture, a uniform buffer, push constants, a pipeline layout with multiple
// descriptor sets, a graphics pipeline, and frame-rate reporting.
// -----------------------------------------------------------------------------
struct showcase_vertex {
	float position[3];
	float color[3];
	float uv[2];
};

struct showcase_ubo {
	mat4 model;
	mat4 view;
	mat4 projection;
};

struct showcase_push_constants {
	vec4  tint;
	float time;
};

int main() {
	std::printf("Running Full Showcase Example\n");
	std::printf("Controls: Space pause | R reset | drag left mouse to tilt | scroll zoom | Shift speed up | Q/Esc quit\n");
	try {
		std::string          app_name = "Full Showcase Example";
		oxide::GLFWContext   glfw;
		oxide::Window        window({ 900, 650, app_name, true });
		oxide::VulkanDevice  device(window.handle(), app_name);
		oxide::VulkanContext vulkan(device, &window);

		// The context owns the surface, swapchain, render pass, framebuffers, and
		// frame synchronization. These getters expose the presentation state.
		const VkExtent2D extent = vulkan.extent();
		std::printf("Swapchain: %ux%u | %zu images | %zu views | format %d\n",
		            extent.width,
		            extent.height,
		            vulkan.swapchainImages().size(),
		            vulkan.swapchainImageViews().size(),
		            static_cast<int>(vulkan.swapchainFormat()));

		// -------------------------------------------------------------------------
		// Buffers: a textured quad, its index list, and the uniform buffer that
		// stores the model, view, and projection matrices for every draw.
		// -------------------------------------------------------------------------
		const std::vector<showcase_vertex> vertices = {
			{ { -0.6f, -0.6f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 0.6f, -0.6f, 0.0f },  { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 0.6f, 0.6f, 0.0f },   { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -0.6f, 0.6f, 0.0f },  { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }
		};
		const std::vector<std::uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

		oxide::Mesh mesh(device, vertices, indices);

		showcase_ubo ubo{};
		glm_mat4_identity(ubo.model);
		glm_mat4_identity(ubo.view);
		glm_mat4_identity(ubo.projection);

		// One UBO and descriptor set per frame in flight so a CPU update cannot
		// overwrite matrices that the previous frame is still reading.
		oxide::UniformBufferRing uniform_buffers(device,
		                                         oxide::MAX_FRAMES_IN_FLIGHT,
		                                         sizeof(showcase_ubo),
		                                         &ubo);

		// -------------------------------------------------------------------------
		// Descriptors: the texture uses set 0 and the uniform buffer uses set 1.
		// The texture convenience type owns its layout, pool, and descriptor set;
		// the uniform ring owns one set per frame.
		// -------------------------------------------------------------------------
		oxide::ImageTexture         texture(device, OXIDE_ASSET_DIR "/smiley_0.png");
		oxide::TextureDescriptorSet texture_descriptor(device, texture);

		// Push constants carry per-draw data without a descriptor or buffer.
		const oxide::PushConstant push_constant = oxide::pushConstant<showcase_push_constants>(
		    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		// The initializer-list order defines the set index: texture=0, UBO=1.
		oxide::PipelineLayout pipeline_layout(device,
		                                      { texture_descriptor.layout(), uniform_buffers.layout() },
		                                      { push_constant.range() });

		// -------------------------------------------------------------------------
		// Pipeline: describe the vertex layout and start from the default
		// fixed-function state, then connect the layout and render pass.
		// -------------------------------------------------------------------------
		oxide::PipelineConfigInfo pipeline_config = oxide::Pipeline::defaultPipelineConfigInfo();

		oxide::VertexInputDescription vertex_input;
		vertex_input.addBinding(0, sizeof(showcase_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(showcase_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(showcase_vertex, color))
		    .addAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(showcase_vertex, uv));
		pipeline_config.setVertexInput(vertex_input)
		    .setRenderPass(vulkan.renderPass())
		    .setPipelineLayout(pipeline_layout.handle());

		oxide::Pipeline pipeline(device,
		                         OXIDE_SHADER_DIR "/showcase.vert.spv",
		                         OXIDE_SHADER_DIR "/showcase.frag.spv",
		                         pipeline_config);

		// -------------------------------------------------------------------------
		// Application state used by the controls below.
		// -------------------------------------------------------------------------
		showcase_push_constants push{
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			0.0f
		};
		double animation_time = 0.0;
		double previous_time  = window.time();
		float  distance       = 2.8f;
		bool   paused         = false;

		tools::FPSCounter fps(window.time());

		// -------------------------------------------------------------------------
		// Render loop: poll input, advance animation state, update the uniform
		// buffer, record one frame, submit it, and present it.
		// -------------------------------------------------------------------------
		while (!window.shouldClose()) {
			window.pollEvents();

			// keyPressed returns queued presses once; keyReleased reads the
			// current physical key state and is useful for held modifiers.
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}
			if (window.keyPressed(GLFW_KEY_SPACE)) {
				paused = !paused;
			}
			if (window.keyPressed(GLFW_KEY_R)) {
				distance = 2.8f;
			}
			const bool  shift_held      = !window.keyReleased(GLFW_KEY_LEFT_SHIFT);
			const float animation_speed = shift_held ? 3.0f : 1.0f;

			// Scroll events are accumulated until consumed.
			const oxide::MousePosition scroll = window.consumeScrollDelta();
			distance                          = glm_clamp(distance * std::exp(static_cast<float>(-scroll.y) * 0.15f), 1.5f, 6.0f);

			const double now   = window.time();
			const double delta = now - previous_time;
			previous_time      = now;
			if (!paused) {
				animation_time += delta * animation_speed;
			}

			// A held mouse button changes the tint and enables cursor-driven tilt.
			const bool  dragging = window.mouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
			const float tint_r   = dragging ? 1.35f : 1.0f;
			const float tint_g   = dragging ? 1.15f : 1.0f;
			const float tint_b   = dragging ? 0.85f : 1.0f;
			push.tint[0]         = tint_r;
			push.tint[1]         = tint_g;
			push.tint[2]         = tint_b;
			push.tint[3]         = 1.0f;
			push.time            = static_cast<float>(animation_time);

			// beginFrame returns an empty frame when the swapchain is rebuilt.
			// Resizing the window exercises that path automatically.
			const oxide::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			// -------------------------------------------------------------------------
			// Model-view-projection: spin the quad, optionally tilt it with the
			// cursor, place the camera at the scroll-controlled distance, and
			// build a perspective projection from the current swapchain extent.
			// -------------------------------------------------------------------------
			glm_mat4_identity(ubo.model);
			vec3 y_axis = { 0.0f, 1.0f, 0.0f };
			vec3 x_axis = { 1.0f, 0.0f, 0.0f };
			glm_rotate(ubo.model, static_cast<float>(animation_time) * glm_rad(45.0f), y_axis);

			if (dragging) {
				const oxide::MousePosition cursor   = window.cursorPosition();
				const float                cursor_x = static_cast<float>(cursor.x) /
				                                          static_cast<float>(window.width()) -
				                                      0.5f;
				const float                cursor_y = static_cast<float>(cursor.y) /
				                                          static_cast<float>(window.height()) -
				                                      0.5f;
				glm_rotate(ubo.model, cursor_x * glm_rad(90.0f), y_axis);
				glm_rotate(ubo.model, -cursor_y * glm_rad(90.0f), x_axis);
			}

			vec3 eye    = { 0.0f, 0.0f, distance };
			vec3 center = { 0.0f, 0.0f, 0.0f };
			vec3 up     = { 0.0f, 1.0f, 0.0f };
			glm_lookat(eye, center, up, ubo.view);

			const float aspect_ratio = static_cast<float>(vulkan.extent().width) /
			                           static_cast<float>(vulkan.extent().height);
			glm_perspective(glm_rad(45.0f), aspect_ratio, 0.1f, 100.0f, ubo.projection);

			// Update only this frame's UBO. beginFrame() has already waited on
			// this slot's fence, so the previous submission that read this
			// buffer has completed.
			uniform_buffers.update(frame.frameIndex, ubo);

			// Clear to a dark background and record the frame.
			vulkan.beginRenderPass(frame, { 0.05f, 0.06f, 0.08f, 1.0f });

			mesh.bind(frame.commandBuffer);
			texture_descriptor.bind(frame.commandBuffer, pipeline_layout.handle(), 0);
			uniform_buffers.bind(frame.commandBuffer, pipeline_layout.handle(), frame.frameIndex, 1);

			pipeline.bind(frame.commandBuffer);
			vulkan.setViewportAndScissor(frame.commandBuffer);

			pipeline_layout.pushConstant(frame.commandBuffer, push_constant, push);

			mesh.draw(frame.commandBuffer);

			vulkan.endRenderPass(frame);
			vulkan.endFrame(frame);
			fps.update(window.time());
		}

		fps.finish();
		device.waitIdle();
	} catch (const std::exception &error) {
		OXIDE_LOG_FATAL << "unhandled exception: " << error.what();
		return 1;
	}
	return 0;
}
