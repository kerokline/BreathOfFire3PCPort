// The callees seam of title_states.cpp: the struct and the one `gTitleStates` its functions call through, so that a
// function moved out of title_states.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in title_states.cpp.
#pragma once

#include <cstdint>

namespace title_states {

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them. Five are ours (Title_FadeMusic,
// Title_StopMusic, Title_DrawBadge, Scenario_Start, Scenario_Load), called
// through here all the same so that each function is tested alone.
// Field_ModeDispatch belongs to another file and is called through its
// address.
struct CalleesTitleStates {
    void (__cdecl* dropped)(unsigned char);
    void (__cdecl* load_dat)(int);
    int (__cdecl* load_done)();
    void (__cdecl* sleep)(int);
    void (__cdecl* clut_restore)();
    void (__cdecl* music_play)(unsigned int, int);
    void (__cdecl* transition)(unsigned char);
    void (__cdecl* fade_music)();
    void (__cdecl* stop_music)();
    void (__cdecl* effect_clear)();
    void (__cdecl* scenario_start)(int);
    void (__cdecl* task_create)(int, void*);
    void (__cdecl* music_fade_out)(int);
    void (__cdecl* music_fade_stop)(int);
    void (__cdecl* badge)();
    void (__cdecl* task_exit)();
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned char* (__cdecl* sprite)(int, int, unsigned, unsigned, unsigned);
    void (__cdecl* shade)(unsigned char*, unsigned char);
    void (__cdecl* scenario_load)();
    void (__cdecl* loading_frame)();
    void (__cdecl* mode_dispatch)();
};

extern CalleesTitleStates gTitleStates;   // defined in title_states.cpp (source-layout B0: promoted for a moved reader)
}  // namespace title_states
