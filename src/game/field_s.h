// Round twelve group FS: the shop overlay's remainder (the field save's
// confirm, the rest sequence's first two states, the party formation screen,
// the resistance shop, the shared ability list's screens, two window setups)
// and the equip screen's two choosers. docs/field_s.md.
#pragma once

void FieldS_Inject();

#include <cstdint>

namespace field_s {
// Where the original's call at 0x581313 (in PartyForm_DrawReserve's body)
// reaches now: Menu_DrawFrame under DIV-0011, the empty 0x4DF820 without it.
std::uint32_t FrameCallTarget();

// BOF3X_SHADOW=field_s: the start-up fuzz, field_s_fuzz.cpp. Clones every
// original before FieldS_Inject patches it.
void SelfTest();
}  // namespace field_s
