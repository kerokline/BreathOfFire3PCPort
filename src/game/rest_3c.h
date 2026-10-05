// Group R3C (round fourteen, wave three): the battle engine's band
// 0x44D000..0x44E4AA - 61 Effect_Handlers slots (50..85 and 87..111; the
// cut's 60 and 0x44D8B0, slot 91, which no list had). docs/rest_3c.md.
//
// Each is a slot of Effect_Handlers (0x64E73C), the table Effect_ApplyResult
// calls with no argument through 0x64E540[ability] (kind 4) or
// 0x64E72C[category][item]: it fills the result record *0x904B60 (+4 the HP
// delta, positive damage; +6 the AP delta; +8 flags) for the actor 0x904B34
// and the target 0x904B54, or changes a record directly. The names are the
// code's shape (a hypothesis each: which ability or item reaches a slot was
// not traced). Their prototypes are symbols.gen.h's (symbols.toml).
#pragma once

void Rest3C_Inject();

namespace rest_3c {
// BOF3X_SHADOW=rest_3c: the start-up fuzz, rest_3c_fuzz.cpp - one
// boss_harness::Run over the group, Group::engine set. Clones every original
// before Rest3C_Inject patches it.
void SelfTest();
}  // namespace rest_3c
