// Enemy actors and their AI.
//
// One state machine per guard: Stand → Path → Chase → Shoot, with Pain and
// Die cutting in from any of them. That is the original's structure, and it
// holds up because each state answers exactly one question — where am I
// going, and am I allowed to shoot yet — rather than one blob of logic
// trying to decide everything every tick.
//
// Both enemy types run the same machine. Everything that distinguishes a
// guard from an SS trooper lives in a tuning table, so "tankier, fires in
// bursts, hits harder" is data rather than a second implementation that
// will drift out of step with the first.
#pragma once

#include <cstdint>
#include <vector>

#include "../render/actor_sprites.h"
#include "../render/sprites.h"

namespace wolf {

class Map;
class Player;
class Items;

enum class EnemyState : uint8_t {
    Stand,   // holding position, watching
    Path,    // patrolling
    Chase,   // closing on the player
    Shoot,   // stopped, firing
    Pain,    // staggered by a hit
    Die,     // playing the death animation
    Dead,    // a corpse; drawn, but no longer an actor
};

struct EnemyTuning {
    int    health;
    double patrolSpeed;
    double chaseSpeed;
    double sightRange;
    double shootRange;
    double reactionTime;   // delay between noticing you and acting
    double fireInterval;   // seconds between shots
    double painChance;     // odds a hit staggers rather than being shrugged off
    int    damageNear;
    int    damageFar;
    int    score;
};

const EnemyTuning& tuningFor(EnemyType type);

struct Enemy {
    double     x = 0.0, y = 0.0;
    double     angle = 0.0;
    EnemyType  type  = EnemyType::Guard;
    EnemyState state = EnemyState::Stand;

    int    health = 0;
    double stateTimer = 0.0;   // seconds spent in the current state
    double fireTimer  = 0.0;   // cooldown until the next shot is allowed
    double animTimer  = 0.0;
    int    frame = 0;

    // Patrol heading, as a grid direction. Guards walk their beat on the
    // grid exactly as they did in the original.
    int patrolDX = 1, patrolDY = 0;

    bool alerted = false;      // has ever noticed the player
};

class Enemies {
public:
    // Seed for the accuracy, pain-stagger and patrol-turn rolls. Defaulted so
    // the self-test's scenarios stay reproducible; the game seeds from the
    // clock at startLevel() so the same fight does not play out identically
    // every replay.
    static constexpr uint32_t kFixedSeed = 0x2545F491u;

    void spawnFrom(const Map& map, uint32_t seed = kFixedSeed);

    // Steps every actor. Takes the map non-const because a chasing guard
    // opens doors on the way through.
    void update(double dt, Map& map, Player& player, const Items& items);

    void appendBillboards(std::vector<Billboard>& out, const Player& player,
                          const ActorSprites& art) const;

    // Living enemies are solid, so the player cannot walk through a guard.
    bool blocks(double x, double y, double r) const;

    // Wakes everything within earshot. Gunfire carries: this is what makes
    // firing a shot a decision rather than a free action.
    void alertToNoise(const Map& map, double x, double y, double radius);

    // Nearest living enemy along a ray, or -1. Used by hitscan weapons.
    int traceHit(const Map& map, double px, double py, double dx, double dy,
                 double range) const;

    // Applies damage and drives the pain/death transitions. Returns the
    // score earned, which is non-zero only on the killing blow.
    int applyDamage(int index, int amount);

    int alive() const;
    int killed() const { return killed_; }
    int total() const { return static_cast<int>(enemies_.size()); }

    const std::vector<Enemy>& all() const { return enemies_; }

private:
    // Can this enemy see the player right now: in range, within the facing
    // cone, and with nothing solid in between.
    bool canSee(const Map& map, const Enemy& e, double px, double py) const;

    void moveToward(Map& map, Enemy& e, double dx, double dy, double dist,
                    const Items& items);
    bool freeAt(const Enemy& self, const Map& map, const Items& items,
                double x, double y) const;

    // Deterministic, so a failing self-test reproduces exactly. A guard
    // whose accuracy came from rand() would make every AI test flaky.
    uint32_t rng_ = 0x2545F491u;
    double   nextRandom();

    std::vector<Enemy> enemies_;
    int killed_ = 0;
};

} // namespace wolf
