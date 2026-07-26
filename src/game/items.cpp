#include "items.h"

#include <cmath>

#include "../render/sprite_set.h"
#include "map.h"
#include "player.h"

namespace wolf {
namespace {

// Pickup values. Small enough that a medkit is worth remembering the
// location of rather than a formality.
constexpr int kHealthPickup = 25;
constexpr int kAmmoPickup   = 8;
constexpr int kWeaponAmmo   = 6;   // a dropped weapon comes part-loaded

// Treasure is the score game, so the four valuables are worth visibly
// different amounts rather than being reskins of one number.
constexpr int kTreasureScore[4] = {100, 500, 1000, 5000};

// Radius used for blocking scenery. Under half a cell, so you can squeeze
// past a table in a doorway instead of being walled out by furniture.
constexpr double kDecorRadius = 0.28;

} // namespace

void Items::spawnFrom(const Map& map) {
    items_.clear();
    treasure_total_ = 0;
    treasure_taken_ = 0;

    int treasureIndex = 0;

    for (const Spawn& s : map.spawns()) {
        Item it;
        // Actors are placed at the centre of their cell, so items line up
        // with the tile the player walks onto.
        it.x = s.tx + 0.5;
        it.y = s.ty + 0.5;

        switch (s.kind) {
        case SpawnKind::Health:
            it.kind = ItemKind::Health;   it.sprite = SprHealth; break;
        case SpawnKind::Ammo:
            it.kind = ItemKind::Ammo;     it.sprite = SprAmmo;   break;
        case SpawnKind::Treasure: {
            it.kind = ItemKind::Treasure;
            // Cycle the four valuables so a hoard does not look stamped out.
            static const uint8_t kLook[4] = {
                SprTreasureCross, SprTreasureChalice,
                SprTreasureChest, SprTreasureCrown};
            it.sprite = kLook[treasureIndex % 4];
            ++treasureIndex;
            ++treasure_total_;
            break;
        }
        case SpawnKind::GoldKey:
            it.kind = ItemKind::GoldKey;    it.sprite = SprGoldKey;    break;
        case SpawnKind::SilverKey:
            it.kind = ItemKind::SilverKey;  it.sprite = SprSilverKey;  break;
        case SpawnKind::MachineGun:
            it.kind = ItemKind::MachineGun; it.sprite = SprMachineGun; break;
        case SpawnKind::Chaingun:
            it.kind = ItemKind::Chaingun;   it.sprite = SprChaingun;   break;
        case SpawnKind::Lamp:
            it.kind = ItemKind::Decor; it.sprite = SprLamp;  it.blocking = true; break;
        case SpawnKind::Table:
            it.kind = ItemKind::Decor; it.sprite = SprTable; it.blocking = true; break;

        // Actors are not items; enemies arrive in the next phase.
        case SpawnKind::PlayerStart:
        case SpawnKind::Guard:
        case SpawnKind::SS:
            continue;
        }

        items_.push_back(it);
    }
}

void Items::collect(Player& player) {
    const int ptx = static_cast<int>(std::floor(player.x()));
    const int pty = static_cast<int>(std::floor(player.y()));

    for (Item& it : items_) {
        if (it.taken || it.kind == ItemKind::Decor) continue;

        // Tile-based collection, as in the original. A radius test looks
        // more precise but plays worse: you end up brushing past a medkit
        // without taking it and having to shuffle back onto it.
        if (static_cast<int>(std::floor(it.x)) != ptx) continue;
        if (static_cast<int>(std::floor(it.y)) != pty) continue;

        switch (it.kind) {
        case ItemKind::Health:
            // Refused when already full, so the pickup stays where it is
            // for the trip back rather than being thrown away.
            if (!player.giveHealth(kHealthPickup)) continue;
            break;
        case ItemKind::Ammo:
            if (!player.giveAmmo(kAmmoPickup)) continue;
            break;
        case ItemKind::Treasure:
            player.addScore(kTreasureScore[treasure_taken_ % 4]);
            ++treasure_taken_;
            break;
        case ItemKind::GoldKey:    player.giveGoldKey();   break;
        case ItemKind::SilverKey:  player.giveSilverKey(); break;
        case ItemKind::MachineGun:
            player.giveMachineGun();
            player.giveAmmo(kWeaponAmmo);
            break;
        case ItemKind::Chaingun:
            player.giveChaingun();
            player.giveAmmo(kWeaponAmmo);
            break;
        case ItemKind::Decor:
            continue;
        }

        it.taken = true;
    }
}

void Items::appendBillboards(std::vector<Billboard>& out,
                             const SpriteSet& sprites) const {
    for (const Item& it : items_) {
        if (it.taken) continue;
        out.push_back(Billboard{it.x, it.y, &sprites[it.sprite]});
    }
}

int Items::remaining() const {
    int n = 0;
    for (const Item& it : items_)
        if (!it.taken) ++n;
    return n;
}

bool Items::blocks(double x, double y, double r) const {
    const double reach = r + kDecorRadius;
    for (const Item& it : items_) {
        if (!it.blocking || it.taken) continue;
        const double dx = x - it.x;
        const double dy = y - it.y;
        if (dx * dx + dy * dy < reach * reach) return true;
    }
    return false;
}

} // namespace wolf
