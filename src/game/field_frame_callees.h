// The callees seam of field_frame.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of field_frame.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in field_frame.cpp.
#pragma once

#include <cstdint>

namespace field_frame {

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike, so that each of the twelve is tested alone. The ones this file
// implements are here too, called through the same pointers.
struct CalleesFieldFrame {
    // the entries' calls, in 0x517240's order
    void (__cdecl* mode_dispatch)();
    void (__cdecl* task_records)();
    void (__cdecl* kind2)();
    void (__cdecl* extra_frame)();
    void (__cdecl* members)();
    void (__cdecl* objects)();
    void (__cdecl* area_map)();
    void (__cdecl* extra_screens)();
    void (__cdecl* party_screens)();
    void (__cdecl* objects_screen)();
    void (__cdecl* effects)();
    void (__cdecl* slots)();
    void (__cdecl* tint)();
    void (__cdecl* draw)();
    // Field_MembersFrame's
    void (__cdecl* pending)();
    void (__cdecl* leader)();
    void (__cdecl* member)();
    // the screen updates'
    void (__cdecl* screen_a)();
    void (__cdecl* update_screen)();
    void (__cdecl* project_a)();
    // the object frames'
    unsigned char (__cdecl* turn_target)(unsigned char*);
    void (__cdecl* face)(unsigned char);
    void (__cdecl* idle)(unsigned char*);
    void (__cdecl* linked)();
    void (__cdecl* idle_long)(unsigned char*);
    unsigned char (__cdecl* tick)();
    unsigned char (__cdecl* tick_once)();
    // Sprite_ProjectA's
    void (__cdecl* inherit_key)();
    void (__cdecl* load_vertex)(const unsigned long*);
    void (__cdecl* rtps)();
    void (__cdecl* store_xy)(unsigned long*);
    void (__cdecl* store_depth)(long*);
    // the frame's draw
    void (__cdecl* header_pass)();
    void (__cdecl* overlays)();
    void (__cdecl* draw_pass)();
    void (__cdecl* swap)(unsigned long, unsigned long);
    void (__cdecl* sprite_draw)();
};

extern CalleesFieldFrame g;   // defined in field_frame.cpp (source-layout B0: promoted for a moved reader)
}  // namespace field_frame
