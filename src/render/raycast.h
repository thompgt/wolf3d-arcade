// The raycaster: one ray per screen column, DDA-stepped through the tile
// grid until it hits something solid.
//
// Because the world is grid-aligned there is no triangle rasterisation and
// no depth sorting for walls — each column is a single vertical run of
// pixels at a distance the DDA hands us exactly. That exactness is the
// whole reason the technique ran on a 386.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/config.h"
#include "../core/framebuffer.h"

namespace wolf {

class Map;
class Player;
class TextureSet;

// What a single ray hit. Kept public because sprites, weapon hitscans and
// the Use key all want to cast rays too.
struct RayHit {
    double perpDist = 0.0;  // distance along the view axis, not the ray, so
                            // walls stay flat instead of bowing (fisheye)
    int    mapX = 0;
    int    mapY = 0;
    bool   sideY = false;   // true if the ray crossed a horizontal edge
    double wallX = 0.0;     // 0..1 across the face that was hit; picks the
                            // texture column
    bool   hit = false;

    // Set when the ray stopped on a door slab rather than a wall face. The
    // texture then slides with the door instead of being pinned to the cell.
    bool   isDoor = false;
    // Set when the wall we hit is the reveal behind a doorway, which gets
    // the jamb texture so a recessed door reads as recessed.
    bool   isJamb = false;
    // Set when the ray stopped on a pushwall in motion. Those have left the
    // tile grid, so mapX/mapY do not identify them.
    bool   isPushwall = false;
    uint8_t tex = 0;        // resolved texture for this hit
};

// Casts a single ray from (px, py) along (dx, dy). Exposed so game code can
// ask line-of-sight and shooting questions with the same traversal the
// renderer uses.
RayHit castRay(const Map& map, double px, double py, double dx, double dy,
               double maxDist = 64.0);

// castRay plus any pushwall currently sliding. Use this for anything that
// must respect a secret in motion — rendering, line of sight, hitscans.
RayHit castRayDynamic(const Map& map, double px, double py,
                      double dx, double dy, double maxDist = 64.0);

// Banded distance darkening, shared so sprites dim on exactly the same
// schedule as the walls behind them. Two separate falloff curves is how you
// get objects that visibly float against their surroundings.
double distanceShade(double dist);

class Raycaster {
public:
    Raycaster() : depth_(kScreenW, 1e30) {}

    // Draws ceiling, floor and every wall column into the top kViewH rows.
    void render(Framebuffer& fb, const Map& map, const Player& player,
                const TextureSet& textures);

    // Per-column wall distance from the last render(). Sprites test against
    // this to know which columns of themselves are hidden behind geometry.
    const std::vector<double>& depth() const { return depth_; }

private:
    std::vector<double> depth_;
};

} // namespace wolf
