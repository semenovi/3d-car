#include "vecfont.h"

namespace {

struct Seg { float x0, y0, x1, y1; };

constexpr Seg kTop{0.0f, 1.0f, 1.0f, 1.0f};
constexpr Seg kTopLeft{0.0f, 0.5f, 0.0f, 1.0f};
constexpr Seg kTopRight{1.0f, 0.5f, 1.0f, 1.0f};
constexpr Seg kMiddle{0.0f, 0.5f, 1.0f, 0.5f};
constexpr Seg kBottomLeft{0.0f, 0.0f, 0.0f, 0.5f};
constexpr Seg kBottomRight{1.0f, 0.0f, 1.0f, 0.5f};
constexpr Seg kBottom{0.0f, 0.0f, 1.0f, 0.0f};

std::vector<Seg> glyphSegments(char c) {
    switch (c) {
        case '0': return {kTop, kTopLeft, kTopRight, kBottomLeft, kBottomRight, kBottom};
        case '1': return {kTopRight, kBottomRight};
        case '2': return {kTop, kTopRight, kMiddle, kBottomLeft, kBottom};
        case '3': return {kTop, kTopRight, kMiddle, kBottomRight, kBottom};
        case '4': return {kTopLeft, kTopRight, kMiddle, kBottomRight};
        case '5': return {kTop, kTopLeft, kMiddle, kBottomRight, kBottom};
        case '6': return {kTop, kTopLeft, kMiddle, kBottomLeft, kBottomRight, kBottom};
        case '7': return {kTop, kTopRight, kBottomRight};
        case '8': return {kTop, kTopLeft, kTopRight, kMiddle, kBottomLeft, kBottomRight, kBottom};
        case '9': return {kTop, kTopLeft, kTopRight, kMiddle, kBottomRight, kBottom};
        case 'F': return {kTop, kTopLeft, kMiddle, kBottomLeft};
        case 'P': return {kTop, kTopLeft, kTopRight, kMiddle, kBottomLeft};
        case 'S': return {kTop, kTopLeft, kMiddle, kBottomRight, kBottom};
        case ':': return {Seg{0.45f, 0.62f, 0.55f, 0.62f}, Seg{0.45f, 0.38f, 0.55f, 0.38f}};
        case '-': return {kMiddle};
        default: return {};
    }
}

}

namespace vecfont {

std::vector<Vertex> buildText(const std::string& text, glm::vec2 originNdc, glm::vec2 glyphSize,
                               float spacingNdc, glm::vec3 color) {
    std::vector<Vertex> verts;
    verts.reserve(text.size() * 14);

    float penX = originNdc.x;
    for (char c : text) {
        for (const Seg& seg : glyphSegments(c)) {
            float x0 = penX + seg.x0 * glyphSize.x;
            float y0 = originNdc.y + (1.0f - seg.y0) * glyphSize.y;
            float x1 = penX + seg.x1 * glyphSize.x;
            float y1 = originNdc.y + (1.0f - seg.y1) * glyphSize.y;
            verts.push_back({glm::vec3(x0, y0, 0.0f), color});
            verts.push_back({glm::vec3(x1, y1, 0.0f), color});
        }
        penX += glyphSize.x + spacingNdc;
    }
    return verts;
}

}
