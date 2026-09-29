// The boss harness's own self-test of its round-twelve widening (group EH,
// docs/boss_harness.md section 10.8): every new shape, Clone::state_cell,
// Via::state_cell and the engine frame driven with Capcom's function on both
// sides - the harness's copy against a copy of this file's with every call
// routed through the recorders - so that a shape that cannot call, seed or
// compare shows before a battle group depends on it. It takes no function:
// no BOF3_INJECT, no impl.
#pragma once

void BossHarnessEh_Inject();

namespace boss_harness_eh {
// BOF3X_SHADOW=boss_harness_eh. BOF3X_EH_CONTROL=n plants control n (1..9)
// in this file's copy, which the run must refuse.
void SelfTest();
}  // namespace boss_harness_eh
