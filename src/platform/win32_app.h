// The only Win32-aware part of the codebase. Everything above this line
// talks to the game through a plain keyboard/mouse snapshot and a
// "present this framebuffer" call, so the engine itself stays portable.
#pragma once

#include <cstdint>

#include "../core/framebuffer.h"

namespace wolf {

// Logical actions the game reads, decoupled from physical scancodes so key
// bindings can be remapped in one place.
enum class Key {
    Forward, Back, StrafeLeft, StrafeRight,
    TurnLeft, TurnRight,
    Fire, Use, Run,
    Weapon1, Weapon2, Weapon3, Weapon4,
    Dash, Grenade, Slowmo,
    Start, Quit,
    Count
};

class Platform {
public:
    bool init(const char* title);
    void shutdown();

    // Drains the Win32 message queue and refreshes the input snapshot.
    // Returns false once the user has closed the window.
    bool pump();

    // Scales the framebuffer up and blits it to the client area.
    void present(const Framebuffer& fb);

    // Held this frame.
    bool down(Key k) const { return down_[static_cast<int>(k)]; }
    // Held this frame but not the previous one: for one-shot actions.
    bool pressed(Key k) const {
        const int i = static_cast<int>(k);
        return down_[i] && !prev_[i];
    }

    // Horizontal mouse delta in pixels since the last pump(), for mouselook.
    int mouseDX() const { return mouse_dx_; }

    // Seconds since init(), from a high-resolution timer.
    double now() const;

private:
    void* hwnd_ = nullptr;   // HWND, kept opaque to avoid leaking windows.h
    bool  down_[static_cast<int>(Key::Count)] = {};
    bool  prev_[static_cast<int>(Key::Count)] = {};
    int   mouse_dx_ = 0;
};

} // namespace wolf
