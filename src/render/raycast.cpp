#include "raycast.h"

#include <algorithm>
#include <cmath>

#include "../game/map.h"
#include "../game/player.h"

namespace wolf {
namespace {

// Flat stand-in colours, one per TexId. Phase 2 replaces these with the
// procedurally generated 64x64 textures; the geometry below does not change.
constexpr uint32_t kFlatColor[TexCount] = {
    rgb(0x9a, 0x8f, 0x84),  // TexBrick      warm grey
    rgb(0x4c, 0x62, 0xa8),  // TexBlueStone
    rgb(0x8a, 0x5d, 0x33),  // TexWood
    rgb(0x5c, 0x7a, 0x4a),  // TexMoss
    rgb(0x86, 0x8c, 0x92),  // TexSteel
    rgb(0xc8, 0xa8, 0x38),  // TexDoor       brass, reads as interactive
    rgb(0x6a, 0x6a, 0x6a),  // TexDoorFrame
    rgb(0xd0, 0x40, 0x30),  // TexExitSwitch red, unmistakable
};

constexpr uint32_t kCeiling = rgb(0x38, 0x38, 0x38);
constexpr uint32_t kFloor   = rgb(0x70, 0x70, 0x70);

// Distance at which a wall reaches full darkness.
constexpr double kFogDist = 22.0;

// Number of discrete shading steps. The original shifted between palette
// rows rather than blending, so banding here is deliberate: smooth falloff
// looks modern and wrong.
constexpr int kShadeBands = 16;

// Faces the ray hit edge-on are drawn darker. With flat colours this is the
// only cue that a corner is a corner, and it survives into the textured
// build for the same reason.
constexpr double kSideDarken = 0.68;

double bandedShade(double dist, bool sideY) {
    double f = 1.0 - dist / kFogDist;
    f = std::clamp(f, 0.15, 1.0);
    f = std::floor(f * kShadeBands) / kShadeBands;
    return sideY ? f * kSideDarken : f;
}

} // namespace

RayHit castRay(const Map& map, double px, double py, double dx, double dy,
               double maxDist) {
    RayHit out;

    int mapX = static_cast<int>(std::floor(px));
    int mapY = static_cast<int>(std::floor(py));

    // Distance the ray travels to cross one full cell in each axis. The
    // divide-by-zero case resolves to infinity, which is exactly right: a
    // ray parallel to an axis never crosses that axis's grid lines.
    const double deltaX = (dx == 0.0) ? 1e30 : std::abs(1.0 / dx);
    const double deltaY = (dy == 0.0) ? 1e30 : std::abs(1.0 / dy);

    int stepX, stepY;
    double sideDistX, sideDistY;

    if (dx < 0.0) {
        stepX = -1;
        sideDistX = (px - mapX) * deltaX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0 - px) * deltaX;
    }
    if (dy < 0.0) {
        stepY = -1;
        sideDistY = (py - mapY) * deltaY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0 - py) * deltaY;
    }

    bool sideY = false;
    // The map border is solid, so this terminates; the step cap is only a
    // guard against a malformed level or a degenerate direction vector.
    for (int steps = 0; steps < 256; ++steps) {
        // Advance whichever grid line is nearer.
        if (sideDistX < sideDistY) {
            sideDistX += deltaX;
            mapX += stepX;
            sideY = false;
        } else {
            sideDistY += deltaY;
            mapY += stepY;
            sideY = true;
        }

        if (map.isSolid(mapX, mapY)) {
            out.hit   = true;
            out.mapX  = mapX;
            out.mapY  = mapY;
            out.sideY = sideY;
            // Back off the one delta we just added to get the distance to
            // the face rather than to the far side of the cell. This is the
            // perpendicular distance already, which is why walls come out
            // flat without an extra cosine correction.
            out.perpDist = sideY ? (sideDistY - deltaY) : (sideDistX - deltaX);
            if (out.perpDist > maxDist) {
                out.hit = false;
                break;
            }
            // Where along the wall face the ray landed, for texturing.
            out.wallX = sideY ? px + out.perpDist * dx : py + out.perpDist * dy;
            out.wallX -= std::floor(out.wallX);
            return out;
        }
    }
    return out;
}

void Raycaster::render(Framebuffer& fb, const Map& map, const Player& player) {
    // Flat ceiling and floor. Textured floors need a second pass per pixel
    // rather than per column; the original didn't have them either.
    fb.fillRect(0, 0, kScreenW, kViewH / 2, kCeiling);
    fb.fillRect(0, kViewH / 2, kScreenW, kViewH - kViewH / 2, kFloor);

    const double px = player.x();
    const double py = player.y();

    for (int x = 0; x < kScreenW; ++x) {
        // Map the column to [-1, 1] across the camera plane.
        const double cameraX = 2.0 * x / kScreenW - 1.0;
        const double rayDirX = player.dirX() + player.planeX() * cameraX;
        const double rayDirY = player.dirY() + player.planeY() * cameraX;

        const RayHit hit = castRay(map, px, py, rayDirX, rayDirY);
        if (!hit.hit) {
            depth_[x] = 1e30;
            continue;
        }

        depth_[x] = hit.perpDist;

        // Projected wall height. Dividing the viewport height by distance is
        // the entire perspective transform: at one tile away a wall exactly
        // fills the view.
        const int lineH = static_cast<int>(kViewH / hit.perpDist);
        const int centre = kViewH / 2;
        const int y0 = centre - lineH / 2;
        const int y1 = y0 + lineH;

        const uint32_t base = kFlatColor[map.at(hit.mapX, hit.mapY).tex];
        fb.vline(x, std::max(y0, 0), std::min(y1, kViewH),
                 shade(base, bandedShade(hit.perpDist, hit.sideY)));
    }
}

} // namespace wolf
