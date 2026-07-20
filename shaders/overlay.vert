#version 450

// Screen-space overlay pass (FPS counter vector font): input positions are already
// in normalized device coordinates, so no view/projection transform is applied.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
    gl_PointSize = 1.0;
    fragColor = inColor;
}
