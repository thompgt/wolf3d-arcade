#include "actor_sprites.h"

#include <algorithm>
#include <cmath>

namespace wolf {
namespace {

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

constexpr uint32_t kInk = rgb(0x10, 0x0c, 0x0a);

// Everything that distinguishes one enemy type from another. Two guards
// that behave differently but are drawn by the same routine read as the
// same army, which is the intent.
struct Uniform {
    uint32_t coat;
    uint32_t coatDark;   // arms and pack, so limbs separate from the torso
    uint32_t trouser;
    uint32_t helmet;
    uint32_t helmetDark; // brim, which is what makes a helmet a helmet
    uint32_t skin;
    uint32_t boot;
    uint32_t belt;
};

constexpr Uniform kGuard{
    rgb(0x8c, 0x6c, 0x38),   // tan field coat
    rgb(0x66, 0x4e, 0x28),
    rgb(0x6a, 0x52, 0x2c),
    rgb(0x5c, 0x5e, 0x48),
    rgb(0x34, 0x36, 0x28),
    rgb(0xe0, 0xb0, 0x88),
    rgb(0x2a, 0x20, 0x18),
    rgb(0x3a, 0x2c, 0x1c),
};

constexpr Uniform kSS{
    rgb(0x3a, 0x46, 0x62),   // blue-grey, deliberately colder than the guard
    rgb(0x28, 0x30, 0x46),
    rgb(0x2c, 0x34, 0x4a),
    rgb(0x24, 0x28, 0x30),
    rgb(0x12, 0x14, 0x18),
    rgb(0xe0, 0xb0, 0x88),
    rgb(0x18, 0x18, 0x1c),
    rgb(0x14, 0x14, 0x18),
};

// Vertical layout of a standing figure inside the 64x64 frame. The feet sit
// at 60 rather than 64 so the figure stands on the floor line instead of
// being buried in it.
constexpr int kHeadTop   = 13;
constexpr int kHeadBot   = 27;
constexpr int kTorsoTop  = 27;
constexpr int kTorsoBot  = 45;
constexpr int kLegTop    = 44;
constexpr int kFeet      = 60;
constexpr int kCentreX   = 32;

// How a figure is foreshortened and which features are visible, per view.
struct View {
    double torsoHW;   // torso half-width; a profile is much narrower
    double shift;     // lateral offset of head and gun, suggesting the turn
    bool   face;      // are we looking at the front of the head
    bool   back;      // are we looking at his back
    bool   profile;
};

View viewFor(int bucket) {
    const double a = bucket * kTwoPi / ActorSprites::kAngles;
    const double s = std::sin(a);
    const double c = std::cos(a);

    View v;
    // Widest seen face-on or from behind, narrowest in profile.
    v.torsoHW = 6.0 + 4.0 * std::abs(c);
    v.shift   = s * 3.5;
    v.face    = c > 0.35;
    v.back    = c < -0.35;
    v.profile = std::abs(c) < 0.35;
    return v;
}

// --- figure parts --------------------------------------------------------

void drawLegs(Sprite& s, const Uniform& u, const View& v, double swing) {
    const int hw = static_cast<int>(v.torsoHW);
    // In profile the legs overlap, so the rear one is drawn first, darker,
    // and offset — otherwise a walking figure reads as a single thick post.
    const int spread = v.profile ? 2 : std::max(hw / 2, 3);

    const int frontX = kCentreX - spread + static_cast<int>(swing);
    const int backX  = kCentreX + spread - static_cast<int>(swing);

    sprBox(s, backX - 3, kLegTop, backX + 3, kFeet - 3,
           v.profile ? u.helmet : u.trouser);
    sprBox(s, backX - 4, kFeet - 4, backX + 4, kFeet, u.boot);

    sprBox(s, frontX - 3, kLegTop, frontX + 3, kFeet - 3, u.trouser);
    sprBox(s, frontX - 4, kFeet - 4, frontX + 4, kFeet, u.boot);
}

void drawTorso(Sprite& s, const Uniform& u, const View& v) {
    const int hw = static_cast<int>(v.torsoHW);
    sprBox(s, kCentreX - hw, kTorsoTop, kCentreX + hw, kTorsoBot, u.coat);
    // Belt, which also visually separates coat from trousers.
    sprBox(s, kCentreX - hw, kTorsoBot - 4, kCentreX + hw, kTorsoBot - 1, u.belt);
    // Shoulders: a slightly wider band reads as a uniform rather than a box.
    sprBox(s, kCentreX - hw - 1, kTorsoTop, kCentreX + hw + 1, kTorsoTop + 4, u.coat);

    if (v.back) {
        // A backpack and a vertical seam. Without something here, the back
        // view is just the front view minus a face, and at sprite scale
        // that is not a difference a player can act on — yet knowing a
        // guard has not turned round is the whole basis of sneaking up.
        sprBox(s, kCentreX - hw + 2, kTorsoTop + 3, kCentreX + hw - 2,
               kTorsoBot - 6, u.coatDark);
        sprBox(s, kCentreX - 1, kTorsoTop + 3, kCentreX + 1, kTorsoBot - 6, u.belt);
    } else if (v.face) {
        // Open collar and a lapel line, so the front is busier than the back.
        sprBox(s, kCentreX - 4, kTorsoTop, kCentreX + 4, kTorsoTop + 5, u.coatDark);
        sprBox(s, kCentreX - 1, kTorsoTop + 4, kCentreX + 1, kTorsoBot - 5, u.coatDark);
    }
}

void drawHead(Sprite& s, const Uniform& u, const View& v) {
    const int hx = kCentreX + static_cast<int>(v.shift);
    const int cy = (kHeadTop + kHeadBot) / 2;

    // Neck first, so the head sits on it rather than floating.
    sprBox(s, hx - 3, cy + 4, hx + 3, kTorsoTop + 1, u.skin);

    if (v.face || v.profile) sprEllipse(s, hx, cy + 1, 6.0, 6.5, u.skin);

    // Helmet: dome, then a wider brim in the darker shade. The brim is what
    // separates a helmet from a haircut at this size.
    sprEllipse(s, hx, cy - 2.0, 7.5, 6.5, u.helmet);
    sprBox(s, hx - 9, cy - 1, hx + 9, cy + 2, u.helmetDark);
    sprBox(s, hx - 8, cy - 3, hx + 8, cy - 1, u.helmet);

    if (v.face) {
        // Eyes only. A mouth at this scale becomes a smudge that reads as
        // damage rather than a face.
        sprBox(s, hx - 4, cy + 3, hx - 2, cy + 5, kInk);
        sprBox(s, hx + 2, cy + 3, hx + 4, cy + 5, kInk);
    } else if (v.profile) {
        // Nose and ear: a profile has to be distinguishable from a back
        // view, and the silhouette is the only place to say so.
        const int dir = (v.shift >= 0.0) ? 1 : -1;
        sprBox(s, hx + dir * 5, cy + 2, hx + dir * 7, cy + 4, u.skin);
        sprBox(s, hx - dir * 2, cy + 2, hx - dir * 1, cy + 5,
               rgb(0xb0, 0x84, 0x62));
    } else {
        // Back of the head: helmet all the way down, plus a chin strap.
        sprEllipse(s, hx, cy + 1, 6.5, 6.0, u.helmet);
        sprBox(s, hx - 6, cy + 4, hx + 6, cy + 6, u.helmetDark);
    }
}

// Arms and weapon. `aim` runs 0 (weapon down, at rest) to 1 (levelled at
// you), which is the single clearest cue that an enemy has started firing.
void drawArmsAndGun(Sprite& s, const Uniform& u, const View& v, double aim,
                    bool flash) {
    const int hw = static_cast<int>(v.torsoHW);
    const int gunY = static_cast<int>(kTorsoTop + 8 - aim * 4.0);

    // Arms in the darker shade. Drawn in the coat colour they merge into the
    // torso and the figure loses its limbs entirely.
    sprBox(s, kCentreX - hw - 4, kTorsoTop + 2, kCentreX - hw + 1, gunY + 6, u.coatDark);
    sprBox(s, kCentreX + hw - 1, kTorsoTop + 2, kCentreX + hw + 4, gunY + 6, u.coatDark);
    // Hands.
    sprBox(s, kCentreX - hw - 4, gunY + 4, kCentreX - hw, gunY + 8, u.skin);
    sprBox(s, kCentreX + hw, gunY + 4, kCentreX + hw + 4, gunY + 8, u.skin);

    const uint32_t steel = rgb(0x8a, 0x90, 0x9c);
    const uint32_t wood  = rgb(0x6c, 0x44, 0x24);

    if (aim > 0.5) {
        // Levelled at the viewer: the barrel is foreshortened to a stub with
        // the muzzle facing out, so it reads as pointing at you rather than
        // lying across the chest.
        sprBox(s, kCentreX - 3, gunY, kCentreX + 3, gunY + 10, steel);
        sprEllipse(s, kCentreX, gunY + 2, 3.0, 3.0, rgb(0x24, 0x26, 0x2a));
        if (flash) {
            sprEllipse(s, kCentreX, gunY + 1, 7.0, 6.0, rgb(0xff, 0xf0, 0x90));
            sprEllipse(s, kCentreX, gunY + 1, 4.0, 3.5, rgb(0xff, 0xff, 0xe0));
        }
    } else {
        // Slung across the body, angled toward whichever way he faces.
        const int gx = kCentreX + static_cast<int>(v.shift * 1.8);
        sprBox(s, gx - 11, gunY + 4, gx + 8, gunY + 8, steel);
        sprBox(s, gx - 13, gunY + 5, gx - 9, gunY + 7, rgb(0x2c, 0x2e, 0x34));
        sprBox(s, gx + 6, gunY + 6, gx + 13, gunY + 11, wood);
    }
}

// --- poses ---------------------------------------------------------------

void finish(Sprite& s) {
    sprShadeAcross(s, 14, 50, 1.14, 0.74);
    sprOutline(s, kInk);
}

void drawUpright(Sprite& s, const Uniform& u, int bucket, double swing,
                 double aim, bool flash) {
    sprClear(s);
    const View v = viewFor(bucket);
    drawLegs(s, u, v, swing);
    drawTorso(s, u, v);
    drawArmsAndGun(s, u, v, aim, flash);
    drawHead(s, u, v);
    finish(s);
}

// Recoiling from a hit: thrown back, head snapped up, weapon dropping.
void drawPain(Sprite& s, const Uniform& u) {
    sprClear(s);
    View v = viewFor(0);
    v.shift = -2.5;
    drawLegs(s, u, v, -3.0);
    drawTorso(s, u, v);
    drawArmsAndGun(s, u, v, 0.0, false);
    drawHead(s, u, v);
    // Wash the whole figure toward white. A flinch has to be readable in the
    // single frame it occupies, and a pose change alone is not enough at
    // this size.
    for (int y = 0; y < kTexSize; ++y) {
        for (int x = 0; x < kTexSize; ++x) {
            const uint32_t c = s.at(x, y);
            if (isTransparent(c)) continue;
            const auto ch = [](uint32_t val) {
                return static_cast<uint8_t>(std::min(255.0, val * 0.55 + 110.0));
            };
            s.set(x, y, rgb(ch((c >> 16) & 0xFF), ch((c >> 8) & 0xFF), ch(c & 0xFF)));
        }
    }
    sprOutline(s, kInk);
}

// Five frames from standing to a corpse on the floor. The figure collapses
// by squashing toward the floor line and spreading outward, so the last
// frame is a low heap rather than a shrunken standing man.
void drawDeath(Sprite& s, const Uniform& u, int frame) {
    const double t = frame / static_cast<double>(ActorSprites::kDieFrames - 1);

    Sprite upright;
    drawUpright(upright, u, 0, 0.0, 0.0, false);
    sprClear(s);

    // Vertical squash and horizontal stretch about the floor line.
    const double squash  = 1.0 - 0.82 * t;
    const double stretch = 1.0 + 1.05 * t;

    // Inverse mapping: walk the destination and pull from the source.
    //
    // Forward mapping — walking the source and pushing to the destination —
    // is the obvious way round and produces a body that dissolves into
    // vertical stripes as it spreads, because a stretch leaves destination
    // pixels that no source pixel ever lands on. The result reads as a
    // barcode rather than a collapsing man, and no amount of filling in
    // neighbours patches it properly.
    for (int dy = 0; dy < kTexSize; ++dy) {
        const double sy = kFeet - (kFeet - dy) / squash;
        if (sy < 0.0 || sy >= kTexSize) continue;
        for (int dx = 0; dx < kTexSize; ++dx) {
            const double sx = kCentreX + (dx - kCentreX) / stretch;
            if (sx < 0.0 || sx >= kTexSize) continue;
            const uint32_t c = upright.at(static_cast<int>(sx),
                                          static_cast<int>(sy));
            if (isTransparent(c)) continue;
            s.set(dx, dy, c);
        }
    }

    // Final frame gets a pool of blood so a corpse is unmistakably a corpse
    // and not an enemy lying in wait.
    if (frame == ActorSprites::kDieFrames - 1) {
        sprEllipse(s, kCentreX, kFeet - 1, 20.0, 4.0, rgb(0x8c, 0x14, 0x10));
        sprEllipse(s, kCentreX - 6, kFeet - 2, 10.0, 3.0, rgb(0xb0, 0x1c, 0x14));
    }
    sprOutline(s, kInk);
}

int frameIndex(ActorPose pose, int angleBucket, int frame) {
    switch (pose) {
    case ActorPose::Stand:
        return angleBucket;
    case ActorPose::Walk:
        return 8 + angleBucket * ActorSprites::kWalkFrames + frame;
    case ActorPose::Shoot:
        return 40 + frame;
    case ActorPose::Pain:
        return 43;
    case ActorPose::Die:
        return 44 + frame;
    }
    return 0;
}

} // namespace

int viewBucket(double actorAngle, double viewAngle) {
    // Angle of the actor's facing relative to the direction we are looking
    // at him from. Bucket 0 is face-on, bucket 4 is his back.
    double rel = actorAngle - viewAngle + kPi;
    rel = std::fmod(rel, kTwoPi);
    if (rel < 0.0) rel += kTwoPi;
    // Offset by half a bucket so each bucket is centred on its angle rather
    // than starting at it, otherwise the sprite flips a bucket early.
    const int b = static_cast<int>(rel / kTwoPi * ActorSprites::kAngles + 0.5);
    return b % ActorSprites::kAngles;
}

void ActorSprites::generate() {
    const Uniform kit[static_cast<size_t>(EnemyType::Count)] = {kGuard, kSS};

    for (size_t t = 0; t < static_cast<size_t>(EnemyType::Count); ++t) {
        const Uniform& u = kit[t];
        auto& set = spr_[t];

        for (int a = 0; a < kAngles; ++a) {
            drawUpright(set[frameIndex(ActorPose::Stand, a, 0)], u, a, 0.0, 0.0, false);

            for (int f = 0; f < kWalkFrames; ++f) {
                // Legs swing through a full cycle over the four frames.
                const double swing = std::sin(f * kTwoPi / kWalkFrames) * 4.0;
                drawUpright(set[frameIndex(ActorPose::Walk, a, f)], u, a,
                            swing, 0.0, false);
            }
        }

        // Raise, fire, recover. The flash is on the middle frame only, so it
        // lasts exactly as long as the shot rather than the whole animation.
        drawUpright(set[frameIndex(ActorPose::Shoot, 0, 0)], u, 0, 0.0, 0.7, false);
        drawUpright(set[frameIndex(ActorPose::Shoot, 0, 1)], u, 0, 0.0, 1.0, true);
        drawUpright(set[frameIndex(ActorPose::Shoot, 0, 2)], u, 0, 0.0, 1.0, false);

        drawPain(set[frameIndex(ActorPose::Pain, 0, 0)], u);

        for (int f = 0; f < kDieFrames; ++f)
            drawDeath(set[frameIndex(ActorPose::Die, 0, f)], u, f);
    }
}

const Sprite& ActorSprites::frame(EnemyType type, ActorPose pose,
                                  int angleBucket, int frame) const {
    const int a = std::clamp(angleBucket, 0, kAngles - 1);
    int f = frame;
    switch (pose) {
    case ActorPose::Walk:  f = std::clamp(f, 0, kWalkFrames - 1);  break;
    case ActorPose::Shoot: f = std::clamp(f, 0, kShootFrames - 1); break;
    case ActorPose::Die:   f = std::clamp(f, 0, kDieFrames - 1);   break;
    default:               f = 0;                                  break;
    }
    const size_t t = std::min(static_cast<size_t>(type),
                              static_cast<size_t>(EnemyType::Count) - 1);
    return spr_[t][static_cast<size_t>(frameIndex(pose, a, f))];
}

} // namespace wolf
