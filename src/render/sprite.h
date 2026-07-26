// A 64x64 sprite frame and the handful of primitives used to draw one.
//
// Shared by the object sprites and the actor sprites, which are drawn by the
// same rules: block out shapes, then outline, then shade. Deliberately tiny
// — at this size the difference between a recognisable figure and a smudge
// is a handful of well-placed pixels, not a drawing library.
#pragma once

#include <array>
#include <cstdint>

#include "../core/config.h"
#include "../core/framebuffer.h"

namespace wolf {

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

inline void sprClear(Sprite& s) { s.px.fill(kTransparent); }

inline void sprBox(Sprite& s, int x0, int y0, int x1, int y1, uint32_t c) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) s.set(x, y, c);
}

inline void sprEllipse(Sprite& s, double cx, double cy, double rx, double ry,
                       uint32_t c) {
    if (rx <= 0.0 || ry <= 0.0) return;
    for (int y = static_cast<int>(cy - ry); y <= static_cast<int>(cy + ry); ++y) {
        for (int x = static_cast<int>(cx - rx); x <= static_cast<int>(cx + rx); ++x) {
            const double nx = (x - cx) / rx;
            const double ny = (y - cy) / ry;
            if (nx * nx + ny * ny <= 1.0) s.set(x, y, c);
        }
    }
}

// Outline drawn just outside the shape. Every sprite gets one: against
// textured masonry an unoutlined figure visually dissolves into the wall.
inline void sprOutline(Sprite& s, uint32_t c) {
    const Sprite src = s;
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

// Left-to-right light ramp, implying a light source off to the left.
//
// Multiplies the existing pixel rather than replacing it with a gradient.
// Replacing looks identical on a single-colour shape and silently erases
// everything else — belt buckles, insignia, the brass on an ammo clip — so
// the shape survives and all the detail inside it does not.
inline void sprShadeAcross(Sprite& s, int x0, int x1, double litF, double darkF) {
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

} // namespace wolf
