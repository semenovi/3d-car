#pragma once

#include <array>

#include <glm/glm.hpp>

#include "renderer.h"
#include "vk_core.h"

// The player's pickup truck. The body is loaded from resources/models/pickup.obj
// (editable independently of the code) and rendered as a wireframe extracted
// from its triangle/quad edges. Wheels are simple procedural geometry (a rimmed
// circle with spokes), positioned at hardcoded offsets that match the body model.
// Movement is a small arcade driving model: WASD accelerates/brakes/steers, the
// truck follows the terrain height and tilts to match its slope.
class Vehicle {
public:
    void init(VkCore& core, Renderer& renderer);
    void cleanup(VkCore& core, Renderer& renderer);

    // throttle/steer in [-1, 1]; dt in seconds.
    void update(float dt, float throttle, float steer);
    // Depth-only pass (see Renderer::drawSolid / Terrain::drawSolid) - call before draw().
    void drawSolid(VkCommandBuffer cmd, Renderer& renderer);
    void draw(VkCommandBuffer cmd, Renderer& renderer);

    const glm::vec3& position() const { return position_; }
    float yaw() const { return yaw_; }
    glm::mat4 bodyModelMatrix() const;

private:
    struct WheelSpec { glm::vec3 offset; bool steers; };
    std::array<glm::mat4, 4> wheelMatrices() const;

    GpuMesh bodyMesh_;
    GpuMesh bodySolidMesh_;
    GpuMesh wheelMesh_;
    GpuMesh wheelSolidMesh_;

    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    float yaw_ = 0.0f;
    float speed_ = 0.0f;
    float steerAngle_ = 0.0f;
    float wheelRoll_ = 0.0f;
    glm::vec3 tiltNormal_ = glm::vec3(0.0f, 1.0f, 0.0f);
};
