// bof3::os::ModuleImage / ModuleExportByOrdinal / ModuleBuildId on Windows:
// the module's PE headers as the loader mapped them (its section table, its
// debug directory's CodeView record), and GetProcAddress by ordinal. See
// os/module.h.
#include "os/module.h"

#include <cstdio>
#include <cstring>

#include <windows.h>

namespace bof3::os {

namespace {

// The PE headers of the module that holds `inside`, and its base; null when
// no module holds it or its headers do not read as a PE's.
const IMAGE_NT_HEADERS* Headers(const void* inside, std::uintptr_t* base_out) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(inside), &m) ||
        !m)
        return nullptr;
    const auto base = reinterpret_cast<std::uintptr_t>(m);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    *base_out = base;
    return nt;
}

}  // namespace

bool ModuleImage(const void* inside, ModuleImageInfo* out) {
    std::uintptr_t base = 0;
    // FileHeader and OptionalHeader.SizeOfImage lie at the same offsets in
    // PE32 and PE32+, and IMAGE_FIRST_SECTION steps by SizeOfOptionalHeader,
    // so the native spelling reads either.
    const IMAGE_NT_HEADERS* nt = Headers(inside, &base);
    if (!nt) return false;
    out->base = base;
    out->extent = nt->OptionalHeader.SizeOfImage;
    out->parts.clear();
    const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
        if (s->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) continue;
        const std::uint32_t size = s->Misc.VirtualSize ? s->Misc.VirtualSize : s->SizeOfRawData;
        ModulePart p;
        p.lo = base + s->VirtualAddress;
        p.hi = p.lo + size;
        p.readable = (s->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        p.executable = (s->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        out->parts.push_back(p);
    }
    return true;
}

void* ModuleExportByOrdinal(const char* name, unsigned ordinal) {
    HMODULE m = GetModuleHandleA(name);
    return m ? reinterpret_cast<void*>(GetProcAddress(m, MAKEINTRESOURCEA(ordinal))) : nullptr;
}

// A PE's build id is its CodeView record (os/module.h): the debug directory
// (IMAGE_DIRECTORY_ENTRY_DEBUG) names an IMAGE_DEBUG_TYPE_CODEVIEW entry whose
// data, mapped at AddressOfRawData, is "RSDS", a 16-byte GUID, a 32-bit age
// and the PDB's name. lld writes one for every bof3x.dll the presets link
// (the section .buildid), and llvm-strip --strip-debug keeps it as it was.
bool ModuleBuildId(const void* inside, std::string* hex) {
    std::uintptr_t base = 0;
    const IMAGE_NT_HEADERS* nt = Headers(inside, &base);
    // The module is in this process, so it is of this process's width: the
    // native optional header, whose DataDirectory lies where this build's
    // IMAGE_NT_HEADERS puts it.
    if (!nt || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) return false;
    const std::uint32_t extent = nt->OptionalHeader.SizeOfImage;
    if (nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG) return false;
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!dir.VirtualAddress || dir.VirtualAddress >= extent || dir.Size > extent - dir.VirtualAddress) return false;
    const auto* e = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + dir.VirtualAddress);
    for (std::size_t i = 0; i < dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i, ++e) {
        // "RSDS" (4), the GUID (16), the age (4): 24 bytes before the name.
        constexpr std::uint32_t kRecord = 24;
        if (e->Type != IMAGE_DEBUG_TYPE_CODEVIEW || !e->AddressOfRawData || e->SizeOfData < kRecord) continue;
        if (e->AddressOfRawData >= extent || e->SizeOfData > extent - e->AddressOfRawData) continue;
        const auto* d = reinterpret_cast<const unsigned char*>(base + e->AddressOfRawData);
        if (d[0] != 'R' || d[1] != 'S' || d[2] != 'D' || d[3] != 'S') continue;
        // The GUID as llvm-readobj --coff-debug-directory and a symbol store
        // write it - its 32-bit and two 16-bit fields as numbers (stored
        // little-endian), then its last eight bytes in order - and the age
        // after it: a symbol store's key for the file, without the dashes.
        std::uint32_t data1, age;
        std::uint16_t data2, data3;
        std::memcpy(&data1, d + 4, 4);
        std::memcpy(&data2, d + 8, 2);
        std::memcpy(&data3, d + 10, 2);
        std::memcpy(&age, d + 20, 4);
        char buf[64];
        int n = std::snprintf(buf, sizeof buf, "%08x%04x%04x", static_cast<unsigned>(data1), static_cast<unsigned>(data2),
                              static_cast<unsigned>(data3));
        for (int i = 12; i < 20; ++i) n += std::snprintf(buf + n, sizeof buf - n, "%02x", static_cast<unsigned>(d[i]));
        std::snprintf(buf + n, sizeof buf - n, "%x", static_cast<unsigned>(age));
        *hex = buf;
        return true;
    }
    return false;
}

}  // namespace bof3::os
