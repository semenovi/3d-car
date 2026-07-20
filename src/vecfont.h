#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vertex.h"

namespace vecfont {

std::vector<Vertex> buildText(const std::string& text, glm::vec2 originNdc, glm::vec2 glyphSize,
                               float spacingNdc, glm::vec3 color);

}
