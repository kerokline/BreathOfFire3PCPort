// Group R2F's callees that are not ours yet, by address, and the two patch
// sites inside its bodies (docs/rest_2f.md section 6).
#pragma once

#include <cstdint>

namespace rest_2f {

// R2E's (round fourteen, wave two, merges after R2F): swaps the bytes its two
// pointers name. The field-standard row "0x58BD50" (scenario_harness.cpp,
// FxSwap) stands in for it.
constexpr std::uint32_t kSwapBytes = 0x58BD50;
// R4F's (wave four): the Config screen's own machine, ConfigMenu_Steps[1]'s
// tail jump (docs/menu-screens.md section 1, "Config: step 1 is jmp 0x460CB0").
constexpr std::uint32_t kConfigMachine = 0x460CB0;

// DIV-0059: the Text_DrawAt call of Win2_DrawItemList's title, which
// BattleDraw_Inject re-aims at ListTitle_DrawAt under a Latin overlay
// (battle_draw.cpp kTitleCallA). Ours calls what the site calls.
constexpr std::uint32_t kTitleCall = 0x596D13;
// DIV-0041: the imm32 of MenuSlide_LeftOff170's `mov ecx, -170`, which
// Widescreen_Inject widens (widescreen.cpp kSlides). Ours reads its bound there.
constexpr std::uint32_t kLeftOff170Bound = 0x596926;

}  // namespace rest_2f
