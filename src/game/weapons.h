// The player's weapons: what they own, what they are holding, and what
// happens when they pull the trigger.
//
// All four weapons are the same machine with different numbers, exactly as
// the enemies are. A weapon is a fire interval, a damage band, a range, an
// aim cone and an ammo cost; "the chaingun" is a row in a table rather than
// a special case in the firing code.
//
// Firing is hitscan, resolved the frame the muzzle flashes, because that is
// what the original did and what makes the weapons feel immediate. The one
// piece of state that matters beyond the cooldown is whether the trigger was
// released: a semi-automatic weapon has to see a fresh press, and an
// automatic one does not.
#pragma once

#include <cstdint>

#include "../render/weapon_sprites.h"

namespace wolf {

class Map;
class Player;
class Enemies;

struct WeaponTuning {
    const char* name;
    double fireInterval;  // seconds between shots
    int    damageNear;
    int    damageFar;
    double range;         // tiles; beyond this the shot simply misses
    double spread;        // half-angle of the aim cone, radians
    int    ammoUse;       // per shot; the knife is free
    double noiseRadius;   // tiles of gunfire that wakes guards
    bool   automatic;     // does holding the trigger keep firing
    int    fireFrames;    // length of the fire animation
};

const WeaponTuning& tuningFor(WeaponType w);

// What one trigger pull did. Returned so the HUD and the audio layer can
// react to a shot without either of them re-deriving it from world state.
struct ShotResult {
    bool fired    = false;   // a round actually left the barrel
    bool hit      = false;
    bool killed   = false;
    int  score    = 0;
    bool dryFired = false;   // wanted to fire, had no ammo
};

class Weapons {
public:
    void reset();

    WeaponType current() const { return current_; }

    // Switching is gated on ownership and, for the firearms, on having a
    // round to put through them — being handed an empty chaingun by a
    // keypress mid-fight is worse than being told no.
    bool select(WeaponType w, const Player& player);
    bool owns(WeaponType w, const Player& player) const;

    // One tick.
    //
    // Both halves of the trigger are needed. `triggerDown` is the held
    // state, which is what an automatic weapon runs on. `triggerPressed` is
    // the platform's latched edge — a fresh press no tick has consumed yet —
    // and a semi-automatic weapon has to use that rather than deriving its
    // own edge from the held state: frames and ticks run at different rates,
    // so a tap shorter than a tick is never down when a tick samples it, and
    // deriving the edge here drops the shot entirely. The latch exists
    // precisely so one keystroke produces exactly one action.
    ShotResult update(double dt, bool triggerDown, bool triggerPressed,
                      Map& map, Player& player, Enemies& enemies);

    // Animation frame of the current weapon; 0 is at rest.
    int frame() const { return frame_; }
    bool isFiring() const { return frame_ > 0; }

    // Muzzle-flash intensity, 1 at the flash and falling to 0. The view is
    // lit from it for a few frames rather than exactly one — a single-frame
    // wash at 60Hz is gone before the eye registers it.
    double flash() const { return flash_; }

private:
    double nextRandom();

    WeaponType current_ = WeaponType::Pistol;
    double cooldown_   = 0.0;
    double animTimer_  = 0.0;
    int    frame_      = 0;
    double flash_      = 0.0;

    // Seeded, like the enemy AI: spread is random, and random spread from
    // rand() would make every weapon test a coin flip.
    uint32_t rng_ = 0x9E3779B9u;
};

} // namespace wolf
