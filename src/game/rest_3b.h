// Round fourteen group R3B (docs/takeover-queue-round14.md, wave three): the
// battle engine's band 0x4468B0..0x44CFF4 - the result screen's EXP helpers,
// three percent clamps the stat rebuilds use, the command menus' remaining
// steps (the party side of the target pick, four cancels, two closing waits,
// the ability window's side pick and four dispatchers), and 45 slots of
// Effect_Handlers (0..49 less 8, 22, 23, 31, 38, which are earlier groups').
// 64 functions: the cut's 60 and four slots no list had (4, 7, 11, 47).
// Their declarations are symbols.gen.h's (each has a signature and `impl` in
// symbols.toml). docs/rest_3b.md.
#pragma once

void Rest3B_Inject();

namespace rest_3b {
// BOF3X_SHADOW=rest_3b: the start-up fuzz, rest_3b_fuzz.cpp - one
// boss_harness::Run over the group, Group::engine set. Clones every original
// before Rest3B_Inject patches it.
void SelfTest();
}  // namespace rest_3b
