#pragma once

#include <glm/glm.hpp>

// The final world heightfield: rolling hills everywhere, sparse mountain
// ranges and carved ravines layered on top, and the road network (roads.h)
// flattened into it. Used by both the terrain mesh generator (terrain.cpp)
// and the vehicle's ground clamping (vehicle.cpp).
namespace landscape {

float heightAt(float worldX, float worldZ);
glm::vec3 normalAt(float worldX, float worldZ);

} // namespace landscape
