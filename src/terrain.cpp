#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "palette.h"

namespace {

// ---- deterministic hash-based value noise (no external noise library) ----

uint32_t hash2i(int x, int z, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float hashFloat(int x, int z, uint32_t seed) {
    return static_cast<float>(hash2i(x, z, seed) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

float valueNoise(float x, float z, uint32_t seed) {
    int x0 = static_cast<int>(std::floor(x));
    int z0 = static_cast<int>(std::floor(z));
    int x1 = x0 + 1, z1 = z0 + 1;
    float tx = smoothstep(x - static_cast<float>(x0));
    float tz = smoothstep(z - static_cast<float>(z0));

    float v00 = hashFloat(x0, z0, seed);
    float v10 = hashFloat(x1, z0, seed);
    float v01 = hashFloat(x0, z1, seed);
    float v11 = hashFloat(x1, z1, seed);

    float a = v00 + (v10 - v00) * tx;
    float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * tz;
}

float fbm(float x, float z) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float maxAmp = 0.0f;
    for (int octave = 0; octave < 5; ++octave) {
        sum += valueNoise(x * frequency, z * frequency, static_cast<uint32_t>(octave) * 101u + 7u) * amplitude;
        maxAmp += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.03f;
    }
    return sum / maxAmp;
}

constexpr glm::vec3 kLightDir = glm::vec3(0.35f, 0.82f, 0.45f); // normalized-ish, points toward the "sun"

float diffuseBrightness(const glm::vec3& normal) {
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 l = glm::normalize(kLightDir);
    float ambient = 0.30f;
    float diffuse = std::max(glm::dot(n, l), 0.0f);
    return std::min(ambient + diffuse * 0.85f, 1.0f);
}

} // namespace

float Terrain::heightAt(float worldX, float worldZ) {
    float base = fbm(worldX * 0.012f, worldZ * 0.012f);
    // Re-center around 0 and shape into rolling hills with occasional larger swells.
    float hills = (base - 0.5f) * 14.0f;
    float swellsRaw = valueNoise(worldX * 0.0025f, worldZ * 0.0025f, 999u);
    float swells = (swellsRaw - 0.5f) * 10.0f;
    return hills + swells;
}

glm::vec3 Terrain::normalAt(float worldX, float worldZ) {
    const float eps = 0.75f;
    float hL = heightAt(worldX - eps, worldZ);
    float hR = heightAt(worldX + eps, worldZ);
    float hD = heightAt(worldX, worldZ - eps);
    float hU = heightAt(worldX, worldZ + eps);
    glm::vec3 n = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
    return n;
}

int64_t Terrain::chunkKey(int cx, int cz) {
    return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

void Terrain::init(VkCore&, Renderer&) {
    // Chunks are loaded lazily by update() once we know the camera position.
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

    // Unload chunks that fell well outside the load radius (small hysteresis margin).
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
    auto idx = [N](int r, int c) { return r * (N + 1) + c; };

    for (int r = 0; r <= N; ++r) {
        for (int c = 0; c <= N; ++c) {
            float wx = originX + static_cast<float>(c) * kCellSize;
            float wz = originZ + static_cast<float>(r) * kCellSize;
            float wy = heightAt(wx, wz);
            pos[static_cast<size_t>(idx(r, c))] = glm::vec3(wx, wy, wz);
            normal[static_cast<size_t>(idx(r, c))] = normalAt(wx, wz);
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

    // Grid lines (horizontal + vertical).
    for (int r = 0; r <= N; ++r)
        for (int c = 0; c < N; ++c) pushEdge(r, c, r, c + 1);
    for (int c = 0; c <= N; ++c)
        for (int r = 0; r < N; ++r) pushEdge(r, c, r + 1, c);

    // Diagonal edges (checkerboard, to keep the mesh from becoming too cluttered).
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (((r + c) & 1) == 0) pushEdge(r, c, r + 1, c + 1);
        }
    }

    // Scattered points across every triangle for surface readability, plus the
    // same triangles as solid (invisible) geometry for the depth pre-pass, so
    // terrain on the far side of a hill doesn't show through it.
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

            // Triangle A: p00, p10, p01 ; Triangle B: p10, p11, p01
            struct Tri { glm::vec3 a, b, cc; };
            Tri tris[2] = {{p00, p10, p01}, {p10, p11, p01}};
            for (int t = 0; t < 2; ++t) {
                glm::vec3 faceNormal = glm::normalize(glm::cross(tris[t].b - tris[t].a, tris[t].cc - tris[t].a));
                float brightness = diffuseBrightness(faceNormal);
                uint32_t seed = static_cast<uint32_t>((r * 73856093) ^ (c * 19349663) ^ (t * 83492791) ^ (cx * 15485863) ^ (cz * 32452843));
                float u = hashFloat(static_cast<int>(seed), static_cast<int>(seed >> 8), 11u);
                float v = hashFloat(static_cast<int>(seed >> 4), static_cast<int>(seed >> 12), 22u);
                if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
                glm::vec3 p = tris[t].a + (tris[t].b - tris[t].a) * u + (tris[t].cc - tris[t].a) * v;
                pointVerts.push_back({p, glm::vec3(brightness)});

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
