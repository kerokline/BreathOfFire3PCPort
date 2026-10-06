// DIVERGENCE DIV-0079 (the owner, 2026-10-06): the six LINE handlers - F2, F3,
// F4 (flat) and G2, G3, G4 (a colour per corner) - draw each segment as a quad
// of the scale's width instead of a Direct3D line strip, which rasterises one
// screen pixel wide at any window scale (a third of the PlayStation's pixel at
// the owner's). One segment per pair of corners, square ends, a smooth
// diagonal; the colour, depth and blend of each end are the corners' as the
// handler set them. docs/DIVERGENCE.md DIV-0079; the switch is armed after
// every module's self-test (BOF3X_LINES=0 leaves the strip).
#pragma once

namespace d3d_lines {

// 0 until Arm has run: the handlers draw the original's line strip.
extern unsigned char g_wide;

// Draws corners - 1 segments from the vertices the handler has already put in
// D3d_Vertices (0..corners - 1: scaled x, y; z; rhw; diffuse; the rest as the
// last draw left it), each as a TRIANGLESTRIP of four through the device. The
// vertex block is overwritten. Returns the last DrawPrimitive's answer.
long DrawWide(unsigned corners);

// After every module's self-test: reads BOF3X_LINES and sets g_wide.
void Arm();

}  // namespace d3d_lines
