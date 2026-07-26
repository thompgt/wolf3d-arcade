// Global compile-time configuration for the engine.
//
// The renderer is a fixed-resolution software rasteriser, exactly like the
// original: everything is drawn into a 320x200 buffer and then scaled up to
// the window. Keeping the internal resolution fixed is what gives the game
// its chunky, period-correct look regardless of window size.
#pragma once

#include <cstdint>

namespace wolf {

// Internal render target. Wolf3D ran at 320x200 with a 1.2:1 pixel aspect;
// we render square pixels and let the window stretch handle the rest.
constexpr int   kScreenW = 320;
constexpr int   kScreenH = 200;

// Integer scale applied when blitting to the window (320*3 x 200*3).
constexpr int   kWindowScale = 3;

// Height in pixels of the bottom status bar. The 3D view occupies the
// remaining rows, so the raycaster must never draw below kViewH.
constexpr int   kStatusBarH = 40;
constexpr int   kViewH      = kScreenH - kStatusBarH;

// Fixed simulation timestep. The original was tied to a 70Hz VGA refresh;
// we decouple sim from render and step the world at a steady 60Hz.
constexpr double kTickRate = 60.0;
constexpr double kTickDT   = 1.0 / kTickRate;

// Every texture in the game is 64x64, like the original VSWAP assets.
constexpr int   kTexSize = 64;

// Field of view, in radians (66 degrees, matching the classic 2:1 plane).
constexpr double kFov = 1.15192;

} // namespace wolf
