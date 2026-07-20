#version 450

// Everything here is real geometry (line segments / points), not a fullscreen
// effect: this shader only computes a per-vertex brightness (fake diffuse slope
// lighting baked on the CPU, attenuated by distance fog) which the fragment
// shader then quantizes to the 8-shade grayscale palette.

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPosFogDist; // xyz = camera world position, w = fog distance
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 params; // x = point size, rest unused
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inBrightness; // .r = base brightness in [0,1]

layout(location = 0) out float fragBrightness;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.viewProj * worldPos;
    gl_PointSize = pc.params.x;

    float dist = length(worldPos.xyz - ubo.cameraPosFogDist.xyz);
    float fog = clamp(1.0 - dist / ubo.cameraPosFogDist.w, 0.0, 1.0);
    fragBrightness = inBrightness.r * fog;
}
