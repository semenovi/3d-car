#include "landscape.h"

#include <algorithm>

#include "noise.h"
#include "roads.h"

namespace landscape {

namespace {

// Gentle rolling hills, present everywhere - this is also what roads settle
// down to (see heightAt), so it deliberately excludes mountains/ravines.
float rollingHills(float x, float z) {
    float hills = (noise::fbm(x * 0.012f, z * 0.012f, 5) - 0.5f) * 14.0f;
    float swells = (noise::valueNoise(x * 0.0025f, z * 0.0025f, 999u) - 0.5f) * 10.0f;
    return hills + swells;
}

// Sparse mountain ranges: a slow-varying mask picks out a fraction of the
// world to be mountainous, ridged noise gives sharp peaks/ridgelines within
// it. fadeOut (0..1, from roads::gradingMask) suppresses mountains near roads
// so a road never has to climb a cliff to reach them.
float mountains(float x, float z, float fadeOut) {
    float mask = noise::valueNoise(x * 0.0009f, z * 0.0009f, 4242u);
    mask = std::clamp((mask - 0.55f) / 0.45f, 0.0f, 1.0f);
    float ridge = noise::fbmRidged(x * 0.006f, z * 0.006f, 4, 11u);
    return mask * ridge * ridge * 75.0f * (1.0f - fadeOut);
}

// Sparse ravines/gullies: same idea as mountains but carved downward, at a
// different frequency/seed/offset so they don't coincide with the mountains.
float ravines(float x, float z, float fadeOut) {
    float mask = noise::valueNoise(x * 0.0014f + 300.0f, z * 0.0014f - 300.0f, 7777u);
    mask = std::clamp((mask - 0.6f) / 0.4f, 0.0f, 1.0f);
    float ridge = noise::fbmRidged(x * 0.011f - 91.0f, z * 0.011f + 91.0f, 3, 23u);
    return mask * ridge * ridge * 16.0f * (1.0f - fadeOut);
}

} // namespace

float heightAt(float worldX, float worldZ) {
    float roadDist = roads::distanceToNearestRoad(worldX, worldZ);
    float grading = roads::gradingMask(roadDist);

    float rough = rollingHills(worldX, worldZ) + mountains(worldX, worldZ, grading) - ravines(worldX, worldZ, grading);

    float surface = roads::surfaceMask(roadDist);
    if (surface <= 0.0f) return rough;

    // Mountains/ravines are already faded out within kGradingRadius of a road
    // (much wider than the road's own surface blend), so rollingHills here is
    // never far from `rough` at the point the surface blend kicks in - no cliffs.
    float roadHeight = rollingHills(worldX, worldZ);
    return rough + (roadHeight - rough) * surface;
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
