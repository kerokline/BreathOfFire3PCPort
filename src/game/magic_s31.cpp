// Four spell overlays compiled into the exe, round nine group S31
// (docs/magic_s31.md): the PSX's MAGIC132, MAGIC137, MAGIC138 and
// MAGIC143.EMI, Magic_Rows rows 139, 101, 118 and 66. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Doom Breath,
// Corona, Main Cannon and Thunder Clap; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC132 0x4E6950..0x4E7413: the caster darkened then brightened through
//     its tint record, one child (kind 1, 0x64) that flies out from the side's
//     centre and draws a stream of 63 textured quads and a two-quad glow;
//   - MAGIC137 0x4E7420..0x4E7FE0: two children (kind 1, 0x4B) - a ray that
//     travels across the field drawing a fan of textured gouraud quads, and a
//     full-screen flash - and, in an event battle whose acting enemy's +0x100
//     is 0x29, a copy of that enemy's record as a third child that plays its
//     animation 3;
//   - MAGIC138 0x4E7FF0..0x4E8656: the caster's script ticked while six shells
//     (kind 1, 0x56) are fired every eighth frame; each flies at the acting
//     actor and, past it, leaves a blast (the same kind, phase 1);
//   - MAGIC143 0x4E8660..0x4E9138: one bolt (kind 1, 0x35) at the source
//     sprite, drawn as Lightning's (group S22) with its own band, arcs and
//     flashes: three band rows, an arc, two flashes.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and Corona's ray
// matrix aborts on a facing past 3, where the original turns by an
// uninitialised stack word (the precedent of group S25's
// SpellConfuse_PushFacingMatrix).
#include "game/magic_s31.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
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
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

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

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S31_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0x10 a gouraud or flat-textured quad's, 0x14 a gouraud-textured one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S31_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
// A draw-mode packet (tpage `tpage`, dithered) committed to layer 2.
void DrawModeCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// Row 26 of Gfx_ClutStrip back from its source (0x812980 / 0x80E980), each
// word with its semi-transparency bit set.
void RestoreRow26Stp() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// The actor-matrix push of 0x4E6DF0 / 0x4E77D0 (MagicFx_PushActorMatrix's
// shape with a turn about z): Camera_Matrix x the task's, translation
// RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX
// block as the original lays it out on its stack, RotTrans writing its
// translation at +0x14; the task is read once, after the push.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
void PushTurnedMatrix(const unsigned char* s, short turn) {
    const short rot[4] = {0, 0, turn, 0};
    short v[4];
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(s)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(s)->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S31_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

}  // namespace

#define S31_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC132 (row 139, Doom Breath read one id down)

// ===========================================================================
// MAGIC137 (row 101, Corona read one id down)

// ===========================================================================
// MAGIC138 (row 118, Main Cannon read one id down)

// ===========================================================================
// MAGIC143 (row 66, Thunder Clap read one id down)

void MagicS31_Inject() {
    if (bof3::WantsShadow("magic_s31")) magic_s31::SelfTest();
}
