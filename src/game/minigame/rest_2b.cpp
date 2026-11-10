// Group R2B of round fourteen (wave two): 38 functions at 0x56E040..0x57F33C -
// the cut's 39 rows for R2B (analysis/round14_cut.tsv) less three that are
// jump-table cases of hosts already ours (0x551E40, 0x553E50, 0x559AD0), and
// two starts in the band no list had (0x57E720, a state of Shisu_PickStates;
// 0x57EDF0, the draw four states jump to), each read to its last instruction
// with capstone (2026-10-04) and fuzzed through the scenario harness's field
// mode (rest_2b_fuzz.cpp). docs/rest_2b.md has them one row each.
//
// Game mode 8's step 8 (Mode8_Step8 0x517340) calls Shisu_ModeDispatch every
// frame: a screen on the menu block 0x929F00 (mode, state +1, step +2, timer
// +4) whose PSX twins lie in the SHISU overlay. Mode 0 counts four items and
// sets two models up (records 0x9398E0 "A" and 0x939960 "B", laid out as the
// sprite records Sprite_ObjectMatrix reads); 1 opens the windows; 2 picks - a
// side, then up to four counts - and shows the two models turning, dropping
// and lighting up; 3 closes; 4 takes the given items, scores them and hands
// the chapter its step. Each model has its own state table (Shisu_ModelBStates
// here; MasterFigure_States' draws past its entry 1 are R2C's).
//
// Every one is a faithful replacement but Shisu_DrawModel (DIV-0073) and,
// under BOF3X_NO_FAIL, Shisu_Result (DIV-0188: the score raised to the top
// rank's 0xB4 before it is ranked). Where
// the original indexes a .data table by a byte it never bounds (every
// dispatcher here) ours aborts with a message (round9 doc section 6); where
// it reads memory it never wrote (a light matrix half filled,
// Shisu_DrawModel) ours writes zeros: DIV-0073, and the doc's section 7. Every call goes through the harness (SH_CALL / SH_AT),
// so the start-up fuzz can stand recorders in for the callees; Sprite_Current
// and the menu cells are re-read after every call, as the originals re-read
// them.
#include "game/rest_2b.h"

#include <cstdint>
#include <windows.h>
#include <cstring>
#include <type_traits>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/record_views.h"
#include "game/move_script_bytes.h"
#include "game/rest_2b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "game/rdata_consts.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Word;

// --- the cells ------------------------------------------------------------------------

// kRounds: now Rest2b_Rounds (symbols.toml). u8: shows run so far (8 at most); also read as a dword & 0xFF
// kSide: now Rest2b_Side (symbols.toml). u8: the pick screen's 0 / 1 choice
// kCursor: now Rest2b_Cursor (symbols.toml). u8: the count screen's row 0..3 (0xFF backs out)
// kLevel: now Rest2c_FigureScaleIndex (symbols.toml). u8: Shisu_ScaleIndex's answer, 9..0x10
// kScoreA: now Rest2b_ScoreA (symbols.toml). s32 x 5: the score's four terms and the score
// kScoreB: now Rest2b_ScoreB (symbols.toml).
// kScoreC: now Rest2b_ScoreC (symbols.toml).
// kScoreD: now Rest2b_ScoreD (symbols.toml).
// kScore: now Rest2b_Score (symbols.toml).

unsigned char& BRest2b(U address) { return move_script::At(address)[0]; }
template <class T> void PutRest2b(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

}  // namespace

// ===========================================================================
// Effect records, a field object trigger, two menu primitives
// ===========================================================================

// ===========================================================================
// Game mode 8's step 8: the mode dispatcher and the five modes
// ===========================================================================

// ===========================================================================
// The two models' states and their draw
// ===========================================================================

void Rest2B_Inject() {
    if (bof3::WantsShadow("rest_2b")) rest_2b::SelfTest();
}
