#include "window.h"

namespace {
constexpr const char* kClassName = "pickup_elite_window_class";
}

LRESULT CALLBACK Window::wndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT Window::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            shouldClose_ = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            framebufferResized_ = true;
            if (cursorCaptured_) clipCursorToClientRect();
            return 0;
        case WM_SETCURSOR:
            if (cursorCaptured_ && LOWORD(lParam) == HTCLIENT) {
                SetCursor(nullptr);
                return TRUE;
            }
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        case WM_INPUT: {
            UINT size = 0;
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
            if (size == 0 || size > sizeof(rawInputBuffer_)) return 0;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawInputBuffer_, &size,
                                 sizeof(RAWINPUTHEADER)) != size) {
                return 0;
            }
            auto* raw = reinterpret_cast<RAWINPUT*>(rawInputBuffer_);
            if (raw->header.dwType == RIM_TYPEMOUSE && cursorCaptured_ &&
                !(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                accumDx_ += raw->data.mouse.lLastX;
                accumDy_ += raw->data.mouse.lLastY;
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam < 256) keys_[wParam] = true;
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wParam < 256) keys_[wParam] = false;
            return 0;
        case WM_SETFOCUS:
            captureCursor();
            return 0;
        case WM_KILLFOCUS:
            for (bool& down : keys_) down = false;
            releaseCursor();
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

bool Window::create(int width, int height, const char* title) {
    hinstance_ = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Window::wndProcThunk;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExA(&wc)) return false;

    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExA(0, kClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hinstance_, this);
    if (!hwnd_) return false;

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;  // Generic Desktop Controls
    rid.usUsage = 0x02;      // Mouse
    rid.dwFlags = 0;         // only deliver WM_INPUT while this window has focus
    rid.hwndTarget = hwnd_;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    captureCursor();
    return true;
}

void Window::destroy() {
    if (cursorCaptured_) releaseCursor();
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    UnregisterClassA(kClassName, hinstance_);
}

void Window::pollEvents() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            shouldClose_ = true;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void Window::waitEvents() {
    WaitMessage();
    pollEvents();
}

void Window::getFramebufferSize(int& width, int& height) const {
    RECT rect;
    GetClientRect(hwnd_, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

void Window::clipCursorToClientRect() {
    RECT rect;
    GetClientRect(hwnd_, &rect);
    POINT topLeft{rect.left, rect.top};
    POINT bottomRight{rect.right, rect.bottom};
    ClientToScreen(hwnd_, &topLeft);
    ClientToScreen(hwnd_, &bottomRight);
    RECT screenRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&screenRect);
}

void Window::captureCursor() {
    if (cursorCaptured_) return;
    cursorCaptured_ = true;
    accumDx_ = 0;
    accumDy_ = 0;
    ShowCursor(FALSE);
    clipCursorToClientRect();
}

void Window::releaseCursor() {
    if (!cursorCaptured_) return;
    cursorCaptured_ = false;
    ClipCursor(nullptr);
    ShowCursor(TRUE);
}

void Window::consumeMouseDelta(double& dx, double& dy) {
    dx = static_cast<double>(accumDx_);
    dy = static_cast<double>(accumDy_);
    accumDx_ = 0;
    accumDy_ = 0;
}

bool Window::consumeFramebufferResized() {
    bool r = framebufferResized_;
    framebufferResized_ = false;
    return r;
}

void Window::toggleFullscreen() {
    if (!fullscreen_) {
        GetWindowRect(hwnd_, &windowedRect_);
        windowedStyle_ = GetWindowLongPtrA(hwnd_, GWL_STYLE);

        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);

        SetWindowLongPtrA(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        fullscreen_ = true;
    } else {
        SetWindowLongPtrA(hwnd_, GWL_STYLE, windowedStyle_);
        SetWindowPos(hwnd_, HWND_TOP, windowedRect_.left, windowedRect_.top, windowedRect_.right - windowedRect_.left,
                     windowedRect_.bottom - windowedRect_.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        fullscreen_ = false;
    }
    if (cursorCaptured_) clipCursorToClientRect();
}
