#include "engine/Window.h"
#include "engine/VulkanEngine.h"
#include <iostream>

// Program entry: create window, init Vulkan, run event loop.
int main() {
    try {
        Window window(800, 600, "Minimal Vulkan");
        std::cout << "Window created" << std::endl;
        VulkanEngine engine(window.get());
        std::cout << "Initializing Vulkan..." << std::endl;
        engine.init();
        // Main loop: handle window events. No rendering in this minimal demo.
        while (!window.shouldClose()) {
            window.pollEvents();
        }
        // Wait for GPU before shutting down.
        engine.waitIdle();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}