// The band census (BOF3X_BANDLOG=<file>; docs/band-census.md): an instrument,
// not a divergence. While the view is wide (DIV-0041), every sprite that
// Sprite_UpdateScreen 0x588F20 (ours, sprite_screen.cpp) puts on the draw
// list and whose screen rectangle lies inside the wide picture but wholly
// outside the original's [0, 320) is written to the file as run-length
// merged rows: area, chapter, object, frames, x / y range, side, band
// (16:9 or 21:9 only), parked, scene. It reads game memory and writes only
// its own file: nothing the game keeps changes, so it is never switched off
// by BOF3X_ORIGINAL and needs no ledger entry.
#pragma once

#include <cstdint>

namespace band_census {

extern bool g_on;   // false until Arm() opened the file

// The hook, Sprite_UpdateScreen's one line: the sprite it has just appended
// to Sprite_DrawList. Off, a load and a branch.
void Record(const unsigned char* sprite);
inline void Seen(const unsigned char* sprite) {
    if (g_on) Record(sprite);
}

// Where a screen rectangle [left, right) x [top, bottom) in the game's
// 320-wide units falls, for a wide picture of `columns` a side.
enum Class : int {
    kNotShown = 0,   // outside the wide picture (or not a rectangle)
    kInside,         // touches [0, 320): the original showed it, or part of it
    kLeft169,        // wholly left of 0, and touching the 16:9 band -53..0
    kRight169,       // wholly right of 320, touching 320..373
    kLeft219,        // wholly left of -53: seen only in a picture wider than 16:9
    kRight219,       // wholly right of 373
};
Class Classify(float left, float right, float top, float bottom, unsigned columns);

// The rectangle the draw pass draws a sprite record at, in the game's 320 x
// 240 units (the census's own reading, section 1 of docs/band-census.md);
// false when Sprite_Draw draws nothing of it. Also read by DIV F's hide
// (wide_staging.cpp), so the census's "hidden" column and the hide agree.
bool ScreenRect(const unsigned char* sprite, float& left, float& right, float& top, float& bottom);

// The scene mask, the census's `mask` column: bits 1..8 are the four tests
// the game makes before the leader may move (a scene holds the leader), 0x10
// the leader's state not 1 (docs/band-census.md section 3).
unsigned SceneMask();
constexpr unsigned kSceneMaskBits = 0xF;

// After every module's self-test (InjectAll): the self-test under
// BOF3X_SHADOW=band_census, then BOF3X_BANDLOG opened, if set.
void Inject();

}  // namespace band_census
