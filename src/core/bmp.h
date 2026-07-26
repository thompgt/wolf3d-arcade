// Framebuffer -> .bmp dump.
//
// Exists because the game blits straight to the window DC, which means
// external screen-capture tools can hand back a stale composited surface
// instead of the current frame. Dumping the buffer we actually drew is the
// only way to be certain a screenshot shows the real output — and it doubles
// as the way README images get made.
//
// BMP because it needs no encoder: a fixed header followed by raw pixels.
#pragma once

#include <string>

namespace wolf {

class Framebuffer;

// Writes fb as a 24-bit uncompressed bitmap. Returns false if the file
// could not be opened.
bool writeBMP(const std::string& path, const Framebuffer& fb);

} // namespace wolf
