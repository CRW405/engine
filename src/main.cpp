#include "glfw/glfw_engine.hpp"
#include "tools/fps.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/image_texture.hpp"
#include "vulkan/pipeline_layout.hpp"
#include "vulkan/render_pipeline.hpp"
#include "vulkan/texture_descriptor.hpp"
#include "vulkan/ubo_descriptor.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_device.hpp"

#include <cglm/cglm.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <vector>

int vulkan_triangle();
int sierpinski_triangle();
int image_texture_example();
int uniform_buffer_example();
int showcase_example();

int main(int argc, char *argv[]) {
	int example = 0;
	if (argc > 1) {
		std::string arg1 = argv[1];
		if (arg1 == "all") {
			vulkan_triangle();
			sierpinski_triangle();
			image_texture_example();
			uniform_buffer_example();
			showcase_example();
			return 0;
		}
	}
	example = std::atoi(argv[1]);
	switch (example) {
	case 0:
		return vulkan_triangle();
	case 1:
		return sierpinski_triangle();
	case 2:
		return image_texture_example();
	case 3:
		return uniform_buffer_example();
	case 4:
		return showcase_example();
	default:
		std::fprintf(stderr, "Unknown program %d\n", example);
		return 1;
	}
}

// -----------------------------------------------------------------------------
// Triangle application
//
// Create the engine, record a rotating indexed triangle, and present frames
// until the window is closed.
// -----------------------------------------------------------------------------
struct simple_vertex {
	float position[2];
	float color[3];
};
int vulkan_triangle() {
	std::printf("Running Vulkan Triangle\n");
	try {
		// -------------------------------------------------------------------------
		// Step 1: create the process, window, Vulkan device, and presentation
		// context. These objects are declared in dependency order so teardown
		// happens in the reverse order automatically.
		// -------------------------------------------------------------------------
		engine::GLFWContext   glfw;
		engine::Window        window({ 500, 500, "Engine", true });
		engine::VulkanDevice  device(window.handle(), "Engine");
		engine::VulkanContext vulkan(device, &window);

		// -------------------------------------------------------------------------
		// Step 2: describe how simple_vertex is laid out in the vertex buffer.
		// The shader locations must match these attribute descriptions.
		// -------------------------------------------------------------------------
		engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

		engine::VertexInputDescription vertexInput;
		vertexInput.addBinding(0, sizeof(simple_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(simple_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(simple_vertex, color));
		pipelineConfig.setVertexInput(vertexInput);
		pipelineConfig.renderPass = vulkan.renderPass();

		// Push constant setup
		const engine::PushConstant transformPush = engine::pushConstant<mat4>(VK_SHADER_STAGE_VERTEX_BIT);
		engine::PipelineLayout     pipelineLayout(device, { transformPush.range() });
		pipelineConfig.pipelineLayout = pipelineLayout.handle();

		// -------------------------------------------------------------------------
		// Step 3: create the pipeline and upload the triangle data. The pipeline
		// owns its shader modules and default layout after construction.
		// -------------------------------------------------------------------------
		engine::Pipeline pipeline(device, "shaders/vulkan_triangle.vert.spv", "shaders/vulkan_triangle.frag.spv", pipelineConfig);

		std::vector<simple_vertex> vulkan_triangle = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.5f },   { 0.0f, 0.0f, 1.0f } }
		};
		const std::vector<std::uint16_t> triangle_indices = { 0, 1, 2 };
		engine::Mesh                     triangle_mesh(device, vulkan_triangle, triangle_indices);
		std::printf("Vertex buffer created with size: %zu bytes\n", sizeof(simple_vertex) * vulkan_triangle.size());

		// -------------------------------------------------------------------------
		// Step 4: acquire, record, submit, and present one frame at a time.
		// beginFrame returns an empty frame when the swapchain was recreated.
		// -------------------------------------------------------------------------
		tools::FPSCounter fps(window.time());

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			// -------------------------------------------------------------------------
			// Begin the render pass against the image acquired for this frame.
			// The swapchain extent is the framebuffer size Vulkan is actually using.
			// -------------------------------------------------------------------------
			vulkan.beginRenderPass(frame, { 1.0f, 1.0f, 1.0f, 1.0f });

			// Bind state first, then issue the draw commands that use it.
			pipeline.bind(frame.commandBuffer);

			vulkan.setViewportAndScissor(frame.commandBuffer);
			triangle_mesh.bind(frame.commandBuffer);

			// Push constant data
			mat4  Transform    = GLM_MAT4_IDENTITY_INIT;
			float angle        = static_cast<float>(window.time() * glm_rad(90.0f)); // Rotate 90 degrees per second
			vec3  rotationAxis = { 0.0f, 0.0f, 1.0f };
			glm_rotate(Transform, angle, rotationAxis);
			pipelineLayout.pushConstant(frame.commandBuffer, transformPush, Transform);

			triangle_mesh.draw(frame.commandBuffer);
			vulkan.endRenderPass(frame);

			vulkan.endFrame(frame);

			fps.update(window.time());
		}

		// Leave the cursor on a fresh line once the loop exits.
		fps.finish();
		device.waitIdle(); // Wait for the device to finish before tearing down the context and window
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}

struct Camera {
	int   recursion_depth;
	float zoom;
	float aspect_ratio;
};
int sierpinski_triangle() {
	// setup
	engine::GLFWContext   glfw;
	engine::Window        window({ 500, 500, "Sierpinski Triangle", true });
	engine::VulkanDevice  device(window.handle(), "Engine");
	engine::VulkanContext vulkan(device, &window);

	// create pipeline
	engine::PipelineConfigInfo pipelineConfig = engine::Pipeline::defaultPipelineConfigInfo();

	const engine::PushConstant cameraPush = engine::pushConstant<Camera>(VK_SHADER_STAGE_FRAGMENT_BIT);
	engine::PipelineLayout     pipelineLayout(device, { cameraPush.range() });
	pipelineConfig.pipelineLayout = pipelineLayout.handle();
	pipelineConfig.renderPass     = vulkan.renderPass();
	engine::Pipeline pipeline(device,
	                          "shaders/sierpinski_triangle.vert.spv",
	                          "shaders/sierpinski_triangle.frag.spv",
	                          pipelineConfig);

	// "globals"
	Camera camera{ 0, 1.0f, 1.0f };

	// debug tools
	tools::FPSCounter fps(window.time());

	// main loop
	while (!window.shouldClose()) {
		window.pollEvents();

		// input
		if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
			glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
		}
		if (window.keyPressed(GLFW_KEY_RIGHT)) {
			camera.recursion_depth++;
			std::printf("Recursion depth: %d\n", camera.recursion_depth);
			std::printf("Number of triangles: %lld\n", (long long)pow(3, camera.recursion_depth));
		}
		if (window.keyPressed(GLFW_KEY_LEFT) && camera.recursion_depth > 0) {
			camera.recursion_depth--;
			std::printf("Recursion depth: %d\n", camera.recursion_depth);
			std::printf("Number of triangles: %lld\n", (long long)pow(3, camera.recursion_depth));
		}

		const engine::MousePosition scroll = window.consumeScrollDelta();
		camera.zoom                        = camera.zoom * std::exp(static_cast<float>(scroll.y) * 0.1f);

		// create frame
		const engine::Frame frame = vulkan.beginFrame();
		if (frame.commandBuffer == VK_NULL_HANDLE) {
			continue;
		}
		camera.aspect_ratio = static_cast<float>(vulkan.extent().width) /
		                      static_cast<float>(vulkan.extent().height);
		vulkan.beginRenderPass(frame, { 1.0f, 1.0f, 1.0f, 1.0f });

		// setup draw
		pipeline.bind(frame.commandBuffer);
		vulkan.setViewportAndScissor(frame.commandBuffer);

		// push constant data
		pipelineLayout.pushConstant(frame.commandBuffer, cameraPush, camera);

		// draw
		pipeline.draw(frame.commandBuffer, 3);

		// end frame
		vulkan.endRenderPass(frame);
		vulkan.endFrame(frame);
		fps.update(window.time());
	}
	fps.finish();
	device.waitIdle();
	return 0;
}

struct texture_vertex {
	float position[2];
	float color[3];
	float uv[2]; // a uv is a 2D coordinate that maps a point on the texture to a point on the mesh
};
int image_texture_example() {
	std::printf("Running Image Texture Example\n");
	try {
		std::string           app_name = "Image Texture Example";
		engine::GLFWContext   glfw;
		engine::Window        window({ 800, 600, app_name, true });
		engine::VulkanDevice  device(window.handle(), app_name);
		engine::VulkanContext vulkan(device, &window);

		// set up texture and the default sampled-image descriptor set
		engine::ImageTexture         texture(device, "../assets/smiley_0.png");
		engine::TextureDescriptorSet textureDescriptor(device, texture);

		// pipeline setup
		engine::PipelineLayout     pipeline_layout(device, { textureDescriptor.layout() }, {});
		engine::PipelineConfigInfo pipeline_Config = engine::Pipeline::defaultPipelineConfigInfo();

		// vertex input description
		engine::VertexInputDescription vertexInput;
		vertexInput.addBinding(0, sizeof(texture_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(texture_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(texture_vertex, color))
		    .addAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(texture_vertex, uv));
		pipeline_Config.setVertexInput(vertexInput)
		    .setRenderPass(vulkan.renderPass())
		    .setPipelineLayout(pipeline_layout.handle());

		engine::Pipeline pipeline(device,
		                          "shaders/simple_texture.vert.spv",
		                          "shaders/simple_texture.frag.spv",
		                          pipeline_Config);

		std::vector<texture_vertex> vertices = {
			{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 0.5f, -0.5f },  { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 0.5f, 0.5f },   { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -0.5f, 0.5f },  { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } }
		};
		const std::vector<std::uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
		engine::Mesh                     mesh(device, vertices, indices);

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			vulkan.beginRenderPass(frame, { 0.0f, 0.0f, 0.0f, 0.0f });

			mesh.bind(frame.commandBuffer);
			textureDescriptor.bind(frame.commandBuffer, pipeline_layout.handle());

			pipeline.bind(frame.commandBuffer);
			vulkan.setViewportAndScissor(frame.commandBuffer);

			mesh.draw(frame.commandBuffer);

			vulkan.endRenderPass(frame);
			vulkan.endFrame(frame);
		}
		device.waitIdle();

	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
	}
	return 0;
}

struct ubo_vertex {
	float position[3];
	float color[3];
};
struct UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 projection;
};
int uniform_buffer_example() {
	std::printf("Running Uniform Buffer Example\n");
	try {
		std::string           app_name = "Uniform Buffer Example";
		engine::GLFWContext   glfw;
		engine::Window        window({ 800, 600, app_name, true });
		engine::VulkanDevice  device(window.handle(), app_name);
		engine::VulkanContext vulkan(device, &window);

		// upload the quad and create the uniform buffer that holds the MVP matrices
		const std::vector<ubo_vertex> vertices = {
			{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f },  { 0.0f, 1.0f, 0.0f } },
			{ { 0.5f, 0.5f, 0.0f },   { 0.0f, 0.0f, 1.0f } },
			{ { -0.5f, 0.5f, 0.0f },  { 1.0f, 1.0f, 1.0f } }
		};
		const std::vector<std::uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
		engine::Mesh                     mesh(device, vertices, indices);

		UniformBufferObject ubo{};
		glm_mat4_identity(ubo.model);
		glm_mat4_identity(ubo.view);
		glm_mat4_identity(ubo.projection);
		engine::UniformBuffer uniform_buffer(device, ubo);

		// set up the default uniform buffer descriptor set
		engine::UBODescriptorSet uniform_descriptor(device, uniform_buffer);

		// pipeline setup
		engine::PipelineLayout     pipeline_layout(device, { uniform_descriptor.layout() }, {});
		engine::PipelineConfigInfo pipeline_Config = engine::Pipeline::defaultPipelineConfigInfo();

		// vertex input description
		engine::VertexInputDescription vertexInput;
		vertexInput.addBinding(0, sizeof(ubo_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ubo_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ubo_vertex, color));
		pipeline_Config.setVertexInput(vertexInput)
		    .setRenderPass(vulkan.renderPass())
		    .setPipelineLayout(pipeline_layout.handle());

		engine::Pipeline pipeline(device,
		                          "shaders/ubo.vert.spv",
		                          "shaders/ubo.frag.spv",
		                          pipeline_Config);

		while (!window.shouldClose()) {
			window.pollEvents();
			if (window.keyPressed(GLFW_KEY_ESCAPE) || window.keyPressed(GLFW_KEY_Q)) {
				glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
			}

			const engine::Frame frame = vulkan.beginFrame();
			if (frame.commandBuffer == VK_NULL_HANDLE) {
				continue;
			}

			// update the model, view, and projection matrices for this frame
			const float time = static_cast<float>(window.time());
			glm_mat4_identity(ubo.model);
			vec3 rotationAxis = { 1.0f, 1.0f, 0.0f };
			glm_rotate(ubo.model, time * glm_rad(45.0f), rotationAxis);

			vec3 eye    = { 0.0f, 0.0f, 2.5f };
			vec3 center = { 0.0f, 0.0f, 0.0f };
			vec3 up     = { 0.0f, 1.0f, 0.0f };
			glm_lookat(eye, center, up, ubo.view);

			const float aspect_ratio = static_cast<float>(vulkan.extent().width) /
			                           static_cast<float>(vulkan.extent().height);
			glm_perspective(glm_rad(45.0f), aspect_ratio, 0.1f, 100.0f, ubo.projection);

			uniform_buffer.update(ubo);

			vulkan.beginRenderPass(frame, { 0.0f, 0.0f, 0.0f, 0.0f });

			mesh.bind(frame.commandBuffer);
			uniform_descriptor.bind(frame.commandBuffer, pipeline_layout.handle());

			pipeline.bind(frame.commandBuffer);
			vulkan.setViewportAndScissor(frame.commandBuffer);

			mesh.draw(frame.commandBuffer);

			vulkan.endRenderPass(frame);
			vulkan.endFrame(frame);
		}
		device.waitIdle();

	} catch (const std::exception &error) {
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
	}
	return 0;
}

// -----------------------------------------------------------------------------
// Full showcase application
//
// This example combines the engine features in one program: window and input
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
int showcase_example() {
	std::printf("Running Full Showcase Example\n");
	std::printf("Controls: Space pause | R reset | drag left mouse to tilt | scroll zoom | Shift speed up | Q/Esc quit\n");
	try {
		std::string           app_name = "Full Showcase Example";
		engine::GLFWContext   glfw;
		engine::Window        window({ 900, 650, app_name, true });
		engine::VulkanDevice  device(window.handle(), app_name);
		engine::VulkanContext vulkan(device, &window);

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

		engine::Mesh mesh(device, vertices, indices);

		showcase_ubo ubo{};
		glm_mat4_identity(ubo.model);
		glm_mat4_identity(ubo.view);
		glm_mat4_identity(ubo.projection);
		engine::UniformBuffer uniform_buffer(device, ubo);

		// -------------------------------------------------------------------------
		// Descriptors: the texture uses set 0 and the uniform buffer uses set 1.
		// Both convenience types own their layout, pool, and descriptor set.
		// -------------------------------------------------------------------------
		engine::ImageTexture         texture(device, "../assets/smiley_0.png");
		engine::TextureDescriptorSet texture_descriptor(device, texture);
		engine::UBODescriptorSet     ubo_descriptor(device, uniform_buffer);

		// Push constants carry per-draw data without a descriptor or buffer.
		const engine::PushConstant push_constant = engine::pushConstant<showcase_push_constants>(
		    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		// The initializer-list order defines the set index: texture=0, UBO=1.
		engine::PipelineLayout pipeline_layout(device,
		                                       { texture_descriptor.layout(), ubo_descriptor.layout() },
		                                       { push_constant.range() });

		// -------------------------------------------------------------------------
		// Pipeline: describe the vertex layout and start from the default
		// fixed-function state, then connect the layout and render pass.
		// -------------------------------------------------------------------------
		engine::PipelineConfigInfo pipeline_config = engine::Pipeline::defaultPipelineConfigInfo();

		engine::VertexInputDescription vertex_input;
		vertex_input.addBinding(0, sizeof(showcase_vertex))
		    .addAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(showcase_vertex, position))
		    .addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(showcase_vertex, color))
		    .addAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(showcase_vertex, uv));
		pipeline_config.setVertexInput(vertex_input)
		    .setRenderPass(vulkan.renderPass())
		    .setPipelineLayout(pipeline_layout.handle());

		engine::Pipeline pipeline(device,
		                          "shaders/showcase.vert.spv",
		                          "shaders/showcase.frag.spv",
		                          pipeline_config);

		// -------------------------------------------------------------------------
		// Application state used by the controls below.
		// -------------------------------------------------------------------------
		showcase_push_constants push{ { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f };
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
			const engine::MousePosition scroll = window.consumeScrollDelta();
			distance = glm_clamp(distance * std::exp(static_cast<float>(-scroll.y) * 0.15f), 1.5f, 6.0f);

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
			const engine::Frame frame = vulkan.beginFrame();
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
				const engine::MousePosition cursor = window.cursorPosition();
				const float cursor_x = static_cast<float>(cursor.x) /
				                       static_cast<float>(window.width()) - 0.5f;
				const float cursor_y = static_cast<float>(cursor.y) /
				                       static_cast<float>(window.height()) - 0.5f;
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

			// The descriptor points at the buffer's full range, so updating the
			// buffer contents is enough to update what the shader reads.
			uniform_buffer.update(ubo);

			// Clear to a dark background and record the frame.
			vulkan.beginRenderPass(frame, { 0.05f, 0.06f, 0.08f, 1.0f });

			mesh.bind(frame.commandBuffer);
			texture_descriptor.bind(frame.commandBuffer, pipeline_layout.handle(), 0);
			ubo_descriptor.bind(frame.commandBuffer, pipeline_layout.handle(), 1);

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
		std::fprintf(stderr, "Fatal error: %s\n", error.what());
		return 1;
	}
	return 0;
}
