#include "win32_app.h"

#include <windows.h>

#include "../core/config.h"

namespace wolf {
namespace {

bool g_quit = false;

// Raw virtual-key state, indexed by VK code. Filled by WM_KEYDOWN/UP rather
// than polled with GetAsyncKeyState so the game only sees input aimed at
// its own window.
bool g_vk[256] = {};

// Accumulated mouse motion, drained once per pump().
int  g_mouse_accum = 0;
bool g_mouse_captured = false;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY:
        g_quit = true;
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256) g_vk[wp] = true;
        // Swallow Alt so tapping it doesn't drop us into the window menu.
        if (msg == WM_SYSKEYDOWN) return 0;
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) g_vk[wp] = false;
        return 0;

    case WM_KILLFOCUS:
        // Drop every held key: without this, alt-tabbing away mid-strafe
        // leaves the player sliding into a wall forever.
        for (bool& k : g_vk) k = false;
        return 0;

    case WM_INPUT: {
        RAWINPUT ri{};
        UINT size = sizeof(ri);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri,
                            &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            ri.header.dwType == RIM_TYPEMOUSE) {
            g_mouse_accum += ri.data.mouse.lLastX;
            if (ri.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
                g_vk[VK_LBUTTON] = true;
            if (ri.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
                g_vk[VK_LBUTTON] = false;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // We overwrite every pixel each frame; skip the flicker.
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// Bitmap header describing our framebuffer to GDI. Negative height marks it
// top-down, so row 0 is the top of the screen like every other renderer.
BITMAPINFO makeBitmapInfo() {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = kScreenW;
    bi.bmiHeader.biHeight      = -kScreenH;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return bi;
}

LARGE_INTEGER g_freq{};
LARGE_INTEGER g_start{};

} // namespace

bool Platform::init(const char* title) {
    const HINSTANCE inst = GetModuleHandleA(nullptr);

    WNDCLASSA wc{};
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = "Wolf3DArcadeWindow";
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    if (!RegisterClassA(&wc)) return false;

    // Size the window so the *client area* is exactly the scaled buffer.
    RECT r{0, 0, kScreenW * kWindowScale, kScreenH * kWindowScale};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);

    const HWND hwnd = CreateWindowExA(
        0, wc.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, inst, nullptr);
    if (!hwnd) return false;
    hwnd_ = hwnd;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Raw mouse input gives us unaccelerated deltas for mouselook.
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage     = 0x02; // generic mouse
    rid.dwFlags     = 0;
    rid.hwndTarget  = hwnd;
    g_mouse_captured = RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    return true;
}

void Platform::shutdown() {
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
}

bool Platform::pump() {
    for (int i = 0; i < static_cast<int>(Key::Count); ++i) prev_[i] = down_[i];

    mouse_dx_ = 0;
    g_mouse_accum = 0;

    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    mouse_dx_ = g_mouse_accum;

    auto set = [this](Key k, bool v) { down_[static_cast<int>(k)] = v; };
    set(Key::Forward,     g_vk['W'] || g_vk[VK_UP]);
    set(Key::Back,        g_vk['S'] || g_vk[VK_DOWN]);
    set(Key::StrafeLeft,  g_vk['A']);
    set(Key::StrafeRight, g_vk['D']);
    set(Key::TurnLeft,    g_vk[VK_LEFT]);
    set(Key::TurnRight,   g_vk[VK_RIGHT]);
    set(Key::Fire,        g_vk[VK_CONTROL] || g_vk[VK_SPACE] || g_vk[VK_LBUTTON]);
    set(Key::Use,         g_vk['E']);
    set(Key::Run,         g_vk[VK_SHIFT]);
    set(Key::Weapon1,     g_vk['1']);
    set(Key::Weapon2,     g_vk['2']);
    set(Key::Weapon3,     g_vk['3']);
    set(Key::Weapon4,     g_vk['4']);
    set(Key::Dash,        g_vk['Q']);
    set(Key::Grenade,     g_vk['G']);
    set(Key::Slowmo,      g_vk['F']);
    set(Key::Start,       g_vk[VK_RETURN]);
    set(Key::Quit,        g_vk[VK_ESCAPE]);

    return !g_quit;
}

void Platform::present(const Framebuffer& fb) {
    const HWND hwnd = static_cast<HWND>(hwnd_);
    if (!hwnd) return;

    RECT client{};
    GetClientRect(hwnd, &client);

    const HDC dc = GetDC(hwnd);
    // Nearest-neighbour upscale keeps the pixels hard-edged; any smoothing
    // mode here would blur the whole point of a 320x200 renderer.
    SetStretchBltMode(dc, COLORONCOLOR);
    const BITMAPINFO bi = makeBitmapInfo();
    StretchDIBits(dc,
                  0, 0, client.right, client.bottom,
                  0, 0, kScreenW, kScreenH,
                  fb.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, dc);
}

double Platform::now() const {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart - g_start.QuadPart) /
           static_cast<double>(g_freq.QuadPart);
}

} // namespace wolf
