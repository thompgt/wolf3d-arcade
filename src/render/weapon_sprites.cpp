#include "weapon_sprites.h"

#include <algorithm>
#include <cmath>

namespace wolf {
namespace {

// Palette. Kept together so the whole arsenal can be recoloured at once and
// so no drawing routine invents a shade of its own.
constexpr uint32_t kGlove     = rgb(0x8c, 0x6a, 0x44);
constexpr uint32_t kGloveLit  = rgb(0xb4, 0x8c, 0x5c);
constexpr uint32_t kGloveDark = rgb(0x5c, 0x44, 0x2c);
constexpr uint32_t kSteel     = rgb(0x70, 0x74, 0x7c);
constexpr uint32_t kSteelLit  = rgb(0xa8, 0xac, 0xb4);
constexpr uint32_t kSteelDark = rgb(0x3c, 0x3e, 0x44);
constexpr uint32_t kBlued     = rgb(0x30, 0x32, 0x38);
constexpr uint32_t kWood      = rgb(0x6c, 0x44, 0x24);
constexpr uint32_t kBrass     = rgb(0xc8, 0x9c, 0x38);
constexpr uint32_t kFlashHot  = rgb(0xff, 0xf4, 0xc0);
constexpr uint32_t kFlashMid  = rgb(0xff, 0xc0, 0x40);
constexpr uint32_t kOutline   = rgb(0x14, 0x12, 0x10);

// The 64x64 frame's floor. Everything rests on this line, so all four
// weapons share a horizon and none of them appears to float.
constexpr int kBase = 63;

// A muzzle flash: a hot core with a ragged corona, drawn at the barrel.
// Star-shaped rather than a disc — a disc reads as a bubble, and the spikes
// are what make it read as an explosion at eight pixels across.
void drawFlash(Sprite& s, double cx, double cy, double scale) {
    // Kept inside the frame rather than allowed to run off the top. On the
    // recoil frame the muzzle is near the top of the sprite, and an
    // unclamped flash gets sliced flat by the frame edge — which reads as a
    // rendering bug, where a flash sitting a few pixels low reads as
    // nothing at all at the speed these cycle.
    const double reach = 11.0 * scale + 1.0;
    cx = std::clamp(cx, reach, kTexSize - reach);
    cy = std::max(cy, reach);

    sprEllipse(s, cx, cy, 7.0 * scale, 5.5 * scale, kFlashMid);
    sprEllipse(s, cx, cy, 4.0 * scale, 3.0 * scale, kFlashHot);
    for (int i = 0; i < 8; ++i) {
        const double a = i * 0.7853981634;
        const double len = ((i & 1) ? 11.0 : 8.0) * scale;
        for (double t = 0.0; t < len; t += 0.5) {
            s.set(static_cast<int>(cx + std::cos(a) * t),
                  static_cast<int>(cy + std::sin(a) * t),
                  (t < len * 0.5) ? kFlashHot : kFlashMid);
        }
    }
}

// The gloved fist that holds everything. Drawn once here so all four weapons
// are visibly held by the same person.
//
// The wrist below it is deliberately narrower than the fist rather than a
// full-width block down to the frame edge. A block reads as a slab of brown
// with a stick coming out of it, and it swallows whatever it is supposed to
// be holding — the weapon has to stay the thing you are looking at.
void drawFist(Sprite& s, int cx, int cy, int w, int h) {
    const double hw = w / 2.0;

    sprBox(s, cx - static_cast<int>(hw * 0.62), cy,
           cx + static_cast<int>(hw * 0.62), kBase, kGlove);   // wrist
    sprEllipse(s, cx, cy, hw, h / 2.0, kGlove);                // palm

    // Knuckles, as four lit bumps across the top. Without them the hand is
    // a mitten.
    for (int i = 0; i < 4; ++i)
        sprEllipse(s, cx - hw * 0.62 + i * (hw * 0.42), cy - h * 0.22,
                   hw * 0.17, 2.2, kGloveLit);

    // Shaded down the right, away from the light every other sprite uses.
    sprEllipse(s, cx + hw * 0.72, cy + 1.0, hw * 0.28, h / 2.6, kGloveDark);
}

// --- knife ---------------------------------------------------------------
//
// Held to the right and thrust diagonally, so the blade crosses the centre
// of the screen on the strike rather than stabbing the bottom-right corner.

void drawKnife(Sprite& s, int frame) {
    // Thrust: out and up, then back. Frame 0 is the resting pose.
    const int lift  = (frame == 0) ? 0 : (frame == 2 ? 22 : (frame == 1 ? 13 : 6));
    const int reach = (frame == 0) ? 0 : (frame == 2 ? 12 : (frame == 1 ?  7 : 3));

    const int handX = 45 - reach;
    const int handY = 50 - lift;

    // Blade: a taper drawn as narrowing rows, angled in toward the centre of
    // the screen so the strike crosses the crosshair rather than jabbing the
    // corner. Lit down one edge, dark down the other — a flat grey taper
    // reads as a plank.
    const int tipY = handY - 34;
    const int hilt = handY - 6;
    for (int y = tipY; y < hilt; ++y) {
        const double t = static_cast<double>(y - tipY) / (hilt - tipY);
        const int halfW = 1 + static_cast<int>(t * 4.5);
        const int bx = handX - static_cast<int>((1.0 - t) * 9.0);
        sprBox(s, bx - halfW, y, bx + halfW, y + 1, kSteel);
        s.set(bx - halfW, y, kSteelLit);          // ground edge catches light
        s.set(bx + halfW, y, kSteelDark);
    }

    sprBox(s, handX - 10, hilt, handX + 8, hilt + 4, kBrass);        // guard
    sprBox(s, handX - 10, hilt, handX + 8, hilt + 1, rgb(0xe8, 0xc0, 0x50));
    drawFist(s, handX, handY + 4, 24, 19);
}

// --- pistol --------------------------------------------------------------

void drawPistol(Sprite& s, int frame) {
    // Recoil: the whole weapon kicks up and settles.
    const int kick = (frame == 0) ? 0 : (frame == 1 ? 9 : (frame == 2 ? 5 : 2));
    const int cx = 31;
    const int y  = 44 - kick;

    // Grip first, then the fist over it: the hand has to be in front of the
    // grip and behind everything above it, which is the whole reason the
    // pistol is drawn bottom-up rather than in one pass.
    sprBox(s, cx - 1, y, cx + 11, y + 22, kWood);
    sprBox(s, cx + 8, y, cx + 11, y + 22, rgb(0x4c, 0x2c, 0x14));   // back strap

    sprBox(s, cx - 8, y - 8, cx + 9, y + 1, kSteel);       // frame
    sprBox(s, cx - 8, y - 8, cx - 5, y + 1, kSteelLit);    // lit left face
    sprBox(s, cx + 1, y + 1, cx + 4, y + 8, kSteelDark);   // trigger guard
    sprBox(s, cx + 2, y + 2, cx + 3, y + 6, kTransparent); // ...and its hole

    sprBox(s, cx - 8, y - 30, cx + 4, y - 8, kBlued);      // slide
    sprBox(s, cx - 8, y - 30, cx - 5, y - 8, kSteel);      // lit left face
    sprBox(s, cx - 7, y - 26, cx + 3, y - 25, kSteelDark); // ejection port
    sprBox(s, cx - 3, y - 32, cx - 1, y - 30, kSteelLit);  // front sight
    sprEllipse(s, cx - 2.0, y - 30.0, 2.2, 1.6, kOutline); // muzzle

    if (frame == 1) drawFlash(s, cx - 2.0, y - 34.0, 1.1);

    drawFist(s, cx + 6, y + 12, 25, 20);
}

// --- machine gun ---------------------------------------------------------

void drawMachineGun(Sprite& s, int frame) {
    const int kick = (frame == 0) ? 0 : (frame == 1 ? 8 : (frame == 2 ? 4 : 2));
    const int cx = 30;
    const int y  = 44 - kick;

    sprBox(s, cx - 3, y + 2, cx + 7, y + 22, kBlued);      // magazine
    sprBox(s, cx + 4, y + 2, cx + 7, y + 22, kSteelDark);

    sprBox(s, cx - 10, y - 14, cx + 10, y + 4, kSteel);    // receiver
    sprBox(s, cx - 10, y - 14, cx - 7, y + 4, kSteelLit);
    sprBox(s, cx - 10, y + 2, cx + 10, y + 4, kSteelDark);
    sprBox(s, cx + 10, y - 8, cx + 18, y + 2, kWood);      // stock stub

    sprBox(s, cx - 4, y - 32, cx + 3, y - 14, kBlued);     // barrel shroud
    // Cooling slots: four notches, which is what stops the barrel reading as
    // a plain rectangle at this size.
    for (int i = 0; i < 4; ++i)
        sprBox(s, cx - 4, y - 30 + i * 4, cx + 3, y - 29 + i * 4, kSteelDark);
    sprBox(s, cx - 3, y - 34, cx + 2, y - 32, kSteel);     // muzzle
    sprEllipse(s, cx - 0.5, y - 34.0, 2.0, 1.4, kOutline);

    if (frame == 1) drawFlash(s, cx, y - 37.0, 1.25);

    drawFist(s, cx + 3, y + 14, 25, 20);
}

// --- chaingun ------------------------------------------------------------
//
// Two hands, and the barrel cluster spins: the barrels shift by one position
// each frame, which is the only way a static sprite reads as rotating.

void drawChaingun(Sprite& s, int frame) {
    const int kick = (frame == 0) ? 0 : 3;
    const int cx = 32;
    const int y  = 46 - kick;

    // Barrels first: the housing is drawn over their roots so the cluster
    // emerges from it rather than sitting on top of it.
    //
    // Six around a hub, phase-shifted per frame. The far-side barrels are
    // drawn darker, which is what makes a static sprite read as spinning
    // instead of as a ring of dots.
    const double phase = frame * 0.5235987756;   // 30 degrees a frame
    for (int i = 0; i < 6; ++i) {
        const double a = phase + i * 1.0471975512;
        const double bx = cx + std::cos(a) * 9.0;
        const double by = (y - 8) + std::sin(a) * 4.0;   // squashed: end-on
        const bool near = std::sin(a) > 0.0;
        sprBox(s, static_cast<int>(bx) - 2, static_cast<int>(by) - 28,
               static_cast<int>(bx) + 3, static_cast<int>(by),
               near ? kSteel : kSteelDark);
        if (near)
            sprBox(s, static_cast<int>(bx) - 2, static_cast<int>(by) - 28,
                   static_cast<int>(bx) - 1, static_cast<int>(by), kSteelLit);
    }
    sprEllipse(s, cx, y - 8.0, 6.0, 4.5, kBlued);              // hub

    sprBox(s, cx - 15, y - 6, cx + 15, y + 12, kSteel);        // housing
    sprBox(s, cx - 15, y - 6, cx - 11, y + 12, kSteelLit);
    sprBox(s, cx - 15, y + 12, cx + 15, y + 15, kSteelDark);
    sprBox(s, cx - 9, y - 2, cx + 9, y + 1, kSteelDark);       // vent band

    if (frame == 1 || frame == 3) drawFlash(s, cx, y - 34.0, 1.45);

    // Two hands: the only weapon here heavy enough to need both, which is
    // most of what tells you it is the chaingun before you have fired it.
    drawFist(s, cx - 17, y + 12, 21, 18);
    drawFist(s, cx + 17, y + 12, 21, 18);
}

} // namespace

void WeaponSprites::generate() {
    for (int w = 0; w < static_cast<int>(WeaponType::Count); ++w) {
        for (int f = 0; f < kFrames; ++f) {
            Sprite& s = spr_[static_cast<size_t>(w)][static_cast<size_t>(f)];
            sprClear(s);

            switch (static_cast<WeaponType>(w)) {
            case WeaponType::Knife:      drawKnife(s, f);      break;
            case WeaponType::Pistol:     drawPistol(s, f);     break;
            case WeaponType::MachineGun: drawMachineGun(s, f); break;
            case WeaponType::Chaingun:   drawChaingun(s, f);   break;
            default: break;
            }

            sprOutline(s, kOutline);
            // Lit from the left, like every other sprite in the game, so the
            // view model agrees with the world it is held in.
            sprShadeAcross(s, 0, kTexSize, 1.12, 0.78);
        }
    }
}

const Sprite& WeaponSprites::frame(WeaponType w, int f) const {
    const int wi = std::clamp(static_cast<int>(w), 0,
                              static_cast<int>(WeaponType::Count) - 1);
    const int fi = std::clamp(f, 0, kFrames - 1);
    return spr_[static_cast<size_t>(wi)][static_cast<size_t>(fi)];
}

void renderWeapon(Framebuffer& fb, const Sprite& art, double bob) {
    constexpr int kScale = 2;
    constexpr int kDrawn = kTexSize * kScale;

    // Anchored to the bottom of the 3D view, not the bottom of the screen:
    // a weapon drawn over the status bar would be clipped by it.
    const int baseX = (kScreenW - kDrawn) / 2;
    const int baseY = kViewH - kDrawn;

    // Sway. Horizontal at the walk frequency, vertical at twice it, which is
    // the figure-eight a real gait produces — a single sine reads as a
    // pendulum instead.
    const int offX = static_cast<int>(std::sin(bob) * 5.0);
    const int offY = static_cast<int>(std::abs(std::cos(bob * 2.0)) * 3.0);

    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const uint32_t c = art.at(x, y);
            if (isTransparent(c)) continue;
            const int px = baseX + x * kScale + offX;
            const int py = baseY + y * kScale + offY;
            for (int sy = 0; sy < kScale; ++sy)
                for (int sx = 0; sx < kScale; ++sx) {
                    const int fx = px + sx, fy = py + sy;
                    if (fy >= kViewH) continue;   // never over the status bar
                    fb.put(fx, fy, c);
                }
        }
    }
}

void applyMuzzleFlash(Framebuffer& fb, double intensity) {
    // The 3D view only. A flash that lit the status bar would read as a
    // rendering bug rather than as a gun going off.
    washRows(fb, kViewH, 0xFF, 0xE8, 0xA0, intensity * 0.45);
}

} // namespace wolf
