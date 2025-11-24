#pragma once
#include <glfw3.h>

class Window {
public:
    Window(uint32_t width, uint32_t height, const char* title);
    ~Window();
    GLFWwindow* get() const { return window; }
    bool shouldClose() const;
    void pollEvents() const;
private:
    GLFWwindow* window = nullptr;
};
