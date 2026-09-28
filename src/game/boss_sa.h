// Round eleven group BSA: the boss band's first units - fight 1's set-up and
// its two kinds (Gary, Mogu), the three set-ups of BOSS002 (fights 2, 3, 39)
// and the kinds before them (Nue twice, Sample 1), and kind 39 (Weretigr),
// whose hook table shares a body with Nue's. 49 functions of
// 0x437A10..0x43D662, through the boss harness (boss_harness.h). The names
// are the disc's (tools/boss_rows.py --disc). docs/boss_sa.md.
#pragma once

void BossSa_Inject();

namespace boss_sa {
// BOF3X_SHADOW=boss_sa: the start-up fuzz, boss_sa_fuzz.cpp - one
// boss_harness::Run per kind (6, 7, 1, 2, 46, 39) and per fight (1, 2, 3,
// 39). Clones every original before BossSa_Inject patches it.
void SelfTest();
}  // namespace boss_sa
