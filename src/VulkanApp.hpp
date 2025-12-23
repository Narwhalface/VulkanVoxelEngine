#ifndef VULKAN_APP_HPP
#define VULKAN_APP_HPP

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <glfw3.h>

#include <cstdint>
#include <vector>

class VulkanApp {
public:
    VulkanApp(GLFWwindow* window, bool enableValidation);
    void initialize();
    void cleanup();

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

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

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

    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    VkQueue          presentQueue   = VK_NULL_HANDLE;
    QueueFamilyIndices selectedQueues;
    VkPipelineLayout pipelineLayout;

    VkSwapchainKHR   swapchain            = VK_NULL_HANDLE;
    VkFormat         swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D       swapchainExtent{};
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
};

#endif // VULKAN_APP_HPP
