#version 450

layout(location = 0) in float fragBrightness;
layout(location = 0) out vec4 outColor;

const float kLevels[8] = float[8](0.06, 0.16, 0.27, 0.40, 0.55, 0.70, 0.85, 1.00);

void main() {
    float b = clamp(fragBrightness, 0.0, 1.0);
    if (b <= 0.02) {
        discard;
    }
    int idx = int(b * 7.0 + 0.5);
    idx = clamp(idx, 0, 7);
    float g = kLevels[idx];
    outColor = vec4(g, g, g, 1.0);
}
