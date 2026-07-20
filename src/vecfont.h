#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vertex.h"

// Tiny 7-segment-style stroke font, drawn with the same line-list machinery as
// everything else in the game (no font rasterization / texture atlas needed).
// Used for the top-right FPS counter overlay.
namespace vecfont {

// originNdc: top-left corner of the text, in normalized device coordinates.
// glyphSize: {width, height} of one glyph, in NDC units.
std::vector<Vertex> buildText(const std::string& text, glm::vec2 originNdc, glm::vec2 glyphSize,
                               float spacingNdc, glm::vec3 color);

} // namespace vecfont
