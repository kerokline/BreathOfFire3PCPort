// Round twelve group FE2: the field engine's event-script rest, the leader's
// hop and step helpers, the mode-11 object, the field tail kinds of the
// engine, three map-cell draw handlers, a map cell cleared, and the
// item-trade screen's states. docs/field_e2.md.
#pragma once

#include <cstdint>

void FieldE2_Inject();
extern unsigned char g_trade_confirm_layout;   // defined in field_e2.cpp, armed by FieldE2_Inject (named here for a moved reader: source-layout B0)

namespace field_e2 {
extern std::uint8_t g_name_nul;   // defined in field_e2.cpp (source-layout B0: promoted for a moved reader)
// BOF3X_SHADOW=field_e2: the start-up fuzz, field_e2_fuzz.cpp. Clones every
// original before FieldE2_Inject patches it.
void SelfTest();
}  // namespace field_e2
