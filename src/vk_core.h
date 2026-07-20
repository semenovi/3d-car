#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "window.h"

[[noreturn]] inline void fatalError(const char* msg) {
    fprintf(stderr, "initialization failed: %s\n", msg);
    fflush(stderr);
    std::abort();
}
[[noreturn]] inline void fatalError(const std::string& msg) { fatalError(msg.c_str()); }

class VkCore {
public:
    static constexpr int kMaxFramesInFlight = 2;

    void init(Window* window);
    void cleanup();

    void recreateSwapchain();
    void waitIdle();

    bool beginFrame(VkCommandBuffer& outCmd, uint32_t& outImageIndex);
    void endFrame(VkCommandBuffer cmd, uint32_t imageIndex);

    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkRenderPass renderPass() const { return renderPass_; }
    VkExtent2D extent() const { return swapchainExtent_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkCommandPool commandPool() const { return commandPool_; }
    bool supportsWideLines() const { return wideLinesSupported_; }
    bool supportsLargePoints() const { return largePointsSupported_; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;

    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) const;
    void destroyBuffer(Buffer& buf) const;
    Buffer createDeviceLocalBufferWithData(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) const;
    void uploadToHostVisible(const Buffer& buf, const void* data, VkDeviceSize size) const;

    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer cmd) const;

private:
    struct QueueFamilies {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        bool complete() const { return graphics.has_value() && present.has_value(); }
    };

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void cleanupSwapchain();

    QueueFamilies findQueueFamilies(VkPhysicalDevice dev) const;
    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const;
    VkFormat findDepthFormat() const;

    Window* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    QueueFamilies queueFamilies_;
    bool wideLinesSupported_ = false;
    bool largePointsSupported_ = false;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    size_t currentFrame_ = 0;

public:
    bool framebufferResized = false;
};
