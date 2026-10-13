// BOF3X_EXEIMAGE: base/exe/data.bin against the running BOF3.exe's .data, and
// laid over it when the two are the same bytes (docs/exe-import-engine.md
// section 4.4). The first step of state 3's data mapping, run while the game
// is still hosted: it proves the importer's image is what the engine would
// map, without changing what the game runs.
#pragma once

#include <cstdint>

namespace bof3 {

// BOF3X_EXEIMAGE=<cache> (the directory importer.py build / exe_tables.py
// build wrote, holding base/exe/data.toml and data.bin). Unset: nothing.
//
// The image's address and size against the live .data section header; .bss
// zero to data.toml's bss_end; then every byte against the live .data. An
// image built from BOF3.exe (build pc-zh) must be byte-identical - a Fatal
// otherwise - and is then copied over .data, which changes no byte. A
// disc-built image is compared and counted by data.toml's ranges, never
// copied: its unfilled words are not the PC's. Runs before InjectAll, while
// .data is still the loader's.
void ExeImage_Check();

// The exe host (docs/state3-design.md section 1.4): BOF3X_EXEIMAGE=<cache> is
// the SOURCE of .data rather than a check. data.toml's image must be `size`
// bytes at `va` with its .bss to `bss_end`; data.bin is copied to `to` and
// the engine's own regions filled (exe_own.h). Built from BOF3.exe (pc-zh),
// those regions are first verified against it. Built from a disc
// (docs/exe-import-engine.md section 4.4), every range must be a class the
// engine accounts for, as recipes/exe-gaps.tsv named them: no pointer word
// unfilled, each `gap` range (zero where the PC has a value no disc
// carries) logged once - a Fatal with BOF3X_EXEIMAGE_STRICT=1. False,
// copying nothing, when the variable is unset.
bool ExeImage_Source(void* to, std::uint32_t va, std::uint32_t size, std::uint32_t bss_end);

// Whether ExeImage_Source mapped a disc-built image: the running .data is
// then not the PC's, and a check against the PC's reads the file instead.
bool ExeImage_DiscMapped();

// DIVERGENCE DIV-0111 (docs/launcher-suites.md section 9): the exe host, after
// .data is in place (the file's or BOF3X_EXEIMAGE's, filled) and before the
// compare below: each layer BOF3X_OPT names whose cache (BOF3X_CACHE, not
// under BOF3X_CACHE_DATA=0) has opt\<layer>\exe\place.toml is laid over .data
// at `to`, in BOF3X_OPT's order - place.bin's bytes at the toml's ranges, the
// file's sha256 and size checked, every range inside .data and, over a
// disc-built image, in a class that need not equal the PC's. A malformed
// layer is a Fatal. Nothing when no such layer is named. The hosted build
// refuses such a layer (dat_load.cpp ReadOptLayers): it keeps Capcom's .data.
void ExeImage_LayOptTables(void* to, std::uint32_t va, std::uint32_t size);

// BOF3X_SHADOW=dat_cache: the layer's reader and laying on synthetic files in
// memory, with controls (a wrong hash, a range outside .data, ranges out of
// order, a byte count that differs, a range over a must-equal class).
void ExeImage_OptSelfTest();

// After every fill (the engine's own regions, the key table): a mapped
// disc-built image against the file's .data when BOF3.exe is there, every
// differing range logged with its class; a difference in a class that must
// equal the PC's (none, own, keys, layout, rebuilt) is a Fatal. Nothing
// without a disc-built image; a line saying so without the file.
void ExeImage_CompareWithFile();

}  // namespace bof3
