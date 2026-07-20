#include "roads.h"

#include <algorithm>
#include <cmath>

#include "noise.h"
#include "terrain_shapes.h"

namespace roads {

namespace {

constexpr float kWanderAmplitude = 40.0f;
constexpr float kWanderFreq = 1.0f / 260.0f;

constexpr float kAvoidSearchRadius = 90.0f;
constexpr int kAvoidCandidates = 5;

float smoothstepLocal(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float dangerAt(float worldX, float worldZ) {
    return terrain_shapes::mountainRegionMask(worldX, worldZ) + terrain_shapes::ravineRegionMask(worldX, worldZ);
}

template <typename DangerFn>
float softAvoidOffset(DangerFn sampleDanger) {
    float weightedSum = 0.0f;
    float weightTotal = 0.0f;
    for (int k = 0; k < kAvoidCandidates; ++k) {
        float t = static_cast<float>(k) / static_cast<float>(kAvoidCandidates - 1) * 2.0f - 1.0f;
        float offset = t * kAvoidSearchRadius;
        float danger = sampleDanger(offset);
        float weight = 1.0f / (1.0f + danger * 6.0f);
        weightedSum += offset * weight;
        weightTotal += weight;
    }
    return weightTotal > 1e-4f ? weightedSum / weightTotal : 0.0f;
}

}

float verticalLineX(int i, float worldZ) {
    float wander = noise::valueNoise(worldZ * kWanderFreq, static_cast<float>(i) * 91.7f, 501u);
    float candidate = static_cast<float>(i) * kSpacing + (wander - 0.5f) * 2.0f * kWanderAmplitude;
    float avoid = softAvoidOffset([&](float offset) { return dangerAt(candidate + offset, worldZ); });
    return candidate + avoid;
}

float horizontalLineZ(int j, float worldX) {
    float wander = noise::valueNoise(worldX * kWanderFreq, static_cast<float>(j) * 57.3f, 907u);
    float candidate = static_cast<float>(j) * kSpacing + (wander - 0.5f) * 2.0f * kWanderAmplitude;
    float avoid = softAvoidOffset([&](float offset) { return dangerAt(worldX, candidate + offset); });
    return candidate + avoid;
}

float distanceToNearestVerticalRoad(float worldX, float worldZ) {
    float best = 1e9f;
    int i0 = static_cast<int>(std::lround(worldX / kSpacing));
    for (int i = i0 - 1; i <= i0 + 1; ++i) {
        float cx = verticalLineX(i, worldZ);
        best = std::min(best, std::fabs(worldX - cx));
    }
    return best;
}

float distanceToNearestHorizontalRoad(float worldX, float worldZ) {
    float best = 1e9f;
    int j0 = static_cast<int>(std::lround(worldZ / kSpacing));
    for (int j = j0 - 1; j <= j0 + 1; ++j) {
        float cz = horizontalLineZ(j, worldX);
        best = std::min(best, std::fabs(worldZ - cz));
    }
    return best;
}

float distanceToNearestRoad(float worldX, float worldZ) {
    return std::min(distanceToNearestVerticalRoad(worldX, worldZ), distanceToNearestHorizontalRoad(worldX, worldZ));
}

float mask(float distance) {
    return 1.0f - smoothstepLocal(kHalfWidth, kHalfWidth + kBlendRadius, distance);
}

namespace {

struct LineHit {
    float distance;
    glm::vec2 centerPoint;
    glm::vec2 tangent;
};

LineHit bestVerticalLine(float worldX, float worldZ) {
    float bestDist = 1e9f, bestCx = 0.0f;
    int bestI = 0;
    int i0 = static_cast<int>(std::lround(worldX / kSpacing));
    for (int i = i0 - 1; i <= i0 + 1; ++i) {
        float cx = verticalLineX(i, worldZ);
        float d = std::fabs(worldX - cx);
        if (d < bestDist) { bestDist = d; bestCx = cx; bestI = i; }
    }
    const float eps = 2.0f;
    float cxN = verticalLineX(bestI, worldZ + eps);
    float cxP = verticalLineX(bestI, worldZ - eps);
    glm::vec2 tangent = glm::normalize(glm::vec2(cxN - cxP, 2.0f * eps));
    return {bestDist, glm::vec2(bestCx, worldZ), tangent};
}

LineHit bestHorizontalLine(float worldX, float worldZ) {
    float bestDist = 1e9f, bestCz = 0.0f;
    int bestJ = 0;
    int j0 = static_cast<int>(std::lround(worldZ / kSpacing));
    for (int j = j0 - 1; j <= j0 + 1; ++j) {
        float cz = horizontalLineZ(j, worldX);
        float d = std::fabs(worldZ - cz);
        if (d < bestDist) { bestDist = d; bestCz = cz; bestJ = j; }
    }
    const float eps = 2.0f;
    float czN = horizontalLineZ(bestJ, worldX + eps);
    float czP = horizontalLineZ(bestJ, worldX - eps);
    glm::vec2 tangent = glm::normalize(glm::vec2(2.0f * eps, czN - czP));
    return {bestDist, glm::vec2(worldX, bestCz), tangent};
}

constexpr float kSnapBand = 3.5f;

}

EdgeSnap snapToRoadEdge(float worldX, float worldZ) {
    LineHit v = bestVerticalLine(worldX, worldZ);
    LineHit h = bestHorizontalLine(worldX, worldZ);
    bool useVertical = v.distance <= h.distance;
    const LineHit& best = useVertical ? v : h;

    if (best.distance < kHalfWidth - kSnapBand || best.distance > kHalfWidth + kSnapBand) {
        return {worldX, worldZ, false, useVertical, 0.0f};
    }

    glm::vec2 normal(-best.tangent.y, best.tangent.x);
    glm::vec2 toPoint = glm::vec2(worldX, worldZ) - best.centerPoint;
    float side = glm::dot(toPoint, normal) >= 0.0f ? 1.0f : -1.0f;
    glm::vec2 snapped = best.centerPoint + normal * (side * kHalfWidth);
    return {snapped.x, snapped.y, true, useVertical, side};
}

}
