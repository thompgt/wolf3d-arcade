#include "player.h"

#include <cmath>

#include "../core/config.h"
#include "../platform/win32_app.h"

namespace wolf {
namespace {

// Half-width of the camera plane. tan(fov/2) for a 66-degree field of view,
// which is the classic 320x200 look; widening it noticeably distorts the
// edges of the screen because there is no perspective correction to hide it.
const double kPlaneLen = std::tan(kFov * 0.5);

} // namespace

void Player::spawn(double x, double y, double angle) {
    x_ = x;
    y_ = y;
    setAngle(angle);
}

void Player::setAngle(double a) {
    // Keep the angle bounded so it never loses precision over a long session.
    constexpr double kTwoPi = 6.283185307179586;
    while (a < 0.0)      a += kTwoPi;
    while (a >= kTwoPi)  a -= kTwoPi;

    angle_ = a;
    dir_x_ = std::cos(a);
    dir_y_ = std::sin(a);
    // Plane is dir rotated 90 degrees, scaled to the half-FOV.
    plane_x_ = -dir_y_ * kPlaneLen;
    plane_y_ =  dir_x_ * kPlaneLen;
}

void Player::update(const Platform& in, const Map& map, double dt) {
    // --- turning -----------------------------------------------------
    double turn = 0.0;
    if (in.down(Key::TurnLeft))  turn -= tuning_.turnSpeed * dt;
    if (in.down(Key::TurnRight)) turn += tuning_.turnSpeed * dt;
    // Mouse motion is already a delta, so it must not be scaled by dt or it
    // would feel different at different frame rates.
    turn += in.mouseDX() * tuning_.mouseSens;
    if (turn != 0.0) setAngle(angle_ + turn);

    // --- movement ----------------------------------------------------
    double fwd = 0.0, strafe = 0.0;
    if (in.down(Key::Forward))     fwd    += 1.0;
    if (in.down(Key::Back))        fwd    -= 1.0;
    if (in.down(Key::StrafeRight)) strafe += 1.0;
    if (in.down(Key::StrafeLeft))  strafe -= 1.0;

    if (fwd == 0.0 && strafe == 0.0) return;

    // Normalise so moving diagonally isn't faster than moving straight.
    const double len = std::sqrt(fwd * fwd + strafe * strafe);
    fwd    /= len;
    strafe /= len;

    const double speed = in.down(Key::Run) ? tuning_.runSpeed : tuning_.walkSpeed;
    // Strafe direction is the camera plane, normalised back to unit length
    // (the plane itself is scaled by the FOV).
    const double rightX = -dir_y_;
    const double rightY =  dir_x_;

    const double dx = (dir_x_ * fwd + rightX * strafe) * speed * dt;
    const double dy = (dir_y_ * fwd + rightY * strafe) * speed * dt;
    moveWithCollision(map, dx, dy);
}

void Player::moveWithCollision(const Map& map, double dx, double dy) {
    if (!collides(map, x_ + dx, y_)) x_ += dx;
    if (!collides(map, x_, y_ + dy)) y_ += dy;
}

bool Player::collides(const Map& map, double x, double y) const {
    const double r = tuning_.radius;
    const int x0 = static_cast<int>(std::floor(x - r));
    const int x1 = static_cast<int>(std::floor(x + r));
    const int y0 = static_cast<int>(std::floor(y - r));
    const int y1 = static_cast<int>(std::floor(y + r));

    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (map.isSolid(tx, ty)) return true;

    return false;
}

} // namespace wolf
