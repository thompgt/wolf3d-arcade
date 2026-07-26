// The level: a tile grid, the things that spawn in it, and the moving parts
// (doors and pushwalls) that make it a place rather than a diagram.
//
// Like the original, the world is strictly grid-aligned — every wall is a
// full cell, which is exactly what makes the DDA raycaster cheap and exact.
// Levels are authored as ASCII art (see map.cpp) so they stay readable and
// editable in a text editor.
//
// Door and pushwall state lives here rather than in a separate system
// because it *is* level state: everything that already takes a Map — the
// raycaster, collision, enemy sight — needs to agree about whether a
// doorway is passable, and a second source of truth is how they end up
// disagreeing.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wolf {

// What a cell *is*, independent of how it looks.
enum class TileKind : uint8_t {
    Empty,        // walkable floor
    Wall,         // solid, textured
    Door,         // slides open on Use / enemy approach
    DoorGold,     // needs the gold key
    DoorSilver,   // needs the silver key
    Pushwall,     // looks solid; Use shoves it to reveal a secret
    ExitSwitch,   // solid, but Use ends the level
};

// Index into the procedurally generated texture set (see render/textures).
enum TexId : uint8_t {
    TexBrick = 0,
    TexBlueStone,
    TexWood,
    TexMoss,
    TexSteel,
    TexDoor,
    TexDoorFrame,
    TexExitSwitch,
    TexCount
};

struct MapCell {
    TileKind kind = TileKind::Empty;
    uint8_t  tex  = TexBrick;
};

enum class DoorState : uint8_t { Closed, Opening, Open, Closing };

struct Door {
    int  tx = 0, ty = 0;
    TileKind lock = TileKind::Door;

    // True when the slab spans the X axis and its plane sits at a constant
    // y — i.e. you walk through it heading north or south. Derived from the
    // neighbouring walls at load time, so level authors never specify it.
    bool spansX = true;

    DoorState state = DoorState::Closed;
    double openness = 0.0;   // 0 shut, 1 fully receded into the wall
    double hold = 0.0;       // seconds left before it closes itself
};

// A secret wall in motion. Once shoved it is no longer on the tile grid: it
// is a moving box, which is why it carries a world-space position rather
// than tile coordinates.
struct Pushwall {
    double x = 0.0, y = 0.0;  // minimum corner of the 1x1 box
    int    dx = 0, dy = 0;    // unit travel direction
    double travelled = 0.0;   // tiles moved so far
    uint8_t tex = TexSteel;
};

// Everything that isn't geometry: actors, pickups and decor. The map only
// records where they belong; the phases that own them do the spawning.
enum class SpawnKind : uint8_t {
    PlayerStart,
    Guard, SS,
    Health, Ammo, Treasure,
    GoldKey, SilverKey,
    MachineGun, Chaingun,
    Lamp, Table,
};

struct Spawn {
    SpawnKind kind;
    int tx;   // tile coordinates; actors are placed at the cell centre
    int ty;
};

// What a Use press accomplished, so the caller can play a sound or flash a
// message without re-deriving what was in front of the player.
enum class UseResult : uint8_t {
    Nothing,
    DoorOpened,
    DoorLocked,   // right kind of door, wrong keyring
    PushwallMoved,
    ExitReached,
};

class Map {
public:
    // Builds the hand-authored first level. Any malformed row is a
    // programming error and trips an assert rather than half-loading.
    static Map level1();

    int width()  const { return w_; }
    int height() const { return h_; }

    // Out-of-bounds reads return a solid wall so callers never need their
    // own bounds checks in the inner loops.
    const MapCell& at(int x, int y) const;

    // Static geometry only: true for walls, doors and secrets regardless of
    // whether they happen to be open. The DDA uses this and then handles
    // doors specially.
    bool isSolid(int x, int y) const;

    bool isDoor(int x, int y) const;

    // Null when the cell holds no door.
    const Door* doorAt(int x, int y) const;

    // Blocks the player: solid tiles, plus doors that have not opened far
    // enough to walk through.
    bool blocksMovement(int x, int y) const;

    // Overlap test for a circle against any pushwall currently in motion.
    // Those have left the tile grid, so blocksMovement() cannot see them.
    bool hitsPushwall(double x, double y, double r) const;

    const std::vector<Pushwall>& pushwalls() const { return pushwalls_; }

    // Animates doors and pushwalls. Needs the player position so a door
    // never closes on top of them.
    void update(double dt, double px, double py);

    // Acts on whatever the player is facing, one cell ahead.
    UseResult use(double px, double py, double dirX, double dirY,
                  bool goldKey, bool silverKey);

    // Opens a door without a key check, for enemies walking into one.
    void openDoorFor(int x, int y);

    const std::vector<Spawn>& spawns() const { return spawns_; }

    // Convenience: the PlayerStart spawn, in world units (1 unit = 1 tile).
    double startX() const { return start_x_; }
    double startY() const { return start_y_; }

    // Secrets, for the end-of-level tally. Counted here rather than by the
    // caller because `pushwalls_` holds only the ones currently in motion —
    // a settled secret has left the list, so counting it from outside would
    // report every secret as un-found the moment it finished moving.
    int secretsTotal() const { return secrets_total_; }
    int secretsFound() const { return secrets_found_; }

private:
    // Parses the ASCII rows into cells and spawns. See map.cpp for the legend.
    void parse(const std::vector<std::string>& rows);
    // Decides each door's orientation from the walls around it.
    void orientDoors();

    Door* doorAtMut(int x, int y);
    MapCell& cellAt(int x, int y);

    int w_ = 0;
    int h_ = 0;
    std::vector<MapCell>  cells_;
    std::vector<Spawn>    spawns_;
    std::vector<Door>     doors_;
    std::vector<Pushwall> pushwalls_;
    // Tile -> index into doors_, or -1. Keeps doorAt() O(1) in the ray loop.
    std::vector<int>      door_index_;

    int secrets_total_ = 0;
    int secrets_found_ = 0;

    double start_x_ = 1.5;
    double start_y_ = 1.5;

    MapCell solid_{TileKind::Wall, TexBrick};
};

} // namespace wolf
