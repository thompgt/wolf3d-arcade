// A plain 32-bit ARGB pixel buffer plus the handful of primitives the rest
// of the engine draws with. There is no hardware acceleration anywhere in
// this project: every wall column, sprite and HUD glyph lands here.
#pragma once

#include <cstdint>
#include <vector>

#include "config.h"

namespace wolf {

// Pack a colour into the 0x00RRGGBB layout a Win32 BI_RGB DIB expects.
constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

// Sprites need a "draw nothing here" value. The DIB format ignores the top
// byte, so setting it marks a pixel transparent without colliding with any
// real colour — in particular it leaves pure black usable, which a sentinel
// like rgb(0,0,0) would not.
constexpr uint32_t kTransparent = 0xFF000000u;

constexpr bool isTransparent(uint32_t c) { return (c & 0xFF000000u) != 0; }

// Scale a packed colour by a 0..1 factor. Used for distance shading and for
// the darker side-wall tint that gives flat walls their sense of volume.
uint32_t shade(uint32_t color, double factor);

class Framebuffer {
public:
    Framebuffer(int w = kScreenW, int h = kScreenH);

    int width()  const { return w_; }
    int height() const { return h_; }

    // Raw access for the platform layer's blit.
    const uint32_t* data() const { return pixels_.data(); }
    uint32_t*       data()       { return pixels_.data(); }

    void clear(uint32_t color);

    // Bounds-checked single pixel write. Hot loops that already clamp their
    // ranges should index data() directly instead.
    void put(int x, int y, uint32_t color);

    // Fill rows [y0, y1) of a single column. The raycaster's main primitive.
    void vline(int x, int y0, int y1, uint32_t color);

    void fillRect(int x, int y, int w, int h, uint32_t color);

private:
    int w_;
    int h_;
    std::vector<uint32_t> pixels_;
};

} // namespace wolf
