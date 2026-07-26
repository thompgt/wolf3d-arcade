#include "game.h"

#include "../core/config.h"

namespace wolf {
namespace {

// Placeholder palette. The real one is generated alongside the procedural
// textures in a later phase.
constexpr uint32_t kCeiling  = rgb(0x38, 0x38, 0x38);
constexpr uint32_t kFloor    = rgb(0x70, 0x70, 0x70);
constexpr uint32_t kBarFace  = rgb(0x00, 0x54, 0x00);
constexpr uint32_t kBarEdge  = rgb(0x00, 0x2c, 0x00);
constexpr uint32_t kTitleBg  = rgb(0x18, 0x10, 0x10);
constexpr uint32_t kTitleFg  = rgb(0xd8, 0xa8, 0x28);

} // namespace

void Game::init() {
    state_ = GameState::Title;
    time_  = 0.0;
}

void Game::update(const Platform& in) {
    time_ += kTickDT;

    if (in.pressed(Key::Quit)) {
        // On the title screen Escape exits; in-game it will open the menu
        // once that exists.
        if (state_ == GameState::Title) quit_ = true;
        else state_ = GameState::Title;
        return;
    }

    switch (state_) {
    case GameState::Title:     updateTitle(in);   break;
    case GameState::Playing:   updatePlaying(in); break;
    case GameState::Dead:
    case GameState::LevelDone:
        if (in.pressed(Key::Start)) state_ = GameState::Title;
        break;
    }
}

void Game::updateTitle(const Platform& in) {
    if (in.pressed(Key::Start)) state_ = GameState::Playing;
}

void Game::updatePlaying(const Platform& /*in*/) {
    // Player movement, doors, enemy AI and projectiles hang off here.
}

void Game::render(Framebuffer& fb) const {
    if (state_ == GameState::Title) {
        renderTitle(fb);
        return;
    }
    renderWorld(fb);
}

void Game::renderTitle(Framebuffer& fb) const {
    fb.clear(kTitleBg);
    // Stand-in for the title art until the font and menu land: a pulsing
    // band so it is obvious at a glance that the loop is running.
    const int pulse = static_cast<int>((1.0 + 0.9) * 6);
    fb.fillRect(40, kScreenH / 2 - pulse, kScreenW - 80, pulse * 2, kTitleFg);
}

void Game::renderWorld(Framebuffer& fb) const {
    // Flat ceiling and floor across the 3D viewport. The raycaster will
    // overwrite the wall columns between them.
    fb.fillRect(0, 0, kScreenW, kViewH / 2, kCeiling);
    fb.fillRect(0, kViewH / 2, kScreenW, kViewH - kViewH / 2, kFloor);

    // Status bar shell. Contents (face, health, ammo, keys) come later.
    fb.fillRect(0, kViewH, kScreenW, kStatusBarH, kBarFace);
    fb.fillRect(0, kViewH, kScreenW, 1, kBarEdge);
}

} // namespace wolf
