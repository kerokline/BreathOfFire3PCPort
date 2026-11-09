// Two spell overlays compiled into the exe, round nine group S04
// (docs/magic_s04.md): the PSX's MAGIC013 and MAGIC015.EMI (MAGIC016 folded
// into MAGIC015 by the linker), Magic_Rows rows 50, 4, 7, 55 and 58. Read one
// id down (docs/cut-content.md section 2) the sibling labels them Snap, Charge,
// Flying Kick and Air Raid; the names below use those labels as hypotheses,
// and say what the code does.
//
//   - MAGIC013 0x49E9F0..0x49FA66 (row 50): a wave child (kind 1, 0x29) that
//     swings out and back from the owner, drawing two tilted rings of gouraud
//     quads, and at each turn bursts up to 32 sparks out of a 64-record pool of
//     the overlay's own (flat triangles thrown on arcs); a buff child (kind 1,
//     0x48) when MagicFx_ApplyBuff says the target takes it;
//   - MAGIC015 0x49FA70..0x4A11D8 (rows 4, 7, 55, 58): copies of the acting
//     actor's record as kind-1 children (0x23) - Charge's four trails and an
//     image that dashes to the source sprite and springs back, Air Raid's three
//     images that rise, turn, dive and bounce, Flying Kick's one image that
//     rises and dives - each with a shadow of eight subtractive triangles while
//     it is in the air.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table (a stack table, or a .data table read in place) aborts where
// the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent), and the two wave
// matrices abort on a facing past 3, where the original's four-entry jump
// table leaves two angles as uninitialised stack words (the precedent of
// group S31's CoronaRay_PushMatrix and S25's SpellConfuse_PushFacingMatrix).
#include "game/magic_s04.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
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
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddL(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}
void AddW(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }
void CopyL(unsigned char* to, const unsigned char* from) { SetLong(to, Long(from)); }
std::uint32_t Addr(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl l` then `sar r` on a dword.
int ShlSar(int v, unsigned l, unsigned r) { return static_cast<int>(static_cast<std::uint32_t>(v) << l) >> r; }
// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return (v - (v >> 31)) >> 1; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions called directly, and the other units' phases a
// stack table holds, by address, as the originals call them: in the game
// Capcom's code or the jmp Inject put there, in the fuzz that address's
// recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}
// A .data table's call or jmp, `[table + phase * 4]`, read in place (the fuzz
// swaps the cells for recorders); the table's own entries only - what follows
// is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The GTE callees with the arguments the originals push: Gte_RotTrans a flag
// pointer more than it takes, the projections a depth and a flag pointer.
using RotTransFn = void (__cdecl*)(const short*, long*, long*);
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
#define S04_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Vertices 0..2 of Prim_VertexScratch projected to prim + 8, + 8 + step,
// + 8 + 2 step; vertices 0..3 to four.
void Rtp3(unsigned char* prim, unsigned step) {
    long p, flag;
    S04_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 8 + step, prim + 8 + 2 * step, &p, &flag);
}

// The matrix pushes' common tail (MagicFx_PushActorMatrix's shape): the
// translation RotTrans of `v`, the rotation of `angles`, Camera_Matrix x it,
// both set. One MATRIX block as the original lays it out on its stack,
// RotTrans writing its translation at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
void SetMatrix(const short* angles, const short* v) {
    Matrix m;
    long flag;
    S04_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}
// ((+0x34 sar 9) - 0x4000, (+0x38 sar 9) - 0x4000, height): the task's point in
// the GTE's units.
void GroundPoint(const unsigned char* s, short height, short* v) {
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(s)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(s)->z >> 9) - 0x4000);
    v[2] = height;
    v[3] = 0;
}

}  // namespace

#define S04_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC013 (row 50, Snap read one id down)

// ===========================================================================
// MAGIC015 (rows 4, 7, 55 and MAGIC016's 58: Charge, Air Raid, Flying Kick
// read one id down)

void MagicS04_Inject() {
    if (bof3::WantsShadow("magic_s04")) magic_s04::SelfTest();
}
