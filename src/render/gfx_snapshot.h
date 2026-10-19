// The quicksave's graphics participant, "GFX" (the quicksave round's group
// SP-G; docs/quicksave.md, the graphics participant, docs/quicksave-census.md 3.1 and 5).
//
// A quick-load writes the span back: the game's logic, the VRAM shadow, the
// cache tables' words. What the span cannot carry is the display's: the
// texture caches name surfaces of the process that took the quicksave, the
// palette rows' converted pixels sit on the heap, the device's pipe state and
// a few of our statics live in the DLL. This participant makes the first frame
// after a restore draw what the live game drew from the same state:
//
//   - before the span is written (Snapshot_Register's discard step, SP-A's): the live
//     process's display words kept (the objects, the window's size and scale,
//     the formats - what Display_Setup and a resize write), and every texture
//     the live caches hold released after the next present;
//   - after it: the palette rows' converted pixels and generations as at the
//     take (carried - a row can lag the shadow, Gfx_LoadImage does not
//     convert); Gfx_TexCache and D3d_CellTexCache emptied (rebuilt on demand
//     from the restored shadow and rows); Font_TexCache rebuilt entry for
//     entry, its words as at the take; the pipe state as at the take; the
//     frame-buffer grabs forgotten; DIV-0071's last pass, DIV-0085's side
//     faces, DIV-0041's slide split and random split as at the take.
#pragma once

// InjectAll, before Snapshot_Inject: registers "GFX" with its discard
// step (before the span is written); the start-up self-test with BOF3X_SELFTEST_ONLY (exe host) or
// BOF3X_SHADOW=gfx_snapshot (hosted).
void GfxSnapshot_Inject();
