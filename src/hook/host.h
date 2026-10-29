// One engine, two hosts (docs/state3-design.md sections 0 and 4).
//
//   Bof3: bof3x.dll injected into the player's own BOF3.exe by the launcher
//         (--host bof3) - Capcom's code is mapped; the shadow fuzz, the A/B
//         switch and the reference pairs live here.
//   Exe:  bof3x.exe, our own image at 0x400000 (exe_layout.h) - or, a
//         developer's, at another base (docs/relocatable-image-design.md 1.5;
//         ImageBase(), common/image_base.h) - importing bof3x.dll - no code
//         of Capcom's is mapped; .stub is a no-access trap, and every code
//         word and code place is the function of ours the bind table names
//         (bind.h).
//
// DetectHost reads the main module's headers once, in DllMain; anything that
// is neither host is a Fatal. Everything in src/hook that walked BOF3.exe's
// section table reads the per-host constants below instead.
#pragma once

#include <cstdint>

namespace bof3 {

enum class HostKind { Bof3, Exe };

// From GetModuleHandle(nullptr)'s headers: Capcom's timestamp and
// SizeOfImage at 0x400000 (VerifyImage's check) mean Bof3; our timestamp
// (exe_layout::kTimestamp) with .stub at its RVA means Exe, at whatever base
// - checked against the 16 MB window, the linked span and BOF3X_IMAGEBASE.
// Sets Bof3_ImageMove (the asm's). Logs the answer.
void DetectHost();

// The answer; DetectHost must have run.
HostKind Host();
inline bool IsExeHost() { return Host() == HostKind::Exe; }

// Capcom's code range as this host maps it, as VAs: BOF3.exe's .text
// (0x401000 + its virtual size) hosted, the whole of .stub in the exe host.
// InCode is a test on a VA (a name: Inject's, PatchBytes'); a place's range
// is Img(CodeLo())..Img(CodeHi()) (crash.cpp).
std::uint32_t CodeLo();
std::uint32_t CodeHi();
inline bool InCode(std::uint32_t va) { return va >= CodeLo() && va < CodeHi(); }

}  // namespace bof3
