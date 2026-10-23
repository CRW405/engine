
# Vulkan boilerplate engine

Meant to be a project to learn Vulkan but also set myself up for more complex projects where I dont have to worry about boilerplate as much.

## TODO

- Sierpinski triangle shader
- Renderer encapsulation // kinda, still want to keep things lowish level and explicit
- Scene abstraction // may save this for a seperate project

## Notes

### Vulkan mental model

Vulkan is an explicit API. It does not try to infer what the application
intends to do, and it does not automatically protect the application from
using resources at the wrong time. The application is responsible for
describing resources, recording work, synchronizing access, and releasing
objects in a valid order.

A useful high-level flow is:

1. Create an instance to connect the application to the Vulkan loader and
   available implementation.
2. Create a surface that connects Vulkan to the window system.
3. Select a physical device whose queues and features support the application.
4. Create a logical device and retrieve the queues used to submit work.
5. Create a swapchain containing images that can be presented to the window.
6. Create image views, render passes, framebuffers, pipelines, and buffers.
7. Record command buffers and submit them to GPU queues.
8. Present completed swapchain images.

The bootstrap separates these responsibilities into two broad ownership
groups. `VulkanDevice` owns the instance, selected physical device, logical
device, and shared queues. `VulkanContext` owns resources tied to one window:
the surface, swapchain, render pass, framebuffers, command pool, and frame
synchronization objects.

### CPU and GPU execution

The CPU and GPU are separate execution systems. The CPU prepares data and
records commands, while the GPU consumes those commands asynchronously. A
function returning after `vkQueueSubmit` does not mean that the GPU has
finished the work. It means the work has been placed into a queue.

This distinction explains several common Vulkan rules:

- A resource must remain alive until the GPU is finished using it.
- Command buffers describe future work; recording a command does not execute it.
- Queue submission and presentation require explicit synchronization.
- The CPU may need to wait for a fence before reusing a command buffer or
  per-frame resource.

The main loop uses two frames in flight. Each frame slot has a command buffer,
an image-available semaphore, and a fence. The swapchain images also track
which fence currently represents work using each image. This separates CPU
progress from the number of images owned by the presentation system.

### Queues and command buffers

A queue is a submission endpoint owned by the logical device. Queue families
advertise capabilities such as graphics, compute, transfer, and presentation.
A graphics queue can execute draw commands, but presentation support must be
checked separately because it depends on the selected surface.

Command buffers are recorded descriptions of GPU work. Recording is organized
around the state needed by later commands: bind a pipeline, bind buffers,
begin a render pass, set dynamic state, and issue a draw. The order matters
because many commands modify state that subsequent commands consume.

The current triangle follows this conceptual sequence:

1. Acquire a swapchain image.
2. Begin recording the frame command buffer.
3. Begin a render pass targeting that image's framebuffer.
4. Bind the graphics pipeline.
5. Set the viewport and scissor.
6. Bind the vertex buffer.
7. Issue the draw command.
8. End the render pass and command buffer.
9. Submit the command buffer and present the image.

### Resources and memory

Vulkan separates a resource from the memory backing that resource. A
`VkBuffer` describes how data will be used, while `VkDeviceMemory` provides
storage. The application must select a memory type whose allowed bit and
properties match the intended access pattern.

The current vertex buffer uses host-visible, host-coherent memory because it is
easy to understand and useful for an initial learning implementation. A more
performance-oriented renderer usually stages data through a CPU-visible buffer
and copies it into device-local memory using a transfer command. Device-local
memory is generally faster for the GPU, while host-visible memory is easier for
the CPU to update.

For every resource, ask:

- Who creates it?
- Who owns it?
- Which queue uses it?
- How long must it stay alive?
- Which synchronization guarantees safe reuse or destruction?

These questions are more important than the Vulkan handle itself. A handle is
only a reference to an implementation-owned object; it does not communicate
ownership or lifetime by itself.

### Pipelines and shaders

A graphics pipeline combines shader stages with fixed-function state. The
vertex shader transforms input positions and passes values forward. The
fragment shader determines the color written to the framebuffer. Between them,
the pipeline describes how vertex data is interpreted, assembled, rasterized,
and blended.

Pipeline creation is relatively expensive compared with binding an existing
pipeline. Vulkan therefore encourages creating pipelines ahead of rendering
and reusing them during command recording. The pipeline layout defines the
interface between shaders and application-provided resources such as descriptor
sets and push constants.

Shader interface locations must agree with the vertex input descriptions. In
this project, vertex location `0` contains the position and location `1`
contains the color. The CPU-side structure, vertex attribute descriptions, and
shader declarations are three views of the same data contract.

### Synchronization and image layout

Synchronization answers two questions:

1. Has an earlier operation completed?
2. Is the resource in a state and layout suitable for the next operation?

Semaphores coordinate work between queue operations and presentation. Fences
let the CPU observe completion of submitted work. Pipeline barriers and render
pass dependencies describe ordering and access relationships inside GPU work.

Image layouts are part of this contract. A swapchain image is acquired for
rendering, used as a color attachment, and transitioned into a presentation
layout. Vulkan requires the application to make these transitions explicit
enough that the implementation can schedule access safely.

Synchronization should be precise rather than global. Waiting for the whole
device to become idle is useful during shutdown and swapchain recreation, but
using it for every frame would prevent useful CPU/GPU overlap.

### Swapchain recreation

The swapchain belongs to the window system and can become invalid when a
window is resized, minimized, or otherwise changes presentation conditions.
Recreation waits for a valid framebuffer size, waits for device work to finish,
destroys dependent resources, and creates them again with the new extent.

Resources that depend on the swapchain include image views, framebuffers, and
often render-pass-compatible pipelines. A resize that changes only the extent
can preserve compatible pipeline state, while a format change may require
recreating the render pass and graphics pipelines.

### Learning direction

The bootstrap intentionally keeps the application loop visible. Future
experiments can build on this foundation by adding:

- descriptor sets and uniform buffers;
- staging buffers and device-local resources;
- depth testing and depth images;
- texture uploads and samplers;
- multiple render passes or dynamic rendering;
- compute pipelines and storage buffers;
- explicit barriers and more specialized queue usage.

When adding a feature, first identify its data flow and lifetime. Then decide
which object should own it, which queue will use it, and what synchronization
is required. This keeps the abstraction useful without turning Vulkan's
important concepts into invisible magic.
