#include "game.h"

#include <cmath>

#include "../core/config.h"

namespace wolf {
namespace {

// Placeholder palette. The real one is generated alongside the procedural
// textures in a later phase.
constexpr uint32_t kBarFace  = rgb(0x00, 0x54, 0x00);
constexpr uint32_t kBarEdge  = rgb(0x00, 0x2c, 0x00);
constexpr uint32_t kTitleBg  = rgb(0x18, 0x10, 0x10);
constexpr uint32_t kTitleFg  = rgb(0xd8, 0xa8, 0x28);

// Minimap: pixels per tile, and where it sits on screen.
constexpr int kMiniScale = 3;
constexpr int kMiniX     = 4;
constexpr int kMiniY     = 4;

constexpr uint32_t kMiniFloor  = rgb(0x20, 0x20, 0x20);
constexpr uint32_t kMiniWall   = rgb(0xb0, 0xb0, 0xb0);
constexpr uint32_t kMiniDoor   = rgb(0xc8, 0xa8, 0x38);
constexpr uint32_t kMiniPlayer = rgb(0x30, 0xf0, 0x30);
constexpr uint32_t kMiniRay    = rgb(0xf0, 0x40, 0x40);

} // namespace

void Game::init() {
    textures_.generate();
    map_   = Map::level1();
    state_ = GameState::Title;
    time_  = 0.0;
}

void Game::startLevel() {
    // Face east into the cell block corridor rather than at the wall behind
    // the spawn point.
    player_.spawn(map_.startX(), map_.startY(), 0.0);
    state_ = GameState::Playing;
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
    if (in.pressed(Key::Start)) startLevel();
}

void Game::updatePlaying(const Platform& in) {
    if (in.pressed(Key::Minimap))  show_minimap_ = !show_minimap_;
    if (in.pressed(Key::TexAtlas)) show_atlas_   = !show_atlas_;

    player_.update(in, map_, kTickDT);

    if (in.pressed(Key::Use)) {
        const UseResult r = map_.use(player_.x(), player_.y(),
                                     player_.dirX(), player_.dirY(),
                                     player_.hasGoldKey(),
                                     player_.hasSilverKey());
        if (r == UseResult::ExitReached) state_ = GameState::LevelDone;
        // A locked door is worth telling the player about; the HUD message
        // that says so arrives with the status bar in phase 7.
        use_result_ = r;
    }

    // Animated after the player has moved, so a door never closes into the
    // cell they just stepped into.
    map_.update(kTickDT, player_.x(), player_.y());

    // Enemy AI and projectiles hang off here.
}

void Game::render(Framebuffer& fb) {
    if (state_ == GameState::Title) {
        renderTitle(fb);
        return;
    }

    caster_.render(fb, map_, player_, textures_);
    renderStatusBar(fb);
    if (show_minimap_) renderMinimap(fb);
    if (show_atlas_)   renderTexAtlas(fb);
}

void Game::renderTexAtlas(Framebuffer& fb) const {
    // Four across, two down: the whole set at 1:1 with room to spare.
    constexpr int kCols = 4;
    constexpr int kPad  = 2;
    const int originX = (kScreenW - (kCols * (kTexSize + kPad) - kPad)) / 2;
    const int originY = 8;

    for (int i = 0; i < TexCount; ++i) {
        const int ox = originX + (i % kCols) * (kTexSize + kPad);
        const int oy = originY + (i / kCols) * (kTexSize + kPad);
        for (int y = 0; y < kTexSize; ++y)
            for (int x = 0; x < kTexSize; ++x)
                fb.put(ox + x, oy + y, textures_[i].at(x, y));
    }
}

void Game::renderTitle(Framebuffer& fb) const {
    fb.clear(kTitleBg);
    // Stand-in for the title art until the font and menu land: a pulsing
    // band so it is obvious at a glance that the loop is running.
    const int pulse = 4 + static_cast<int>(3.0 * (1.0 + std::sin(time_ * 3.0)));
    fb.fillRect(40, kScreenH / 2 - pulse, kScreenW - 80, pulse * 2, kTitleFg);
}

void Game::renderStatusBar(Framebuffer& fb) const {
    // Shell only; contents (face, health, ammo, keys) come later.
    fb.fillRect(0, kViewH, kScreenW, kStatusBarH, kBarFace);
    fb.fillRect(0, kViewH, kScreenW, 1, kBarEdge);
}

void Game::renderMinimap(Framebuffer& fb) const {
    for (int ty = 0; ty < map_.height(); ++ty) {
        for (int tx = 0; tx < map_.width(); ++tx) {
            uint32_t c;
            if (map_.isDoor(tx, ty))        c = kMiniDoor;
            else if (map_.isSolid(tx, ty))  c = kMiniWall;
            else                            c = kMiniFloor;
            fb.fillRect(kMiniX + tx * kMiniScale, kMiniY + ty * kMiniScale,
                        kMiniScale, kMiniScale, c);
        }
    }

    const double pxf = kMiniX + player_.x() * kMiniScale;
    const double pyf = kMiniY + player_.y() * kMiniScale;

    // A short facing whisker, so it is obvious which way the camera looks.
    for (int i = 0; i < 8; ++i) {
        fb.put(static_cast<int>(pxf + player_.dirX() * i),
               static_cast<int>(pyf + player_.dirY() * i), kMiniRay);
    }
    fb.fillRect(static_cast<int>(pxf) - 1, static_cast<int>(pyf) - 1,
                3, 3, kMiniPlayer);
}

} // namespace wolf
