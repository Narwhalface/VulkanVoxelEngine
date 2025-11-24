# Beginner Vulkan Guide

Short version for undergrads: how to get a minimal Vulkan app working in this repo.

Files to look at:
- `src/main.cpp` — program entry and main loop.
- `src/engine/VulkanEngine.cpp` — Vulkan setup (annotated).
- `docs/3d_cube_pseudocode.txt` — step-by-step plan to add a cube.

What this guide covers:
- Basic Vulkan init steps and where they are in this project.
- How to build on Windows (PowerShell).
- Short checklist for making a triangle or cube.

---

Prerequisites
- A GPU with Vulkan and current drivers.
- Visual Studio 2022 or another C++ toolchain.
- The Vulkan SDK and `VULKAN_SDK` set.

Build (PowerShell)
Run these from the repo root (`c:\Dev\VulkanVoxelEngine`):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

If configure fails, check the CMake output for missing SDKs or wrong generator.

---

Where the key init steps are
- Instance: `VulkanEngine::createInstance()` — starts Vulkan and optionally enables debug messages.
- Surface: `VulkanEngine::createSurface()` — makes a surface that links Vulkan to the window.
- Pick GPU: `VulkanEngine::pickPhysicalDevice()` — pick a GPU that supports graphics + present.
- Logical device + queues: `VulkanEngine::createLogicalDevice()` — create a device and get queues.

The engine stops here. To draw you still need: swapchain, image views, render pass, pipeline, buffers, and command buffers. See `docs/3d_cube_pseudocode.txt`.

---

Quick walkthrough of the annotations
- Top of `VulkanEngine.cpp`: short summary of each init function.
- `createInstance()`: why the VkInstance matters and why we enable validation layers during development.
- `createSurface()`: what the surface does.
- `pickPhysicalDevice()`: why we check queue families.
- `createLogicalDevice()`: how we request queues and why we get VkQueue handles.

---

Minimal extra pieces to render a triangle/cube
- Swapchain + image views.
- Render pass with attachments.
- Graphics pipeline (vertex + fragment shaders).
- Vertex/index buffers and a UBO for the MVP matrix.
- Command buffers that bind the pipeline and draw.

See `docs/3d_cube_pseudocode.txt` for a short plan that lists these steps.

---

Example shaders (GLSL)
Vertex (`shaders/triangle.vert`):

```glsl
#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 fragColor;
layout(set = 0, binding = 0) uniform UBO { mat4 mvp; } ubo;
void main() {
    fragColor = inColor;
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
```

Fragment (`shaders/triangle.frag`):

```glsl
#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(fragColor, 1.0);
}
```

Compile with `glslangValidator` (part of the Vulkan SDK):

```powershell
glslangValidator -V shaders/triangle.vert -o build/shaders/triangle.vert.spv
glslangValidator -V shaders/triangle.frag -o build/shaders/triangle.frag.spv
```

---

Common problems (short)
- Black window: check shaders, vertex formats, and enable validation layers.
- Faces draw wrong: add depth testing and a depth attachment.
- Crashes: wait for device idle before destroying resources.
- Resize: recreate swapchain and related resources.

Validation layers
- Turn them on during development. They print useful errors to stderr.

---

Next steps you can do now
1. Build the project and run the binary.
2. Compile shaders and put them in `shaders/`.
3. Follow `docs/3d_cube_pseudocode.txt` to add swapchain, pipeline, and draw calls.

---

What I changed in the repo
- Short comments added to `src/engine/VulkanEngine.cpp`.
- This guide: `docs/Beginner_Vulkan_Guide.md` (simplified language).

---

Want me to do more?
- I can simplify comments in `src/main.cpp` and `src/engine/Window.cpp` the same way.
- I can implement swapchain + render pass + pipeline and draw a triangle/cube.
- I can run CMake configure to verify the project configures on your machine.

Which next step do you want?

---

Vulkan Syntax Quick Reference (short)

- Struct init pattern:

```cpp
VkInstanceCreateInfo ci{};               // zero-init the struct
ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // every Vulkan struct has sType
ci.pApplicationInfo = &appInfo;         // set fields as needed
// then call the create function:
if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance");
}
```

- Key points:
    - `sType` identifies the struct type and must be set.
    - `pNext` is used for extension structs (usually `nullptr` when unused).
    - Create functions follow `vkCreateXxx(&createInfo, allocator, &outHandle)`.

- Enumerate / query pattern:

```cpp
uint32_t count = 0;
vkEnumeratePhysicalDevices(instance, &count, nullptr); // get count
std::vector<VkPhysicalDevice> devices(count);
vkEnumeratePhysicalDevices(instance, &count, devices.data()); // fill array
```

- Get queue pattern:

```cpp
// get a handle to the queue from a created device
vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
```

- Getting extension functions:

```cpp
auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
if (func) func(instance, &dbg, nullptr, &debugMessenger);
```

These small patterns appear throughout the code. The comments in `src/engine/VulkanEngine.cpp` point at real examples.

---

Key types & functions (what they do)

- `QueueFamilyIndices` / `findQueueFamilies`:
    - Purpose: find which queue families on the GPU support graphics work
        and which can present images to the window surface.
    - Why it matters: you need a graphics queue to record draw commands and
        a present-capable queue to show the rendered image on screen.

- `vkGetPhysicalDeviceQueueFamilyProperties`:
    - Queries the GPU for the properties of each queue family (what bits
        they support: graphics, compute, transfer, etc.). Used inside
        `findQueueFamilies`.

- `vkGetPhysicalDeviceSurfaceSupportKHR`:
    - For a given queue family index, returns whether that family can present
        to a particular `VkSurfaceKHR` (window). This is how we test present
        support.

- `VkInstanceCreateInfo` / `vkCreateInstance`:
    - `VkInstanceCreateInfo` configures how to create a `VkInstance` (enabled
        extensions, enabled layers, pointers to other optional structs).
    - `vkCreateInstance` creates the `VkInstance` — the root object used to
        talk to Vulkan.

- `vkGetInstanceProcAddr`:
    - Loads function pointers for extension functions (like debug utils).
    - Used when calling functions not directly exported by the loader.

- `VkDeviceQueueCreateInfo` / `vkCreateDevice`:
    - `VkDeviceQueueCreateInfo` asks the driver to create one or more queues
        from a queue family. `pQueuePriorities` provides priorities for those
        queues.
    - `vkCreateDevice` creates a `VkDevice` (logical device) from a physical
        device and enables requested device extensions (like `VK_KHR_swapchain`).

- `vkGetDeviceQueue`:
    - After `vkCreateDevice`, retrieve the `VkQueue` handle with
        `vkGetDeviceQueue(device, familyIndex, queueIndex, &queue)`.
    - Use the returned `VkQueue` to submit command buffers and present.

- `glfwGetRequiredInstanceExtensions`:
    - Returns the instance extensions GLFW needs so Vulkan can present to the
        window on the current platform (Windows/Mac/Linux). Pass these to the
        `VkInstanceCreateInfo`.

These explanations are short by design — they show the exact role each
type/function plays in the initialization flow. If you'd like, I can add a
small annotated code snippet per item showing the real call site.
