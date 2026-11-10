// The callees seam of frame_callees.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of frame_callees.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in frame_callees.cpp.
#pragma once

#include <cstdint>

namespace frame_callees {

// Every direct callee, through pointers so that the start-up fuzz can stand
// recording functions in for them. The indirect ones are the two tables,
// Effect_KindHandlers and CameraTurn_States, which the fuzz fills with
// recorders instead.
struct CalleesFrameCallees {
    void (__cdecl* run_slot)(unsigned char);   // 0x455300
    void (__cdecl* update_screen_slot)();      // 0x588F00
    void (__cdecl* update_screen)();           // 0x588F20
    void (__cdecl* release)();                 // 0x589840
};

extern CalleesFrameCallees g;   // defined in frame_callees.cpp (source-layout B0: promoted for a moved reader)
}  // namespace frame_callees
