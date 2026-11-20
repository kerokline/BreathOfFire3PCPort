// The player's BOF3.exe read as a file (exe_file.h), and ExeFile_Present /
// ExeFile_Read (exe_host.h) over it. Portable: the PE headers are read as the
// bytes the format fixes, through a C stream the seam opens
// (os::OpenPath), so the same code serves the Windows exe host and a Linux
// start-up (docs/linux-round.md, "R3: what a Linux start-up needs from the PE
// host"). The messages are the ones exe_host.cpp gave when it read the file
// through a Win32 handle.
#include "hook/exe_file.h"

#include <cstdio>
#include <cstring>

#include "common/image_base.h"
#include "hook/exe_host.h"
#include "hook/host.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3::exe_file {
namespace {

std::FILE* g_exe = nullptr;   // the player's BOF3.exe, read-only, for the process's life; null: no file
Section g_text, g_rdata, g_data;   // its sections, when it is there

// The PE format's fixed offsets (Microsoft's PE/COFF specification; the
// fields IMAGE_DOS_HEADER, IMAGE_NT_HEADERS32 and IMAGE_SECTION_HEADER name
// in <winnt.h>).
constexpr std::uint32_t kDosHeaderBytes = 64;      // sizeof(IMAGE_DOS_HEADER)
constexpr std::uint32_t kDosMagicAt = 0x00;        // e_magic, "MZ"
constexpr std::uint32_t kDosLfanewAt = 0x3C;       // e_lfanew
constexpr std::uint16_t kDosMagic = 0x5A4D;        // IMAGE_DOS_SIGNATURE
constexpr std::uint32_t kNtHeaderBytes = 248;      // sizeof(IMAGE_NT_HEADERS32)
constexpr std::uint32_t kNtSignature = 0x00004550; // IMAGE_NT_SIGNATURE, "PE\0\0"
constexpr std::uint32_t kNtSectionsAt = 4 + 2;     // FileHeader.NumberOfSections
constexpr std::uint32_t kNtOptSizeAt = 4 + 16;     // FileHeader.SizeOfOptionalHeader
constexpr std::uint32_t kNtOptionalAt = 4 + 20;    // offsetof(IMAGE_NT_HEADERS32, OptionalHeader)
constexpr std::uint32_t kNtImageBaseAt = kNtOptionalAt + 28;   // OptionalHeader.ImageBase (PE32)
constexpr std::uint32_t kSectionBytes = 40;        // sizeof(IMAGE_SECTION_HEADER)

std::uint16_t U16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | p[1] << 8); }
std::uint32_t U32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

Section Find(const char* want) {
    std::uint8_t dos[kDosHeaderBytes];
    std::uint8_t nt[kNtHeaderBytes];
    ReadAt(0, dos, sizeof dos, "the DOS header");
    const std::uint32_t lfanew = U32(dos + kDosLfanewAt);
    ReadAt(lfanew, nt, sizeof nt, "the PE header");
    if (U16(dos + kDosMagicAt) != kDosMagic || U32(nt) != kNtSignature || U32(nt + kNtImageBaseAt) != image::kBase)
        Fatal("data: BOF3.exe is not a 32-bit PE based at 0x%X", (unsigned)image::kBase);
    const std::uint32_t first = lfanew + kNtOptionalAt + U16(nt + kNtOptSizeAt);
    const unsigned count = U16(nt + kNtSectionsAt);
    for (unsigned i = 0; i < count; ++i) {
        std::uint8_t s[kSectionBytes];
        ReadAt(first + i * kSectionBytes, s, sizeof s, "the section table");
        char name[9] = {};
        std::memcpy(name, s, 8);
        if (std::strcmp(name, want) != 0) continue;
        // Name[8], then VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData.
        return {image::kBase + U32(s + 12), U32(s + 8), U32(s + 20), U32(s + 16)};
    }
    Fatal("data: BOF3.exe has no %s section", want);
}

}  // namespace

bool Open(const std::filesystem::path& path) {
    if (g_exe) Fatal("data: BOF3.exe is opened twice");
    g_exe = os::OpenPath(path, "rb");
    return g_exe != nullptr;
}

bool IsOpen() { return g_exe != nullptr; }

void ReadSections() {
    g_text = Find(".text");
    g_rdata = Find(".rdata");
    g_data = Find(".data");
}

const Section& Text() { return g_text; }
const Section& Rdata() { return g_rdata; }
const Section& Data() { return g_data; }

void ReadAt(std::uint32_t offset, void* to, std::uint32_t n, const char* what) {
    // A file offset the game's file never passes 2 GB: fseek's long holds it
    // on both systems.
    if (!g_exe || offset > 0x7FFFFFFFu || std::fseek(g_exe, static_cast<long>(offset), SEEK_SET) != 0 ||
        std::fread(to, 1, n, g_exe) != n)
        Fatal("data: cannot read %s (0x%X bytes at file offset 0x%X) from BOF3.exe, error %u", what, (unsigned)n,
              (unsigned)offset, (unsigned)os::LastError());
}

}  // namespace bof3::exe_file

namespace bof3 {

bool ExeFile_Present() { return !IsExeHost() || exe_file::IsOpen(); }

void ExeFile_Read(std::uint32_t va, void* to, std::uint32_t n) {
    if (!IsExeHost()) Fatal("ExeFile_Read(0x%08X) in the hosted build: Capcom's image is mapped, read it there", (unsigned)va);
    if (!exe_file::IsOpen()) Fatal("ExeFile_Read(0x%08X): there is no BOF3.exe (ask ExeFile_Present first)", (unsigned)va);
    for (const exe_file::Section* s : {&exe_file::Text(), &exe_file::Rdata(), &exe_file::Data()})
        if (va >= s->va && va + n <= s->va + s->raw_size && va + n >= va) {
            exe_file::ReadAt(s->raw + (va - s->va), to, n, "a verification's bytes");
            return;
        }
    Fatal("ExeFile_Read(0x%08X + %u): not inside the initialised bytes of BOF3.exe's .text, .rdata or .data", (unsigned)va,
          (unsigned)n);
}

}  // namespace bof3
