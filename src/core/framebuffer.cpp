#include "framebuffer.h"

#include <algorithm>

namespace wolf {

uint32_t shade(uint32_t color, double factor) {
    if (factor >= 1.0) return color;
    if (factor <= 0.0) return 0;
    const auto r = static_cast<uint8_t>(((color >> 16) & 0xFF) * factor);
    const auto g = static_cast<uint8_t>(((color >> 8) & 0xFF) * factor);
    const auto b = static_cast<uint8_t>((color & 0xFF) * factor);
    return rgb(r, g, b);
}

void washRows(Framebuffer& fb, int rows, uint8_t r, uint8_t g, uint8_t b,
              double t) {
    t = std::clamp(t, 0.0, 1.0);
    if (t <= 0.0) return;

    const int y1 = std::min(rows, fb.height());
    const int w  = fb.width();
    uint32_t* px = fb.data();

    // Signed, deliberately: a channel already brighter than the target has
    // to move down toward it, and unsigned arithmetic would wrap that
    // difference into a huge positive number.
    const auto mix = [t](uint32_t v, int target) {
        const double o = v + (target - static_cast<int>(v)) * t;
        return static_cast<uint8_t>(std::clamp(o, 0.0, 255.0));
    };

    for (int y = 0; y < y1; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t& c = px[static_cast<size_t>(y) * w + x];
            c = rgb(mix((c >> 16) & 0xFF, r),
                    mix((c >> 8) & 0xFF, g),
                    mix(c & 0xFF, b));
        }
    }
}

Framebuffer::Framebuffer(int w, int h)
    : w_(w), h_(h), pixels_(static_cast<size_t>(w) * h, 0) {}

void Framebuffer::clear(uint32_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Framebuffer::put(int x, int y, uint32_t color) {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
    pixels_[static_cast<size_t>(y) * w_ + x] = color;
}

void Framebuffer::vline(int x, int y0, int y1, uint32_t color) {
    if (x < 0 || x >= w_) return;
    y0 = std::max(y0, 0);
    y1 = std::min(y1, h_);
    uint32_t* col = pixels_.data() + static_cast<size_t>(y0) * w_ + x;
    for (int y = y0; y < y1; ++y, col += w_) *col = color;
}

void Framebuffer::fillRect(int x, int y, int w, int h, uint32_t color) {
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, w_);
    const int y1 = std::min(y + h, h_);
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = pixels_.data() + static_cast<size_t>(yy) * w_ + x0;
        for (int xx = x0; xx < x1; ++xx) *row++ = color;
    }
}

} // namespace wolf
