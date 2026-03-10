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

    /**
     * Returns the Vulkan vertex binding description for the Vertex struct.
     * @return Binding metadata that describes stride, binding index, and input rate.
     */
    static VkVertexInputBindingDescription getBindingDescription();
    /**
     * Returns Vulkan vertex attribute descriptions for position and color.
     * @return Array of attribute descriptions matching Vertex::pos and Vertex::color.
     */
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

class VulkanApp {
public:
    /**
     * Constructs the Vulkan application wrapper.
     * @param window GLFW window handle used for surface creation and input.
     * @param enableValidation Whether Vulkan validation layers should be enabled.
     */
    VulkanApp(GLFWwindow* window, bool enableValidation);
    /**
     * Initializes Vulkan objects, terrain systems, worker threads, and camera state.
     * @return No return value.
     */
    void initialize();
    /**
     * Shuts down workers and releases Vulkan and mesh resources.
     * @return No return value.
     */
    void cleanup();
    /**
     * Destroys and resets swapchain-dependent resources.
     * @return No return value.
     */
    void cleanupSwapChain();
    /**
     * Renders a single frame and presents it to the swapchain.
     * @param app Application instance used by the frame draw path.
     * @return No return value.
     */
    void drawFrame(VulkanApp& app);
    /**
     * Runs the main render loop until the window closes.
     * @return No return value.
     */
    void renderLoop();
    /**
     * Waits for the logical device to become idle.
     * @return No return value.
     */
    void waitIdle() const;
    /**
     * Records draw commands for one swapchain image.
     * @param commandBuffer Command buffer to record into.
     * @param imageIndex Swapchain image index targeted by the commands.
     * @return No return value.
     */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    /**
     * GLFW callback that marks framebuffer resize state.
     * @param window GLFW window that triggered the callback.
     * @param width New framebuffer width in pixels.
     * @param height New framebuffer height in pixels.
     * @return No return value.
     */
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
        /**
         * Checks whether both required queue families have been discovered.
         * @return True when graphics and present families are both valid.
         */
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

    /**
     * Creates the Vulkan instance.
     * @return No return value.
     */
    void createInstance();
    /**
     * Creates a Vulkan surface from the GLFW window.
     * @return No return value.
     */
    void createSurface();
    /**
     * Selects a suitable physical device.
     * @return No return value.
     */
    void pickPhysicalDevice();
    /**
     * Creates the logical device and queues.
     * @return No return value.
     */
    void createLogicalDevice();
    /**
     * Creates the swapchain and stores image metadata.
     * @return No return value.
     */
    void createSwapchain();
    /**
     * Creates image views for each swapchain image.
     * @return No return value.
     */
    void createImageViews();
    /**
     * Creates the render pass used by the graphics pipeline.
     * @return No return value.
     */
    void createRenderPass();
    /**
     * Creates pipeline state and graphics pipeline objects.
     * @return No return value.
     */
    void createGraphicsPipeline();
    /**
     * Creates a Vulkan shader module from bytecode.
     * @param device Logical device used to create the module.
     * @param code SPIR-V bytecode data.
     * @return Handle to the created shader module.
     */
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    /**
     * Creates framebuffers for each swapchain image view.
     * @return No return value.
     */
    void createFramebuffers();
    /**
     * Creates the command pool for graphics command buffers.
     * @return No return value.
     */
    void createCommandPool();
    /**
     * Creates and uploads the shared vertex buffer.
     * @return No return value.
     */
    void createVertexBuffer();
    /**
     * Allocates primary command buffers.
     * @return No return value.
     */
    void createCommandBuffers();
    /**
     * Creates semaphores and fences for frame synchronization.
     * @return No return value.
     */
    void createSyncObjects();
    /**
     * Rebuilds swapchain resources after resize/out-of-date events.
     * @return No return value.
     */
    void recreateSwapChain();
    /**
     * Creates a Vulkan image and allocates device memory for it.
     * @param width Image width in texels.
     * @param height Image height in texels.
     * @param format Image format.
     * @param tiling Image tiling mode.
     * @param usage Image usage flags.
     * @param properties Memory property flags for allocation.
     * @param image Output image handle.
     * @param imageMemory Output memory handle bound to image.
     * @return No return value.
     */
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    /**
     * Creates an image view for an image resource.
     * @param image Image handle to view.
     * @param format Pixel format for the view.
     * @param aspectFlags Aspect mask (color/depth/stencil).
     * @return Created image view handle.
     */
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    /**
     * Creates depth images, memory, and views for each swapchain image.
     * @return No return value.
     */
    void createDepthResources();
    /**
     * Finds the preferred depth format supported by the device.
     * @return Chosen depth format.
     */
    VkFormat findDepthFormat() const;
    /**
     * Finds the first supported format from a candidate list.
     * @param candidates Candidate formats to evaluate.
     * @param tiling Required tiling mode.
     * @param features Required format feature flags.
     * @return First compatible format.
     */
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    /**
     * Checks whether a format contains a stencil component.
     * @param format Format to test.
     * @return True when format has stencil bits.
     */
    bool hasStencilComponent(VkFormat format) const;
    /**
     * Creates a Vulkan buffer and allocates memory.
     * @param size Buffer size in bytes.
     * @param usage Buffer usage flags.
     * @param properties Memory property flags.
     * @param buffer Output buffer handle.
     * @param bufferMemory Output memory handle bound to buffer.
     * @return No return value.
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    /**
     * Copies data between Vulkan buffers.
     * @param srcBuffer Source buffer.
     * @param dstBuffer Destination buffer.
     * @param size Number of bytes to copy.
     * @return No return value.
     */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    /**
     * Creates and uploads the shared index buffer.
     * @return No return value.
     */
    void createIndexBuffer();
    /**
     * Creates descriptor set layout bindings.
     * @return No return value.
     */
    void createDescriptorSetLayout();
    /**
     * Creates descriptor pool used by uniform descriptors.
     * @return No return value.
     */
    void createDescriptorPool();
    /**
     * Allocates and writes descriptor sets.
     * @return No return value.
     */
    void createDescriptorSets();
    /**
     * Creates per-frame uniform buffers and mappings.
     * @return No return value.
     */
    void createUniformBuffers();
    /**
     * Updates the uniform buffer for the active frame image.
     * @param currentImage Frame-in-flight index.
     * @return No return value.
     */
    void updateUniformBuffer(uint32_t currentImage);
    /**
     * Finds a compatible memory type index on the physical device.
     * @param typeFilter Bitmask of acceptable memory types.
     * @param properties Required memory properties.
     * @return Matching memory type index.
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    /**
     * Rebuilds CPU-side voxel mesh arrays from world data.
     * @return No return value.
     */
    void buildVoxelMesh();
    /**
     * Converts world position to chunk coordinate.
     * @param position World-space position.
     * @return Chunk coordinate containing the position.
     */
    ChunkCoord chunkForPosition(const glm::vec3& position) const;
    /**
     * Samples procedural terrain height at world X/Z coordinates.
     * @param worldX World X coordinate.
     * @param worldZ World Z coordinate.
     * @return Terrain surface height in world units.
     */
    float sampleTerrainHeightAt(int worldX, int worldZ) const;
    /**
     * Snaps or offsets the camera to terrain height at its position.
     * @return No return value.
     */
    void placeCameraOnTerrain();
    /**
     * Updates desired/keep chunk sets around a center chunk.
     * @param centerChunk Center chunk coordinate for streaming window.
     * @return No return value.
     */
    void requestTerrainWindow(const ChunkCoord& centerChunk);
    /**
     * Tests whether a chunk mesh is visible in frustum and distance.
     * @param mesh Chunk mesh bounds and GPU data.
     * @param frustumPlanes View frustum planes in world space.
     * @param maxVisibleDistance Maximum view distance.
     * @param cameraPos Camera world position.
     * @return True when chunk should be rendered.
     */
    bool isChunkVisible(const GpuChunkMesh& mesh, const std::array<glm::vec4, 6>& frustumPlanes, float maxVisibleDistance, const glm::vec3& cameraPos) const;
    /**
     * Worker loop that generates chunk voxel data.
     * @return No return value.
     */
    void runChunkGenerationWorker();
    /**
     * Worker loop that builds mesh data from generated chunks.
     * @return No return value.
     */
    void runChunkMeshWorker();
    /**
     * Builds CPU mesh data for one chunk coordinate.
     * @param coord Chunk coordinate to mesh.
     * @return Pending mesh payload and bounds for upload.
     */
    PendingChunkMesh buildChunkMeshData(const ChunkCoord& coord);
    /**
     * Uploads completed chunk meshes to GPU buffers.
     * @return No return value.
     */
    void uploadCompletedChunkMeshes();
    /**
     * Destroys Vulkan resources owned by a chunk mesh.
     * @param mesh Mesh resource bundle to destroy/reset.
     * @return No return value.
     */
    void destroyChunkMeshResources(GpuChunkMesh& mesh);
    /**
     * Queues a chunk mesh for deferred GPU-safe destruction.
     * @param mesh Mesh resources to retire.
     * @return No return value.
     */
    void enqueueChunkMeshForDestruction(GpuChunkMesh&& mesh);
    /**
     * Frees deferred chunk meshes whose retire frame has passed.
     * @return No return value.
     */
    void processDeferredDestroyQueue();
    /**
     * Finalizes completed upload batches and installs active meshes.
     * @return No return value.
     */
    void processCompletedUploadBatches();
    /**
     * Recomputes aggregate mesh center/radius from active chunks.
     * @return No return value.
     */
    void updateActiveMeshBounds();
    /**
     * Removes all active chunk meshes and releases GPU resources.
     * @return No return value.
     */
    void clearAllChunkMeshes();

    /**
     * Verifies requested Vulkan validation layers are available.
     * @return True when all requested layers exist.
     */
    bool checkValidationLayerSupport() const;
    /**
     * Evaluates whether a physical device satisfies engine requirements.
     * @param candidate Physical device to validate.
     * @return True when device supports required queues/extensions/features.
     */
    bool isDeviceSuitable(VkPhysicalDevice candidate) const;
    /**
     * Checks whether required device extensions are supported.
     * @param candidate Physical device to check.
     * @return True when all required extensions are present.
     */
    bool checkDeviceExtensionSupport(VkPhysicalDevice candidate) const;
    /**
     * Finds graphics and present queue family indices for a device.
     * @param candidate Physical device to inspect.
     * @return Queue family index set.
     */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice candidate) const;
    /**
     * Queries swapchain capabilities/formats/present modes.
     * @param candidate Physical device to query.
     * @return Swapchain support details for the device and surface.
     */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice candidate) const;
    /**
     * Chooses the preferred swapchain surface format.
     * @param formats Available surface formats.
     * @return Selected surface format.
     */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    /**
     * Chooses the preferred presentation mode.
     * @param presentModes Available presentation modes.
     * @return Selected present mode.
     */
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    /**
     * Chooses swapchain extent based on surface limits and window size.
     * @param capabilities Surface capability limits.
     * @return Chosen swapchain extent.
     */
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
    std::condition_variable completedChunkSpaceCv;
    std::atomic<bool> chunkWorkerRunning{false};
    uint64_t submittedSubmissionCount = 0;
    uint64_t completedSubmissionCount = 0;
    uint32_t requestWindowUpdateCounter = 0;
    
    /**
     * Requests mesh rebuild for current streaming window.
     * @return No return value.
     */
    void rebuildVoxelMesh();
    /**
     * Starts asynchronous voxel mesh build work.
     * @return No return value.
     */
    void buildVoxelMeshAsync();
    /**
     * Uploads pending CPU mesh data to GPU buffers.
     * @return No return value.
     */
    void uploadPendingMesh();
};

#endif // VULKAN_APP_HPP
