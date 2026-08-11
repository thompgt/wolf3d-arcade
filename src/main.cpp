// Entry point and frame loop.
//
// The simulation runs on a fixed 60Hz timestep with an accumulator, so game
// logic behaves identically no matter how fast the machine draws frames.
// Rendering happens once per simulation tick: a pass that steps the world
// zero times would draw a pixel-identical frame, so it sleeps out the rest of
// the tick instead. That caps the loop at the 60Hz tick rate rather than
// letting it spin as fast as GDI will accept blits.
#include "core/bmp.h"
#include "core/config.h"
#include "core/framebuffer.h"
#include "game/game.h"
#include "platform/win32_app.h"
#include "selftest.h"

#include <cstring>
#include <exception>
#include <memory>

using namespace wolf;

int main(int argc, char** argv) {
    // Headless mode: no window, no input, just assertions on game logic.
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0) return runSelfTest();

    Platform platform;
    if (!platform.init("Wolf3D Arcade")) return 1;

    Framebuffer fb(kScreenW, kScreenH);

    // On the heap, and it has to be. Game holds every generated sprite by
    // value -- the actor set alone is 98 frames of 64x64x4 -- which puts it
    // over two megabytes. As a local it does not merely risk overflowing the
    // stack: the compiler reserves the whole frame in main's prologue, so
    // main blows the stack on entry and never reaches a line of this
    // function, including the --selftest branch above.
    auto game = std::make_unique<Game>();
    // Level parsing refuses to half-load: a ragged row or an unknown glyph
    // throws rather than shifting the whole level sideways. There is no
    // console behind the release build, so say so in a dialog and stop.
    try {
        game->init();
    } catch (const std::exception& e) {
        Platform::showError("Wolf3D Arcade", e.what());
        platform.shutdown();
        return 1;
    }

    double previous    = platform.now();
    double accumulator = 0.0;
    bool warnedAboutShot = false;

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

        bool ticked = false;
        while (accumulator >= kTickDT) {
            game->update(platform);
            // One keystroke must produce one pressed() edge, even when this
            // loop catches up on several ticks at once.
            platform.consumeEdges();
            accumulator -= kTickDT;
            ticked = true;
        }

        if (game->wantsQuit()) break;

        // Nothing simulated since the last present, so the frame would be
        // pixel-identical. Neither pump() nor StretchDIBits blocks, so without
        // this the loop would redraw the same image hundreds of times a second
        // and pin a core doing it. Sleep out the rest of the tick instead,
        // leaving a millisecond of slack so we wake before the boundary rather
        // than after it.
        if (!ticked) {
            const int ms = static_cast<int>((kTickDT - accumulator) * 1000.0) - 1;
            platform.sleepMs(ms);
            continue;
        }

        game->render(fb);

        // Dumped after render, before present, so it is exactly the frame
        // the player is about to see. Next to the exe rather than in the
        // working directory, which from a shortcut is wherever the shortcut
        // says and from an elevated shell is somewhere unwritable.
        if (wantShot && !writeBMP(exeRelativePath("shot.bmp"), fb) &&
            !warnedAboutShot) {
            warnedAboutShot = true;   // once, not once per keypress
            Platform::showError("Wolf3D Arcade",
                                "Could not write shot.bmp next to the "
                                "executable. Is the folder read-only?");
        }

        platform.present(fb);
    }

    platform.shutdown();
    return 0;
}
