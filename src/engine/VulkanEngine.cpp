#include "VulkanEngine.h"
#include <stdexcept>
#include <iostream>
#include <set>
#include <optional>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Debug messenger destroy helper
static void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    if (!messenger) return;
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, messenger, nullptr);
}

// ---------------------------------------------------------------------------
// Simple overview (for undergrads):
// - createInstance(): start Vulkan and (optionally) enable debug messages.
// - createSurface(): make a surface so Vulkan can show images in the window.
// - pickPhysicalDevice(): pick a GPU that supports graphics and presenting.
// - createLogicalDevice(): make a device to talk to the GPU and get queues.
// The rest of the engine builds on these objects (swapchain, pipelines, etc.).
// ---------------------------------------------------------------------------

VulkanEngine::VulkanEngine(GLFWwindow* w) : window(w) {}

VulkanEngine::~VulkanEngine() {
    if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    DestroyDebugUtilsMessenger(instance, debugMessenger);
    if (instance) vkDestroyInstance(instance, nullptr);
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if (presentSupport) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

// What is `QueueFamilyIndices` and `findQueueFamilies`?
// - GPUs expose one or more "queue families". A queue family is a group of
//   queues that support a certain set of operations (graphics, compute,
//   transfer, etc.). Before creating a logical device we must find which
//   queue family indices support the features we need.
// - `QueueFamilyIndices` stores the index (u32) of the family that supports
//   graphics work and the family that supports presenting images to the
//   surface. They can be the same or different.
// - `findQueueFamilies` queries the physical device for its queue families
//   (via `vkGetPhysicalDeviceQueueFamilyProperties`) and checks for two
//   things:
//   * `VK_QUEUE_GRAPHICS_BIT` in `queueFlags` — indicates the family can do
//     drawing/graphics commands.
//   * `vkGetPhysicalDeviceSurfaceSupportKHR(...)` — returns whether that
//     family can present to our window surface.
// - The function returns indices we later use when creating the logical
//   device and retrieving VkQueue handles.

void VulkanEngine::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Minimal";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    // What is `VkInstanceCreateInfo` and `vkCreateInstance`?
    // - `VkInstanceCreateInfo` is a struct that tells Vulkan how to create a
    //   `VkInstance`. It includes pointers to optional structures, enabled
    //   extensions, and enabled validation layers.
    // - `vkCreateInstance(&ci, allocator, &instance)` creates the instance.
    // - The `VkInstance` is the connection between your program and the
    //   Vulkan driver. You must create it before using any other Vulkan
    //   functions that depend on an instance.
    // Syntax note: common Vulkan pattern:
    // 1) zero-init a `Vk*CreateInfo` struct with `{}`
    // 2) set `sType` to the matching `VK_STRUCTURE_TYPE_*` value
    // 3) fill fields and call `vkCreate*(&ci, allocator, &outHandle)`

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    // Ask GLFW which Vulkan extensions are needed so we can present to the window.
    if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    ci.enabledExtensionCount = (uint32_t)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        ci.enabledLayerCount = (uint32_t)validationLayers.size();
        ci.ppEnabledLayerNames = validationLayers.data();
    } else {
        ci.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance");

    // We now have a VkInstance — the main Vulkan object. Other Vulkan objects
    // are created from this instance.

    if (enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT dbg{};
        dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT,
                                 VkDebugUtilsMessageTypeFlagsEXT,
                                 const VkDebugUtilsMessengerCallbackDataEXT* data,
                                 void*) -> VkBool32 {
            std::cerr << "VULKAN: " << data->pMessage << std::endl;
            return VK_FALSE;
        };
        // Create the debug messenger (if available). It prints validation
        // warnings/errors to stderr which helps while developing.
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        // `vkGetInstanceProcAddr` is how you load functions exposed by
        // extensions (they are not always linked into the loader). Here we
        // load `vkCreateDebugUtilsMessengerEXT` and call it to create the
        // debug messenger if available.
        if (func) func(instance, &dbg, nullptr, &debugMessenger);
    }
}

void VulkanEngine::createSurface() {
    // Create a surface (platform-specific) using GLFW. The surface is where
    // Vulkan will present rendered images to the window.
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface");
}

void VulkanEngine::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    // Check how many Vulkan-capable GPUs are available on the system.
    if (deviceCount == 0) throw std::runtime_error("no GPUs with Vulkan support");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (auto d : devices) {
        // Pick the first GPU that supports both graphics and presenting.
        // (A real app would do more checks here.)
        if (findQueueFamilies(d, surface).isComplete()) { physicalDevice = d; break; }
    }
    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("no suitable GPU found");
}

void VulkanEngine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    std::set<uint32_t> uniqueFamilies = { *indices.graphicsFamily, *indices.presentFamily };
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    // Request one queue from each required queue family (graphics, present).
    for (uint32_t f : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = f;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        qInfos.push_back(qi);
    }
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)qInfos.size();
    ci.pQueueCreateInfos = qInfos.data();
    ci.pEnabledFeatures = &features;
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    ci.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    ci.ppEnabledExtensionNames = deviceExtensions.data();
    // Syntax note: device creation follows the same pattern as instance
    // creation. Fill `VkDeviceCreateInfo` then call `vkCreateDevice(..., &device)`.
    if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device");
    // Get the queues we requested. Queues are used to submit GPU work and to
    // present rendered images to the window.
    vkGetDeviceQueue(device, *indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, *indices.presentFamily, 0, &presentQueue);
    // After this you have `VkQueue` handles (graphicsQueue, presentQueue).
    // Use `vkQueueSubmit(graphicsQueue, ...)` to submit command buffers for
    // execution and `vkQueuePresentKHR(presentQueue, ...)` to present images.
}

void VulkanEngine::init() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    std::cout << "Vulkan initialized (minimal)" << std::endl;
}

// Note:
// This setup stops after creating the logical device. To draw anything you
// still need swapchain, render pass, pipeline, buffers, command buffers, etc.
// See `docs/3d_cube_pseudocode.txt` for the next steps.

void VulkanEngine::waitIdle() {
    if (device) vkDeviceWaitIdle(device);
}
