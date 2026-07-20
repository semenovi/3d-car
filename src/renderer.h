#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "vertex.h"
#include "vk_core.h"

// A GPU-resident mesh: a vertex buffer plus how many vertices to draw with which
// topology. No index buffer - meshes here are small (terrain chunks, one vehicle
// body, a handful of wheels) and edges/points are cheap to just duplicate.
struct GpuMesh {
    VkCore::Buffer vertexBuffer;
    uint32_t vertexCount = 0;
};

enum class Topology { Lines, Points };

// Draws everything with a black-background, 8-shade grayscale vector-graphics
// look: two 3D pipelines (line list / point list) sharing a view-projection UBO,
// plus a screen-space overlay line pipeline used for the FPS vector font.
class Renderer {
public:
    void init(VkCore& core);
    void cleanup(VkCore& core);

    GpuMesh uploadMesh(VkCore& core, const std::vector<Vertex>& vertices) const;
    void destroyMesh(VkCore& core, GpuMesh& mesh) const;
    // Overwrites a mesh's contents in place (must not be larger than original capacity).
    // Used for the terrain's per-frame changing chunks and the FPS overlay text.
    GpuMesh uploadDynamicMesh(VkCore& core, VkDeviceSize capacityBytes, VkBufferUsageFlags extraUsage = 0) const;
    void updateDynamicMesh(VkCore& core, GpuMesh& mesh, const std::vector<Vertex>& vertices) const;

    void beginSceneFrame(VkCore& core, const glm::mat4& viewProj, const glm::vec3& cameraPos, float fogDistance);
    void drawMesh(VkCommandBuffer cmd, const GpuMesh& mesh, Topology topology, const glm::mat4& model, float pointSize = 4.0f);
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

    VkPipelineLayout overlayLayout_ = VK_NULL_HANDLE;
    VkPipeline overlayPipeline_ = VK_NULL_HANDLE;

    size_t currentFrame_ = 0;
};
