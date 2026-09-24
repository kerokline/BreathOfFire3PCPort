// Battle set-up: the per-turn status chain's nine steps (the sub-state machine
// 0x430510 calls them one per state), the counters tick 0x4303D0, the acting
// actor's flag clear 0x4301B0, the faster-side marks 0x4453C0 and its two
// tests, the status clear 0x44F4B0 and four small helpers. Group BA of the
// seventh round. docs/battle_setup.md.
#pragma once

void BattleSetup_Inject();
