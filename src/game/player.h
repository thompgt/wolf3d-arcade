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

    void update(const Platform& in, const Map& map, double dt);

    double x() const { return x_; }
    double y() const { return y_; }
    double angle() const { return angle_; }

    // Camera basis, recomputed whenever the angle changes.
    double dirX()   const { return dir_x_; }
    double dirY()   const { return dir_y_; }
    double planeX() const { return plane_x_; }
    double planeY() const { return plane_y_; }

    const PlayerTuning& tuning() const { return tuning_; }

private:
    void setAngle(double a);

    // Moves along one axis at a time and reverts only the axis that hit
    // something. That separation is what lets the player slide along a wall
    // instead of sticking to it when walking into it at an angle.
    void moveWithCollision(const Map& map, double dx, double dy);

    // True if a circle of the player's radius at (x, y) overlaps any solid
    // cell. Checked against the tile span the circle covers, so a corner
    // never slips through diagonally.
    bool collides(const Map& map, double x, double y) const;

    double x_ = 1.5, y_ = 1.5;
    double angle_ = 0.0;
    double dir_x_ = 1.0, dir_y_ = 0.0;
    double plane_x_ = 0.0, plane_y_ = 0.66;

    PlayerTuning tuning_;
};

} // namespace wolf
