README

Short description
- This repo contains a small Vulkan demo that opens a GLFW window and renders a triangle.

Quick steps
1. Create a tiny Vulkan app using GLFW.
2. Add two GLSL shaders (vertex + fragment).
3. Use CMake to find Vulkan, include GLFW/GLM, and compile GLSL to SPIR-V.
4. Implement init code in `src/main.cpp` (instance, device, swapchain, pipeline, command buffers, present loop).

Prerequisites
- Windows 10/11 with Vulkan-capable GPU and drivers.
- Visual Studio 2022 or MSVC toolchain.
- CMake >= 3.10.
- Vulkan SDK installed and `VULKAN_SDK` set.

Reproduce (commands)

Clone the repo and build (Visual Studio generator):

```powershell
git clone <your-repo-url> VulkanVoxelEngine
cd VulkanVoxelEngine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Notes
- The CMakeLists adds a `Shaders` step that runs `glslangValidator` to produce SPIR-V.
- Built shaders are copied into `build/bin/<Config>/shaders` so the exe can load them.

Run

```powershell
.\build\bin\Debug\VulkanProject.exe
```

If it fails to run: check the Vulkan SDK, drivers, and that `glslangValidator` is available.

Troubleshooting (short)
- "Vulkan SDK not found": install the LunarG Vulkan SDK and set `VULKAN_SDK`.
- Shader compile errors: check that `glslangValidator` is present.
- GLFW link errors: use a GLFW lib built with the same toolchain and arch.

Want automation?
- I can add a script to download/build externals and produce an artifacts zip. Say "automate deps" to proceed.
