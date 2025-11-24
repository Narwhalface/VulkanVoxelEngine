#pragma once
// Small Vulkan setup helper (instance, surface, device).

#include <vulkan/vulkan.h>
#include <glfw3.h>
#include <vector>

class VulkanEngine {
public:
    explicit VulkanEngine(GLFWwindow* window);
    ~VulkanEngine();

    // Initialize Vulkan (instance, surface, physical & logical device).
    void init();
    // Wait for device idle before shutdown.
    void waitIdle();

private:
    // Enable validation layers during development.
    bool enableValidationLayers = true;
    std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    GLFWwindow* window = nullptr;
};
