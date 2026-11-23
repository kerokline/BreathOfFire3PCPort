#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os::ModuleImage / ModuleExportByOrdinal / ModuleBuildId on POSIX: the ELF program
// headers of the loaded object that holds the address (dl_iterate_phdr). See
// os/module.h.
#include "os/module.h"

#include <link.h>

#include <cstring>

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

namespace {

struct NoteSearch {
    std::uintptr_t at;
    std::string* hex;
    bool found;
};

int VisitNotes(dl_phdr_info* info, std::size_t, void* data) {
    auto* s = static_cast<NoteSearch*>(data);
    bool holds = false;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type == PT_LOAD && s->at >= info->dlpi_addr + p.p_vaddr && s->at < info->dlpi_addr + p.p_vaddr + p.p_memsz)
            holds = true;
    }
    if (!holds) return 0;
    for (unsigned i = 0; i < info->dlpi_phnum && !s->found; ++i) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_NOTE) continue;
        const auto* at = reinterpret_cast<const unsigned char*>(info->dlpi_addr + p.p_vaddr);
        const unsigned char* const end = at + p.p_memsz;
        // Each note: a header, the name and the description, each padded to 4 bytes.
        while (at + sizeof(ElfW(Nhdr)) <= end) {
            ElfW(Nhdr) note;
            std::memcpy(&note, at, sizeof note);
            const unsigned char* const name = at + sizeof note;
            const unsigned char* const desc = name + ((note.n_namesz + 3u) & ~3u);
            if (desc > end || note.n_descsz > static_cast<std::size_t>(end - desc)) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && std::memcmp(name, "GNU", 4) == 0) {
                static const char kHex[] = "0123456789abcdef";
                s->hex->clear();
                for (unsigned k = 0; k < note.n_descsz; ++k) {
                    s->hex->push_back(kHex[desc[k] >> 4]);
                    s->hex->push_back(kHex[desc[k] & 15]);
                }
                s->found = note.n_descsz != 0;
                break;
            }
            at = desc + ((note.n_descsz + 3u) & ~3u);
        }
    }
    return 1;
}

}  // namespace

bool ModuleBuildId(const void* inside, std::string* hex) {
    NoteSearch s{reinterpret_cast<std::uintptr_t>(inside), hex, false};
    dl_iterate_phdr(&VisitNotes, &s);
    return s.found;
}

// ELF numbers no export (os/module.h): no module answers an ordinal.
void* ModuleExportByOrdinal(const char*, unsigned) { return nullptr; }

}  // namespace bof3::os
