#include "bmp.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "framebuffer.h"

namespace wolf {
namespace {

void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>(x >> 8));
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
}

} // namespace

bool writeBMP(const std::string& path, const Framebuffer& fb) {
    const int w = fb.width();
    const int h = fb.height();

    // Each row is padded to a 4-byte boundary; 24-bit pixels rarely land
    // there on their own.
    const int rowBytes = w * 3;
    const int padding  = (4 - (rowBytes % 4)) % 4;
    const uint32_t imageBytes = static_cast<uint32_t>((rowBytes + padding) * h);
    const uint32_t headerBytes = 14 + 40;

    std::vector<uint8_t> head;
    head.reserve(headerBytes);
    head.push_back('B');
    head.push_back('M');
    put32(head, headerBytes + imageBytes);
    put16(head, 0);
    put16(head, 0);
    put32(head, headerBytes);      // offset to pixel data

    put32(head, 40);               // BITMAPINFOHEADER size
    put32(head, static_cast<uint32_t>(w));
    put32(head, static_cast<uint32_t>(h));
    put16(head, 1);                // planes
    put16(head, 24);               // bits per pixel
    put32(head, 0);                // BI_RGB
    put32(head, imageBytes);
    put32(head, 2835);             // ~72 DPI, x
    put32(head, 2835);             // ~72 DPI, y
    put32(head, 0);
    put32(head, 0);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(head.data()),
            static_cast<std::streamsize>(head.size()));

    // BMP rows run bottom-up, the opposite of our top-down buffer.
    const uint8_t pad[3] = {0, 0, 0};
    std::vector<uint8_t> row(static_cast<size_t>(rowBytes));
    const uint32_t* px = fb.data();
    for (int y = h - 1; y >= 0; --y) {
        const uint32_t* src = px + static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            const uint32_t c = src[x];
            row[x * 3 + 0] = static_cast<uint8_t>(c & 0xFF);          // blue
            row[x * 3 + 1] = static_cast<uint8_t>((c >> 8) & 0xFF);   // green
            row[x * 3 + 2] = static_cast<uint8_t>((c >> 16) & 0xFF);  // red
        }
        f.write(reinterpret_cast<const char*>(row.data()), rowBytes);
        if (padding) f.write(reinterpret_cast<const char*>(pad), padding);
    }
    return static_cast<bool>(f);
}

} // namespace wolf
