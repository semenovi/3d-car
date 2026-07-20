#pragma once

#include <algorithm>
#include <array>

#include <glm/glm.hpp>

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

inline bool isBelowVisibility(float brightness01) {
    return brightness01 <= 0.02f;
}

}
