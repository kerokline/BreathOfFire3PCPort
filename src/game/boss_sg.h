// Group BSG of the boss round: fights 29, 31, 32 and 33, kinds 34..38 and 40,
// and the kind-3 effect task F6 (tools/boss_rows.py's units K34, B29, K35,
// K36, K37, B31, K38, B32, B33, F6, K40 - chapters 7 and 8), 53 functions of
// 0x43CDE0..0x43E535. Round eleven, wave two, through the boss harness
// (boss_harness.h). docs/boss_sg.md.
#pragma once

void BossSg_Inject();

namespace boss_sg {
// BOF3X_SHADOW=boss_sg: the start-up fuzz, boss_sg_fuzz.cpp - one
// boss_harness::Run per fight, kind and effect task (BOF3X_BSG_RUN=<unit> runs
// one). Clones every original before BossSg_Inject patches it.
void SelfTest();
}  // namespace boss_sg
