#include "win32_app.h"

#include <windows.h>
#include <mmsystem.h>

#include "../core/config.h"

namespace wolf {
namespace {

bool g_quit = false;

// Raw virtual-key state, indexed by VK code. Filled by WM_KEYDOWN/UP rather
// than polled with GetAsyncKeyState so the game only sees input aimed at
// its own window.
bool g_vk[256] = {};

// Sticky "was pressed at least once since the last pump()" flags. Without
// these, a tap that both starts and ends inside a single frame is invisible:
// the message loop processes keydown then keyup, leaving g_vk false by the
// time the game samples it, and the input is silently dropped. Latching the
// press guarantees every keystroke is seen for exactly one frame.
bool g_vk_hit[256] = {};

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
        if (wp < 256) {
            g_vk[wp] = true;
            g_vk_hit[wp] = true;
        }
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
        for (bool& k : g_vk_hit) k = false;
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
bool g_timer_period = false;

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

    // Without this, Sleep(1) can overshoot by up to a full scheduler quantum
    // (~15ms), which is most of a frame — the pacing in main() depends on it.
    timeBeginPeriod(1);
    g_timer_period = true;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    return true;
}

void Platform::shutdown() {
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
    if (g_timer_period) {
        timeEndPeriod(1);
        g_timer_period = false;
    }
}

bool Platform::pump() {
    g_mouse_accum = 0;

    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    // Accumulated, not assigned. Frames and ticks run at different rates, so
    // overwriting here would throw away the motion from every pump that ran
    // no tick, and replay the surviving one on each tick of a catch-up frame.
    // consumeEdges() drains it, exactly as it does the keyboard edges.
    mouse_dx_ += g_mouse_accum;

    // A key counts as down if it is still held, or if it was tapped at any
    // point since the last pump. vk() folds those two together so a fast
    // tap survives for exactly one frame and still produces a pressed() edge.
    auto vk = [](int code) { return g_vk[code] || g_vk_hit[code]; };

    bool now[static_cast<int>(Key::Count)] = {};
    auto set = [&now](Key k, bool v) { now[static_cast<int>(k)] = v; };
    set(Key::Forward,     vk('W') || vk(VK_UP));
    set(Key::Back,        vk('S') || vk(VK_DOWN));
    set(Key::StrafeLeft,  vk('A'));
    set(Key::StrafeRight, vk('D'));
    set(Key::TurnLeft,    vk(VK_LEFT));
    set(Key::TurnRight,   vk(VK_RIGHT));
    set(Key::Fire,        vk(VK_CONTROL) || vk(VK_SPACE) || vk(VK_LBUTTON));
    set(Key::Use,         vk('E'));
    set(Key::Run,         vk(VK_SHIFT));
    set(Key::Weapon1,     vk('1'));
    set(Key::Weapon2,     vk('2'));
    set(Key::Weapon3,     vk('3'));
    set(Key::Weapon4,     vk('4'));
    set(Key::Dash,        vk('Q'));
    set(Key::Grenade,     vk('G'));
    set(Key::Slowmo,      vk('F'));
    set(Key::Start,       vk(VK_RETURN));
    set(Key::Quit,        vk(VK_ESCAPE));
    set(Key::Minimap,     vk('M'));
    set(Key::TexAtlas,    vk('T'));
    set(Key::Screenshot,  vk(VK_F12));

    // Latch a rising edge for anything newly pressed. edge_ is only cleared
    // by consumeEdges(), so a press made between two ticks survives until a
    // tick actually runs.
    for (int i = 0; i < static_cast<int>(Key::Count); ++i) {
        if (now[i] && !last_[i]) edge_[i] = true;
        last_[i] = now[i];
        down_[i] = now[i];
    }

    // Consumed for this frame; a still-held key stays true via g_vk.
    for (bool& k : g_vk_hit) k = false;

    return !g_quit;
}

void Platform::consumeEdges() {
    for (bool& e : edge_) e = false;
    mouse_dx_ = 0;
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

std::string exeRelativePath(const char* filename) {
    char buf[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    // Nothing useful to fall back on but the working directory, which is the
    // behaviour this function exists to replace -- but a broken path is worse
    // than an awkward one.
    if (n == 0 || n >= MAX_PATH) return filename;

    std::string path(buf, n);
    const size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return filename;

    path.resize(slash + 1);
    path += filename;
    return path;
}

void Platform::showError(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
}

void Platform::sleepMs(int ms) const {
    if (ms > 0) Sleep(static_cast<DWORD>(ms));
}

double Platform::now() const {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart - g_start.QuadPart) /
           static_cast<double>(g_freq.QuadPart);
}

} // namespace wolf
