// The event script's field side: the party leader's frame and its first two
// states, the area set-up that places the party, the party-set loader and
// their helpers - 31 originals from 0x52D8F0 to 0x536AC0, plus 0x572570 /
// 0x572590 and Sprite_ReleaseTint 0x454DC0. docs/field-event.md.
#pragma once

#include <cstdint>

void FieldEvent_Inject();

namespace field_event {
extern std::uint8_t g_mantle_cap;   // defined in field_event.cpp (source-layout B0: promoted for a moved reader)
}  // namespace field_event
