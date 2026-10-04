// The party sets' field actions' seven shared helpers, originals 0x51C390..
// 0x524E4E (round fourteen's stage-A group R0A): the probes ahead of the sprite
// being run, the kind-0x30 effect object lined up ahead of it and the party
// members in reach of that object, an effect object of kind 0x34 placed on a
// cell, and the two side probes for a steep slope. Wave one's field-action
// groups (R1A..R1F, 133 call sites) call them, so they are taken first and
// called by name. docs/rest_0a.md.
//
// All seven are cdecl. Four take nothing and work on Sprite_Current (the
// sprite whose state handler is running: its direction byte +8, its 16.16
// position +0x34 / +0x38, its height word +0x3E, its byte +0x2B); two take an
// Effect_Objects index (the dword's low byte, 0..19 - ours aborts on any other,
// which no caller passes: docs/rest_0a.md section 5); one takes a state byte
// and a cell. Each "a step" is one row of Field_DirectionSteps (half a cell per
// axis), read in place with the direction byte unmasked, as the originals read
// it. Where a function answers, it answers in al (every caller tests al only:
// docs/rest_0a.md section 2); the rest of eax is undefined, as the originals
// leave it.
#pragma once

extern "C" {

// original 0x51C390: something to act on two steps ahead of Sprite_Current (a
// cell ahead): 1 when Sprite_ObjectAt(that point, margin 0) finds an object or
// AreaMap_ByteAt of its cell is 0xF2 - or of the cell one on in x when the
// point's x has a fraction, or one on in z when its z has one; else 0. Takes
// nothing; the point is computed once, before any call. 36 call sites.
unsigned char __cdecl PartyAction_TargetAhead(void);

// original 0x51C6A0: 1 when a party member is in reach (Sprite_PointInReach,
// margin 1) of the point two steps beyond effect object `index`, in
// Sprite_Current's direction, at the object's height word (+0x3E, re-read
// for each member); else 0. The members are ObjTrio's records 1 ..
// Field_MemberCount - 1 (the count re-read for each; the leader, record 0, is
// not tested). `index` is the dword's low byte, 0..19. 8 call sites, each
// handing it PartyAction_Kind30Ahead's answer.
unsigned char __cdecl PartyAction_MemberBeyondEffect(unsigned index);

// original 0x51DD70: as PartyAction_MemberBeyondEffect, at effect object
// `index`'s own position and its height word plus 0x200 (16 bits), all three
// re-read for each member. With Field_MemberCount 1 or less nothing is read
// and the answer is 0. 8 call sites.
unsigned char __cdecl PartyAction_MemberOnEffect(unsigned index);

// original 0x521510: the effect object of kind 0x30 lined up one step ahead
// of Sprite_Current - 0xFF unless Field_State +0x89 is 2 and bit 0 of its
// +0x138 is clear; else the index of the first of Effect_Objects' 20 records in
// use (+0), of kind 0x30 (+5), that the point one step ahead is in reach of
// (Sprite_PointInReach, Sprite_Current's height word, margin 1) and whose x
// (+0x34) or z (+0x38) equals Sprite_Current's (re-read after the call); 0xFF
// when none is. 8 call sites (each tests al against 0xFF).
unsigned char __cdecl PartyAction_Kind30Ahead(void);

// original 0x522560: the way two steps ahead of Sprite_Current blocked: 1 when
// Sprite_ObjectAt(that point, margin 0) finds an object, Field_EffectAhead
// finds an effect object of kind 0x17, or AreaMap_ByteAt is 0xF0, 0xF1, 0xF4,
// 0xF6 or 0xF7 at its cell (and the cells one on in x / z across a fraction,
// as PartyAction_TargetAhead); else 0. Every call is made whatever the earlier
// ones answered. 18 call sites.
unsigned char __cdecl PartyAction_BlockedAhead(void);

// original 0x522FB0: an effect object of kind 0x34 on the cell (x, z), as
// Effect_SpawnAtCell 0x524870 places one but 0x200 above the ground and +0xB
// 1: with Effect_FindFree's slot not 0xFF, +0 = 1, +5 = 0x34, +1 = `state`'s
// low byte, +0x34 / +0x38 = the words x and z sign-extended as 16.16, +0x3E =
// AreaMap_Elevation there + 0x200, +0xB = 1. Nothing when no slot is free.
// The callers push a fourth dword, never read. No caller reads eax (45 sites).
void __cdecl Effect_SpawnAtCellHigh(unsigned state, unsigned x, unsigned z);

// original 0x524DA0: Sprite_Current's +0x2B = 1, then the side probes in
// directions 3 and 5: the point one step that way; when MapView_SlopeAt there
// is steep (DamageScratch's flag byte set and the slope's low word, signed,
// above 0x40) and MapView_GroundAt there is above the sprite's height word
// (both signed 16 bits), +0x2B = 0. Sprite_Current, its position and the two
// steps are re-read for the second probe. No caller reads eax (14 sites).
void __cdecl PartyAction_SideProbes(void);

}  // extern "C"

void Rest0A_Inject();

namespace rest_0a {
// BOF3X_SHADOW=rest_0a: the start-up fuzz, rest_0a_fuzz.cpp. Clones the seven
// originals before Rest0A_Inject patches them.
void SelfTest();
}  // namespace rest_0a
