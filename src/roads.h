#pragma once

#include <glm/glm.hpp>

namespace roads {

inline constexpr float kSpacing = 650.0f;
inline constexpr float kHalfWidth = 4.0f;
inline constexpr float kBlendRadius = 10.0f;

float distanceToNearestRoad(float worldX, float worldZ);

float distanceToNearestVerticalRoad(float worldX, float worldZ);
float distanceToNearestHorizontalRoad(float worldX, float worldZ);

float verticalLineX(int i, float worldZ);
float horizontalLineZ(int j, float worldX);

float mask(float distance);

struct EdgeSnap {
    float x, z;
    bool snapped;
    bool vertical;
    float side;
};
EdgeSnap snapToRoadEdge(float worldX, float worldZ);

}
