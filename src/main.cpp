#include <vulkan/vulkan.h>
#include <glfw3.h>
#include <cstdio>
#include <vector>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan_structs.hpp>

void InitializeVulkan() {
    
    uint32_t layerCount = 0;
    VkPhysicalDevice gpu;
    std::vector<VkPhysicalDevice> gpuList(1);
    uint32_t gpuCount = 0;

    // Enumerate instance layers
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0) {
        printf("No instance layers found or enumeration failed\n");
    } else {
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        for (uint32_t i = 0; i < layerCount; i++) {
            printf("Layer %u: %s\n", i, layers[i].layerName);
        }
    }

    // Choose validation layer (check availability first in real code)
    const char* enabledLayers[] = { "VK_LAYER_KHRONOS_validation" };
    uint32_t enabledLayerCount = 1;

    // Required extensions (query via glfwGetRequiredInstanceExtensions in real code)
    const char* enabledExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    uint32_t enabledExtensionCount = 2;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanVoxelEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Custom";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledLayerCount = enabledLayerCount;
    ci.ppEnabledLayerNames = enabledLayers;
    ci.enabledExtensionCount = enabledExtensionCount;
    ci.ppEnabledExtensionNames = enabledExtensions;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&ci, nullptr, &instance);
    if (res != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d\n", res);
        return;
    }

    // Enumerate physical devices

    vkEnumeratePhysicalDeviceGroups(instance, &gpuCount, NULL);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpuList.data());
    
    vkDestroyInstance(instance, nullptr);
}
