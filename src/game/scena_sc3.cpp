// Scenario chapters 3 and 4 (the PSX's SCENA03 / SCENA04 overlays, compiled
// into the exe at 0x5428C0..0x54638D): chapter 3's vtable Scena03_Hooks
// 0x660F50 and chapter 4's Scena04_Hooks 0x660FD0 and everything they reach
// inside the band. docs/scena_sc3.md.
//
//   - Each chapter's slot 0 is a frame that jumps through its state table on
//     the s8 0x8034E2 (0 the start, 1 the area entry, 2 the run); the run
//     jumps through the run table on MoveScript_Var7 to a scene, a switch on
//     the step byte 0x8034E5 whose every case waits on one thing (a script
//     counter, the timer 0x8034E6, the message request, the wait word, a
//     stream), does one thing and sets the next step - Scena16_*'s shape.
//   - Slot 1 (the object trigger) dispatches on the object's +0x86 through a
//     table of handlers given (object, flag row); slots 2 and 3 (the step and
//     arrive hooks) test (x, z) by area and answer in al whether a scene
//     starts; slot 4 (the cell hook) asks 0x56D800 for the chapter's cell
//     record and runs its handler.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// The .data tables are read in place and called through, as the originals do,
// unchecked: the fuzz swaps them for recorders. No divergence: each function
// is a faithful replacement; every read the original makes after a call is
// made after the same call here, every store before a call before it.
#include "game/scena_sc3.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc3_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc3::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::EffectObject>(p) is given effect
// objects (Effect_Objects).
using ObjectHandler = std::uint32_t (__cdecl*)(unsigned char* object, unsigned char* bank);

// --- the cells --------------------------------------------------------------

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }
unsigned Area() { return Game_AreaNumber; }
void SetStep(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }
void SetRun(unsigned v) { MoveScript_Var7 = static_cast<unsigned char>(v); }
unsigned Timer() { return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)); }
void SetTimer(unsigned v) { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), v); }
void DecTimer() { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) - 1u); }
unsigned char& C0() { return Byte(::bof3::Img(scena_sc3::at::kCounter0)); }
unsigned char& C1() { return Byte(::bof3::Img(scena_sc3::at::kCounter1)); }
void ClearCounters() { SetLong(At(::bof3::Img(scena_sc3::at::kCounters)), 0); }
void PassFlags(unsigned v) { Draw_PassFlags = static_cast<unsigned char>(v); }
bool WaitDone() { return Transition_Busy == 0; }
bool Requested() { return Field_Request == 2; }
void ScriptFlagsOr80() { Byte(::bof3::Img(scena_sc3::at::kScriptFlags)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc3::at::kScriptFlags)) | 0x80); }
void ScriptFlagsAnd(unsigned mask) { Field_ScriptFlags = Field_ScriptFlags & mask; }
short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }

// --- the callees nobody owns, by address -------------------------------------

void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc3::kPartyRestore.v)(); }
void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc3::kStatusBit80.v)(); }
unsigned char CellFind(std::uint32_t records, unsigned n, int x, int z) {
    return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(std::uint32_t, unsigned, int, int), scena_sc3::kCellFind.v)(records, n, x, z));
}

// --- the effects ---------------------------------------------------------------

// Takes an effect slot into the byte 0x903850 and, when there is one, marks
// its record live as `kind`; the record is indexed by the stored byte read
// back as a dword's low byte, as the originals have it.
unsigned char* TakeEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    scena_sc3::at::kSlot.b_00 = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(::bof3::Img(Effect_Objects_addr) + ((static_cast<std::uint32_t>(scena_sc3::at::kSlot.d_00) & 0xFF) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}
// The effect whose slot is kept in the s8 0x8034E3: indexed by the answer as
// a signed byte (an answer of 0x80..0xFE would index below the pool).
unsigned char* TakeKeptEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    MoveScript_PartyRecords[0x63] = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(::bof3::Img(Effect_Objects_addr) + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(slot)) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}

// The high word of a 16.16 coordinate, and "within n cells of lo" as the
// originals test it: a 16-bit subtraction compared unsigned.
unsigned Hi(int v) { return (static_cast<std::uint32_t>(v) >> 16) & 0xFFFF; }
bool Near(unsigned hi, unsigned lo, unsigned n) { return ((hi - lo) & 0xFFFF) < n; }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void MusicPlayTrack() { SH_CALL(Music_Play)(Music_Track, 8); }

}  // namespace

// ===========================================================================
// Chapter 3
// ===========================================================================

// ===========================================================================
// Chapter 4
// ===========================================================================

// ===========================================================================

void ScenaSc3_Inject() {
    if (bof3::WantsShadow("scena_sc3")) scena_sc3::SelfTest();
}
