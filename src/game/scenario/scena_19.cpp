// Moved here by tools/move_funcs.py from scena_sc15.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_19.h"

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

unsigned char& B(uint32_t a) { return AtScenaSc15(a)[0]; }

uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(AtScenaSc15(a))); }

uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }

constexpr uint32_t kInputPressed = 0x7E1BEC;   // Input_Pressed

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

unsigned char* Effect(unsigned slot) { return AtScenaSc15(::bof3::Img(Effect_Objects_addr) + ((slot & 0xFFu) << 7)); }

// Effect_FindFree; with a slot, its record live (+0 = 1) and of `kind` (+5).
unsigned char NewEffect(unsigned char kind) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = kind;
    }
    return s;
}

}  // namespace

// original 0x56D550: chapter 19's vtable slot 0 (PSX 0x801F6C04 in SCENA19):
// a tail jump through Scena19_States - 0 is 0x5646B0, 1 Scena18_EnterArea, 2
// Scena19_Run. Unchecked.
SC15_EXPORT void __cdecl Scena19_Frame(void) {
    Handler(::bof3::Img(Scena19_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]), "Scena19_Frame")();
}

// original 0x56D560: chapters 18's and 19's state 1: state 2.
SC15_EXPORT void __cdecl Scena18_EnterArea(void) { MoveScript_PartyRecords[0x62] = 2; }

// original 0x56D570: chapter 19's state 2: a tail jump through Scena19_Runs on
// the s8 MoveScript_Var7 (one entry, Scena19_Run0). Unchecked.
SC15_EXPORT void __cdecl Scena19_Run(void) {
    Handler(::bof3::Img(Scena19_Runs_addr), MoveScript_Var7, "Scena19_Run")();
}

// original 0x56D580: chapter 19's run 0: on Input_Pressed bit 8, an effect of
// kind 0x2F.
SC15_EXPORT void __cdecl Scena19_Run0(void) {
    if ((B(::bof3::Img(kInputPressed)) & 8) == 0) return;
    NewEffect(0x2F);
}

// original 0x56D5C0: chapter 19's vtable slot 1 (PSX 0x801F6D10):
// Scena19_Objects[object +0x86] (object, the flag row); one entry, a bare ret.
SC15_EXPORT void __cdecl Scena19_ObjectTrigger(unsigned char* object) {
    CallObject(::bof3::Img(Scena19_Objects_addr), object, "Scena19_ObjectTrigger");
}

void Scena19_Inject() {
    BOF3_INJECT(Scena19_Frame);
    BOF3_INJECT(Scena18_EnterArea);
    BOF3_INJECT(Scena19_Run);
    BOF3_INJECT(Scena19_Run0);
    BOF3_INJECT(Scena19_ObjectTrigger);
}
