#include "landscape.h"

#include "noise.h"
#include "roads.h"
#include "terrain_shapes.h"

namespace landscape {

namespace {

// Gentle rolling hills, present everywhere.
float rollingHills(float x, float z) {
    float hills = (noise::fbm(x * 0.012f, z * 0.012f, 5) - 0.5f) * 14.0f;
    float swells = (noise::valueNoise(x * 0.0025f, z * 0.0025f, 999u) - 0.5f) * 10.0f;
    return hills + swells;
}

// What a road settles down to: the same broad, large-scale trend as
// rollingHills (a graded road still gently follows the lay of the land over
// long distances) but with the higher-frequency bumps damped out, as if the
// surface had actually been leveled/compacted for the road.
float roadGrade(float x, float z) {
    float hills = (noise::fbm(x * 0.012f, z * 0.012f, 2) - 0.5f) * 14.0f;
    float swells = (noise::valueNoise(x * 0.0025f, z * 0.0025f, 999u) - 0.5f) * 10.0f;
    return hills * 0.35f + swells;
}

} // namespace

float heightAt(float worldX, float worldZ) {
    float roadMask = roads::mask(roads::distanceToNearestRoad(worldX, worldZ));
    if (roadMask <= 0.0f) {
        return rollingHills(worldX, worldZ) + terrain_shapes::mountainHeight(worldX, worldZ) - terrain_shapes::ravineDepth(worldX, worldZ);
    }

    // One mask drives both the mountain/ravine fade-out *and* the blend
    // toward the graded road height, so they're mathematically tied together
    // - by the time the road is fully graded (roadMask == 1) the rough side
    // of the mix has *also* already faded its mountains/ravines to 0, so
    // both sides of the blend agree and there's no cliff at the shoulder.
    // Roads also actively steer away from mountains/ravines (see roads.cpp),
    // so in practice this fallback flattening rarely has much work to do.
    float rough = rollingHills(worldX, worldZ) + terrain_shapes::mountainHeight(worldX, worldZ) * (1.0f - roadMask) -
                  terrain_shapes::ravineDepth(worldX, worldZ) * (1.0f - roadMask);
    float graded = roadGrade(worldX, worldZ);
    return rough + (graded - rough) * roadMask;
}

glm::vec3 normalAt(float worldX, float worldZ) {
    const float eps = 0.75f;
    float hL = heightAt(worldX - eps, worldZ);
    float hR = heightAt(worldX + eps, worldZ);
    float hD = heightAt(worldX, worldZ - eps);
    float hU = heightAt(worldX, worldZ + eps);
    return glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
}

} // namespace landscape
