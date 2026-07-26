// Top-level game object: owns the world, steps the simulation on a fixed
// tick, and draws one frame. Everything the main loop needs is here.
//
// Scaffold status: the state machine and frame skeleton are in place; the
// world simulation and 3D renderer land in later phases (see WORKPLAN.md).
#pragma once

#include "../core/framebuffer.h"
#include "../platform/win32_app.h"

namespace wolf {

enum class GameState {
    Title,      // attract screen, waiting on Enter
    Playing,
    Dead,       // death melt, then back to Title
    LevelDone,  // exit switch hit, tally screen
};

class Game {
public:
    void init();

    // Advances the world by exactly one fixed tick (kTickDT seconds).
    void update(const Platform& in);

    // Renders the current state into fb. Never mutates game state, so it can
    // be skipped freely when we're behind on ticks.
    void render(Framebuffer& fb) const;

    bool wantsQuit() const { return quit_; }

private:
    void updateTitle(const Platform& in);
    void updatePlaying(const Platform& in);

    void renderTitle(Framebuffer& fb) const;
    void renderWorld(Framebuffer& fb) const;

    GameState state_ = GameState::Title;
    double    time_  = 0.0;   // seconds of simulated time
    bool      quit_  = false;
};

} // namespace wolf
