// DIV-0204's drawn ring (docs/DIVERGENCE.md): the faerie village entrance on
// the desert map gets a flower ring like the Lost Shore map's, built from the
// player's own AREA151.DAT and the desert map's sand when the map loads.
#pragma once

#include <cstdint>

// After area 152's placement script, with its block, page and palettes
// loaded: the ring composed (once a session) and written into a page-0 grid
// cell and a palette row the map leaves unused, for the cell (x, z). A ring
// that cannot be made (a file missing, a map not shaped as measured) is
// logged once and not drawn; the entrance works either way.
void FaerieRing_Lay(unsigned x, unsigned z);

// After any other area's placement script: if the ring was laid, each of its
// palette entries that still holds the ring's colour (in both strips) back to
// zero, as the desert map had them; one the new area loaded is left alone.
void FaerieRing_Leave();

// For MapView_CellTextures: on area 152 with the ring laid, the texture word
// for its cell (x, z) into *word and true; false for every other cell.
bool FaerieRing_CellWord(unsigned x, unsigned z, std::uint32_t* word);
