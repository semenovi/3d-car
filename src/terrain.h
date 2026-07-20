#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "renderer.h"
#include "vk_core.h"

class Terrain {
public:
    static constexpr float kCellSize = 6.0f;
    static constexpr int kChunkCells = 16;
    static constexpr int kLoadRadius = 3;
    static constexpr float kChunkWorldSize = kCellSize * kChunkCells;
    static constexpr float kFogDistance = kChunkWorldSize * (kLoadRadius + 0.5f);

    void init(VkCore& core, Renderer& renderer);
    void cleanup(VkCore& core, Renderer& renderer);

    void update(VkCore& core, Renderer& renderer, const glm::vec3& cameraPos);

    void drawSolid(VkCommandBuffer cmd, Renderer& renderer);
    void draw(VkCommandBuffer cmd, Renderer& renderer);

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
