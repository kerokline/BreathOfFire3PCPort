// One engine, two hosts (docs/state3-design.md sections 0 and 4).
//
//   Bof3: bof3x.dll injected into the player's own BOF3.exe by the launcher
//         (--host bof3) - Capcom's code is mapped; the shadow fuzz, the A/B
//         switch and the reference pairs live here.
//   Exe:  bof3x.exe, our own image at 0x400000 (exe_layout.h) - or, a
//         developer's, at another base (docs/relocatable-image-design.md 1.5;
//         ImageBase(), common/image_base.h) - importing bof3x.dll - no code
//         of Capcom's is mapped; .stub is a no-access trap, and every code
//         word and code place is its VA's place in the bind table, a jmp to ours
//         (bind.h).
//
// The host is told once, by SetHost (host.cpp, portable), which checks the
// image's place and keeps the answer. On Windows DetectHost reads the main
// module's headers in DllMain and calls it (host_win32.cpp); anything that is
// neither host is a Fatal. A Linux start-up, the exe host alone, calls it with
// what it mapped (docs/linux-round.md, "R3: what a Linux start-up needs from
// the PE host"). Everything in src/hook that walked BOF3.exe's section table
// reads the per-host constants below instead.
#pragma once

#include <cstdint>

namespace bof3 {

enum class HostKind { Bof3, Exe };

// What a start-up found: the host, the main module's base (the image's base,
// which ImageBase() must read the same), its header's timestamp (for the log
// line alone) and its SizeOfImage (the exe host's linked span, 0x400000 + this:
// a moved image must lie wholly off it).
struct HostFound {
    HostKind kind;
    std::uint32_t module;
    std::uint32_t stamp;
    std::uint32_t span;
};

// Keeps the answer, once (a second call is a Fatal): ImageBase() against
// `module`; for the exe host the 16 MB window at a 64 KB boundary and the
// linked span; BOF3X_IMAGEBASE against the base. Sets Bof3_ImageMove (the
// asm's). Logs the answer.
void SetHost(const HostFound& found);

// Windows only (host_win32.cpp): from GetModuleHandle(nullptr)'s headers,
// Capcom's timestamp and SizeOfImage at 0x400000 (VerifyImage's check) mean
// Bof3; our timestamp (exe_layout::kTimestamp) with .stub at its RVA means
// Exe, at whatever base. Then SetHost.
void DetectHost();

// The answer; SetHost must have run.
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
