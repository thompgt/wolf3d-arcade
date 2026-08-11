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
    Minimap,     // debug: top-down view of the level
    TexAtlas,    // debug: show the generated textures flat
    Screenshot,  // debug: dump the framebuffer to shot.bmp
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

    // Held right now.
    bool down(Key k) const { return down_[static_cast<int>(k)]; }

    // A fresh press that no game tick has acted on yet.
    //
    // This is deliberately *sticky* rather than a plain "down and not down
    // last frame". Frames and ticks run at different rates: at 200fps
    // against a 60Hz tick, most frames run no tick at all, and an edge
    // recomputed every pump() would be created and destroyed between ticks
    // without update() ever seeing it. Latching the press until a tick
    // consumes it is what makes every keystroke count.
    bool pressed(Key k) const { return edge_[static_cast<int>(k)]; }

    // Clears the latched presses and the accumulated mouse delta. Called by
    // the loop after each tick, so one keystroke produces exactly one
    // pressed() — never zero (dropped between ticks) and never two (a
    // catch-up frame running several ticks) — and every pixel of mouse motion
    // is applied exactly once.
    void consumeEdges();

    // Horizontal mouse motion, in pixels, accumulated since the last tick
    // consumed it — not since the last pump(). At 200fps against a 60Hz tick
    // most pumps run no tick, so a per-pump delta would be mostly discarded
    // and the surviving one re-applied on every tick of a catch-up frame.
    int mouseDX() const { return mouse_dx_; }

    // Seconds since init(), from a high-resolution timer.
    double now() const;

    // Puts a message in front of the user. The release build is linked
    // -mwindows and has no console, so anything printed to stderr goes
    // nowhere — a failure the player needs to know about has to be shown.
    static void showError(const char* title, const char* message);

    // Hands the rest of the millisecond back to the OS. init() raises the
    // scheduler's timer resolution to 1ms, so this is accurate enough to pace
    // a 60Hz loop instead of spinning a core on redundant frames.
    void sleepMs(int ms) const;

private:
    void* hwnd_ = nullptr;   // HWND, kept opaque to avoid leaking windows.h
    bool  down_[static_cast<int>(Key::Count)] = {};  // held now
    bool  edge_[static_cast<int>(Key::Count)] = {};  // press awaiting a tick
    bool  last_[static_cast<int>(Key::Count)] = {};  // previous pump's down_
    int   mouse_dx_ = 0;
};

} // namespace wolf
