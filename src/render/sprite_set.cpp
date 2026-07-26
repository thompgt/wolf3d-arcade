#include "sprite_set.h"

#include <cmath>

namespace wolf {
namespace {

// --- drawing primitives --------------------------------------------------
//
// Deliberately tiny. At 64x64 the difference between a recognisable object
// and a smudge is a handful of well-placed pixels, so these stay close to
// "set this rectangle" rather than becoming a drawing library.

void clear(Sprite& s) { s.px.fill(kTransparent); }

void box(Sprite& s, int x0, int y0, int x1, int y1, uint32_t c) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) s.set(x, y, c);
}

// Outline drawn just outside the shape. Every sprite gets one: against a
// textured wall an unoutlined object visually dissolves into the masonry.
void outline(Sprite& s, uint32_t c) {
    Sprite src = s;
    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            if (!isTransparent(src.at(x, y))) continue;
            bool touches = false;
            for (int dy = -1; dy <= 1 && !touches; ++dy)
                for (int dx = -1; dx <= 1 && !touches; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= kTexSize || ny < 0 || ny >= kTexSize) continue;
                    if (!isTransparent(src.at(nx, ny))) touches = true;
                }
            if (touches) s.set(x, y, c);
        }
    }
}

void ellipse(Sprite& s, double cx, double cy, double rx, double ry, uint32_t c) {
    for (int y = static_cast<int>(cy - ry); y <= static_cast<int>(cy + ry); ++y) {
        for (int x = static_cast<int>(cx - rx); x <= static_cast<int>(cx + rx); ++x) {
            const double nx = (x - cx) / rx;
            const double ny = (y - cy) / ry;
            if (nx * nx + ny * ny <= 1.0) s.set(x, y, c);
        }
    }
}

// Left-to-right light ramp, giving a flat shape a sense of roundness by
// implying a light source off to the left.
//
// Multiplies the existing pixel rather than replacing it with a gradient
// between two colours. Replacing looks equivalent on a single-colour shape
// and destroys everything else: a chest's gold strap, a crown's band and an
// ammo clip's brass rounds all vanish under a flat repaint, leaving a
// smooth blob where the identifying detail used to be.
void shadeAcross(Sprite& s, int x0, int x1, double litF, double darkF) {
    if (x1 <= x0) return;
    for (int y = 0; y < kTexSize; ++y) {
        for (int x = x0; x < x1; ++x) {
            const uint32_t c = s.at(x, y);
            if (isTransparent(c)) continue;
            const double t = static_cast<double>(x - x0) / (x1 - x0);
            const double f = litF + (darkF - litF) * t;
            const auto ch = [f](uint32_t v) {
                const double o = v * f;
                return static_cast<uint8_t>(o > 255.0 ? 255.0 : o);
            };
            s.set(x, y, rgb(ch((c >> 16) & 0xFF), ch((c >> 8) & 0xFF), ch(c & 0xFF)));
        }
    }
}

// --- palette -------------------------------------------------------------

constexpr uint32_t kInk       = rgb(0x14, 0x10, 0x0e);
constexpr uint32_t kGoldLit   = rgb(0xff, 0xd8, 0x50);
constexpr uint32_t kGoldDark  = rgb(0x9a, 0x6c, 0x14);
constexpr uint32_t kSteelLit  = rgb(0xd8, 0xdc, 0xe4);
constexpr uint32_t kGunBody   = rgb(0x3c, 0x40, 0x46);
constexpr uint32_t kGunLit    = rgb(0x6e, 0x74, 0x7c);
constexpr uint32_t kWoodLit   = rgb(0x9a, 0x66, 0x36);
constexpr uint32_t kWoodDark  = rgb(0x5c, 0x38, 0x1c);

// --- pickups -------------------------------------------------------------

// First aid kit: white case, red cross, sitting on the floor.
void makeHealth(Sprite& s) {
    clear(s);
    box(s, 20, 40, 44, 58, rgb(0xe8, 0xe8, 0xe4));
    box(s, 20, 40, 44, 44, rgb(0xc0, 0xc0, 0xbc));   // lid
    // Cross, the one detail that has to survive being three pixels wide.
    box(s, 30, 46, 34, 56, rgb(0xd0, 0x24, 0x20));
    box(s, 26, 49, 38, 53, rgb(0xd0, 0x24, 0x20));
    box(s, 28, 36, 36, 40, rgb(0x90, 0x90, 0x8c));   // handle
    outline(s, kInk);
}

// Ammo clip: magazine with brass rounds showing at the top.
void makeAmmo(Sprite& s) {
    clear(s);
    box(s, 26, 44, 38, 58, kGunBody);
    for (int i = 0; i < 3; ++i) box(s, 27 + i * 4, 40, 30 + i * 4, 45, kGoldLit);
    box(s, 26, 52, 38, 54, kGunLit);                 // band
    shadeAcross(s, 26, 38, 1.30, 0.78);
    outline(s, kInk);
}

void makeTreasureCross(Sprite& s) {
    clear(s);
    box(s, 29, 34, 35, 58, kGoldLit);
    box(s, 22, 40, 42, 46, kGoldLit);
    shadeAcross(s, 22, 42, 1.18, 0.62);
    outline(s, kInk);
}

void makeTreasureChalice(Sprite& s) {
    clear(s);
    ellipse(s, 32, 44, 10, 8, kGoldLit);             // bowl
    box(s, 24, 36, 40, 44, kGoldLit);
    box(s, 30, 50, 34, 55, kGoldLit);                // stem
    ellipse(s, 32, 56, 9, 3, kGoldLit);              // foot
    shadeAcross(s, 22, 42, 1.18, 0.62);
    outline(s, kInk);
}

void makeTreasureChest(Sprite& s) {
    clear(s);
    box(s, 18, 42, 46, 58, kWoodLit);
    ellipse(s, 32, 42, 14, 8, kWoodLit);             // domed lid
    box(s, 18, 48, 46, 50, kGoldLit);                // strap
    box(s, 30, 46, 34, 53, kGoldLit);                // lock
    shadeAcross(s, 18, 46, 1.15, 0.66);
    outline(s, kInk);
}

void makeTreasureCrown(Sprite& s) {
    clear(s);
    box(s, 22, 48, 42, 58, kGoldLit);
    // Points, tallest in the middle so it reads as a crown at four pixels.
    const int peak[5] = {44, 40, 36, 40, 44};
    for (int i = 0; i < 5; ++i) box(s, 22 + i * 4, peak[i], 26 + i * 4, 50, kGoldLit);
    box(s, 22, 52, 42, 54, kGoldDark);
    shadeAcross(s, 22, 42, 1.18, 0.62);
    outline(s, kInk);
}

// Keys: bow, shaft and teeth. The only thing separating gold from silver is
// the palette, so they must differ in nothing else or players will misread
// which one they picked up.
void makeKey(Sprite& s, uint32_t lit) {
    clear(s);
    ellipse(s, 26, 48, 7, 7, lit);
    ellipse(s, 26, 48, 3, 3, kTransparent);          // hole in the bow
    box(s, 32, 46, 46, 50, lit);                     // shaft
    box(s, 40, 50, 43, 55, lit);                     // teeth
    box(s, 44, 50, 46, 54, lit);
    shadeAcross(s, 19, 47, 1.20, 0.64);
    outline(s, kInk);
}

// --- weapons on the floor ------------------------------------------------

void makeMachineGun(Sprite& s) {
    clear(s);
    box(s, 18, 46, 46, 52, kGunBody);                // receiver
    box(s, 12, 47, 20, 50, kGunLit);                 // barrel
    box(s, 30, 52, 36, 58, kGunBody);                // magazine
    box(s, 42, 44, 48, 50, kWoodDark);               // stock
    shadeAcross(s, 12, 48, 1.30, 0.78);
    outline(s, kInk);
}

void makeChaingun(Sprite& s) {
    clear(s);
    box(s, 22, 44, 46, 54, kGunBody);
    // Barrel cluster, the one silhouette cue that says chaingun.
    for (int i = 0; i < 3; ++i) box(s, 8, 44 + i * 4, 24, 47 + i * 4, kGunLit);
    box(s, 30, 54, 38, 58, kGunBody);
    shadeAcross(s, 8, 46, 1.30, 0.78);
    outline(s, kInk);
}

// --- scenery -------------------------------------------------------------

// Floor lamp. Drawn tall in the frame so it stands up in the world; the
// bright head reads as a light source without any actual lighting.
void makeLamp(Sprite& s) {
    clear(s);
    box(s, 30, 26, 34, 56, rgb(0x50, 0x50, 0x54));   // pole
    ellipse(s, 32, 56, 9, 4, rgb(0x40, 0x40, 0x44)); // base
    ellipse(s, 32, 22, 11, 9, rgb(0xff, 0xf0, 0xa0)); // glowing head
    ellipse(s, 32, 20, 6, 5, rgb(0xff, 0xff, 0xe4));
    outline(s, kInk);
}

void makeTable(Sprite& s) {
    clear(s);
    box(s, 14, 40, 50, 45, kWoodLit);                // top
    box(s, 18, 45, 22, 58, kWoodDark);               // legs
    box(s, 42, 45, 46, 58, kWoodDark);
    box(s, 14, 44, 50, 46, kWoodDark);               // apron
    outline(s, kInk);
}

} // namespace

void SpriteSet::generate() {
    makeHealth(spr_[SprHealth]);
    makeAmmo(spr_[SprAmmo]);
    makeTreasureCross(spr_[SprTreasureCross]);
    makeTreasureChalice(spr_[SprTreasureChalice]);
    makeTreasureChest(spr_[SprTreasureChest]);
    makeTreasureCrown(spr_[SprTreasureCrown]);
    makeKey(spr_[SprGoldKey], kGoldLit);
    makeKey(spr_[SprSilverKey], kSteelLit);
    makeMachineGun(spr_[SprMachineGun]);
    makeChaingun(spr_[SprChaingun]);
    makeLamp(spr_[SprLamp]);
    makeTable(spr_[SprTable]);
}

} // namespace wolf
