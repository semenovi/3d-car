#pragma once

#include <algorithm>
#include <array>

#include <glm/glm.hpp>

// Elite-style rendering uses a black background with lines/points drawn in one of
// 8 discrete grayscale shades (like the limited color ramps of old vector games).
// Brightness is used both as fake diffuse shading (slope vs. light) and as a
// distance cue (things fade toward the darkest shade and finally vanish).
namespace palette {

inline constexpr std::array<float, 8> kLevels = {
    0.06f, 0.16f, 0.27f, 0.40f, 0.55f, 0.70f, 0.85f, 1.00f,
};

inline glm::vec3 shade(float brightness01) {
    brightness01 = std::clamp(brightness01, 0.0f, 1.0f);
    int idx = static_cast<int>(brightness01 * (kLevels.size() - 1) + 0.5f);
    idx = std::clamp(idx, 0, static_cast<int>(kLevels.size()) - 1);
    float g = kLevels[static_cast<size_t>(idx)];
    return glm::vec3(g, g, g);
}

// Returns true if, after fog attenuation, the point would be indistinguishable
// from the black background and should simply be culled from the mesh.
inline bool isBelowVisibility(float brightness01) {
    return brightness01 <= 0.02f;
}

} // namespace palette
