// The host's state, portable (docs/linux-round.md, "R3: what a Linux start-up
// needs from the PE host", proposed for LC): which host this is, where its
// image lies, and the checks of that place - the 16 MB window, the linked
// span, BOF3X_IMAGEBASE - with Bof3_ImageMove, the asm's word. What answers
// the identity and the base is per host: on Windows DetectHost reads the main
// module's PE header (host_win32.cpp); a Linux start-up knows where it mapped
// the image itself. Each hands its answer to SetHost once.
#include "hook/host.h"

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
// and this in a register. Set by SetHost, before any of the game's code
// runs; 0 at the player's base. A 32-bit difference on both widths: the image
// lies below 16 MB on every host (SetHost's window check), so the move is
// a small non-negative number, which the x86-64 asm loads RIP-relative
// (`movl Bof3_ImageMove(%rip), %ebx`, zero-extended into a 64-bit base
// register; docs/x64-asm-policy.md 4.4, docs/x64-build-design.md 7.2).
extern "C" __attribute__((used)) std::uint32_t Bof3_ImageMove = 0;

namespace bof3 {

#if !defined(_WIN32)
// ImageBase()'s source off Windows (image_base.h): no process environment
// block holds the image's base, so the start-up that mapped the image says
// where, once, before anything places an address.
namespace image {
std::uint32_t g_runBase = 0;

void ImageBaseUnset() {
    Fatal("ImageBase() read before the start-up set it (SetImageBase): every place would be wrong; refusing to go on");
}
}  // namespace image

void SetImageBase(std::uint32_t base) {
    if (base == 0) Fatal("SetImageBase(0): the image cannot lie at address 0");
    if (image::g_runBase != 0 && image::g_runBase != base)
        Fatal("SetImageBase(0x%X): the image's base was already set to 0x%X, and it never changes in a process's life",
              (unsigned)base, (unsigned)image::g_runBase);
    image::g_runBase = base;
}
#endif

void SetHost(const HostFound& found) {
    if (g_known) Fatal("host: SetHost called twice");
    g_host = found.kind;
    const std::uint32_t at = found.module;
    const std::uint32_t stamp = found.stamp;
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
        const std::uint32_t span = found.span;
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
    if (const std::uint32_t n = bof3::os::GetEnv("BOF3X_IMAGEBASE", asked, sizeof asked); n > 0 && n < sizeof asked) {
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
