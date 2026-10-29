# Oxide Code Review

**Date:** 2026-10-28
**Scope:** `src/`, `examples/`, `CMakeLists.txt`, `README.md`
**Baseline:** commit `0b48ade` plus local changes

This document records the findings from a full read-through of the library,
the examples, and the build configuration. Line numbers refer to the working
tree at the time of the review and may drift as the code changes.

Severity levels:

- **Critical** — correctness bug that can crash, hang, or corrupt rendering.
- **High** — likely to bite real applications or block adoption.
- **Medium** — design or portability problem with a practical workaround.
- **Low** — polish, consistency, or developer-experience issue.

The project describes itself as an engine *layer* rather than a complete
engine, so some omissions below are deliberate scope decisions. They are still
listed because they affect what the layer can be used for.

---

## Correctness

### C1 — Resource destructors can race in-flight GPU work

**Severity:** Critical
**Where:** `src/vulkan/*`, examples' `main.cpp`

Every wrapper destroys its Vulkan object immediately in its destructor.
`VulkanDevice::~VulkanDevice()` waits idle only at the very end. Because
resources are declared *after* the device and context, they are destroyed
*first*. The examples call `device.waitIdle()` manually on the normal path, but
on an exception after `endFrame()` (present failure, acquire failure, device
loss) stack unwinding destroys the resources while frames may still be running.
The examples' `catch` block is outside the scope, so it cannot call
`waitIdle()` either.

**Fix options:** add a device-idle scope guard declared last (destroyed first),
register resources with the device so it can wait and destroy them itself, or
make destructors wait in debug builds at minimum.

### C2 — Minimized-window close hangs forever

**Severity:** High
**Where:** `src/vulkan/vulkan_context.cpp`, `waitForValidFramebufferSize()`

The loop waits with `glfwWaitEvents()` while the framebuffer size is `0x0`. If
the user closes a minimized window, the close event wakes the loop but the size
stays zero, so it blocks again with no further events. The loop never checks
`shouldClose()`.

**Fix:** check the window close flag each iteration and return/throw, or use
`glfwWaitEventsTimeout` and a bounded wait.

### C3 — Swapchain format change silently invalidates user pipelines

**Severity:** High
**Where:** `src/vulkan/vulkan_context.cpp`, `recreateSwapchain()`

When the surface format changes, the render pass is destroyed and recreated, but
pipelines referencing the old render pass are left dangling. The library only
logs "pipelines … may be invalid". There is no generation counter or callback,
so an application cannot know to rebuild them. This happens when moving a window
between monitors or toggling HDR.

**Fix:** expose a swapchain generation counter and/or an
`onSwapchainRecreated` callback, and document that pipelines must be recreated
when the render pass changes.

### C4 — Public headers depend on `GLFW_INCLUDE_VULKAN`

**Severity:** High
**Where:** all headers under `src/vulkan/`

The headers rely on `#define GLFW_INCLUDE_VULKAN` before including
`GLFW/glfw3.h`. If a consumer includes `<GLFW/glfw3.h>` first (a common thing to
do), GLFW's include guard prevents the Vulkan header from being pulled in, and
the Oxide headers fail to compile because `VkInstance`, `VkBuffer`, etc. are
undefined. The macro also leaks into every translation unit that includes an
Oxide header.

**Fix:** include `<vulkan/vulkan.h>` explicitly in public headers, and define
`GLFW_INCLUDE_NONE` before including GLFW (or forward-declare `GLFWwindow`).

### C5 — `VulkanContext` claims the GLFW window user pointer

**Severity:** High
**Where:** `src/vulkan/vulkan_context.cpp`

The constructor calls `glfwSetWindowUserPointer(window, this)` for the resize
callback. Applications using the standard GLFW user-pointer pattern will have
their data overwritten, or their callback will interpret a `VulkanContext*` as
their own type. `Window` already uses a static map keyed by `GLFWwindow*`; the
context should use the same approach or store the context on `Window`.

### C6 — Examples use an OpenGL-style projection

**Severity:** High
**Where:** `examples/showcase/main.cpp`, `examples/uniform_buffer/main.cpp`

cglm defaults to a right-handed, negative-one-to-one depth projection
(`CGLM_CLIP_CONTROL_RH_NO`). Vulkan NDC is Y-down with depth in `[0, 1]`. No
example flips Y: the symmetric quads and negated texture coordinates hide it,
but asymmetric 3D geometry renders upside down, and the depth buffer only uses
half of its range.

**Fix:** define `CGLM_FORCE_DEPTH_ZERO_TO_ONE` and either negate
`projection[1][1]` or use a negative viewport height (Vulkan 1.1 /
`VK_KHR_maintenance1`).

### C7 — `PipelineConfigInfo` has two sources of truth

**Severity:** Medium
**Where:** `src/vulkan/render_pipeline.cpp`, `refreshInternalPointers()`

The raw `VkPipelineVertexInputStateCreateInfo` is public, but copy and move
overwrite its pointers and counts from private vectors. A caller that sets
`pVertexBindingDescriptions` / `vertexBindingDescriptionCount` directly will
have those values silently discarded by any copy or move of the config.

**Fix:** encapsulate the vertex input fields, or make the refresh preserve
manually supplied data when the private vectors are empty and document the
rule.

### C8 — `UniformBufferRing::update()` accepts mismatched sizes

**Severity:** Medium
**Where:** `src/vulkan/uniform_buffer_ring.cpp`

`Buffer::update` only rejects out-of-bounds writes, so a smaller struct leaves
the rest of the uniform buffer holding stale bytes. Unlike `Buffer::update`,
the ring's template overload has no pointer guard either, so
`ring.update(0, "text")` compiles and uploads a pointer value.

**Fix:** require `size == size_`, add a pointer `static_assert`, and consider a
typed `UniformBufferRing<T>`.

### C9 — stb implementation lives in a library translation unit

**Severity:** Medium
**Where:** `src/vulkan/image_texture.cpp`

Defining `STB_IMAGE_IMPLEMENTATION` here gives the static library exported
`stbi_*` symbols. A consumer that also defines the macro (normal when using stb
directly) gets duplicate symbol errors at link time.

**Fix:** move the implementation to its own TU and keep it internal, or make the
symbols hidden.

### C10 — `.comp` shaders are compiled but compute is unsupported

**Severity:** Low
**Where:** `examples/CMakeLists.txt`, `src/vulkan/render_pipeline.cpp`

CMake globs `.comp` sources and builds them, but `Pipeline` only accepts vertex
and fragment shader paths and only creates `VK_PIPELINE_BIND_POINT_GRAPHICS`
pipelines.

**Fix:** add a compute pipeline path, or remove `.comp` from the example glob.

### C11 — `Mesh` does not validate indices

**Severity:** Medium
**Where:** `src/vulkan/buffer.hpp`, `Mesh`

Out-of-range indices are passed straight to `vkCmdDrawIndexed`. Robust buffer
access may clamp them, but it is not guaranteed, and the failure is silent.

**Fix:** validate index values against `vertices.size()` in debug builds, or
document the requirement.

### C12 — `checkVk` throws on `VK_INCOMPLETE`

**Severity:** Low
**Where:** `src/vulkan/vk_error.hpp` and the enumeration helpers

Enumerating instance extensions, layers, device extensions, physical devices,
surface formats, and present modes uses the count-then-fill pattern. If the
count grows between the two calls, the second call returns `VK_INCOMPLETE`,
which `checkVk` turns into an exception. The robust pattern is to retry when
the result is `VK_INCOMPLETE`.

---

## Missing capabilities

### Rendering

- **R2 — MSAA, multiple render targets, resolve.** The pipeline config and
  render pass support one color attachment and one sample. No multisampled
  target, resolve attachment, or MRT.
- **R3 — Offscreen and headless rendering.** `VulkanDevice` requires a window
  for device selection and `VulkanContext` requires a surface. There is no way
  to render to a texture, run without a display, or capture a frame.
- **R4 — Compute and advanced drawing.** No compute pipelines, geometry or
  tessellation stages, indirect draws, or instancing in `Mesh` (the pipeline
  has an `instanceCount` parameter, but `Mesh::draw` hardcodes one instance).
- **R5 — Pipeline tooling.** No pipeline cache, specialization constants, or
  shader reflection. Vertex layouts and descriptor bindings are hand-maintained
  and can silently drift from the shaders.
- **R6 — Swapchain configuration.** Format, present mode, image usage, and
  image count are hardcoded. There is no vsync/latency choice, HDR/color-space
  selection, screenshot/readback path, or image array support.
- **R7 — Debugging aids.** No debug object names, command buffer labels, or GPU
  timestamps, which makes RenderDoc and validation output harder to read.

### Memory and resources

- **M1 — No memory allocator (High).** Every buffer and image calls
  `vkAllocateMemory` once. Typical `maxMemoryAllocationCount` is around 4096, so
  larger scenes fail, and fragmentation is unmanaged. A suballocator (or VMA)
  is needed before the API hardens.
- **M2 — Host-visible vertex/index buffers (High).** `Buffer` always requests
  `HOST_VISIBLE | HOST_COHERENT` memory (`src/vulkan/buffer.cpp`). On discrete
  GPUs this streams over PCIe. There is no staging-buffer → device-local path
  and no persistent mapping; every update maps and unmaps.
- **M3 — Textures are minimal.** One format (`R8G8B8A8_SRGB`), one mip level,
  one fixed sampler, file paths only. No mipmap generation, compressed formats,
  cubemaps, array textures, sampler configuration, or in-memory uploads.

### Device and platform

- **D1 — Device selection picks the first suitable device.** `selectPhysicalDevice`
  does not score devices or prefer discrete GPUs, so it can select llvmpipe/CPU
  over a real GPU.
- **D2 — No feature negotiation.** The logical device enables no
  `VkPhysicalDeviceFeatures`. Public pipeline fields such as
  `polygonMode = VK_POLYGON_MODE_LINE` or wide lines will fail validation.
  Query features and either enable or validate the ones the API exposes.
- **D3 — No device-lost or surface-lost recovery.** Those results propagate as
  exceptions; there is no way to recreate the device and its resources.
- **D4 — No MoltenVK/macOS support.** The instance does not request
  `VK_KHR_portability_enumeration` and the device does not enable
  `VK_KHR_portability_subset`, both required on MoltenVK.
- **D5 — API version is hardcoded to 1.0.** That excludes negative viewport
  height, synchronization2, dynamic rendering, and other 1.1+ features.
  `vkEnumerateInstanceVersion` is not used.
- **D6 — Missing validation is a hard failure.** With validation enabled, a
  missing `VK_LAYER_KHRONOS_validation` aborts startup instead of warning and
  continuing.

### Window and input

- **W1 — Window features.** No fullscreen/monitor selection, window icon,
  cursor mode, text input, gamepad support, or DPI/content-scale accessor.
- **W2 — No application-facing resize event.** The context handles resizing
  internally, but apps are only told indirectly via an empty frame. See C3 for
  the related swapchain-generation problem.
- **W3 — Convenience gaps.** No `requestClose()`, no Wayland app ID
  configuration, and examples call `glfwSetWindowShouldClose` directly.

### Packaging and infrastructure

- **P1 — Not consumable as a package (High).** No `install()` rules, exported
  target, `oxideConfig.cmake`, or pkg-config file, so `find_package(oxide)`
  cannot work.
- **P2 — Public include layout.** Headers live under `src/` and are included as
  `"vulkan/buffer.hpp"` or `"tools/log.hpp"`. The roots are generic enough to
  collide with other projects. Prefer `include/oxide/...` plus
  `#include <oxide/...>`.
- **P3 — Undeclared dependencies (High).** Three examples include
  `<cglm/cglm.h>`, but CMake never finds or links cglm. stb is searched only in
  `/usr/include/stb`.
- **P4 — Missing usage requirements.** `CMAKE_CXX_STANDARD` is set globally
  rather than with `target_compile_features(oxide PUBLIC cxx_std_20)`, and the
  target is forced to static. Add the feature requirement and consider
  `BUILD_SHARED_LIBS` support with an export macro.
- **P5 — No tests or CI (High).** There are no unit tests for the nontrivial
  parts (`PipelineConfigInfo` copy/move, buffer bounds, log formatting) and no
  CI matrix for validation/logging on and off.
- **P6 — Repository hygiene.** No `LICENSE`, `.clang-format`, `.clang-tidy`, or
  sanitizer configuration.
- **P7 — Shader build.** No dependency tracking for GLSL `#include`s, no
  optimization or debug-info flags, no `spirv-val` step, and no hot reload.
- **P8 — Asset pipeline.** No OBJ or GLTF readers (already in the TODO), no
  asset cache, and no camera/scene/material layer (explicitly out of scope).

---

## API and design

- **A1 — Manual lifetime coupling.** Every resource stores `VulkanDevice&`;
  `VulkanContext` stores `Window*`. Destruction order is the caller's
  responsibility, and an exception path makes it impossible to get right (C1).
  Declaring `Window` before `GLFWContext`, or `VulkanContext` before
  `VulkanDevice`, is undefined behavior.
- **A2 — Exceptions are the only error strategy.** There is no non-throwing or
  result-based path. `OXIDE_LOG_FATAL` only logs; it does not terminate, despite
  the name.
- **A3 — Naming and namespace inconsistency.** `tools::FPSCounter` is the only
  public symbol outside `oxide`. `sample_` vs `sampler_`, and example locals
  mix `pipelineConfig` and `pipeline_Config`.
- **A4 — External linkage in translation units.** Helpers such as
  `framebufferResizeCallback`, `debugCallback`, `transitionImageLayout`, and
  `beginSingleTimeCommands` are non-static free functions in namespace `oxide`.
  They should be in an anonymous namespace to avoid symbol collisions in a
  static library.
- **A5 — `WindowConfig` is a positional aggregate.**
  `Window({ 500, 500, "Oxide", true, true, false })` is unreadable and easy to
  mis-order. Use designated initializers or a builder.
- **A6 — `Mesh` is immutable.** The buffer getters are const, so vertices
  cannot be updated after construction without rebuilding the mesh.
- **A7 — `Window::keyReleased()` is misnamed.** It returns whether the key is
  currently up, not whether it transitioned; the name implies edge detection.
  Callers end up writing `!keyReleased(...)` to test a held key.
- **A8 — `Buffer::bind()` is a vertex-buffer operation on the base class.**
  `UniformBuffer` and staging buffers inherit it, so an invalid bind compiles.
  Move it to `VertexBuffer`.
- **A9 — README file map inaccuracy.** The file map attributes `Frame` and
  `MAX_FRAMES_IN_FLIGHT` to `vulkan_context.cpp` when they are declared in the
  header.

---

## Nits

- `findQueueFamilies()` keeps the last graphics family seen instead of
  preferring a family that supports both graphics and present, which forces
  `CONCURRENT` sharing unnecessarily.
- `depthWriteEnable = VK_TRUE` while depth testing is disabled is harmless but
  misleading.
- The swapchain format fallback uses `formats.front()`, which may be UNORM and
  wash out colors. Prefer another SRGB format if one exists.
- CMake requires 3.25 (Ubuntu 22.04 ships 3.22).
- `-DGLFW_USE_WAYLAND=ON` is added to consumer compile options, where it has no
  effect; it is a GLFW build-time define.
- MSVC builds use `/W4` but not `/WX`, so warnings are not errors on Windows.
- SPIR-V is read into `std::vector<char>` and reinterpret-cast to
  `uint32_t*`; this relies on allocator alignment in practice.
- `FPSCounter` writes `\r\033[K` to stdout even when redirected, unlike the
  logger's `NO_COLOR` handling.
- `vkResultName` is missing several newer result codes and reports them as
  `UNKNOWN_VK_RESULT`.
- `VulkanDevice` stores `app_name_` but never uses it after construction;
  `vulkan_device.hpp` includes unused `<cstring>`/`<vector>`.
- Examples rely on `<cstddef>` being included transitively for `offsetof`.
- `image_texture.hpp` includes `"vulkan/vulkan_device.hpp"` while the other
  headers use `"vulkan_device.hpp"`; both work only because two include roots
  are published.
- `VulkanContext` creates its own command pool while `VulkanDevice` already owns
  one for one-time submissions.

---

## Suggested order

1. **Correctness and lifetime:** C1, C2, C3, C4, C5.
2. **Rendering foundation:** R2 (MSAA/MRT), R3 (offscreen), and C6.
3. **Memory:** M1 and M2 before the resource API is used more widely.
4. **Robustness:** C7, C8, C9, C11.
5. **Consumability:** P3, P1, P5, P6.
6. **Everything else** in the order that matches the next milestone.

## Verification notes

- The default configuration builds cleanly with GCC and
  `-Wall -Wextra -Wpedantic -Werror`.
- `-DOXIDE_ENABLE_VALIDATION=OFF -DOXIDE_ENABLE_LOGGING=OFF` also builds the
  library and all examples cleanly.
- The `triangle` example runs under the validation layer without reporting
  depth-attachment errors.
- There are no automated tests, so the above was verified by inspection and by
  building, not by runtime coverage.
