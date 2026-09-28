// World 4's areas 188..191 (the PSX's BIN/WORLD04/AREA188..191.EMI),
// compiled into the exe at 0x42A320..0x42BD60: the band's 51 functions -
// area 188's choices, map-patch handlers, tail kind 43 and init; area 189's
// two choices, tail kind 50, init and the walk it compiles as leader state 13
// (Field_LeaderStates[13], five states in .data and twelve helpers: a
// stepped walk with counters, events every so many steps, an exit button);
// area 191's choices and handlers (three shared with area 192), a scale
// animation of three states, tail kind 53, a step hook, its init and the talk
// message table the chapter code asks by a member. Area 190 has no code (its
// EMI has 4 code bytes). Round ten group AR4E, through the area harness
// (area_harness.h). docs/area_w4e.md.
#pragma once

void AreaW4e_Inject();

namespace area_w4e {
// BOF3X_SHADOW=area_w4e: the start-up fuzz, area_w4e_fuzz.cpp - one
// area_harness::Run per area with code. Clones every original before
// AreaW4e_Inject patches it.
void SelfTest();
}  // namespace area_w4e
