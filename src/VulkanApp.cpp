#include "VulkanApp.hpp"
#include "LuaTerrainScriptBridge.hpp"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <fstream>
#include <chrono>
#include <mutex>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace {
std::filesystem::path gExecutableDir;
constexpr int kChunkSize = Chunk::SIZE;

int floorDiv(int value, int divisor) noexcept {
    // Divides value by divisor with floor semantics for negatives; returns the floored quotient.
    // Performs mathematical floor division so negative coordinates map to the expected chunk.
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

struct FaceDefinition {
    glm::ivec3 normal;
    std::array<glm::vec3, 4> corners;
};

const std::array<FaceDefinition, 6> gFaceDefinitions = {{
    {{0, 0, -1}, {glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{1.0f, 1.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}}},
    {{0, 0, 1}, {glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 1.0f, 1.0f}, glm::vec3{0.0f, 1.0f, 1.0f}}},
    {{-1, 0, 0}, {glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 1.0f, 1.0f}, glm::vec3{0.0f, 1.0f, 0.0f}}},
    {{1, 0, 0}, {glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 1.0f, 0.0f}, glm::vec3{1.0f, 1.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 1.0f}}},
    {{0, -1, 0}, {glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, 1.0f}}},
    {{0, 1, 0}, {glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 1.0f}, glm::vec3{1.0f, 1.0f, 1.0f}, glm::vec3{1.0f, 1.0f, 0.0f}}}
}};

glm::vec3 voxelBaseColor(uint8_t type, const glm::ivec3& normal) {
    // Maps voxel type and face normal to an RGB color; returns the selected base color.
    // Chooses a base albedo by voxel type, with directional tinting for grass faces.
    switch (type) {
        case 2:
            if (normal.y > 0) {
                return {0.20f, 0.65f, 0.23f};
            }
            if (normal.y < 0) {
                return {0.35f, 0.22f, 0.12f};
            }
            return {0.25f, 0.50f, 0.20f};
        case 3:
            return {0.05f, 0.32f, 0.75f};
        case 1:
        default:
            return {0.55f, 0.55f, 0.55f};
    }
}

float shadeForNormal(const glm::ivec3& normal) {
    // Computes directional shading from a face normal; returns a brightness multiplier.
    // Returns a simple face-lighting factor based on axis orientation.
    if (normal.y > 0) {
        return 1.0f;
    }
    if (normal.y < 0) {
        return 0.55f;
    }
    if (normal.x != 0) {
        return 0.75f;
    }
    return 0.85f;
}
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    // Builds the vertex binding layout for Vertex data; returns Vulkan binding metadata.
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
    // Builds position/color vertex attributes; returns two Vulkan attribute descriptions.
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = offsetof(Vertex, pos);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset   = offsetof(Vertex, color);

    return attributeDescriptions;
}

static std::filesystem::path resolveAssetPath(const std::filesystem::path& relative) {
    // Resolves a relative asset path against known roots; returns the first existing path or input.
    // Searches upward from current and executable directories to find an existing asset path.
    std::vector<std::filesystem::path> roots;
    auto registerRoots = [&roots](const std::filesystem::path& source) {
        if (source.empty()) {
            return;
        }

        std::filesystem::path current = source;
        while (!current.empty()) {
            const auto normalised = current.lexically_normal();
            if (std::find(roots.begin(), roots.end(), normalised) == roots.end()) {
                roots.push_back(normalised);
            }

            const auto parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = parent;
        }
    };

    registerRoots(std::filesystem::current_path());
    registerRoots(gExecutableDir);

    for (const auto& base : roots) {
        const auto candidate = (base / relative).lexically_normal();
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return relative;
}

static std::vector<char> readfile(const std::string& filename) {
    // Reads a binary file into memory using an asset-relative filename; returns raw file bytes.
    // Loads a binary file into memory (used for SPIR-V shader blobs).
    const auto assetPath = resolveAssetPath(filename);

    std::ifstream file(assetPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + assetPath.string());
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void setExecutableDirectory(const std::filesystem::path& path) {
    // Sets executable directory used by asset lookup; takes a filesystem path and returns nothing.
    // Stores the executable directory so runtime asset lookup can resolve relative paths.
    gExecutableDir = path;
}

VulkanApp::VulkanApp(GLFWwindow* window, bool enableValidation)
// Initializes app state from a GLFW window handle and validation flag; constructs a VulkanApp instance.
    : windowRef(window),
      enableValidationLayers(enableValidation),
      validationLayers({"VK_LAYER_KHRONOS_validation"}),
      deviceExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME}) {}

void VulkanApp::initialize() {
    // Initializes Vulkan, terrain, threading, and camera systems; takes no args and returns nothing.
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    terrainSettings.baseHeight = 24.0f;
    terrainSettings.elevationAmplitude = 12.0f;
    terrainSettings.horizontalScale = 0.05f;
    terrainSettings.octaves = 4;
    terrainSettings.persistence = 0.55f;
    terrainSettings.lacunarity = 2.0f;
    terrainSettings.waterLevel = 10;

    const auto scriptValues = LuaTerrainScriptBridge::loadScript(resolveAssetPath("scripts/terrain.lua"));
    if (!scriptValues.errorMessage.empty()) {
        std::cout << "Lua terrain script warning: " << scriptValues.errorMessage << "\n";
    } else {
        std::cout << "Lua terrain script loaded successfully.\n";
    }

    if (scriptValues.noiseIntensity.has_value()) {
        terrainSettings.elevationAmplitude = std::clamp(*scriptValues.noiseIntensity, 1.0f, 128.0f);
    }

    if (scriptValues.renderDistanceChunks.has_value()) {
        const int clampedDistance = (std::max)(2, *scriptValues.renderDistanceChunks);
        renderDistanceChunks.store(clampedDistance, std::memory_order_relaxed);
    }

    uint32_t terrainSeed = 1337u;
    if (scriptValues.randomizeSeed.value_or(false)) {
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_int_distribution<uint32_t> distribution;
        terrainSeed = distribution(generator);
    }
    if (scriptValues.terrainSeed.has_value()) {
        terrainSeed = *scriptValues.terrainSeed;
    }

    std::cout
        << "Lua terrain values: render_distance="
        << renderDistanceChunks.load(std::memory_order_relaxed)
        << ", noise_intensity="
        << terrainSettings.elevationAmplitude
        << ", terrain_seed="
        << terrainSeed
        << "\n";
    std::cout << "Current terrain seed: " << terrainSeed << "\n";

    world.setTerrainGenerator(terrainSeed, terrainSettings);

    const float initialHeight = terrainSettings.baseHeight + terrainSettings.elevationAmplitude + 8.0f;
    camera.setPosition(glm::vec3(0.5f, initialHeight, 0.5f));
    camera.lookAt(glm::vec3(8.0f, initialHeight, 8.0f));

    requestedTerrainCenterChunk = chunkForPosition(camera.position());
    loadedTerrainCenterChunk = requestedTerrainCenterChunk;
    pendingTerrainCenterChunk = requestedTerrainCenterChunk;

    chunkWorkerRunning.store(true, std::memory_order_relaxed);
    const uint32_t hardwareThreads = (std::max)(1u, std::thread::hardware_concurrency());
    const uint32_t generationWorkerCount = (std::clamp)(hardwareThreads / 2, 2u, 8u);
    const uint32_t meshWorkerCount = (std::clamp)(hardwareThreads - generationWorkerCount, 2u, 8u);
    generationWorkerThreads.reserve(generationWorkerCount);
    meshWorkerThreads.reserve(meshWorkerCount);
    for (uint32_t workerIndex = 0; workerIndex < generationWorkerCount; ++workerIndex) {
        generationWorkerThreads.emplace_back([this]() {
            this->runChunkGenerationWorker();
        });
    }
    for (uint32_t workerIndex = 0; workerIndex < meshWorkerCount; ++workerIndex) {
        meshWorkerThreads.emplace_back([this]() {
            this->runChunkMeshWorker();
        });
    }
    requestTerrainWindow(requestedTerrainCenterChunk);
    
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();

    const float aspect = swapchainExtent.height == 0
                             ? 1.0f
                             : swapchainExtent.width / static_cast<float>(swapchainExtent.height);
    camera.setPerspective(45.0f, aspect, 0.1f, 500.0f);

    inputController.attach(windowRef, &camera);
    inputController.syncOrientationFromCamera();
    lastFrameTimeSeconds = glfwGetTime();
}

void VulkanApp::cleanup() {
    // Stops workers and releases Vulkan resources in shutdown order; takes no args and returns nothing.
    chunkWorkerRunning.store(false, std::memory_order_relaxed);
    generationQueueCv.notify_all();
    meshQueueCv.notify_all();
    completedChunkSpaceCv.notify_all();

    for (auto& worker : generationWorkerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    generationWorkerThreads.clear();

    for (auto& worker : meshWorkerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    meshWorkerThreads.clear();
    
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        cleanupSwapChain();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        clearAllChunkMeshes();

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device,indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }

        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vertexBuffer = VK_NULL_HANDLE;
        }

        if (vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            vertexBufferMemory = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

}

void VulkanApp::createInstance() {
    // Creates the Vulkan instance with required extensions/layers; takes no args and returns nothing.
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VoxelEngine 32002614";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "Custom";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount   = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    std::cout << "Vulkan instance created\n";
}

bool VulkanApp::checkValidationLayerSupport() const {
    // Checks whether requested validation layers are present; returns true when all are available.
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const auto& props : availableLayers) {
            if (std::strcmp(layerName, props.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Validation layer not found: " << layerName << "\n";
            return false;
        }
    }

    return true;
}

void VulkanApp::createSurface() {
    // Creates a Vulkan surface from the GLFW window; takes no args and returns nothing.
    if (glfwCreateWindowSurface(instance, windowRef, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    std::cout << "Window surface created\n";
}

void VulkanApp::pickPhysicalDevice() {
    // Selects the first suitable physical device and queue families; takes no args and returns nothing.
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate physical devices");
    }

    for (VkPhysicalDevice candidate : devices) {
        if (isDeviceSuitable(candidate)) {
            physicalDevice    = candidate;
            selectedQueues    = findQueueFamilies(candidate);
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "Using GPU: " << props.deviceName << "\n";
    std::cout << "Graphics queue family index: " << selectedQueues.graphicsFamily << "\n";
}

bool VulkanApp::isDeviceSuitable(VkPhysicalDevice candidate) const {
    // Evaluates a candidate GPU for required queues/extensions/swapchain; returns suitability.
    QueueFamilyIndices indices = findQueueFamilies(candidate);
    bool extensionsSupported   = checkDeviceExtensionSupport(candidate);

    bool swapchainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails support = querySwapChainSupport(candidate);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

VulkanApp::QueueFamilyIndices VulkanApp::findQueueFamilies(VkPhysicalDevice candidate) const {
    // Finds graphics and present queue families on a device; returns discovered family indices.
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool VulkanApp::checkDeviceExtensionSupport(VkPhysicalDevice candidate) const {
    // Verifies required device extensions for a GPU; returns true when all are supported.
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        required.erase(extension.extensionName);
    }

    return required.empty();
}

void VulkanApp::createLogicalDevice() {
    // Creates logical device and retrieves graphics/present queues; takes no args and returns nothing.
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueFamilies = {
        selectedQueues.graphicsFamily,
        selectedQueues.presentFamily
    };

    float queuePriority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount   = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, selectedQueues.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, selectedQueues.presentFamily, 0, &presentQueue);

    std::cout << "Logical device and graphics queue created\n";
}

VulkanApp::SwapChainSupportDetails VulkanApp::querySwapChainSupport(VkPhysicalDevice candidate) const {
    // Queries swapchain capabilities, formats, and present modes for a device; returns support details.
    SwapChainSupportDetails details{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    // Chooses a preferred surface format from available formats; returns selected format.
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
}

VkPresentModeKHR VulkanApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const {
    // Chooses a presentation mode from available modes; returns selected present mode.
    for (VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }

    return presentModes.empty() ? VK_PRESENT_MODE_FIFO_KHR : presentModes.front();
}

VkExtent2D VulkanApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    // Chooses swap extent using surface capabilities and framebuffer size; returns chosen extent.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(windowRef, &width, &height);

    VkExtent2D actualExtent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

void VulkanApp::createSwapchain() {
    // Creates swapchain and stores images/format/extent; takes no args and returns nothing.
    SwapChainSupportDetails support = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR  presentMode    = chooseSwapPresentMode(support.presentModes);
    VkExtent2D        extent         = chooseSwapExtent(support.capabilities);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueIndices[] = {
        selectedQueues.graphicsFamily,
        selectedQueues.presentFamily
    };

    if (selectedQueues.graphicsFamily != selectedQueues.presentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain");
    }

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent      = extent;

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    std::cout << "Swapchain created\n";
}

void VulkanApp::createImageViews() {
    // Creates image views for each swapchain image; takes no args and returns nothing.
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        swapchainImageViews[i] = createImageView(swapchainImages[i], swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

VkShaderModule VulkanApp::createShaderModule(VkDevice device, const std::vector<char>& code) {
    // Creates a shader module from SPIR-V bytes for a device; returns shader module handle.
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;

    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

void VulkanApp::createDescriptorSetLayout() {
    // Creates descriptor set layout for uniform bindings; takes no args and returns nothing.
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void VulkanApp::createDescriptorPool() {
    // Creates descriptor pool sized for frames in flight; takes no args and returns nothing.
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void VulkanApp::createDescriptorSets() {
    // Allocates and writes descriptor sets for uniform buffers; takes no args and returns nothing.
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = descriptorSets[i];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void VulkanApp::createGraphicsPipeline() {
    // Creates shader stages, pipeline layout, and graphics pipeline state; takes no args and returns nothing.
    auto vertShaderCode = readfile("shaders/voxel.vert.spv");
    auto fragShaderCode = readfile("shaders/voxel.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchainExtent.width;
    viewport.height = (float) swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    std::cout << "Graphics pipeline layout created\n";

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = pipelineLayout;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = nullptr;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    } else {
        std::cout << "Graphics pipeline created\n";
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void VulkanApp::createRenderPass() {
    // Creates color/depth render pass attachments and subpass; takes no args and returns nothing.

    if (depthImageFormat == VK_FORMAT_UNDEFINED) {
        depthImageFormat = findDepthFormat();
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthImageFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    } else {
        std::cout << "Render pass created\n";
    }
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    // GLFW resize callback that marks swapchain recreation state; takes window/size and returns nothing.
    auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void VulkanApp::createFramebuffers() {
    // Creates framebuffer objects for each swapchain image view; takes no args and returns nothing.
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            swapchainImageViews[i],
            depthImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = swapchainExtent.width;
        framebufferInfo.height          = swapchainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        } else {
            std::cout << "Framebuffer " << i << " created\n";
        }
    }
}

void VulkanApp::createCommandPool() {
    // Creates the graphics command pool used for command buffers; takes no args and returns nothing.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = selectedQueues.graphicsFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    } else {
        std::cout << "Command pool created\n";
    }
}

void VulkanApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    // Creates a Vulkan buffer from size/usage/properties and outputs buffer + memory handles.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanApp::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    // Creates an image with given dimensions/format/usage and outputs image + memory handles.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanApp::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    // Creates an image view for an image/aspect combination; returns created image view handle.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

void VulkanApp::buildVoxelMesh() {
    // Builds full CPU voxel mesh around current center chunk; takes no args and returns nothing.
    constexpr int chunkSize = Chunk::SIZE;
    const int chunkRadius = renderDistanceChunks.load(std::memory_order_relaxed);
    const ChunkCoord centerChunk = requestedTerrainCenterChunk;

    voxelVertices.clear();
    voxelIndices.clear();

    // Pre-load all chunks on main thread (thread-safe initialization)
    auto startLoadTime = std::chrono::high_resolution_clock::now();
    
    std::vector<ChunkCoord> chunkCoords;
    for (int cz = -chunkRadius; cz <= chunkRadius; ++cz) {
        for (int cx = -chunkRadius; cx <= chunkRadius; ++cx) {
            for (int cy = -chunkRadius; cy <= chunkRadius; ++cy) {
                ChunkCoord coord{centerChunk.x + cx, centerChunk.y + cy, centerChunk.z + cz};
                world.getOrCreateChunk(coord);  // Pre-load on main thread
                chunkCoords.push_back(coord);
            }
        }
    }

    auto endLoadTime = std::chrono::high_resolution_clock::now();
    auto loadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endLoadTime - startLoadTime).count();

    // Pre-reserve space to reduce allocations
    const int totalChunks = chunkCoords.size();
    const int estimatedVerticesPerChunk = 24 * 16 * 16;  // Heuristic based on avg exposed faces
    voxelVertices.reserve(totalChunks * estimatedVerticesPerChunk);
    voxelIndices.reserve(totalChunks * estimatedVerticesPerChunk * 2);

    glm::vec3 minCorner((std::numeric_limits<float>::max)());
    glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

    // Build mesh for each chunk (now thread-safe - chunks are pre-loaded)
    // Cap to 4 threads to reduce CPU spike while maintaining good parallelism
    const size_t numThreads = std::min(4U, std::max(1U, std::thread::hardware_concurrency()));
    const size_t chunksPerThread = (chunkCoords.size() + numThreads - 1) / numThreads;

    auto startMeshTime = std::chrono::high_resolution_clock::now();

    std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>> threadMeshes(numThreads);
    std::vector<std::pair<glm::vec3, glm::vec3>> threadBounds(numThreads, {
        glm::vec3((std::numeric_limits<float>::max)()),
        glm::vec3(std::numeric_limits<float>::lowest())
    });

    std::vector<std::thread> threads;
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            size_t startIdx = t * chunksPerThread;
            size_t endIdx = std::min(startIdx + chunksPerThread, chunkCoords.size());
            
            auto& threadVertices = threadMeshes[t].first;
            auto& threadIndices = threadMeshes[t].second;
            threadVertices.reserve(estimatedVerticesPerChunk * chunksPerThread);
            threadIndices.reserve(estimatedVerticesPerChunk * 2 * chunksPerThread);

            for (size_t i = startIdx; i < endIdx; ++i) {
                const ChunkCoord coord = chunkCoords[i];
                const Chunk* chunk = world.findChunk(coord);
                if (!chunk) continue;

                const Chunk* neighborXN = world.findChunk({coord.x - 1, coord.y, coord.z});
                const Chunk* neighborXP = world.findChunk({coord.x + 1, coord.y, coord.z});
                const Chunk* neighborYN = world.findChunk({coord.x, coord.y - 1, coord.z});
                const Chunk* neighborYP = world.findChunk({coord.x, coord.y + 1, coord.z});
                const Chunk* neighborZN = world.findChunk({coord.x, coord.y, coord.z - 1});
                const Chunk* neighborZP = world.findChunk({coord.x, coord.y, coord.z + 1});

                const int baseX = coord.x * chunkSize;
                const int baseY = coord.y * chunkSize;
                const int baseZ = coord.z * chunkSize;

                const auto isNeighborSolid = [&](int nx, int ny, int nz) {
                    if (nx >= 0 && nx < chunkSize && ny >= 0 && ny < chunkSize && nz >= 0 && nz < chunkSize) {
                        return chunk->at(nx, ny, nz).isSolid();
                    }

                    const Chunk* neighborChunk = nullptr;
                    int sampleX = nx;
                    int sampleY = ny;
                    int sampleZ = nz;

                    if (nx < 0) {
                        neighborChunk = neighborXN;
                        sampleX += chunkSize;
                    } else if (nx >= chunkSize) {
                        neighborChunk = neighborXP;
                        sampleX -= chunkSize;
                    } else if (ny < 0) {
                        neighborChunk = neighborYN;
                        sampleY += chunkSize;
                    } else if (ny >= chunkSize) {
                        neighborChunk = neighborYP;
                        sampleY -= chunkSize;
                    } else if (nz < 0) {
                        neighborChunk = neighborZN;
                        sampleZ += chunkSize;
                    } else {
                        neighborChunk = neighborZP;
                        sampleZ -= chunkSize;
                    }

                    return neighborChunk != nullptr && neighborChunk->at(sampleX, sampleY, sampleZ).isSolid();
                };

                for (int localZ = 0; localZ < chunkSize; ++localZ) {
                    for (int localY = 0; localY < chunkSize; ++localY) {
                        for (int localX = 0; localX < chunkSize; ++localX) {
                            const Voxel& voxel = chunk->at(localX, localY, localZ);
                            if (!voxel.isSolid()) {
                                continue;
                            }

                            const int worldX = baseX + localX;
                            const int worldY = baseY + localY;
                            const int worldZ = baseZ + localZ;
                            const glm::vec3 basePosition(static_cast<float>(worldX), static_cast<float>(worldY), static_cast<float>(worldZ));

                            for (const auto& face : gFaceDefinitions) {
                                const int nx = localX + face.normal.x;
                                const int ny = localY + face.normal.y;
                                const int nz = localZ + face.normal.z;
                                if (isNeighborSolid(nx, ny, nz)) {
                                    continue;
                                }

                                const glm::vec3 baseColour = voxelBaseColor(voxel.type, face.normal);
                                const float shade = shadeForNormal(face.normal);
                                const glm::vec3 colour = glm::clamp(baseColour * shade, glm::vec3(0.0f), glm::vec3(1.0f));
                                const uint32_t startIndex = static_cast<uint32_t>(threadVertices.size());

                                for (const auto& corner : face.corners) {
                                    const glm::vec3 position = basePosition + corner;
                                    threadVertices.push_back(Vertex{position, colour});
                                    threadBounds[t].first = (glm::min)(threadBounds[t].first, position);
                                    threadBounds[t].second = (glm::max)(threadBounds[t].second, position);
                                }

                                threadIndices.push_back(startIndex);
                                threadIndices.push_back(startIndex + 1);
                                threadIndices.push_back(startIndex + 2);
                                threadIndices.push_back(startIndex);
                                threadIndices.push_back(startIndex + 2);
                                threadIndices.push_back(startIndex + 3);
                            }
                        }
                    }
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto endMeshTime = std::chrono::high_resolution_clock::now();
    auto meshDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endMeshTime - startMeshTime).count();

    // Merge all thread meshes
    uint32_t vertexOffset = 0;
    for (size_t t = 0; t < numThreads; ++t) {
        const auto& threadVerts = threadMeshes[t].first;
        const auto& threadInds = threadMeshes[t].second;

        voxelVertices.insert(voxelVertices.end(), threadVerts.begin(), threadVerts.end());
        
        for (uint32_t idx : threadInds) {
            voxelIndices.push_back(idx + vertexOffset);
        }

        vertexOffset += static_cast<uint32_t>(threadVerts.size());
        minCorner = (glm::min)(minCorner, threadBounds[t].first);
        maxCorner = (glm::max)(maxCorner, threadBounds[t].second);
    }

    if (!voxelVertices.empty()) {
        meshCenter = (minCorner + maxCorner) * 0.5f;
        meshRadius = glm::length(maxCorner - meshCenter);
        meshRadius = std::max(meshRadius, 1.0f);
    } else {
        meshCenter = glm::vec3(0.0f);
        meshRadius = 1.0f;
    }

    std::cout << "Terrain generation timing: " << loadDuration << "ms (load chunks) + " << meshDuration << "ms (parallel mesh build)\n";
}

void VulkanApp::createVertexBuffer() {
    // Uploads voxelVertices into GPU vertex buffer resources; takes no args and returns nothing.
    if (voxelVertices.empty()) {
        std::cerr << "Warning: Creating vertex buffer with no vertices\n";
        return;
    }

    VkDeviceSize bufferSize = sizeof(Vertex) * voxelVertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, voxelVertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanApp::createIndexBuffer() {
    // Uploads voxelIndices into GPU index buffer resources; takes no args and returns nothing.
    if (voxelIndices.empty()) {
        std::cerr << "Warning: Creating index buffer with no indices\n";
        return;
    }

    VkDeviceSize bufferSize = sizeof(uint32_t) * voxelIndices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, voxelIndices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanApp::createUniformBuffers() {
    // Allocates and maps per-frame uniform buffers; takes no args and returns nothing.
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void VulkanApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // Copies size bytes from srcBuffer to dstBuffer using one-time commands; returns nothing.
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence transferFence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fenceInfo, nullptr, &transferFence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to create transfer fence");
    }

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, transferFence) != VK_SUCCESS) {
        vkDestroyFence(device, transferFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to submit transfer command buffer");
    }

    vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, transferFence, nullptr);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t VulkanApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    // Finds a compatible memory type index matching filter/properties; returns memory type index.
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

VkFormat VulkanApp::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
    // Finds first candidate format supporting requested tiling/features; returns selected format.
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }

        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format");
}

VkFormat VulkanApp::findDepthFormat() const {
    // Selects a supported depth format from preferred candidates; returns depth-capable format.
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VulkanApp::hasStencilComponent(VkFormat format) const {
    // Tests whether the provided depth format includes a stencil component; returns true/false.
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanApp::createDepthResources() {
    // Creates depth images, memory, and views for swapchain images; takes no args and returns nothing.
    depthImageFormat = findDepthFormat();

    const size_t imageCount = swapchainImages.size();
    depthImages.resize(imageCount);
    depthImageMemories.resize(imageCount);
    depthImageViews.resize(imageCount);

    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencilComponent(depthImageFormat)) {
        aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    for (size_t i = 0; i < imageCount; ++i) {
        createImage(
            swapchainExtent.width,
            swapchainExtent.height,
            depthImageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImages[i],
            depthImageMemories[i]);

        depthImageViews[i] = createImageView(depthImages[i], depthImageFormat, aspectMask);
    }
}

void VulkanApp::createCommandBuffers() {
    // Allocates primary command buffers for in-flight frames; takes no args and returns nothing.
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    } else {
        std::cout << "Command buffer allocated\n";
    }
}

void VulkanApp::createSyncObjects() {
    // Creates per-frame semaphores and fences for rendering sync; takes no args and returns nothing.
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame");
        }
    }
}

void VulkanApp::cleanupSwapChain() {
    // Destroys all swapchain-dependent Vulkan resources and clears containers; returns nothing.
    for (auto framebuffer : swapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    swapchainFramebuffers.clear();

    for (auto view : depthImageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    depthImageViews.clear();

    for (auto image : depthImages) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device, image, nullptr);
        }
    }
    depthImages.clear();

    for (auto memory : depthImageMemories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }
    depthImageMemories.clear();

    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    for (auto imageView : swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    swapchainImages.clear();
    depthImageFormat = VK_FORMAT_UNDEFINED;
}

void VulkanApp::recreateSwapChain() {
    // Recreates swapchain resources after resize/minimize events; takes no args and returns nothing.
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(windowRef, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(windowRef, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage) {
    // Updates model/view/projection data for current frame image; takes frame index and returns nothing.
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);

    const float aspect = swapchainExtent.height == 0
                             ? 1.0f
                             : swapchainExtent.width / static_cast<float>(swapchainExtent.height);
    const float radius = std::max(meshRadius * 1.8f, 30.0f);
    const float distanceToCenter = glm::length(camera.position() - meshCenter);
    const int activeRenderDistance = renderDistanceChunks.load(std::memory_order_relaxed);
    const float chunkExtent = static_cast<float>((activeRenderDistance + kKeepRadiusExtra + 2) * kChunkSize);
    const float ringDiagonal = chunkExtent * 1.45f;
    const float farPlaneFromDistance = distanceToCenter + ringDiagonal + 96.0f;
    const float farPlaneFromBounds = std::max(distanceToCenter + radius * 2.5f, radius * 5.0f);
    const float farPlane = std::max(farPlaneFromBounds, farPlaneFromDistance);
    camera.setPerspective(45.0f, aspect, 1.5f, std::max(farPlane, 900.0f));

    ubo.view = camera.viewMatrix();
    ubo.proj = camera.projectionMatrix();

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanApp::waitIdle() const {
    // Waits until device finishes all queued work; takes no args and returns nothing.
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }
}
void VulkanApp::rebuildVoxelMesh() {
    // Requests terrain window refresh to trigger mesh rebuild; takes no args and returns nothing.
    requestTerrainWindow(requestedTerrainCenterChunk);
}

void VulkanApp::buildVoxelMeshAsync() {
    // Builds mesh data asynchronously into pending buffers; takes no args and returns nothing.
    // Background thread function - builds mesh without blocking main thread
    constexpr int chunkSize = Chunk::SIZE;
    const int chunkRadius = renderDistanceChunks.load(std::memory_order_relaxed);
    const ChunkCoord centerChunk = requestedTerrainCenterChunk;

    pendingVertices.clear();
    pendingIndices.clear();
    pendingTerrainCenterChunk = centerChunk;

    // Pre-load all chunks on main thread (thread-safe initialization)
    auto startLoadTime = std::chrono::high_resolution_clock::now();
    
    std::vector<ChunkCoord> chunkCoords;
    for (int cz = -chunkRadius; cz <= chunkRadius; ++cz) {
        for (int cx = -chunkRadius; cx <= chunkRadius; ++cx) {
            for (int cy = -chunkRadius; cy <= chunkRadius; ++cy) {
                ChunkCoord coord{centerChunk.x + cx, centerChunk.y + cy, centerChunk.z + cz};
                world.getOrCreateChunk(coord);  // Pre-load on main thread
                chunkCoords.push_back(coord);
            }
        }
    }

    auto endLoadTime = std::chrono::high_resolution_clock::now();
    auto loadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endLoadTime - startLoadTime).count();

    // Pre-reserve space to reduce allocations
    const int totalChunks = chunkCoords.size();
    totalChunksToLoad = totalChunks;
    const int estimatedVerticesPerChunk = 24 * 16 * 16;  // Heuristic based on avg exposed faces
    pendingVertices.reserve(totalChunks * estimatedVerticesPerChunk);
    pendingIndices.reserve(totalChunks * estimatedVerticesPerChunk * 2);

    glm::vec3 minCorner((std::numeric_limits<float>::max)());
    glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

    // Build mesh for each chunk (now thread-safe - chunks are pre-loaded)
    // Cap to 4 threads to reduce CPU spike while maintaining good parallelism
    const size_t numThreads = std::min(4U, std::max(1U, std::thread::hardware_concurrency()));
    const size_t chunksPerThread = (chunkCoords.size() + numThreads - 1) / numThreads;

    auto startMeshTime = std::chrono::high_resolution_clock::now();

    std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>> threadMeshes(numThreads);
    std::vector<std::pair<glm::vec3, glm::vec3>> threadBounds(numThreads, {
        glm::vec3((std::numeric_limits<float>::max)()),
        glm::vec3(std::numeric_limits<float>::lowest())
    });

    std::vector<std::thread> threads;
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            size_t startIdx = t * chunksPerThread;
            size_t endIdx = std::min(startIdx + chunksPerThread, chunkCoords.size());
            
            auto& threadVertices = threadMeshes[t].first;
            auto& threadIndices = threadMeshes[t].second;
            threadVertices.reserve(estimatedVerticesPerChunk * chunksPerThread);
            threadIndices.reserve(estimatedVerticesPerChunk * 2 * chunksPerThread);

            for (size_t i = startIdx; i < endIdx; ++i) {
                const ChunkCoord coord = chunkCoords[i];
                const Chunk* chunk = world.findChunk(coord);
                if (!chunk) continue;

                const Chunk* neighborXN = world.findChunk({coord.x - 1, coord.y, coord.z});
                const Chunk* neighborXP = world.findChunk({coord.x + 1, coord.y, coord.z});
                const Chunk* neighborYN = world.findChunk({coord.x, coord.y - 1, coord.z});
                const Chunk* neighborYP = world.findChunk({coord.x, coord.y + 1, coord.z});
                const Chunk* neighborZN = world.findChunk({coord.x, coord.y, coord.z - 1});
                const Chunk* neighborZP = world.findChunk({coord.x, coord.y, coord.z + 1});

                const int baseX = coord.x * chunkSize;
                const int baseY = coord.y * chunkSize;
                const int baseZ = coord.z * chunkSize;

                const auto isNeighborSolid = [&](int nx, int ny, int nz) {
                    if (nx >= 0 && nx < chunkSize && ny >= 0 && ny < chunkSize && nz >= 0 && nz < chunkSize) {
                        return chunk->at(nx, ny, nz).isSolid();
                    }

                    const Chunk* neighborChunk = nullptr;
                    int sampleX = nx;
                    int sampleY = ny;
                    int sampleZ = nz;

                    if (nx < 0) {
                        neighborChunk = neighborXN;
                        sampleX += chunkSize;
                    } else if (nx >= chunkSize) {
                        neighborChunk = neighborXP;
                        sampleX -= chunkSize;
                    } else if (ny < 0) {
                        neighborChunk = neighborYN;
                        sampleY += chunkSize;
                    } else if (ny >= chunkSize) {
                        neighborChunk = neighborYP;
                        sampleY -= chunkSize;
                    } else if (nz < 0) {
                        neighborChunk = neighborZN;
                        sampleZ += chunkSize;
                    } else {
                        neighborChunk = neighborZP;
                        sampleZ -= chunkSize;
                    }

                    return neighborChunk != nullptr && neighborChunk->at(sampleX, sampleY, sampleZ).isSolid();
                };

                for (int localZ = 0; localZ < chunkSize; ++localZ) {
                    for (int localY = 0; localY < chunkSize; ++localY) {
                        for (int localX = 0; localX < chunkSize; ++localX) {
                            const Voxel& voxel = chunk->at(localX, localY, localZ);
                            if (!voxel.isSolid()) {
                                continue;
                            }

                            const int worldX = baseX + localX;
                            const int worldY = baseY + localY;
                            const int worldZ = baseZ + localZ;
                            const glm::vec3 basePosition(static_cast<float>(worldX), static_cast<float>(worldY), static_cast<float>(worldZ));

                            for (const auto& face : gFaceDefinitions) {
                                const int nx = localX + face.normal.x;
                                const int ny = localY + face.normal.y;
                                const int nz = localZ + face.normal.z;
                                if (isNeighborSolid(nx, ny, nz)) {
                                    continue;
                                }

                                const glm::vec3 baseColour = voxelBaseColor(voxel.type, face.normal);
                                const float shade = shadeForNormal(face.normal);
                                const glm::vec3 colour = glm::clamp(baseColour * shade, glm::vec3(0.0f), glm::vec3(1.0f));
                                const uint32_t startIndex = static_cast<uint32_t>(threadVertices.size());

                                for (const auto& corner : face.corners) {
                                    const glm::vec3 position = basePosition + corner;
                                    threadVertices.push_back(Vertex{position, colour});
                                    threadBounds[t].first = (glm::min)(threadBounds[t].first, position);
                                    threadBounds[t].second = (glm::max)(threadBounds[t].second, position);
                                }

                                threadIndices.push_back(startIndex);
                                threadIndices.push_back(startIndex + 1);
                                threadIndices.push_back(startIndex + 2);
                                threadIndices.push_back(startIndex);
                                threadIndices.push_back(startIndex + 2);
                                threadIndices.push_back(startIndex + 3);
                            }
                        }
                    }
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto endMeshTime = std::chrono::high_resolution_clock::now();
    auto meshDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endMeshTime - startMeshTime).count();

    // Merge all thread meshes
    uint32_t vertexOffset = 0;
    for (size_t t = 0; t < numThreads; ++t) {
        const auto& threadVerts = threadMeshes[t].first;
        const auto& threadInds = threadMeshes[t].second;

        pendingVertices.insert(pendingVertices.end(), threadVerts.begin(), threadVerts.end());
        
        for (uint32_t idx : threadInds) {
            pendingIndices.push_back(idx + vertexOffset);
        }

        vertexOffset += static_cast<uint32_t>(threadVerts.size());
        minCorner = (glm::min)(minCorner, threadBounds[t].first);
        maxCorner = (glm::max)(maxCorner, threadBounds[t].second);
    }

    if (!pendingVertices.empty()) {
        pendingMeshCenter = (minCorner + maxCorner) * 0.5f;
        pendingMeshRadius = glm::length(maxCorner - pendingMeshCenter);
        pendingMeshRadius = std::max(pendingMeshRadius, 1.0f);
    } else {
        pendingMeshCenter = glm::vec3(0.0f);
        pendingMeshRadius = 1.0f;
    }

    std::cout << "Async mesh generation complete: " << loadDuration << "ms (load chunks) + " << meshDuration << "ms (parallel mesh build)\n";
    meshBuildProgress.store(100, std::memory_order_relaxed);
}

void VulkanApp::uploadPendingMesh() {
    // Swaps pending mesh into active data and uploads buffers to GPU; takes no args and returns nothing.
    // Main thread only - swap pending mesh with active and upload to GPU
    {
        std::lock_guard<std::mutex> lock(meshDataLock);
        
        voxelVertices = std::move(pendingVertices);
        voxelIndices = std::move(pendingIndices);
        meshCenter = pendingMeshCenter;
        meshRadius = pendingMeshRadius;
        loadedTerrainCenterChunk = pendingTerrainCenterChunk;
        
        pendingVertices.clear();
        pendingIndices.clear();
    }

    // Destroy old buffers if they exist
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }

    // Create new buffers with loaded mesh data
    createVertexBuffer();
    createIndexBuffer();

    std::cout << "Mesh uploaded to GPU: " << voxelVertices.size() << " vertices, " << voxelIndices.size() << " indices\n";
}

VulkanApp::PendingChunkMesh VulkanApp::buildChunkMeshData(const ChunkCoord& coord) {
    // Generates meshed geometry for one chunk coordinate; returns pending mesh vertices/indices/bounds.
    PendingChunkMesh result{};
    result.coord = coord;

    Chunk chunkSnapshot{};
    Chunk neighborXNNnapshot{};
    Chunk neighborXPSnapshot{};
    Chunk neighborYNSnapshot{};
    Chunk neighborYPSnapshot{};
    Chunk neighborZNSnapshot{};
    Chunk neighborZPSnapshot{};
    bool hasNeighborXN = false;
    bool hasNeighborXP = false;
    bool hasNeighborYN = false;
    bool hasNeighborYP = false;
    bool hasNeighborZN = false;
    bool hasNeighborZP = false;

    {
        std::shared_lock<std::shared_mutex> worldLock(worldDataMutex);
        const Chunk* chunk = world.findChunk(coord);
        if (!chunk) {
            return result;
        }

        chunkSnapshot = *chunk;

        if (const Chunk* neighbor = world.findChunk({coord.x - 1, coord.y, coord.z}); neighbor != nullptr) {
            neighborXNNnapshot = *neighbor;
            hasNeighborXN = true;
        }
        if (const Chunk* neighbor = world.findChunk({coord.x + 1, coord.y, coord.z}); neighbor != nullptr) {
            neighborXPSnapshot = *neighbor;
            hasNeighborXP = true;
        }
        if (const Chunk* neighbor = world.findChunk({coord.x, coord.y - 1, coord.z}); neighbor != nullptr) {
            neighborYNSnapshot = *neighbor;
            hasNeighborYN = true;
        }
        if (const Chunk* neighbor = world.findChunk({coord.x, coord.y + 1, coord.z}); neighbor != nullptr) {
            neighborYPSnapshot = *neighbor;
            hasNeighborYP = true;
        }
        if (const Chunk* neighbor = world.findChunk({coord.x, coord.y, coord.z - 1}); neighbor != nullptr) {
            neighborZNSnapshot = *neighbor;
            hasNeighborZN = true;
        }
        if (const Chunk* neighbor = world.findChunk({coord.x, coord.y, coord.z + 1}); neighbor != nullptr) {
            neighborZPSnapshot = *neighbor;
            hasNeighborZP = true;
        }
    }

    constexpr int chunkSize = Chunk::SIZE;
    const int baseX = coord.x * chunkSize;
    const int baseY = coord.y * chunkSize;
    const int baseZ = coord.z * chunkSize;

    result.minCorner = glm::vec3((std::numeric_limits<float>::max)());
    result.maxCorner = glm::vec3(std::numeric_limits<float>::lowest());

    result.vertices.reserve(24 * chunkSize * chunkSize);
    result.indices.reserve(36 * chunkSize * chunkSize);

    const auto isNeighborSolid = [&](int nx, int ny, int nz) {
        if (nx >= 0 && nx < chunkSize && ny >= 0 && ny < chunkSize && nz >= 0 && nz < chunkSize) {
            return chunkSnapshot.at(nx, ny, nz).isSolid();
        }

        const Chunk* neighborChunk = nullptr;
        int sampleX = nx;
        int sampleY = ny;
        int sampleZ = nz;

        if (nx < 0) {
            neighborChunk = hasNeighborXN ? &neighborXNNnapshot : nullptr;
            sampleX += chunkSize;
        } else if (nx >= chunkSize) {
            neighborChunk = hasNeighborXP ? &neighborXPSnapshot : nullptr;
            sampleX -= chunkSize;
        } else if (ny < 0) {
            neighborChunk = hasNeighborYN ? &neighborYNSnapshot : nullptr;
            sampleY += chunkSize;
        } else if (ny >= chunkSize) {
            neighborChunk = hasNeighborYP ? &neighborYPSnapshot : nullptr;
            sampleY -= chunkSize;
        } else if (nz < 0) {
            neighborChunk = hasNeighborZN ? &neighborZNSnapshot : nullptr;
            sampleZ += chunkSize;
        } else {
            neighborChunk = hasNeighborZP ? &neighborZPSnapshot : nullptr;
            sampleZ -= chunkSize;
        }

        return neighborChunk != nullptr && neighborChunk->at(sampleX, sampleY, sampleZ).isSolid();
    };

    const auto appendFaceQuad = [&](int faceIndex, int slice, int u, int v, int width, int height, uint8_t voxelType) {
        const glm::ivec3 normal = gFaceDefinitions[static_cast<size_t>(faceIndex)].normal;
        const glm::vec3 baseColour = voxelBaseColor(voxelType, normal);
        const float shade = shadeForNormal(normal);
        const glm::vec3 colour = glm::clamp(baseColour * shade, glm::vec3(0.0f), glm::vec3(1.0f));

        std::array<glm::vec3, 4> corners{};

        switch (faceIndex) {
            case 0: {
                const float x = static_cast<float>(baseX + u);
                const float y = static_cast<float>(baseY + v);
                const float z = static_cast<float>(baseZ + slice);
                corners = {{
                    {x, y, z},
                    {x, y + static_cast<float>(height), z},
                    {x + static_cast<float>(width), y + static_cast<float>(height), z},
                    {x + static_cast<float>(width), y, z}
                }};
                break;
            }
            case 1: {
                const float x = static_cast<float>(baseX + u);
                const float y = static_cast<float>(baseY + v);
                const float z = static_cast<float>(baseZ + slice + 1);
                corners = {{
                    {x, y, z},
                    {x + static_cast<float>(width), y, z},
                    {x + static_cast<float>(width), y + static_cast<float>(height), z},
                    {x, y + static_cast<float>(height), z}
                }};
                break;
            }
            case 2: {
                const float x = static_cast<float>(baseX + slice);
                const float y = static_cast<float>(baseY + v);
                const float z = static_cast<float>(baseZ + u);
                corners = {{
                    {x, y, z},
                    {x, y, z + static_cast<float>(width)},
                    {x, y + static_cast<float>(height), z + static_cast<float>(width)},
                    {x, y + static_cast<float>(height), z}
                }};
                break;
            }
            case 3: {
                const float x = static_cast<float>(baseX + slice + 1);
                const float y = static_cast<float>(baseY + v);
                const float z = static_cast<float>(baseZ + u);
                corners = {{
                    {x, y, z},
                    {x, y + static_cast<float>(height), z},
                    {x, y + static_cast<float>(height), z + static_cast<float>(width)},
                    {x, y, z + static_cast<float>(width)}
                }};
                break;
            }
            case 4: {
                const float x = static_cast<float>(baseX + u);
                const float y = static_cast<float>(baseY + slice);
                const float z = static_cast<float>(baseZ + v);
                corners = {{
                    {x, y, z},
                    {x + static_cast<float>(width), y, z},
                    {x + static_cast<float>(width), y, z + static_cast<float>(height)},
                    {x, y, z + static_cast<float>(height)}
                }};
                break;
            }
            default: {
                const float x = static_cast<float>(baseX + u);
                const float y = static_cast<float>(baseY + slice + 1);
                const float z = static_cast<float>(baseZ + v);
                corners = {{
                    {x, y, z},
                    {x, y, z + static_cast<float>(height)},
                    {x + static_cast<float>(width), y, z + static_cast<float>(height)},
                    {x + static_cast<float>(width), y, z}
                }};
                break;
            }
        }

        const uint32_t startIndex = static_cast<uint32_t>(result.vertices.size());
        for (const glm::vec3& position : corners) {
            result.vertices.push_back(Vertex{position, colour});
            result.minCorner = (glm::min)(result.minCorner, position);
            result.maxCorner = (glm::max)(result.maxCorner, position);
        }

        result.indices.push_back(startIndex);
        result.indices.push_back(startIndex + 1);
        result.indices.push_back(startIndex + 2);
        result.indices.push_back(startIndex);
        result.indices.push_back(startIndex + 2);
        result.indices.push_back(startIndex + 3);
    };

    std::vector<uint8_t> mask(static_cast<size_t>(chunkSize * chunkSize), 0);
    for (int faceIndex = 0; faceIndex < static_cast<int>(gFaceDefinitions.size()); ++faceIndex) {
        const glm::ivec3 normal = gFaceDefinitions[static_cast<size_t>(faceIndex)].normal;

        for (int slice = 0; slice < chunkSize; ++slice) {
            std::fill(mask.begin(), mask.end(), 0);

            for (int v = 0; v < chunkSize; ++v) {
                for (int u = 0; u < chunkSize; ++u) {
                    int localX = 0;
                    int localY = 0;
                    int localZ = 0;

                    switch (faceIndex) {
                        case 0:
                        case 1:
                            localX = u;
                            localY = v;
                            localZ = slice;
                            break;
                        case 2:
                        case 3:
                            localX = slice;
                            localY = v;
                            localZ = u;
                            break;
                        default:
                            localX = u;
                            localY = slice;
                            localZ = v;
                            break;
                    }

                    const Voxel& voxel = chunkSnapshot.at(localX, localY, localZ);
                    if (!voxel.isSolid()) {
                        continue;
                    }

                    const int nx = localX + normal.x;
                    const int ny = localY + normal.y;
                    const int nz = localZ + normal.z;
                    if (isNeighborSolid(nx, ny, nz)) {
                        continue;
                    }

                    mask[static_cast<size_t>(v * chunkSize + u)] = voxel.type;
                }
            }

            for (int v = 0; v < chunkSize; ++v) {
                for (int u = 0; u < chunkSize; ) {
                    const uint8_t voxelType = mask[static_cast<size_t>(v * chunkSize + u)];
                    if (voxelType == 0) {
                        ++u;
                        continue;
                    }

                    int width = 1;
                    while (u + width < chunkSize && mask[static_cast<size_t>(v * chunkSize + (u + width))] == voxelType) {
                        ++width;
                    }

                    int height = 1;
                    bool canGrow = true;
                    while (v + height < chunkSize && canGrow) {
                        for (int k = 0; k < width; ++k) {
                            if (mask[static_cast<size_t>((v + height) * chunkSize + (u + k))] != voxelType) {
                                canGrow = false;
                                break;
                            }
                        }
                        if (canGrow) {
                            ++height;
                        }
                    }

                    appendFaceQuad(faceIndex, slice, u, v, width, height, voxelType);

                    for (int dy = 0; dy < height; ++dy) {
                        for (int dx = 0; dx < width; ++dx) {
                            mask[static_cast<size_t>((v + dy) * chunkSize + (u + dx))] = 0;
                        }
                    }

                    u += width;
                }
            }
        }
    }

    result.hasGeometry = !result.indices.empty();
    if (!result.hasGeometry) {
        result.minCorner = glm::vec3(0.0f);
        result.maxCorner = glm::vec3(0.0f);
    }

    return result;
}

void VulkanApp::destroyChunkMeshResources(GpuChunkMesh& mesh) {
    // Destroys GPU buffers owned by a chunk mesh and resets handles; takes mesh ref and returns nothing.
    if (mesh.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
        mesh.vertexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, mesh.vertexBufferMemory, nullptr);
        mesh.vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (mesh.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
        mesh.indexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, mesh.indexBufferMemory, nullptr);
        mesh.indexBufferMemory = VK_NULL_HANDLE;
    }
    mesh.indexCount = 0;
}

bool VulkanApp::isChunkVisible(const GpuChunkMesh& mesh, const std::array<glm::vec4, 6>& frustumPlanes, float maxVisibleDistance, const glm::vec3& cameraPos) const {
    // Performs frustum and distance culling for a chunk mesh; returns true when chunk should render.
    const glm::vec3 chunkCenter = (mesh.minCorner + mesh.maxCorner) * 0.5f;
    const float chunkRadius = glm::length(mesh.maxCorner - chunkCenter);
    const glm::vec3 toChunk = chunkCenter - cameraPos;
    const float distanceSq = glm::dot(toChunk, toChunk);
    const float maxDistanceWithRadius = maxVisibleDistance + chunkRadius;
    if (distanceSq > maxDistanceWithRadius * maxDistanceWithRadius) {
        return false;
    }

    for (const glm::vec4& plane : frustumPlanes) {
        const glm::vec3 normal(plane.x, plane.y, plane.z);
        const glm::vec3 positiveVertex(
            normal.x >= 0.0f ? mesh.maxCorner.x : mesh.minCorner.x,
            normal.y >= 0.0f ? mesh.maxCorner.y : mesh.minCorner.y,
            normal.z >= 0.0f ? mesh.maxCorner.z : mesh.minCorner.z
        );
        if (glm::dot(normal, positiveVertex) + plane.w < 0.0f) {
            return false;
        }
    }

    return true;
}

void VulkanApp::enqueueChunkMeshForDestruction(GpuChunkMesh&& mesh) {
    // Defers chunk mesh destruction until GPU submissions are safe; takes mesh rvalue and returns nothing.
    if (mesh.vertexBuffer == VK_NULL_HANDLE && mesh.indexBuffer == VK_NULL_HANDLE) {
        return;
    }

    DeferredDestroyEntry entry{};
    entry.mesh = std::move(mesh);
    entry.retireAfterCompletedSubmission = completedSubmissionCount + static_cast<uint64_t>(MAX_FRAMES_IN_FLIGHT) + 1;
    deferredDestroyQueue.push_back(std::move(entry));
}

void VulkanApp::processDeferredDestroyQueue() {
    // Destroys deferred chunk meshes whose retire frame is complete; takes no args and returns nothing.
    while (!deferredDestroyQueue.empty()) {
        DeferredDestroyEntry& entry = deferredDestroyQueue.front();
        if (entry.retireAfterCompletedSubmission > completedSubmissionCount) {
            break;
        }
        destroyChunkMeshResources(entry.mesh);
        deferredDestroyQueue.pop_front();
    }
}

void VulkanApp::updateActiveMeshBounds() {
    // Recomputes global mesh center/radius from active chunk bounds; takes no args and returns nothing.
    if (activeChunkMeshes.empty()) {
        meshCenter = glm::vec3(0.0f);
        meshRadius = 1.0f;
        return;
    }

    glm::vec3 minCorner((std::numeric_limits<float>::max)());
    glm::vec3 maxCorner(std::numeric_limits<float>::lowest());
    bool hasGeometry = false;

    for (const auto& [coord, mesh] : activeChunkMeshes) {
        if (mesh.indexCount == 0) {
            continue;
        }
        minCorner = (glm::min)(minCorner, mesh.minCorner);
        maxCorner = (glm::max)(maxCorner, mesh.maxCorner);
        hasGeometry = true;
    }

    if (!hasGeometry) {
        meshCenter = glm::vec3(0.0f);
        meshRadius = 1.0f;
        return;
    }

    meshCenter = (minCorner + maxCorner) * 0.5f;
    meshRadius = std::max(glm::length(maxCorner - meshCenter), 1.0f);
}

void VulkanApp::clearAllChunkMeshes() {
    // Releases all active/deferred/pending chunk mesh GPU resources; takes no args and returns nothing.
    processDeferredDestroyQueue();
    for (auto& [coord, mesh] : activeChunkMeshes) {
        destroyChunkMeshResources(mesh);
    }
    activeChunkMeshes.clear();
    completedEmptyChunkSet.clear();

    for (auto& entry : deferredDestroyQueue) {
        destroyChunkMeshResources(entry.mesh);
    }
    deferredDestroyQueue.clear();

    for (auto& batch : pendingUploadBatches) {
        if (batch.fence != VK_NULL_HANDLE) {
            vkDestroyFence(device, batch.fence, nullptr);
            batch.fence = VK_NULL_HANDLE;
        }
        if (batch.commandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device, commandPool, 1, &batch.commandBuffer);
            batch.commandBuffer = VK_NULL_HANDLE;
        }
        for (const auto& staging : batch.stagingBuffers) {
            if (staging.stagingVertex != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, staging.stagingVertex, nullptr);
            }
            if (staging.stagingVertexMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, staging.stagingVertexMemory, nullptr);
            }
            if (staging.stagingIndex != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, staging.stagingIndex, nullptr);
            }
            if (staging.stagingIndexMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, staging.stagingIndexMemory, nullptr);
            }
        }
        for (auto& ready : batch.readyMeshes) {
            destroyChunkMeshResources(ready.second);
        }
    }
    pendingUploadBatches.clear();
}

ChunkCoord VulkanApp::chunkForPosition(const glm::vec3& position) const {
    // Converts world-space position to integer chunk coordinate; returns containing chunk.
    const int worldX = static_cast<int>(std::floor(position.x));
    const int worldY = static_cast<int>(std::floor(position.y));
    const int worldZ = static_cast<int>(std::floor(position.z));
    return {
        floorDiv(worldX, kChunkSize),
        floorDiv(worldY, kChunkSize),
        floorDiv(worldZ, kChunkSize)
    };
}

float VulkanApp::sampleTerrainHeightAt(int worldX, int worldZ) const {
    // Samples highest solid voxel at X/Z and returns terrain height in world units.
    std::shared_lock<std::shared_mutex> worldLock(worldDataMutex);
    const int activeRenderDistance = renderDistanceChunks.load(std::memory_order_relaxed);
    const int minWorldY = (loadedTerrainCenterChunk.y - activeRenderDistance) * kChunkSize;
    const int maxWorldY = (loadedTerrainCenterChunk.y + activeRenderDistance + 1) * kChunkSize - 1;

    for (int worldY = maxWorldY; worldY >= minWorldY; --worldY) {
        const auto voxel = world.getVoxel(worldX, worldY, worldZ);
        if (voxel.has_value() && voxel->isSolid()) {
            return static_cast<float>(worldY + 1);
        }
    }

    return static_cast<float>(terrainSettings.baseHeight + 2.0f);
}

void VulkanApp::placeCameraOnTerrain() {
    // Places camera above sampled terrain near loaded center chunk; takes no args and returns nothing.
    const int sampleX = loadedTerrainCenterChunk.x * kChunkSize + (kChunkSize / 2);
    const int sampleZ = loadedTerrainCenterChunk.z * kChunkSize + (kChunkSize / 2);
    const float terrainHeight = sampleTerrainHeightAt(sampleX, sampleZ);
    const glm::vec3 spawnPosition(
        static_cast<float>(sampleX) + 0.5f,
        terrainHeight + 4.0f,
        static_cast<float>(sampleZ) + 0.5f
    );

    camera.setPosition(spawnPosition);
    camera.lookAt(spawnPosition + glm::vec3(1.0f, -0.2f, 1.0f));
    inputController.syncOrientationFromCamera();
    cameraPlacedOnTerrain = true;
}

void VulkanApp::processCompletedUploadBatches() {
    // Finalizes completed upload batches and installs ready chunk meshes; takes no args and returns nothing.
    bool boundsDirty = false;

    while (!pendingUploadBatches.empty()) {
        PendingUploadBatch& batch = pendingUploadBatches.front();
        if (batch.fence == VK_NULL_HANDLE) {
            break;
        }

        const VkResult fenceStatus = vkGetFenceStatus(device, batch.fence);
        if (fenceStatus == VK_NOT_READY) {
            break;
        }
        if (fenceStatus != VK_SUCCESS) {
            break;
        }

        vkDestroyFence(device, batch.fence, nullptr);
        batch.fence = VK_NULL_HANDLE;

        if (batch.commandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device, commandPool, 1, &batch.commandBuffer);
            batch.commandBuffer = VK_NULL_HANDLE;
        }

        for (const auto& staging : batch.stagingBuffers) {
            if (staging.stagingVertex != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, staging.stagingVertex, nullptr);
            }
            if (staging.stagingVertexMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, staging.stagingVertexMemory, nullptr);
            }
            if (staging.stagingIndex != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, staging.stagingIndex, nullptr);
            }
            if (staging.stagingIndexMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, staging.stagingIndexMemory, nullptr);
            }
        }

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            for (auto& ready : batch.readyMeshes) {
                completedEmptyChunkSet.erase(ready.first);
                auto existing = activeChunkMeshes.find(ready.first);
                if (existing != activeChunkMeshes.end()) {
                    enqueueChunkMeshForDestruction(std::move(existing->second));
                    existing->second = std::move(ready.second);
                } else {
                    activeChunkMeshes[ready.first] = std::move(ready.second);
                }
                boundsDirty = true;
            }
        }

        pendingUploadBatches.pop_front();
    }

    if (boundsDirty) {
        loadedTerrainCenterChunk = requestedTerrainCenterChunk;
        updateActiveMeshBounds();
    }
}