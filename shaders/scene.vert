#version 450

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPosFogDist;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 params;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inBrightness;

layout(location = 0) out float fragBrightness;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.viewProj * worldPos;
    gl_PointSize = pc.params.x;

    float dist = length(worldPos.xyz - ubo.cameraPosFogDist.xyz);
    float fog = clamp(1.0 - dist / ubo.cameraPosFogDist.w, 0.0, 1.0);
    fragBrightness = inBrightness.r * fog;
}
