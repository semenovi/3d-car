#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "vertex.h"
#include "vk_core.h"

struct GpuMesh {
    VkCore::Buffer vertexBuffer;
    uint32_t vertexCount = 0;
};

enum class Topology { Lines, Points };

class Renderer {
public:
    void init(VkCore& core);
    void cleanup(VkCore& core);

    GpuMesh uploadMesh(VkCore& core, const std::vector<Vertex>& vertices) const;
    void destroyMesh(VkCore& core, GpuMesh& mesh) const;
    GpuMesh uploadDynamicMesh(VkCore& core, VkDeviceSize capacityBytes, VkBufferUsageFlags extraUsage = 0) const;
    void updateDynamicMesh(VkCore& core, GpuMesh& mesh, const std::vector<Vertex>& vertices) const;

    void beginSceneFrame(VkCore& core, const glm::mat4& viewProj, const glm::vec3& cameraPos, float fogDistance);
    void drawMesh(VkCommandBuffer cmd, const GpuMesh& mesh, Topology topology, const glm::mat4& model, float pointSize = 4.0f);
    void drawSolid(VkCommandBuffer cmd, const GpuMesh& mesh, const glm::mat4& model);
    void drawOverlay(VkCommandBuffer cmd, const GpuMesh& mesh);

private:
    void createDescriptorSetLayout(VkCore& core);
    void createUniformBuffers(VkCore& core);
    void createDescriptorPool(VkCore& core);
    void createDescriptorSets(VkCore& core);
    VkShaderModule loadShader(VkCore& core, const char* filename) const;
    void createScenePipelines(VkCore& core);
    void createOverlayPipeline(VkCore& core);

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkCore::Buffer> uniformBuffers_;

    VkPipelineLayout sceneLayout_ = VK_NULL_HANDLE;
    VkPipeline linePipeline_ = VK_NULL_HANDLE;
    VkPipeline pointPipeline_ = VK_NULL_HANDLE;
    VkPipeline depthPrepassPipeline_ = VK_NULL_HANDLE;

    VkPipelineLayout overlayLayout_ = VK_NULL_HANDLE;
    VkPipeline overlayPipeline_ = VK_NULL_HANDLE;

    size_t currentFrame_ = 0;
};
