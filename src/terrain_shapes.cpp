#include "terrain_shapes.h"

#include <algorithm>

#include "noise.h"

namespace terrain_shapes {

namespace {

float rawMountainMask(float x, float z) {
    float m = noise::valueNoise(x * 0.0009f, z * 0.0009f, 4242u);
    return std::clamp((m - 0.55f) / 0.45f, 0.0f, 1.0f);
}

float rawRavineMask(float x, float z) {
    float m = noise::valueNoise(x * 0.0014f + 300.0f, z * 0.0014f - 300.0f, 7777u);
    return std::clamp((m - 0.6f) / 0.4f, 0.0f, 1.0f);
}

} // namespace

float mountainRegionMask(float worldX, float worldZ) { return rawMountainMask(worldX, worldZ); }

float ravineRegionMask(float worldX, float worldZ) { return rawRavineMask(worldX, worldZ); }

float mountainHeight(float worldX, float worldZ) {
    float mask = rawMountainMask(worldX, worldZ);
    float ridge = noise::fbmRidged(worldX * 0.006f, worldZ * 0.006f, 4, 11u);
    return mask * ridge * ridge * 75.0f;
}

float ravineDepth(float worldX, float worldZ) {
    float mask = rawRavineMask(worldX, worldZ);
    float ridge = noise::fbmRidged(worldX * 0.011f - 91.0f, worldZ * 0.011f + 91.0f, 3, 23u);
    return mask * ridge * ridge * 16.0f;
}

} // namespace terrain_shapes
