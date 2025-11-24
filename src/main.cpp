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
    const char* enabledLayers[] = { "VK_LAYER_KHRONOS_validation" };
    uint32_t enabledLayerCount = 1;
    uint32_t queueCount = 0;
    uint32_t graphicsFamily = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkPhysicalDeviceProperties gpuProps{};

    //Enumerate instance layers
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS || layerCount == 0) {
        printf("No instance layers found or enumeration failed\n");
    } else {
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        for (uint32_t i = 0; i < layerCount; i++) {
            printf("Layer %u: %s\n", i, layers[i].layerName);
        }
    }

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

    //Instance description
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
    } else {
        printf("Vulkan instance created successfully\n");
    }

    //Enumerate physical devices
    vkEnumeratePhysicalDeviceGroups(instance, &gpuCount, NULL);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpuList.data());

    //Create a device and Queue
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, NULL);
    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queueProps.data());
    for (uint32_t i = 0;  i < queueCount; ++i) {
        if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            break;
        }
    }
    if (graphicsFamily == UINT32_MAX) {
        printf("No graphics queue family found\n");
        return;
    } else {
        printf("Graphics queue family index: %u\n", graphicsFamily);
    }

    //Gets the device information
    vkGetPhysicalDeviceMemoryProperties(gpu, &memoryProperties);
    vkGetPhysicalDeviceProperties(gpu, &gpuProps);

    //Queue description
    float QueuePriorities = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &QueuePriorities;

    //Logical device description
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    
    VkDevice device = VK_NULL_HANDLE;
    VkResult devRes = vkCreateDevice(gpu, &deviceCreateInfo, nullptr, &device);
    if (devRes == VK_SUCCESS) {
        printf("Vulkan device created successfully\n");
    } else {
        printf("Failed to create Vulkan device: %d\n", devRes);
        return;
    }

    vkDestroyInstance(instance, nullptr);
}

int main() {
    InitializeVulkan();
    return 0;
}
