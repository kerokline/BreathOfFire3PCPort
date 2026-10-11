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
// bytes at `va` with its .bss to `bss_end`, built from BOF3.exe (pc-zh) - a
// disc-built image is refused; data.bin is then copied to `to`. False,
// copying nothing, when the variable is unset.
bool ExeImage_Source(void* to, std::uint32_t va, std::uint32_t size, std::uint32_t bss_end);

}  // namespace bof3
