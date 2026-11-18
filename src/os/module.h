// bof3::os, a loaded module (a DLL, a shared object, the program itself) as
// the loader mapped it: its extent and the parts a pointer can name, and an
// export of a module by its ordinal. The state hash reads the first to tell a
// pointer of ours from game data (hook/statehash.cpp); the WAV dump the second,
// to know DSOUND.dll's DirectSoundCreate (hook/wav_dump.cpp).
//
// Windows: the PE headers in memory (the module's MZ and PE headers, its
// section table) and GetModuleHandleA + GetProcAddress. POSIX: the ELF program
// headers through dl_iterate_phdr; ELF has no export ordinals (below).
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bof3::os {

// A part of a module the loader keeps mapped. Windows: a section that is not
// discardable (IMAGE_SCN_MEM_DISCARDABLE: .reloc, the DWARF sections), its
// size the VirtualSize or, where that is 0, the SizeOfRawData. POSIX: a
// PT_LOAD segment, its size p_memsz.
struct ModulePart {
    std::uintptr_t lo = 0, hi = 0;   // [lo, hi)
    bool readable = false;           // IMAGE_SCN_MEM_READ / PF_R
    bool executable = false;         // IMAGE_SCN_MEM_EXECUTE / PF_X
};

struct ModuleImageInfo {
    std::uintptr_t base = 0;         // the load address (a Windows module handle is this)
    std::size_t extent = 0;          // the image's length from `base`: SizeOfImage / the last segment's end
    std::vector<ModulePart> parts;   // in the headers' order
};

// The image of the module that holds the address `inside` (its own base will
// do). False when no module holds it or its headers do not read as one.
bool ModuleImage(const void* inside, ModuleImageInfo* out);

// What the loaded module `name` exports under `ordinal` (GetProcAddress by
// ordinal). Null when no module of that name is loaded or it exports nothing
// under that number. On POSIX always null: an ELF object names its exports
// and numbers none, so no export is found by a number there - a caller that
// asks for one (wav_dump.cpp, for DSOUND.dll's #1) is asking about a Windows
// module and answers itself from the null.
void* ModuleExportByOrdinal(const char* name, unsigned ordinal);

}  // namespace bof3::os
