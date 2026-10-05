// Group R2H of round fourteen (wave two): 36 functions at 0x59AA80..0x5A9874 -
// the cut's 34 rows for R2H (analysis/round14_cut.tsv), the start in their span
// no list had (0x59C810) and one start the catalogue filed as the renderer's
// (0x59E160, a menu draw only 0x59CCE0 calls). docs/rest_2h.md.
//
// Window kinds: record handler 7's kinds 11..18 (Window_Handler7KindTable),
// handler 8's kinds 3..5 (Window_Handler8KindTable) and MenuList_Kinds[20],
// with their slides and draws - the shared ability list, the row menu, the
// masters' list, caption and pupils, the battle equipment window's item list;
// and three strays: the battle equipment window's stat bar, DirectInput's
// joystick enumeration callback and Cfg_Load's key-table copy.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

#include <cstdint>

void Rest2H_Inject();

namespace rest_2h {
// Where MasterWin_SlideOut reads its bound: the imm32 at 0x59C136 (DIV-0041
// widens it); the fuzz aims it at its own copy's immediate.
extern std::uint32_t g_master_bound;
// BOF3X_SHADOW=rest_2h: the start-up fuzz, rest_2h_fuzz.cpp. Clones the 36
// originals before Rest2H_Inject patches them.
void SelfTest();
}  // namespace rest_2h
