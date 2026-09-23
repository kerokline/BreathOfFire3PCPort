// The field frame loop's unread callees: originals 0x494030 (the effect
// objects' runner), 0x469E30 / 0x469E50 / 0x469EF0 / 0x469F70 (effect kind
// 0x13, the camera turn, and its three states), 0x455250 (the slot records),
// 0x454AD0 (the tint records' CLUT rebuild) and 0x531B60 (the party members'
// screen updates). docs/frame-callees.md.
#pragma once

void FrameCallees_Inject();
