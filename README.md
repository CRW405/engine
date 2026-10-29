
# Oxide

## Description

Oxide is a small C++ engine layer for Vulkan.

It handles the setup that most Vulkan programs need:

- GLFW initialization and window creation
- Vulkan instance and validation layers
- Physical and logical device selection
- Graphics and presentation queues
- Swapchain creation and recreation
- Render passes and framebuffers
- Command buffers and frame synchronization
- Vertex and index buffers
- Texture loading and sampled image descriptor sets
- Uniform buffers and uniform buffer descriptor sets
- Pipeline layouts and graphics pipelines

The code keeps Vulkan objects visible and explicit. It reduces repeated setup
without hiding how Vulkan works.

While Oxide does a lot of the heavy lifting and provides some useful defaults,
manual control is still recommended. Oxide is just intended to accelerate setup
and allow for more rapid experimentation and prototyping.

Oxide is built as the `oxide` library from `src/glfw`, `src/tools`,
and `src/vulkan`. Programs that use it live under `examples/`, where each
example is a standalone executable with its own `main.cpp` and `shaders/`
directory:

- `examples/triangle` — rotating indexed triangle with a transform push constant
- `examples/sierpinski` — fragment-shader fractal driven by push constants
- `examples/image_texture` — textured quad with a sampled image descriptor
- `examples/uniform_buffer` — rotating quad using model-view-projection uniforms
- `examples/showcase` — all of the above combined, including input handling and FPS reporting

## Use Case

This project is useful for learning Vulkan and for starting small rendering
experiments without rewriting the same initialization code each time.

It is aimed at programs that need direct control over:

- GPU resources
- Command recording
- Shader interfaces
- Synchronization
- Swapchain behavior

It is not a complete game engine. It does not provide scenes, materials,
asset loading, cameras, or an entity system. (yet (maybe))

## Building

```sh
cmake -S . -B build
cmake --build build -j
```

This builds the `oxide` library and every example. Run an example directly:

```sh
./build/examples/showcase/showcase
```

or through its convenience target:

```sh
cmake --build build --target run_showcase
```

The same `run_` prefix works for `triangle`, `sierpinski`, `image_texture`, and
`uniform_buffer`. Pass `-DOXIDE_BUILD_EXAMPLES=OFF` to configure the library
without building any examples.

Vulkan validation layers and Oxide diagnostics are both enabled by default.
For a lean release build, disable either or both:

```sh
cmake -S . -B build \
    -DOXIDE_ENABLE_VALIDATION=OFF \
    -DOXIDE_ENABLE_LOGGING=OFF
cmake --build build -j
```

Disabling validation skips the validation layer and debug messenger. Disabling
logging compiles the `OXIDE_LOG_*` statements down to no-ops.

## Diagnostics

All engine diagnostics go to stderr through the small logger in
`src/tools/log.hpp`. Each line carries a severity, the source location that
produced it, and a message:

```
[oxide][INFO] vulkan_device.cpp:337: selected Vulkan device: AMD Radeon RX 9070 XT
[oxide][WARNING] vulkan_context.cpp:113: preferred surface format B8G8R8A8_SRGB is unavailable; using format 44
[oxide][ERROR] buffer.cpp:24: create buffer failed with VK_ERROR_OUT_OF_DEVICE_MEMORY (-2)
[oxide][FATAL] main.cpp:90: unhandled exception: image_texture.cpp:143: failed to load texture image '...'
```

Severity tags are colorized when stderr is a terminal. Set `NO_COLOR=1` to
disable colors for logs captured to a file or another program.

Warnings and errors from the Vulkan validation layer use the same format with a
`Vulkan validation` source label. The debug messenger subscribes only to warning
and error severities so loader info messages do not bury real problems.
`-DOXIDE_ENABLE_VALIDATION=OFF` skips the layer and debug messenger entirely.
Validation output also goes through the logger, so `-DOXIDE_ENABLE_LOGGING=OFF`
suppresses it as well.

Exceptions thrown by the engine embed their throw site in `what()`. Example
programs catch `std::exception` and report it with `OXIDE_LOG_FATAL`, then exit
with a non-zero status. New diagnostics should use the `OXIDE_LOG_*` macros so
they automatically report the call site. With `-DOXIDE_ENABLE_LOGGING=OFF`,
those macros compile to no-ops, but exception behavior is unchanged.

## TODO

- OBJ reader
- GLTF reader
- DOD Refactor

## Technical overview

### Architecture

The code uses RAII to manage Vulkan and GLFW objects. Objects are created in
dependency order and destroyed in reverse order. The public API lives in the
`oxide` namespace.

`VulkanDevice` owns the Vulkan instance, selected physical device, logical
device, and shared queues. `VulkanContext` owns resources connected to one
window, including its surface, swapchain, render pass, framebuffers, command
pool, and synchronization objects.

`Buffer` provides the common buffer and memory code. `VertexBuffer` and
`IndexBuffer` add the binding operations needed for drawing. `UniformBuffer`
wraps a typed value with the uniform-buffer usage flag. `Mesh` pairs a vertex and
index buffer and records the common indexed draw with `bind()` and `draw()`.

`DescriptorSetLayout`, `DescriptorPool`, and `DescriptorSet` provide the shared
descriptor plumbing. `combinedImageSampler()` and `uniformBuffer()` are the
default bindings for the two common single-resource cases; build a
`DescriptorSetLayout` from explicit `DescriptorBinding` values for more complex
interfaces.

`ImageTexture` loads an image and owns its image view and sampler.
`TextureDescriptorSet` uses the shared descriptor wrappers to own the layout,
pool, and set needed to sample that texture. Its defaults describe one combined
image sampler at binding `0` in the fragment stage. Pass a different binding or
stage to the constructor when a shader needs another interface. `bind()` records
the set through a pipeline layout.

`UBODescriptorSet` follows the same pattern for uniform buffers. Its defaults
describe one uniform buffer at binding `0` in the vertex stage, which is enough
for static data or data the application synchronizes itself. For data updated
every frame, use `UniformBufferRing`: it owns one buffer and descriptor set per
frame in flight. Update and bind by `Frame::frameIndex`; `beginFrame()` has
already waited on that slot's fence, so the CPU never overwrites a buffer that
the previous frame is still reading.

`PipelineLayout` describes push constants and descriptor set layouts.
`pushConstant<T>()` creates a `PushConstant` that carries its range, stage flags,
and typed `push()` call so the layout and recorded data stay in sync.
`Pipeline` loads SPIR-V shader files and creates a graphics pipeline from
configurable fixed-function state. `defaultPipelineConfigInfo()` provides a
sensible starting point, while the pipeline config fields stay public and can be
overridden for custom rendering state.

The current buffer implementation uses host-visible, host-coherent memory.
That keeps data upload simple. A larger renderer would usually upload through
a staging buffer into device-local memory.

Each example keeps its GLSL sources in `examples/<name>/shaders`. CMake
compiles them into SPIR-V next to the example binary and passes the output
directory and the shared `assets/` directory to the program as
`OXIDE_SHADER_DIR` and `OXIDE_ASSET_DIR`, so binaries can be launched from
any working directory. The project uses C++20, GLFW, Vulkan, and `glslc` or
`glslangValidator`.

### File map

The following map lists the engine sources, build files, assets, and examples by
directory.

#### Build and configuration

- `CMakeLists.txt` — Builds `oxide` as a static library, publishes its include
  directories and dependencies, defines the validation/logging compile options,
  and optionally adds the examples.
- `examples/CMakeLists.txt` — Declares each example executable, compiles its
  GLSL sources to SPIR-V, sets `OXIDE_SHADER_DIR` and `OXIDE_ASSET_DIR`, and adds
  the `run_<example>` convenience targets.
- `src/oxide_config.hpp` — Defaults for the compile-time
  `OXIDE_ENABLE_VALIDATION` and `OXIDE_ENABLE_LOGGING` switches. CMake defines
  them as `0` or `1`.
- `assets/smiley_0.png` — Shared texture used by the image and showcase examples.

#### GLFW layer (`src/glfw`)

- `src/glfw/glfw_oxide.{hpp,cpp}` — Declares and implements `WindowConfig`,
  `MousePosition`, `GLFWContext`, and `Window`. The header exposes the window
  API and input state; the source handles GLFW initialization/termination,
  window creation/destruction, callback dispatch, key/scroll tracking, and the
  process-wide GLFW error callback. A static handle-to-wrapper map lets GLFW
  callbacks find the owning `Window`.

#### Utilities (`src/tools`)

- `src/tools/log.{hpp,cpp}` — Declares the log levels, `LogLine`, located
  exception factories, and `OXIDE_LOG_*` macros; implements stderr formatting,
  ANSI/`NO_COLOR` handling, and `file:line` exception messages. Disabled logging
  builds compile the macros to a `NullLogLine` sink.
- `src/tools/fps.{hpp,cpp}` — Declares and implements `FPSCounter`, which
  accumulates frame counts, reports once per second, and overwrites the current
  terminal line.

#### Vulkan core (`src/vulkan`)

- `src/vulkan/vulkan_device.{hpp,cpp}` — Declares `QueueFamilies` and
  `VulkanDevice` and implements instance/device setup. The object owns the
  instance, selected physical and logical devices, graphics/present queues,
  debug messenger, and shared command pool; the source creates the instance,
  optionally enables validation, enumerates devices/extensions/layers, selects
  queues, creates the logical device, and resolves memory types.
- `src/vulkan/vulkan_context.{hpp,cpp}` — Declares `Frame`,
  `MAX_FRAMES_IN_FLIGHT`, and `VulkanContext` and implements per-window
  presentation. The context owns the surface, swapchain, image views, render
  pass, framebuffers, command pool/buffers, semaphores, and fences, and handles
  the frame lifecycle plus resize/out-of-date recreation.
- `src/vulkan/vk_error.hpp` — Maps `VkResult` values to names and provides
  `checkVk`, `vkError`, and `logVkFailure` helpers so Vulkan failures report the
  operation, result code, and call site.
- `src/vulkan/buffer.{hpp,cpp}` — Declares `Buffer`, `VertexBuffer`,
  `IndexBuffer`, `UniformBuffer`, and `Mesh`; implements creation, host-visible
  allocation, binding, mapping, upload/update, and mesh bind/draw helpers.
- `src/vulkan/descriptor_set.{hpp,cpp}` — Declares `DescriptorBinding`, the
  `combinedImageSampler()` and `uniformBuffer()` factories, and the
  `DescriptorSetLayout`, `DescriptorPool`, and `DescriptorSet` RAII wrappers;
  implements layout/pool creation, set allocation, and command-buffer binding.
- `src/vulkan/ubo_descriptor.{hpp,cpp}` — Single uniform-buffer wrapper with one
  layout, pool, and set; the implementation points the descriptor write at the
  supplied buffer's full range. Best for static or externally synchronized data.
- `src/vulkan/uniform_buffer_ring.{hpp,cpp}` — Per-frame UBO helper with one
  buffer and descriptor set per frame slot plus a shared layout/pool; implements
  allocation, descriptor writes, and frame-indexed update/bind operations. This
  is the safe path for data rewritten every frame.
- `src/vulkan/texture_descriptor.{hpp,cpp}` — Combined-image-sampler wrapper
  owning one layout, pool, and set for an `ImageTexture`; the implementation
  writes the image view and sampler into the set.
- `src/vulkan/image_texture.{hpp,cpp}` — Declares and implements `ImageTexture`,
  which owns a `VkImage`, memory, view, and sampler. It loads with stb_image,
  uploads through a staging buffer, transitions layout, creates the view and
  sampler, and cleans up partial resources on failure.
- `src/vulkan/pipeline_layout.{hpp,cpp}` — Declares `PushConstant`,
  `pushConstant<T>()`, `pushConstantRange<T>()`, and `PipelineLayout`; implements
  layout creation/destruction, typed push helpers, and descriptor-set binding.
- `src/vulkan/render_pipeline.{hpp,cpp}` — Declares `VertexInputDescription`,
  `PipelineConfigInfo`, and `Pipeline`; implements pointer repair for copied
  configs, default fixed-function state, SPIR-V loading, shader modules, pipeline
  creation, and bind/draw helpers.

#### Examples (`examples/`)

- `examples/triangle/main.cpp` — Rotating indexed triangle. Demonstrates vertex
  and index buffers, a transform push constant, and the basic frame loop.
- `examples/triangle/shaders/vulkan_triangle.vert` — Applies the push-constant
  transform and forwards vertex color.
- `examples/triangle/shaders/vulkan_triangle.frag` — Writes the interpolated
  vertex color.
- `examples/sierpinski/main.cpp` — Fractal demo driven by iteration depth, zoom,
  and aspect-ratio push constants.
- `examples/sierpinski/shaders/sierpinski_triangle.vert` — Emits a full-screen
  triangle and passes its coordinates to the fragment stage.
- `examples/sierpinski/shaders/sierpinski_triangle.frag` — Evaluates the
  iterated function system per fragment and colors the fractal.
- `examples/image_texture/main.cpp` — Textured quad using `ImageTexture` and
  `TextureDescriptorSet`; loads the shared smiley asset.
- `examples/image_texture/shaders/simple_texture.vert` — Passes position, color,
  and texture coordinates, flipping V for the stb/Vulkan conventions.
- `examples/image_texture/shaders/simple_texture.frag` — Samples the combined
  image sampler.
- `examples/uniform_buffer/main.cpp` — Rotating quad using per-frame MVP uniform
  buffers via `UniformBufferRing`.
- `examples/uniform_buffer/shaders/ubo.vert` — Applies model/view/projection
  from descriptor set `0`.
- `examples/uniform_buffer/shaders/ubo.frag` — Writes interpolated vertex color.
- `examples/showcase/main.cpp` — Combined demo with texture, per-frame UBO,
  push constants, input, and FPS output.
- `examples/showcase/shaders/showcase.vert` — Uses the set-1 UBO for MVP, adds a
  push-constant wave, and forwards color and texture coordinates.
- `examples/showcase/shaders/showcase.frag` — Combines texture and vertex color
  and applies the push-constant tint.

## Workflow

A program using Oxide normally follows this order:

1. Create a `GLFWContext`.
2. Create a `Window`.
3. Create a `VulkanDevice` using the window.
4. Create a `VulkanContext`.
5. Create meshes, textures, uniform buffers, descriptor sets, pipeline layouts, and pipelines.
6. Start the render loop.
7. Poll window events and read input.
8. Call `beginFrame()`.
9. Begin a render pass and set the viewport and scissor.
10. Bind pipelines and buffers, then issue draw commands.
11. End the render pass and call `endFrame()`.
12. Wait for the device to become idle before resources are destroyed.

If the window changes size, the context recreates the swapchain and the
resources that depend on it. `beginFrame()` returns an empty frame while that
work is being done, so the application can skip that iteration.

## Theory

### General graphics programming

The CPU prepares data and commands. The GPU processes those commands later.
Creating or recording a command does not mean that the GPU has finished it.

A basic draw needs:

1. Vertex data
2. A description of the vertex data layout
3. A graphics pipeline
4. A render target
5. Recorded draw commands
6. Synchronization between the CPU, GPU, and display system

A graphics pipeline turns vertex data into pixels. The vertex shader processes
vertices and produces positions. Rasterization turns primitives into fragments.
The fragment shader determines the color of each fragment. The resulting
pixels are written to a framebuffer.

Shader inputs and CPU data must agree. In the triangle example, vertex
location `0` is a two-component position and location `1` is a three-component
color. The C++ structure, Vulkan vertex descriptions, and shader declarations
all describe the same data.

### Vulkan

Vulkan is an explicit API. The application must state which resources it
uses, how they are connected, when work is submitted, and when resources can
be reused or destroyed.

The main Vulkan objects used here follow this order:

1. An instance connects the application to the Vulkan implementation.
2. A surface connects Vulkan to the window system.
3. A physical device is selected based on queue and swapchain support.
4. A logical device provides the queues used by the application.
5. A swapchain provides images that can be presented to the window.
6. Image views, a render pass, and framebuffers describe the render targets.
7. Command buffers record GPU work.
8. Semaphores and fences coordinate work.
9. The presentation queue displays completed swapchain images.

Semaphores coordinate operations submitted to queues. Fences let the CPU
check whether submitted work has completed. The context uses two frames in
flight so the CPU can prepare another frame while the GPU works on the
previous one.

Swapchain images change layout as they move through the frame. They are
acquired for rendering, used as color attachments, and then transitioned for
presentation. Render pass dependencies and submission synchronization describe
these transitions to Vulkan.

Window resizing can invalidate the swapchain. The context waits for the
device, rebuilds the swapchain-dependent resources, and continues rendering
with the new extent.
