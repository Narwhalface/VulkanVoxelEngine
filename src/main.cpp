#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <set>
#include <limits>
#include <algorithm>
#include <string>

//Global Vulkan variables
VkInstance       g_instance        = VK_NULL_HANDLE;
VkPhysicalDevice g_physicalDevice  = VK_NULL_HANDLE;
VkDevice         g_device          = VK_NULL_HANDLE;
VkQueue          g_graphicsQueue   = VK_NULL_HANDLE;
uint32_t         g_graphicsFamily  = UINT32_MAX;
VkSurfaceKHR     g_surface         = VK_NULL_HANDLE;
VkSwapchainKHR   g_swapchain       = VK_NULL_HANDLE;

const std::vector<const char*> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> g_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct QueueFamilyIndices {
    uint32_t g_graphicsFamily = UINT32_MAX;
    uint32_t presentFamily = UINT32_MAX;
    bool isComplete() {
        return g_graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

bool checkDeviceExtensionSupport(VkPhysicalDevice device);
void createSwapchain(GLFWwindow* window);

#ifdef NDEBUG
const bool g_enableValidationLayers = false;
#else
const bool g_enableValidationLayers = true;
#endif

//Check if validation layers are available
bool checkValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    for (const char* name : g_validationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(name, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Validation layer not found: " << name << "\n";
            return false;
        }
    }
    return true;
}

//Create Vulkan instance
void createInstance(GLFWwindow* window) {
    if (g_enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VoxelEngine 32002614";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "Custom";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    VkInstanceCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    if (g_enableValidationLayers) {
        ci.enabledLayerCount   = static_cast<uint32_t>(g_validationLayers.size());
        ci.ppEnabledLayerNames = g_validationLayers.data();
    } else {
        ci.enabledLayerCount   = 0;
        ci.ppEnabledLayerNames = nullptr;
    }

    VkResult res = vkCreateInstance(&ci, nullptr, &g_instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    std::cout << "Vulkan instance created\n";
}

//Physical device selection
void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    VkResult res = vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr);
    if (res != VK_SUCCESS || deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    res = vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices.data());
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate physical devices");
    }

    for (auto dev : devices) {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueProps(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, queueProps.data());

        for (uint32_t i = 0; i < queueCount; ++i) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, g_surface, &presentSupport);

            if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                g_physicalDevice = dev;
                g_graphicsFamily = i;
                break;
            }
        }

        if (g_physicalDevice != VK_NULL_HANDLE) break;
    }

    if (g_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU with graphics queue");
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(g_physicalDevice, &props);
    std::cout << "Using GPU: " << props.deviceName << "\n";
    std::cout << "Graphics queue family index: " << g_graphicsFamily << "\n";
}

//Logical device and graphics queue creation
void createLogicalDevice() {
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.queueFamilyIndex = g_graphicsFamily;
    queueCI.queueCount       = 1;
    queueCI.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo devCI{};
    devCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devCI.queueCreateInfoCount    = 1;
    devCI.pQueueCreateInfos       = &queueCI;
    devCI.pEnabledFeatures        = &deviceFeatures;

    devCI.enabledExtensionCount   = static_cast<uint32_t>(g_deviceExtensions.size());
    devCI.ppEnabledExtensionNames = g_deviceExtensions.data();

    if (g_enableValidationLayers) {
        devCI.enabledLayerCount   = static_cast<uint32_t>(g_validationLayers.size());
        devCI.ppEnabledLayerNames = g_validationLayers.data();
    } else {
        devCI.enabledLayerCount   = 0;
    }

    VkResult res = vkCreateDevice(g_physicalDevice, &devCI, nullptr, &g_device);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(g_device, g_graphicsFamily, 0, &g_graphicsQueue);
    std::cout << "Logical device and graphics queue created\n";
}

void CreateSurface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(g_instance, window, nullptr, &g_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    } else { 
        std::cout << "Window surface created\n";
    }
}

//Window initialization and main loop
GLFWwindow* InitialiseWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return nullptr;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "32002614 Voxel Engine Prototype", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return nullptr;
    }

    return window;
}

void RenderLoop(GLFWwindow* window) {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

//Vulkan initialization
void initializeVulkan(GLFWwindow* window) {
    createInstance(window);
    CreateSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain(window);
}

void cleanupVulkan() {
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
        vkDestroyDevice(g_device, nullptr);
        g_device = VK_NULL_HANDLE;
    }

    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
        g_instance = VK_NULL_HANDLE;
    }

    if (g_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        g_surface = VK_NULL_HANDLE;
    }

    if (g_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
        g_swapchain = VK_NULL_HANDLE;
    }
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueProps.data());

    for (uint32_t i = 0; i < queueCount; ++i) {
        // Check for graphics support
        if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.g_graphicsFamily = i;
        }

        // Check for present support (only if a surface exists)
        VkBool32 presentSupport = VK_FALSE;
        if (g_surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_surface, &presentSupport);
        }
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, g_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std:: vector<VkSurfaceFormatKHR>& availableFormats) {

    for (const auto& availableFormats : availableFormats) {
        if (availableFormats.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormats.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            return availableFormats;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

bool isDeviceSuitable(VkPhysicalDevice device) {

    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapchainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapchainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(g_deviceExtensions.begin(), g_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void createSwapchain(GLFWwindow* window) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(g_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(window, swapChainSupport.capabilities);
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_surface;

    createInfo.minImageCount  = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilyIndices indices = findQueueFamilies(g_physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.g_graphicsFamily, indices.presentFamily};

    if (indices.g_graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(g_device, &createInfo, nullptr, &g_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    std::cout << "Swapchain created\n";
}

int main() {
    GLFWwindow* window = InitialiseWindow();
    if (!window) return EXIT_FAILURE;

    try {
        initializeVulkan(window);
        RenderLoop(window);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        cleanupVulkan();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    cleanupVulkan();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
