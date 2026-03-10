#ifndef VULKAN_APP_HPP
#define VULKAN_APP_HPP

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <condition_variable>
#include <shared_mutex>

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
    struct PendingChunkMesh {
        ChunkCoord coord;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        glm::vec3 minCorner{0.0f};
        glm::vec3 maxCorner{0.0f};
        bool hasGeometry = false;
    };

    struct GpuChunkMesh {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        glm::vec3 minCorner{0.0f};
        glm::vec3 maxCorner{0.0f};
    };

    struct DeferredDestroyEntry {
        GpuChunkMesh mesh;
        uint64_t retireAfterCompletedSubmission = 0;
    };

    struct UploadStagingBuffers {
        VkBuffer stagingVertex = VK_NULL_HANDLE;
        VkDeviceMemory stagingVertexMemory = VK_NULL_HANDLE;
        VkBuffer stagingIndex = VK_NULL_HANDLE;
        VkDeviceMemory stagingIndexMemory = VK_NULL_HANDLE;
    };

    struct PendingUploadBatch {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        std::vector<UploadStagingBuffers> stagingBuffers;
        std::vector<std::pair<ChunkCoord, GpuChunkMesh>> readyMeshes;
    };

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
    ChunkCoord chunkForPosition(const glm::vec3& position) const;
    float sampleTerrainHeightAt(int worldX, int worldZ) const;
    void placeCameraOnTerrain();
    void requestTerrainWindow(const ChunkCoord& centerChunk);
    bool isChunkVisible(const GpuChunkMesh& mesh, const std::array<glm::vec4, 6>& frustumPlanes, float maxVisibleDistance, const glm::vec3& cameraPos) const;
    void runChunkGenerationWorker();
    void runChunkMeshWorker();
    PendingChunkMesh buildChunkMeshData(const ChunkCoord& coord);
    void uploadCompletedChunkMeshes();
    void destroyChunkMeshResources(GpuChunkMesh& mesh);
    void enqueueChunkMeshForDestruction(GpuChunkMesh&& mesh);
    void processDeferredDestroyQueue();
    void processCompletedUploadBatches();
    void updateActiveMeshBounds();
    void clearAllChunkMeshes();

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
    static constexpr int kChunkUploadsPerFrame = 12;
    static constexpr int kPrefetchRadiusExtra = 3;
    static constexpr int kKeepRadiusExtra = 5;
    static constexpr uint32_t kGenerationWorkerCount = 2;
    static constexpr uint32_t kMeshingWorkerCount = 2;
    static constexpr size_t kMaxCompletedChunkMeshes = 128;
    static constexpr size_t kMaxGenerationJobs = 8192;
    static constexpr size_t kMaxMeshJobs = 8192;
    std::atomic<int> renderDistanceChunks{16};
    World world;
    mutable std::shared_mutex worldDataMutex;
    TerrainSettings terrainSettings{};
    glm::vec3 meshCenter{0.0f};
    float meshRadius = 1.0f;
    ChunkCoord loadedTerrainCenterChunk{};
    ChunkCoord requestedTerrainCenterChunk{};
    ChunkCoord pendingTerrainCenterChunk{};
    bool cameraPlacedOnTerrain = false;
    Camera camera;
    InputController inputController;
    double lastFrameTimeSeconds = 0.0;
    std::atomic<bool> meshNeedsRebuild{false};
    
    // Async chunk streaming
    std::vector<std::thread> generationWorkerThreads;
    std::vector<std::thread> meshWorkerThreads;
    std::vector<Vertex> pendingVertices;
    std::vector<uint32_t> pendingIndices;
    glm::vec3 pendingMeshCenter{0.0f};
    float pendingMeshRadius = 1.0f;
    std::mutex meshDataLock;
    std::atomic<bool> meshBuildInProgress{false};
    std::atomic<int> meshBuildProgress{0};  // 0-100
    int totalChunksToLoad = 1;
    std::deque<ChunkCoord> generationJobQueue;
    std::deque<ChunkCoord> meshJobQueue;
    std::unordered_set<ChunkCoord, ChunkCoordHash> desiredChunkSet;
    std::unordered_set<ChunkCoord, ChunkCoordHash> keepChunkSet;
    std::unordered_set<ChunkCoord, ChunkCoordHash> queuedOrGeneratingChunkSet;
    std::unordered_set<ChunkCoord, ChunkCoordHash> queuedOrMeshingChunkSet;
    std::deque<PendingChunkMesh> completedChunkMeshes;
    std::deque<DeferredDestroyEntry> deferredDestroyQueue;
    std::deque<PendingUploadBatch> pendingUploadBatches;
    std::unordered_map<ChunkCoord, GpuChunkMesh, ChunkCoordHash> activeChunkMeshes;
    std::unordered_set<ChunkCoord, ChunkCoordHash> completedEmptyChunkSet;
    std::mutex chunkQueueMutex;
    std::mutex completedChunkMutex;
    std::condition_variable generationQueueCv;
    std::condition_variable meshQueueCv;
    std::atomic<bool> chunkWorkerRunning{false};
    uint64_t submittedSubmissionCount = 0;
    uint64_t completedSubmissionCount = 0;
    uint32_t requestWindowUpdateCounter = 0;
    
    void rebuildVoxelMesh();
    void buildVoxelMeshAsync();
    void uploadPendingMesh();
};

#endif // VULKAN_APP_HPP
