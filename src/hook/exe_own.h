// The bytes of BOF3.exe's .data no disc carries that are the port's own -
// the file layer's names and formats, the window, DirectX, MCI and config
// strings and initial values, and two computed tables - held by meaning as
// the engine's (docs/exe-engine-own.md). A disc-built image of .data
// (tools/exe_tables.py) leaves them zero; ExeOwn_Fill writes them.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bof3 {

// One region the engine holds: [lo, hi) of .data's PC addresses, every byte
// of it written by ExeOwn_Fill. tools/exe_own.py reads the table's rows from
// exe_own.cpp's source (its `// regions:` block), so the loader, the gate and
// data.toml's `own` class can name the same ranges.
struct ExeOwnRegion {
    std::uint32_t lo, hi;
    const char* what;
};

// The regions, sorted, not overlapping, inside .data; *count set.
const ExeOwnRegion* ExeOwn_Regions(std::size_t* count);

// Writes every region over an image of .data whose first byte is the PC's
// 0x5DA000 (exe_layout.h's kDataVa). Nothing outside the regions changes.
void ExeOwn_Fill(std::uint8_t* data_base);

// The engine's bytes against BOF3.exe's: fills a scratch copy and compares
// each region with `pc_data` (an image of the PC's .data, first byte at
// 0x5DA000), a Fatal naming the region and the first differing address.
// `against` names the image in the log line.
void ExeOwn_Verify(const std::uint8_t* pc_data, const char* against);

// InjectAll's check, every start in both hosts: the running .data is still
// the PC's there (the file's or the loader's, or a pc-zh cache already
// verified equal to it), so the regions are verified against it. With
// BOF3X_SHADOW=exe_own also the self-test: a fill into a zeroed scratch image
// writes exactly the regions' bytes and nothing outside them.
void ExeOwn_CheckAtStart();

}  // namespace bof3
