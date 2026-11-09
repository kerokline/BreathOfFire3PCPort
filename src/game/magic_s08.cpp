// Four spell overlays compiled into the exe, round nine group S08
// (docs/magic_s08.md): the PSX's MAGIC041, MAGIC042, MAGIC043 and
// MAGIC044.EMI, Magic_Rows rows 80, 81, 110 and 96. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Berserk, Counter /
// Mind's Eye, WardOfLight / Resist and Evil Eye; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - MAGIC041 0x4A6440..0x4A6DE9: the source sprite darkened through its tint
//     record, brightened and faded back; nine children (kind 1, 0x3D): eight
//     rings of 64 gouraud quads that rise and open one after another, and a
//     glow of sixteen gouraud triangles;
//   - MAGIC042 0x4A6DF0..0x4A7822: one child (kind 1, 0x3E) on the target's
//     screen point (moved by a per-character offset table for a party target):
//     a disc of sixteen triangles, spokes and three arcs of lines;
//   - MAGIC043 0x4A7830..0x4A8246: the source sprite tinted, a pool of its
//     own (64 records at 0x67BC40) of eight motes launched from the owner
//     toward the field's kind-2 point, each a band of eight gouraud quads;
//     for ability 0x2B a buff (MagicFx_ApplyBuff) and a child (kind 1, 0x48);
//   - MAGIC044 0x4A8250..0x4A9822: two beams (kind 1, 6) that seek the source
//     sprite leaving a trail of up to sixteen screen points (0x67DD40, 0x80
//     bytes a beam), drawn as a ribbon of gouraud quads, and sparks shed
//     along the way (the same kind, phase 1), each one textured quad.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and
// CounterMark_DrawSpokes aborts for a first angle of 0xE0 or more, where the
// original's byte counter never reaches its end (its one caller passes 0).
#include "game/magic_s08.h"

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
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kWardTilts: now WardMote_Tilts (symbols.toml). WardMote_Tilts

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
unsigned char* TintRecord(unsigned index) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + index * 12u); }

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

// The phase handler of another unit a stack table holds (docs/magic_s08.md
// section 3), S12's MagicFx_CountDownRelease, is ours now and named there.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}
// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The task's position +0x34 / +0x38 / +0x3C from the owner's.
void OwnerPosition() {
    const unsigned char* const o = Owner();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = rv::As<records::SpriteHead>(o)->x;
    ts->z = rv::As<records::SpriteHead>(o)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(o)->height);
}

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S08_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four vertex-scratch SVECTORs projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void LinkAtTask(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy, size);
}
int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC041 (row 80, Berserk read one id down)

// ===========================================================================
// MAGIC042 (row 81, Counter and Mind's Eye read one id down)

// ===========================================================================
// MAGIC043 (row 110, WardOfLight and Resist read one id down)

// The mote's steps: the position +0x34 / +0x38 on by the step +0xC / +0x10,
// +9 up.
// original 0x4A7E60: the mote's matrix pushed: Camera_Matrix x the rotation
// (0x400, WardMote_Tilts[+8], 0), translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)). One MATRIX block as the original lays it
// out on its stack, RotTrans writing its translation at +0x14.
namespace {
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
}  // namespace

// ===========================================================================
// MAGIC044 (row 96, Evil Eye read one id down)

// The beam's trail drawn as a ribbon of semi-transparent gouraud quads
// (layer 3, tpage 0x35): from the first point at or after +0xB that is not
// erased (x 0xFFFF), each segment up to +0xA; each quad's corners are the
// segment's two points pushed out along the perpendicular of the segment's
// Math_Ratan2 heading by the width (scratch word 0): `edges` +1 the left
// (heading + 0x400), -1 the right (heading - 0x400), 0 both. The first
// segment's heading is its own; each quad takes the previous segment's at its
// near end.
// The trail pushed back one point, from +0xA - 1 down to +0xB (points of beam
// +4 in EvilEyeBeam_Trails, 32 a beam, unchecked).
void MagicS08_Inject() {
    if (bof3::WantsShadow("magic_s08")) magic_s08::SelfTest();
}
