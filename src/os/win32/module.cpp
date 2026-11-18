// bof3::os::ModuleImage / ModuleExportByOrdinal on Windows: the module's PE
// headers as the loader mapped them, and GetProcAddress by ordinal. See
// os/module.h.
#include "os/module.h"

#include <windows.h>

namespace bof3::os {

bool ModuleImage(const void* inside, ModuleImageInfo* out) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(inside), &m) ||
        !m)
        return false;
    const auto base = reinterpret_cast<std::uintptr_t>(m);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    // FileHeader and OptionalHeader.SizeOfImage lie at the same offsets in
    // PE32 and PE32+, and IMAGE_FIRST_SECTION steps by SizeOfOptionalHeader,
    // so the native spelling reads either.
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
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

}  // namespace bof3::os
