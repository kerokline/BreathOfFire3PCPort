#pragma once

// The solo-character mode's scene guards (docs/solo-character-scenes.md;
// DIVERGENCE DIV-0228 SoloCharacter). Group SA's module.
//
// The field half's fold (solo_field.h) leaves the map team at the hero and
// at most one other, so a scene written for a party of three can find a
// walker slot empty. Capcom's party helpers read the three slots and the
// three list bytes whatever the count, and the engine's Party_Remove takes
// the last walker off for an id that is not walking at all; with three
// walkers - the only party those helpers met - neither ever happened. These
// guards give each such read the answer that keeps the helper's own intent
// for the slots the fold emptied, and nothing else:
//
//   - a walker slot at or past the count holds no member (Scene_MemberId:
//     0xFF, the party lists' own "none"; Scene_SlotPose: 0, a walker that
//     has finished walking in);
//   - a party save leaves 0xFF in the slots past the count, not the stale
//     bytes of an earlier save (Scene_PadSaved), and a party restore treats a
//     saved 0xFF as no member (Scene_SavedNone);
//   - Party_Remove of an id in neither party list below the count removes
//     nothing (Scene_RemoveNotWalking), where the original would take the
//     last walker off;
//   - Party_Join of a byte that is not a character id (0xFF from a slot the
//     fold emptied) joins nothing (Scene_JoinNoId), where the original would
//     index Character_IndexById past its 24 entries.
//
// Every guard asks the mode first: with SoloCharacter off, or the mode
// standing down (solo::Solo_Active() false), each returns the raw value or
// "no", and the caller runs the original's path unchanged. With the mode on
// and three walkers, every answer is the original's as well: the guards only
// differ for a slot past the count.

namespace solo {

// SoloCharacter on and the mode active (solo.h Solo_Active).
bool Scene_Active();

// The id a party helper reads for walker `slot` (0..2; ObjTrio +0x89, read by
// the caller as `raw`): 0xFF when the mode is active and `slot` is at or past
// Field_MemberCount (the record there is off, its +0x89 a stale byte); else
// `raw`.
unsigned char Scene_MemberId(unsigned slot, unsigned char raw);

// Walker `slot`'s walk-in byte ObjTrio +0x137, read by the caller as `raw`:
// 0 when the mode is active and `slot` is at or past Field_MemberCount (no
// walker there to wait for); else `raw`.
unsigned char Scene_SlotPose(unsigned slot, unsigned char raw);

// A party save of `n` ids into `saved` (three bytes): with the mode active,
// the bytes from `n` to 2 become 0xFF. Off: nothing (the original leaves
// them as an earlier save wrote them).
void Scene_PadSaved(unsigned char* saved, unsigned n);

// Whether a saved party byte means "no member" to a restore: the mode active
// and the byte 0xFF.
bool Scene_SavedNone(unsigned char id);

// Party_Remove (0x534030): true when the mode is active and `id` is in
// neither party list (0x904062, 0x904065) below Field_MemberCount - the
// caller then removes nothing.
bool Scene_RemoveNotWalking(unsigned char id);

// Party_Join (0x533EF0): true when the mode is active and `id` is not a
// character id (24 or more; Character_IndexById has 24 entries) - the caller
// then joins nothing.
bool Scene_JoinNoId(unsigned char id);

// The decisions' self-test, on state of its own; a Fatal on a case that does
// not hold. Run under the exe host's BOF3X_SELFTEST_ONLY or hosted
// BOF3X_SHADOW=solo_scenes (SoloScenes_Inject).
void SoloScenes_SelfTest();
void SoloScenes_Inject();

}  // namespace solo
