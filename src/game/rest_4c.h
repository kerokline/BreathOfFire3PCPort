// Group R4C of round fourteen (wave four): 60 functions at 0x459EE0..0x45C3F3,
// the cut's 60 rows for R4C (analysis/round14_cut.tsv), none added, none
// dropped. docs/rest_4c.md.
//
// What they are, by the code: two of the four entries of R4B's game table
// 0x652A84 (the field hook's state 2, by the byte 0x9039F5) - two games with
// a stake, each a phase / state / step machine on the message-choice bytes
// 0x939A3E..0x939A40 with its own nested .data tables:
//   - CommuHiLo_*: a stake of up to 100 zenny entered as three digits; nine
//     values (a shuffle of 1..9) dealt as cards; up to eight picks, each 0 or
//     1; then each card turned in order and its pick compared with whether the
//     card before it is not below it; the stake times a table's factor by the
//     cards turned;
//   - CommuHitBlow_*: 500 zenny taken; three distinct digits 1..9 drawn; up
//     to eight guesses of three digits, each scored by digits in place and
//     digits elsewhere; three in place gives an item of a table by the guess
//     and a rank;
// and the draw helpers both use (Commu_*): the money and stake boxes, their
// frame of menu pieces, a card, a piece, an underline, a random digit, and
// Commu_PushSubscreen, which R4B's screens call.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest4C_Inject();

namespace rest_4c {
// BOF3X_SHADOW=rest_4c: the start-up fuzz, rest_4c_fuzz.cpp. Clones the 60
// originals before Rest4C_Inject patches them.
void SelfTest();
}  // namespace rest_4c
