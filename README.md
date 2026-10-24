
# Vulkan Boilerplate Engine

## Description

This project is a small C++ engine layer for Vulkan.

It handles the setup that most Vulkan programs need:

- GLFW initialization and window creation
- Vulkan instance and validation layers
- Physical and logical device selection
- Graphics and presentation queues
- Swapchain creation and recreation
- Render passes and framebuffers
- Command buffers and frame synchronization
- Vertex and index buffers
- Pipeline layouts and graphics pipelines

The code keeps Vulkan objects visible and explicit. It reduces repeated setup
without hiding how Vulkan works.

`src/main.cpp` contains examples of how to use the engine. The engine code is
in `src/glfw`, `src/tools`, and `src/vulkan`.

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
asset loading, cameras, or an entity system.

## TODO

- OBJ reader
- Camera Abstraction
- DOD Refactor
- Fill this out more

## Technical overview

The code uses RAII to manage Vulkan and GLFW objects. Objects are created in
dependency order and destroyed in reverse order.

`VulkanDevice` owns the Vulkan instance, selected physical device, logical
device, and shared queues. `VulkanContext` owns resources connected to one
window, including its surface, swapchain, render pass, framebuffers, command
pool, and synchronization objects.

`Buffer` provides the common buffer and memory code. `VertexBuffer` and
`IndexBuffer` add the binding operations needed for drawing.

`PipelineLayout` describes push constants and descriptor set layouts.
`Pipeline` loads SPIR-V shader files and creates a graphics pipeline from
configurable fixed-function state.

The current buffer implementation uses host-visible, host-coherent memory.
That keeps data upload simple. A larger renderer would usually upload through
a staging buffer into device-local memory.

Shaders are compiled from `src/shaders` into SPIR-V files by CMake. The
project uses C++20, GLFW, Vulkan, and `glslc` or `glslangValidator`.

## Workflow

A program using the engine normally follows this order:

1. Create a `GLFWContext`.
2. Create a `Window`.
3. Create a `VulkanDevice` using the window.
4. Create a `VulkanContext`.
5. Create pipeline layouts, pipelines, and buffers.
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
