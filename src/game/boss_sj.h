// Group BSJ of the boss round: fights 52, 54 and 55, kinds 59 (D>Lord) and 61
// (Shroom), and the effect tasks of the kind-3 dispatcher's slots 4 and 5
// (tools/boss_rows.py's units K59, B52, F4, K61, B54, B55, F5; FB93, the Head
// Cracker rock, was ours already) - 44 functions of 0x43F7A0..0x441087.
// Round eleven, wave two, through the boss harness (boss_harness.h).
// docs/boss_sj.md.
#pragma once

void BossSj_Inject();

namespace boss_sj {
// BOF3X_SHADOW=boss_sj: the start-up fuzz, boss_sj_fuzz.cpp - one
// boss_harness::Run per fight, kind and effect task (BOF3X_BSJ_RUN=<unit>
// runs one). Clones every original before BossSj_Inject patches it.
void SelfTest();
}  // namespace boss_sj
