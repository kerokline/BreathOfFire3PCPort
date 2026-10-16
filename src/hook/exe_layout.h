// The layout of bof3x.exe, our own executable (docs/state3-design.md section
// 1.2): one table, read by the engine (host.cpp's layout check, the stub
// page, the data copy, the draw pool) and by tools/make_exe.py, which writes
// the file from it and verifies the result - so the two cannot drift apart.
//
// make_exe.py parses the `kName = 0x...;` lines of this namespace; keep each
// constant on one line in that form. Every value is layout, like
// symbols.toml's addresses, not game data (CLAUDE.md rules 1 and 3): Capcom's
// sections' virtual extents, which our image occupies so that the loader
// reserves the range before any heap or thread block can land in it
// (docs/state3-build.md section 1.1, measured).
#pragma once

#include <cstdint>

namespace bof3::exe_layout {

// ImageBase: Capcom's, so GetModuleHandle(nullptr) is the hinstance his
// WinMain received.
inline constexpr std::uint32_t kImageBase = 0x400000;
// .stub: BOF3.exe's .text range, int3 (0xCC) except a jmp at each function
// start we own (section 2). RWX while the engine writes it, then RX.
inline constexpr std::uint32_t kStubVa = 0x401000;
inline constexpr std::uint32_t kStubSize = 0x1C3000;
// .game: .rdata 0x5C4000..0x5DA000 and .data 0x5DA000..0x93D6EC (its .bss
// from 0x676000), copied in by the engine; zero at load. To 0x93F000, where
// Capcom's .rsrc ended.
inline constexpr std::uint32_t kGameVa = 0x5C4000;
inline constexpr std::uint32_t kGameSize = 0x37B000;
// The two sections copied into .game, as BOF3.exe's section table gives them
// (docs/exe-resources.md section 2; checked against the player's file before
// a byte is copied): .rdata's file bytes fill its pages (0x16000 of them for
// a virtual size of 0x15D22, as the loader maps them); .data's 0x9C000 file
// bytes, then its .bss, zero, to 0x93D6EC.
inline constexpr std::uint32_t kRdataVa = 0x5C4000;
inline constexpr std::uint32_t kRdataBytes = 0x16000;
inline constexpr std::uint32_t kDataVa = 0x5DA000;
inline constexpr std::uint32_t kDataBytes = 0x9C000;
inline constexpr std::uint32_t kBssEnd = 0x93D6EC;
// .pool: DIV-0062's 2,048 draw items (draw_pool_room.h, 0x90 bytes each),
// below 16 MB for the 24-bit links; the file carries the stamp.
inline constexpr std::uint32_t kPoolVa = 0x93F000;
inline constexpr std::uint32_t kPoolSize = 0x48000;
// The rest of ours - .text (the entry), .idata, .rsrc - from here.
inline constexpr std::uint32_t kOwnVa = 0x987000;
// The PE header's stack (section 5; docs/crt-startup.md 11.3's floor is
// reserve >= 0x40000, commit >= 0x10000).
inline constexpr std::uint32_t kStackReserve = 0x100000;
inline constexpr std::uint32_t kStackCommit = 0x10000;
// TimeDateStamp: our identity. Host detection (host.cpp) tells our image from
// Capcom's (0x3ADD0C06) by it and by .stub at kStubVa. A fixed constant, not
// the build's clock, so every build of the layout carries the same stamp; it
// decodes to 2026-10-12 07:36:32 UTC, the day state 3 merged (PR #53), rather
// than the 0xB0F30003 of the first builds, which read as the year 2064 and fed
// antivirus heuristics ("timestamp in the future", 2026-10-16's VirusTotal
// run of the launcher).
inline constexpr std::uint32_t kTimestamp = 0x6ACC8E00;

static_assert(kStubVa + kStubSize == kGameVa, ".stub ends where .rdata begins");
static_assert(kGameVa + kGameSize == kPoolVa, ".game ends where .pool begins");
static_assert(kPoolVa + kPoolSize == kOwnVa, ".pool ends where our own sections begin");

}  // namespace bof3::exe_layout
