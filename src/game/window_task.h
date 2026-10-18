// The window/task layer (originals 0x5954B0..0x596150, 0x59E230, 0x59E310):
// the per-frame body of every window record, its three kinds, their five
// states each, and the frame, outline and line the open window draws. With
// the area-change funnel that sits just above it in the image
// (Field_ChangeArea 0x594E00 and the three zone lookups it and its neighbour
// use). docs/window-task.md.
#pragma once

void WindowTask_Inject();
// DIV-0143: Area_PickMusic keeps no track while the kept one fades to a stop -
// after every module's self-test, which all ran Capcom's keep.
void AreaKeepStoppingNone_Arm();

// The track an area's zone record names: byte +6 itself when +7 is zero, else
// the first flagged entry of the music set +6 names (the table part of
// Area_PickMusic, which the area change runs; DIV-0092's second amendment
// runs it for the leader's zone after an event battle).
unsigned char Area_ZoneTrack(const unsigned char* zone);
