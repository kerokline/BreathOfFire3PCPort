#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os::ModuleImage / ModuleExportByOrdinal on POSIX: the ELF program
// headers of the loaded object that holds the address (dl_iterate_phdr). See
// os/module.h.
#include "os/module.h"

#include <link.h>

namespace bof3::os {

namespace {

struct Search {
    std::uintptr_t at;
    ModuleImageInfo* out;
    bool found;
};

int Visit(dl_phdr_info* info, std::size_t, void* data) {
    auto* s = static_cast<Search*>(data);
    bool holds = false;
    std::uintptr_t lo = ~std::uintptr_t{0}, hi = 0;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD) continue;
        const std::uintptr_t a = info->dlpi_addr + p.p_vaddr, b = a + p.p_memsz;
        if (s->at >= a && s->at < b) holds = true;
        if (a < lo) lo = a;
        if (b > hi) hi = b;
    }
    if (!holds) return 0;
    // The load address: the first segment's page, which is where the ELF
    // header lies (a Windows module handle's analogue).
    const std::uintptr_t page = 0x1000;
    s->out->base = lo & ~(page - 1);
    s->out->extent = hi - s->out->base;
    s->out->parts.clear();
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD) continue;
        ModulePart part;
        part.lo = info->dlpi_addr + p.p_vaddr;
        part.hi = part.lo + p.p_memsz;
        part.readable = (p.p_flags & PF_R) != 0;
        part.executable = (p.p_flags & PF_X) != 0;
        s->out->parts.push_back(part);
    }
    s->found = true;
    return 1;
}

}  // namespace

bool ModuleImage(const void* inside, ModuleImageInfo* out) {
    Search s{reinterpret_cast<std::uintptr_t>(inside), out, false};
    dl_iterate_phdr(&Visit, &s);
    return s.found;
}

// ELF numbers no export (os/module.h): no module answers an ordinal.
void* ModuleExportByOrdinal(const char*, unsigned) { return nullptr; }

}  // namespace bof3::os
