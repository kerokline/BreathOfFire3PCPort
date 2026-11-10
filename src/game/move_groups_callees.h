// The callees seam of move_groups.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of move_groups.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in move_groups.cpp.
#pragma once

#include <cstdint>

namespace move_groups {

// Every callee, through pointers so that the start-up fuzz can stand recording
// functions in for them, for the original's copies and for ours alike. The
// area's handler table (op DE) and its +0x18 script array (op 86) are read
// from memory, as the original reads them.
struct CalleesMoveGroups {
    int (__cdecl* kind)();
    void (__cdecl* face)(unsigned char);
    unsigned char (__cdecl* turn_target)(unsigned char*);   // its result unread by group E
    void (__cdecl* op_e7)(unsigned char);
    unsigned char (__cdecl* op_e9)(unsigned char*, signed char, signed char, unsigned short, unsigned short, unsigned char, unsigned char);
    unsigned char (__cdecl* tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* tint_release)(unsigned char*);
    void (__cdecl* set_animation)(unsigned char);
    unsigned char (__cdecl* animation_bank)(unsigned short);
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_elevation)(int);
    unsigned short (__cdecl* peek)();
    void (__cdecl* play_by_id)(unsigned short);
    void (__cdecl* sound)(unsigned short);
    void (__cdecl* music_play)(unsigned int, int);
    void (__cdecl* fade_out_stop)(int);
    void (__cdecl* fade_in)(int);
    void (__cdecl* fade_out)(int);
    void (__cdecl* op_b8)(unsigned char);
    void (__cdecl* op_db)();
    unsigned char (__cdecl* test_fb)(short, short);
    unsigned char (__cdecl* test_fc)(short, short);
    unsigned char (__cdecl* effect_spawn)(unsigned char, signed char, signed char, short, short);
    unsigned char (__cdecl* effect_spawn_at)(unsigned char, signed char, signed char, long, long, long);
    long* (__cdecl* handle_position)(long*, unsigned char, unsigned char);
    void (__cdecl* op_87)();
    void (__cdecl* op_88)();
    void (__cdecl* load_stream)(unsigned);   // Sound_LoadStream reads the whole dword (save_menu.cpp)
    int (__cdecl* stream_done)();
};

extern CalleesMoveGroups g;   // defined in move_groups.cpp (source-layout B0: promoted for a moved reader)
}  // namespace move_groups
