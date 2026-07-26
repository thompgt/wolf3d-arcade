// Entry point and frame loop.
//
// The simulation runs on a fixed 60Hz timestep with an accumulator, so game
// logic behaves identically no matter how fast the machine draws frames.
// Rendering happens once per pass through the loop, at whatever rate the
// display manages.
#include "core/bmp.h"
#include "core/config.h"
#include "core/framebuffer.h"
#include "game/game.h"
#include "platform/win32_app.h"
#include "selftest.h"

#include <cstring>

using namespace wolf;

int main(int argc, char** argv) {
    // Headless mode: no window, no input, just assertions on game logic.
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0) return runSelfTest();

    Platform platform;
    if (!platform.init("Wolf3D Arcade")) return 1;

    Framebuffer fb(kScreenW, kScreenH);
    Game game;
    game.init();

    double previous    = platform.now();
    double accumulator = 0.0;

    while (platform.pump()) {
        const double current = platform.now();
        double elapsed = current - previous;
        previous = current;

        // Clamp so a breakpoint or a stalled window doesn't make the world
        // fast-forward through hundreds of ticks at once.
        if (elapsed > 0.25) elapsed = 0.25;
        accumulator += elapsed;

        // Sampled before the tick loop, which consumes input edges.
        const bool wantShot = platform.pressed(Key::Screenshot);

        while (accumulator >= kTickDT) {
            game.update(platform);
            // One keystroke must produce one pressed() edge, even when this
            // loop catches up on several ticks at once.
            platform.consumeEdges();
            accumulator -= kTickDT;
        }

        if (game.wantsQuit()) break;

        game.render(fb);

        // Dumped after render, before present, so it is exactly the frame
        // the player is about to see.
        if (wantShot) writeBMP("shot.bmp", fb);

        platform.present(fb);
    }

    platform.shutdown();
    return 0;
}
