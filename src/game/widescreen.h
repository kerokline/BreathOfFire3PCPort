// Widescreen (DIVERGENCE DIV-0041, docs/widescreen.md): the picture 426 x 240
// with the game's 320 x 240 view centred in it - the PSP's approach, every
// primitive shifted right by 53 columns on the way to the screen, and the
// culls and edge-anchored elements that the wider view exposes re-authored.
// BOF3X_WIDE=1 turns it on; off, nothing here changes anything.
#pragma once

// Extra columns each side of the 320-wide view: 53 under BOF3X_WIDE=1, 0
// otherwise. Read once from the environment; any other value is Fatal.
unsigned Widescreen_Columns();

// The columns each side once Widescreen_Inject has run - 0 before it, so a
// start-up fuzz that runs earlier compares the original's layout - and 0
// for good when the view is not wide. What our own drawing code reads.
unsigned Widescreen_Live();

// The terrain cull of MapView_Build 0x56EC00 (ours, src/game/map_layers.cpp):
// the kept screen-x interval, -50..370 as the original has it, widened by
// Widescreen_Inject to -150..470 when the view is wide.
extern float Widescreen_TerrainLo, Widescreen_TerrainHi;

// Re-aims the four x-range constants of AreaMap_FrameAreaBD 0x510780 (still
// Capcom's) at wider copies and widens the terrain cull above. Last in
// inject_all.cpp: every module's start-up fuzz has run against the original
// bounds by then. Does nothing unless the view is wide.
void Widescreen_Inject();
