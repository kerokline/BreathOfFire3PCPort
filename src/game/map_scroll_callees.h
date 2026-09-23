// Internal to map_scroll.cpp and map_scroll_fuzz.cpp: every direct call the
// nine functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. Four of the callees are ours in this module (AreaMap_BakePatches,
// AreaMap_SetupEntries, AreaMap_ClutCycleStart, MapView_PlaceRuns); through
// the pointers each function is tested alone. AreaMap_SetupEntries' indirect
// calls go through AreaMap_SetupHandlers, which the fuzz swaps as a table.
#pragma once

namespace map_scroll {

struct Callees {
    // the four shifts (and Field_ViewReset's cell loop)
    void (__cdecl* release_cell)(unsigned char*);            // DrawItemPool_ReleaseCell 0x56FC00
    void (__cdecl* cell_to_map)(int, int, unsigned char*);   // MapView_CellToMap 0x56F910
    // Field_ViewReset
    void (__cdecl* geom_screen)(long);                       // Gte_SetGeomScreen 0x5A7B00
    void (__cdecl* geom_offset)(long, long);                 // Gte_SetGeomOffset 0x5A7AE0
    void (__cdecl* back_color)(long, long, long);            // Gte_SetBackColor 0x5A7B60
    void (__cdecl* set_poly_ft4)(unsigned char*);            // Gpu_SetPolyFT4 0x5A75D0
    void (__cdecl* set_shade_tex)(unsigned char*, unsigned); // Gpu_SetShadeTex 0x5A77A0
    long (__cdecl* elevation)(long, long);                   // AreaMap_Elevation 0x5720C0
    void (__cdecl* bake_patches)();                          // AreaMap_BakePatches 0x56FAD0
    void (__cdecl* setup_entries)();                         // AreaMap_SetupEntries 0x571720
    void (__cdecl* place_runs)();                            // MapView_PlaceRuns 0x571FF0
    // AreaMap_SetupEntries
    void (__cdecl* clut_start)();                            // AreaMap_ClutCycleStart 0x5717B0
    void (__cdecl* apply_patch)(const unsigned char*);       // AreaMap_ApplyPatch 0x571110
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (map_scroll_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace map_scroll
