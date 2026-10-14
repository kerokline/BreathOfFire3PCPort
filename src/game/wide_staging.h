// The wide staging (docs/wide-staging.md; DIV-0122, DIV-0123 and DIV-0124 of the
// ultrawide round): what the wider picture shows of a scene's staging that the
// original's 320 columns hid, re-staged in the wide modes only. Narrow
// (BOF3X_WIDE unset or 0) nothing here is armed and every hook is a load of
// one bool and a branch.
//
//   D  The 16:9 re-staging (BOF3X_WIDE_STAGING, on by default in the wide
//      modes): an actor a scene starts, parks or ends in the band beside the
//      original view starts (or walks on to) a point past the live picture's
//      edge, and its walk grows by the same steps, so the walk ends where the
//      original's did - the PSP's "out" shape (docs/ultrawide-staging-psp.md),
//      as a rule in the columns per site (kSites, wide_staging.cpp).
//   E  Case zero (area 175, chapter 7, the descriptor's +0x18[4] and [5]):
//      the PSP's staging, the pair waiting inside the 4:3 view - written as a
//      change of the original's steps (kCaseZero), cited to PD.
//   F  The 21:9 hide (BOF3X_WIDE_HIDE, on by default at BOF3X_WIDE=2): a field
//      object's sprite wholly beyond the 16:9 view is not drawn while a scene
//      holds the leader. Draw-only: Sprite_Draw returns before its primitive,
//      after its cull and its bit 7 (UC2's), and nothing the game keeps changes.
#pragma once

namespace wide_staging {

extern bool g_staging;   // D and E armed: BOF3X_WIDE 1 or 2, BOF3X_WIDE_STAGING not 0, after every self-test
extern bool g_hide;      // F armed: BOF3X_WIDE=2, BOF3X_WIDE_HIDE not 0, after every self-test

// The hooks, one line each in the interpreters.
//
// MoveScript_Step (move_script.cpp), after the flow pass, before its dispatch:
// a site's move op (or the op a walk is inserted before) is run here as its
// rewritten walk. True: the step is done and `result` is what the step
// returns; false: the step goes on as the original's.
bool StepSite(unsigned char* context, const unsigned char* script, unsigned char& result);
inline bool Step(unsigned char* context, const unsigned char* script, unsigned char& result) {
    return g_staging && StepSite(context, script, result);
}
// The flow pass's op 0D (move_script.cpp), after it placed Sprite_Current and
// before it takes the ground's height: a site's start moved out.
void Placed0DSite(unsigned char* context, const unsigned char* script, unsigned at);
inline void Placed0D(unsigned char* context, const unsigned char* script, unsigned at) {
    if (g_staging) Placed0DSite(context, script, at);
}
// EventScript_Op (event_script.cpp), after the op's handler: a site's
// placement op, its object's start moved out.
void PlacedSite(const unsigned char* op);
inline void Placed(const unsigned char* op) {
    if (g_staging) PlacedSite(op);
}

// Sprite_Draw (sprite_draw.cpp), after the cull and bit 7, before the
// primitive: true when F hides this sprite this frame.
bool HideSite(const unsigned char* sprite);
inline bool Hide(const unsigned char* sprite) { return g_hide && HideSite(sprite); }

// F's decision, apart from the sprite's reading: the rectangle's x extent in
// the game's 320 units, the scene mask (band_census::SceneMask), whether the
// record is a field object or a party member other than the leader, and
// whether an exception names it. Exposed for the census's column and the fuzz.
bool HideDecision(float left, float right, unsigned scene_mask, bool field_scene, bool candidate, bool excepted);

// A field scene, as the game tells it from the world map and the attract
// demo: Game_Mode 2 (the field), not chapter 16 (the demo), and an area
// WorldMap_RecordIndex finds no world-map record for (11). EB-1's census
// found the scene mask set on the world map (0C) and in the demo (02).
bool FieldScene(unsigned game_mode, int chapter, unsigned world_map);

// The census's "hidden" column: F would hide this sprite now (false while F
// is not armed). Reads the same rectangle Sprite_Draw's call does.
bool WouldHide(const unsigned char* sprite);

// After every module's self-test (InjectAll): the self-test (BOF3X_SHADOW=
// wide_staging, and '*'), then D, E and F armed when the view is wide.
void Inject();

}  // namespace wide_staging
