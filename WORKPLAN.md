# WORKPLAN

Build order for Wolf3D Arcade. Each phase ends in a **playable, committed
state** — the game runs after every phase, it just does less. Phases are
sequenced so that each one is visible on screen, which keeps regressions
obvious.

Legend: `[ ]` todo · `[~]` in progress · `[x]` done

---

## Phase 0 — Scaffold `[x]`

Get a window on screen with a real frame loop, so every later phase has
something to draw into.

- [x] Repo, `.gitignore`, README, this workplan
- [x] `build.bat` + `Makefile` (MinGW UCRT64, static link)
- [x] `Framebuffer` — 320x200 ARGB buffer, `clear` / `put` / `vline` / `fillRect`
- [x] `Platform` — Win32 window, `StretchDIBits` blit, key + raw-mouse input
- [x] Fixed 60Hz timestep with accumulator (`src/main.cpp`)
- [x] `Game` state machine shell (Title / Playing / Dead / LevelDone)

**Done when:** `build.bat run` opens a window, Enter moves from title to a
flat ceiling/floor/status-bar view, Escape quits.

---

## Phase 1 — Map and movement `[x]`

The raycaster, untextured. This is the load-bearing phase; get the math
right here and everything after it is decoration.

- [x] `game/map.h|cpp` — tile grid, one hand-authored 48x48 level (cell
      block, guard room, storeroom, mess hall, courtyard, armory, exit hall)
- [x] Tile taxonomy: solid wall (with texture id), door, locked door,
      pushwall, exit switch, plus a spawn list for actors/pickups/decor
- [x] `game/player.h|cpp` — position/direction/camera-plane, forward+strafe
      movement, keyboard turn, mouselook from `Platform::mouseDX()`
- [x] Collision: circle-vs-grid, axis-separated so sliding along walls feels
      right rather than sticking
- [x] `render/raycast.cpp` — DDA grid traversal, perpendicular distance
      (avoids fisheye), per-column wall height, per-column depth buffer
- [x] Flat-colour walls, darker on edge-on faces, 16-band distance shading
- [x] Debug overlay: toggleable top-down minimap (`M`) with player facing
- [x] `core/bmp.cpp` + `F12` — dump the real framebuffer, since compositor
      capture of a direct-blit window returns stale frames

Level authored at 48x48 rather than the original's 64x64: a hand-written
floor plan that size stays legible in the source, and the level fills it
rather than leaving a third of the grid as dead space.

**Done when:** you can walk the whole map, walls look solid, no fisheye
warp, and you cannot clip through geometry. ✔ verified by driving the built
game and dumping its own frames.

---

## Phase 2 — Procedural textures `[x]`

No asset files anywhere: every texture is generated into a 64x64 buffer at
startup.

- [x] `render/textures.h|cpp` — seeded xorshift + tiling value noise, so
      builds are byte-for-byte reproducible and textures repeat seamlessly
- [x] Wall set: running-bond brick, blue ashlar, knotted wood, mossy stone,
      riveted steel
- [x] Door texture (barred window, recessed panel) + door-frame jamb
- [x] Exit-switch texture with a red lever
- [x] Textured wall columns: `texX` from the hit fraction with correct
      mirroring, `texY` stepped from the *unclamped* column top
- [x] 16-band distance shading (banded, not smooth — a smooth ramp reads as
      modern fog rather than palette shifting)
- [x] Flat texture-atlas debug view (`T`)

**Done when:** every wall in the level is textured and reads clearly at
distance without shimmering. ✔

---

## Phase 3 — Doors, pushwalls, keys `[x]`

- [x] Sliding door: opening/open/closing/closed states, auto-close timer,
      refuses to close while the player stands in the doorway
- [x] Doors raycast at the tile midline, texture sliding with the panel
- [x] Jamb texture on the reveal, so a recessed door reads as recessed
- [x] Locked doors requiring gold/silver keys, closed to enemies too
- [x] Secret pushwalls: `Use` shoves the box two cells, off the tile grid,
      tested against rays and collision as a moving AABB
- [x] Exit switch → `LevelDone`
- [x] `--selftest`: 46 headless checks over the above

Doors and secrets are verified by `--selftest` rather than by playing.
Reaching the pushwall in game means passing two locked doors, so a scripted
keystroke run cannot get there, and a screenshot could not prove the secret
settled exactly two tiles away in any case.

**Done when:** doors open on `Use` and on enemy approach, locked doors
refuse without the key, and the secret is findable. ✔

---

## Phase 4 — Sprites and pickups `[x]`

- [x] `render/sprites.cpp` — billboard projection, clipped per column
      against the depth buffer the raycaster writes
- [x] Back-to-front sort by squared distance
- [x] Transparency via the DIB's unused top byte, with column scaling
- [x] `render/sprite_set.cpp` — 12 procedurally drawn sprites
- [x] Pickups: first aid, ammo clip, treasure ×4, gold key, silver key,
      machine gun, chaingun
- [x] Scenery: lamps and tables, blocking via a circle test rather than a
      tile, since a solid tile would also stop rays
- [x] Sprite atlas added to the debug view (`T` now cycles walls → sprites)
- [x] 17 more self-test checks (63 total)

**Done when:** sprites are correctly occluded by walls, scale with distance,
and pickups apply their effect and vanish on touch. ✔

---

## Phase 5 — Enemy AI `[x]`

The core ask. Guards that patrol, notice you, chase, and shoot back.

- [x] `game/enemy.h|cpp` — actor struct: pos, angle, health, state, timer,
      patrol direction, target
- [x] State machine: `Stand → Path → Chase → Shoot → Pain → Die`, each with
      its own animation frames and durations
- [x] Detection: FOV cone + line-of-sight raycast, plus a noise trigger so
      firing a gun wakes nearby guards (as in the original)
- [x] Pathing: grid-following patrol along waypoint tiles; chase uses the
      classic 8-direction "try preferred axis, then the other" move
- [x] **Shoot action:** stop, telegraph frame, hitscan at the player with
      distance- and movement-dependent hit chance; damage scales with range
- [x] Pain state on hit interrupts the attack (gives the player pressure
      relief and makes the chaingun feel powerful)
- [x] Death animation → corpse sprite, no longer blocks movement
- [x] `render/actor_sprites.cpp` — procedurally drawn guard: 8 view angles ×
      {walk ×4, stand, shoot ×3, pain, die ×5}, built from parametric
      head/torso/legs/gun shapes
- [x] Two enemy types: **Guard** (pistol, fast, weak) and **SS** (machine
      gun burst, tanky) — same machine, different tuning table
- [x] 31 more self-test checks (94 total), including a determinism check —
      the AI's randomness comes from a seeded xorshift rather than `rand()`,
      so a failing AI test reproduces exactly instead of being flaky

`Billboard` carries a `const Sprite*` rather than a sprite index, because an
enemy's frame depends on its facing relative to the camera, its animation
and its state — an index would only mean something qualified by which set it
indexed, and the renderer has no reason to know about either set.

**Done when:** guards patrol on their own, spot you and shout, close
distance, take cover-less potshots that actually hurt, flinch when hit, and
die with a proper animation. ✔

---

## Phase 6 — Weapons `[x]`

- [x] `game/weapons.h|cpp` — tuning table for knife / pistol / machine gun /
      chaingun: rate of fire, damage, ammo use, spread
- [x] Hitscan resolution: nearest actor within the aim cone, wall-blocked
- [x] Weapon-switch on keys 1–4, gated on ownership + ammo
- [x] Bottom-centre weapon sprite, procedurally drawn, with fire animation
      and muzzle flash lighting the view
- [x] Ammo pool shared across firearms, knife always available
- [x] Gunfire wakes guards through open space; the knife is silent
- [x] 35 more self-test checks (129 total)

The flash lights the view for several frames rather than the one this plan
called for: at 60Hz a single-frame wash is gone before the eye registers it,
so the thing meant to sell the shot was invisible.

Two things surfaced only by driving the built game and dumping frames, and
neither is visible in source. The first pass at the art drew the fist as a
full-width block to the bottom of the frame, which swallowed every weapon it
was holding. And the muzzle flash ran off the top of the sprite on the
recoil frame, where the frame edge sliced it flat.

`Game` also had to move to the heap in `main`. It holds every generated
sprite by value and the weapon set pushed it past two megabytes; because a
compiler reserves a function's whole frame in its prologue, as a local it
blew the stack on entry to `main` — taking `--selftest` down with it, despite
that branch returning long before the object would have been constructed.

**Done when:** all four weapons feel distinct and the chaingun can stagger a
guard chain with pain-state lock. ✔

---

## Phase 7 — HUD and game states `[x]`

- [x] `render/font.cpp` — 8x8 bitmap digits + uppercase, written as binary
      literals so each glyph is legible in the table
- [x] Status bar: FLOOR / SCORE / LIVES / face / HEALTH / KEYS / weapon+ammo
- [x] Face portrait: reacts to health tier, looks left/right idly, grimaces
      on damage, gloats on a kill
- [x] Damage flash (red) and pickup flash (yellow) washes, sharing one
      `washRows` primitive with the muzzle flash
- [x] Title screen, death sequence, level-complete tally with kill, treasure
      and secret percentages
- [x] 19 more self-test checks (151 total)

Two departures from the plan above, both forced by the screen itself.

The bar is 320 pixels and the font is 8 wide, so a five-letter label costs
40 of them — seven labelled fields do not fit. The keys lost their caption,
since a gold key and a silver key need none, and the weapon's name became
the ammo slot's label rather than taking a slot of its own: the count
belongs to the weapon, so naming it there is free. The layout budget is now
asserted in the self-test rather than eyeballed, because the first attempt
ran FLOOR into SCORE and pushed the weapon off the right-hand edge.

`Map` also had to count its own secrets. `pushwalls_` holds only the ones
still in motion, so a tally derived from that list reports every secret as
un-found the instant it settles.

Driving the game turned up an input bug that had nothing to do with the HUD:
firing read only `down(Key::Fire)`, so a keystroke shorter than one 60Hz
tick was never down when a tick sampled it and the shot vanished — the exact
failure `Platform`'s sticky press latch exists to prevent, in the one place
that was not using it.

**Done when:** the screen reads as a Wolf3D screen at a glance. ✔ verified
by driving the built game and dumping its own frames — the title and the
status bar in play. The death and level-complete screens are covered by
their logic in `--selftest` rather than by a captured frame; scripted
keystrokes cannot reliably die on cue or reach the exit past two locked
doors.

---

## Phase 8 — Modern abilities layer `[ ]`

Non-canonical additions, kept on a separate input row so the classic feel is
untouched if they're ignored.

- [ ] Sprint with a stamina meter (`Shift`), drains and regenerates
- [ ] Dash (`Q`) — short i-frame burst along the movement vector, cooldown
- [ ] Grenade (`G`) — arcing thrown projectile, radius damage, wall bounce
- [ ] Slow-mo (`F`) — scales the sim timestep, drains a charge meter
- [ ] Ability cooldown pips drawn on the status bar

**Done when:** each ability has a visible cost, a cooldown, and a reason to
use it in a fight.

---

## Phase 9 — Polish `[ ]`

- [ ] Procedural audio via `waveOut`: pistol, chaingun, door, guard alert,
      death, pickup — square/noise waveforms, no asset files
- [ ] Difficulty tiers scaling enemy count, damage and reaction time
- [ ] Screen-melt transition between states
- [ ] README screenshots / GIF, controls table
- [ ] Second map if the first one plays well

---

## Conventions

- One phase = one branch is overkill here; commit **per file or per logical
  unit** and push immediately.
- No external dependencies, ever. If something needs an asset, generate it.
- Anything tunable (speeds, damage, timers) lives in a named constant table,
  not inline in logic.
- Keep `render/` free of game rules and `game/` free of pixel writes.
