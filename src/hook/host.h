// One engine, two hosts (docs/state3-design.md sections 0 and 4).
//
//   Bof3: bof3x.dll injected into the player's own BOF3.exe by the launcher
//         (--host bof3) - Capcom's code is mapped; the shadow fuzz, the A/B
//         switch and the reference pairs live here.
//   Exe:  bof3x.exe, our own image at 0x400000 (exe_layout.h), importing
//         bof3x.dll - no code of Capcom's is mapped; .stub is int3 but for a
//         jmp at each function start we own.
//
// DetectHost reads the main module's headers once, in DllMain; anything that
// is neither host is a Fatal. Everything in src/hook that walked BOF3.exe's
// section table reads the per-host constants below instead.
#pragma once

#include <cstdint>

namespace bof3 {

enum class HostKind { Bof3, Exe };

// From GetModuleHandle(nullptr)'s headers: Capcom's timestamp and
// SizeOfImage (VerifyImage's check) mean Bof3; our timestamp
// (exe_layout::kTimestamp) with .stub at 0x401000 means Exe. Logs the answer.
void DetectHost();

// The answer; DetectHost must have run.
HostKind Host();
inline bool IsExeHost() { return Host() == HostKind::Exe; }

// Capcom's code range as this host maps it: BOF3.exe's .text
// (0x401000 + its virtual size) hosted, the whole of .stub in the exe host.
std::uint32_t CodeLo();
std::uint32_t CodeHi();
inline bool InCode(std::uint32_t va) { return va >= CodeLo() && va < CodeHi(); }

}  // namespace bof3
