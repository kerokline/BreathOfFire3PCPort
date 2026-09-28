// Round eleven group BSD: the boss band's fights 17..21 and kinds 18, 21..27
// (0x43A590..0x43B74A) - Mutant and its set-up; the area-79 fight's Claw,
// Cawer, Patrio and Dodai with set-ups 18, 19, 20; Emitai and Golem with
// set-up 21; Garr (the tool's names). Through the boss harness
// (boss_harness.h). docs/boss_sd.md.
#pragma once

void BossSd_Inject();

namespace boss_sd {
// BOF3X_SHADOW=boss_sd: the start-up fuzz, boss_sd_fuzz.cpp - one
// boss_harness::Run per fight id or kind (BOF3X_BSD_RUN=<unit> runs one).
// Clones every original before BossSd_Inject patches it.
void SelfTest();
}  // namespace boss_sd
