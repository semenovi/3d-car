#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "landscape.h"
#include "noise.h"
#include "palette.h"
#include "roads.h"

namespace {

constexpr glm::vec3 kLightDir = glm::vec3(0.35f, 0.82f, 0.45f);

float diffuseBrightness(const glm::vec3& normal) {
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 l = glm::normalize(kLightDir);
    float ambient = 0.30f;
    float diffuse = std::max(glm::dot(n, l), 0.0f);
    return std::min(ambient + diffuse * 0.85f, 1.0f);
}

}

float Terrain::heightAt(float worldX, float worldZ) { return landscape::heightAt(worldX, worldZ); }

glm::vec3 Terrain::normalAt(float worldX, float worldZ) { return landscape::normalAt(worldX, worldZ); }

int64_t Terrain::chunkKey(int cx, int cz) {
    return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

void Terrain::init(VkCore&, Renderer&) {
}

void Terrain::cleanup(VkCore& core, Renderer& renderer) {
    for (auto& [key, chunk] : chunks_) {
        renderer.destroyMesh(core, chunk.lines);
        renderer.destroyMesh(core, chunk.points);
        renderer.destroyMesh(core, chunk.solid);
    }
    chunks_.clear();
}

void Terrain::update(VkCore& core, Renderer& renderer, const glm::vec3& cameraPos) {
    int camChunkX = static_cast<int>(std::floor(cameraPos.x / kChunkWorldSize));
    int camChunkZ = static_cast<int>(std::floor(cameraPos.z / kChunkWorldSize));

    for (int dz = -kLoadRadius; dz <= kLoadRadius; ++dz) {
        for (int dx = -kLoadRadius; dx <= kLoadRadius; ++dx) {
            int cx = camChunkX + dx;
            int cz = camChunkZ + dz;
            int64_t key = chunkKey(cx, cz);
            if (chunks_.find(key) == chunks_.end()) {
                loadChunk(core, renderer, cx, cz);
            }
        }
    }

    const int unloadRadius = kLoadRadius + 1;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        int64_t key = it->first;
        int cx = static_cast<int>(key >> 32);
        int cz = static_cast<int>(static_cast<int32_t>(key & 0xFFFFFFFFu));
        if (std::abs(cx - camChunkX) > unloadRadius || std::abs(cz - camChunkZ) > unloadRadius) {
            renderer.destroyMesh(core, it->second.lines);
            renderer.destroyMesh(core, it->second.points);
            renderer.destroyMesh(core, it->second.solid);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

void Terrain::loadChunk(VkCore& core, Renderer& renderer, int cx, int cz) {
    const int N = kChunkCells;
    const float originX = static_cast<float>(cx) * kChunkWorldSize;
    const float originZ = static_cast<float>(cz) * kChunkWorldSize;

    std::vector<glm::vec3> pos((N + 1) * (N + 1));
    std::vector<glm::vec3> normal((N + 1) * (N + 1));
    std::vector<float> roadMask((N + 1) * (N + 1));
    std::vector<roads::EdgeSnap> snap((N + 1) * (N + 1));
    auto idx = [N](int r, int c) { return r * (N + 1) + c; };

    for (int r = 0; r <= N; ++r) {
        for (int c = 0; c <= N; ++c) {
            float latticeX = originX + static_cast<float>(c) * kCellSize;
            float latticeZ = originZ + static_cast<float>(r) * kCellSize;
            roads::EdgeSnap s = roads::snapToRoadEdge(latticeX, latticeZ);
            float wx = s.snapped ? s.x : latticeX;
            float wz = s.snapped ? s.z : latticeZ;
            float wy = heightAt(wx, wz);
            pos[static_cast<size_t>(idx(r, c))] = glm::vec3(wx, wy, wz);
            normal[static_cast<size_t>(idx(r, c))] = normalAt(wx, wz);
            roadMask[static_cast<size_t>(idx(r, c))] = roads::mask(roads::distanceToNearestRoad(wx, wz));
            snap[static_cast<size_t>(idx(r, c))] = s;
        }
    }

    std::vector<Vertex> lineVerts;
    lineVerts.reserve(static_cast<size_t>(N * (N + 1) * 2 * 2 + N * N));

    auto pushEdge = [&](int r0, int c0, int r1, int c1) {
        const glm::vec3& p0 = pos[static_cast<size_t>(idx(r0, c0))];
        const glm::vec3& p1 = pos[static_cast<size_t>(idx(r1, c1))];
        float b0 = diffuseBrightness(normal[static_cast<size_t>(idx(r0, c0))]);
        float b1 = diffuseBrightness(normal[static_cast<size_t>(idx(r1, c1))]);
        lineVerts.push_back({p0, glm::vec3(b0)});
        lineVerts.push_back({p1, glm::vec3(b1)});
    };

    for (int r = 0; r <= N; ++r)
        for (int c = 0; c < N; ++c) pushEdge(r, c, r, c + 1);
    for (int c = 0; c <= N; ++c)
        for (int r = 0; r < N; ++r) pushEdge(r, c, r + 1, c);

    auto nearRoad = [&](int r, int c) {
        return std::max({roadMask[static_cast<size_t>(idx(r, c))], roadMask[static_cast<size_t>(idx(r + 1, c))],
                          roadMask[static_cast<size_t>(idx(r, c + 1))], roadMask[static_cast<size_t>(idx(r + 1, c + 1))]}) > 0.05f;
    };

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (nearRoad(r, c)) continue;
            if (((r + c) & 1) == 0) pushEdge(r, c, r + 1, c + 1);
        }
    }

    auto pushBoundarySegment = [&](int idxA, int idxB) {
        lineVerts.push_back({pos[static_cast<size_t>(idxA)], glm::vec3(1.0f)});
        lineVerts.push_back({pos[static_cast<size_t>(idxB)], glm::vec3(1.0f)});
    };
    for (float side : {-1.0f, 1.0f}) {
        int prevIdx = -1;
        for (int r = 0; r <= N; ++r) {
            int found = -1;
            for (int c = 0; c <= N; ++c) {
                const roads::EdgeSnap& s = snap[static_cast<size_t>(idx(r, c))];
                if (s.snapped && s.vertical && s.side == side) { found = idx(r, c); break; }
            }
            if (prevIdx >= 0 && found >= 0) pushBoundarySegment(prevIdx, found);
            prevIdx = found;
        }
        prevIdx = -1;
        for (int c = 0; c <= N; ++c) {
            int found = -1;
            for (int r = 0; r <= N; ++r) {
                const roads::EdgeSnap& s = snap[static_cast<size_t>(idx(r, c))];
                if (s.snapped && !s.vertical && s.side == side) { found = idx(r, c); break; }
            }
            if (prevIdx >= 0 && found >= 0) pushBoundarySegment(prevIdx, found);
            prevIdx = found;
        }
    }

    std::vector<Vertex> pointVerts;
    std::vector<Vertex> solidVerts;
    pointVerts.reserve(static_cast<size_t>(N * N * 2));
    solidVerts.reserve(static_cast<size_t>(N * N * 6));
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            const glm::vec3& p00 = pos[static_cast<size_t>(idx(r, c))];
            const glm::vec3& p10 = pos[static_cast<size_t>(idx(r, c + 1))];
            const glm::vec3& p01 = pos[static_cast<size_t>(idx(r + 1, c))];
            const glm::vec3& p11 = pos[static_cast<size_t>(idx(r + 1, c + 1))];

            bool onRoad = nearRoad(r, c);

            struct Tri { glm::vec3 a, b, cc; };
            Tri tris[2] = {{p00, p10, p01}, {p10, p11, p01}};
            for (int t = 0; t < 2; ++t) {
                glm::vec3 faceNormal = glm::normalize(glm::cross(tris[t].b - tris[t].a, tris[t].cc - tris[t].a));
                float brightness = diffuseBrightness(faceNormal);

                if (!onRoad) {
                    uint32_t seed = static_cast<uint32_t>((r * 73856093) ^ (c * 19349663) ^ (t * 83492791) ^ (cx * 15485863) ^ (cz * 32452843));
                    float u = noise::hashFloat(static_cast<int>(seed), static_cast<int>(seed >> 8), 11u);
                    float v = noise::hashFloat(static_cast<int>(seed >> 4), static_cast<int>(seed >> 12), 22u);
                    if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
                    glm::vec3 p = tris[t].a + (tris[t].b - tris[t].a) * u + (tris[t].cc - tris[t].a) * v;
                    pointVerts.push_back({p, glm::vec3(brightness)});
                }

                solidVerts.push_back({tris[t].a, glm::vec3(brightness)});
                solidVerts.push_back({tris[t].b, glm::vec3(brightness)});
                solidVerts.push_back({tris[t].cc, glm::vec3(brightness)});
            }
        }
    }

    Chunk chunk;
    chunk.lines = renderer.uploadMesh(core, lineVerts);
    chunk.points = renderer.uploadMesh(core, pointVerts);
    chunk.solid = renderer.uploadMesh(core, solidVerts);
    chunks_[chunkKey(cx, cz)] = chunk;
}

void Terrain::drawSolid(VkCommandBuffer cmd, Renderer& renderer) {
    glm::mat4 identity(1.0f);
    for (auto& [key, chunk] : chunks_) {
        renderer.drawSolid(cmd, chunk.solid, identity);
    }
}

void Terrain::draw(VkCommandBuffer cmd, Renderer& renderer) {
    glm::mat4 identity(1.0f);
    for (auto& [key, chunk] : chunks_) {
        renderer.drawMesh(cmd, chunk.lines, Topology::Lines, identity);
        renderer.drawMesh(cmd, chunk.points, Topology::Points, identity, 2.5f);
    }
}
