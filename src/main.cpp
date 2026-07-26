// Entry point and frame loop.
//
// The simulation runs on a fixed 60Hz timestep with an accumulator, so game
// logic behaves identically no matter how fast the machine draws frames.
// Rendering happens once per pass through the loop, at whatever rate the
// display manages.
#include "core/config.h"
#include "core/framebuffer.h"
#include "game/game.h"
#include "platform/win32_app.h"

using namespace wolf;

int main() {
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

        while (accumulator >= kTickDT) {
            game.update(platform);
            accumulator -= kTickDT;
        }

        if (game.wantsQuit()) break;

        game.render(fb);
        platform.present(fb);
    }

    platform.shutdown();
    return 0;
}
