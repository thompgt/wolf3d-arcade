#include "weapons.h"

#include <algorithm>
#include <cmath>

#include "enemy.h"
#include "map.h"
#include "player.h"

namespace wolf {
namespace {

// The tuning table. Every difference between the four weapons is here, and
// the four rows are deliberately readable side by side — the point of a
// table is that you can see at a glance that the chaingun trades accuracy
// and ammo for rate of fire, which you cannot see spread across four
// functions.
const WeaponTuning kTable[static_cast<size_t>(WeaponType::Count)] = {
    // name          rate  dmgN dmgF  range  spread  ammo  noise  auto  frames
    {  "KNIFE",      0.35,  28,  28,   1.5,  0.030,    0,   0.0,  true,   4  },
    {  "PISTOL",     0.42,  26,   9,  22.0,  0.020,    1,  14.0, false,   4  },
    {  "MACHINEGUN", 0.13,  22,   8,  24.0,  0.045,    1,  18.0,  true,   4  },
    {  "CHAINGUN",   0.07,  20,   7,  24.0,  0.075,    1,  22.0,  true,   4  },
};

// A shot that lands closer than this takes full damage; past it, damage
// falls off toward damageFar. Point-blank should be decisive.
constexpr double kFullDamageRange = 2.0;

} // namespace

const WeaponTuning& tuningFor(WeaponType w) {
    const int i = std::clamp(static_cast<int>(w), 0,
                             static_cast<int>(WeaponType::Count) - 1);
    return kTable[i];
}

double Weapons::nextRandom() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_ / 4294967296.0;
}

void Weapons::reset(uint32_t seed) {
    current_  = WeaponType::Pistol;
    cooldown_ = 0.0;
    animTimer_ = 0.0;
    frame_ = 0;
    flash_ = 0.0;
    // xorshift is a fixed point at zero: seeded with it, every "random"
    // number is zero forever.
    rng_ = seed ? seed : kFixedSeed;
}

bool Weapons::owns(WeaponType w, const Player& player) const {
    switch (w) {
    case WeaponType::Knife:      return true;   // never taken away
    case WeaponType::Pistol:     return true;   // issued at spawn
    case WeaponType::MachineGun: return player.hasMachineGun();
    case WeaponType::Chaingun:   return player.hasChaingun();
    default:                     return false;
    }
}

bool Weapons::select(WeaponType w, const Player& player) {
    if (w == current_) return true;
    if (!owns(w, player)) return false;
    // An empty firearm is not a choice, it is a way to be holding nothing
    // useful when a guard rounds the corner. The knife always answers.
    if (tuningFor(w).ammoUse > 0 && player.ammo() <= 0) return false;

    current_  = w;
    frame_    = 0;
    animTimer_ = 0.0;
    // Switching does not refund the cooldown; otherwise cycling weapons is a
    // free rate-of-fire increase.
    return true;
}

ShotResult Weapons::update(double dt, bool triggerDown, bool triggerPressed,
                           Map& map, Player& player, Enemies& enemies) {
    ShotResult out;

    if (cooldown_ > 0.0) cooldown_ -= dt;
    if (flash_ > 0.0) flash_ = std::max(flash_ - dt * 12.0, 0.0);

    const WeaponTuning& t = tuningFor(current_);

    // Advance the fire animation independently of the trigger, so a shot
    // always plays out even if the player lets go mid-swing.
    if (frame_ > 0) {
        animTimer_ += dt;
        const double frameTime = t.fireInterval / t.fireFrames;
        if (animTimer_ >= frameTime) {
            animTimer_ = 0.0;
            if (++frame_ >= t.fireFrames) frame_ = 0;
        }
    }

    // An automatic weapon also honours the latch, so a tap too short to be
    // held across a tick boundary still fires one round rather than nothing.
    const bool wantsToFire = t.automatic ? (triggerDown || triggerPressed)
                                         : triggerPressed;
    if (!wantsToFire || cooldown_ > 0.0 || player.isDead()) return out;

    if (t.ammoUse > 0 && player.ammo() < t.ammoUse) {
        // Out of ammo: report it once per press rather than every tick, so
        // the HUD click does not become a buzz.
        out.dryFired = triggerPressed;
        // Falling back to the knife automatically would take the decision
        // away from the player at the worst possible moment.
        return out;
    }

    // --- the shot ----------------------------------------------------
    if (t.ammoUse > 0) player.useAmmo(t.ammoUse);
    out.fired = true;

    cooldown_  = t.fireInterval;
    frame_     = 1;
    animTimer_ = 0.0;
    flash_     = (t.ammoUse > 0) ? 1.0 : 0.0;   // a knife has no muzzle

    // Spread is symmetric about the crosshair, so a weapon is inaccurate
    // rather than biased.
    const double off = (nextRandom() * 2.0 - 1.0) * t.spread;
    const double c = std::cos(off), s = std::sin(off);
    const double dx = player.dirX() * c - player.dirY() * s;
    const double dy = player.dirX() * s + player.dirY() * c;

    // Gunfire carries before the bullet is resolved: a guard who is about to
    // be shot at should be woken by the shot whether or not it connects.
    if (t.noiseRadius > 0.0)
        enemies.alertToNoise(map, player.x(), player.y(), t.noiseRadius);

    const int target = enemies.traceHit(map, player.x(), player.y(), dx, dy,
                                        t.range);
    if (target < 0) return out;

    const Enemy& e = enemies.all()[static_cast<size_t>(target)];
    const double ex = e.x - player.x(), ey = e.y - player.y();
    const double dist = std::sqrt(ex * ex + ey * ey);

    const double far = std::clamp((dist - kFullDamageRange) /
                                      std::max(t.range - kFullDamageRange, 1e-6),
                                  0.0, 1.0);
    const int dmg = static_cast<int>(
        t.damageNear + (t.damageFar - t.damageNear) * far);

    out.hit   = true;
    out.score = enemies.applyDamage(target, std::max(dmg, 1));
    out.killed = out.score > 0;
    if (out.score > 0) player.addScore(out.score);
    return out;
}

} // namespace wolf
