#include "textures.h"

#include <cmath>

namespace wolf {
namespace {

// --- deterministic noise -------------------------------------------------

// xorshift32. Small, fast, and reproducible across compilers, which matters
// here because the textures are effectively checked-in art: the same seed
// must give the same wall on every machine.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}

    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    // Uniform in [0, 1).
    double unit() { return next() / 4294967296.0; }
    // Uniform in [-a, a].
    double sym(double a) { return (unit() * 2.0 - 1.0) * a; }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1)); }
};

// Hash a lattice point to a stable value in [0, 1). Wrapping the
// coordinates by the lattice period is what makes the noise tile: the
// right edge of the texture samples the same lattice column as the left.
double latticeValue(int x, int y, int period, uint32_t seed) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0;
}

// Smoothstep, so interpolated noise has no visible lattice creases.
double fade(double t) { return t * t * (3.0 - 2.0 * t); }

// Bilinear value noise at the given lattice period, tiling over 64 pixels.
double valueNoise(double x, double y, int period, uint32_t seed) {
    const double sx = x * period / kTexSize;
    const double sy = y * period / kTexSize;
    const int x0 = static_cast<int>(std::floor(sx));
    const int y0 = static_cast<int>(std::floor(sy));
    const double fx = fade(sx - x0);
    const double fy = fade(sy - y0);

    const double a = latticeValue(x0,     y0,     period, seed);
    const double b = latticeValue(x0 + 1, y0,     period, seed);
    const double c = latticeValue(x0,     y0 + 1, period, seed);
    const double d = latticeValue(x0 + 1, y0 + 1, period, seed);

    const double top = a + (b - a) * fx;
    const double bot = c + (d - c) * fx;
    return top + (bot - top) * fy;
}

// Two octaves is enough at 64x64; more just looks muddy at this scale.
double fbm(double x, double y, int period, uint32_t seed) {
    return valueNoise(x, y, period, seed) * 0.65 +
           valueNoise(x, y, period * 2, seed ^ 0x5bf03635u) * 0.35;
}

// --- colour helpers ------------------------------------------------------

uint8_t clamp8(double v) {
    if (v < 0.0) return 0;
    if (v > 255.0) return 255;
    return static_cast<uint8_t>(v);
}

// Scales a colour multiplicatively. Keeps hue, which is what you want for
// shading masonry: an additive tint washes out toward grey instead.
uint32_t scaleColor(uint32_t c, double f) {
    return rgb(clamp8(((c >> 16) & 0xFF) * f),
               clamp8(((c >> 8) & 0xFF) * f),
               clamp8((c & 0xFF) * f));
}

uint32_t mixColor(uint32_t a, uint32_t b, double t) {
    return rgb(clamp8(((a >> 16) & 0xFF) * (1 - t) + ((b >> 16) & 0xFF) * t),
               clamp8(((a >> 8) & 0xFF) * (1 - t) + ((b >> 8) & 0xFF) * t),
               clamp8((a & 0xFF) * (1 - t) + (b & 0xFF) * t));
}

// --- generators ----------------------------------------------------------

// Running bond brickwork: every other course is offset by half a brick,
// which is the single detail that stops a brick wall reading as a grid.
void makeBrick(Texture& t, uint32_t base, uint32_t mortar, uint32_t seed) {
    constexpr int kBrickH = 8;
    constexpr int kBrickW = 16;
    Rng rng(seed);

    // Pre-roll a per-brick tint so bricks vary course to course but stay
    // stable across the texture.
    double tint[kTexSize / kBrickH][kTexSize / kBrickW + 1];
    for (auto& row : tint)
        for (double& v : row) v = 0.82 + rng.unit() * 0.36;

    for (int y = 0; y < kTexSize; ++y) {
        const int course = y / kBrickH;
        const int offset = (course % 2) ? kBrickW / 2 : 0;
        const int yInBrick = y % kBrickH;

        for (int x = 0; x < kTexSize; ++x) {
            const int bx = (x + offset) % kTexSize;
            const int xInBrick = bx % kBrickW;

            // Mortar occupies the top row and the left column of each brick.
            if (yInBrick == 0 || xInBrick == 0) {
                const double n = fbm(x, y, 16, seed);
                t.set(x, y, scaleColor(mortar, 0.85 + n * 0.3));
                continue;
            }

            const int bi = bx / kBrickW;
            double f = tint[course][bi];
            // Grain within the brick face.
            f *= 0.88 + fbm(x, y, 24, seed + 7) * 0.24;
            // Lit top edge, shadowed bottom edge: cheap bevel that reads as
            // depth even at one pixel wide.
            if (yInBrick == 1) f *= 1.16;
            if (yInBrick == kBrickH - 1) f *= 0.80;

            t.set(x, y, scaleColor(base, f));
        }
    }
}

// Large ashlar blocks with a chiselled bevel; coarser and colder than brick.
void makeBlockStone(Texture& t, uint32_t base, uint32_t seam, uint32_t seed) {
    constexpr int kBlockH = 16;
    constexpr int kBlockW = 32;
    Rng rng(seed);
    double tint[kTexSize / kBlockH][kTexSize / kBlockW + 1];
    for (auto& row : tint)
        for (double& v : row) v = 0.86 + rng.unit() * 0.28;

    for (int y = 0; y < kTexSize; ++y) {
        const int course = y / kBlockH;
        const int offset = (course % 2) ? kBlockW / 2 : 0;
        const int yIn = y % kBlockH;

        for (int x = 0; x < kTexSize; ++x) {
            const int bx = (x + offset) % kTexSize;
            const int xIn = bx % kBlockW;

            if (yIn == 0 || xIn == 0) {
                t.set(x, y, scaleColor(seam, 0.8 + fbm(x, y, 12, seed) * 0.4));
                continue;
            }

            double f = tint[course][bx / kBlockW];
            f *= 0.9 + fbm(x, y, 8, seed + 3) * 0.2;
            // Two-pixel bevel on the top/left, one on the bottom/right.
            if (yIn <= 2 || xIn <= 2) f *= 1.12;
            if (yIn >= kBlockH - 2 || xIn >= kBlockW - 2) f *= 0.82;
            t.set(x, y, scaleColor(base, f));
        }
    }
}

// Vertical planks: grain runs along the plank, knots are darker whorls.
void makeWood(Texture& t, uint32_t base, uint32_t seed) {
    constexpr int kPlankW = 16;
    Rng rng(seed);

    struct Knot { double x, y, r; };
    Knot knots[3];
    for (Knot& k : knots)
        k = {rng.unit() * kTexSize, rng.unit() * kTexSize, 3.0 + rng.unit() * 3.0};

    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const int xIn = x % kPlankW;

            // Plank seam: dark groove with a highlight on the far side.
            if (xIn == 0) { t.set(x, y, scaleColor(base, 0.38)); continue; }
            if (xIn == 1) { t.set(x, y, scaleColor(base, 1.14)); continue; }

            // Grain: noise stretched hard along y so it reads as long fibres.
            const double grain = fbm(x * 3.0, y * 0.35, 16, seed);
            double f = 0.82 + grain * 0.36;

            for (const Knot& k : knots) {
                const double dx = x - k.x;
                const double dy = (y - k.y) * 1.8;   // knots are elliptical
                const double d = std::sqrt(dx * dx + dy * dy);
                if (d < k.r) {
                    // Concentric rings, darkening toward the centre.
                    f *= 0.55 + 0.25 * std::sin(d * 2.4) + 0.2 * (d / k.r);
                }
            }
            t.set(x, y, scaleColor(base, f));
        }
    }
}

// Damp stone: block masonry with moss creeping out of the seams.
void makeMossyStone(Texture& t, uint32_t stone, uint32_t moss, uint32_t seed) {
    makeBlockStone(t, stone, scaleColor(stone, 0.55), seed);

    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            // Moss grows in patches, and more of it low down the wall.
            const double n = fbm(x, y, 6, seed ^ 0xA53Fu);
            const double gravity = 0.25 + 0.75 * (y / static_cast<double>(kTexSize));
            const double amount = (n * gravity - 0.34) * 2.6;
            if (amount <= 0.0) continue;
            const double a = amount > 0.85 ? 0.85 : amount;
            const double speck = 0.8 + fbm(x * 2.0, y * 2.0, 24, seed + 11) * 0.5;
            t.set(x, y, mixColor(t.at(x, y), scaleColor(moss, speck), a));
        }
    }
}

// Riveted steel plating: horizontal seams, rivets, faint vertical brushing.
void makeSteel(Texture& t, uint32_t base, uint32_t seed) {
    constexpr int kPlateH = 21;

    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const int yIn = y % kPlateH;
            // Brushed finish: high-frequency in x, smeared in y.
            double f = 0.90 + fbm(x * 4.0, y * 0.4, 32, seed) * 0.20;
            // Plates are lit at the top and fall off toward the seam.
            f *= 1.06 - 0.16 * (yIn / static_cast<double>(kPlateH));
            if (yIn == 0)      f *= 0.52;   // seam
            if (yIn == 1)      f *= 1.18;   // catch light below the seam
            t.set(x, y, scaleColor(base, f));
        }
    }

    // Rivets along each seam.
    for (int py = 0; py < kTexSize; py += kPlateH) {
        for (int px = 5; px < kTexSize; px += 13) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx * dx + dy * dy > 2) continue;
                    const int x = (px + dx + kTexSize) % kTexSize;
                    const int y = (py + 4 + dy + kTexSize) % kTexSize;
                    // Lit on the upper-left, shadowed lower-right.
                    const double f = (dx <= 0 && dy <= 0) ? 1.45 : 0.72;
                    t.set(x, y, scaleColor(base, f));
                }
            }
        }
    }
}

// Elevator-style door: steel with a heavy frame and a barred window.
void makeDoor(Texture& t, uint32_t base, uint32_t seed) {
    makeSteel(t, base, seed);

    const uint32_t dark  = scaleColor(base, 0.45);
    const uint32_t light = scaleColor(base, 1.35);

    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const int edge = 4;
            // Border frame, bevelled so the door reads as inset.
            if (x < edge || x >= kTexSize - edge ||
                y < edge || y >= kTexSize - edge) {
                const bool lit = (x < edge && y < kTexSize - x) ||
                                 (y < edge && x < kTexSize - y);
                t.set(x, y, lit ? light : scaleColor(base, 0.75));
                continue;
            }

            // Window: a dark recess with three vertical bars.
            const bool inWindow = x >= 18 && x < 46 && y >= 12 && y < 30;
            if (inWindow) {
                const bool bar = ((x - 18) % 9) < 2;
                t.set(x, y, bar ? light : scaleColor(base, 0.18));
                continue;
            }
            // Recessed lower panel.
            if (x >= 14 && x < 50 && y >= 38 && y < 56) {
                const bool rim = x == 14 || x == 49 || y == 38 || y == 55;
                t.set(x, y, rim ? dark : scaleColor(t.at(x, y), 0.88));
            }
        }
    }
}

// The jamb a door slides into: plain vertical grooves, deliberately duller
// than the door itself so the eye goes to the moving part.
void makeDoorFrame(Texture& t, uint32_t base, uint32_t seed) {
    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            double f = 0.88 + fbm(x * 2.0, y * 0.5, 24, seed) * 0.18;
            const int g = x % 8;
            if (g == 0) f *= 0.58;
            if (g == 1) f *= 1.22;
            t.set(x, y, scaleColor(base, f));
        }
    }
}

// The level-exit lever: unmistakable, because missing it means wandering
// the map looking for a way out.
void makeExitSwitch(Texture& t, uint32_t base, uint32_t seed) {
    makeSteel(t, base, seed);

    constexpr int kPlateX0 = 20, kPlateX1 = 44;
    constexpr int kPlateY0 = 14, kPlateY1 = 50;

    for (int y = kPlateY0; y < kPlateY1; ++y) {
        for (int x = kPlateX0; x < kPlateX1; ++x) {
            const bool rim = x == kPlateX0 || x == kPlateX1 - 1 ||
                             y == kPlateY0 || y == kPlateY1 - 1;
            t.set(x, y, rim ? rgb(0x20, 0x20, 0x22) : rgb(0x3a, 0x3a, 0x40));
        }
    }

    // Red lever in the up (unpulled) position, with a lit bezel.
    for (int y = 18; y < 34; ++y)
        for (int x = 28; x < 36; ++x)
            t.set(x, y, (x == 28 || y == 18) ? rgb(0xff, 0x90, 0x70)
                                             : rgb(0xc4, 0x28, 0x1c));
    // Slot the lever travels in.
    for (int y = 34; y < 46; ++y)
        for (int x = 30; x < 34; ++x)
            t.set(x, y, rgb(0x12, 0x12, 0x14));
}

} // namespace

void TextureSet::generate() {
    makeBrick(tex_[TexBrick], rgb(0x9a, 0x8b, 0x7c), rgb(0x54, 0x50, 0x4a), 1u);
    makeBlockStone(tex_[TexBlueStone], rgb(0x4f, 0x63, 0xa6), rgb(0x24, 0x2e, 0x52), 2u);
    makeWood(tex_[TexWood], rgb(0x8c, 0x5c, 0x30), 3u);
    makeMossyStone(tex_[TexMoss], rgb(0x77, 0x7a, 0x72), rgb(0x46, 0x74, 0x33), 4u);
    makeSteel(tex_[TexSteel], rgb(0x8a, 0x90, 0x98), 5u);
    makeDoor(tex_[TexDoor], rgb(0x9a, 0x9e, 0xa6), 6u);
    makeDoorFrame(tex_[TexDoorFrame], rgb(0x6e, 0x72, 0x78), 7u);
    makeExitSwitch(tex_[TexExitSwitch], rgb(0x8a, 0x90, 0x98), 8u);
}

} // namespace wolf
