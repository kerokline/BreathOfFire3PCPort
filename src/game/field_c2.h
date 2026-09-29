// The field core's second half (round twelve, group FC2, 0x46BBF0..0x46D5ED):
// four kinds of the Effect_Objects pool - kind 0x30, an object that slides a
// cell in the leader's facing and shatters into 24 faces and 8 sparks when
// the way is blocked; kind 0x34, five variants of bursts and falling pieces;
// kind 0x3A, a thrown object that clears cells and may find zenny; kind 0x41,
// a number bouncing over a party member. docs/field_c2.md.
#pragma once

void FieldC2_Inject();

namespace field_c2 {
// BOF3X_SHADOW=field_c2: the start-up fuzz, field_c2_fuzz.cpp. Clones every
// original before FieldC2_Inject patches it.
void SelfTest();
}  // namespace field_c2
