#version 450

// Screen-space overlay (FPS vector font): color is passed through as-is, no
// fog/lighting/quantization - the CPU side already picks a palette shade.

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
