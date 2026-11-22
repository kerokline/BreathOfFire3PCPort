#pragma once

// The solo-character mode's two screens of one member (DIVERGENCE DIV-0228 (amended 2026-11-22);
// docs/solo-character-shops.md): the merchant's buy screen and the masters'
// member screens show the champion alone, in place of the walkers.
//
// Both screens hold their members as a count and a slot's character record.
// The merchant takes them from the first party list (Party_Count(0), and
// Character_IndexById[0x904062[slot]]); the masters from the walkers
// (Field_MemberCount, and ObjTrio[slot] +0x148). Each of those reads asks
// here first. While the champion fights alone (solo::Solo_ChampionFights():
// the mode active, and the champion joined or SoloChampionIfPresent off) the
// count is 1 and slot 0 is the champion's record, joined or not, a walker or
// not. Otherwise the answer is the game's own, passed in.
//
// Nothing is written: the party lists, the walkers and Field_MemberCount stay
// the game's, so every other reader (the field, the scripts' party tests, the
// field menu) sees the team as before. With SoloCharacter off these read
// nothing and answer the game's value.

namespace solo {

// Whether the two screens show the champion alone now.
bool SoloScreens_ChampionOnly();

// The screen's member count: 1 while the champion is shown alone, else
// `game_count` (the count the original reads).
unsigned SoloScreens_Count(unsigned game_count);

// The character record of the screen's slot `slot`: the champion's record
// while the champion is shown alone, else `game_record` (the record the
// original reads for the slot). While the champion is shown alone only slot 0
// exists; another slot is a Fatal naming `who`.
unsigned SoloScreens_Record(unsigned slot, unsigned game_record, const char* who);

// The pure half's self-test (no game state); the number of checks. Called
// from Solo_SelfTest.
int Solo_ScreensSelfTest();

}  // namespace solo
