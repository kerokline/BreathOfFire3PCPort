// World 2's areas 76..82 and 84: the PSX's BIN/WORLD02/AREA076..084.EMI
// compiled into the exe at 0x40EB90..0x40F720 (Area_Descriptors entries
// 76..84; area 83's descriptor names no code). Round ten, group AR2A: the
// band's 50 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2a.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); two indexes past a table abort where the original would jump
// through its neighbour or read past Area_Descriptors - area 79's object
// state and area 77's leap step (the project's rule, round9 doc section 6;
// docs/area_w2a.md section 6). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the callees of the group's own are called the
// same way, so each function is fuzzed alone.
#include "game/area_w2a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
}

// The choice answer, as every choice of the band reads it: a signed byte.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// A message word from a choice's table by the signed answer (unchecked: a
// negative answer reads before the table, a large one past it - in .data).
unsigned MessageAt(std::uint32_t table, signed char answer) {
    return Word(Mem(table + static_cast<std::uint32_t>(static_cast<int>(answer) * 2)));
}

// The band's commonest choice: no new message; answer 0 sets counter 0 to
// `on0`, answer 1 to `on1`, any other nothing.
void CountersByAnswer(signed char answer, unsigned char on0, unsigned char on1) {
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = on0;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = on1;
}
void ChoiceCounter0(unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    CountersByAnswer(answer, on0, on1);
}
// The same with the message word from `table` by the answer.
void ChoiceMessageCounter0(std::uint32_t table, unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(MessageAt(table, answer));
    CountersByAnswer(answer, on0, on1);
}

// A free effect record of kind `kind` (Effect_FindFree; none: nothing), its
// +0 set 1 and +5 the kind; answers the record, or null for none.
unsigned char* SpawnEffectRecord(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    return e;
}

// Camera_ShiftY moved by `by`, MapView_Redraw 2.
void ShiftCamera(int by) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + by);
    MapView_Redraw = 2;
}

}  // namespace

// ===========================================================================
// Area 76 (descriptor 0x60AC78): no choices, handlers or init; one step hook.
// ===========================================================================

// ===========================================================================
// Area 77 (descriptor 0x60BB88; PSX 0x801F6AD0): 23 choices, 21 handlers
// (choice k + 2 is handler k), a cell hook, an object trigger.
// ===========================================================================

// ===========================================================================
// Area 78 (descriptor 0x60CEF0; PSX 0x801F4140): nine choices, one handler
// (choice 8).
// ===========================================================================

// ===========================================================================
// Area 79 (descriptor 0x60EC30; PSX 0x801F4C2C): six choices, one handler,
// whose state table's second entry is the band's.
// ===========================================================================

// ===========================================================================
// Area 80 (descriptor 0x60F308; PSX 0x801F4450): seven choices, four
// handlers (choice k + 3 is handler k), two of them area 49's camera shifts.
// ===========================================================================

// ===========================================================================
// Area 81 (descriptor 0x60FD90; PSX 0x801F37B8): two choices, four handlers
// (the fourth Area80_ResetCameraShift).
// ===========================================================================

// ===========================================================================
// Area 82 (descriptor 0x610870; PSX 0x801F62A4): four handlers.
// ===========================================================================

// ===========================================================================
// Area 84 (descriptor 0x611218; PSX 0x801F3130): one handler. (Area 83's
// descriptor names no code.)
// ===========================================================================

void AreaW2a_Inject() {
    if (bof3::WantsShadow("area_w2a")) area_w2a::SelfTest();
}
