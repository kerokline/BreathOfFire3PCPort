// Group BSH of the boss round: fights 34, 35, 36, 41, 43 and 47 and kinds 41,
// 42, 43, 44, 48, 50 and 54 plus the kind-3 dispatcher's slot 3
// (tools/boss_rows.py's units K48, B34, B41, K41, K42, K54, B35, B47, K43, K50,
// B36, B43, F3, K44), 46 functions of 0x43DEF0..0x43ECC0. Round eleven, wave
// two, through the boss harness (boss_harness.h). docs/boss_sh.md.
#pragma once

void BossSh_Inject();

namespace boss_sh {
// BOF3X_SHADOW=boss_sh: the start-up fuzz, boss_sh_fuzz.cpp - one
// boss_harness::Run per fight and per kind (BOF3X_BSH_RUN=<unit> runs one).
// Clones every original before BossSh_Inject patches it.
void SelfTest();
}  // namespace boss_sh
