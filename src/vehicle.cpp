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

#include "terrain.h"

namespace {

// Must match resources/models/pickup.obj (see scratch generator notes there):
// half body width 0.92, wheelbase between hardcoded front/rear axle Z positions.
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
constexpr float kSteerRate = 2.5f; // how fast the steering angle approaches its target

std::vector<Vertex> buildWheelMesh() {
    constexpr int kSegments = 16;
    constexpr int kSpokes = 5;
    std::vector<Vertex> verts;
    glm::vec3 color(kWheelBrightness);

    std::vector<glm::vec3> rim(kSegments);
    for (int i = 0; i < kSegments; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(kSegments)) * 6.2831853f;
        rim[static_cast<size_t>(i)] = glm::vec3(0.0f, std::cos(theta), std::sin(theta));
    }
    for (int i = 0; i < kSegments; ++i) {
        verts.push_back({rim[static_cast<size_t>(i)], color});
        verts.push_back({rim[static_cast<size_t>((i + 1) % kSegments)], color});
    }
    for (int s = 0; s < kSpokes; ++s) {
        int i = (s * kSegments) / kSpokes;
        verts.push_back({glm::vec3(0.0f), color});
        verts.push_back({rim[static_cast<size_t>(i)], color});
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

} // namespace

void Vehicle::init(VkCore& core, Renderer& renderer) {
    auto bodyVerts = loadBodyWireframe(RESOURCE_DIR "models/pickup.obj");
    bodyMesh_ = renderer.uploadMesh(core, bodyVerts);

    auto wheelVerts = buildWheelMesh();
    wheelMesh_ = renderer.uploadMesh(core, wheelVerts);

    position_ = glm::vec3(0.0f, Terrain::heightAt(0.0f, 0.0f), 0.0f);
}

void Vehicle::cleanup(VkCore& core, Renderer& renderer) {
    renderer.destroyMesh(core, bodyMesh_);
    renderer.destroyMesh(core, wheelMesh_);
}

void Vehicle::update(float dt, float throttle, float steer) {
    throttle = std::clamp(throttle, -1.0f, 1.0f);
    steer = std::clamp(steer, -1.0f, 1.0f);

    if (throttle > 0.0f) {
        speed_ += throttle * kAcceleration * dt;
    } else if (throttle < 0.0f) {
        // Braking if moving forward, reversing if already stopped/slow.
        if (speed_ > 0.1f) speed_ += throttle * kBrakeDeceleration * dt;
        else speed_ += throttle * kAcceleration * dt;
    } else {
        float friction = kFriction * dt;
        if (speed_ > 0.0f) speed_ = std::max(0.0f, speed_ - friction);
        else speed_ = std::min(0.0f, speed_ + friction);
    }
    speed_ = std::clamp(speed_, -kReverseMaxSpeed, kMaxSpeed);

    float targetSteer = steer * kMaxSteerRadians;
    float steerDelta = targetSteer - steerAngle_;
    float maxDelta = kSteerRate * dt;
    steerAngle_ += std::clamp(steerDelta, -maxDelta, maxDelta);

    if (std::abs(speed_) > 0.01f) {
        yaw_ += (speed_ / kWheelbase) * std::tan(steerAngle_) * dt;
    }

    glm::vec3 forward(std::sin(yaw_), 0.0f, std::cos(yaw_));
    position_ += forward * speed_ * dt;
    position_.y = Terrain::heightAt(position_.x, position_.z);

    glm::vec3 groundNormal = Terrain::normalAt(position_.x, position_.z);
    tiltNormal_ = glm::normalize(glm::mix(tiltNormal_, groundNormal, std::min(1.0f, dt * 6.0f)));

    wheelRoll_ += (speed_ / kWheelRadius) * dt;
}

glm::mat4 Vehicle::bodyModelMatrix() const {
    glm::vec3 forward = glm::normalize(glm::vec3(std::sin(yaw_), 0.0f, std::cos(yaw_)));
    glm::vec3 up = tiltNormal_;
    glm::vec3 right = glm::normalize(glm::cross(up, forward));
    up = glm::normalize(glm::cross(forward, right));
    forward = glm::normalize(glm::cross(right, up));

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0.0f);
    model[1] = glm::vec4(up, 0.0f);
    model[2] = glm::vec4(forward, 0.0f);
    model[3] = glm::vec4(position_, 1.0f);
    return model;
}

void Vehicle::draw(VkCommandBuffer cmd, Renderer& renderer) {
    glm::mat4 body = bodyModelMatrix();
    renderer.drawMesh(cmd, bodyMesh_, Topology::Lines, body);

    struct WheelSpec { glm::vec3 offset; bool steers; };
    const WheelSpec wheels[4] = {
        {{-kWheelX, kWheelY, kWheelFrontZ}, true},
        {{kWheelX, kWheelY, kWheelFrontZ}, true},
        {{-kWheelX, kWheelY, kWheelRearZ}, false},
        {{kWheelX, kWheelY, kWheelRearZ}, false},
    };

    for (const auto& w : wheels) {
        glm::mat4 m = body * glm::translate(glm::mat4(1.0f), w.offset);
        if (w.steers) m = glm::rotate(m, steerAngle_, glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, wheelRoll_, glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::scale(m, glm::vec3(kWheelRadius));
        renderer.drawMesh(cmd, wheelMesh_, Topology::Lines, m);
    }
}
