#include "Window.h"
#include <stdexcept>

Window::Window(uint32_t width, uint32_t height, const char* title) {
    // Initialize GLFW and create a window suitable for Vulkan.
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) throw std::runtime_error("Failed to create GLFW window");
}

Window::~Window() {
    // Destroy the window and terminate GLFW.
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window); }
void Window::pollEvents() const { glfwPollEvents(); }
