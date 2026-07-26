// Procedurally generated wall textures.
//
// There are no image files in this project. Wolfenstein 3D's art is
// copyrighted and not redistributable, so every 64x64 texture is drawn by
// code at startup from noise and simple masonry rules. That keeps the repo
// self-contained, keeps the whole set stylistically consistent for free,
// and makes a texture a function you can tweak rather than an asset you
// have to repaint.
//
// Generation is seeded and deterministic: the same build always produces
// the same pixels, so a visual regression is a code change, never noise.
#pragma once

#include <array>
#include <cstdint>

#include "../core/config.h"
#include "../core/framebuffer.h"   // rgb()
#include "../game/map.h"           // TexId

namespace wolf {

struct Texture {
    std::array<uint32_t, kTexSize * kTexSize> px{};

    uint32_t at(int x, int y) const {
        return px[static_cast<size_t>(y) * kTexSize + x];
    }
    void set(int x, int y, uint32_t c) {
        px[static_cast<size_t>(y) * kTexSize + x] = c;
    }
};

class TextureSet {
public:
    // Builds every texture in TexId order. Cheap enough (a few hundred
    // thousand pixels) to run at startup with no visible delay.
    void generate();

    const Texture& operator[](int id) const { return tex_[id]; }

private:
    std::array<Texture, TexCount> tex_{};
};

} // namespace wolf
