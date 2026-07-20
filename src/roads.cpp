#include "roads.h"

#include <algorithm>
#include <cmath>

#include "noise.h"

namespace roads {

namespace {

// Small relative to kSpacing on purpose - see the comment in roads.h.
constexpr float kWanderAmplitude = 40.0f;
constexpr float kWanderFreq = 1.0f / 260.0f;

float smoothstepLocal(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Wandering center of the vertical road line with grid index i, at a given
// world Z. "i" is folded into the noise coordinate so each line gets its own
// independent-looking wander pattern.
float verticalLineX(int i, float worldZ) {
    float wander = noise::valueNoise(worldZ * kWanderFreq, static_cast<float>(i) * 91.7f, 501u);
    return static_cast<float>(i) * kSpacing + (wander - 0.5f) * 2.0f * kWanderAmplitude;
}

// Wandering center of the horizontal road line with grid index j, at a given
// world X.
float horizontalLineZ(int j, float worldX) {
    float wander = noise::valueNoise(worldX * kWanderFreq, static_cast<float>(j) * 57.3f, 907u);
    return static_cast<float>(j) * kSpacing + (wander - 0.5f) * 2.0f * kWanderAmplitude;
}

} // namespace

float distanceToNearestRoad(float worldX, float worldZ) {
    float best = 1e9f;

    int i0 = static_cast<int>(std::lround(worldX / kSpacing));
    for (int i = i0 - 1; i <= i0 + 1; ++i) {
        float cx = verticalLineX(i, worldZ);
        best = std::min(best, std::fabs(worldX - cx));
    }

    int j0 = static_cast<int>(std::lround(worldZ / kSpacing));
    for (int j = j0 - 1; j <= j0 + 1; ++j) {
        float cz = horizontalLineZ(j, worldX);
        best = std::min(best, std::fabs(worldZ - cz));
    }

    return best;
}

float surfaceMask(float distance) {
    return 1.0f - smoothstepLocal(kHalfWidth, kHalfWidth + kEdgeSoftness, distance);
}

float gradingMask(float distance) {
    return 1.0f - smoothstepLocal(kHalfWidth, kHalfWidth + kGradingRadius, distance);
}

} // namespace roads
