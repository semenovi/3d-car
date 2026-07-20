#pragma once

#include <glm/glm.hpp>

namespace landscape {

float heightAt(float worldX, float worldZ);
glm::vec3 normalAt(float worldX, float worldZ);

}
