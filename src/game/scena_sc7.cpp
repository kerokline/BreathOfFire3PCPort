// Scenario chapters 7 and 8 (the PSX's SCENA07 / SCENA08 overlays, compiled
// into the exe at 0x54F080..0x553B21): chapter 7's vtable Scena07_Hooks
// 0x6611B8 and chapter 8's Scena08_Hooks 0x661368 and everything they reach
// inside the band. docs/scena_sc7.md.
//
//   - Each chapter's slot 0 is a frame that jumps through its state table on
//     the s8 0x8034E2 (0 the start, 1 the area entry, 2 the run); the run
//     jumps through the run table on MoveScript_Var7 to a scene, a switch on
//     the step byte 0x8034E5 whose every case waits on one thing (a script
//     counter, the timer 0x8034E6, the message request, the wait word, a
//     stream or a file), does one thing and sets the next step -
//     Scena16_*'s shape.
//   - Slot 1 (the object trigger) dispatches on the object's +0x86 through a
//     table of handlers given (object, flag row); slots 2 and 3 (the step and
//     arrive hooks) test (x, z) by area and answer in al whether a scene
//     starts; chapter 7's slot 4 (the cell hook) asks 0x56D800 for the
//     chapter's cell record and runs its handler. Chapter 8 has no slot 4.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// The .data tables are read in place and called through, as the originals do,
// unchecked: the fuzz swaps them for recorders. No divergence: each function
// is a faithful replacement; every read the original makes after a call is
// made after the same call here, every store before a call before it.
#include "game/scena_sc7.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc7_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc7::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();
using ObjectHandler = void (__cdecl*)(unsigned char* object, unsigned char* bank);

// --- the cells --------------------------------------------------------------

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }
unsigned Area() { return Game_AreaNumber; }
unsigned Step() { return MoveScript_PartyRecords[0x65]; }
void SetState(unsigned v) { MoveScript_PartyRecords[0x62] = static_cast<unsigned char>(v); }
void SetStep(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }
void SetRun(unsigned v) { MoveScript_Var7 = static_cast<unsigned char>(v); }
void SetTimer(unsigned v) { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), v); }
// `dec word [timer]` then the flags of the result: the timer after.
unsigned DecTimer() {
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) - 1u);
    return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66));
}
unsigned char& C0() { return Byte(::bof3::Img(scena_sc7::at::kCounter0)); }
void IncC0() { C0() = static_cast<unsigned char>(C0() + 1); }
void PassFlags(unsigned v) { Draw_PassFlags = static_cast<unsigned char>(v); }
bool WaitDone() { return Transition_Busy == 0; }
bool Requested() { return Field_Request == 2; }
void Request(unsigned v) { Field_Request = static_cast<unsigned char>(v); }
void ScriptFlagsOr(unsigned v) { Byte(::bof3::Img(scena_sc7::at::kScriptFlags)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc7::at::kScriptFlags)) | v); }
void ScriptFlagsAnd(unsigned mask) { Field_ScriptFlags = Field_ScriptFlags & mask; }
short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }
void Kind2At(std::int32_t x, std::int32_t z) {
    Field_Kind2X = x;
    Field_Kind2Z = z;
}
unsigned char* Sprite(std::uint32_t k) { return At(::bof3::Img(Sprite_Objects_addr) + k * scena_sc7::at::kSpriteStride); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }

// The high word of a 16.16 coordinate, and "within n cells of lo" as the
// originals test it: a 16-bit subtraction compared unsigned.
unsigned Hi(int v) { return (static_cast<std::uint32_t>(v) >> 16) & 0xFFFF; }
bool Near(unsigned hi, unsigned lo, unsigned n) { return ((hi - lo) & 0xFFFF) < n; }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void DropIn(unsigned k) { SH_CALL(Party_DropIn)(k); }
void Message(unsigned id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
    Request(2);
}
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Transition(unsigned kind) { SH_CALL(Transition_Start)(static_cast<unsigned char>(kind)); }
// The file loaded (a load already started): polled with a frame's sleep.
void WaitFile() {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do SH_CALL(Task_Sleep)(1);
    while (SH_CALL(File_LoadDone)() == 0);
}

// --- the callees nobody owns, by address -------------------------------------

void PlaceParty(std::int32_t x, std::int32_t z, unsigned k) { SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, unsigned), scena_sc7::kPlaceParty.v)(x, z, k); }
void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc7::kPartyRestore.v)(); }
void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc7::kStatusBit80.v)(); }
std::uint32_t SpriteFindFree() { return SH_AT(std::uint32_t (__cdecl*)(), scena_sc7::kSpriteFindFree.v)(); }
void MusicStop() { SH_AT(void (__cdecl*)(), scena_sc7::kMusicStop.v)(); }

// --- the effects ---------------------------------------------------------------

unsigned char* EffectAt(unsigned slot) { return At(::bof3::Img(Effect_Objects_addr) + ((slot & 0xFFu) << 7)); }
// Effect_FindFree into the byte `cell`: the slot (0xFF none).
unsigned char TakeInto(std::uint32_t cell) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Byte(cell) = slot;
    return slot;
}
// A record marked live as `kind`.
void Live(unsigned char* e, unsigned kind) {
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
}
// Kind 0x13 with its three words (+0x64, +0x68, +0x6C) and +9.
void Effect13(unsigned char* e, std::int32_t a, std::int32_t b, std::int32_t c, unsigned b9) {
    Live(e, 0x13);
    SetLong(e + 0x64, a);
    SetLong(e + 0x68, b);
    SetLong(e + 0x6C, c);
    e[9] = static_cast<unsigned char>(b9);
}
// The record 0x903850 names, read back as a dword's low byte, as the
// originals index it after the store.
unsigned char* Slot850() { return EffectAt(static_cast<std::uint32_t>(scena_sc7::at::kSlot.d_00) & 0xFF); }

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::EffectObject>(p) is given effect
// objects (Effect_Objects).

}  // namespace

// ===========================================================================
// Chapter 7
// ===========================================================================

// ===========================================================================
// Chapter 8
// ===========================================================================

void ScenaSc7_Inject() {
    if (bof3::WantsShadow("scena_sc7")) scena_sc7::SelfTest();
}
