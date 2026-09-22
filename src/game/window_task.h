// The window/task layer (originals 0x5954B0..0x596150, 0x59E230, 0x59E310):
// the per-frame body of every window record, its three kinds, their five
// states each, and the frame, outline and line the open window draws. With
// the area-change funnel that sits just above it in the image
// (Field_ChangeArea 0x594E00 and the three zone lookups it and its neighbour
// use). docs/window-task.md.
#pragma once

void WindowTask_Inject();
