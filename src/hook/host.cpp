#include "hook/host.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/exe_layout.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace {

bool g_known = false;
HostKind g_host = HostKind::Bof3;

}  // namespace
}  // namespace bof3

// ImageBase() - 0x400000, for the hand-written asm that addresses the image
// (task_sched.cpp, field_e2.cpp, field_modes.cpp): a VA as the displacement
// and this in a register. Set by DetectHost, before any of the game's code
// runs; 0 at the player's base. A 32-bit difference on both widths: the image
// lies below 16 MB on every host (DetectHost's window check), so the move is
// a small non-negative number, which the x86-64 asm loads RIP-relative
// (`movl Bof3_ImageMove(%rip), %ebx`, zero-extended into a 64-bit base
// register; docs/x64-asm-policy.md 4.4, docs/x64-build-design.md 7.2).
extern "C" __attribute__((used)) std::uint32_t Bof3_ImageMove = 0;

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
        g_host = HostKind::Bof3;
#endif
    } else if (pe && stamp == exe_layout::kTimestamp && nt->FileHeader.NumberOfSections > 0 &&
               std::memcmp(IMAGE_FIRST_SECTION(nt)->Name, ".stub\0\0\0", 8) == 0 &&
               IMAGE_FIRST_SECTION(nt)->VirtualAddress == exe_layout::kStubVa - exe_layout::kImageBase) {
        g_host = HostKind::Exe;
    } else {
        Fatal("the main module at %p (timestamp 0x%lX) is neither BOF3.exe (0x%X) nor bof3x.exe (0x%X): every "
              "address would be wrong; refusing to start",
              static_cast<const void*>(base), static_cast<unsigned long>(stamp), (unsigned)image::kTimestamp,
              (unsigned)exe_layout::kTimestamp);
    }
    // The image's base, as ImageBase() read it, against what may hold it.
    const std::uint32_t image = ImageBase();
    if (image != at) Fatal("host: ImageBase() reads 0x%X, the main module is at 0x%X", (unsigned)image, (unsigned)at);
    if (g_host == HostKind::Exe) {
        // The 16 MB window (design 1.2): the ordering table's links are 24
        // bits, and the packets live in .bss and .pool.
        const std::uint32_t end = image + (exe_layout::kPoolVa + exe_layout::kPoolSize - exe_layout::kImageBase);
        if (image % 0x10000 || image < 0x10000 || end > 0x1000000)
            Fatal("host: bof3x.exe at 0x%X, its image to 0x%X: the image must lie below 16 MB (the ordering table's "
                  "24-bit links) at a 64 KB boundary",
                  (unsigned)image, (unsigned)end);
        // A moved image lies wholly off the linked span 0x400000..its
        // SizeOfImage: a value in that span is read as a VA where a word may
        // be either a VA or a place (CodePlace, image_base.h; PatchBytes,
        // detour.cpp), and the fence reserves it (exe_host.cpp).
        const std::uint32_t span = nt->OptionalHeader.SizeOfImage;
        if (image != exe_layout::kImageBase && image < exe_layout::kImageBase + span && image + span > exe_layout::kImageBase)
            Fatal("host: bof3x.exe at 0x%X..0x%X overlaps the linked span 0x%X..0x%X, where a word is read as a VA; "
                  "a moved image lies wholly off it (0x%X..0x%X fits the 16 MB window)",
                  (unsigned)image, (unsigned)(image + span), (unsigned)exe_layout::kImageBase,
                  (unsigned)(exe_layout::kImageBase + span), (unsigned)(exe_layout::kImageBase + span),
                  (unsigned)(0x1000000 - span));
    }
    // BOF3X_IMAGEBASE, the launcher's developer switch: the executable it
    // started must be the one at that base.
    char asked[32];
    if (const DWORD n = bof3::os::GetEnv("BOF3X_IMAGEBASE", asked, sizeof asked); n > 0 && n < sizeof asked) {
        char* tail = nullptr;
        const unsigned long want = std::strtoul(asked, &tail, 0);
        if (!tail || *tail || want != image)
            Fatal("host: BOF3X_IMAGEBASE=%s, but the image is at 0x%X", asked, (unsigned)image);
    }
    Bof3_ImageMove = image - image::kBase;
    g_known = true;
    Log("host: %s (main module at 0x%X, timestamp 0x%X; the image at 0x%X, %s)",
        g_host == HostKind::Exe ? "bof3x.exe, our own executable" : "BOF3.exe, the player's own (hosted)", (unsigned)at,
        (unsigned)stamp, (unsigned)image,
        image == image::kBase ? "where it was linked" : "moved from 0x400000: its pointer words follow it");
}

HostKind Host() {
    if (!g_known) Fatal("host: asked before DetectHost ran");
    return g_host;
}

std::uint32_t CodeLo() { return IsExeHost() ? exe_layout::kStubVa : image::kTextVa; }

std::uint32_t CodeHi() {
    return IsExeHost() ? exe_layout::kStubVa + exe_layout::kStubSize : image::kTextVa + image::kTextSize;
}

}  // namespace bof3
