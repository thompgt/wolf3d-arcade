// Billboard sprite rendering.
//
// Objects have no geometry: each one is a flat image that always faces the
// camera, scaled by distance and clipped per column against the wall depth
// the raycaster recorded. That per-column test is the whole trick — it is
// what lets a guard stand half-hidden behind a doorway rather than being
// drawn either wholly in front of the wall or wholly behind it.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/framebuffer.h"

namespace wolf {

class Player;
class SpriteSet;

struct Billboard {
    double  x = 0.0;
    double  y = 0.0;
    uint8_t sprite = 0;
};

// Draws every billboard, furthest first. `list` is sorted in place, since
// the caller rebuilds it each frame anyway and sorting a copy would mean
// allocating one every frame.
void renderBillboards(Framebuffer& fb, const Player& player,
                      const std::vector<double>& wallDepth,
                      std::vector<Billboard>& list,
                      const SpriteSet& sprites);

} // namespace wolf
