// World 2's areas 90, 91, 92 and 94: the PSX's BIN/WORLD02/AREA090..094.EMI
// compiled into the exe at 0x411F10..0x4135B0 (Area_Descriptors entries
// 90..94; area 93's descriptor names no code). Round ten, group AR2C: the
// band's 45 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); area 91's handler 0 aborts on a state past its six-entry table,
// where the original would jump to itself forever (state 6) or through the
// descriptor's words (docs/area_w2c.md section 6). Every call goes through
// the harness (AH_CALL), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies; the group's own callee (Area91_DrawGlow) is
// called the same way, so each function is fuzzed alone.
#include "game/area_w2c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }
// A message word of a table indexed by the signed answer (unchecked).
unsigned MessageAt(U table, int answer) { return Word(Mem(table + static_cast<U>(answer) * 2u)); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The scene start the choices share: ScriptFlags_Set40, then the movement
// script's four counters 0, the run step byte after MoveScript_Var7 and
// MoveScript_Var7 itself.
void RunStep(unsigned char var7, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kCounter0.v)) = 0;
    B(::bof3::Img(at::kCounter1.v)) = 0;
    B(::bof3::Img(at::kCounter2.v)) = 0;
    B(::bof3::Img(at::kCounter3.v)) = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
    B(::bof3::Img(MoveScript_Var7_addr)) = var7;
}

// ---- area 90 ----

}  // namespace

// ===========================================================================
// Area 90 (descriptor 0x614588; PSX 0x801F40EC)
// ===========================================================================

// ===========================================================================
// Area 91 (descriptor 0x614748; PSX 0x801F3B60)
// ===========================================================================

// ===========================================================================
// Area 92 (descriptor 0x614E78; PSX 0x801F416C)
// ===========================================================================

// ===========================================================================
// Area 94 (descriptor 0x616D70; PSX 0x801F52F8)
// ===========================================================================

void AreaW2c_Inject() {
    if (bof3::WantsShadow("area_w2c")) area_w2c::SelfTest();
}
