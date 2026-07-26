// Enemy sprites, drawn by code rather than loaded.
//
// A guard needs to be legible from any angle, so the figure is built
// parametrically: the same head/torso/arms/legs/gun routine runs with
// widths, offsets and a facing shift driven by which of eight view buckets
// the enemy is being seen from. That is far more maintainable than 49
// hand-authored frames per enemy type, and it means a change to the uniform
// is one constant rather than a repaint.
//
// Which angle you see is what tells you whether a guard has noticed you —
// a back view means you still have the drop on him. It is gameplay
// information, not decoration, which is why all eight buckets exist.
//
// Attack, pain and death frames are single-view, as in the original: an
// enemy doing any of those is by definition dealing with you, so it faces
// you.
#pragma once

#include <array>
#include <cstdint>

#include "sprite.h"

namespace wolf {

enum class EnemyType : uint8_t { Guard, SS, Count };

enum class ActorPose : uint8_t { Stand, Walk, Shoot, Pain, Die };

class ActorSprites {
public:
    static constexpr int kAngles      = 8;
    static constexpr int kWalkFrames  = 4;
    static constexpr int kShootFrames = 3;
    static constexpr int kDieFrames   = 5;

    void generate();

    // angleBucket is ignored for the single-view poses; frame is ignored for
    // Stand and Pain. Out-of-range values are clamped rather than asserted,
    // because a one-frame animation glitch is a far better failure than a
    // crash mid-firefight.
    const Sprite& frame(EnemyType type, ActorPose pose,
                        int angleBucket, int frame) const;

private:
    // 8 stand + 32 walk + 3 shoot + 1 pain + 5 die.
    static constexpr int kFramesPerType = 49;

    std::array<std::array<Sprite, kFramesPerType>,
               static_cast<size_t>(EnemyType::Count)> spr_{};
};

// Which of the eight view buckets an enemy facing `actorAngle` falls into
// when seen from `viewAngle`. Exposed because the renderer and the debug
// tools both need it and must agree.
int viewBucket(double actorAngle, double viewAngle);

} // namespace wolf
