#pragma once

// The solo-character mode's combat half (docs/solo-character-round.md
// section 4 "CB", 0a items 1-5, 6a item 2; docs/solo-character-combat.md;
// DIVERGENCE DIV-0228 SoloCharacter). Group CB's module.
//
// When the champion fights (solo::Solo_ChampionFights(), "champion always"
// unless SoloChampionIfPresent), mode 5 fights with the champion alone, the
// way chapter 6's SortParty / Boss25_Exit already do it in Capcom's code:
//
//   - GameMode5_Turn (step 1), after the party has turned and before its
//     PartySet_Select: the second list 0x904065, Field_MemberCount 0x929EC0,
//     the formation byte 0x904060 and the party-set byte 0x90412C are saved,
//     and the party set becomes a row that holds the champion when the
//     current one does not, so the step's select loads that row's battle file.
//   - GameMode5_Load (step 3), before its Field_PartyLoad(1): the second list
//     becomes the champion's id alone, the count 1, the formation 0, and
//     Field_PartyLoad(1) builds the champion from its own record. Not at
//     step 1: step 2's Encounter_PartyToPlaces walks each of the count's
//     walkers to the spot of its id in the second list, and the leader,
//     absent from a list of the champion alone, would be put at the fourth
//     spot (docs/solo-character-combat.md 2). The first list 0x904062 is left
//     alone.
//   - GameMode5_Place (step 6), on entry: each saved byte goes back only
//     where the live byte still holds what was written (an exit hook that
//     restored its own party, Boss25_Exit, wins). After Party_PlaceAtSlots
//     the count is held at 1, and the count and a changed party set go back
//     at step 7 (GameMode5_Leave), the set's field file loaded again, right
//     before Field_PartyLoad(0) rebuilds the walkers.
//   - GameMode_Start: a save never put back (a plain loss restarts the boot
//     task, solo-character-reads.md M3) is dropped.
//
// Nothing is written back to CharacterRecords and no ObjTrio record is
// copied: the battle credits the champion's own record itself (0a item 3,
// verified in docs/solo-character-combat.md 3).
//
// The saved bytes are this module's statics, outside the quicksave span, so
// they are a snapshot participant ("SOLOCB").
//
// With SoloCharacter off nothing here reads or writes game state: SoloBattle_Arm
// asks Solo_ChampionFights() first (which reads nothing then), and the other
// hooks act only on a save this module holds, which only Arm makes.

namespace solo {

// GameMode5_Turn, once the party has turned (Encounter_PartyTurn answered
// non-zero), after the approach and before the step's PartySet_Select.
// `event_flags` is the step's own read of 0x904AE5 (bit 4: the step skips its
// select). Arms the battle for the champion alone, or does nothing.
void SoloBattle_Arm(unsigned char event_flags);

// GameMode5_Load, before Field_PartyLoad(1): writes the champion alone into
// the second list, the count 1 and the formation 0 when Arm armed this battle.
void SoloBattle_WriteParty();

// GameMode5_Place, on entry, before Party_PlaceAtSlots: puts back what Arm
// saved, byte by byte where the live byte still holds what Arm wrote, and
// reloads the field's party-set file when Arm changed the set. Returns true
// when Party_PlaceAtSlots must find the leader at ObjTrio's first record (the
// champion, at the leader's battle spot) because the walker it looks for by
// the first list's id is not among the restored count's records - the caller
// then holds the first list's first byte at the champion's id for that one
// call (SoloBattle_PlaceLeaderId).
bool SoloBattle_Restore();

// GameMode5_Place, right after Party_PlaceAtSlots: the member count is held
// at 1 until step 7 when it stands above 1. ObjTrio's second and third
// records were not built for this battle (the walkers' records from before
// it, flags cleared, sprite pointers into the set file loaded before it);
// Party_PlaceAtSlots places them, and nothing ticks or draws them until
// Field_PartyLoad(0) rebuilds them (docs/solo-character-combat.md 9).
void SoloBattle_AfterPlaceAtSlots();

// GameMode5_Leave, in its branch, before Field_PartyLoad(0): the held count
// back, and the party set Restore put back - its byte, and the field's own
// row's file loaded again when Arm changed the row - so the walkers are
// rebuilt from a file that matches the byte.
void SoloBattle_RestoreAtStep7();

// The id SoloBattle_Restore's "true" asks the caller to hold in the first
// list's first byte for Party_PlaceAtSlots. Only valid right after it.
unsigned char SoloBattle_PlaceLeaderId();

// GameMode_Start, first: a save not put back (a loss) is dropped.
void SoloBattle_Drop();

// From TransitionFromModeRest_Inject: the snapshot participant, and the
// self-test under the exe host's BOF3X_SELFTEST_ONLY or hosted
// BOF3X_SHADOW=solo (or '*').
void SoloBattle_Inject();

// The save / write / restore logic's self-test, on state of its own. A Fatal
// on a case that does not hold.
void SoloBattle_SelfTest();

}  // namespace solo
