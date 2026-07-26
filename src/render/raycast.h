// The raycaster: one ray per screen column, DDA-stepped through the tile
// grid until it hits something solid.
//
// Because the world is grid-aligned there is no triangle rasterisation and
// no depth sorting for walls — each column is a single vertical run of
// pixels at a distance the DDA hands us exactly. That exactness is the
// whole reason the technique ran on a 386.
#pragma once

#include <vector>

#include "../core/config.h"
#include "../core/framebuffer.h"

namespace wolf {

class Map;
class Player;

// What a single ray hit. Kept public because sprites, weapon hitscans and
// the Use key all want to cast rays too.
struct RayHit {
    double perpDist = 0.0;  // distance along the view axis, not the ray, so
                            // walls stay flat instead of bowing (fisheye)
    int    mapX = 0;
    int    mapY = 0;
    bool   sideY = false;   // true if the ray crossed a horizontal edge
    double wallX = 0.0;     // 0..1 across the face that was hit; the
                            // texture column in phase 2
    bool   hit = false;
};

// Casts a single ray from (px, py) along (dx, dy). Exposed so game code can
// ask line-of-sight and shooting questions with the same traversal the
// renderer uses.
RayHit castRay(const Map& map, double px, double py, double dx, double dy,
               double maxDist = 64.0);

class Raycaster {
public:
    Raycaster() : depth_(kScreenW, 1e30) {}

    // Draws ceiling, floor and every wall column into the top kViewH rows.
    void render(Framebuffer& fb, const Map& map, const Player& player);

    // Per-column wall distance from the last render(). Sprites test against
    // this to know which columns of themselves are hidden behind geometry.
    const std::vector<double>& depth() const { return depth_; }

private:
    std::vector<double> depth_;
};

} // namespace wolf
