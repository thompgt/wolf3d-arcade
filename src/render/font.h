// An 8x8 bitmap font, defined in source like everything else here.
//
// The glyphs are 5x7 inside an 8x8 cell, written out as binary literals so
// the shape of each letter is legible in the table itself — a font stored as
// hex is unreadable and unmaintainable, and the whole point of having no
// asset files is that the art stays editable in the source.
//
// Only uppercase, digits and a handful of punctuation exist. That is the
// entire character set a Wolf3D status bar and menu ever needed, and a
// lowercase set would be dead weight.
#pragma once

#include <cstdint>

#include "../core/framebuffer.h"

namespace wolf {

constexpr int kGlyphW = 8;
constexpr int kGlyphH = 8;

// Draws text at `scale` times size, with the top-left of the first glyph at
// (x, y). Lowercase is folded to uppercase; anything with no glyph is drawn
// as a space rather than dropped, so columns in a status bar stay aligned.
void drawText(Framebuffer& fb, int x, int y, const char* text,
              uint32_t color, int scale = 1);

// Same, with a one-pixel offset drop shadow underneath. The status bar is a
// mid-green and pure white text on it vibrates; a shadow anchors it.
void drawTextShadowed(Framebuffer& fb, int x, int y, const char* text,
                      uint32_t color, uint32_t shadow, int scale = 1);

// Width in pixels of `text` at `scale`, for right-aligning and centring.
int textWidth(const char* text, int scale = 1);

// Right-aligned at `rightX`, which is what a score or an ammo count wants:
// the digits should grow leftward rather than shoving the label around.
void drawTextRight(Framebuffer& fb, int rightX, int y, const char* text,
                   uint32_t color, int scale = 1);

} // namespace wolf
