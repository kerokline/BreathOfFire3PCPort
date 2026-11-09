// Moved here by tools/move_funcs.py from scena_sc15.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_18.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc15.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

using namespace scena_sc15;

}  // namespace

#define SC15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using std::uint32_t;

using VoidFn = void (__cdecl*)();

using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);

unsigned char* AtScenaSc15(uint32_t a) { return move_script::At(a); }

uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(AtScenaSc15(a))); }

uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }

// A handler out of one of the chapters' .data tables, read in place and
// unchecked as the original reads it; where the original would jump to what
// is not code (an index past its table), ours aborts. During the fuzz the
// tables hold the harness's recorders, outside .text.
uint32_t CodeAt(uint32_t table, int index, const char* who) {
    const uint32_t at = table + 4u * static_cast<uint32_t>(index);
    const uint32_t entry = L(at);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %d reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)at, (unsigned)::bof3::VaOf(at));
    return entry;
}

VoidFn Handler(uint32_t table, int index, const char* who) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(CodeAt(table, index, who)));
}

// An object hook's handler, called (object, the flag row) as 0x56D6D0's slot 1
// calls through a chapter's object table.
void CallObject(uint32_t table, unsigned char* object, const char* who) {
    unsigned char* const row = AtScenaSc15(Chapter_FlagRow);
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(CodeAt(table, object[0x86], who)))(object, row);
}

}  // namespace

// original 0x56D510: chapter 18's vtable slot 0 (PSX 0x801F6C04): a tail jump
// through Scena18_States - 0 is 0x5646B0 (state 1), 1 Scena18_EnterArea, 2
// Scena18_Run. Unchecked.
SC15_EXPORT void __cdecl Scena18_Frame(void) {
    Handler(::bof3::Img(Scena18_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]), "Scena18_Frame")();
}

// original 0x56D520: chapter 18's state 2: a tail jump through Scena18_Runs
// on the s8 MoveScript_Var7 (one entry, a bare ret). Unchecked.
SC15_EXPORT void __cdecl Scena18_Run(void) {
    Handler(::bof3::Img(Scena18_Runs_addr), MoveScript_Var7, "Scena18_Run")();
}

// original 0x56D530: chapter 18's vtable slot 1 (PSX 0x801F6CAC):
// Scena18_Objects[object +0x86] (object, the flag row); one entry, a bare ret.
SC15_EXPORT void __cdecl Scena18_ObjectTrigger(unsigned char* object) {
    CallObject(::bof3::Img(Scena18_Objects_addr), object, "Scena18_ObjectTrigger");
}

void Scena18_Inject() {
    BOF3_INJECT(Scena18_Frame);
    BOF3_INJECT(Scena18_Run);
    BOF3_INJECT(Scena18_ObjectTrigger);
}
