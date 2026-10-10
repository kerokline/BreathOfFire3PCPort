// BOF3X_EXEIMAGE: base/exe/data.bin against the running BOF3.exe's .data, and
// laid over it when the two are the same bytes (docs/exe-import-engine.md
// section 4.4). The first step of state 3's data mapping, run while the game
// is still hosted: it proves the importer's image is what the engine would
// map, without changing what the game runs.
#pragma once

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

}  // namespace bof3
