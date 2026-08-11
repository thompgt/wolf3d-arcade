#include "enemy.h"

#include <algorithm>
#include <cmath>

#include "../render/raycast.h"
#include "items.h"
#include "map.h"
#include "player.h"

namespace wolf {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Half-angle of the vision cone, as a cosine. 60 degrees either side: wide
// enough that walking across a guard's front is noticed, narrow enough that
// approaching from behind is a real tactic rather than a coin flip.
constexpr double kSightCos = 0.5;

// Inside this range a guard notices you regardless of facing — he has heard
// you, or you are simply too close to miss.
constexpr double kProximityWake = 1.6;

// Actor radius, for collision against the player and each other.
constexpr double kActorRadius = 0.30;

// Animation rates.
constexpr double kWalkFrameTime  = 0.16;
constexpr double kShootFrameTime = 0.12;
constexpr double kDieFrameTime   = 0.09;
constexpr double kPainTime       = 0.22;

// How long a guard holds a patrol heading before it may turn at a junction.
constexpr double kPatrolRepick = 0.35;

const EnemyTuning kGuardTuning{
    /*health*/        25,
    /*patrolSpeed*/   1.1,
    /*chaseSpeed*/    2.2,
    /*sightRange*/    16.0,
    /*shootRange*/    12.0,
    /*reactionTime*/  0.28,
    /*fireInterval*/  0.95,
    /*painChance*/    0.55,
    /*damageNear*/    12,
    /*damageFar*/     4,
    /*score*/         100,
};

const EnemyTuning kSSTuning{
    /*health*/        55,
    /*patrolSpeed*/   1.3,
    /*chaseSpeed*/    2.6,
    /*sightRange*/    18.0,
    /*shootRange*/    14.0,
    /*reactionTime*/  0.20,
    /*fireInterval*/  0.55,   // bursts noticeably faster than a guard
    /*painChance*/    0.28,   // and shrugs off most hits
    /*damageNear*/    18,
    /*damageFar*/     6,
    /*score*/         500,
};

} // namespace

const EnemyTuning& tuningFor(EnemyType type) {
    return type == EnemyType::SS ? kSSTuning : kGuardTuning;
}

double Enemies::nextRandom() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_ / 4294967296.0;
}

void Enemies::spawnFrom(const Map& map, uint32_t seed) {
    enemies_.clear();
    killed_ = 0;
    // Zero is xorshift's fixed point; seeded with it nothing is ever random.
    rng_ = seed ? seed : kFixedSeed;

    for (const Spawn& s : map.spawns()) {
        if (s.kind != SpawnKind::Guard && s.kind != SpawnKind::SS) continue;

        Enemy e;
        e.type   = (s.kind == SpawnKind::SS) ? EnemyType::SS : EnemyType::Guard;
        e.x      = s.tx + 0.5;
        e.y      = s.ty + 0.5;
        e.health = tuningFor(e.type).health;
        // Guards start standing and facing a direction their patrol will
        // use, so the first thing they do is not a pirouette.
        e.state  = EnemyState::Stand;
        e.angle  = 0.0;
        enemies_.push_back(e);
    }
}

bool Enemies::canSee(const Map& map, const Enemy& e,
                     double px, double py) const {
    const double dx = px - e.x;
    const double dy = py - e.y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist > tuningFor(e.type).sightRange) return false;
    if (dist < 1e-6) return true;

    const double nx = dx / dist;
    const double ny = dy / dist;

    // Outside the cone, and not close enough to notice regardless.
    if (dist > kProximityWake) {
        const double facing = std::cos(e.angle) * nx + std::sin(e.angle) * ny;
        if (facing < kSightCos) return false;
    }

    // Line of sight, cast with the same traversal the renderer uses — so an
    // enemy can never see through something you can see, or vice versa.
    const RayHit hit = castRayDynamic(map, e.x, e.y, nx, ny, dist);
    return !hit.hit || hit.perpDist >= dist - 1e-6;
}

bool Enemies::freeAt(const Enemy& self, const Map& map, const Items& items,
                     double x, double y) const {
    const double r = kActorRadius;
    const int x0 = static_cast<int>(std::floor(x - r));
    const int x1 = static_cast<int>(std::floor(x + r));
    const int y0 = static_cast<int>(std::floor(y - r));
    const int y1 = static_cast<int>(std::floor(y + r));

    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (map.blocksMovement(tx, ty)) return false;

    if (map.hitsPushwall(x, y, r)) return false;
    if (items.blocks(x, y, r)) return false;

    // Guards do not walk through each other. Corpses are walked over, which
    // is what stops a doorway silting up with bodies.
    for (const Enemy& o : enemies_) {
        if (&o == &self) continue;
        if (o.state == EnemyState::Dead || o.state == EnemyState::Die) continue;
        const double ox = x - o.x, oy = y - o.y;
        const double reach = r + kActorRadius;
        if (ox * ox + oy * oy < reach * reach) return false;
    }
    return true;
}

void Enemies::moveToward(Map& map, Enemy& e, double dx, double dy,
                         double dist, const Items& items) {
    if (dist <= 0.0) return;

    // A guard walking into a door opens it and waits for it rather than
    // treating it as a wall — otherwise patrol routes stop at every doorway.
    const int aheadX = static_cast<int>(std::floor(e.x + dx * 0.6));
    const int aheadY = static_cast<int>(std::floor(e.y + dy * 0.6));
    if (map.isDoor(aheadX, aheadY)) map.openDoorFor(aheadX, aheadY);

    // Axis-separated, same as the player: a guard that catches a corner
    // slides along it instead of stopping dead and looking broken.
    if (freeAt(e, map, items, e.x + dx * dist, e.y)) e.x += dx * dist;
    if (freeAt(e, map, items, e.x, e.y + dy * dist)) e.y += dy * dist;
}

void Enemies::update(double dt, Map& map, Player& player, const Items& items) {
    const double px = player.x();
    const double py = player.y();

    for (Enemy& e : enemies_) {
        if (e.state == EnemyState::Dead) continue;

        const EnemyTuning& t = tuningFor(e.type);
        e.stateTimer += dt;
        e.animTimer  += dt;
        if (e.fireTimer > 0.0) e.fireTimer -= dt;

        const double dx = px - e.x;
        const double dy = py - e.y;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double nx = (dist > 1e-6) ? dx / dist : 0.0;
        const double ny = (dist > 1e-6) ? dy / dist : 0.0;

        switch (e.state) {
        case EnemyState::Die:
            if (e.animTimer >= kDieFrameTime) {
                e.animTimer = 0.0;
                if (++e.frame >= ActorSprites::kDieFrames - 1) {
                    e.frame = ActorSprites::kDieFrames - 1;
                    e.state = EnemyState::Dead;
                }
            }
            break;

        case EnemyState::Pain:
            if (e.stateTimer >= kPainTime) {
                e.state = EnemyState::Chase;
                e.stateTimer = 0.0;
                e.frame = 0;
            }
            break;

        case EnemyState::Stand:
            if (canSee(map, e, px, py)) {
                e.alerted = true;
                // React, do not teleport into action. The delay is what
                // gives the player the half-second that makes getting the
                // first shot off feel earned.
                if (e.stateTimer >= t.reactionTime) {
                    e.state = EnemyState::Chase;
                    e.stateTimer = 0.0;
                }
            } else if (e.stateTimer > 1.2) {
                // Nothing to look at: walk the beat.
                e.state = EnemyState::Path;
                e.stateTimer = 0.0;
            }
            break;

        case EnemyState::Path: {
            if (canSee(map, e, px, py)) {
                e.alerted = true;
                e.state = EnemyState::Chase;
                e.stateTimer = 0.0;
                e.frame = 0;
                break;
            }

            const double before = e.x + e.y;
            e.angle = std::atan2(static_cast<double>(e.patrolDY),
                                 static_cast<double>(e.patrolDX));
            moveToward(map, e, e.patrolDX, e.patrolDY, t.patrolSpeed * dt, items);

            // Blocked, or time to consider a turn: pick a new heading. Four
            // candidates, preferring to carry straight on so patrols look
            // purposeful rather than drunk.
            const bool stuck = std::abs((e.x + e.y) - before) < 1e-9;
            if (stuck || e.stateTimer > kPatrolRepick * 8.0) {
                const int dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                int best = -1;
                double bestScore = -1.0;
                for (int i = 0; i < 4; ++i) {
                    const double tx = e.x + dirs[i][0] * 0.7;
                    const double ty = e.y + dirs[i][1] * 0.7;
                    if (!freeAt(e, map, items, tx, ty)) continue;
                    double score = nextRandom();
                    if (dirs[i][0] == e.patrolDX && dirs[i][1] == e.patrolDY)
                        score += 0.9;                       // prefer straight on
                    if (dirs[i][0] == -e.patrolDX && dirs[i][1] == -e.patrolDY)
                        score -= 0.8;                       // avoid doubling back
                    if (score > bestScore) { bestScore = score; best = i; }
                }
                if (best >= 0) {
                    e.patrolDX = dirs[best][0];
                    e.patrolDY = dirs[best][1];
                }
                e.stateTimer = 0.0;
            }

            if (e.animTimer >= kWalkFrameTime) {
                e.animTimer = 0.0;
                e.frame = (e.frame + 1) % ActorSprites::kWalkFrames;
            }
            break;
        }

        case EnemyState::Chase: {
            e.angle = std::atan2(ny, nx);
            const bool sees = canSee(map, e, px, py);

            if (sees && dist <= t.shootRange && e.fireTimer <= 0.0) {
                e.state = EnemyState::Shoot;
                e.stateTimer = 0.0;
                e.animTimer = 0.0;
                e.frame = 0;
                break;
            }

            // Close the distance, but stop short so a guard does not end up
            // standing inside the player.
            if (dist > 0.9) moveToward(map, e, nx, ny, t.chaseSpeed * dt, items);

            if (e.animTimer >= kWalkFrameTime) {
                e.animTimer = 0.0;
                e.frame = (e.frame + 1) % ActorSprites::kWalkFrames;
            }
            break;
        }

        case EnemyState::Shoot: {
            e.angle = std::atan2(ny, nx);

            if (e.animTimer >= kShootFrameTime) {
                e.animTimer = 0.0;
                ++e.frame;

                // The shot resolves on the flash frame, so what you see and
                // what hits you are the same event.
                if (e.frame == 1) {
                    const bool stillVisible = canSee(map, e, px, py);
                    // Accuracy falls off with range, and a moving target is
                    // harder — which makes strafing a real defence rather
                    // than a cosmetic one.
                    double chance = 0.78 - dist * 0.035;
                    if (player.isMoving()) chance -= 0.16;
                    chance = std::clamp(chance, 0.10, 0.85);

                    if (stillVisible && nextRandom() < chance) {
                        const double far = std::clamp(dist / t.shootRange, 0.0, 1.0);
                        const int dmg = static_cast<int>(
                            t.damageNear + (t.damageFar - t.damageNear) * far);
                        player.damage(std::max(dmg, 1));
                    }
                }

                if (e.frame >= ActorSprites::kShootFrames) {
                    e.frame = 0;
                    e.fireTimer = t.fireInterval;
                    e.state = EnemyState::Chase;
                    e.stateTimer = 0.0;
                }
            }
            break;
        }

        case EnemyState::Dead:
            break;
        }
    }
}

int Enemies::applyDamage(int index, int amount) {
    if (index < 0 || index >= static_cast<int>(enemies_.size())) return 0;
    Enemy& e = enemies_[static_cast<size_t>(index)];
    if (e.state == EnemyState::Dead || e.state == EnemyState::Die) return 0;

    e.health -= amount;
    e.alerted = true;

    if (e.health <= 0) {
        e.state = EnemyState::Die;
        e.frame = 0;
        e.animTimer = 0.0;
        e.stateTimer = 0.0;
        ++killed_;
        return tuningFor(e.type).score;
    }

    // Being hit staggers, but not reliably, and an SS shrugs off most of it.
    // Guaranteed pain would let a fast weapon lock an enemy in place
    // forever; never staggering would make the chaingun feel like a hose.
    if (nextRandom() < tuningFor(e.type).painChance) {
        e.state = EnemyState::Pain;
        e.stateTimer = 0.0;
        e.frame = 0;
    } else if (e.state == EnemyState::Stand || e.state == EnemyState::Path) {
        // Shot from behind without being staggered: still turn and fight.
        e.state = EnemyState::Chase;
        e.stateTimer = 0.0;
    }
    return 0;
}

void Enemies::alertToNoise(const Map& map, double x, double y, double radius) {
    for (Enemy& e : enemies_) {
        if (e.state == EnemyState::Dead || e.state == EnemyState::Die) continue;
        if (e.state == EnemyState::Chase || e.state == EnemyState::Shoot) continue;

        const double dx = x - e.x, dy = y - e.y;
        const double dist = std::sqrt(dx * dx + dy * dy);
        if (dist > radius) continue;

        // Sound does not go through walls. Without this check every shot
        // wakes the entire level and there is no reason to ever stop firing.
        const double nx = (dist > 1e-6) ? dx / dist : 0.0;
        const double ny = (dist > 1e-6) ? dy / dist : 1.0;
        const RayHit hit = castRayDynamic(map, e.x, e.y, nx, ny, dist);
        if (hit.hit && hit.perpDist < dist - 1e-6) continue;

        e.alerted = true;
        e.state = EnemyState::Chase;
        e.stateTimer = 0.0;
    }
}

int Enemies::traceHit(const Map& map, double px, double py,
                      double dx, double dy, double range) const {
    // A wall between you and a guard stops the bullet, so the wall distance
    // is the real limit on the shot.
    const RayHit wall = castRayDynamic(map, px, py, dx, dy, range);
    const double limit = wall.hit ? std::min(wall.perpDist, range) : range;

    int best = -1;
    double bestDist = limit;

    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Enemy& e = enemies_[i];
        if (e.state == EnemyState::Dead || e.state == EnemyState::Die) continue;

        // Distance from the enemy centre to the ray, and how far along the
        // ray the nearest approach happens.
        const double ex = e.x - px, ey = e.y - py;
        const double along = ex * dx + ey * dy;
        if (along <= 0.0 || along > bestDist) continue;

        const double perp = std::abs(ex * dy - ey * dx);
        if (perp > kActorRadius + 0.06) continue;

        best = static_cast<int>(i);
        bestDist = along;
    }
    return best;
}

bool Enemies::blocks(double x, double y, double r) const {
    const double reach = r + kActorRadius;
    for (const Enemy& e : enemies_) {
        if (e.state == EnemyState::Dead || e.state == EnemyState::Die) continue;
        const double dx = x - e.x, dy = y - e.y;
        if (dx * dx + dy * dy < reach * reach) return true;
    }
    return false;
}

int Enemies::alive() const {
    int n = 0;
    for (const Enemy& e : enemies_)
        if (e.state != EnemyState::Dead && e.state != EnemyState::Die) ++n;
    return n;
}

void Enemies::appendBillboards(std::vector<Billboard>& out, const Player& player,
                               const ActorSprites& art) const {
    for (const Enemy& e : enemies_) {
        ActorPose pose = ActorPose::Stand;
        switch (e.state) {
        case EnemyState::Stand: pose = ActorPose::Stand; break;
        case EnemyState::Path:
        case EnemyState::Chase: pose = ActorPose::Walk;  break;
        case EnemyState::Shoot: pose = ActorPose::Shoot; break;
        case EnemyState::Pain:  pose = ActorPose::Pain;  break;
        case EnemyState::Die:
        case EnemyState::Dead:  pose = ActorPose::Die;   break;
        }

        // Which way he appears to face depends on where he is seen from,
        // not just where he is looking.
        const double viewAngle = std::atan2(e.y - player.y(), e.x - player.x());
        const int bucket = viewBucket(e.angle, viewAngle);

        out.push_back(Billboard{e.x, e.y,
                                &art.frame(e.type, pose, bucket, e.frame)});
    }
}

} // namespace wolf
