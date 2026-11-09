// Three spell overlays compiled into the exe, round nine group S03
// (docs/magic_s03.md): the PSX's MAGIC006, MAGIC009 and MAGIC012.EMI,
// Magic_Rows rows 109, 51 and 71. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Mind Sword, Chlorine and Blitz; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC006 0x49C3D0..0x49D831: four blades (kind 1, 0x53) rise over the
//     caster, spin, and fly at the source sprite; the first to arrive flags the
//     target 0x10 and bursts into eight sparks and a flash (the same kind,
//     phases 1 and 2);
//   - MAGIC009 0x49D840..0x49DEEE: the acting actor's record copied into a
//     child that plays its animation, CLUT row 26's second sixteen words set
//     semi-transparent, then three clouds (kind 1, 0x2A) that open round the
//     source sprite;
//   - MAGIC012 0x49E000..0x49E9E2: one bolt (kind 1, 0x37) per live actor of
//     the targeted side, each flying from the caster to its actor three times;
//     at the end the acting actor's word +0x98 (party) / +0xA4 (enemy) halved,
//     at least 1.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table, and ChlorineCloud_Start's offset index past its three-entry
// table, abort where the original would read on (docs/magic_fx_reached.md
// section 3, the precedent).
#include "game/magic_s03.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// SVECTORs of Prim_VertexScratch (0x9037A0..). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// CLUT row 26 of Gfx_ClutStrip (and of its source), in words.
constexpr unsigned kRow26 = 0x1A00;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Src() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return Low32(p); }
// A dword a + b (or a - b) as the originals' add / sub: wrapping.
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` then `sar m` on a dword.
int ShlSar(std::uint32_t v, unsigned n, unsigned m) { return static_cast<int>(v << n) >> m; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records unchecked: the party's by the index, the
// enemies' by the battle index - 3 (the acting actor) or by an enemy index
// (Blitz's bolts).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyByBattleIndex(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + (battle_index - 3u) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

// The phase handlers of other units a table holds (docs/magic_s03.md
// section 3) are ours now and named in the tables: S12's
// MagicFx_CountDownRelease, S07's FocusMote_Rise / EnlightenRays_Fade, S11's
// MagicFx_UncountAndFree.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

float AsFloat(std::int32_t v) { return static_cast<float>(v); }

void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}

// CLUT row 26's words `from`..`to` - 1 from the source with the
// semi-transparency bit set.
void Row26Stp(unsigned from, unsigned to) {
    for (unsigned k = kRow26 + from; k < kRow26 + to; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
}

}  // namespace

#define S03_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC006 (row 109, Mind Sword read one id down)

// ===========================================================================
// MAGIC009 (row 51, Chlorine read one id down)

// ===========================================================================
// MAGIC012 (row 71, Blitz read one id down)

void MagicS03_Inject() {
    if (bof3::WantsShadow("magic_s03")) magic_s03::SelfTest();
}
