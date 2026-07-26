// Procedurally drawn object sprites: pickups and scenery.
//
// Same rule as the wall textures — no asset files, everything drawn by code
// into a 64x64 buffer, with kTransparent marking the pixels that are not
// part of the object.
//
// Sprites are billboarded across a whole cell, so where an object sits
// inside its 64x64 frame is what decides where it sits in the world. Things
// that rest on the floor are drawn low in the frame; a lamp hangs high. That
// is how the original did it too, and it means the renderer never needs a
// per-object vertical offset.
#pragma once

#include <array>
#include <cstdint>

#include "../core/config.h"
#include "../core/framebuffer.h"

namespace wolf {

enum SpriteId : uint8_t {
    SprHealth = 0,
    SprAmmo,
    SprTreasureCross,
    SprTreasureChalice,
    SprTreasureChest,
    SprTreasureCrown,
    SprGoldKey,
    SprSilverKey,
    SprMachineGun,
    SprChaingun,
    SprLamp,
    SprTable,
    SprCount
};

struct Sprite {
    std::array<uint32_t, kTexSize * kTexSize> px{};

    uint32_t at(int x, int y) const {
        return px[static_cast<size_t>(y) * kTexSize + x];
    }
    void set(int x, int y, uint32_t c) {
        if (x < 0 || x >= kTexSize || y < 0 || y >= kTexSize) return;
        px[static_cast<size_t>(y) * kTexSize + x] = c;
    }
};

class SpriteSet {
public:
    void generate();
    const Sprite& operator[](int id) const { return spr_[id]; }

private:
    std::array<Sprite, SprCount> spr_{};
};

} // namespace wolf
