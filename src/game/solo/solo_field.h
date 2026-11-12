#pragma once

// The solo-character mode's field half (docs/solo-character-round.md section
// 4 "FS" and 6a item 1; docs/solo-character-field.md; DIVERGENCE DIV-0228
// SoloCharacter). Group FS's module. While solo::Solo_Active():
//
//   - the map team is every member the game marks "must stay"
//     (CharacterRecords +0xB bit 1; record 0, the hero, carries it from New
//     Game) and at most one member who is not marked. The others who have
//     joined (+0xB bit 0) are in reserve, as the game's own members screens
//     already count them (TacticsMembers_Build, PartyForm_Setup).
//   - the fold puts the map team back to that rule at the frame control is
//     back with the player (Field_LeaderStand after its first three
//     returns), when the party is not the rule's: the members over the rule
//     leave by the game's own Party_Remove (the joined bit kept) and
//     Party_ReloadPalettes, as Capcom's scenes take a member out
//     (Scena02_Leave43). It never adds a member.
//   - the L swap rotates the lead as the original does; when the member who
//     leads is not marked and someone joined is in reserve, that member is
//     replaced at the press, with Field_SwapExchange's reload steps, by the
//     next joined character in record order (the "bring out"), the party set
//     loaded first when it does not hold them; the original swap then runs.
//   - a join with three on the map does not open the choose-who-leaves
//     screen (Field_Request 6): the member has joined (the game set the bit)
//     and is in reserve.
//
// With SoloCharacter off, or the mode standing down (Solo_Active false), each
// entry point reads the behaviour (and, on, the mode's answer) and changes
// nothing: the field is the original's.
//
// What it keeps between frames is a cache (the last party signature seen and
// whether the mode was active): the fold is idempotent, so a stale cache - a
// quick-load, a game loaded from the title - costs one evaluation that finds
// nothing to do. No snapshot participant is needed (solo-character-field.md
// 5).

namespace solo {

// Field_LeaderStand (0x52DA70) after its first three returns: the fold.
void SoloField_Stand();

// Field_LeaderSwapTest (0x5302C0) once the swap is allowed, before the
// leader's state 3: with a bring-out due, the party set made to hold the map
// team after it (and the champion where a row allows) and loaded as
// PartySet_Load loads it; then the leader who is leaving the lead made the
// next character in place, with Field_SwapExchange's reload steps, and every
// walker's sprite set again from the new set. The swap that follows is the
// original's.
void SoloField_SwapStart();

// Party_Join (0x533EF0) with three on the map: true when the mode skips
// Field_Request 6 (the choose-who-leaves screen); the caller then answers 1,
// the original's answer for a join it did not place.
bool SoloField_JoinSkipsScreen();

// The decisions' self-test, on state of its own; a Fatal on a case that does
// not hold. Run from Field4_Inject under the exe host's BOF3X_SELFTEST_ONLY
// or hosted BOF3X_SHADOW=solo_field.
void SoloField_SelfTest();
void SoloField_Inject();

}  // namespace solo
