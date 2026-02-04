cmake_minimum_required(VERSION 3.10)
project(VulkanProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)

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

set(GLFW_DIR "${CMAKE_SOURCE_DIR}/external/glfw")
set(GLM_DIR "${CMAKE_SOURCE_DIR}/external/glm")

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/VulkanApp.cpp
    src/VulkanApp.hpp
    src/Camera.cpp
    src/Camera.hpp
    src/InputController.cpp
    src/InputController.hpp
    src/World.cpp
    src/World.hpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
)

# Locate glslc for shader compilation
set(GLSLC_HINT_DIRS)
if(DEFINED ENV{VULKAN_SDK})
    if(WIN32)
        list(APPEND GLSLC_HINT_DIRS "$ENV{VULKAN_SDK}/Bin")
    else()
        list(APPEND GLSLC_HINT_DIRS "$ENV{VULKAN_SDK}/bin")
    endif()
endif()

find_program(GLSLC_EXECUTABLE glslc HINTS ${GLSLC_HINT_DIRS} REQUIRED)

set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/shaders")

set(SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/voxel.vert"
    "${SHADER_SOURCE_DIR}/voxel.frag"
)

set(SPIRV_OUTPUTS)
foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_FILE} -o ${SPIRV_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
        VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS ${SPIRV_FILE})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(${PROJECT_NAME} Shaders)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_DIR}/include
    ${GLM_DIR}
)

if(WIN32)
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
else()
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

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
