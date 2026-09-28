// Group BSE of the boss round: fights 22..26, 30 and 48 and kinds 28..32 and
// 55 (tools/boss_rows.py's units B22, K28, B23, B30, K29, K55, B24, B48, K30,
// K31, K32, B25, B26 - chapters 5, 6, 7), 52 functions of 0x43B5B0..0x43E7A0.
// Round eleven, wave one, stage B, through the boss harness (boss_harness.h).
// docs/boss_se.md.
#pragma once

void BossSe_Inject();

namespace boss_se {
// BOF3X_SHADOW=boss_se: the start-up fuzz, boss_se_fuzz.cpp - one
// boss_harness::Run per fight and per kind (BOF3X_BSE_RUN=<unit> runs one).
// Clones every original before BossSe_Inject patches it.
void SelfTest();
}  // namespace boss_se
