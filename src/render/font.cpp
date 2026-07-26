#include "font.h"

#include <cstring>

namespace wolf {
namespace {

// One glyph: eight rows, high bit leftmost. Written as binary so the letter
// is visible in the table.
struct Glyph { uint8_t row[kGlyphH]; };

// Order: 0-9, A-Z, then space % : - . ! / and a solid block.
const Glyph kGlyphs[] = {
    // 0
    {{0b01110000, 0b10001000, 0b10011000, 0b10101000, 0b11001000, 0b10001000, 0b01110000, 0}},
    // 1
    {{0b00100000, 0b01100000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b01110000, 0}},
    // 2
    {{0b01110000, 0b10001000, 0b00001000, 0b00110000, 0b01000000, 0b10000000, 0b11111000, 0}},
    // 3
    {{0b11111000, 0b00010000, 0b00100000, 0b00110000, 0b00001000, 0b10001000, 0b01110000, 0}},
    // 4
    {{0b00010000, 0b00110000, 0b01010000, 0b10010000, 0b11111000, 0b00010000, 0b00010000, 0}},
    // 5
    {{0b11111000, 0b10000000, 0b11110000, 0b00001000, 0b00001000, 0b10001000, 0b01110000, 0}},
    // 6
    {{0b00110000, 0b01000000, 0b10000000, 0b11110000, 0b10001000, 0b10001000, 0b01110000, 0}},
    // 7
    {{0b11111000, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b01000000, 0b01000000, 0}},
    // 8
    {{0b01110000, 0b10001000, 0b10001000, 0b01110000, 0b10001000, 0b10001000, 0b01110000, 0}},
    // 9
    {{0b01110000, 0b10001000, 0b10001000, 0b01111000, 0b00001000, 0b00010000, 0b01100000, 0}},
    // A
    {{0b01110000, 0b10001000, 0b10001000, 0b11111000, 0b10001000, 0b10001000, 0b10001000, 0}},
    // B
    {{0b11110000, 0b10001000, 0b10001000, 0b11110000, 0b10001000, 0b10001000, 0b11110000, 0}},
    // C
    {{0b01110000, 0b10001000, 0b10000000, 0b10000000, 0b10000000, 0b10001000, 0b01110000, 0}},
    // D
    {{0b11100000, 0b10010000, 0b10001000, 0b10001000, 0b10001000, 0b10010000, 0b11100000, 0}},
    // E
    {{0b11111000, 0b10000000, 0b10000000, 0b11110000, 0b10000000, 0b10000000, 0b11111000, 0}},
    // F
    {{0b11111000, 0b10000000, 0b10000000, 0b11110000, 0b10000000, 0b10000000, 0b10000000, 0}},
    // G
    {{0b01110000, 0b10001000, 0b10000000, 0b10111000, 0b10001000, 0b10001000, 0b01111000, 0}},
    // H
    {{0b10001000, 0b10001000, 0b10001000, 0b11111000, 0b10001000, 0b10001000, 0b10001000, 0}},
    // I
    {{0b01110000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b01110000, 0}},
    // J
    {{0b00111000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b10010000, 0b01100000, 0}},
    // K
    {{0b10001000, 0b10010000, 0b10100000, 0b11000000, 0b10100000, 0b10010000, 0b10001000, 0}},
    // L
    {{0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b11111000, 0}},
    // M
    {{0b10001000, 0b11011000, 0b10101000, 0b10101000, 0b10001000, 0b10001000, 0b10001000, 0}},
    // N
    {{0b10001000, 0b11001000, 0b10101000, 0b10011000, 0b10001000, 0b10001000, 0b10001000, 0}},
    // O
    {{0b01110000, 0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b01110000, 0}},
    // P
    {{0b11110000, 0b10001000, 0b10001000, 0b11110000, 0b10000000, 0b10000000, 0b10000000, 0}},
    // Q
    {{0b01110000, 0b10001000, 0b10001000, 0b10001000, 0b10101000, 0b10010000, 0b01101000, 0}},
    // R
    {{0b11110000, 0b10001000, 0b10001000, 0b11110000, 0b10100000, 0b10010000, 0b10001000, 0}},
    // S
    {{0b01111000, 0b10000000, 0b10000000, 0b01110000, 0b00001000, 0b00001000, 0b11110000, 0}},
    // T
    {{0b11111000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0}},
    // U
    {{0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b01110000, 0}},
    // V
    {{0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b10001000, 0b01010000, 0b00100000, 0}},
    // W
    {{0b10001000, 0b10001000, 0b10001000, 0b10101000, 0b10101000, 0b11011000, 0b10001000, 0}},
    // X
    {{0b10001000, 0b10001000, 0b01010000, 0b00100000, 0b01010000, 0b10001000, 0b10001000, 0}},
    // Y
    {{0b10001000, 0b10001000, 0b01010000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0}},
    // Z
    {{0b11111000, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000, 0b11111000, 0}},
    // space
    {{0, 0, 0, 0, 0, 0, 0, 0}},
    // %
    {{0b11001000, 0b11010000, 0b00010000, 0b00100000, 0b01000000, 0b01011000, 0b10011000, 0}},
    // :
    {{0, 0b00100000, 0b00100000, 0, 0b00100000, 0b00100000, 0, 0}},
    // -
    {{0, 0, 0, 0b11111000, 0, 0, 0, 0}},
    // .
    {{0, 0, 0, 0, 0, 0b01100000, 0b01100000, 0}},
    // !
    {{0b00100000, 0b00100000, 0b00100000, 0b00100000, 0b00100000, 0, 0b00100000, 0}},
    // /
    {{0b00001000, 0b00010000, 0b00010000, 0b00100000, 0b01000000, 0b01000000, 0b10000000, 0}},
};

constexpr int kSpaceIndex = 36;

// -1 for anything with no glyph; callers draw a space instead, which keeps
// the columns of a status bar lined up rather than shifting on bad input.
int glyphIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');   // folded to uppercase
    switch (c) {
    case ' ': return kSpaceIndex;
    case '%': return kSpaceIndex + 1;
    case ':': return kSpaceIndex + 2;
    case '-': return kSpaceIndex + 3;
    case '.': return kSpaceIndex + 4;
    case '!': return kSpaceIndex + 5;
    case '/': return kSpaceIndex + 6;
    default:  return -1;
    }
}

} // namespace

void drawText(Framebuffer& fb, int x, int y, const char* text,
              uint32_t color, int scale) {
    if (!text || scale < 1) return;

    int penX = x;
    for (const char* p = text; *p; ++p, penX += kGlyphW * scale) {
        const int gi = glyphIndex(*p);
        if (gi < 0 || gi == kSpaceIndex) continue;

        const Glyph& g = kGlyphs[gi];
        for (int row = 0; row < kGlyphH; ++row) {
            const uint8_t bits = g.row[row];
            if (!bits) continue;
            for (int col = 0; col < kGlyphW; ++col) {
                if (!(bits & (0x80u >> col))) continue;
                // Scaled by drawing a block per source pixel, which keeps
                // the glyph hard-edged. Any smoothing here would look wrong
                // against a 320x200 view that has none anywhere else.
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx)
                        fb.put(penX + col * scale + sx, y + row * scale + sy,
                               color);
            }
        }
    }
}

void drawTextShadowed(Framebuffer& fb, int x, int y, const char* text,
                      uint32_t color, uint32_t shadow, int scale) {
    drawText(fb, x + scale, y + scale, text, shadow, scale);
    drawText(fb, x, y, text, color, scale);
}

int textWidth(const char* text, int scale) {
    if (!text) return 0;
    return static_cast<int>(std::strlen(text)) * kGlyphW * scale;
}

void drawTextRight(Framebuffer& fb, int rightX, int y, const char* text,
                   uint32_t color, int scale) {
    drawText(fb, rightX - textWidth(text, scale), y, text, color, scale);
}

} // namespace wolf
