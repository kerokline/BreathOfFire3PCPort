// Top-level modes, the system choice and the field core (originals
// 0x496250..0x496B57, 0x498A30, 0x516E70, 0x517290..0x517327,
// 0x525370..0x525958, 0x539AD0..0x53A2A7, 0x53D830, 0x5960D0): the look-around
// mode's camera and the shop mode's three steps, the menu's transition wait,
// the message box's system choice, the 8 px UI string draw, the field's
// loading, menu and shop frames, the field core's state-2 and fade steps,
// scenario chapter 1's frame, area entry and step hook, and the window
// cursor draw. docs/mode_states.md.
#pragma once

void ModeStates_Inject();
