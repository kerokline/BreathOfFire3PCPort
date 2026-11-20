// Windows-only by design (docs/linux-round.md, LB1, and "R3: what a Linux
// start-up needs from the PE host"): the hosts are told apart by the main
// module's PE header. The state and the checks of the image's place are the
// portable host.cpp's; this file answers the identity and the base and hands
// them to SetHost. A Linux start-up answers them itself (it mapped the image).
#include "hook/host.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/exe_layout.h"
#include "hook/log.h"

namespace bof3 {

void DetectHost() {
    // The main module's base as the 32-bit word every place is built on
    // (image_base.h): Low32 is a Fatal above 4 GB, where the 64-bit loader
    // never puts a fixed-base PE32+ (docs/x64-build-design.md 1.2).
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto at = Low32(base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    // The width's header: IMAGE_NT_HEADERS32 in the 32-bit DLL, whose main
    // module is a PE32 (BOF3.exe, or the PE32 bof3x.exe); IMAGE_NT_HEADERS64
    // in the 64-bit one, whose bof3x.exe make_exe.py writes as a PE32+
    // (docs/x64-build-design.md 1.4). The fields read below lie at the same
    // offsets in both.
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const bool pe = dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE;
    const std::uint32_t stamp = pe ? nt->FileHeader.TimeDateStamp : 0;
#if defined(__x86_64__)
    // A 64-bit DLL's main module is a PE32+ or nothing: a PE32 bof3x.exe
    // could not have loaded it.
    if (pe && nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        Fatal("host: the main module at %p has optional-header magic 0x%X, not a PE32+'s 0x%X: a 64-bit bof3x.dll "
              "runs in the x86-64 bof3x.exe alone (docs/x64-build-design.md 1)",
              static_cast<const void*>(base), (unsigned)nt->OptionalHeader.Magic, (unsigned)IMAGE_NT_OPTIONAL_HDR64_MAGIC);
#endif
    HostKind kind = HostKind::Bof3;
    // The exe host by its identity alone (docs/relocatable-image-design.md
    // 1.3): our timestamp and .stub at its RVA, wherever the header based it.
    if (pe && at == image::kBase && stamp == image::kTimestamp && nt->OptionalHeader.SizeOfImage == image::kSizeOfImage) {
#if defined(__x86_64__)
        // The hosted build is 32-bit for ever (docs/x64-build-round.md 2): a
        // 64-bit DLL cannot be inside BOF3.exe's process, and nothing of the
        // hosted mode is built to run at this width (detour.cpp).
        Fatal("host: the main module is BOF3.exe, which a 64-bit bof3x.dll cannot be hosted in; the hosted build is the "
              "32-bit one (bof3x-launcher --host bof3)");
#else
        kind = HostKind::Bof3;
#endif
    } else if (pe && stamp == exe_layout::kTimestamp && nt->FileHeader.NumberOfSections > 0 &&
               std::memcmp(IMAGE_FIRST_SECTION(nt)->Name, ".stub\0\0\0", 8) == 0 &&
               IMAGE_FIRST_SECTION(nt)->VirtualAddress == exe_layout::kStubVa - exe_layout::kImageBase) {
        kind = HostKind::Exe;
    } else {
        Fatal("the main module at %p (timestamp 0x%lX) is neither BOF3.exe (0x%X) nor bof3x.exe (0x%X): every "
              "address would be wrong; refusing to start",
              static_cast<const void*>(base), static_cast<unsigned long>(stamp), (unsigned)image::kTimestamp,
              (unsigned)exe_layout::kTimestamp);
    }
    // The checks of the place, the state and the log line: the portable part
    // (host.cpp). SizeOfImage is the exe host's linked span there.
    SetHost(HostFound{kind, at, stamp, static_cast<std::uint32_t>(nt->OptionalHeader.SizeOfImage)});
}

// detour.h's VerifyImage: the hosted build's check of BOF3.exe's header. It
// lived in detour.cpp until the Linux round's R3 moved it to host.cpp, beside
// the other reader of the main module's PE header, so that detour.cpp
// compiles for a system without one (docs/linux-round.md); here since the
// split of host.cpp (stage LC, wave A). Unchanged.
void VerifyImage() {
#if defined(__x86_64__)
    Fatal("VerifyImage in the 64-bit DLL: BOF3.exe is a 32-bit process, which never hosts this DLL "
          "(docs/x64-build-round.md 2; the hosted build is bof3x-launcher --host bof3, 32-bit)");
#else
    auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(base) != image::kBase)
        Fatal("main module is at %p, not 0x%X: this is not BOF3.exe, or it was rebased",
              (void*)base, (unsigned)image::kBase);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.TimeDateStamp != image::kTimestamp ||
        nt->OptionalHeader.SizeOfImage != image::kSizeOfImage)
        Fatal("main module is not the BOF3.exe that symbols.toml describes "
              "(timestamp 0x%lX size 0x%lX, expected 0x%X / 0x%X). Every address "
              "would be wrong; refusing to patch.",
              nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage,
              (unsigned)image::kTimestamp, (unsigned)image::kSizeOfImage);
    Log("image verified: base 0x%X timestamp 0x%X", (unsigned)image::kBase,
        (unsigned)image::kTimestamp);
#endif
}

}  // namespace bof3
