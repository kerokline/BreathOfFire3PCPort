// Memory: address space at a fixed address, commit, protect, release, query.
//
// The narrow image and its pools live at addresses the original binary fixes
// (0x400000 and up), so a call that takes `at` gets exactly that address or
// fails with a null or false return, and never moves it; LastError() (env.h)
// then holds the system's number. Nothing in the standard library reserves,
// commits or protects pages, so this is the one service all of which stays
// here. The heap is std::malloc / std::calloc / std::free.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).

#pragma once

#include <cstddef>
#include <cstdint>

namespace bof3::os {

// Windows: VirtualAlloc, VirtualProtect, VirtualFree, VirtualQuery. POSIX: mmap,
// mprotect, munmap, /proc/self/maps. `at` null: anywhere.

enum class Prot : std::uint8_t { None = 0, Read = 1, ReadWrite = 3, ReadExec = 5, ReadWriteExec = 7 };

// What pages had before a MemProtect, to put back by MemRestore exactly (on
// Windows the native protection word, copy-on-write flavours included).
struct ProtSave {
    std::uint32_t native = 0;
};

// Address space with no access (MEM_RESERVE / PROT_NONE).
void* MemReserve(void* at, std::size_t size);
// Pages made usable inside a reservation (MEM_COMMIT / mprotect), or, where
// the range is free on Windows, made from nothing. Returns `at`, or null.
void* MemCommit(void* at, std::size_t size, Prot prot);
// Reserve and commit in one call (MEM_RESERVE | MEM_COMMIT / mmap). Returns
// the base, or null.
void* MemAlloc(void* at, std::size_t size, Prot prot);
// Pages back to reserved, contents gone (MEM_DECOMMIT).
bool MemDecommit(void* at, std::size_t size);
// A whole MemReserve / MemAlloc block given back. `base` is what the call
// returned; `size` is what it was given (Windows ignores it, munmap needs it).
bool MemRelease(void* base, std::size_t size);
// Change the protection of whole pages. `saved`, if given, receives what the
// first page had.
bool MemProtect(void* at, std::size_t size, Prot prot, ProtSave* saved = nullptr);
bool MemRestore(void* at, std::size_t size, const ProtSave& saved);

// Code written into memory made visible to the instruction stream (a no-op on
// x86; FlushInstructionCache / __builtin___clear_cache).
void FlushCode(void* at, std::size_t size);

enum class MemState : std::uint8_t { Free, Reserved, Committed };

// The run of pages that share state and protection around an address.
struct MemInfo {
    std::uintptr_t base = 0;        // the run's first byte
    std::uintptr_t alloc_base = 0;  // the block it was made in
    std::size_t size = 0;           // the run's length
    MemState state = MemState::Free;
    bool readable = false;          // committed, and not no-access or a guard page
    bool writable = false;          // includes copy-on-write
    bool executable = false;
    bool guard = false;             // a page that faults once (a stack's)
    bool is_private = false;        // not a mapped image or file
};
bool MemQuery(const void* at, MemInfo* out);


}  // namespace bof3::os
