// The callees seam of object_kinds.cpp: the struct and the one `gObjectKinds` its functions call through, so that a
// function moved out of object_kinds.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in object_kinds.cpp.
#pragma once

#include <cstdint>

#include "common/slot32.h"

namespace object_kinds {

using PaceFn = unsigned char (__cdecl*)(unsigned char*, unsigned, unsigned);
// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Ours among them (the pace entries, the turn, the three helpers) are
// tested alone.
struct CalleesObjectKinds {
    const ::bof3::CodeSlot<PaceFn>* pace;   // the table itself (32-bit code words), read at the call as the original reads it
    PaceFn pace0;         // Field_ObjectIdleLong's direct call of entry 0
    int (__cdecl* rand)();
    unsigned char (__cdecl* random_turn)(unsigned char*);
    unsigned char (__cdecl* open_direction)(unsigned char*);
    unsigned char (__cdecl* in_home)(unsigned char*);
    void (__cdecl* turn)(unsigned char*);
    void (__cdecl* settle)(unsigned char*, unsigned char);
    void (__cdecl* motion)(unsigned char*);
    unsigned char (__cdecl* flee)(unsigned char*);
    unsigned char (__cdecl* chase)(unsigned char*);
    unsigned char (__cdecl* best)(unsigned char*, unsigned char);
    unsigned char (__cdecl* blocked_ahead)(unsigned char*);
    void (__cdecl* move)(unsigned char*, unsigned char);
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* tint_release)(unsigned char);
    void (__cdecl* area_handler)(unsigned short);
    void (__cdecl* trigger)(unsigned char*);
    void (__cdecl* event_script)(const unsigned char*);
    void (__cdecl* msg_script)(unsigned short);   // Msg_OpenScript reads a word
    void (__cdecl* msg_system)(unsigned);         // Msg_OpenSystem a dword
};

extern CalleesObjectKinds gObjectKinds;   // defined in object_kinds.cpp (source-layout B0: promoted for a moved reader)
}  // namespace object_kinds
