#include "map.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace wolf {
namespace {

// --- moving-part tuning --------------------------------------------------

// Openness per second. Roughly half a second door to door, which is slow
// enough to feel heavy and fast enough that you never wait on it in a fight.
constexpr double kDoorSpeed = 1.9;

// How long a door stays open before shutting itself.
constexpr double kDoorHold = 5.0;

// How far open a door must be before the player fits through. Deliberately
// short of 1.0: you can start walking as it finishes opening.
constexpr double kDoorPassable = 0.72;

// Secrets always travel exactly two cells, as in the original.
constexpr double kPushwallDistance = 2.0;
constexpr double kPushwallSpeed    = 1.3;

// How far ahead Use reaches. Less than a full cell, so you have to actually
// walk up to a door rather than opening it from across the room.
constexpr double kUseReach = 0.75;

// ASCII legend
// ------------
//   .  floor                 #  grey brick          B  blue stone
//   @  player start          W  wood panel          M  mossy stone
//   =  door                  S  steel
//   $  gold-locked door      %  silver-locked door
//   P  pushwall (secret)     E  exit switch
//   g  guard                 z  SS trooper
//   h  health                a  ammo                t  treasure
//   k  gold key              l  silver key
//   m  machine gun           c  chaingun
//   o  lamp (blocks)         x  table (blocks)
//
// Layout, north at the top:
//
//   rows  1- 9  cell block (start) | guard room | storeroom (silver-locked)
//   row  10     divider with three doors
//   rows 11-12  east-west spine corridor
//   row  13     divider with two doors
//   rows 14-27  mess hall | courtyard (pillared)
//   row  28     divider; the courtyard door south is gold-locked
//   rows 29-46  armory (weapons) | exit hall, with a pushwall vault SE
//   row  47     south border, exit switch at col 30
constexpr int kLevelSize = 48;

const std::vector<std::string>& level1Rows() {
    static const std::vector<std::string> rows = {
        "################################################", //  0
        "#....#....#....#...............#...............#", //  1
        "#....#....#....#.......g.......#..a.........t..#", //  2
        "#.h..#....#..a.#....o......o...#........g......#", //  3
        "#..............#............g..#...............#", //  4
        "#..............#...............#...............#", //  5
        "#.....x........#...BBB...BBB...#...............#", //  6
        "#..............#...............#.......k.......#", //  7
        "#..@...........#..g............#...............#", //  8
        "#..............#...............#...............#", //  9
        "#######=###############=###############%########", // 10
        "#..................................g...........#", // 11
        "#...................a..........................#", // 12
        "#######=######################=#################", // 13
        "#...............#..............................#", // 14
        "#..x.x..........#........g.....................#", // 15
        "#...............#..............................#", // 16
        "#.......h.......#.....MM......MM......MM.......#", // 17
        "#...............#.......................g......#", // 18
        "#...............#..............................#", // 19
        "#...........a...=..............................#", // 20
        "#...............#.....MM......MM......MM.......#", // 21
        "#...............#................z.............#", // 22
        "#......l........#..............................#", // 23
        "#...............#..............................#", // 24
        "#...............#...........................t..#", // 25
        "#...............#..............................#", // 26
        "#...............#..............................#", // 27
        "#######=######################$#################", // 28
        "#...............#..............................#", // 29
        "#...m...........#...g..........................#", // 30
        "#...............#..............................#", // 31
        "#...........c...#.....BB......BB......BB.......#", // 32
        "#...............#..............................#", // 33
        "#......g........#..............z...............#", // 34
        "#...............=..............................#", // 35
        "#...............#..............................#", // 36
        "#...............#.....BB......BB......BB.......#", // 37
        "#...............#..........z...................#", // 38
        "#...............#..............................#", // 39
        "#...............#.......................SSSSSSSS", // 40
        "#......h........#.......................S......S", // 41
        "#...............#.......................St.t.t.S", // 42
        "#...g...........#.......................P......S", // 43
        "#...............#.......................S.tttt.S", // 44
        "#......a........#.......................S......S", // 45
        "#...............#.......................SSSSSSSS", // 46
        "##############################E#################", // 47
    };
    return rows;
}

// Overlap between a one-cell box whose corner sits at (bx, by) and an actor
// of radius r at (cx, cy). Shared by the player's collision test and the
// pushwall's own advance check, so a secret stops in exactly the places the
// player is stopped by it — the two disagreeing is what let a wall walk
// through someone.
bool boxOverlapsActor(double bx, double by, double cx, double cy, double r) {
    return cx + r > bx && cx - r < bx + 1.0 &&
           cy + r > by && cy - r < by + 1.0;
}

// Maps a wall glyph to its texture, or returns false if the glyph is not a
// wall at all.
bool wallTexture(char c, uint8_t& tex) {
    switch (c) {
    case '#': tex = TexBrick;     return true;
    case 'B': tex = TexBlueStone; return true;
    case 'W': tex = TexWood;      return true;
    case 'M': tex = TexMoss;      return true;
    case 'S': tex = TexSteel;     return true;
    default:  return false;
    }
}

// Maps a spawn glyph to its kind, or returns false for anything else.
bool spawnKind(char c, SpawnKind& kind) {
    switch (c) {
    case '@': kind = SpawnKind::PlayerStart; return true;
    case 'g': kind = SpawnKind::Guard;       return true;
    case 'z': kind = SpawnKind::SS;          return true;
    case 'h': kind = SpawnKind::Health;      return true;
    case 'a': kind = SpawnKind::Ammo;        return true;
    case 't': kind = SpawnKind::Treasure;    return true;
    case 'k': kind = SpawnKind::GoldKey;     return true;
    case 'l': kind = SpawnKind::SilverKey;   return true;
    case 'm': kind = SpawnKind::MachineGun;  return true;
    case 'c': kind = SpawnKind::Chaingun;    return true;
    case 'o': kind = SpawnKind::Lamp;        return true;
    case 'x': kind = SpawnKind::Table;       return true;
    default:  return false;
    }
}

} // namespace

Map Map::level1() {
    Map m;
    m.parse(level1Rows());
    m.orientDoors();
    return m;
}

void Map::orientDoors() {
    door_index_.assign(static_cast<size_t>(w_) * h_, -1);

    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            if (!isDoor(x, y)) continue;

            Door d;
            d.tx = x;
            d.ty = y;
            d.lock = at(x, y).kind;
            // A door flanked by walls east and west is one you walk through
            // heading north or south, so its slab spans the X axis and its
            // plane sits at a constant y. Deriving this from the geometry
            // means the ASCII level never has to spell out orientation, and
            // it cannot get out of sync with the walls around it.
            d.spansX = isSolid(x - 1, y) && isSolid(x + 1, y);

            door_index_[static_cast<size_t>(y) * w_ + x] =
                static_cast<int>(doors_.size());
            doors_.push_back(d);
        }
    }
}

const Door* Map::doorAt(int x, int y) const {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return nullptr;
    const int i = door_index_[static_cast<size_t>(y) * w_ + x];
    return i < 0 ? nullptr : &doors_[static_cast<size_t>(i)];
}

Door* Map::doorAtMut(int x, int y) {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return nullptr;
    const int i = door_index_[static_cast<size_t>(y) * w_ + x];
    return i < 0 ? nullptr : &doors_[static_cast<size_t>(i)];
}

MapCell& Map::cellAt(int x, int y) {
    return cells_[static_cast<size_t>(y) * w_ + x];
}

bool Map::blocksMovement(int x, int y) const {
    const TileKind k = at(x, y).kind;
    if (k == TileKind::Empty) return false;
    if (const Door* d = doorAt(x, y)) return d->openness < kDoorPassable;
    return true;
}

bool Map::hitsPushwall(double x, double y, double r) const {
    for (const Pushwall& p : pushwalls_)
        if (boxOverlapsActor(p.x, p.y, x, y, r)) return true;
    return false;
}

void Map::update(double dt, double px, double py, double pr) {
    const int ptx = static_cast<int>(std::floor(px));
    const int pty = static_cast<int>(std::floor(py));

    for (Door& d : doors_) {
        switch (d.state) {
        case DoorState::Closed:
            break;

        case DoorState::Opening:
            d.openness += kDoorSpeed * dt;
            if (d.openness >= 1.0) {
                d.openness = 1.0;
                d.state = DoorState::Open;
                d.hold = kDoorHold;
            }
            break;

        case DoorState::Open:
            d.hold -= dt;
            // Never guillotine the player. Standing in the doorway keeps
            // resetting the timer rather than shutting on them.
            if (d.hold <= 0.0) {
                if (ptx == d.tx && pty == d.ty) d.hold = 1.0;
                else d.state = DoorState::Closing;
            }
            break;

        case DoorState::Closing:
            if (ptx == d.tx && pty == d.ty) {
                // Walked back in while it was shutting: reopen.
                d.state = DoorState::Opening;
                break;
            }
            d.openness -= kDoorSpeed * dt;
            if (d.openness <= 0.0) {
                d.openness = 0.0;
                d.state = DoorState::Closed;
            }
            break;
        }
    }

    for (size_t i = 0; i < pushwalls_.size();) {
        Pushwall& p = pushwalls_[i];

        // Advance, then clamp to the stopping point, and move the box by
        // exactly the distance actually travelled. Stepping first and
        // reconstructing the origin afterwards is how a secret ends up
        // settling half a tile off the grid.
        const double before   = p.travelled;
        const double target   = std::min(before + kPushwallSpeed * dt,
                                         kPushwallDistance);
        const double advanced = target - before;
        const double nx = p.x + p.dx * advanced;
        const double ny = p.y + p.dy * advanced;

        // Collision only ever stopped the *player*, so a secret shoved into
        // the corridor they are standing in used to overtake them and then
        // mark the cell solid underneath — sealing them inside the wall for
        // the rest of the level. The secret waits instead: it is stone, and
        // stone does not walk through people.
        if (boxOverlapsActor(nx, ny, px, py, pr)) { ++i; continue; }

        p.travelled = target;
        p.x = nx;
        p.y = ny;

        if (p.travelled >= kPushwallDistance) {
            const int fx = static_cast<int>(std::lround(p.x));
            const int fy = static_cast<int>(std::lround(p.y));
            if (fx >= 0 && fx < w_ && fy >= 0 && fy < h_) {
                cellAt(fx, fy).kind = TileKind::Wall;
                cellAt(fx, fy).tex  = p.tex;
            }
            pushwalls_.erase(pushwalls_.begin() +
                             static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

void Map::openDoorFor(int x, int y) {
    Door* d = doorAtMut(x, y);
    if (!d) return;
    // Locked doors stay locked for enemies too; they path around instead.
    if (d->lock != TileKind::Door) return;
    if (d->state == DoorState::Open) d->hold = kDoorHold;
    else if (d->state != DoorState::Opening) d->state = DoorState::Opening;
}

UseResult Map::use(double px, double py, double dirX, double dirY,
                   bool goldKey, bool silverKey) {
    const int tx = static_cast<int>(std::floor(px + dirX * kUseReach));
    const int ty = static_cast<int>(std::floor(py + dirY * kUseReach));
    if (tx < 0 || tx >= w_ || ty < 0 || ty >= h_) return UseResult::Nothing;

    const TileKind kind = at(tx, ty).kind;

    if (kind == TileKind::ExitSwitch) return UseResult::ExitReached;

    if (Door* d = doorAtMut(tx, ty)) {
        if (d->lock == TileKind::DoorGold && !goldKey)     return UseResult::DoorLocked;
        if (d->lock == TileKind::DoorSilver && !silverKey) return UseResult::DoorLocked;
        if (d->state == DoorState::Open) {
            d->hold = kDoorHold;   // Use again to hold it open
            return UseResult::DoorOpened;
        }
        d->state = DoorState::Opening;
        return UseResult::DoorOpened;
    }

    if (kind == TileKind::Pushwall) {
        // Shove along whichever axis the player is most squarely facing;
        // pushwalls only ever travel on the grid.
        int dx = 0, dy = 0;
        if (std::abs(dirX) > std::abs(dirY)) dx = dirX > 0 ? 1 : -1;
        else                                 dy = dirY > 0 ? 1 : -1;

        // Both destination cells must be clear, exactly as in the original:
        // a secret that would end up inside another wall simply refuses.
        for (int step = 1; step <= static_cast<int>(kPushwallDistance); ++step) {
            const int cx = tx + dx * step;
            const int cy = ty + dy * step;
            if (cx < 0 || cx >= w_ || cy < 0 || cy >= h_) return UseResult::Nothing;
            if (at(cx, cy).kind != TileKind::Empty) return UseResult::Nothing;
        }

        Pushwall p;
        p.x = tx;
        p.y = ty;
        p.dx = dx;
        p.dy = dy;
        p.tex = at(tx, ty).tex;
        pushwalls_.push_back(p);
        ++secrets_found_;

        // It has left the grid; the tile it was standing on opens up.
        cellAt(tx, ty).kind = TileKind::Empty;
        return UseResult::PushwallMoved;
    }

    return UseResult::Nothing;
}

void Map::parse(const std::vector<std::string>& rows) {
    h_ = static_cast<int>(rows.size());
    assert(h_ > 0);
    w_ = static_cast<int>(rows[0].size());
    assert(w_ == kLevelSize && h_ == kLevelSize);

    cells_.assign(static_cast<size_t>(w_) * h_, MapCell{});

    for (int y = 0; y < h_; ++y) {
        // A short or long row would silently shift the entire level, so it
        // is worth catching loudly at load rather than debugging visually.
        assert(static_cast<int>(rows[y].size()) == w_);
        for (int x = 0; x < w_; ++x) {
            const char c = rows[y][x];
            MapCell& cell = cells_[static_cast<size_t>(y) * w_ + x];

            uint8_t tex = TexBrick;
            SpawnKind kind{};

            if (wallTexture(c, tex)) {
                cell.kind = TileKind::Wall;
                cell.tex  = tex;
            } else if (c == '=') {
                cell.kind = TileKind::Door;
                cell.tex  = TexDoor;
            } else if (c == '$') {
                cell.kind = TileKind::DoorGold;
                cell.tex  = TexDoor;
            } else if (c == '%') {
                cell.kind = TileKind::DoorSilver;
                cell.tex  = TexDoor;
            } else if (c == 'P') {
                // A pushwall must look like its neighbours or it isn't a
                // secret; it borrows the steel vault texture here.
                cell.kind = TileKind::Pushwall;
                cell.tex  = TexSteel;
                ++secrets_total_;
            } else if (c == 'E') {
                cell.kind = TileKind::ExitSwitch;
                cell.tex  = TexExitSwitch;
            } else if (spawnKind(c, kind)) {
                cell.kind = TileKind::Empty;
                spawns_.push_back(Spawn{kind, x, y});
                if (kind == SpawnKind::PlayerStart) {
                    start_x_ = x + 0.5;
                    start_y_ = y + 0.5;
                }
            } else {
                assert(c == '.' && "unknown map glyph");
                cell.kind = TileKind::Empty;
            }
        }
    }
}

const MapCell& Map::at(int x, int y) const {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return solid_;
    return cells_[static_cast<size_t>(y) * w_ + x];
}

bool Map::isSolid(int x, int y) const {
    return at(x, y).kind != TileKind::Empty;
}

bool Map::isDoor(int x, int y) const {
    const TileKind k = at(x, y).kind;
    return k == TileKind::Door || k == TileKind::DoorGold ||
           k == TileKind::DoorSilver;
}

} // namespace wolf
