// The callees seam of kind2_object.cpp: the struct and the one `gKind2Object` its functions call through, so that a
// function moved out of kind2_object.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in kind2_object.cpp.
#pragma once

#include <cstdint>

#include "common/slot32.h"

namespace kind2_object {

using HandlerKind2Object = void (__cdecl*)();
// Every call out, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. The two tables are the original's own, in BOF3.exe's .data: the
// dispatchers index them with a whole byte, as the original does, so a run
// byte or a state out of range reaches exactly what it reached in 2001 (see
// Kind2_Run). Their entries are the original addresses, which Inject turns
// into jumps to ours; entry 0 of the run table, 0x437CC0, is Capcom's bare ret.
struct CalleesKind2Object {
    const ::bof3::CodeSlot<HandlerKind2Object>* run_table;     // 32-bit code words in the image
    const ::bof3::CodeSlot<HandlerKind2Object>* state_table;
    unsigned char (__cdecl* step)(unsigned char*, const unsigned char*);
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_elevation)(int);
    void (__cdecl* view_reset)();
    void (__cdecl* script)();   // Kind2_Move's tail jump
};

extern CalleesKind2Object gKind2Object;   // defined in kind2_object.cpp (source-layout B0: promoted for a moved reader)
}  // namespace kind2_object
