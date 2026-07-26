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

## Phase 4 — Sprites and pickups `[ ]`

- [ ] `render/sprites.cpp` — billboard projection, per-column depth buffer
      written by the raycaster so sprites clip correctly behind walls
- [ ] Back-to-front sort by squared distance
- [ ] Transparent-colour blitting with column scaling
- [ ] Pickups: health (dog food / first aid), ammo clip, treasure ×4,
      gold key, silver key, machine gun, chaingun
- [ ] Static decor: lamps, tables, barrels — some blocking, some not

**Done when:** sprites are correctly occluded by walls, scale with distance,
and pickups apply their effect and vanish on touch.

---

## Phase 5 — Enemy AI `[ ]`

The core ask. Guards that patrol, notice you, chase, and shoot back.

- [ ] `game/enemy.h|cpp` — actor struct: pos, angle, health, state, timer,
      patrol direction, target
- [ ] State machine: `Stand → Path → Chase → Shoot → Pain → Die`, each with
      its own animation frames and durations
- [ ] Detection: FOV cone + line-of-sight raycast, plus a noise trigger so
      firing a gun wakes nearby guards (as in the original)
- [ ] Pathing: grid-following patrol along waypoint tiles; chase uses the
      classic 8-direction "try preferred axis, then the other" move
- [ ] **Shoot action:** stop, telegraph frame, hitscan at the player with
      distance- and movement-dependent hit chance; damage scales with range
- [ ] Pain state on hit interrupts the attack (gives the player pressure
      relief and makes the chaingun feel powerful)
- [ ] Death animation → corpse sprite, no longer blocks movement
- [ ] `render/actor_sprites.cpp` — procedurally drawn guard: 8 view angles ×
      {walk ×4, stand, shoot ×3, pain, die ×5}, built from parametric
      head/torso/legs/gun shapes
- [ ] Two enemy types: **Guard** (pistol, fast, weak) and **SS** (machine
      gun burst, tanky) — same machine, different tuning table

**Done when:** guards patrol on their own, spot you and shout, close
distance, take cover-less potshots that actually hurt, flinch when hit, and
die with a proper animation.

---

## Phase 6 — Weapons `[ ]`

- [ ] `game/weapons.h|cpp` — tuning table for knife / pistol / machine gun /
      chaingun: rate of fire, damage, ammo use, spread
- [ ] Hitscan resolution: nearest actor within the aim cone, wall-blocked
- [ ] Weapon-switch on keys 1–4, gated on ownership + ammo
- [ ] Bottom-centre weapon sprite, procedurally drawn, with fire animation
      and muzzle flash lighting the view for one frame
- [ ] Ammo pool shared across firearms, knife always available

**Done when:** all four weapons feel distinct and the chaingun can stagger a
guard chain with pain-state lock.

---

## Phase 7 — HUD and game states `[ ]`

- [ ] `render/font.cpp` — procedurally built 8x8 bitmap digits + uppercase
- [ ] Status bar: FLOOR / SCORE / LIVES / face / HEALTH / AMMO / KEYS / weapon
- [ ] Face portrait: reacts to health tier, looks left/right idly, grimaces
      on damage, gloats on a kill streak
- [ ] Damage flash (red) and pickup flash (yellow) palette washes
- [ ] Title screen, death sequence, level-complete tally with time/kill/secret
      percentages

**Done when:** the screen reads as a Wolf3D screen at a glance.

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
