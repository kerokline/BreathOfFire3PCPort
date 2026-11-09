// Chapter 9's tail and chapter 10 of the scenario code, 0x557170..0x55C040
// (round ten group SC9b). docs/scena_sc9b.md.
//
// Chapter 9's tail (0x557170..0x558140), reached only through tables:
//   - eight Scena09_Objects entries (7..11, 13..15; group SC9a's table, read
//     there in place): a flag or a run started;
//   - vtable 0x6613E8 slot 2 Scena09_StepHook (x, z): by the area, the flags
//     and a rectangle of the position, a run started, al 1; else 0;
//   - slot 4 Scena09_CellHook (x, z): 0x56D800 over Scena09_Cells, then a
//     tail jump through Scena09_CellHooks to one of fourteen cell handlers,
//     each answering in al.
// Chapter 10 (0x558140..0x55C040), vtable 0x661510:
//   - slot 0 Scena10_Frame through Scena10_States (0 is 0x5646B0, group
//     SC13's; 1 Scena10_EnterArea; 2 Scena10_Run through Scena10_Runs);
//   - the runs, each a switch on the step byte 0x8034E5 whose cases wait on a
//     counter, the request byte, the wait word, a timer or an effect, do one
//     thing and set the next step;
//   - slot 1 Scena10_ObjectTrigger through Scena10_Objects (13; entries 11 and
//     12 are also Scena09_Objects 5 and 6); slots 2 and 3 the step and arrive
//     hooks;
//   - the helpers the runs call directly, and two handlers areas 75 and 86
//     name in their descriptors' tables (Scena10_PickupPose,
//     Scena10_PickupEffect).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena12_Frame
// does; the fuzz swaps them for recorders. No divergence: each function is a
// faithful replacement, except that a dispatcher whose index lies outside its
// table, or whose entry is 0, aborts where the original would jump through
// its neighbour or to address 0 - the project's rule for an index past a
// table (round nine, section 6).
#include "game/scena_sc9b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc9b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc9b::at;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }

unsigned Area() { return Game_AreaNumber; }
void SetCounterScenaSc9b(unsigned k, unsigned char v) { B(::bof3::Img(scena_sc9b::at::kCounters) + k) = v; }
void SetStep(unsigned char v) { MoveScript_PartyRecords[0x65] = v; }
void SetRun(unsigned char v) { MoveScript_Var7 = v; }
unsigned char FD() { return Cond_ByteFD; }
void SetFE(unsigned char v) { Cond_ByteFE = v; }
// Field_ScriptFlags as the originals touch it: its low byte or'd, the word and'd.
void ScriptOrByteScenaSc9b(unsigned char v) { B(::bof3::Img(scena_sc9b::at::kScriptFlags)) = static_cast<unsigned char>(B(::bof3::Img(scena_sc9b::at::kScriptFlags)) | v); }

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Sound(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }

// A table entry read in place, the index checked against the table and the
// entry against 0 (the original would call address 0).
std::uint32_t EntryScenaSc9b(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    const std::uint32_t e = D(table + 4 * static_cast<std::uint32_t>(index));
    if (e == 0) bof3::Fatal("%s: entry %d of the table at 0x%X is 0 (the original calls address 0)", who, index, table);
    return e;
}

// A cell word minus a base below a bound, as the originals compare 16-bit
// registers unsigned.
bool Near(std::uint16_t cell, unsigned base, unsigned n) { return static_cast<std::uint16_t>(cell - base) < n; }

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC9B_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Chapter 9's tail: Scena09_Objects entries 7..11 and 13..15 (the entry's
// (object, bits) are not read)

// ===========================================================================
// Chapter 10: the vtable's slots, the state and run dispatchers

// ===========================================================================
// Chapter 10's runs (Scena10_Runs entries), each a switch on the step 0x8034E5

// ===========================================================================
// Chapter 10's helpers, called directly by its runs

// ===========================================================================
// Chapter 10's object handlers (Scena10_Objects entries)

// The object's pose set by objects 4, 6 and 7: +1 = 4, +0x84 = 2, +0x83 the
// pose, the word +0x8A 0.
void Pose(unsigned char* object, unsigned char pose) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = pose;
    *reinterpret_cast<std::uint16_t*>(object + 0x8A) = 0;
}

// ===========================================================================
// The handlers areas 75 and 86 name (their descriptors' +0x34 / +0x3C tables)

// ===========================================================================
// Chapter 10's hooks

void ScenaSc9b_Inject() {
    if (bof3::WantsShadow("scena_sc9b")) scena_sc9b::SelfTest();
}
