#include "hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "font.h"

namespace wolf {
namespace {

// --- palette -------------------------------------------------------------

constexpr uint32_t kBarFace   = rgb(0x00, 0x54, 0x00);
constexpr uint32_t kBarEdge   = rgb(0x00, 0x2c, 0x00);
constexpr uint32_t kBarLight  = rgb(0x00, 0x74, 0x00);
constexpr uint32_t kLabel     = rgb(0xc8, 0xc8, 0xc8);
constexpr uint32_t kValue     = rgb(0xff, 0xff, 0xff);
constexpr uint32_t kValueLow  = rgb(0xff, 0x50, 0x40);
constexpr uint32_t kShadow    = rgb(0x00, 0x28, 0x00);
constexpr uint32_t kGoldKey   = rgb(0xd8, 0xb0, 0x28);
constexpr uint32_t kSilverKey = rgb(0xc0, 0xc4, 0xcc);
constexpr uint32_t kKeyHole   = rgb(0x20, 0x20, 0x20);

constexpr uint32_t kSkin      = rgb(0xd8, 0xa0, 0x78);
constexpr uint32_t kSkinDark  = rgb(0xa8, 0x74, 0x50);
constexpr uint32_t kHair      = rgb(0x70, 0x50, 0x28);
constexpr uint32_t kHairLit   = rgb(0x98, 0x70, 0x38);
constexpr uint32_t kEyeWhite  = rgb(0xf0, 0xf0, 0xf0);
constexpr uint32_t kPupil     = rgb(0x20, 0x30, 0x60);
constexpr uint32_t kMouth     = rgb(0x50, 0x20, 0x20);
constexpr uint32_t kTeeth     = rgb(0xe8, 0xe8, 0xe0);
constexpr uint32_t kBlood     = rgb(0xb0, 0x18, 0x18);
constexpr uint32_t kFaceBg    = rgb(0x00, 0x2c, 0x00);

constexpr uint32_t kTitleBg   = rgb(0x18, 0x10, 0x10);
constexpr uint32_t kTitleFg   = rgb(0xd8, 0xa8, 0x28);
constexpr uint32_t kTitleDim  = rgb(0x88, 0x64, 0x18);
constexpr uint32_t kTitlePanel = rgb(0x2c, 0x18, 0x10);

// Short enough to fit the bar's last slot. The tuning table's names are for
// logs, not for a 46-pixel column.
const char* const kWeaponLabel[] = {"KNIFE", "PISTOL", "MG", "CHAIN"};

// --- the face ------------------------------------------------------------

// Head, hair and jaw. Everything else is drawn on top of this, so all the
// expressions sit on the same skull.
void drawHead(Sprite& s, int tier) {
    constexpr int cx = FaceSprites::kFaceW / 2;

    sprBox(s, 0, 0, FaceSprites::kFaceW, FaceSprites::kFaceH, kFaceBg);

    sprEllipse(s, cx, 17.0, 8.5, 10.0, kSkin);          // head
    sprEllipse(s, cx + 4.0, 18.0, 4.0, 8.0, kSkinDark); // shaded right side
    sprBox(s, cx - 6, 26, cx + 6, 32, kSkin);           // neck and shoulders
    sprBox(s, cx - 9, 29, cx + 9, 32, rgb(0x38, 0x50, 0x38));   // collar

    sprEllipse(s, cx, 8.0, 9.0, 5.0, kHair);            // hair
    sprEllipse(s, cx - 3.0, 6.5, 5.0, 2.5, kHairLit);

    // Blood arrives progressively rather than all at once: the face is a
    // health bar you read without counting, so it has to change at every
    // tier and not just at the bottom one.
    if (tier >= 2) sprBox(s, cx - 7, 10, cx - 3, 12, kBlood);
    if (tier >= 3) {
        sprBox(s, cx + 2, 12, cx + 7, 14, kBlood);
        sprBox(s, cx - 8, 18, cx - 6, 24, kBlood);
    }
    if (tier >= 4) {
        sprBox(s, cx - 5, 22, cx + 5, 24, kBlood);
        sprBox(s, cx + 5, 16, cx + 8, 26, kBlood);
        sprBox(s, cx - 2, 6, cx + 3, 9, kBlood);
    }
}

// Eyes looking one of three ways. The glance is what makes the portrait feel
// alive while you are standing still doing nothing.
void drawEyes(Sprite& s, int look, bool squint) {
    constexpr int cx = FaceSprites::kFaceW / 2;
    const int shift = look - 1;   // -1 left, 0 ahead, +1 right

    if (squint) {
        // Screwed shut, which is what sells a grimace at this size.
        sprBox(s, cx - 7, 15, cx - 2, 17, kSkinDark);
        sprBox(s, cx + 2, 15, cx + 7, 17, kSkinDark);
        return;
    }

    sprEllipse(s, cx - 4.0, 15.5, 2.6, 2.0, kEyeWhite);
    sprEllipse(s, cx + 4.0, 15.5, 2.6, 2.0, kEyeWhite);
    sprEllipse(s, cx - 4.0 + shift, 15.5, 1.3, 1.4, kPupil);
    sprEllipse(s, cx + 4.0 + shift, 15.5, 1.3, 1.4, kPupil);

    sprBox(s, cx - 7, 12, cx - 1, 13, kHair);    // brows
    sprBox(s, cx + 1, 12, cx + 7, 13, kHair);
}

void drawMouth(Sprite& s, int tier, bool grimace, bool gloat) {
    constexpr int cx = FaceSprites::kFaceW / 2;

    if (gloat) {
        // A grin with teeth. Only shows on a kill, so it reads as a reaction
        // rather than as the default face.
        sprEllipse(s, cx, 22.0, 5.0, 2.6, kMouth);
        sprBox(s, cx - 4, 21, cx + 4, 22, kTeeth);
        return;
    }
    if (grimace) {
        sprEllipse(s, cx, 23.0, 4.0, 2.8, kMouth);
        sprBox(s, cx - 3, 22, cx + 3, 23, kTeeth);
        return;
    }

    // Idle: a flat line that turns down as the tiers get worse.
    const int droop = tier / 2;
    sprBox(s, cx - 4, 22 + droop, cx + 4, 23 + droop, kMouth);
    if (tier >= 3) sprBox(s, cx - 4, 24 + droop, cx + 4, 25 + droop, kBlood);
}

void drawDeadFace(Sprite& s) {
    drawHead(s, 4);
    constexpr int cx = FaceSprites::kFaceW / 2;

    // Crossed-out eyes, the one piece of cartoon shorthand in the whole
    // game, and worth it: at 24x32 nothing else reads as dead.
    for (int i = -3; i <= 3; ++i) {
        s.set(cx - 4 + i, 15 + i, kMouth);
        s.set(cx - 4 + i, 15 - i, kMouth);
        s.set(cx + 4 + i, 15 + i, kMouth);
        s.set(cx + 4 + i, 15 - i, kMouth);
    }
    sprEllipse(s, cx, 23.0, 3.0, 2.0, kMouth);
    sprBox(s, cx - 8, 8, cx + 8, 10, kBlood);
}

} // namespace

void FaceSprites::generate() {
    for (int tier = 0; tier < kTiers; ++tier) {
        for (int look = 0; look < kLooks; ++look) {
            Sprite& s = idle_[static_cast<size_t>(tier * kLooks + look)];
            sprClear(s);
            drawHead(s, tier);
            drawEyes(s, look, false);
            drawMouth(s, tier, false, false);
        }
        Sprite& h = hurt_[static_cast<size_t>(tier)];
        sprClear(h);
        drawHead(h, tier);
        drawEyes(h, 1, true);
        drawMouth(h, tier, true, false);
    }

    sprClear(gloat_);
    drawHead(gloat_, 0);
    drawEyes(gloat_, 1, false);
    drawMouth(gloat_, 0, false, true);

    sprClear(dead_);
    drawDeadFace(dead_);
}

const Sprite& FaceSprites::pick(int health, int look, bool grimace,
                                bool gloat) const {
    if (health <= 0) return dead_;

    // Tier 0 is untouched, tier 4 is about to die. Derived from health
    // rather than stored, so there is one definition of "hurt".
    const int tier = std::clamp((100 - std::clamp(health, 1, 100)) / 20, 0,
                                kTiers - 1);

    // A grimace outranks a gloat: being hit is the more urgent thing to tell
    // the player about, and a face grinning through damage reads as a bug.
    if (grimace) return hurt_[static_cast<size_t>(tier)];
    if (gloat && tier == 0) return gloat_;

    const int l = std::clamp(look, 0, kLooks - 1);
    return idle_[static_cast<size_t>(tier * kLooks + l)];
}

namespace {

// Blits the used corner of a face sprite. The face is drawn into a 64x64
// Sprite so it can use the same primitives as everything else, but only the
// top-left 24x32 of it is the portrait.
void blitFace(Framebuffer& fb, const Sprite& s, int x, int y) {
    for (int fy = 0; fy < FaceSprites::kFaceH; ++fy)
        for (int fx = 0; fx < FaceSprites::kFaceW; ++fx) {
            const uint32_t c = s.at(fx, fy);
            if (isTransparent(c)) continue;
            fb.put(x + fx, y + fy, c);
        }
}

// A slot: a label in small text with its value under it. Every field on the
// bar is one of these, which is what keeps them on a common baseline.
void drawSlot(Framebuffer& fb, int x, int y, const char* label,
              const char* value, uint32_t valueColor) {
    drawText(fb, x, y, label, kLabel, 1);
    drawTextShadowed(fb, x, y + 11, value, valueColor, kShadow, 2);
}

void drawKeyIcon(Framebuffer& fb, int x, int y, uint32_t color, bool held) {
    if (!held) {
        // An empty socket, not nothing: a missing key has to be visibly
        // missing, or the player cannot tell the slot exists.
        fb.fillRect(x, y, 7, 9, kBarEdge);
        return;
    }
    fb.fillRect(x, y, 7, 9, color);
    fb.fillRect(x + 2, y + 2, 3, 2, kKeyHole);   // bow
    fb.fillRect(x + 3, y + 4, 1, 4, kKeyHole);   // shaft
    fb.fillRect(x + 4, y + 6, 2, 1, kKeyHole);   // bit
}

} // namespace

// The bar is 320 pixels and the font is 8 wide, so a five-letter label costs
// 40 of them. Laying seven fields out means budgeting the width explicitly
// rather than eyeballing offsets — the first attempt at this ran FLOOR into
// SCORE and pushed the weapon off the right-hand edge.
//
// Two decisions come out of that budget. The keys get icons and no label,
// because a gold key and a silver key need no caption and the two columns
// that caption would cost do not exist. And the weapon's name is used as the
// AMMO slot's label instead of taking a slot of its own: the count belongs
// to the weapon, so naming it there is free.
constexpr int kFloorX  = 4;
constexpr int kScoreX  = 48;
constexpr int kLivesX  = 132;
constexpr int kFaceX   = 176;
constexpr int kHealthX = 208;
constexpr int kKeysX   = 258;
constexpr int kAmmoX   = 272;

// Five digits is the whole field. A larger score is clamped for display
// rather than allowed to run into LIVES.
constexpr int kMaxShownScore = 99999;

void renderStatusBar(Framebuffer& fb, const FaceSprites& faces,
                     const HudState& hud) {
    const int top = kViewH;

    fb.fillRect(0, top, kScreenW, kStatusBarH, kBarFace);
    fb.fillRect(0, top, kScreenW, 1, kBarEdge);
    fb.fillRect(0, top + 1, kScreenW, 1, kBarLight);

    const int labelY = top + 5;
    const int valueY = labelY + 11;
    char buf[32];

    std::snprintf(buf, sizeof(buf), "%d", hud.floor);
    drawSlot(fb, kFloorX, labelY, "FLOOR", buf, kValue);

    std::snprintf(buf, sizeof(buf), "%d", std::min(hud.score, kMaxShownScore));
    drawSlot(fb, kScoreX, labelY, "SCORE", buf, kValue);

    std::snprintf(buf, sizeof(buf), "%d", hud.lives);
    drawSlot(fb, kLivesX, labelY, "LIVES", buf, kValue);

    // Face, sunk into a recess so it reads as a window rather than a sticker.
    const int faceY = top + 4;
    fb.fillRect(kFaceX - 2, faceY - 2, FaceSprites::kFaceW + 4,
                FaceSprites::kFaceH + 3, kBarEdge);
    blitFace(fb, faces.pick(hud.health, hud.faceLook, hud.grimace, hud.gloat),
             kFaceX, faceY);

    // No per-cent sign: the label already says HEALTH, and those two columns
    // are needed by the field next door.
    std::snprintf(buf, sizeof(buf), "%d", std::max(hud.health, 0));
    // Health goes red before it runs out, which is the one number on the bar
    // worth reading with your eyes rather than your attention.
    drawSlot(fb, kHealthX, labelY, "HEALTH", buf,
             hud.health <= 25 ? kValueLow : kValue);

    // Keys stacked vertically in the gutter, gold above silver.
    drawKeyIcon(fb, kKeysX, labelY + 1, kGoldKey, hud.goldKey);
    drawKeyIcon(fb, kKeysX, labelY + 12, kSilverKey, hud.silverKey);

    const int wi = std::clamp(static_cast<int>(hud.weapon), 0,
                              static_cast<int>(WeaponType::Count) - 1);
    drawText(fb, kAmmoX, labelY, kWeaponLabel[wi], kLabel, 1);

    // The knife has no ammo to report, and a zero next to it would read as
    // being out of something.
    if (hud.weapon == WeaponType::Knife) {
        drawText(fb, kAmmoX, valueY + 4, "-", kValue, 2);
    } else {
        std::snprintf(buf, sizeof(buf), "%d", hud.ammo);
        drawTextShadowed(fb, kAmmoX, valueY, buf,
                         hud.ammo <= 5 ? kValueLow : kValue, kShadow, 2);
    }
}

// --- full-screen states --------------------------------------------------

namespace {

void centred(Framebuffer& fb, int y, const char* text, uint32_t color,
             int scale) {
    drawText(fb, (kScreenW - textWidth(text, scale)) / 2, y, text, color,
             scale);
}

// Blink at roughly half a second on, half off. Prompts that sit still are
// easy to look straight past on an attract screen.
bool blink(double time) { return std::fmod(time, 1.0) < 0.55; }

} // namespace

void renderTitleScreen(Framebuffer& fb, double time) {
    fb.clear(kTitleBg);

    // A dark panel behind the title with bright rules top and bottom. The
    // first version filled the panel with the same gold the title is drawn
    // in, and the title vanished into it — a backing has to contrast with
    // what it backs, which is obvious right up until you pick two colours
    // from the same four-entry palette.
    fb.fillRect(20, 34, kScreenW - 40, 72, kTitlePanel);

    // The rules pulse rather than the panel resizing. A panel that changes
    // height moves the text inside it, and the title is the one thing on
    // this screen that should sit still.
    const double pulse = 0.62 + 0.38 * std::sin(time * 2.2);
    const uint32_t rule = shade(kTitleFg, pulse);
    fb.fillRect(20, 34, kScreenW - 40, 2, rule);
    fb.fillRect(20, 104, kScreenW - 40, 2, rule);

    centred(fb, 44, "WOLF3D", kTitleFg, 3);
    centred(fb, 78, "ARCADE", kTitleDim, 2);
    centred(fb, 118, "A RAYCASTER WITH NO ASSET FILES", kTitleDim, 1);

    if (blink(time)) centred(fb, 142, "PRESS ENTER", kValue, 1);
    centred(fb, 162, "ESC TO QUIT", kTitleDim, 1);
}

void renderDeathScreen(Framebuffer& fb, double time) {
    // Fades up to red over about a second rather than cutting, so the moment
    // of dying is legible instead of instantaneous.
    washRows(fb, kScreenH, 0x80, 0x00, 0x00, std::clamp(time * 1.2, 0.0, 0.85));

    centred(fb, 78, "YOU DIED", kValue, 3);
    if (time > 1.2 && blink(time)) centred(fb, 118, "PRESS ENTER", kValue, 1);
}

void renderLevelDone(Framebuffer& fb, double time, int floor, int score,
                     int killPct, int treasurePct, int secretPct) {
    fb.clear(kTitleBg);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "FLOOR %d COMPLETE", floor);
    centred(fb, 28, buf, kTitleFg, 2);

    // The tally lands one line at a time. The original did the same, and it
    // turns a wall of numbers into a small event.
    const int lines = std::clamp(static_cast<int>(time / 0.45), 0, 4);
    const int x = 84;

    if (lines >= 1) {
        std::snprintf(buf, sizeof(buf), "KILL     %d%%", killPct);
        drawText(fb, x, 70, buf, kValue, 1);
    }
    if (lines >= 2) {
        std::snprintf(buf, sizeof(buf), "TREASURE %d%%", treasurePct);
        drawText(fb, x, 86, buf, kValue, 1);
    }
    if (lines >= 3) {
        std::snprintf(buf, sizeof(buf), "SECRET   %d%%", secretPct);
        drawText(fb, x, 102, buf, kValue, 1);
    }
    if (lines >= 4) {
        std::snprintf(buf, sizeof(buf), "SCORE    %d", score);
        drawText(fb, x, 122, buf, kTitleFg, 1);
        if (blink(time)) centred(fb, 150, "PRESS ENTER", kValue, 1);
    }
}

} // namespace wolf
