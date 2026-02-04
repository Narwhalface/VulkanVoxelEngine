#ifndef VULKAN_APP_HPP
#define VULKAN_APP_HPP

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <vector>

#include "Camera.hpp"
#include "InputController.hpp"
#include "World.hpp"

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

class VulkanApp {
public:
    VulkanApp(GLFWwindow* window, bool enableValidation);
    void initialize();
    void cleanup();
    void cleanupSwapChain();
    void drawFrame(VulkanApp& app);
    void renderLoop();
    void waitIdle() const;
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
    struct QueueFamilyIndices {
        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t presentFamily  = UINT32_MAX;
        bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR        capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createCommandBuffers();
    void createSyncObjects();
    void recreateSwapChain();
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createDepthResources();
    VkFormat findDepthFormat() const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    bool hasStencilComponent(VkFormat format) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createIndexBuffer();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void buildVoxelMesh();

    bool checkValidationLayerSupport() const;
    bool isDeviceSuitable(VkPhysicalDevice candidate) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice candidate) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice candidate) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice candidate) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    GLFWwindow* windowRef = nullptr;
    bool enableValidationLayers = false;

    const std::vector<const char*> validationLayers;
    const std::vector<const char*> deviceExtensions;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    VkQueue          presentQueue   = VK_NULL_HANDLE;
    QueueFamilyIndices selectedQueues;
    VkRenderPass     renderPass     = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout  = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkSwapchainKHR   swapchain            = VK_NULL_HANDLE;
    VkFormat         swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D       swapchainExtent{};
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkImage> depthImages;
    std::vector<VkDeviceMemory> depthImageMemories;
    std::vector<VkImageView> depthImageViews;
    VkFormat depthImageFormat = VK_FORMAT_UNDEFINED;
    VkCommandPool    commandPool    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>  commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<Vertex> voxelVertices;
    std::vector<uint32_t> voxelIndices;
    World world;
    TerrainSettings terrainSettings{};
    glm::vec3 meshCenter{0.0f};
    float meshRadius = 1.0f;
    Camera camera;
    InputController inputController;
    double lastFrameTimeSeconds = 0.0;
};

#endif // VULKAN_APP_HPP
