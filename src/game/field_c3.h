// Round twelve group FC3: object steering (the approach / avoid directions,
// op E7, the fade cases), mode 8's and mode 11's frames, and the field
// core's state-2 sub-states 0 and 1 and 3..8 with every step of theirs
// (0x5172C0..0x5195F9, 0x525390..0x526DAF). docs/field_c3.md.
#pragma once

void FieldC3_Inject();

namespace field_c3 {
// BOF3X_SHADOW=field_c3: the start-up fuzz, field_c3_fuzz.cpp. Clones every
// original before FieldC3_Inject patches it.
void SelfTest();
}  // namespace field_c3
