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

// Wheel local space: the spin axis is local X (the axle), the disc lies in the
// Y-Z plane. kSegments is deliberately low (a low-poly tire, not a smooth
// cylinder) - readable as a wireframe without cluttering the view.
constexpr int kWheelSegments = 10;
constexpr float kTireHalfWidth = 0.34f; // tire thickness, as a fraction of wheel radius
constexpr float kHubRadius = 0.55f;     // wheel rim radius, inset from the tire's outer edge

std::vector<glm::vec3> wheelRimCircle(float x, float radius) {
    std::vector<glm::vec3> rim(kWheelSegments);
    for (int i = 0; i < kWheelSegments; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(kWheelSegments)) * 6.2831853f;
        rim[static_cast<size_t>(i)] = glm::vec3(x, radius * std::cos(theta), radius * std::sin(theta));
    }
    return rim;
}

// Solid tire for the depth pre-pass: the tread (outer cylinder side wall) plus
// an *annulus* on each side face between kHubRadius and the outer edge - not a
// full disc. That leaves the middle open (matching the wireframe hub's hole),
// so the hub/spokes stay visible through it instead of being sealed inside an
// opaque drum.
std::vector<Vertex> buildWheelSolidMesh() {
    std::vector<Vertex> verts;
    glm::vec3 color(kWheelBrightness);

    auto front = wheelRimCircle(kTireHalfWidth, 1.0f);
    auto back = wheelRimCircle(-kTireHalfWidth, 1.0f);
    auto frontInner = wheelRimCircle(kTireHalfWidth, kHubRadius);
    auto backInner = wheelRimCircle(-kTireHalfWidth, kHubRadius);

    for (int i = 0; i < kWheelSegments; ++i) {
        int j = (i + 1) % kWheelSegments;
        // Side annuli (quad -> two triangles each).
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

        // Tire tread side wall (quad -> two triangles).
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(j)], color});
        verts.push_back({front[static_cast<size_t>(j)], color});
    }
    return verts;
}

// Wireframe wheel: a tire (two side rims + tread lines joining them) with a
// smaller spoked hub/rim floating in the middle of the tire's width.
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
        // Tread lines joining the two tire sidewalls.
        verts.push_back({front[static_cast<size_t>(i)], color});
        verts.push_back({back[static_cast<size_t>(i)], color});
        // Hub/rim circle.
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

// Triangulated version of the same OBJ, used only for the invisible depth
// pre-pass (fills the depth buffer so the far side of the body doesn't show
// through as X-ray wireframe).
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

} // namespace

void Vehicle::init(VkCore& core, Renderer& renderer) {
    auto bodyVerts = loadBodyWireframe(RESOURCE_DIR "models/pickup.obj");
    bodyMesh_ = renderer.uploadMesh(core, bodyVerts);
    auto bodySolidVerts = loadBodySolid(RESOURCE_DIR "models/pickup.obj");
    bodySolidMesh_ = renderer.uploadMesh(core, bodySolidVerts);

    auto wheelVerts = buildWheelMesh();
    wheelMesh_ = renderer.uploadMesh(core, wheelVerts);
    auto wheelSolidVerts = buildWheelSolidMesh();
    wheelSolidMesh_ = renderer.uploadMesh(core, wheelSolidVerts);

    // Spawn exactly on the i=0 north-south road (not just "near the origin",
    // which the road network only happens to pass close to) and face along it.
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

    // Sample ground height at all 4 wheel contact points (not just the body's
    // center point) and fit a plane through them. A single center sample with
    // a tiny finite-difference epsilon (see landscape::normalAt) can't see
    // bumps/edges across the vehicle's actual 3m wheelbase / ~1.9m track
    // width, so wheels would visibly clip through terrain the center-point
    // normal didn't know about.
    glm::vec3 right(std::cos(yaw_), 0.0f, -std::sin(yaw_));
    auto groundAt = [&](float localX, float localZ) {
        glm::vec3 world = position_ + right * localX + forward * localZ;
        return glm::vec3(world.x, Terrain::heightAt(world.x, world.z), world.z);
    };
    glm::vec3 fl = groundAt(-kWheelX, kWheelFrontZ);
    glm::vec3 fr = groundAt(kWheelX, kWheelFrontZ);
    glm::vec3 rl = groundAt(-kWheelX, kWheelRearZ);
    glm::vec3 rr = groundAt(kWheelX, kWheelRearZ);

    position_.y = (fl.y + fr.y + rl.y + rr.y) * 0.25f;

    glm::vec3 rawNormal = glm::cross(fr - rl, fl - rr);
    float rawLen = glm::length(rawNormal);
    if (rawLen > 1e-5f) {
        glm::vec3 groundNormal = rawNormal / rawLen;
        if (groundNormal.y < 0.0f) groundNormal = -groundNormal;
        tiltNormal_ = glm::mix(tiltNormal_, groundNormal, std::min(1.0f, dt * 6.0f));
    }
    // else: degenerate sample (shouldn't happen on real terrain) - keep the
    // previous tiltNormal_ rather than risk normalizing a near-zero vector.
    float tiltLen = glm::length(tiltNormal_);
    tiltNormal_ = tiltLen > 1e-5f ? tiltNormal_ / tiltLen : glm::vec3(0.0f, 1.0f, 0.0f);

    wheelRoll_ += (speed_ / kWheelRadius) * dt;
}

glm::mat4 Vehicle::bodyModelMatrix() const {
    // Orthogonalize against `up` (the ground normal) first, and only derive
    // `forward` from it afterwards - deriving `up` from the flat heading (as
    // this used to do) throws away any pitch, leaving only roll.
    glm::vec3 headingFlat = glm::normalize(glm::vec3(std::sin(yaw_), 0.0f, std::cos(yaw_)));
    glm::vec3 up = glm::normalize(tiltNormal_);
    glm::vec3 rightRaw = glm::cross(up, headingFlat);
    float rightLen = glm::length(rightRaw);
    // Degenerate only if `up` is (near-)horizontal, i.e. an ~90-degree slope -
    // guard against it rather than normalize a near-zero vector into garbage.
    glm::vec3 right = rightLen > 1e-5f ? rightRaw / rightLen : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 forward = glm::normalize(glm::cross(right, up));

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0.0f);
    model[1] = glm::vec4(up, 0.0f);
    model[2] = glm::vec4(forward, 0.0f);
    model[3] = glm::vec4(position_, 1.0f);
    return model;
}

std::array<glm::mat4, 4> Vehicle::wheelMatrices() const {
    glm::mat4 body = bodyModelMatrix();
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
