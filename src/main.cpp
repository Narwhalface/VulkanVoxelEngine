#include <vulkan/vulkan.h>
#include <glfw3.h>
#include <iostream>
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
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    //Enumerate extensions for each layer
    for (const auto& layer : layers) {
        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(layer.layerName, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        VkResult res = vkEnumerateInstanceExtensionProperties(layer.layerName, &extCount, exts.data());
        if (res != VK_SUCCESS) {
            std::cout << "Failed to enumerate extensions for layer " << layer.layerName << ": " << res << std::endl;
            continue;
        } else {
            std::cout << "Extensions for layer " << layer.layerName << " :" << std::endl;
            for (const auto& ext : exts) {
                std::cout << "\t" << ext.extensionName << std::endl;
            }
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
        std::cout << "vkCreateInstance failed: " << res << std::endl;
        return;
    } else {
        std::cout << "Vulkan instance created successfully" << std::endl;
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
        std::cout << "No graphics queue family found" << std::endl;
        return;
    } else {
        std::cout << "Graphics queue family index: " << graphicsFamily << std::endl;
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
        std::cout << "Vulkan device created successfully" << std::endl;
    } else {
        std::cout << "Failed to create Vulkan device: " << devRes << std::endl;
        return;
    }

    vkDestroyInstance(instance, nullptr);
}

int main() {
    InitializeVulkan();
    getchar();
    return 0;
}
