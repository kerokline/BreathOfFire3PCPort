// bof3::os memory on Windows: VirtualAlloc, VirtualProtect, VirtualFree,
// VirtualQuery. See os/memory.h.

#include "os/memory.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

namespace {

DWORD Native(Prot p) {
    switch (p) {
        case Prot::None: return PAGE_NOACCESS;
        case Prot::Read: return PAGE_READONLY;
        case Prot::ReadWrite: return PAGE_READWRITE;
        case Prot::ReadExec: return PAGE_EXECUTE_READ;
        case Prot::ReadWriteExec: return PAGE_EXECUTE_READWRITE;
    }
    return PAGE_NOACCESS;
}

constexpr DWORD kWritable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
constexpr DWORD kExecutable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

}  // namespace

void* MemReserve(void* at, std::size_t size) { return VirtualAlloc(at, size, MEM_RESERVE, PAGE_NOACCESS); }
void* MemCommit(void* at, std::size_t size, Prot prot) { return VirtualAlloc(at, size, MEM_COMMIT, Native(prot)); }
void* MemAlloc(void* at, std::size_t size, Prot prot) {
    return VirtualAlloc(at, size, MEM_RESERVE | MEM_COMMIT, Native(prot));
}
bool MemDecommit(void* at, std::size_t size) { return VirtualFree(at, size, MEM_DECOMMIT) != 0; }
bool MemRelease(void* base, std::size_t) { return VirtualFree(base, 0, MEM_RELEASE) != 0; }
bool MemProtect(void* at, std::size_t size, Prot prot, ProtSave* saved) {
    DWORD old = 0;
    if (!VirtualProtect(at, size, Native(prot), &old)) return false;
    if (saved) saved->native = old;
    return true;
}
bool MemRestore(void* at, std::size_t size, const ProtSave& saved) {
    DWORD ignored = 0;
    return VirtualProtect(at, size, saved.native, &ignored) != 0;
}

void FlushCode(void* at, std::size_t size) { FlushInstructionCache(GetCurrentProcess(), at, size); }

bool MemQuery(const void* at, MemInfo* out) {
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery(at, &m, sizeof m)) return false;
    out->base = reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    out->alloc_base = reinterpret_cast<std::uintptr_t>(m.AllocationBase);
    out->size = m.RegionSize;
    out->state = m.State == MEM_COMMIT ? MemState::Committed : m.State == MEM_RESERVE ? MemState::Reserved : MemState::Free;
    out->guard = (m.Protect & PAGE_GUARD) != 0;
    const bool open = m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD));
    out->readable = open && (m.Protect & kReadable) != 0;
    out->writable = (m.Protect & kWritable) != 0;
    out->executable = (m.Protect & kExecutable) != 0;
    out->is_private = m.Type == MEM_PRIVATE;
    return true;
}



}  // namespace bof3::os
