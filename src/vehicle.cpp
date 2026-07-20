#include "vehicle.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "roads.h"
#include "terrain.h"

namespace {

constexpr float kWheelRadius = 0.48f;
constexpr float kWheelX = 0.96f;
constexpr float kWheelFrontZ = 1.35f;
constexpr float kWheelRearZ = -1.65f;
constexpr float kWheelY = kWheelRadius;
constexpr float kWheelbase = kWheelFrontZ - kWheelRearZ;

constexpr float kBodyBrightness = 0.85f;
constexpr float kWheelBrightness = 0.75f;

constexpr float kMaxSpeed = 22.0f;
constexpr float kReverseMaxSpeed = 8.0f;
constexpr float kAcceleration = 9.0f;
constexpr float kBrakeDeceleration = 16.0f;
constexpr float kFriction = 4.0f;
constexpr float kMaxSteerRadians = 0.55f;
constexpr float kSteerRate = 2.5f;
constexpr float kSteerInputRate = 4.0f;
constexpr float kMinSteerSpeedFactor = 0.4f;
constexpr float kSteerSpeedSensitivity = 0.05f;

constexpr float kGravity = -18.0f;
constexpr float kMaxLateralDecel = 13.0f;
constexpr float kGroundSnapEpsilon = 0.02f;

constexpr float kSuspensionSpring = 220.0f;
constexpr float kSuspensionDamping = 20.0f;
constexpr float kImpactToSuspensionVel = 0.25f;
constexpr float kMaxSuspensionCompress = -0.35f;
constexpr float kMaxSuspensionExtend = 0.15f;

constexpr float kPitchPerAccel = 0.015f;
constexpr float kPitchRate = 7.0f;
constexpr float kMaxPitch = 0.24f;

constexpr float kRollPerLateralSpeed = 0.035f;
constexpr float kRollRate = 6.0f;
constexpr float kMaxRoll = 0.26f;

constexpr float kGroundNormalSmoothRate = 10.0f;
constexpr float kTiltSpringGrounded = 55.0f;
constexpr float kTiltDampingGrounded = 15.0f;
constexpr float kTiltSpringAir = 0.6f;
constexpr float kTiltDampingAir = 0.5f;
constexpr float kMaxTiltAngularVel = 4.0f;

constexpr int kWheelSegments = 10;
constexpr float kTireHalfWidth = 0.34f;
constexpr float kHubRadius = 0.55f;

std::vector<glm::vec3> wheelRimCircle(float x, float radius) {
    std::vector<glm::vec3> rim(kWheelSegments);
    for (int i = 0; i < kWheelSegments; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(kWheelSegments)) * 6.2831853f;
        rim[static_cast<size_t>(i)] = glm::vec3(x, radius * std::cos(theta), radius * std::sin(theta));
    }
    return rim;
}

std::vector<Vertex> buildWheelSolidMesh() {
    std::vector<Vertex> verts;
    glm::vec3 color(kWheelBrightness);

    auto front = wheelRimCircle(kTireHalfWidth, 1.0f);
    auto back = wheelRimCircle(-kTireHalfWidth, 1.0f);
    auto frontInner = wheelRimCircle(kTireHalfWidth, kHubRadius);
    auto backInner = wheelRimCircle(-kTireHalfWidth, kHubRadius);

    for (int i = 0; i < kWheelSegments; ++i) {
        int j = (i + 1) % kWheelSegments;
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({front[static_cast<size_t>(j)], color});
        verts.push_back({frontInner[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({frontInner[static_cast<size_t>(j)], color});
        verts.push_back({frontInner[static_cast<size_t>(i)], color});

        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({backInner[static_cast<size_t>(j)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({backInner[static_cast<size_t>(j)], color});
        verts.push_back({backInner[static_cast<size_t>(i)], color});

        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(j)], color});
    }
    return verts;
}

std::vector<Vertex> buildWheelMesh() {
    constexpr int kSpokes = 5;
    std::vector<Vertex> verts;
    glm::vec3 color(kWheelBrightness);

    auto front = wheelRimCircle(kTireHalfWidth, 1.0f);
    auto back = wheelRimCircle(-kTireHalfWidth, 1.0f);
    auto hub = wheelRimCircle(0.0f, kHubRadius);

    for (int i = 0; i < kWheelSegments; ++i) {
        int j = (i + 1) % kWheelSegments;
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({front[static_cast<size_t>(j)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({hub[static_cast<size_t>(i)], color});
        verts.push_back({hub[static_cast<size_t>(j)], color});
    }
    glm::vec3 hubCenter(0.0f);
    for (int s = 0; s < kSpokes; ++s) {
        int i = (s * kWheelSegments) / kSpokes;
        verts.push_back({hubCenter, color});
        verts.push_back({hub[static_cast<size_t>(i)], color});
    }
    return verts;
}

std::vector<Vertex> loadBodyWireframe(const char* path) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = false;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        fprintf(stderr, "[vehicle] failed to load %s: %s\n", path, reader.Error().c_str());
        return {};
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    std::set<std::pair<int, int>> uniqueEdges;
    std::vector<Vertex> verts;
    glm::vec3 color(kBodyBrightness);

    auto vertexAt = [&](int i) {
        return glm::vec3(attrib.vertices[static_cast<size_t>(3 * i + 0)],
                          attrib.vertices[static_cast<size_t>(3 * i + 1)],
                          attrib.vertices[static_cast<size_t>(3 * i + 2)]);
    };

    for (const auto& shape : reader.GetShapes()) {
        size_t offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int faceVerts = shape.mesh.num_face_vertices[f];
            for (int i = 0; i < faceVerts; ++i) {
                int a = shape.mesh.indices[offset + static_cast<size_t>(i)].vertex_index;
                int b = shape.mesh.indices[offset + static_cast<size_t>((i + 1) % faceVerts)].vertex_index;
                auto edge = std::make_pair(std::min(a, b), std::max(a, b));
                if (uniqueEdges.insert(edge).second) {
                    verts.push_back({vertexAt(a), color});
                    verts.push_back({vertexAt(b), color});
                }
            }
            offset += static_cast<size_t>(faceVerts);
        }
    }
    return verts;
}

std::vector<Vertex> loadBodySolid(const char* path) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        fprintf(stderr, "[vehicle] failed to load %s: %s\n", path, reader.Error().c_str());
        return {};
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    std::vector<Vertex> verts;
    glm::vec3 color(kBodyBrightness);

    auto vertexAt = [&](int i) {
        return glm::vec3(attrib.vertices[static_cast<size_t>(3 * i + 0)],
                          attrib.vertices[static_cast<size_t>(3 * i + 1)],
                          attrib.vertices[static_cast<size_t>(3 * i + 2)]);
    };

    for (const auto& shape : reader.GetShapes()) {
        for (const auto& index : shape.mesh.indices) {
            verts.push_back({vertexAt(index.vertex_index), color});
        }
    }
    return verts;
}

}

void Vehicle::init(VkCore& core, Renderer& renderer) {
    auto bodyVerts = loadBodyWireframe(RESOURCE_DIR "models/pickup.obj");
    bodyMesh_ = renderer.uploadMesh(core, bodyVerts);
    auto bodySolidVerts = loadBodySolid(RESOURCE_DIR "models/pickup.obj");
    bodySolidMesh_ = renderer.uploadMesh(core, bodySolidVerts);

    auto wheelVerts = buildWheelMesh();
    wheelMesh_ = renderer.uploadMesh(core, wheelVerts);
    auto wheelSolidVerts = buildWheelSolidMesh();
    wheelSolidMesh_ = renderer.uploadMesh(core, wheelSolidVerts);

    float spawnZ = 0.0f;
    float spawnX = roads::verticalLineX(0, spawnZ);
    position_ = glm::vec3(spawnX, Terrain::heightAt(spawnX, spawnZ), spawnZ);

    const float roadDirEps = 2.0f;
    float xBack = roads::verticalLineX(0, spawnZ - roadDirEps);
    float xFwd = roads::verticalLineX(0, spawnZ + roadDirEps);
    yaw_ = std::atan2(xFwd - xBack, 2.0f * roadDirEps);
}

void Vehicle::cleanup(VkCore& core, Renderer& renderer) {
    renderer.destroyMesh(core, bodyMesh_);
    renderer.destroyMesh(core, bodySolidMesh_);
    renderer.destroyMesh(core, wheelMesh_);
    renderer.destroyMesh(core, wheelSolidMesh_);
}

void Vehicle::update(float dt, float throttle, float steer) {
    throttle = std::clamp(throttle, -1.0f, 1.0f);
    steer = std::clamp(steer, -1.0f, 1.0f);

    float prevSpeed = speed_;
    glm::vec3 forward(std::sin(yaw_), 0.0f, std::cos(yaw_));
    glm::vec3 right(std::cos(yaw_), 0.0f, -std::sin(yaw_));
    float lateralSpeed = 0.0f;

    if (grounded_) {
        speed_ = glm::dot(velocity_, forward);

        if (throttle > 0.0f) {
            speed_ += throttle * kAcceleration * dt;
        } else if (throttle < 0.0f) {
            if (speed_ > 0.1f) speed_ += throttle * kBrakeDeceleration * dt;
            else speed_ += throttle * kAcceleration * dt;
        } else {
            float friction = kFriction * dt;
            if (speed_ > 0.0f) speed_ = std::max(0.0f, speed_ - friction);
            else speed_ = std::min(0.0f, speed_ + friction);
        }
        speed_ = std::clamp(speed_, -kReverseMaxSpeed, kMaxSpeed);

        steerInput_ += (steer - steerInput_) * std::min(1.0f, kSteerInputRate * dt);
        float speedFactor = std::max(kMinSteerSpeedFactor, 1.0f / (1.0f + kSteerSpeedSensitivity * std::abs(speed_)));
        float targetSteer = steerInput_ * kMaxSteerRadians * speedFactor;
        float steerDelta = targetSteer - steerAngle_;
        float maxDelta = kSteerRate * dt;
        steerAngle_ += std::clamp(steerDelta, -maxDelta, maxDelta);

        if (std::abs(speed_) > 0.01f) {
            yaw_ += (speed_ / kWheelbase) * std::tan(steerAngle_) * dt;
        }
        forward = glm::vec3(std::sin(yaw_), 0.0f, std::cos(yaw_));
        right = glm::vec3(std::cos(yaw_), 0.0f, -std::sin(yaw_));

        lateralSpeed = glm::dot(velocity_, right);

        float maxRemovable = kMaxLateralDecel * dt;
        if (std::abs(lateralSpeed) <= maxRemovable) {
            lateralSpeed = 0.0f;
        } else {
            lateralSpeed -= std::copysign(maxRemovable, lateralSpeed);
        }

        velocity_ = forward * speed_ + right * lateralSpeed;

        float slope = tiltNormal_.y > 1e-3f
            ? -(tiltNormal_.x * forward.x + tiltNormal_.z * forward.z) / tiltNormal_.y
            : 0.0f;
        vY_ = slope * speed_;
    }

    vY_ += kGravity * dt;

    position_.x += velocity_.x * dt;
    position_.z += velocity_.z * dt;
    position_.y += vY_ * dt;

    auto groundAt = [&](float localX, float localZ) {
        glm::vec3 world = position_ + right * localX + forward * localZ;
        return glm::vec3(world.x, Terrain::heightAt(world.x, world.z), world.z);
    };
    glm::vec3 fl = groundAt(-kWheelX, kWheelFrontZ);
    glm::vec3 fr = groundAt(kWheelX, kWheelFrontZ);
    glm::vec3 rl = groundAt(-kWheelX, kWheelRearZ);
    glm::vec3 rr = groundAt(kWheelX, kWheelRearZ);
    float groundY = (fl.y + fr.y + rl.y + rr.y) * 0.25f;

    glm::vec3 rawNormal = glm::cross(fr - rl, fl - rr);
    float rawLen = glm::length(rawNormal);
    glm::vec3 groundNormal = tiltNormal_;
    if (rawLen > 1e-5f) {
        groundNormal = rawNormal / rawLen;
        if (groundNormal.y < 0.0f) groundNormal = -groundNormal;
    }

    bool wasAirborne = !grounded_;
    if (position_.y <= groundY + kGroundSnapEpsilon) {
        if (wasAirborne && vY_ < -1.0f) {
            suspensionVel_ -= (-vY_) * kImpactToSuspensionVel;
        }
        position_.y = groundY;
        vY_ = 0.0f;
        grounded_ = true;
    } else {
        grounded_ = false;
    }

    auto expectedCornerY = [&](float localX, float localZ) {
        glm::vec3 offset = right * localX + forward * localZ;
        float dy = tiltNormal_.y > 1e-3f
            ? -(tiltNormal_.x * offset.x + tiltNormal_.z * offset.z) / tiltNormal_.y
            : 0.0f;
        return position_.y + dy;
    };
    constexpr float kWheelContactSlack = 0.15f;
    int contactCount = 0;
    contactCount += fl.y >= expectedCornerY(-kWheelX, kWheelFrontZ) - kWheelContactSlack ? 1 : 0;
    contactCount += fr.y >= expectedCornerY(kWheelX, kWheelFrontZ) - kWheelContactSlack ? 1 : 0;
    contactCount += rl.y >= expectedCornerY(-kWheelX, kWheelRearZ) - kWheelContactSlack ? 1 : 0;
    contactCount += rr.y >= expectedCornerY(kWheelX, kWheelRearZ) - kWheelContactSlack ? 1 : 0;
    float contactFraction = static_cast<float>(contactCount) * 0.25f;

    smoothedGroundNormal_ = glm::mix(smoothedGroundNormal_, groundNormal, std::min(1.0f, dt * kGroundNormalSmoothRate));
    float smoothedLen = glm::length(smoothedGroundNormal_);
    if (smoothedLen > 1e-5f) smoothedGroundNormal_ /= smoothedLen;

    glm::vec3 error = smoothedGroundNormal_ - tiltNormal_;
    float spring = kTiltSpringGrounded * contactFraction + kTiltSpringAir * (1.0f - contactFraction);
    float damping = kTiltDampingGrounded * contactFraction + kTiltDampingAir * (1.0f - contactFraction);
    tiltAngularVel_ += (error * spring - tiltAngularVel_ * damping) * dt;
    float angVelLen = glm::length(tiltAngularVel_);
    if (angVelLen > kMaxTiltAngularVel) tiltAngularVel_ *= kMaxTiltAngularVel / angVelLen;
    tiltNormal_ += tiltAngularVel_ * dt;

    float tiltLen = glm::length(tiltNormal_);
    tiltNormal_ = tiltLen > 1e-5f ? tiltNormal_ / tiltLen : glm::vec3(0.0f, 1.0f, 0.0f);

    suspensionVel_ += (-kSuspensionSpring * suspensionOffset_ - kSuspensionDamping * suspensionVel_) * dt;
    suspensionOffset_ = std::clamp(suspensionOffset_ + suspensionVel_ * dt, kMaxSuspensionCompress, kMaxSuspensionExtend);

    float accel = grounded_ ? (speed_ - prevSpeed) / std::max(dt, 1e-4f) : 0.0f;
    float targetPitch = std::clamp(-accel * kPitchPerAccel, -kMaxPitch, kMaxPitch);
    pitchOffset_ += (targetPitch - pitchOffset_) * std::min(1.0f, kPitchRate * dt);

    float targetRoll = std::clamp(-lateralSpeed * kRollPerLateralSpeed, -kMaxRoll, kMaxRoll);
    rollOffset_ += (targetRoll - rollOffset_) * std::min(1.0f, kRollRate * dt);

    wheelRoll_ += (glm::dot(velocity_, forward) / kWheelRadius) * dt;
}

glm::mat4 Vehicle::groundModelMatrix() const {
    glm::vec3 headingFlat = glm::normalize(glm::vec3(std::sin(yaw_), 0.0f, std::cos(yaw_)));
    glm::vec3 up = glm::normalize(tiltNormal_);
    glm::vec3 rightRaw = glm::cross(up, headingFlat);
    float rightLen = glm::length(rightRaw);
    glm::vec3 right = rightLen > 1e-5f ? rightRaw / rightLen : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 forward = glm::normalize(glm::cross(right, up));

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0.0f);
    model[1] = glm::vec4(up, 0.0f);
    model[2] = glm::vec4(forward, 0.0f);
    model[3] = glm::vec4(position_, 1.0f);
    return model;
}

glm::mat4 Vehicle::bodyModelMatrix() const {
    glm::mat4 model = groundModelMatrix();
    glm::vec3 up(model[1]);

    model[3] += glm::vec4(up * suspensionOffset_, 0.0f);
    model = glm::rotate(model, pitchOffset_, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rollOffset_, glm::vec3(0.0f, 0.0f, 1.0f));
    return model;
}

std::array<glm::mat4, 4> Vehicle::wheelMatrices() const {
    glm::mat4 body = groundModelMatrix();
    const WheelSpec specs[4] = {
        {{-kWheelX, kWheelY, kWheelFrontZ}, true},
        {{kWheelX, kWheelY, kWheelFrontZ}, true},
        {{-kWheelX, kWheelY, kWheelRearZ}, false},
        {{kWheelX, kWheelY, kWheelRearZ}, false},
    };

    std::array<glm::mat4, 4> result;
    for (size_t i = 0; i < 4; ++i) {
        glm::mat4 m = body * glm::translate(glm::mat4(1.0f), specs[i].offset);
        if (specs[i].steers) m = glm::rotate(m, steerAngle_, glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, wheelRoll_, glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::scale(m, glm::vec3(kWheelRadius));
        result[i] = m;
    }
    return result;
}

void Vehicle::drawSolid(VkCommandBuffer cmd, Renderer& renderer) {
    renderer.drawSolid(cmd, bodySolidMesh_, bodyModelMatrix());
    for (const auto& m : wheelMatrices()) {
        renderer.drawSolid(cmd, wheelSolidMesh_, m);
    }
}

void Vehicle::draw(VkCommandBuffer cmd, Renderer& renderer) {
    renderer.drawMesh(cmd, bodyMesh_, Topology::Lines, bodyModelMatrix());
    for (const auto& m : wheelMatrices()) {
        renderer.drawMesh(cmd, wheelMesh_, Topology::Lines, m);
    }
}
