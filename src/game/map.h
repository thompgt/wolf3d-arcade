// The level: a tile grid plus the list of things that spawn in it.
//
// Like the original, the world is strictly grid-aligned — every wall is a
// full cell, which is exactly what makes the DDA raycaster cheap and exact.
// Levels are authored as ASCII art (see map.cpp) so they stay readable and
// editable in a text editor.
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

    // Blocks movement and sight. Doors count as solid here; once doors are
    // animated (phase 3) their open state is tracked separately.
    bool isSolid(int x, int y) const;

    // True for the door family, which the raycaster draws inset at the
    // cell midline rather than flush with the cell edge.
    bool isDoor(int x, int y) const;

    const std::vector<Spawn>& spawns() const { return spawns_; }

    // Convenience: the PlayerStart spawn, in world units (1 unit = 1 tile).
    double startX() const { return start_x_; }
    double startY() const { return start_y_; }

private:
    // Parses the ASCII rows into cells and spawns. See map.cpp for the legend.
    void parse(const std::vector<std::string>& rows);

    int w_ = 0;
    int h_ = 0;
    std::vector<MapCell>  cells_;
    std::vector<Spawn>    spawns_;
    double start_x_ = 1.5;
    double start_y_ = 1.5;

    MapCell solid_{TileKind::Wall, TexBrick};
};

} // namespace wolf
