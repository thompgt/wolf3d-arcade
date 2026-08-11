#include "selftest.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "core/config.h"
#include "game/enemy.h"
#include "game/items.h"
#include "game/map.h"
#include "game/player.h"
#include "game/weapons.h"
#include "render/font.h"
#include "render/hud.h"
#include "render/raycast.h"
#include "render/sprites.h"

namespace wolf {
namespace {

struct Report {
    int passed = 0;
    int failed = 0;
    std::ostringstream log;

    void check(bool ok, const std::string& what) {
        log << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
        ok ? ++passed : ++failed;
    }

    // Floating-point comparison with an explicit tolerance, and the actual
    // value in the message: a bare pass/fail on a distance tells you nothing
    // about how far off it was.
    void near(double got, double want, double tol, const std::string& what) {
        const bool ok = std::abs(got - want) <= tol;
        log << (ok ? "  pass  " : "  FAIL  ") << what
            << "  (got " << got << ", want " << want << ")\n";
        ok ? ++passed : ++failed;
    }

    void section(const std::string& name) { log << "\n" << name << "\n"; }
};

// Advances level state by a wall-clock duration at the real tick rate, so
// the tests exercise the same stepping the game does.
void step(Map& map, double seconds, double px, double py,
          double pr = PlayerTuning{}.radius) {
    const int ticks = static_cast<int>(seconds / kTickDT);
    for (int i = 0; i < ticks; ++i) map.update(kTickDT, px, py, pr);
}

// --- level integrity -----------------------------------------------------

void testLevel(Report& r) {
    r.section("level");
    Map m = Map::level1();

    r.check(m.width() == 48 && m.height() == 48, "level is 48x48");
    r.near(m.startX(), 3.5, 1e-9, "player start x");
    r.near(m.startY(), 8.5, 1e-9, "player start y");

    bool sealed = true;
    for (int i = 0; i < m.width(); ++i) {
        if (!m.isSolid(i, 0) || !m.isSolid(i, m.height() - 1)) sealed = false;
        if (!m.isSolid(0, i) || !m.isSolid(m.width() - 1, i)) sealed = false;
    }
    r.check(sealed, "border is sealed on all four sides");

    int guards = 0, treasure = 0;
    bool goldKey = false, silverKey = false;
    for (const Spawn& s : m.spawns()) {
        if (s.kind == SpawnKind::Guard || s.kind == SpawnKind::SS) ++guards;
        if (s.kind == SpawnKind::Treasure) ++treasure;
        if (s.kind == SpawnKind::GoldKey) goldKey = true;
        if (s.kind == SpawnKind::SilverKey) silverKey = true;
    }
    r.check(guards == 13, "13 enemies spawn");
    r.check(treasure == 9, "9 treasures spawn");
    r.check(goldKey && silverKey, "both keys are placed");
}

// --- doors ---------------------------------------------------------------

void testDoors(Report& r) {
    r.section("doors");
    Map m = Map::level1();

    // The cell-block door. Flanked east and west, so you pass through it
    // heading north/south and its slab spans X.
    const Door* d = m.doorAt(7, 10);
    r.check(d != nullptr, "door exists at (7,10)");
    if (!d) return;
    r.check(d->spansX, "orientation derived as spanning X");
    r.check(d->state == DoorState::Closed, "starts closed");
    r.check(m.blocksMovement(7, 10), "closed door blocks movement");

    // Open it from the north side.
    const UseResult u = m.use(7.5, 9.5, 0.0, 1.0, false, false);
    r.check(u == UseResult::DoorOpened, "Use from the north opens it");

    step(m, 1.0, 7.5, 9.5);
    r.check(m.doorAt(7, 10)->state == DoorState::Open, "reaches fully open");
    r.check(!m.blocksMovement(7, 10), "open door is walkable");

    // Stand in the doorway well past the hold time: it must not close.
    step(m, 12.0, 7.5, 10.5);
    r.check(m.doorAt(7, 10)->state == DoorState::Open,
            "will not close while the player stands in it");

    // Step away and it shuts itself.
    step(m, 12.0, 7.5, 8.5);
    r.check(m.doorAt(7, 10)->state == DoorState::Closed, "auto-closes once clear");
    r.check(m.blocksMovement(7, 10), "closed again blocks movement");
}

void testLockedDoors(Report& r) {
    r.section("locked doors");
    Map m = Map::level1();

    // Gold-locked door into the exit hall.
    r.check(m.use(30.5, 27.5, 0.0, 1.0, false, false) == UseResult::DoorLocked,
            "gold door refuses without the gold key");
    r.check(m.doorAt(30, 28)->state == DoorState::Closed,
            "refused gold door stays shut");
    r.check(m.use(30.5, 27.5, 0.0, 1.0, true, false) == UseResult::DoorOpened,
            "gold door opens with the gold key");

    // Silver-locked storeroom.
    r.check(m.use(39.5, 9.5, 0.0, 1.0, false, false) == UseResult::DoorLocked,
            "silver door refuses without the silver key");
    r.check(m.use(39.5, 9.5, 0.0, 1.0, false, true) == UseResult::DoorOpened,
            "silver door opens with the silver key");

    // Enemies must not be able to walk through progression gates.
    Map m2 = Map::level1();
    m2.openDoorFor(30, 28);
    r.check(m2.doorAt(30, 28)->state == DoorState::Closed,
            "enemies cannot open a locked door");
    m2.openDoorFor(7, 10);
    r.check(m2.doorAt(7, 10)->state == DoorState::Opening,
            "enemies can open an unlocked door");
}

// --- pushwalls -----------------------------------------------------------

void testPushwalls(Report& r) {
    r.section("pushwall secrets");
    Map m = Map::level1();

    r.check(m.at(40, 43).kind == TileKind::Pushwall, "secret at (40,43)");

    // Blocked path first, because a successful shove removes the secret.
    // Called directly rather than through a walkable position: south of the
    // secret is vault wall, so there is nowhere legal to stand and face it.
    r.check(m.use(40.5, 42.6, 0.0, 1.0, false, false) == UseResult::Nothing,
            "refuses to move into the vault wall");
    r.check(m.at(40, 43).kind == TileKind::Pushwall, "refused secret stays put");
    r.check(m.pushwalls().empty(), "refused secret does not start moving");

    // Now shove it east from the corridor, which is where a player would be.
    r.check(m.use(39.5, 43.5, 1.0, 0.0, false, false) == UseResult::PushwallMoved,
            "Use from the west shoves it east");
    r.check(m.at(40, 43).kind == TileKind::Empty, "origin tile opens up");
    r.check(m.pushwalls().size() == 1, "one secret in motion");

    // Mid-travel it is off the grid, so only the box test can find it.
    step(m, 0.5, 39.5, 43.5);
    r.check(!m.pushwalls().empty(), "still travelling after 0.5s");
    r.check(m.hitsPushwall(m.pushwalls()[0].x + 0.5,
                           m.pushwalls()[0].y + 0.5, 0.24),
            "moving secret blocks the player");

    // Let it finish and check it settled exactly two tiles on.
    step(m, 3.0, 39.5, 43.5);
    r.check(m.pushwalls().empty(), "secret comes to rest");
    r.check(m.at(42, 43).kind == TileKind::Wall,
            "settles exactly two tiles east");
    r.check(m.at(41, 43).kind == TileKind::Empty, "leaves a walkable gap behind");
    r.check(m.at(42, 43).tex == TexSteel, "keeps its own texture");
}

// A secret used to advance regardless of what was in its way. Player
// collision only ever stopped the *player*, so a wall shoved into the tile
// someone was standing in overtook them and then marked that tile solid,
// sealing them inside it permanently. It must wait for them to step clear.
void testPushwallVsPlayer(Report& r) {
    r.section("a secret does not walk through the player");
    Map m = Map::level1();

    r.check(m.use(39.5, 43.5, 1.0, 0.0, false, false) == UseResult::PushwallMoved,
            "secret shoved east");
    const double startX = m.pushwalls()[0].x;

    // Stand directly in its path, one tile east of where it started.
    const double px = 41.5, py = 43.5;
    step(m, 3.0, px, py);

    r.check(!m.pushwalls().empty(), "it has not finished travelling");
    r.check(m.pushwalls()[0].x < startX + 1.0,
            "it stops short of the player rather than passing through");
    r.check(!m.hitsPushwall(px, py, PlayerTuning{}.radius),
            "and never overlaps them");
    r.check(m.at(41, 43).kind == TileKind::Empty,
            "the tile under the player is not marked solid");

    // Step aside and it completes as normal.
    step(m, 3.0, 39.5, 43.5);
    r.check(m.pushwalls().empty(), "it resumes once the way is clear");
    r.check(m.at(42, 43).kind == TileKind::Wall, "and settles two tiles east");
}

// --- exit ----------------------------------------------------------------

void testExit(Report& r) {
    r.section("exit switch");
    Map m = Map::level1();
    r.check(m.use(30.5, 46.5, 0.0, 1.0, false, false) == UseResult::ExitReached,
            "Use on the exit switch ends the level");
    r.check(m.use(20.5, 46.5, 0.0, 1.0, false, false) == UseResult::Nothing,
            "plain wall does nothing");
}

// --- rays ----------------------------------------------------------------

void testRays(Report& r) {
    r.section("raycasting through doors");
    Map m = Map::level1();

    // Straight south from the cell block, at the door column.
    RayHit hit = castRayDynamic(m, 7.5, 9.5, 0.0, 1.0);
    r.check(hit.hit && hit.isDoor, "closed door stops the ray");
    // Slab sits at the cell midline, y = 10.5, one tile from the origin.
    r.near(hit.perpDist, 1.0, 1e-6, "hits the slab at the cell midline");

    // Open it fully, then the same ray must carry on to the next door two
    // rows further south. If a missed slab stopped the DDA instead of
    // continuing it, an open door would still be an opaque wall.
    m.use(7.5, 9.5, 0.0, 1.0, false, false);
    step(m, 1.0, 7.5, 9.5);
    hit = castRayDynamic(m, 7.5, 9.5, 0.0, 1.0);
    r.check(hit.hit && hit.isDoor, "ray passes through and finds the next door");
    r.near(hit.perpDist, 4.0, 1e-6, "reaches the row-13 door slab");

    // The reveal inside a doorway must wear the jamb texture, or the recess
    // renders as brickwork and the door stops looking inset.
    hit = castRayDynamic(m, 7.5, 11.0, 1.0, 0.0);
    r.check(hit.hit, "ray east from inside the doorway hits something");

    // A wall with nothing in the way is unremarkable, but it pins down that
    // perpendicular distance is being measured, not ray length.
    hit = castRayDynamic(m, 3.5, 8.5, 1.0, 0.0);
    r.check(hit.hit && !hit.isDoor, "east from spawn hits a wall");
    r.near(hit.perpDist, 11.5, 1e-6, "wall at x=15 is 11.5 tiles away");
}

// --- items ---------------------------------------------------------------

// Places a fresh player on a tile and collects whatever is there.
Player playerOn(Items& items, double x, double y) {
    Player p;
    p.spawn(x, y, 0.0);
    items.collect(p);
    return p;
}

int billboardCount(const Items& items) { return items.remaining(); }

void testItems(Report& r) {
    r.section("items");
    Map m = Map::level1();
    Items items;
    items.spawnFrom(m);

    // 9 treasure + 3 health + 5 ammo + 2 keys + 2 weapons + 2 lamps
    // + 3 tables. Enemies are not items and must not appear here.
    r.check(billboardCount(items) == 26, "26 objects spawn into the world");
    r.check(items.treasureTotal() == 9, "9 treasures counted for the tally");

    // Ammo at (13,3), in the third cell.
    {
        Items fresh; fresh.spawnFrom(m);
        Player p = playerOn(fresh, 13.5, 3.5);
        r.check(p.ammo() == 16, "walking over a clip takes it (8 -> 16)");
        r.check(billboardCount(fresh) == 25, "collected item leaves the world");
        fresh.collect(p);
        r.check(p.ammo() == 16, "an already-taken clip cannot be taken twice");
    }

    // A full-health player must leave the medkit where it is, so it is
    // still there on the way back rather than being silently binned.
    {
        Items fresh; fresh.spawnFrom(m);
        Player p = playerOn(fresh, 2.5, 3.5);
        r.check(p.health() == 100, "full health is not exceeded");
        r.check(billboardCount(fresh) == 26, "refused medkit stays in the world");
    }

    // Keys and weapons.
    {
        Items fresh; fresh.spawnFrom(m);
        Player g = playerOn(fresh, 39.5, 7.5);
        r.check(g.hasGoldKey(), "gold key is collected from the storeroom");

        Items fresh2; fresh2.spawnFrom(m);
        Player s = playerOn(fresh2, 7.5, 23.5);
        r.check(s.hasSilverKey(), "silver key is collected from the mess hall");

        Items fresh3; fresh3.spawnFrom(m);
        Player w = playerOn(fresh3, 4.5, 30.5);
        r.check(w.hasMachineGun(), "machine gun is collected from the armory");
        r.check(w.ammo() > 8, "a picked-up weapon comes part-loaded");
    }

    // Treasure is the score game.
    {
        Items fresh; fresh.spawnFrom(m);
        Player p = playerOn(fresh, 44.5, 2.5);
        r.check(p.score() > 0, "treasure scores");
        r.check(fresh.treasureTaken() == 1, "treasure counts toward the tally");
    }

    // Scenery blocks, pickups do not: walking through a medkit is the whole
    // point of it, walking through a table is not.
    r.check(items.blocks(6.5, 6.5, 0.24), "a table blocks the player");
    r.check(items.blocks(20.5, 3.5, 0.24), "a lamp blocks the player");
    r.check(!items.blocks(13.5, 3.5, 0.24), "an ammo clip does not block");
    r.check(!items.blocks(30.5, 30.5, 0.24), "empty floor does not block");
}

// --- enemy AI ------------------------------------------------------------

// Index of the enemy standing on a given tile. Positions change once they
// start moving, so this is only meaningful before stepping.
int enemyAt(const Enemies& es, int tx, int ty) {
    const std::vector<Enemy>& all = es.all();
    for (size_t i = 0; i < all.size(); ++i) {
        if (static_cast<int>(std::floor(all[i].x)) == tx &&
            static_cast<int>(std::floor(all[i].y)) == ty)
            return static_cast<int>(i);
    }
    return -1;
}

void stepAI(Enemies& es, Map& map, Player& p, const Items& items,
            double seconds) {
    const int ticks = static_cast<int>(seconds / kTickDT);
    for (int i = 0; i < ticks; ++i) {
        map.update(kTickDT, p.x(), p.y(), p.tuning().radius);
        es.update(kTickDT, map, p, items);
    }
}

void testEnemies(Report& r) {
    r.section("enemy spawning");
    {
        Map m = Map::level1();
        Enemies es;
        es.spawnFrom(m);
        r.check(es.total() == 13, "13 enemies spawn");
        r.check(es.alive() == 13, "all start alive");
        r.check(es.killed() == 0, "none start dead");

        int ss = 0;
        for (const Enemy& e : es.all())
            if (e.type == EnemyType::SS) ++ss;
        r.check(ss == 3, "3 of them are SS troopers");

        // The SS is meant to be the harder fight, not a reskin.
        const EnemyTuning& g = tuningFor(EnemyType::Guard);
        const EnemyTuning& s = tuningFor(EnemyType::SS);
        r.check(s.health > g.health, "SS is tankier than a guard");
        r.check(s.fireInterval < g.fireInterval, "SS fires faster");
        r.check(s.painChance < g.painChance, "SS shrugs off more hits");
    }

    // the corridor guard at (35,11), with a long clear sightline both ways.
    r.section("enemy detection");
    {
        Map m = Map::level1();
        Items items; items.spawnFrom(m);
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);
        r.check(gi >= 0, "found the corridor guard at (35,11)");

        // Standing behind him: he faces east, the player is west.
        Player behind; behind.spawn(28.5, 11.5, 0.0);
        stepAI(es, m, behind, items, 1.0);
        r.check(es.all()[gi].state != EnemyState::Chase,
                "does not notice the player standing behind him");
        r.check(!es.all()[gi].alerted, "and is not alerted by it");
    }
    {
        Map m = Map::level1();
        Items items; items.spawnFrom(m);
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);

        // In front of him, well within the cone and the sight range.
        Player ahead; ahead.spawn(40.5, 11.5, 0.0);
        stepAI(es, m, ahead, items, 0.6);
        r.check(es.all()[gi].alerted, "notices a player in front of him");
        r.check(es.all()[gi].state == EnemyState::Chase ||
                es.all()[gi].state == EnemyState::Shoot,
                "and reacts by closing or firing");
    }

    r.section("enemy chase and attack");
    {
        Map m = Map::level1();
        Items items; items.spawnFrom(m);
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);
        const double startX = es.all()[gi].x;

        Player p; p.spawn(44.5, 11.5, 0.0);
        stepAI(es, m, p, items, 1.5);
        r.check(es.all()[gi].x > startX + 0.3,
                "closes the distance once it has seen you");

        // Given long enough it must actually land shots. Accuracy is
        // random but the generator is seeded, so this is reproducible.
        stepAI(es, m, p, items, 8.0);
        r.check(p.health() < 100, "a guard that can see you does damage");
        r.check(p.health() > 0 || p.isDead(), "damage is applied coherently");
    }

    r.section("enemy damage and death");
    {
        Map m = Map::level1();
        Items items; items.spawnFrom(m);
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);

        r.check(es.applyDamage(gi, 5) == 0, "a survivable hit scores nothing");
        r.check(es.all()[gi].alerted, "being shot alerts, even from behind");

        const int score = es.applyDamage(gi, 999);
        r.check(score == tuningFor(EnemyType::Guard).score,
                "the killing blow scores");
        r.check(es.all()[gi].state == EnemyState::Die, "starts dying");
        r.check(es.killed() == 1, "kill is counted");
        r.check(es.alive() == 12, "and no longer counts as alive");
        r.check(es.applyDamage(gi, 999) == 0, "a corpse cannot be killed twice");

        // Corpses must stop being obstacles or doorways silt up with bodies.
        r.check(!es.blocks(es.all()[gi].x, es.all()[gi].y, 0.24),
                "a corpse does not block the player");

        Player p; p.spawn(44.5, 11.5, 0.0);
        stepAI(es, m, p, items, 1.0);
        r.check(es.all()[gi].state == EnemyState::Dead,
                "death animation finishes and settles");
    }

    r.section("hitscan tracing");
    {
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);

        // West along the corridor, straight at him.
        r.check(es.traceHit(m, 40.5, 11.5, -1.0, 0.0, 20.0) == gi,
                "traces a hit down the corridor");
        // East, away from him.
        r.check(es.traceHit(m, 40.5, 11.5, 1.0, 0.0, 20.0) == -1,
                "misses when aimed the other way");
        // Correct direction but out of range.
        r.check(es.traceHit(m, 40.5, 11.5, -1.0, 0.0, 2.0) == -1,
                "range limits the shot");
        // Through the guard room wall at a guard beyond it.
        r.check(es.traceHit(m, 5.5, 8.5, 1.0, 0.0, 40.0) == -1,
                "a wall stops the bullet");
    }

    r.section("noise alerting");
    {
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int corridor = enemyAt(es, 35, 11);
        const int guardRoom = enemyAt(es, 18, 8);

        // A shot fired in the corridor, in the open.
        es.alertToNoise(m, 40.5, 11.5, 20.0);
        r.check(es.all()[corridor].state == EnemyState::Chase,
                "gunfire in the open wakes a guard down the corridor");

        // The same shot must not carry through the cell block wall, or
        // firing once wakes the entire level and there is no reason ever
        // to stop shooting.
        Enemies es2; es2.spawnFrom(m);
        es2.alertToNoise(m, 5.5, 8.5, 30.0);
        r.check(es2.all()[guardRoom].state != EnemyState::Chase,
                "gunfire does not carry through a wall");
    }

    r.section("AI determinism");
    {
        // Two identical runs must agree exactly. A guard whose accuracy came
        // from an unseeded generator would make every test above flaky.
        auto run = []() {
            Map m = Map::level1();
            Items items; items.spawnFrom(m);
            Enemies es; es.spawnFrom(m);
            Player p; p.spawn(44.5, 11.5, 0.0);
            stepAI(es, m, p, items, 6.0);
            return p.health();
        };
        const int a = run();
        const int b = run();
        r.check(a == b, "the same scenario produces the same outcome");
    }
}

// --- weapons -------------------------------------------------------------

// Holds the trigger down for a span of time, modelling the platform exactly:
// the key goes down once and stays down, so the latched press edge is set on
// the first tick and on no other. A semi-automatic weapon must therefore
// fire exactly one round no matter how long this runs.
void holdTrigger(Weapons& w, Map& map, Player& p, Enemies& es,
                 double seconds, ShotResult* last = nullptr) {
    const int ticks = static_cast<int>(seconds / kTickDT);
    for (int i = 0; i < ticks; ++i) {
        const ShotResult s = w.update(kTickDT, true, i == 0, map, p, es);
        if (last && s.fired) *last = s;
    }
}

// Presses and releases the trigger repeatedly, which is what a player does
// with a semi-automatic. The edge is latched on the tick the key goes down,
// matching Platform::pressed().
void pumpTrigger(Weapons& w, Map& map, Player& p, Enemies& es, double seconds) {
    const int ticks = static_cast<int>(seconds / kTickDT);
    for (int i = 0; i < ticks; ++i)
        w.update(kTickDT, (i % 8) < 4, (i % 8) == 0, map, p, es);
}

// One keystroke so brief it is never down when a tick samples it -- only the
// latch records it. This is the case that used to drop the shot silently.
ShotResult tapTrigger(Weapons& w, Map& map, Player& p, Enemies& es) {
    return w.update(kTickDT, false, true, map, p, es);
}

// Rounds actually fired over a span, measured by ammo spent rather than by
// counting calls: ammo is what the player feels.
int shotsFired(Weapons& w, Map& map, Player& p, Enemies& es, double seconds) {
    const int before = p.ammo();
    holdTrigger(w, map, p, es, seconds);
    return before - p.ammo();
}

void testWeapons(Report& r) {
    r.section("weapon tuning");
    {
        const WeaponTuning& knife = tuningFor(WeaponType::Knife);
        const WeaponTuning& pistol = tuningFor(WeaponType::Pistol);
        const WeaponTuning& mg = tuningFor(WeaponType::MachineGun);
        const WeaponTuning& cg = tuningFor(WeaponType::Chaingun);

        // The four have to be distinct choices, not reskins.
        r.check(cg.fireInterval < mg.fireInterval &&
                mg.fireInterval < pistol.fireInterval,
                "each firearm cycles faster than the last");
        r.check(cg.spread > mg.spread && mg.spread > pistol.spread,
                "and pays for it in accuracy");
        r.check(knife.ammoUse == 0, "the knife costs no ammo");
        r.check(knife.noiseRadius == 0.0, "and makes no noise");
        r.check(knife.range < 2.0, "but only reaches arm's length");
        r.check(!pistol.automatic && mg.automatic && cg.automatic,
                "the pistol is semi-automatic and the rest are not");
    }

    r.section("weapon selection");
    {
        Player p; p.spawn(3.5, 8.5, 0.0);
        Weapons w; w.reset();

        r.check(w.current() == WeaponType::Pistol, "starts on the pistol");
        r.check(w.owns(WeaponType::Knife, p), "the knife is always owned");
        r.check(!w.owns(WeaponType::Chaingun, p), "the chaingun is not");

        r.check(!w.select(WeaponType::Chaingun, p),
                "cannot select a weapon you do not have");
        r.check(w.current() == WeaponType::Pistol, "and the choice is unchanged");

        p.giveChaingun();
        r.check(w.select(WeaponType::Chaingun, p), "can once you pick it up");
        r.check(w.current() == WeaponType::Chaingun, "and it is in your hands");
    }
    {
        // An empty firearm is not a choice. The knife still is.
        Player p; p.spawn(3.5, 8.5, 0.0);
        Weapons w; w.reset();
        p.useAmmo(p.ammo());

        r.check(p.ammo() == 0, "player is out of ammo");
        r.check(w.select(WeaponType::Knife, p), "the knife always answers");
        r.check(!w.select(WeaponType::Pistol, p),
                "an empty pistol cannot be selected");
    }

    r.section("ammo");
    {
        Player p; p.spawn(3.5, 8.5, 0.0);
        const int start = p.ammo();
        r.check(!p.useAmmo(start + 1), "cannot spend ammo you do not have");
        r.check(p.ammo() == start, "and nothing is spent trying");
        r.check(p.useAmmo(3) && p.ammo() == start - 3, "a spend is exact");
    }

    r.section("rate of fire");
    {
        // Facing the corridor guard at (35,11) from the east, so shots have
        // something to hit and the geometry matches the enemy tests.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
        p.giveAmmo(99);

        Weapons w; w.reset();
        const int pistolShots = shotsFired(w, m, p, es, 1.0);

        Map m2 = Map::level1();
        Enemies es2; es2.spawnFrom(m2);
        Player p2; p2.spawn(40.5, 11.5, 3.14159265358979323846);
        p2.giveAmmo(99);
        p2.giveChaingun();
        Weapons w2; w2.reset();
        w2.select(WeaponType::Chaingun, p2);
        const int chainShots = shotsFired(w2, m2, p2, es2, 1.0);

        // Held for a full second, the pistol fires once and the chaingun
        // empties into the corridor. That gap is the whole point of having
        // both, and a semi-automatic that repeated on a held trigger would
        // make the machine gun pointless.
        r.check(pistolShots == 1, "a held pistol fires exactly once");
        r.check(chainShots > 8, "a held chaingun keeps firing");
    }
    {
        // A keystroke shorter than a tick is never down when a tick samples
        // it: only the platform's latched edge records it happened. Firing
        // off the held state alone drops that shot silently, which is what
        // driving the real game turned up.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
        Weapons w; w.reset();

        const int before = p.ammo();
        r.check(tapTrigger(w, m, p, es).fired, "a tap between ticks still fires");
        r.check(p.ammo() == before - 1, "and costs exactly one round");
    }
    {
        // The same for an automatic weapon, which would otherwise need the
        // key held across a tick boundary to do anything at all.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
        p.giveChaingun();
        Weapons w; w.reset();
        w.select(WeaponType::Chaingun, p);

        r.check(tapTrigger(w, m, p, es).fired,
                "a tapped chaingun fires one round too");
    }

    r.section("firing");
    {
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);
        const int fullHealth = es.all()[gi].health;

        // Aimed west, straight down the corridor at him.
        Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
        p.giveAmmo(99);
        Weapons w; w.reset();

        ShotResult shot;
        holdTrigger(w, m, p, es, 0.1, &shot);
        r.check(shot.fired, "the pistol fires");
        r.check(es.all()[gi].health < fullHealth, "and the guard is hit");

        // Enough rounds to kill him, then confirm the score lands on the
        // player rather than only being returned. Pumped, not held: a held
        // pistol has already fired its one round above.
        const int before = p.score();
        pumpTrigger(w, m, p, es, 3.0);

        r.check(es.all()[gi].state == EnemyState::Die ||
                es.all()[gi].state == EnemyState::Dead,
                "sustained fire kills him");
        r.check(p.score() > before, "and the kill scores");
    }
    {
        // A wall between you and a guard stops the round.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);
        const int fullHealth = es.all()[gi].health;

        Player p; p.spawn(5.5, 8.5, 0.0);   // cell block, far side of a wall
        p.giveAmmo(99);
        Weapons w; w.reset();
        holdTrigger(w, m, p, es, 2.0);

        r.check(es.all()[gi].health == fullHealth,
                "a wall stops the bullet");
    }

    r.section("dry firing");
    {
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
        Weapons w; w.reset();
        p.useAmmo(p.ammo());

        const ShotResult s = w.update(kTickDT, true, true, m, p, es);
        r.check(!s.fired, "an empty pistol does not fire");
        r.check(s.dryFired, "and says so");
        r.check(w.current() == WeaponType::Pistol,
                "running dry does not silently swap weapons");
        r.check(p.ammo() == 0, "and cannot go negative");
    }
    {
        // The knife works with an empty pouch, which is the reason it is
        // never taken away.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);
        const int fullHealth = es.all()[gi].health;

        // Stood right next to him, inside knife reach.
        Player p; p.spawn(es.all()[gi].x + 0.9, es.all()[gi].y, 3.14159265358979323846);
        p.useAmmo(p.ammo());
        Weapons w; w.reset();
        w.select(WeaponType::Knife, p);

        holdTrigger(w, m, p, es, 0.5);
        r.check(es.all()[gi].health < fullHealth, "the knife cuts with no ammo");
        r.check(p.ammo() == 0, "and spends none");
    }

    r.section("gunfire is heard");
    {
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);

        // Behind him, out of his cone: only the noise can wake him.
        Player p; p.spawn(28.5, 11.5, 0.0);
        p.giveAmmo(99);
        Weapons w; w.reset();
        holdTrigger(w, m, p, es, 0.1);

        r.check(es.all()[gi].alerted || es.all()[gi].state == EnemyState::Chase,
                "firing wakes a guard who never saw you");
    }
    {
        // The knife is silent, which is what makes it worth carrying.
        Map m = Map::level1();
        Enemies es; es.spawnFrom(m);
        const int gi = enemyAt(es, 35, 11);

        Player p; p.spawn(28.5, 11.5, 0.0);
        Weapons w; w.reset();
        w.select(WeaponType::Knife, p);
        holdTrigger(w, m, p, es, 0.5);

        r.check(es.all()[gi].state != EnemyState::Chase,
                "the knife does not give you away");
    }

    r.section("weapon determinism");
    {
        // Spread is random; the generator is seeded. Two identical runs must
        // agree exactly or every check above is a coin flip.
        auto run = []() {
            Map m = Map::level1();
            Enemies es; es.spawnFrom(m);
            const int gi = enemyAt(es, 35, 11);
            Player p; p.spawn(40.5, 11.5, 3.14159265358979323846);
            p.giveAmmo(99);
            Weapons w; w.reset();
            pumpTrigger(w, m, p, es, 3.0);
            return es.all()[gi].health;
        };
        r.check(run() == run(), "the same burst produces the same outcome");
    }
}

// --- HUD -----------------------------------------------------------------

void testHud(Report& r) {
    r.section("secret tally");
    {
        Map m = Map::level1();
        r.check(m.secretsTotal() > 0, "the level has secrets to find");
        r.check(m.secretsFound() == 0, "none are found to begin with");

        m.use(39.5, 43.5, 1.0, 0.0, false, false);
        r.check(m.secretsFound() == 1, "shoving one counts it");

        // The count has to survive the secret settling. pushwalls_ drops it
        // from the list once it stops moving, so a tally derived from that
        // list would report the secret as un-found the moment it arrived.
        step(m, 3.0, 39.5, 43.5);
        r.check(m.pushwalls().empty(), "it has come to rest");
        r.check(m.secretsFound() == 1, "and is still counted");
    }

    r.section("face portrait");
    {
        FaceSprites faces;
        faces.generate();

        // Identity comparison: distinct states must not resolve to the same
        // frame, which is the only way the face carries information.
        const Sprite* healthy = &faces.pick(100, 1, false, false);
        const Sprite* hurt    = &faces.pick(10,  1, false, false);
        const Sprite* dead    = &faces.pick(0,   1, false, false);

        r.check(healthy != hurt, "a wounded face differs from a healthy one");
        r.check(dead != healthy && dead != hurt, "and a dead one from both");

        r.check(&faces.pick(100, 0, false, false) !=
                &faces.pick(100, 2, false, false),
                "looking left differs from looking right");

        r.check(&faces.pick(100, 1, true, false) != healthy,
                "a grimace differs from the idle face");
        r.check(&faces.pick(100, 1, false, true) != healthy,
                "so does a gloat");

        // Being hit is the more urgent thing to report, and a face grinning
        // through damage reads as a bug.
        r.check(&faces.pick(100, 1, true, true) ==
                &faces.pick(100, 1, true, false),
                "a grimace outranks a gloat");

        // Out-of-range input is clamped, not asserted: a wrong frame is
        // cosmetic and a crash mid-firefight is not. Health above 100 clamps
        // to the healthy tier and a glance of 9 clamps to the last one, so
        // the result is the rightmost healthy face.
        r.check(&faces.pick(999, 9, false, false) ==
                &faces.pick(100, 2, false, false),
                "out-of-range health and glance clamp");
        r.check(&faces.pick(-50, 1, false, false) == dead,
                "negative health reads as dead");
    }

    r.section("text metrics");
    {
        r.check(textWidth("") == 0, "empty text has no width");
        r.check(textWidth("AB") == 2 * kGlyphW, "width scales with length");
        r.check(textWidth("AB", 2) == 2 * 2 * kGlyphW, "and with scale");

        // The status bar's widest fields have to fit the 320-pixel bar, and
        // the layout is hand-budgeted -- so the budget is asserted rather
        // than trusted.
        r.check(textWidth("HEALTH") <= 48, "HEALTH fits its slot");
        r.check(textWidth("PISTOL") <= 48, "so does the widest weapon name");
        r.check(textWidth("100", 2) <= 48, "and full health at value size");
    }
}

} // namespace

int runSelfTest() {
    Report r;
    testLevel(r);
    testDoors(r);
    testLockedDoors(r);
    testPushwalls(r);
    testPushwallVsPlayer(r);
    testExit(r);
    testRays(r);
    testItems(r);
    testEnemies(r);
    testWeapons(r);
    testHud(r);

    std::ostringstream out;
    out << "wolf3d-arcade self-test\n" << r.log.str() << "\n"
        << r.passed << " passed, " << r.failed << " failed\n";

    const std::string text = out.str();
    std::fputs(text.c_str(), stdout);
    std::fflush(stdout);

    // The release build is a Windows-subsystem binary with no console, so
    // the file is the only way to read results from a normal build.
    std::ofstream f("selftest.txt");
    if (f) f << text;

    return r.failed == 0 ? 0 : 1;
}

} // namespace wolf
