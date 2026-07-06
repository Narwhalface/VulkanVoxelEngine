# Vulkan Voxel Engine

## Overview
This project is a Windows Vulkan voxel terrain prototype built with GLFW and GLM. The current application opens an 800x600 Vulkan window, streams voxel terrain around the camera, shades it with a directional light, and renders a shadow map for terrain self-shadowing.

The world is generated procedurally at runtime and can be tuned from [scripts/terrain.lua](scripts/terrain.lua).

## Implemented Features
- Stable Vulkan rendering pipeline with swapchain management, depth buffering, and per-frame synchronization
- Procedural voxel terrain generation, including solid terrain volumes, grass surface layers, and water fill regions
- Asynchronous chunk generation, meshing, GPU upload, and camera-centered streaming
- First-person navigation with mouse-look, vertical traversal, and sprint movement
- Frustum culling for visible chunk mesh selection
- Directional lighting with a 4096x4096 shadow map for terrain self-shadowing
- Automated GLSL-to-SPIR-V shader compilation as part of the CMake build process
- Lua-configurable terrain startup parameters on Windows via a dynamically loaded Lua runtime DLL

## Controls
- `W`, `A`, `S`, `D`: move
- Mouse: look around
- `Space`: move up
- `Left Ctrl` or `C`: move down
- `Left Shift` or `Right Shift`: sprint
- `Esc`: close the application

The cursor is captured when the window is active.

## Terrain Script
On startup the app reads [scripts/terrain.lua](scripts/terrain.lua) if it can find it relative to the current working directory or the executable directory.

Supported globals:
- `Render_distance`: chunk draw distance, clamped to `2..32`
- `Noise_intensity`: terrain elevation amplitude, clamped to `1.0..128.0`
- `Terrain_seed`: fixed world seed
- `Randomize_seed`: generate a new random seed on each launch

If no supported Lua runtime DLL is present next to the executable (`lua54.dll`, `lua53.dll`, or `lua52.dll`), the app logs a warning and continues with built-in defaults.

## Project Layout
```text
VulkanVoxelEngine/
├── src/
│   ├── main.cpp
│   ├── VulkanApp.cpp/.hpp
│   ├── RenderLoop.cpp/.hpp
│   ├── Camera.cpp/.hpp
│   ├── InputController.cpp/.hpp
│   ├── World.cpp/.hpp
│   ├── LuaTerrainScriptBridge.hpp
│   └── shaders/
│       ├── voxel.vert
│       ├── voxel.frag
│       └── shadow.vert
├── scripts/
│   └── terrain.lua
├── shaders/
│   └── *.spv                # Generated at build time
├── external/
│   ├── GLFW/
│   └── glm/
├── CMakeLists.txt
└── CMakePresets.json
```

## Prerequisites
- Windows 10 or 11
- Visual Studio 2022 with MSVC
- CMake 3.10+
- Vulkan SDK with `glslc` available
- Vulkan-capable GPU and current drivers

## Build
This repository already includes CMake presets for Visual Studio 2022.

Release build:

```powershell
cmake --preset vs2022-release
cmake --build --preset build-release
```

Debug build:

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

Build output:
- Executable: `build/bin/<Config>/VulkanProject.exe`
- Generated shaders: `shaders/*.spv`

## Run
From the repository root:

```powershell
.\build\bin\Release\VulkanProject.exe
```

You can also run the Debug build if you built that configuration instead.

At startup the app:
- loads terrain settings from `scripts/terrain.lua` when available
- creates worker threads for chunk generation and meshing
- positions the camera above generated terrain once nearby chunks are ready

## Rendering Notes
- Terrain is built from 16x16x16 voxel chunks
- The default runtime render distance is 16 chunks unless overridden by Lua
- The terrain generator uses layered value noise, not a full Perlin-noise implementation
- Water is generated up to a configurable water level
- Shadowing is sampled in the voxel fragment shader from a depth texture produced in a separate shadow pass

## Troubleshooting
### Vulkan SDK not found
Install the LunarG Vulkan SDK and make sure CMake can find Vulkan headers, libraries, and `glslc`.

### Shaders fail to load at runtime
Build the project first so `shaders/voxel.vert.spv`, `shaders/voxel.frag.spv`, and `shaders/shadow.vert.spv` exist.

### Lua settings are ignored
Place `lua54.dll`, `lua53.dll`, or `lua52.dll` next to the executable. Without a Lua DLL the app falls back to defaults and prints a warning.

### Application starts but terrain seems missing
The app streams terrain asynchronously. Initial chunk generation and upload can take a moment, after which the camera is placed above the loaded terrain.

## Current Scope
This README describes the functionality currently implemented in the codebase. It does not assume editor tooling, in-game UI, terrain editing, save/load, or gameplay systems beyond free-fly terrain exploration.
