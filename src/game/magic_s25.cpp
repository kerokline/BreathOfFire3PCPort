// Four spell overlays of Magic_Rows, compiled into the exe at
// 0x4D2D80..0x4D610B - the spell round's group S25 (docs/magic_s25.md):
//
//   - MAGIC107 (row 24, ids 0x6B / 0xD1, "Sleep" read one id down),
//     0x4D2D80..0x4D3E9A: a kind-2 task that starts a kind-1 child (0x19)
//     over the target side - a stem, a dome of flat quads, a fan and a
//     trail of gouraud quads and lines, drawn under two actor matrices;
//   - MAGIC108 (row 25, ids 0x1B / 0x6C / 0xD2, "Confuse"), 0x4D3EA0..
//     0x4D49D2: its child (0x1A) flies from the source to the actor and
//     draws a burst of rays and two quads;
//   - MAGIC109 (row 12, ids 0x6D / 0xD3, "Depress"), 0x4D49E0..0x4D52FC: a
//     kind-2 task that grabs the target's screen box into VRAM and draws it
//     back as a twisting vortex of textured quads;
//   - MAGIC110 (row 38, ids 0x6E / 0xD4, "Ragnarok"), 0x4D5300..0x4D610B: a
//     kind-2 task with a screen-wide tint, two CLUT strips copied with the
//     semi-transparency bit, and kind-1 children (0x10) of three kinds - a
//     sprite that rises, a ring on the ground, and six sparks round the
//     target side.
//
// The names are the TCRF / sibling labels read one id down (cut-content
// section 2): hypotheses, not what the reading showed; what each function
// does is. Every call goes through the harness (MH_CALL / MH_AT / Phase), so
// the start-up fuzz can stand recorders in for ours as for the originals'
// copies. Calls into other groups' units go by name (the effect library's
// MagicFx_CenterOnSide, _StepTowardPoint, _NearPoint3D; MAGIC078's
// Leech_WaitOrbs; the engine's MagicFx_DoneAndFree), or through the .data
// tables (MAGIC130's 0x4E47F0, MAGIC082's 0x4C0680, MAGIC060's 0x4B1740);
// calls into the unnamed engine helper 0x446770 and libgpu setters 0x5A7570 /
// 0x5A76F0 go by raw address.
//
// No divergence: each is a faithful replacement, except that a dispatch past
// its table aborts where the original would call or jump through whatever
// follows the table (docs/magic_fx_reached.md section 3, the precedent):
// the four task stack tables, the six .data phase tables and the jump table
// of SpellConfuse_PushFacingMatrix (whose default case would build the
// matrix from uninitialised stack).
#include "game/magic_s25.h"

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
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The vertex scratch (Prim_VertexScratch, four SVECTORs at +0 / +8 / +0x10 /
// +0x18) and the word scratch at 0x903850 (+0xC is Scratch_Swap).
// kVertex: now Prim_VertexScratch (symbols.toml).
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};
unsigned char* V(unsigned k) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + k); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(V(k)); }
unsigned char* S(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }
std::int32_t SL(unsigned k) { return Long(S(k)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// imul (a 32-bit product that wraps), then sar 0xC.
int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
int Sar12(int v) { return v >> 12; }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }
void SetPointer(unsigned char* cell, const void* p) {
    SetLong(cell, static_cast<std::int32_t>(Low32(p)));
}

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
#define S25_AS(type, name) ::magic_harness::Call(As<type>(&::name))

// The GTE entry points as the overlays push them (one pointer more than
// symbols.gen.h's prototypes: the flag word).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);
using RotTransFn = void (__cdecl*)(const short*, long*, long*);

// Other units' phases in the stack tables, by name: S17's Leech_WaitOrbs
// ("free once the owner's +0xB is 0xFF") and the engine's MagicFx_DoneAndFree.

void Rtp3(unsigned char* p, unsigned a, unsigned b, unsigned c) {
    long depth, flag;
    S25_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), reinterpret_cast<float*>(p + a),
                                      reinterpret_cast<float*>(p + b), reinterpret_cast<float*>(p + c), &depth, &flag);
}
void Rtp4(unsigned char* p, unsigned a, unsigned b, unsigned c, unsigned d) {
    long depth, flag;
    S25_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), reinterpret_cast<float*>(p + a),
                                      reinterpret_cast<float*>(p + b), reinterpret_cast<float*>(p + c),
                                      reinterpret_cast<float*>(p + d), &depth, &flag);
}
void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                dy, size);
}

// The actor matrix every overlay of this group builds on its stack, as
// MagicFx_PushActorMatrix does with angles of its own: translation
// RotTrans((x >> 9) - 0x4000, (z >> 9) - 0x4000, -(height / 2)) of the
// sprite, rotation RotMatrix(angles), times Camera_Matrix, loaded. The
// caller has pushed the matrix; it pops nothing.
void LoadActorMatrix(const unsigned char* sc, const short* angles) {
    short v[4];
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(sc)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(sc)->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    S25_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// A .data phase table read in place: the original's `call` / `jmp
// [table + phase * 4]`, bounded.
Handler TableEntry(const char* who, std::uint32_t table, unsigned entries, unsigned phase) {
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table 0x%X", who, phase, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))));
}

}  // namespace

#define S25_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC107 (row 24): "Sleep"

// ===========================================================================
// MAGIC108 (row 25): "Confuse"

// ===========================================================================
// MAGIC109 (row 12): "Depress"

// ===========================================================================
// MAGIC110 (row 38): "Ragnarok"

void MagicS25_Inject() {
    if (bof3::WantsShadow("magic_s25")) magic_s25::SelfTest();
}
