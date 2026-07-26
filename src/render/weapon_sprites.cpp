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
void drawFist(Sprite& s, int cx, int cy, int w, int h) {
    sprBox(s, cx - w / 2, cy, cx + w / 2, kBase, kGlove);
    sprEllipse(s, cx, cy + 2.0, w / 2.0, h / 2.0, kGlove);
    // Knuckles, as three lit bumps. Without them the hand is a mitten.
    for (int i = -1; i <= 1; ++i)
        sprEllipse(s, cx + i * (w / 4.0), cy + 1.0, w / 9.0, 2.0, kGloveLit);
    sprBox(s, cx - w / 2, cy + h / 2, cx - w / 2 + 2, kBase, kGloveDark);
}

// --- knife ---------------------------------------------------------------
//
// Held to the right and thrust diagonally, so the blade crosses the centre
// of the screen on the strike rather than stabbing the bottom-right corner.

void drawKnife(Sprite& s, int frame) {
    // Thrust: out and up, then back. Frame 0 is the resting pose.
    const int lift  = (frame == 0) ? 0 : (frame == 2 ? 20 : (frame == 1 ? 12 : 6));
    const int reach = (frame == 0) ? 0 : (frame == 2 ? 10 : (frame == 1 ?  6 : 3));

    const int handX = 46 - reach;
    const int handY = 46 - lift;

    // Blade: a taper drawn as narrowing rows, with a lit edge down one side.
    const int tipY = handY - 26;
    for (int y = tipY; y < handY - 4; ++y) {
        const double t = static_cast<double>(y - tipY) / (handY - 4 - tipY);
        const int halfW = 1 + static_cast<int>(t * 3.5);
        const int bx = handX - static_cast<int>((1.0 - t) * 6.0);
        sprBox(s, bx - halfW, y, bx + halfW, y + 1, kSteel);
        s.set(bx - halfW, y, kSteelLit);          // ground edge catches light
        s.set(bx + halfW, y, kSteelDark);
    }

    sprBox(s, handX - 7, handY - 5, handX + 7, handY - 2, kBrass);   // guard
    drawFist(s, handX, handY, 20, 18);
}

// --- pistol --------------------------------------------------------------

void drawPistol(Sprite& s, int frame) {
    // Recoil: the whole weapon kicks up and settles.
    const int kick = (frame == 0) ? 0 : (frame == 1 ? 9 : (frame == 2 ? 5 : 2));
    const int cx = 34;
    const int y  = 40 - kick;

    sprBox(s, cx - 4, y - 20, cx + 4, y + 2, kBlued);      // slide
    sprBox(s, cx - 4, y - 20, cx - 2, y + 2, kSteel);      // lit left face
    sprBox(s, cx - 2, y - 21, cx + 2, y - 20, kSteelLit);  // front sight
    sprBox(s, cx - 3, y + 2, cx + 3, y + 8, kBlued);       // frame
    sprBox(s, cx - 2, y + 6, cx + 5, y + 16, kWood);       // grip
    sprBox(s, cx - 1, y + 5, cx + 1, y + 9, kSteelDark);   // trigger guard

    if (frame == 1) drawFlash(s, cx, y - 22.0, 1.0);

    drawFist(s, cx + 2, y + 12, 20, 18);
}

// --- machine gun ---------------------------------------------------------

void drawMachineGun(Sprite& s, int frame) {
    const int kick = (frame == 0) ? 0 : (frame == 1 ? 7 : (frame == 2 ? 4 : 2));
    const int cx = 32;
    const int y  = 38 - kick;

    sprBox(s, cx - 3, y - 30, cx + 3, y - 12, kBlued);     // barrel
    // Cooling slots: three notches, which is what stops the barrel reading
    // as a plain rectangle.
    for (int i = 0; i < 3; ++i)
        sprBox(s, cx - 3, y - 27 + i * 5, cx + 3, y - 26 + i * 5, kSteelDark);

    sprBox(s, cx - 6, y - 12, cx + 6, y + 6, kSteel);      // receiver
    sprBox(s, cx - 6, y - 12, cx - 4, y + 6, kSteelLit);
    sprBox(s, cx - 2, y + 6, cx + 4, y + 20, kBlued);      // magazine
    sprBox(s, cx + 6, y - 6, cx + 12, y + 2, kWood);       // stock stub

    if (frame == 1) drawFlash(s, cx, y - 32.0, 1.2);

    drawFist(s, cx + 4, y + 14, 22, 18);
}

// --- chaingun ------------------------------------------------------------
//
// Two hands, and the barrel cluster spins: the barrels shift by one position
// each frame, which is the only way a static sprite reads as rotating.

void drawChaingun(Sprite& s, int frame) {
    const int kick = (frame == 0) ? 0 : 3;
    const int cx = 32;
    const int y  = 40 - kick;

    sprBox(s, cx - 12, y - 10, cx + 12, y + 8, kSteel);        // housing
    sprBox(s, cx - 12, y - 10, cx - 9, y + 8, kSteelLit);
    sprBox(s, cx - 12, y + 8, cx + 12, y + 11, kSteelDark);

    // Six barrels around a hub, phase-shifted per frame so the cluster turns.
    const double phase = frame * 0.5235987756;   // 30 degrees a frame
    for (int i = 0; i < 6; ++i) {
        const double a = phase + i * 1.0471975512;
        const double bx = cx + std::cos(a) * 7.0;
        const double by = (y - 4) + std::sin(a) * 3.2;   // squashed: seen end-on
        // Barrels on the far side of the hub are darker, which is what gives
        // the cluster depth rather than making it a ring of dots.
        const bool near = std::sin(a) > 0.0;
        sprBox(s, static_cast<int>(bx) - 2, static_cast<int>(by) - 22,
               static_cast<int>(bx) + 2, static_cast<int>(by),
               near ? kSteel : kSteelDark);
    }
    sprEllipse(s, cx, y - 4.0, 5.0, 4.0, kBlued);              // hub

    if (frame == 1 || frame == 3) drawFlash(s, cx, y - 26.0, 1.4);

    drawFist(s, cx - 14, y + 6, 20, 17);
    drawFist(s, cx + 14, y + 6, 20, 17);
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
    if (intensity <= 0.0) return;
    const double t = std::clamp(intensity, 0.0, 1.0) * 0.45;

    uint32_t* px = fb.data();
    for (int y = 0; y < kViewH; ++y) {
        for (int x = 0; x < kScreenW; ++x) {
            const uint32_t c = px[static_cast<size_t>(y) * kScreenW + x];
            // Lerp toward the flash colour rather than adding: adding clips
            // bright walls to flat white and loses the texture entirely.
            // Signed, deliberately: a pixel already brighter than the flash
            // colour must dim toward it, and unsigned arithmetic would wrap
            // that difference into a huge positive number.
            const auto mix = [t](uint32_t v, int target) {
                const double o = v + (target - static_cast<int>(v)) * t;
                return static_cast<uint8_t>(std::clamp(o, 0.0, 255.0));
            };
            px[static_cast<size_t>(y) * kScreenW + x] =
                rgb(mix((c >> 16) & 0xFF, 0xFF),
                    mix((c >> 8) & 0xFF, 0xE8),
                    mix(c & 0xFF, 0xA0));
        }
    }
}

} // namespace wolf
