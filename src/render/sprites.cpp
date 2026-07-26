#include "sprites.h"

#include <algorithm>
#include <cmath>

#include "../core/config.h"
#include "../game/player.h"
#include "raycast.h"
#include "sprite_set.h"

namespace wolf {
namespace {

// Anything nearer than this is either clipping into the camera or divides
// the projection into absurd sizes; skip it rather than draw a wall of
// pixels across the screen.
constexpr double kNearClip = 0.12;

} // namespace

void renderBillboards(Framebuffer& fb, const Player& player,
                      const std::vector<double>& wallDepth,
                      std::vector<Billboard>& list) {
    if (list.empty()) return;

    const double px = player.x();
    const double py = player.y();

    // Painter's algorithm, furthest first. Sorting on squared distance
    // avoids a square root per comparison and orders identically.
    std::sort(list.begin(), list.end(),
              [px, py](const Billboard& a, const Billboard& b) {
                  const double da = (a.x - px) * (a.x - px) + (a.y - py) * (a.y - py);
                  const double db = (b.x - px) * (b.x - px) + (b.y - py) * (b.y - py);
                  return da > db;
              });

    const double dirX = player.dirX(), dirY = player.dirY();
    const double planeX = player.planeX(), planeY = player.planeY();

    // Inverse of the camera matrix [plane | dir]. Constant for the frame, so
    // it is computed once rather than per sprite.
    const double det = planeX * dirY - dirX * planeY;
    if (det == 0.0) return;
    const double invDet = 1.0 / det;

    for (const Billboard& b : list) {
        if (!b.image) continue;
        const double relX = b.x - px;
        const double relY = b.y - py;

        // Into camera space: transY is depth along the view axis, directly
        // comparable with the wall depths, and transX is the lateral offset.
        const double transX = invDet * (dirY * relX - dirX * relY);
        const double transY = invDet * (-planeY * relX + planeX * relY);
        if (transY < kNearClip) continue;   // behind the camera, or in it

        const int screenX = static_cast<int>((kScreenW / 2) * (1.0 + transX / transY));

        // Same projection as a wall column, so a sprite standing against a
        // wall is exactly as tall as that wall.
        const int size = std::abs(static_cast<int>(kViewH / transY));
        if (size <= 0) continue;

        const int top    = kViewH / 2 - size / 2;
        const int left   = screenX - size / 2;
        const int x0 = std::max(left, 0);
        const int x1 = std::min(left + size, kScreenW);
        if (x0 >= x1) continue;

        const Sprite& spr = *b.image;
        const double shadeF = distanceShade(transY);

        for (int x = x0; x < x1; ++x) {
            // Per-column depth test against the walls. Without it a sprite
            // in the next room draws straight through the wall between you.
            if (transY >= wallDepth[static_cast<size_t>(x)]) continue;

            const int texX = ((x - left) * kTexSize) / size;
            if (texX < 0 || texX >= kTexSize) continue;

            const int y0 = std::max(top, 0);
            const int y1 = std::min(top + size, kViewH);
            for (int y = y0; y < y1; ++y) {
                const int texY = ((y - top) * kTexSize) / size;
                const uint32_t c = spr.at(texX, texY);
                if (isTransparent(c)) continue;
                fb.put(x, y, shade(c, shadeF));
            }
        }
    }
}

} // namespace wolf
