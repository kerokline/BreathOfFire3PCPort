#include "hook/host.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/exe_layout.h"
#include "hook/log.h"

namespace bof3 {
namespace {

bool g_known = false;
HostKind g_host = HostKind::Bof3;

}  // namespace

void DetectHost() {
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const bool pe = dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE;
    const std::uint32_t stamp = pe ? nt->FileHeader.TimeDateStamp : 0;
    if (pe && at == image::kBase && stamp == image::kTimestamp && nt->OptionalHeader.SizeOfImage == image::kSizeOfImage) {
        g_host = HostKind::Bof3;
    } else if (pe && at == exe_layout::kImageBase && stamp == exe_layout::kTimestamp &&
               nt->FileHeader.NumberOfSections > 0 &&
               std::memcmp(IMAGE_FIRST_SECTION(nt)->Name, ".stub\0\0\0", 8) == 0 &&
               IMAGE_FIRST_SECTION(nt)->VirtualAddress == exe_layout::kStubVa - exe_layout::kImageBase) {
        g_host = HostKind::Exe;
    } else {
        Fatal("the main module at %p (timestamp 0x%lX) is neither BOF3.exe (0x%X) nor bof3x.exe (0x%X): every "
              "address would be wrong; refusing to start",
              static_cast<const void*>(base), static_cast<unsigned long>(stamp), (unsigned)image::kTimestamp,
              (unsigned)exe_layout::kTimestamp);
    }
    g_known = true;
    Log("host: %s (main module at 0x%X, timestamp 0x%X)",
        g_host == HostKind::Exe ? "bof3x.exe, our own executable" : "BOF3.exe, the player's own (hosted)", (unsigned)at,
        (unsigned)stamp);
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
