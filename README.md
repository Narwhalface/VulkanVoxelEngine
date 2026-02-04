# Vulkan Voxel Engine

## Overview
A 3D voxel rendering engine built with Vulkan, GLFW, and GLM. This project demonstrates real-time voxel terrain generation and rendering with procedural heightmap-based world generation using Perlin noise.

## Features
- **Vulkan Graphics Pipeline**: Modern low-level graphics API implementation with depth buffering and proper synchronization
- **Voxel World System**: Chunk-based world management (16×16×16 voxel chunks) with efficient storage
- **Procedural Terrain Generation**: Multi-octave Perlin noise terrain generation with configurable parameters
- **Camera System**: First-person perspective camera with full 3D movement capabilities
- **Input Controller**: Keyboard and mouse input handling for camera navigation
- **GLSL Shader Pipeline**: Vertex and fragment shaders compiled to SPIR-V at build time

## Project Structure
```
VulkanVoxelEngine/
├── src/
│   ├── main.cpp              # Entry point and window initialization
│   ├── VulkanApp.cpp/.hpp    # Core Vulkan rendering engine
│   ├── Camera.cpp/.hpp       # Camera system
│   ├── InputController.cpp/.hpp  # Input handling
│   ├── World.cpp/.hpp        # Voxel world and terrain generation
│   └── shaders/
│       ├── voxel.vert        # Vertex shader (GLSL)
│       └── voxel.frag        # Fragment shader (GLSL)
├── external/
│   ├── glfw/                 # GLFW window library
│   └── glm/                  # GLM math library
├── build/                    # Generated build files
└── CMakeLists.txt            # CMake build configuration
```

## Prerequisites
- **OS**: Windows 10/11 (64-bit)
- **Compiler**: Visual Studio 2022 or MSVC toolchain
- **Build System**: CMake 3.10 or higher
- **Graphics API**: Vulkan SDK (with `VULKAN_SDK` environment variable set)
- **Hardware**: Vulkan-capable GPU with up-to-date drivers

## Building the Project

1. **Clone the repository**:
```powershell
git clone <your-repo-url> VulkanVoxelEngine
cd VulkanVoxelEngine
```

2. **Configure with CMake**:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

3. **Build the project**:
```powershell
cmake --build build --config Debug
```

The build process will:
- Compile C++ source files
- Compile GLSL shaders to SPIR-V using `glslc` from the Vulkan SDK
- Copy shader binaries to `build/bin/Debug/shaders/`

## Running the Application

```powershell
.\build\bin\Debug\VulkanProject.exe
```

The application will open a window titled "32002614 Voxel Engine Prototype" (800×600) and render the procedurally generated voxel terrain.

## Troubleshooting

### Vulkan SDK Not Found
- **Issue**: CMake cannot locate the Vulkan SDK
- **Solution**: Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/) and ensure the `VULKAN_SDK` environment variable is set

### Shader Compilation Errors
- **Issue**: Shaders fail to compile during build
- **Solution**: Verify `glslc` is available in `%VULKAN_SDK%\Bin\` and accessible in your PATH

### GLFW Linking Errors
- **Issue**: Linker errors related to GLFW
- **Solution**: Ensure GLFW libraries in `external/glfw/lib/` match your toolchain (Visual Studio 2022, x64)

### Runtime Validation Errors
- **Issue**: Vulkan validation layer warnings/errors
- **Solution**: Enable validation layers by setting `ENABLE_VALIDATION` CMake option to `ON` for debugging:
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DENABLE_VALIDATION=ON
  ```

### Application Crashes on Startup
- **Check**: GPU driver is up to date and supports Vulkan
- **Check**: Shader files exist in `build/bin/Debug/shaders/` (voxel.vert.spv, voxel.frag.spv)

## Technical Details

### Rendering Pipeline
- **Vertex Input**: Position (vec3) and Color (vec3) attributes
- **Uniform Buffers**: Model-View-Projection matrices updated per frame
- **Depth Testing**: Enabled with 32-bit depth buffer
- **Render Pass**: Single subpass with color and depth attachments

### World Generation
- **Terrain Algorithm**: Multi-octave Perlin noise heightmap
- **Default Settings**: Base height 32, amplitude 24, 4 octaves, water level 16
- **Chunk System**: Dynamically managed chunks with hash-based lookup

### Frame Timing
- Target frame rate: 60 FPS with vsync-like behavior
- Command buffers submitted per frame with proper synchronization (semaphores and fences)

## Development Notes
- C++17 standard required
- GLM forced to use radians for angle calculations
- Framebuffer resizing callback implemented for window resize support
- Validation layers available but disabled by default for performance
