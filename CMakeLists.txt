cmake_minimum_required(VERSION 3.10)
# Project
project(VulkanProject)

# Link libraries
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optionally enable validation layers (off by default)
option(ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)

# Try to find Vulkan using CMake's FindVulkan or Vulkan SDK variables
find_package(Vulkan)
if(NOT Vulkan_FOUND)
    message(STATUS "Vulkan not found by find_package; trying VULKAN_SDK environment variable")
    if(DEFINED ENV{VULKAN_SDK})
        set(Vulkan_INCLUDE_DIRS "$ENV{VULKAN_SDK}/Include")
        set(Vulkan_LIBRARIES "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
        set(Vulkan_FOUND TRUE)
    endif()
endif()

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found. Install the Vulkan SDK or set VULKAN_SDK environment variable.")
endif()

# Set paths to GLFW and GLM (bundled under external/)
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/external/glfw")
set(GLM_DIR "${CMAKE_SOURCE_DIR}/external/glm")

# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/engine/Window.cpp
    src/engine/VulkanEngine.cpp
)

# Add include directory for engine headers
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_DIR}/include
    ${GLM_DIR}
)

if(WIN32)
    # Link Vulkan (prefer imported target) and the provided glfw3.lib
    if(TARGET Vulkan::Vulkan)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            Vulkan::Vulkan
            "${GLFW_DIR}/lib/glfw3.lib"
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${Vulkan_LIBRARIES}
            "${GLFW_DIR}/lib/glfw3.lib"
        )
    endif()
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} 
        Vulkan::Vulkan
        ${GLFW_DIR}/lib/libglfw3.a
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
else() # Linux
    target_link_libraries(${PROJECT_NAME} 
        Vulkan::Vulkan
        ${GLFW_DIR}/lib/libglfw3.a
        ${CMAKE_DL_LIBS}
        pthread
        X11
        Xxf86vm
        Xrandr
        Xi
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Minimal build: remove shader compilation and copy steps.
# (Shaders retained on disk but not built or required.)

# Install step (optional)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)