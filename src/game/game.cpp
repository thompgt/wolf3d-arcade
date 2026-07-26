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

// Weapon sway frequency, radians per second. Tuned against the walk speed
// so one sway matches roughly one stride.
constexpr double kBobRate = 7.5;

// Minimap: pixels per tile, and where it sits on screen.
constexpr int kMiniScale = 3;
constexpr int kMiniX     = 4;
constexpr int kMiniY     = 4;

constexpr uint32_t kMiniFloor  = rgb(0x20, 0x20, 0x20);
constexpr uint32_t kMiniWall   = rgb(0xb0, 0xb0, 0xb0);
constexpr uint32_t kMiniDoor   = rgb(0xc8, 0xa8, 0x38);
constexpr uint32_t kMiniPlayer = rgb(0x30, 0xf0, 0x30);
constexpr uint32_t kMiniRay    = rgb(0xf0, 0x40, 0x40);

// Enemy states are colour-coded on the minimap. Watching guards change
// colour as they notice you is the only practical way to tell a working
// state machine from one that merely looks busy.
constexpr uint32_t kMiniIdle    = rgb(0xc0, 0x80, 0x20);  // standing/patrolling
constexpr uint32_t kMiniHunting = rgb(0xf0, 0x40, 0x40);  // chasing
constexpr uint32_t kMiniFiring  = rgb(0xff, 0xf0, 0x60);  // shooting
constexpr uint32_t kMiniCorpse  = rgb(0x60, 0x28, 0x28);

} // namespace

void Game::init() {
    textures_.generate();
    sprites_.generate();
    actors_.generate();
    weapon_art_.generate();
    map_   = Map::level1();
    state_ = GameState::Title;
    time_  = 0.0;
}

void Game::startLevel() {
    // Rebuilt rather than reused: the level carries mutable state now
    // (open doors, moved secrets, collected pickups), so restarting has to
    // start from a fresh copy or the second run begins half-looted.
    map_ = Map::level1();
    items_.spawnFrom(map_);
    enemies_.spawnFrom(map_);
    player_ = Player();
    weapons_.reset();
    bob_ = 0.0;
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
    if (in.pressed(Key::TexAtlas))
        atlas_mode_ = (atlas_mode_ + 1) % kAtlasModes;

    player_.update(in, map_, items_, enemies_, kTickDT);

    // Ownership before the sweep, so a weapon picked up this tick can be put
    // in the player's hands rather than waiting for them to press a number.
    const bool hadMachineGun = player_.hasMachineGun();
    const bool hadChaingun   = player_.hasChaingun();
    items_.collect(player_);
    if (!hadChaingun && player_.hasChaingun())
        weapons_.select(WeaponType::Chaingun, player_);
    else if (!hadMachineGun && player_.hasMachineGun())
        weapons_.select(WeaponType::MachineGun, player_);

    // The walk cycle only advances while walking; a weapon that swayed on
    // after the player stopped would look like it was floating.
    if (player_.isMoving()) bob_ += kBobRate * kTickDT;

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

    // Weapons before the enemies, so a guard killed this tick does not also
    // get to take his shot.
    updateWeapons(in);

    enemies_.update(kTickDT, map_, player_, items_);

    if (player_.isDead()) {
        state_ = GameState::Dead;
        time_  = 0.0;
    }
}

void Game::updateWeapons(const Platform& in) {
    // Selection is a table lookup rather than four branches, so adding a
    // fifth weapon is a row here and a row in the tuning table.
    static const struct { Key key; WeaponType weapon; } kBindings[] = {
        {Key::Weapon1, WeaponType::Knife},
        {Key::Weapon2, WeaponType::Pistol},
        {Key::Weapon3, WeaponType::MachineGun},
        {Key::Weapon4, WeaponType::Chaingun},
    };
    for (const auto& b : kBindings)
        if (in.pressed(b.key)) weapons_.select(b.weapon, player_);

    // down(), not pressed(): the automatic weapons need the held state, and
    // Weapons itself works out the edge for the ones that fire on a press.
    weapons_.update(kTickDT, in.down(Key::Fire), map_, player_, enemies_);
}

void Game::render(Framebuffer& fb) {
    if (state_ == GameState::Title) {
        renderTitle(fb);
        return;
    }

    caster_.render(fb, map_, player_, textures_);

    // Sprites go after the walls, because they clip against the depth the
    // wall pass just recorded.
    billboards_.clear();
    items_.appendBillboards(billboards_, sprites_);
    enemies_.appendBillboards(billboards_, player_, actors_);
    renderBillboards(fb, player_, caster_.depth(), billboards_);

    // The flash lights the world, so it goes on before the weapon is drawn —
    // washing the gun itself would just make it disappear at the moment the
    // player is looking hardest at it.
    applyMuzzleFlash(fb, weapons_.flash());
    renderWeapon(fb, weapon_art_.frame(weapons_.current(), weapons_.frame()),
                 bob_);

    renderStatusBar(fb);
    if (show_minimap_) renderMinimap(fb);
    if (atlas_mode_)   renderTexAtlas(fb);
}

namespace {

constexpr int kAtlasCols = 4;
constexpr int kAtlasPad  = 2;

int atlasOriginX() {
    return (kScreenW - (kAtlasCols * (kTexSize + kAtlasPad) - kAtlasPad)) / 2;
}

// Sprites are mostly empty, so a flat backdrop is drawn behind them —
// against the 3D view there is no telling a transparent pixel from a dark
// one, which is exactly the confusion the atlas exists to remove.
void blitSpriteCell(Framebuffer& fb, const Sprite& s, int ox, int oy) {
    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const uint32_t c = s.at(x, y);
            fb.put(ox + x, oy + y,
                   isTransparent(c) ? rgb(0x30, 0x30, 0x38) : c);
        }
    }
}

} // namespace

void Game::renderTexAtlas(Framebuffer& fb) const {
    const int originX = atlasOriginX();
    const int originY = 4;

    if (atlas_mode_ == 1) {
        for (int i = 0; i < static_cast<int>(TexCount); ++i) {
            const int ox = originX + (i % kAtlasCols) * (kTexSize + kAtlasPad);
            const int oy = originY + (i / kAtlasCols) * (kTexSize + kAtlasPad);
            for (int y = 0; y < kTexSize; ++y)
                for (int x = 0; x < kTexSize; ++x)
                    fb.put(ox + x, oy + y, textures_[i].at(x, y));
        }
        return;
    }

    // Assemble the page as a list of frames, so each mode is a description
    // of what to show rather than its own nested loop.
    std::vector<const Sprite*> page;
    const auto guard = EnemyType::Guard;

    switch (atlas_mode_) {
    case 2:
        for (int i = 0; i < static_cast<int>(SprCount); ++i)
            page.push_back(&sprites_[i]);
        break;

    case 3:   // all eight facings, which is the gameplay-relevant set
        for (int a = 0; a < ActorSprites::kAngles; ++a)
            page.push_back(&actors_.frame(guard, ActorPose::Stand, a, 0));
        break;

    case 4:   // walk cycle face-on, walk cycle in profile, then firing
        for (int f = 0; f < ActorSprites::kWalkFrames; ++f)
            page.push_back(&actors_.frame(guard, ActorPose::Walk, 0, f));
        for (int f = 0; f < ActorSprites::kWalkFrames; ++f)
            page.push_back(&actors_.frame(guard, ActorPose::Walk, 2, f));
        for (int f = 0; f < ActorSprites::kShootFrames; ++f)
            page.push_back(&actors_.frame(guard, ActorPose::Shoot, 0, f));
        break;

    case 5:   // pain, the death sequence, then the SS for comparison
        page.push_back(&actors_.frame(guard, ActorPose::Pain, 0, 0));
        for (int f = 0; f < ActorSprites::kDieFrames; ++f)
            page.push_back(&actors_.frame(guard, ActorPose::Die, 0, f));
        for (int a = 0; a < ActorSprites::kAngles; a += 2)
            page.push_back(&actors_.frame(EnemyType::SS, ActorPose::Stand, a, 0));
        page.push_back(&actors_.frame(EnemyType::SS, ActorPose::Shoot, 0, 1));
        break;

    // The weapons, two per page. All four at once would be sixteen cells,
    // and only three rows of four fit in 200 rows — the fourth weapon would
    // be silently off the bottom of the screen, which is exactly the kind of
    // thing a debug view exists to not do.
    case 6:
    case 7: {
        const int first = (atlas_mode_ == 6) ? 0 : 2;
        for (int w = first; w < first + 2; ++w)
            for (int f = 0; f < WeaponSprites::kFrames; ++f)
                page.push_back(&weapon_art_.frame(static_cast<WeaponType>(w), f));
        break;
    }

    default:
        return;
    }

    for (size_t i = 0; i < page.size(); ++i) {
        const int ox = originX + static_cast<int>(i % kAtlasCols) * (kTexSize + kAtlasPad);
        const int oy = originY + static_cast<int>(i / kAtlasCols) * (kTexSize + kAtlasPad);
        blitSpriteCell(fb, *page[i], ox, oy);
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

    for (const Enemy& e : enemies_.all()) {
        uint32_t c = kMiniIdle;
        switch (e.state) {
        case EnemyState::Chase: c = kMiniHunting; break;
        case EnemyState::Shoot: c = kMiniFiring;  break;
        case EnemyState::Die:
        case EnemyState::Dead:  c = kMiniCorpse;  break;
        default: break;
        }
        fb.fillRect(static_cast<int>(kMiniX + e.x * kMiniScale) - 1,
                    static_cast<int>(kMiniY + e.y * kMiniScale) - 1,
                    3, 3, c);
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
