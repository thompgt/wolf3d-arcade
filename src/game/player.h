// The player's position and camera.
//
// The camera is stored the way the original raycaster wants it: a direction
// vector plus a perpendicular "plane" vector whose length encodes the field
// of view. A screen column is then just dir + plane * cameraX, which keeps
// the per-column work down to a couple of multiplies.
#pragma once

#include "map.h"

namespace wolf {

class Platform;
class Items;
class Enemies;

// Tuning. Speeds are in tiles per second, turn rates in radians per second.
struct PlayerTuning {
    double walkSpeed   = 3.2;
    double runSpeed    = 5.4;
    double turnSpeed   = 2.6;
    double mouseSens   = 0.0032;  // radians per pixel of raw mouse motion
    double radius      = 0.24;    // collision circle; a little under half a
                                  // tile so doorways feel generous
};

class Player {
public:
    void spawn(double x, double y, double angle);

    // Items and Enemies are passed in because blocking scenery and living
    // guards are billboards, not cells — no amount of map lookup will find
    // them, and walking through a guard would look ridiculous.
    void update(const Platform& in, const Map& map, const Items& items,
                const Enemies& enemies, double dt);

    double x() const { return x_; }
    double y() const { return y_; }
    double angle() const { return angle_; }

    // Camera basis, recomputed whenever the angle changes.
    double dirX()   const { return dir_x_; }
    double dirY()   const { return dir_y_; }
    double planeX() const { return plane_x_; }
    double planeY() const { return plane_y_; }

    const PlayerTuning& tuning() const { return tuning_; }

    bool hasGoldKey()   const { return gold_key_; }
    bool hasSilverKey() const { return silver_key_; }
    void giveGoldKey()   { gold_key_ = true; }
    void giveSilverKey() { silver_key_ = true; }

    int  health() const { return health_; }
    int  ammo()   const { return ammo_; }
    int  score()  const { return score_; }
    int  lives()  const { return lives_; }
    bool hasMachineGun() const { return machine_gun_; }
    bool hasChaingun()   const { return chaingun_; }

    // Pickups return false when the player is already topped up, so an item
    // is left in the world rather than wasted — the original did the same,
    // and it stops a full-health player hoovering up the medkit they will
    // want on the way back.
    bool giveHealth(int amount);
    bool giveAmmo(int amount);
    void addScore(int amount) { score_ += amount; }

    // Spends rounds from the pool shared by all three firearms. Returns
    // false and spends nothing if there are not enough — a partial burst
    // would let a two-round weapon fire on one.
    bool useAmmo(int amount);

    void damage(int amount);
    bool isDead() const { return health_ <= 0; }

    // True if the player moved on the last tick. Enemy accuracy consults
    // this, which is what makes strafing an actual defence rather than a
    // cosmetic one.
    bool isMoving() const { return moving_; }
    void giveMachineGun() { machine_gun_ = true; }
    void giveChaingun()   { chaingun_ = true; }

private:
    void setAngle(double a);

    // Moves along one axis at a time and reverts only the axis that hit
    // something. That separation is what lets the player slide along a wall
    // instead of sticking to it when walking into it at an angle.
    void moveWithCollision(const Map& map, const Items& items,
                           const Enemies& enemies, double dx, double dy);

    // True if a circle of the player's radius at (x, y) overlaps any solid
    // cell. Checked against the tile span the circle covers, so a corner
    // never slips through diagonally.
    bool collides(const Map& map, const Items& items, const Enemies& enemies,
                  double x, double y) const;

    double x_ = 1.5, y_ = 1.5;
    double angle_ = 0.0;
    double dir_x_ = 1.0, dir_y_ = 0.0;
    double plane_x_ = 0.0, plane_y_ = 0.66;

    bool gold_key_   = false;
    bool silver_key_ = false;

    int  health_ = 100;
    int  ammo_   = 8;
    int  score_  = 0;
    int  lives_  = 3;
    bool machine_gun_ = false;
    bool chaingun_    = false;
    bool moving_      = false;

    PlayerTuning tuning_;
};

} // namespace wolf
