#include "VulkanApp.hpp"
#include "RenderLoop.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Forward declaration for setting executable directory
/**
 * Stores executable directory used for runtime asset lookup.
 * @param path Absolute or relative executable directory path.
 * @return No return value.
 */
void setExecutableDirectory(const std::filesystem::path& path);

/**
 * Initializes GLFW and creates the Vulkan-compatible window.
 * @return Created GLFW window handle, or nullptr on failure.
 */
GLFWwindow* InitialiseWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return nullptr;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(800, 600, "32002614 Voxel Engine Prototype", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return nullptr;
    }

    glfwSetFramebufferSizeCallback(window, VulkanApp::framebufferResizeCallback);

    return window;
}

/**
 * Application entry point that initializes, runs, and shuts down the engine.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Process exit code (`EXIT_SUCCESS` or `EXIT_FAILURE`).
 */
int main(int argc, char** argv) {
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        try {
            auto executableDir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
            setExecutableDirectory(executableDir);
        } catch (const std::exception&) {
            // Failed to set executable directory - continue anyway
        }
    }

    GLFWwindow* window = InitialiseWindow();
    if (!window) {
        return EXIT_FAILURE;
    }

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = false;  // Disable verbose Vulkan spec warnings
#endif

    VulkanApp app(window, enableValidationLayers);
    glfwSetWindowUserPointer(window, &app);

    try {
        app.initialize();
        RenderLoop(app, window);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        app.cleanup();
        return EXIT_FAILURE;
    }

    app.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
