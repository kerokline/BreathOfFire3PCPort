// The callees seam of move_script.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of move_script.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in move_script.cpp.
#pragma once

#include <cstdint>

namespace move_script {

using GroupFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*, unsigned char);
// Every callee that is not one of these three, through pointers so that the
// start-up fuzz can stand recording functions in for them - for the
// original's copy and for ours alike. The two tables the flow pass calls
// through (the area's handlers, MoveScript_CounterOps) are read from memory,
// as the original reads them; the fuzz swaps their contents instead.
struct CalleesMoveScript {
    int (__cdecl* kind)();
    void (__cdecl* move)(unsigned char*, unsigned char);
    void (__cdecl* move_kind2)(unsigned char);
    GroupFn group6, group8, group9, group_c, group_d, group_e;
    void (__cdecl* group_b)(unsigned char*, const unsigned char*);
    unsigned short (__cdecl* find_label)(unsigned char*, unsigned char, const unsigned char*);
    int (__cdecl* variable)(unsigned char);
    void (__cdecl* trio_clear)();
    void (__cdecl* trio_set)();
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_animation)(unsigned char);
    void (__cdecl* set_animation_at)(unsigned char, unsigned short);
    void (__cdecl* flags_clear)();
    void (__cdecl* flags_set)();
    int (__cdecl* wait_test)(unsigned char, unsigned char);
    unsigned char (__cdecl* tick_once)();
    unsigned short (__cdecl* peek)();
    void (__cdecl* sound)(unsigned short);
    void (__cdecl* op_f7)(long, long, unsigned char, unsigned char*);
    void (__cdecl* attach)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    unsigned char (__cdecl* test_fb)(short, short);
    unsigned char (__cdecl* test_fc)(short, short);
};

extern CalleesMoveScript g;   // defined in move_script.cpp (source-layout B0: promoted for a moved reader)
}  // namespace move_script
