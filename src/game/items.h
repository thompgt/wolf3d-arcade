// Pickups and scenery: everything in the world that is an object rather
// than geometry, and is not trying to kill you.
//
// Items are billboards, so they have a position but no facing and no
// footprint on the tile grid. Scenery that blocks movement therefore needs
// its own collision test — a lamp is not a wall and cannot be one, because
// a wall would also stop rays and light.
#pragma once

#include <cstdint>
#include <vector>

#include "../render/sprites.h"

namespace wolf {

class Map;
class Player;

enum class ItemKind : uint8_t {
    Health, Ammo, Treasure,
    GoldKey, SilverKey,
    MachineGun, Chaingun,
    Decor,          // never collected; may or may not block
};

struct Item {
    double   x = 0.0, y = 0.0;
    uint8_t  sprite = 0;
    ItemKind kind = ItemKind::Decor;
    bool     blocking = false;
    bool     taken = false;
};

class Items {
public:
    // Reads the map's spawn list. Treasure cycles through the four
    // different valuables so a room full of it does not look stamped out.
    void spawnFrom(const Map& map);

    // Collects anything the player is standing on. Collection is by tile,
    // as in the original: walking over an item takes it, and there is no
    // fiddly radius to miss by a pixel.
    void collect(Player& player);

    // Adds every item still in the world to the render list.
    void appendBillboards(std::vector<Billboard>& out) const;

    // True if a circle at (x, y) overlaps blocking scenery.
    bool blocks(double x, double y, double r) const;

    int treasureTotal() const { return treasure_total_; }
    int treasureTaken() const { return treasure_taken_; }

private:
    std::vector<Item> items_;
    int treasure_total_ = 0;
    int treasure_taken_ = 0;
};

} // namespace wolf
