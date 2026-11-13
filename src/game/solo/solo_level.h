#pragma once

// The solo-character mode's level-1 start (docs/solo-character-round.md 6b
// and 6c; docs/solo-character-level1.md, M10 and its "as run" section;
// DIVERGENCE DIV-0228 SoloCharacter). Group LV's module.
//
// At New Game only (the owner, 6c item 1), right after NewGame_InitCharacters
// 0x437820 has copied the default records, the champion's record is taken
// from its starting level N down to level 1 by subtracting what the game's
// own level-up adds, read at run time from the image's tables:
//
//   Char_DefaultRecords 0x64B390 + 0xA4 r   the default record, as laid
//   Char_ExpTable       0x658F48 + 8 (99 r + L)   row L, the step L -> L + 1
//                       (char.cpp Char_LevelUp: +2, +3 HP and AP, +4 / +5
//                       nibbles power, defence, agility, intelligence, +6 /
//                       +7 two ability ids)
//
// The record keeps no history, so this is defined only from the default
// record; at New Game the record is the default by construction, and a record
// that is not is a Fatal (6b's guard is a check, not a branch: 6c item 1).
// Willpower (+0x4E, +0x1C, +0x2E) is not touched (6c item 2). No table of
// ours: nothing here holds a stat value.

namespace solo {

// Takes CharacterRecords[record] from the default record's level N to level
// 1: level 1, EXP 0; each base word +0x40..+0x4A the default's less the
// gains of rows 1..N-1; one copy of each non-zero ability id of those rows
// taken out of its list (AbilityList_ForType 0x591EC0), the rest moved up;
// then Char_RecalcStats 0x590660 and HP / AP (+0x18 / +0x1A) to the new
// maxima (+0x20 / +0x22). A record whose default level is 1 is left as it is.
//
// A Fatal, naming the record, when: the record is not 0..6 (the seven default
// records); the live record differs from the default anywhere but the three
// willpower bytes; the default has a master (+0x1F not 0xFF), a master level
// (+0x88) or a growth bias (+0x89..+0x8E), or a level outside 1..99; a
// default base word is 999 or above (Char_LevelUp's cap may have acted, so
// the sum cannot be undone); a derived word is negative; a row's ability is
// not in its list. `log` writes the before and after lines.
void Solo_LevelReset(int record, bool log);

// Solo_LevelReset's checks of the tables alone, at launch: the default
// record's level, master, growth bias and base words, the words it derives,
// and every row ability found in its list - on a copy of the default record,
// so nothing of the game's is written. The same Fatals as Solo_LevelReset,
// each sentence saying it was found at launch. Called from Solo_Read when
// SoloCharacter is on and BOF3X_SOLO names a champion: the tables are final
// there - the exe host lays the image and every BOF3X_OPT layer's table cells
// (ExeImage_Source, ExeImage_LayOptTables, a table-edit or disc-patch mod's
// char_defaults / exp_table edits among them) in CopyData before InjectAll,
// whose second call is Solo_Read; the hosted build keeps Capcom's .data and
// refuses table layers; nothing of ours writes either table after that (the
// language layer's kind 10 writes only the default names, which nothing here
// reads). Solo_LevelReset keeps its own checks as the backstop at New Game.
void Solo_LevelCheckTables(int record);

// The round trip, on the default records copied into the record slots and
// put back after (the slots' bytes are restored whole, so no game state
// changes): for each champion record, the reset, then the game's own
// Char_LevelUp 0x498DE0 from level 1 with the default's EXP, and the result
// must equal the default record put through Char_RecalcStats byte for byte,
// but for current HP and AP (+0x18..+0x1B: a level-up does not refill them).
// Returns the checks made; a Fatal on one that does not hold. Run from
// Solo_SelfTest.
int Solo_LevelSelfTest();

}  // namespace solo
