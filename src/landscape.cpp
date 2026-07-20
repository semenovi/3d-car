#include "landscape.h"

#include "noise.h"
#include "roads.h"
#include "terrain_shapes.h"

namespace landscape {

namespace {

float rollingHills(float x, float z) {
    float hills = (noise::fbm(x * 0.012f, z * 0.012f, 5) - 0.5f) * 14.0f;
    float swells = (noise::valueNoise(x * 0.0025f, z * 0.0025f, 999u) - 0.5f) * 10.0f;
    return hills + swells;
}

float roadGrade(float x, float z) {
    float hills = (noise::fbm(x * 0.012f, z * 0.012f, 2) - 0.5f) * 14.0f;
    float swells = (noise::valueNoise(x * 0.0025f, z * 0.0025f, 999u) - 0.5f) * 10.0f;
    return hills * 0.35f + swells;
}

}

float heightAt(float worldX, float worldZ) {
    float roadMask = roads::mask(roads::distanceToNearestRoad(worldX, worldZ));
    if (roadMask <= 0.0f) {
        return rollingHills(worldX, worldZ) + terrain_shapes::mountainHeight(worldX, worldZ) - terrain_shapes::ravineDepth(worldX, worldZ);
    }

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

}
