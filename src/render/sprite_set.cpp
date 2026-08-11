#include "sprite_set.h"

#include <cmath>

namespace wolf {
namespace {

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
    sprClear(s);
    sprBox(s, 20, 40, 44, 58, rgb(0xe8, 0xe8, 0xe4));
    sprBox(s, 20, 40, 44, 44, rgb(0xc0, 0xc0, 0xbc));   // lid
    // Cross, the one detail that has to survive being three pixels wide.
    sprBox(s, 30, 46, 34, 56, rgb(0xd0, 0x24, 0x20));
    sprBox(s, 26, 49, 38, 53, rgb(0xd0, 0x24, 0x20));
    sprBox(s, 28, 36, 36, 40, rgb(0x90, 0x90, 0x8c));   // handle
    sprOutline(s, kInk);
}

// Ammo clip: magazine with brass rounds showing at the top.
void makeAmmo(Sprite& s) {
    sprClear(s);
    sprBox(s, 26, 44, 38, 58, kGunBody);
    for (int i = 0; i < 3; ++i) sprBox(s, 27 + i * 4, 40, 30 + i * 4, 45, kGoldLit);
    sprBox(s, 26, 52, 38, 54, kGunLit);                 // band
    sprShadeAcross(s, 26, 38, 1.30, 0.78);
    sprOutline(s, kInk);
}

void makeTreasureCross(Sprite& s) {
    sprClear(s);
    sprBox(s, 29, 34, 35, 58, kGoldLit);
    sprBox(s, 22, 40, 42, 46, kGoldLit);
    sprShadeAcross(s, 22, 42, 1.18, 0.62);
    sprOutline(s, kInk);
}

void makeTreasureChalice(Sprite& s) {
    sprClear(s);
    sprEllipse(s, 32, 44, 10, 8, kGoldLit);             // bowl
    sprBox(s, 24, 36, 40, 44, kGoldLit);
    sprBox(s, 30, 50, 34, 55, kGoldLit);                // stem
    sprEllipse(s, 32, 56, 9, 3, kGoldLit);              // foot
    sprShadeAcross(s, 22, 42, 1.18, 0.62);
    sprOutline(s, kInk);
}

void makeTreasureChest(Sprite& s) {
    sprClear(s);
    sprBox(s, 18, 42, 46, 58, kWoodLit);
    sprEllipse(s, 32, 42, 14, 8, kWoodLit);             // domed lid
    sprBox(s, 18, 48, 46, 50, kGoldLit);                // strap
    sprBox(s, 30, 46, 34, 53, kGoldLit);                // lock
    sprShadeAcross(s, 18, 46, 1.15, 0.66);
    sprOutline(s, kInk);
}

void makeTreasureCrown(Sprite& s) {
    sprClear(s);
    sprBox(s, 22, 48, 42, 58, kGoldLit);
    // Points, tallest in the middle so it reads as a crown at four pixels.
    const int peak[5] = {44, 40, 36, 40, 44};
    for (int i = 0; i < 5; ++i) sprBox(s, 22 + i * 4, peak[i], 26 + i * 4, 50, kGoldLit);
    sprBox(s, 22, 52, 42, 54, kGoldDark);
    sprShadeAcross(s, 22, 42, 1.18, 0.62);
    sprOutline(s, kInk);
}

// Keys: bow, shaft and teeth. The only thing separating gold from silver is
// the palette, so they must differ in nothing else or players will misread
// which one they picked up.
void makeKey(Sprite& s, uint32_t lit) {
    sprClear(s);
    sprEllipse(s, 26, 48, 7, 7, lit);
    sprEllipse(s, 26, 48, 3, 3, kTransparent);          // hole in the bow
    sprBox(s, 32, 46, 46, 50, lit);                     // shaft
    sprBox(s, 40, 50, 43, 55, lit);                     // teeth
    sprBox(s, 44, 50, 46, 54, lit);
    sprShadeAcross(s, 19, 47, 1.20, 0.64);
    sprOutline(s, kInk);
}

// --- weapons on the floor ------------------------------------------------

void makeMachineGun(Sprite& s) {
    sprClear(s);
    sprBox(s, 18, 46, 46, 52, kGunBody);                // receiver
    sprBox(s, 12, 47, 20, 50, kGunLit);                 // barrel
    sprBox(s, 30, 52, 36, 58, kGunBody);                // magazine
    sprBox(s, 42, 44, 48, 50, kWoodDark);               // stock
    sprShadeAcross(s, 12, 48, 1.30, 0.78);
    sprOutline(s, kInk);
}

void makeChaingun(Sprite& s) {
    sprClear(s);
    sprBox(s, 22, 44, 46, 54, kGunBody);
    // Barrel cluster, the one silhouette cue that says chaingun.
    for (int i = 0; i < 3; ++i) sprBox(s, 8, 44 + i * 4, 24, 47 + i * 4, kGunLit);
    sprBox(s, 30, 54, 38, 58, kGunBody);
    sprShadeAcross(s, 8, 46, 1.30, 0.78);
    sprOutline(s, kInk);
}

// --- scenery -------------------------------------------------------------

// Floor lamp. Drawn tall in the frame so it stands up in the world; the
// bright head reads as a light source without any actual lighting.
void makeLamp(Sprite& s) {
    sprClear(s);
    sprBox(s, 30, 26, 34, 56, rgb(0x50, 0x50, 0x54));   // pole
    sprEllipse(s, 32, 56, 9, 4, rgb(0x40, 0x40, 0x44)); // base
    sprEllipse(s, 32, 22, 11, 9, rgb(0xff, 0xf0, 0xa0)); // glowing head
    sprEllipse(s, 32, 20, 6, 5, rgb(0xff, 0xff, 0xe4));
    sprOutline(s, kInk);
}

void makeTable(Sprite& s) {
    sprClear(s);
    sprBox(s, 14, 40, 50, 45, kWoodLit);                // top
    sprBox(s, 18, 45, 22, 58, kWoodDark);               // legs
    sprBox(s, 42, 45, 46, 58, kWoodDark);
    sprBox(s, 14, 44, 50, 46, kWoodDark);               // apron
    sprOutline(s, kInk);
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
