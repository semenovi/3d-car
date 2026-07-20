#pragma once

#include <array>

#include <glm/glm.hpp>

#include "renderer.h"
#include "vk_core.h"

class Vehicle {
public:
    void init(VkCore& core, Renderer& renderer);
    void cleanup(VkCore& core, Renderer& renderer);

    void update(float dt, float throttle, float steer);
    void drawSolid(VkCommandBuffer cmd, Renderer& renderer);
    void draw(VkCommandBuffer cmd, Renderer& renderer);

    const glm::vec3& position() const { return position_; }
    float yaw() const { return yaw_; }
    glm::mat4 bodyModelMatrix() const;

private:
    struct WheelSpec { glm::vec3 offset; bool steers; };
    glm::mat4 groundModelMatrix() const;
    std::array<glm::mat4, 4> wheelMatrices() const;

    GpuMesh bodyMesh_;
    GpuMesh bodySolidMesh_;
    GpuMesh wheelMesh_;
    GpuMesh wheelSolidMesh_;

    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    float yaw_ = 0.0f;
    float speed_ = 0.0f;
    float steerAngle_ = 0.0f;
    float steerInput_ = 0.0f;
    float wheelRoll_ = 0.0f;
    glm::vec3 tiltNormal_ = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 tiltAngularVel_ = glm::vec3(0.0f);
    glm::vec3 smoothedGroundNormal_ = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 velocity_ = glm::vec3(0.0f);
    float vY_ = 0.0f;
    bool grounded_ = true;

    float suspensionOffset_ = 0.0f;
    float suspensionVel_ = 0.0f;
    float pitchOffset_ = 0.0f;
    float rollOffset_ = 0.0f;
};
