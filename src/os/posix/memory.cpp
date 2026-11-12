#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os memory on Linux: mmap, mprotect, munmap, /proc/self/maps.
// A MemQuery run is one mapping of /proc/self/maps, not the merged run
// VirtualQuery reports; a PROT_NONE mapping reads as Reserved. See os/memory.h.

#include "os/memory.h"
#include "os/os.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bof3::os {

namespace {

int NativeProt(Prot p) {
    switch (p) {
        case Prot::None: return PROT_NONE;
        case Prot::Read: return PROT_READ;
        case Prot::ReadWrite: return PROT_READ | PROT_WRITE;
        case Prot::ReadExec: return PROT_READ | PROT_EXEC;
        case Prot::ReadWriteExec: return PROT_READ | PROT_WRITE | PROT_EXEC;
    }
    return PROT_NONE;
}

std::size_t PageSize() {
    static const std::size_t n = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    return n;
}

// A fixed mapping at exactly `at`, or null. MAP_FIXED_NOREPLACE refuses a range
// in use; a kernel that predates it takes `at` as a hint, so the result is
// checked and given back if it is elsewhere.
void* MapAt(void* at, std::size_t size, int prot, int extra) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | extra;
    if (at) flags |= MAP_FIXED_NOREPLACE;
    void* p = mmap(at, size, prot, flags, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    if (at && p != at) {
        munmap(p, size);
        errno = EEXIST;
        return nullptr;
    }
    return p;
}

}  // namespace

void* MemReserve(void* at, std::size_t size) { return MapAt(at, size, PROT_NONE, MAP_NORESERVE); }

void* MemAlloc(void* at, std::size_t size, Prot prot) { return MapAt(at, size, NativeProt(prot), 0); }

void* MemCommit(void* at, std::size_t size, Prot prot) {
    if (mprotect(at, size, NativeProt(prot)) == 0) return at;
    if (errno == ENOMEM) return MemAlloc(at, size, prot);   // not mapped: made from nothing, as Windows does
    return nullptr;
}

bool MemDecommit(void* at, std::size_t size) {
    return mmap(at, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != MAP_FAILED;
}

bool MemRelease(void* base, std::size_t size) { return munmap(base, size) == 0; }

bool MemProtect(void* at, std::size_t size, Prot prot, ProtSave* saved) {
    if (saved) {
        MemInfo m;
        if (!MemQuery(at, &m)) return false;
        saved->native = (m.readable ? PROT_READ : 0) | (m.writable ? PROT_WRITE : 0) | (m.executable ? PROT_EXEC : 0);
    }
    return mprotect(at, size, NativeProt(prot)) == 0;
}

bool MemRestore(void* at, std::size_t size, const ProtSave& saved) {
    return mprotect(at, size, static_cast<int>(saved.native)) == 0;
}

void FlushCode(void* at, std::size_t size) {
    __builtin___clear_cache(static_cast<char*>(at), static_cast<char*>(at) + size);
}

bool MemQuery(const void* at, MemInfo* out) {
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return false;
    const auto addr = reinterpret_cast<std::uintptr_t>(at);
    const std::uintptr_t page = PageSize();
    std::uintptr_t prev_end = 0;
    bool found = false, free_run = false;
    char line[1024];
    while (std::fgets(line, sizeof line, maps)) {
        unsigned long long lo = 0, hi = 0, off = 0, inode = 0;
        char perms[8] = {}, dev[16] = {};
        int used = 0;
        if (std::sscanf(line, "%llx-%llx %7s %llx %15s %llu %n", &lo, &hi, perms, &off, dev, &inode, &used) < 6) continue;
        if (addr >= hi) {
            prev_end = hi;
            continue;
        }
        if (addr >= lo) {
            const bool r = perms[0] == 'r', w = perms[1] == 'w', x = perms[2] == 'x';
            out->base = lo;
            out->alloc_base = lo;
            out->size = hi - lo;
            out->state = (r || w || x) ? MemState::Committed : MemState::Reserved;
            out->readable = r;
            out->writable = w;
            out->executable = x;
            out->guard = false;
            const char* name = line + used;
            out->is_private = inode == 0 && (*name == '\n' || *name == 0 || *name == '[');
            found = true;
        } else {
            out->base = std::max<std::uintptr_t>(prev_end, addr & ~(page - 1));
            out->alloc_base = 0;
            out->size = lo - out->base;
            out->state = MemState::Free;
            out->readable = out->writable = out->executable = out->guard = out->is_private = false;
            free_run = true;
        }
        break;
    }
    if (!found && !free_run) {   // past the last mapping
        out->base = std::max<std::uintptr_t>(prev_end, addr & ~(page - 1));
        out->alloc_base = 0;
        out->size = ~static_cast<std::size_t>(0) - out->base + 1;   // to the end of the address space
        out->state = MemState::Free;
        out->readable = out->writable = out->executable = out->guard = out->is_private = false;
    }
    std::fclose(maps);
    return true;
}



}  // namespace bof3::os
