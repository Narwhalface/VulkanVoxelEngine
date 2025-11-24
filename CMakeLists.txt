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
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

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
