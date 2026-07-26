// The first-person weapon view model, drawn by code like everything else.
//
// `WeaponType` lives here rather than in game/weapons.h for the same reason
// `EnemyType` lives in actor_sprites.h: the art has to be indexed by it, and
// putting it in the render layer is what keeps render/ from ever including
// game/. The tuning that makes a chaingun a chaingun stays in game/.
//
// Each weapon is four frames — at rest, then three of firing. The original
// used the same count, and it is genuinely enough: at 320x200 a firing gun
// reads as a recoil jolt and a flash, and extra inbetweens are invisible at
// the rate the thing cycles.
//
// The view model is drawn bottom-centre and scaled 2x, so the art sits low
// in its 64x64 frame; the empty top half is what the player sees through.
#pragma once

#include <array>
#include <cstdint>

#include "sprite.h"

namespace wolf {

enum class WeaponType : uint8_t { Knife, Pistol, MachineGun, Chaingun, Count };

class WeaponSprites {
public:
    static constexpr int kFrames = 4;

    void generate();

    // Out-of-range values are clamped rather than asserted: a wrong frame is
    // a cosmetic glitch, and a crash mid-firefight is not.
    const Sprite& frame(WeaponType w, int frame) const;

private:
    std::array<std::array<Sprite, kFrames>,
               static_cast<size_t>(WeaponType::Count)> spr_{};
};

// Draws the view model into the 3D view. `bob` is the walk cycle phase in
// radians; the weapon sways with it, which is most of what sells first-person
// movement when there is no head bob on the camera itself.
void renderWeapon(Framebuffer& fb, const Sprite& art, double bob);

// Washes the view toward white for one muzzle flash. Applied to the 3D view
// only — a flash that lit the status bar would read as a rendering bug.
void applyMuzzleFlash(Framebuffer& fb, double intensity);

} // namespace wolf
