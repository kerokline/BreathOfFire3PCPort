// BOF3X_STPCENSUS: a diagnostic census of the semi-transparent textured
// primitives the Direct3D draw blends, by what their texels' bit 15 says
// (docs/stp-census.md).
//
//   BOF3X_STPCENSUS=<path of a .tsv>
//
// On the PlayStation a semi-transparent textured primitive blends only the
// texels whose colour has bit 15 set; a texel with it clear is drawn opaque,
// and a colour of 0 is transparent. The port drops bit 15 (Gfx_ConvertRow,
// Tex_Convert16) and blends the whole primitive (D3d_SetBlend). For each such
// primitive the walk reaches, this reads the texels it can sample - the box of
// its (u, v), through the page's texture window, or a cell sprite's records -
// out of the VRAM shadow and its CLUT, and counts them transparent, bit 15 set
// or bit 15 clear. Rows aggregate by area, code, tpage, CLUT and window; the
// file is rewritten every 300 rendered frames.
//
// Instrumentation, not a replacement: unset, the hooks are one test of a bool
// that is never set. Set, they read game memory and write this module's own
// tables and file. No DIVERGENCE.md entry: it changes no behaviour.
#pragma once

#include <cstdint>

namespace stp_census {

extern bool g_on;   // BOF3X_STPCENSUS was set

// BOF3X_STPCENSUS read; call after InjectAll (dllmain, exe_host).
void Start();

// Gfx_DrawOTag's, before the walk of a Direct3D frame.
void Frame();

// Gfx_DrawOTag's Dispatch, before a Direct3D handler runs: the primitive at p.
void Note(std::uint32_t p);

}  // namespace stp_census
