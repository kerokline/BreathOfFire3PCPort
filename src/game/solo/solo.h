#pragma once

// The solo-character mode's state and decisions (docs/solo-character-round.md;
// DIVERGENCE DIV-0228 SoloCharacter, DIV-0229 SoloChampionIfPresent). Group
// SC's module: the switch, the save's mark and the questions the combat,
// field, screen and level-1 halves ask. It changes no gameplay by itself:
// its one write is the mark (Solo_Stamp), and only with SoloCharacter on.
//
// The champion is a character record, not an id (solo-character-reads.md M6):
// one of records 0, 1, 2, 4, 5, 6. Records 3 and 7 are never champions (the
// owner, round doc 6a item 7: the two temporary members).
//
// The launcher's choice is BOF3X_SOLO, one of ryu, nina, garr, rei, momo,
// peco (lower case; the contract with the launcher, group LA), records 0, 1,
// 2, 4, 5, 6 in that order. Read once at start by Solo_Read.
//
// The run's mark is one byte of ours in the save block, Solo_ChampionMark
// (0x904A21, block +0x1041; symbols.toml has the evidence): 0 = no solo run,
// else the champion's record + 1. With SoloCharacter off nothing here reads or
// writes it. With it on, a value that is not 0 or a champion's record + 1 is a
// Fatal naming it, wherever it is read.
//
// Every question but Solo_MustStay answers "no" with SoloCharacter off, and
// does not read the mark then. Solo_ChampionRecord, Solo_ChampionId and
// Solo_ChampionJoined are a Fatal when Solo_Active() is false: ask that first.

namespace solo {

// Reads BOF3X_SOLO. Called once, from InjectAll right after Behaviour_Read
// (the hook-order trap of behaviour.h: BehaviourOn before Behaviour_Read is a
// Fatal). A value that names no champion is a Fatal naming it, with the
// behaviour on or off; unset or empty is no launcher champion (logged with the
// behaviour on). Runs the self-test under the exe host's BOF3X_SELFTEST_ONLY
// or hosted BOF3X_SHADOW=solo. A second call is a Fatal.
void Solo_Read();

// SoloCharacter on, the live chapter byte Cond_ByteFA (0x8034E0) neither 0
// (the intro, the owner's 6a item 3) nor 16 (the attract demo's own chapter,
// which Area_Enter skips in play), and a champion in the save's mark.
bool Solo_Active();

// The champion's record: 0, 1, 2, 4, 5 or 6. Needs Solo_Active().
int Solo_ChampionRecord();

// The character id to put in a party list for the champion now: the record's
// own id byte, CharacterRecords[R] +9, read live (records 0 and 1 are reached
// by several ids, M6). Needs Solo_Active().
unsigned Solo_ChampionId();

// CharacterRecords[R] +0xB bit 0, "joined", for the champion's record R
// (M6: the story clears it when it takes a character away). Needs
// Solo_Active().
bool Solo_ChampionJoined();

// Whether the champion fights this battle alone: Solo_Active(), and the
// champion is joined or SoloChampionIfPresent is off ("champion always", the
// mode's rule, 6a item 2). Before the story has introduced the champion,
// "champion always" fights from chapter 1 on: kChampionFightsBeforeFirstJoin
// in solo.cpp, the owner's open call.
bool Solo_ChampionFights();

// CharacterRecords[record] +0xB bit 1, the game's "must stay" mark, as the
// game has it (record 0's permanent mark counts: 6a item 1). Any of the eight
// records; another index is a Fatal. Asks nothing of the mode.
bool Solo_MustStay(int record);

// The mark's one writer. With SoloCharacter on, the chapter not 0 or 16, the
// mark 0 and BOF3X_SOLO naming a champion: the mark becomes that record + 1,
// once. A mark already set holds (a line is logged, once per mark, when
// BOF3X_SOLO names another). Called from Area_Enter (src/game/menu/
// item_trade.cpp) after its chapter step: every game, new or loaded, enters
// an area before it can fight, and the chapter is final for the entry there.
void Solo_Stamp();

// The decision table's self-test, on inputs of its own (no game state). A
// Fatal on a case that does not hold.
void Solo_SelfTest();

}  // namespace solo
