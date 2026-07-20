#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class Window {
public:
    bool create(int width, int height, const char* title);
    void destroy();

    bool shouldClose() const { return shouldClose_; }
    void requestClose() { shouldClose_ = true; }

    void pollEvents();
    void waitEvents();

    void getFramebufferSize(int& width, int& height) const;

    // Relative mouse-look delta accumulated from WM_INPUT since the last
    // call; resets to 0 afterward. Raw input, not GetCursorPos diffing -
    // see CLAUDE.md for why.
    void consumeMouseDelta(double& dx, double& dy);

    bool isKeyDown(int vkCode) const { return vkCode >= 0 && vkCode < 256 && keys_[vkCode]; }

    void toggleFullscreen();

    bool consumeFramebufferResized();

    HWND hwnd() const { return hwnd_; }
    HINSTANCE hinstance() const { return hinstance_; }

private:
    static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void clipCursorToClientRect();
    void captureCursor();
    void releaseCursor();

    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    bool shouldClose_ = false;
    bool framebufferResized_ = false;
    bool keys_[256] = {};

    bool fullscreen_ = false;
    RECT windowedRect_{};
    LONG_PTR windowedStyle_ = 0;

    bool cursorCaptured_ = false;
    LONG accumDx_ = 0;
    LONG accumDy_ = 0;
    BYTE rawInputBuffer_[64] = {};
};
