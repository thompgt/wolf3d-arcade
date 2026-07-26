// The status bar, the face portrait, and the full-screen state screens.
//
// Everything here takes a plain `HudState` rather than the game objects it
// came from. That is what keeps render/ from making game decisions: this
// file can tell you that health is 12, and it cannot ask whether you are
// about to die.
//
// The face is the one piece of a Wolf3D status bar that is not a number. It
// is also the only place the game acknowledges you at all, so it gets a
// health tier, an idle glance left and right, a grimace when hurt and a
// gloat on a kill streak.
#pragma once

#include <array>
#include <cstdint>

#include "../core/framebuffer.h"
#include "sprite.h"
#include "weapon_sprites.h"

namespace wolf {

// What the bar draws. Assembled by the game each frame; nothing here is
// derived from world state on this side of the boundary.
struct HudState {
    int  floor   = 1;
    int  score   = 0;
    int  lives   = 3;
    int  health  = 100;
    int  ammo    = 8;
    bool goldKey   = false;
    bool silverKey = false;
    WeaponType weapon = WeaponType::Pistol;

    // Which way the portrait is looking, and what it is doing. Driven by the
    // game so the idle glance keeps time with the sim rather than the frame
    // rate.
    int  faceLook = 1;      // 0 left, 1 ahead, 2 right
    bool grimace  = false;  // took damage recently
    bool gloat    = false;  // killed something recently
};

class FaceSprites {
public:
    // Five health tiers, three glances each, plus the grimace, the gloat and
    // the dead face.
    static constexpr int kTiers = 5;
    static constexpr int kLooks = 3;
    static constexpr int kFaceW = 24;
    static constexpr int kFaceH = 32;

    void generate();

    // Picks the frame for a given health and expression. Out-of-range health
    // is clamped rather than asserted.
    const Sprite& pick(int health, int look, bool grimace, bool gloat) const;

private:
    std::array<Sprite, kTiers * kLooks> idle_{};
    std::array<Sprite, kTiers> hurt_{};
    Sprite gloat_{};
    Sprite dead_{};
};

void renderStatusBar(Framebuffer& fb, const FaceSprites& faces,
                     const HudState& hud);

// Title, death and level-complete screens. Each takes the time it has been
// on screen so it can animate without owning a clock of its own.
void renderTitleScreen(Framebuffer& fb, double time);
void renderDeathScreen(Framebuffer& fb, double time);

// The end-of-level tally. Percentages are passed in already computed: what
// counts as a secret is a game rule, not a rendering one.
void renderLevelDone(Framebuffer& fb, double time, int floor, int score,
                     int killPct, int treasurePct, int secretPct);

} // namespace wolf
