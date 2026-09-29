// Round twelve group FC1: the field core's first half, 0x461800 and
// 0x469D10..0x46BBF0 - the Config screen's row label and the state handlers
// of effect kinds 0x04, 0x06 (two ticks), 0x14, 0x17, 0x19, 0x1B, 0x30,
// 0x31, 0x32, 0x37 and 0x3C with their helpers. docs/field_c1.md.
#pragma once

void FieldC1_Inject();

namespace field_c1 {
// BOF3X_SHADOW=field_c1: the start-up fuzz, field_c1_fuzz.cpp. Clones every
// original before FieldC1_Inject patches it.
void SelfTest();
}  // namespace field_c1
