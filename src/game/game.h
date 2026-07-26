// Top-level game object: owns the world, steps the simulation on a fixed
// tick, and draws one frame. Everything the main loop needs is here.
#pragma once

#include "../core/framebuffer.h"
#include "../platform/win32_app.h"
#include <vector>

#include "../render/raycast.h"
#include "../render/sprite_set.h"
#include "../render/sprites.h"
#include "../render/textures.h"
#include "items.h"
#include "map.h"
#include "player.h"

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

    // Renders the current state into fb. Render state (the depth buffer)
    // lives in the raycaster, so this is non-const, but it never touches
    // simulation state — frames can be skipped freely.
    void render(Framebuffer& fb);

    bool wantsQuit() const { return quit_; }

private:
    void startLevel();
    void updateTitle(const Platform& in);
    void updatePlaying(const Platform& in);

    void renderTitle(Framebuffer& fb) const;
    void renderStatusBar(Framebuffer& fb) const;
    // Debug aid: top-down view of the level with the player's facing.
    void renderMinimap(Framebuffer& fb) const;
    // Debug aid: the generated textures laid out flat, which is the only
    // way to judge procedural art without perspective and shading in the way.
    void renderTexAtlas(Framebuffer& fb) const;

    GameState state_ = GameState::Title;
    double    time_  = 0.0;   // seconds of simulated time
    bool      quit_  = false;
    bool      show_minimap_ = false;
    // 0 off, 1 wall textures, 2 object sprites. Cycled by T; both sets are
    // too tall to show at once at 320x200.
    int       atlas_mode_   = 0;
    UseResult use_result_   = UseResult::Nothing;

    Map        map_;
    Player     player_;
    Items      items_;
    Raycaster  caster_;
    TextureSet textures_;
    SpriteSet  sprites_;

    // Rebuilt every frame from whatever is currently in the world. Kept as
    // a member so the per-frame sort reuses this allocation instead of
    // making a new one sixty times a second.
    std::vector<Billboard> billboards_;
};

} // namespace wolf
