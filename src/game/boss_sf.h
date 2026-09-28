// Group BSF of the boss round: fights 27 and 28, kinds 33 and 62 (Gazer,
// Myria) and two effect tasks - BattleFx_Dispatch's slot 8 (fight 26's count)
// and BattleBossFx_Dispatch's slot 2 (the Gazer's) - tools/boss_rows.py's
// units B27, FB8, K33, K62, B28, F2: 54 functions of 0x43C480..0x4406D7.
// Round eleven, wave two, through the boss harness (boss_harness.h).
// docs/boss_sf.md.
#pragma once

void BossSf_Inject();

namespace boss_sf {
// BOF3X_SHADOW=boss_sf: the start-up fuzz, boss_sf_fuzz.cpp - one
// boss_harness::Run per unit (BOF3X_BSF_RUN=<unit> runs one). Clones every
// original before BossSf_Inject patches it.
void SelfTest();
}  // namespace boss_sf
