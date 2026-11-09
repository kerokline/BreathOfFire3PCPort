// The callees seam of mode_tasks.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of mode_tasks.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in mode_tasks.cpp.
#pragma once

#include <cstdint>

namespace mode_tasks {

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Four of them are this file's (Title_CheckStart, Title_DrawBackdrop,
// Title_DrawLogo and Title_Sprite), called through here all the same so that
// each function is tested alone; Title_FadeMusic, Title_DrawSetA/B/C,
// Field_ModeDispatch and Field_Frame are ours in other files (title_states,
// field_modes, field_frame); Gpu_SetSprt, Gpu_SetSemiTrans, Gpu_SetDrawMode
// and Gfx_CommitPrim are other files'.
struct Callees {
    void (__cdecl* clear_private)();
    void (__cdecl* sleep)(int);
    void (__cdecl* check_start)();
    void (__cdecl* backdrop)();
    void (__cdecl* logo)();
    int (__cdecl* load_done)();
    void (__cdecl* transition)(unsigned char);
    void (__cdecl* fade_music)();
    void (__cdecl* effect)(unsigned short);
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned char* (__cdecl* sprite)(int, int, unsigned, unsigned, unsigned);
    void (__cdecl* set_sprt)(unsigned char*);
    void (__cdecl* semi_trans)(unsigned char*, unsigned);
    void (__cdecl* set_a)(unsigned, unsigned);
    void (__cdecl* set_b)(int, int, unsigned, unsigned);
    void (__cdecl* set_c)(int, int, unsigned, unsigned);
    void (__cdecl* event_dispatch)();
    void (__cdecl* field_frame)();
    void (__cdecl* wait_transition)(unsigned char);
    void (__cdecl* music_fade)(int);
    void (__cdecl* music_stop)(int);
};

extern Callees g;   // defined in mode_tasks.cpp (source-layout B0: promoted for a moved reader)
}  // namespace mode_tasks
