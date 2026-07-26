// Headless checks for game logic that is impractical to verify by playing.
//
// Driving the built game with timed keystrokes works for anything a few
// steps from the spawn, but it falls apart quickly: the pushwall secret sits
// behind a gold-locked door whose key is behind a silver-locked door, and
// "hold W for 1100ms" is not a test, it is a guess that happens to work
// today. Enemy AI and weapons will make that worse.
//
// These run the real Map and raycaster with no window and no input, so they
// can assert on things a screenshot cannot show: that a secret settles
// exactly two tiles away, that a door refuses to close on the player, that
// a ray actually passes through an open doorway.
#pragma once

namespace wolf {

// Runs every check and reports to stdout and to selftest.txt (the release
// build is a Windows-subsystem binary with no console attached).
// Returns 0 if everything passed, 1 otherwise.
int runSelfTest();

} // namespace wolf
