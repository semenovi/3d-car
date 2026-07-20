#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer.h"
#include "terrain.h"
#include "vecfont.h"
#include "vehicle.h"
#include "vk_core.h"

namespace {

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;

struct AppState {
    VkCore* core = nullptr;
    bool firstMouse = true;
    double lastMouseX = 0.0, lastMouseY = 0.0;
    float orbitYaw = 0.0f;
    float orbitPitch = 0.32f;

    // Fullscreen toggle bookkeeping.
    bool fullscreen = false;
    int windowedX = 100, windowedY = 100, windowedW = kDefaultWidth, windowedH = kDefaultHeight;
    bool f11WasDown = false;
};

void framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* state = reinterpret_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state && state->core) state->core->framebufferResized = true;
}

void toggleFullscreen(GLFWwindow* window, AppState& state) {
    if (!state.fullscreen) {
        glfwGetWindowPos(window, &state.windowedX, &state.windowedY);
        glfwGetWindowSize(window, &state.windowedW, &state.windowedH);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        state.fullscreen = true;
    } else {
        glfwSetWindowMonitor(window, nullptr, state.windowedX, state.windowedY, state.windowedW, state.windowedH, 0);
        state.fullscreen = false;
    }
}

} // namespace

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "failed to initialize GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(kDefaultWidth, kDefaultHeight, "Pickup / Elite-style vector renderer", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "failed to create window\n");
        glfwTerminate();
        return 1;
    }

    AppState state;
    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    VkCore core;
    Renderer renderer;
    Terrain terrain;
    Vehicle vehicle;

    try {
        core.init(window);
        state.core = &core;
        renderer.init(core);
        terrain.init(core, renderer);
        vehicle.init(core, renderer);
    } catch (const std::exception& e) {
        fprintf(stderr, "initialization failed: %s\n", e.what());
        return 1;
    }

    // Dynamic mesh for the FPS counter text ("FPS: 123" comfortably fits in 16 chars).
    constexpr size_t kMaxOverlayChars = 24;
    constexpr size_t kMaxOverlayVertsPerChar = 14; // worst case glyph (7 segments * 2 verts)
    GpuMesh overlayMesh = renderer.uploadDynamicMesh(core, sizeof(Vertex) * kMaxOverlayChars * kMaxOverlayVertsPerChar);

    double lastTime = glfwGetTime();
    double fpsAccum = 0.0;
    int fpsFrameCount = 0;
    int lastFps = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;
        dt = std::min(dt, 0.1f); // clamp huge spikes (e.g. window drag)

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        bool f11Down = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Down && !state.f11WasDown) toggleFullscreen(window, state);
        state.f11WasDown = f11Down;

        // ---- mouse-look ----
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (state.firstMouse) {
            state.lastMouseX = mx;
            state.lastMouseY = my;
            state.firstMouse = false;
        }
        double dx = mx - state.lastMouseX;
        double dy = my - state.lastMouseY;
        state.lastMouseX = mx;
        state.lastMouseY = my;
        constexpr float kMouseSensitivity = 0.0028f;
        state.orbitYaw += static_cast<float>(dx) * kMouseSensitivity;
        state.orbitPitch += static_cast<float>(-dy) * kMouseSensitivity;
        state.orbitPitch = std::clamp(state.orbitPitch, -0.9f, 1.2f);

        // ---- WASD vehicle control ----
        float throttle = 0.0f, steer = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) throttle += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) throttle -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) steer += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) steer -= 1.0f;
        vehicle.update(dt, throttle, steer);
        terrain.update(core, renderer, vehicle.position());

        // ---- camera (mouse-orbit chase cam) ----
        glm::vec3 target = vehicle.position() + glm::vec3(0.0f, 1.3f, 0.0f);
        float baseAngle = vehicle.yaw() + glm::pi<float>() + state.orbitYaw;
        float pitch = state.orbitPitch;
        constexpr float kCamDistance = 9.0f;
        glm::vec3 dir(std::sin(baseAngle) * std::cos(pitch), std::sin(pitch), std::cos(baseAngle) * std::cos(pitch));
        glm::vec3 cameraPos = target + dir * kCamDistance;
        glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0.0f, 1.0f, 0.0f));

        VkExtent2D extent = core.extent();
        float aspect = extent.height > 0 ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, Terrain::kFogDistance * 1.3f);
        proj[1][1] *= -1.0f; // Vulkan clip space has +Y down

        glm::mat4 viewProj = proj * view;

        // ---- FPS counter ----
        fpsAccum += dt;
        fpsFrameCount += 1;
        if (fpsAccum >= 0.25) {
            lastFps = static_cast<int>(static_cast<double>(fpsFrameCount) / fpsAccum + 0.5);
            fpsAccum = 0.0;
            fpsFrameCount = 0;
        }
        std::string fpsText = "FPS:" + std::to_string(lastFps);

        float pixelH = 22.0f, pixelW = 13.0f, pixelSpacing = 4.0f, marginPx = 14.0f;
        glm::vec2 glyphSize(2.0f * pixelW / static_cast<float>(extent.width), 2.0f * pixelH / static_cast<float>(extent.height));
        float spacingNdc = 2.0f * pixelSpacing / static_cast<float>(extent.width);
        float textWidthNdc = static_cast<float>(fpsText.size()) * (glyphSize.x + spacingNdc);
        glm::vec2 origin(1.0f - 2.0f * marginPx / static_cast<float>(extent.width) - textWidthNdc,
                          -1.0f + 2.0f * marginPx / static_cast<float>(extent.height));
        auto overlayVerts = vecfont::buildText(fpsText, origin, glyphSize, spacingNdc, glm::vec3(1.0f));
        renderer.updateDynamicMesh(core, overlayMesh, overlayVerts);

        // ---- render ----
        VkCommandBuffer cmd;
        uint32_t imageIndex;
        if (!core.beginFrame(cmd, imageIndex)) continue;

        renderer.beginSceneFrame(core, viewProj, cameraPos, Terrain::kFogDistance);

        terrain.draw(cmd, renderer);
        vehicle.draw(cmd, renderer);
        renderer.drawOverlay(cmd, overlayMesh);

        core.endFrame(cmd, imageIndex);
    }

    core.waitIdle();
    renderer.destroyMesh(core, overlayMesh);
    vehicle.cleanup(core, renderer);
    terrain.cleanup(core, renderer);
    renderer.cleanup(core);
    core.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
