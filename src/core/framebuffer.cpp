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
