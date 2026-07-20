#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "renderer.h"
#include "vk_core.h"

// Procedurally generated rolling terrain, streamed in square chunks around the
// camera. Each chunk is rendered as real geometry: a wireframe line-list along
// the heightfield's triangle edges, plus points scattered inside the triangles
// (this is what keeps the surface readable instead of looking like a bare grid
// cage - see the point-scatter idea in Renderer/Vertex). Per-vertex brightness
// is baked from fake diffuse slope lighting; distance fog + palette quantization
// happen in the shader (scene.vert / scene.frag).
class Terrain {
public:
    static constexpr float kCellSize = 6.0f;
    static constexpr int kChunkCells = 16;
    static constexpr int kLoadRadius = 3;
    static constexpr float kChunkWorldSize = kCellSize * kChunkCells;
    static constexpr float kFogDistance = kChunkWorldSize * (kLoadRadius + 0.5f);

    void init(VkCore& core, Renderer& renderer);
    void cleanup(VkCore& core, Renderer& renderer);

    // Streams chunks in/out around the camera. Call once per frame.
    void update(VkCore& core, Renderer& renderer, const glm::vec3& cameraPos);

    // Fills the depth buffer with solid triangles so occluded lines/points get
    // culled by the depth test. Must be called before draw() in the same pass.
    void drawSolid(VkCommandBuffer cmd, Renderer& renderer);
    void draw(VkCommandBuffer cmd, Renderer& renderer);

    // Deterministic height field, also used by the vehicle for ground clamping.
    static float heightAt(float worldX, float worldZ);
    static glm::vec3 normalAt(float worldX, float worldZ);

private:
    struct Chunk {
        GpuMesh lines;
        GpuMesh points;
        GpuMesh solid;
    };

    static int64_t chunkKey(int cx, int cz);
    void loadChunk(VkCore& core, Renderer& renderer, int cx, int cz);

    std::unordered_map<int64_t, Chunk> chunks_;
};
