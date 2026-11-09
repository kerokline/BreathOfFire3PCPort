// Four spell overlays compiled into the exe, round nine group S09
// (docs/magic_s09.md): the PSX's MAGIC045, MAGIC046 / MAGIC047 (the linker
// kept one copy of the two files' code), MAGIC048 and MAGIC050.EMI,
// Magic_Rows rows 61, 62 / 63, 41 and 31. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Bone Dart,
// Firebreath / Icebreath, Dream Breath and Pollen / Venom Breath; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC045 0x4A9830..0x4A9FD8: two children (kind 1, 0x31) - a dart that
//     flies at the source sprite, flags the target and bounces back, and a
//     shadow that follows it along the ground - unless the actor and the
//     target are on one side, which only flags the target;
//   - MAGIC046/047 0x4A9FE0..0x4AAC36: an emitter child (kind 1, 0x32) that
//     takes motes from a pool of its own (ElemBreath_Motes, 48 records) every
//     other frame; the task walks the pool, each mote a textured quad and a
//     flare of eight triangles drifting out along a sine; in an event battle
//     whose current enemy's +0x100 is 0x29 a second child plays the task's
//     animation 2;
//   - MAGIC048 0x4AAC40..0x4AB3EB: twelve motes (kind 1, 0x1E) that fly from
//     the caster to points over the target side's mean height, each a band of
//     32 gouraud quads;
//   - MAGIC050 0x4AB3F0..0x4ABC95: ten motes (kind 1, 4) placed round the
//     field's kind-2 point, each a fan, a ring and sixteen dots in screen
//     space, shaded by the ability word 0x904B80 (0x32 one way, else the
//     other: Pollen and Venom Breath read one id down).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and
// DreamBreath_TargetHeight aborts where the original's idiv faults (no target
// in: the live-target divide, docs/takeover-queue-round9.md section 7).
#include "game/magic_s09.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>

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
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kPollenDelays: now Pollen_Delays (symbols.toml). Pollen_Delays: 10 bytes
// kPollenOffsets: now PollenMote_Offsets (symbols.toml). PollenMote_Offsets: (dx, dz) dword pairs

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return Low32(p); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }
// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }
// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) - static_cast<std::uint32_t>(v >> 31)) >> 1; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

#define S09_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}
// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's +8 and position (+0x34 / +0x38 / +0x3C) the owner's.
void FacingAndPositionFromOwner() {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// Gfx_ClutStrip 0x1A00..0x1A0F (row 26's first sixteen words) back from its
// source with the semi-transparency bit, then the first word plain; the
// strip marked dirty first.
void RestoreRow26Head() {
    Gfx_ClutStripDirty = 1;
    for (unsigned k = 0; k < 0x10; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
}

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

// Camera_Matrix x the rotation of `angles`, translated by RotTrans of `v`,
// set as the GTE's (the two matrix pushes' common tail). The original pushes
// a third argument (a flag pointer) to Gte_RotTrans, which takes two.
void SetTurnedMatrix(const short* v, const short* angles) {
    Matrix m;
    long flag;
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S09_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC045 (row 61, Bone Dart read one id down)

// ===========================================================================
// MAGIC046 / MAGIC047 (rows 62 / 63, Firebreath / Icebreath read one id down)

// ===========================================================================
// MAGIC048 (row 41, Dream Breath read one id down)

// ===========================================================================
// MAGIC050 (row 31, Pollen / Venom Breath read one id down)

void MagicS09_Inject() {
    if (bof3::WantsShadow("magic_s09")) magic_s09::SelfTest();
}
