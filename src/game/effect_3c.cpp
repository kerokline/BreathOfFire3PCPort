// Round thirteen group E3C (docs/effect_3c.md): the 50 functions of
// analysis/round13_cut.tsv's group E3C, 0x484050..0x485CA0, and one start the
// cut does not list (kind 0x75's shared tail 0x485C60, which two states jump
// to and which has its own frame and ret), each read with capstone to its last
// instruction (2026-10-03). Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x6E   sixteen shards (0x28 bytes at EffectKind30_Shards) emitted
//               one every fourth frame from the record's point and risen,
//               shrinking and darkening, by E3B's shard quad; Field_ScriptFlags
//               bit 6 held while any is live
//   kind 0x6D   thirty-two flat triangles (0x28 bytes, at 0x92C780 + 0x500 *
//               +6) thrown from one of two fixed boxes and falling, each a
//               shape of eight by a random byte
//   kind 0x6F   a line of four segments along z at x 0x5C (+1 = 1) or along x
//               at z 0x44 (+1 = 2), each a LINE_F2 and two shaded quads, by
//               story flags 0x5C / 0x5D and Cond_ByteFD; party members on its
//               four cells are turned away and set state 2_8
//   kind 0x72   thirty-two debris triangles (0x2C bytes at EffectKind30_Shards),
//               kind 0x1F's twin
//   kind 0x73   thirty-two textured sparks (0x18 bytes at EffectKind30_Shards):
//               +1 = 0 emitted one every fourth frame and risen; +1 = 1 sixteen
//               at once, spread by their angle byte
//   kind 0x74   a column of textured quads turning round the record's point,
//               rising to eight units over 0x40 frames and fading
//   kind 0x75   a full-screen tile brightened over fifteen frames (E4D's
//               0x48CA90), the field objects at pose 6 redrawn over it
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table of its own past its rows or ObjTrio past
// its three records, ours aborts with a message (docs/effect_3c.md section 6).
#include "game/effect_3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;
using RecordFn = void (__cdecl*)(unsigned char*);
using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; sub eax, edx; sar eax, 1: a signed halving toward zero.
U Half(U v) { return Sar(v + (S32(v) < 0 ? 1u : 0u), 1); }
// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }
const unsigned char* Story() { return At(at::kStoryFlags); }
U Cell(U cell) { return UL(At(cell)); }
void SetCell(U cell, U v) { SetUL(At(cell), v); }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(std::int32_t v, void* o) {
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(v) : "st", "memory");
}
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fild dword [v]; fsubr dword [c]; fstp dword [o]`: c - v
void FildSubr(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fild dword [v]; fsubr dword [c]; fiadd dword [w]; fstp dword [o]`: c - v + w
void FildSubrAdd(std::int32_t v, const void* c, std::int32_t w, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfiaddl %3\n\tfstps (%0)"
                     :
                     : "r"(o), "r"(c), "m"(v), "m"(w)
                     : "st", "memory");
}
// `fld dword [a]; fst dword [o1]; fstp dword [o2]` (a copy through the FPU,
// which turns a signalling NaN quiet)
void FldCopy2(const void* a, void* o1, void* o2) {
    __asm__ volatile("flds (%0)\n\tfsts (%1)\n\tfstps (%2)" : : "r"(a), "r"(o1), "r"(o2) : "st", "memory");
}
// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword
// (eax), which is all the callers keep.
// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// `fld dword [a]; call _ftol`
U FldFtol(const void* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3c.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 (or +2) up, Sprite_Current read afresh.
void Step(unsigned byte) {
    unsigned char* const s = S();
    s[byte] = static_cast<unsigned char>(s[byte] + 1);
}
// +9 down one; answers whether it is now 0 (Sprite_Current read for each access).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// Kind 0x6F's colour row, by Sprite_Current +6 (read afresh each time): one row
// of three bytes; a +6 past it reads into the state table after it.
unsigned char Kind6FColour(unsigned byte, const char* who) {
    const unsigned k = S()[6];
    if (k >= at::kKind6FColourCount)
        bof3::Fatal("%s: +6 is %u, past kind 0x6F's one colour row at 0x%X - the original reads EffectKind6F_States' "
                    "bytes as a colour (docs/effect_3c.md section 6)",
                    who, k, (unsigned)at::kKind6FColour);
    return At(at::kKind6FColour + 3 * k + byte)[0];
}

// ObjTrio record m (Party_MemberAt's answer); past the three, ours aborts.
unsigned char* Member(U m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: Party_MemberAt answered %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_3c.md section 6)",
                    who, (unsigned)m);
    return ObjTrio + at::kObjTrioStride * m;
}

}  // namespace

// ===========================================================================
// Kind 0x6E: Effect_KindHandlers[0x6E] (0x655508), EffectKind6E_States (four)
// ===========================================================================

// original 0x484050 (Effect_KindHandlers[0x6E], hidden in E3B's 0x483DA0):
// call [EffectKind6E_States + +1 * 4], unbounded; then a tail jump to
// EffectKind6E_DrawShards (its answer left in eax).
extern "C" void __cdecl EffectKind6E_Run(void) {
    Dispatch("EffectKind6E_Run", AddressOf(EffectKind6E_States), EffectKind6E_States_count, 1);
    SH_CALL(EffectKind6E_DrawShards)();
}

// original 0x484070 (state 0): Field_ScriptFlags bit 6 set (or byte), E3B's
// shard spread (0x483C10), +9 = 0xB4, +1 up.
extern "C" void __cdecl EffectKind6E_Start(void) {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x40u);
    SH_AT(VoidFn, at::kShardsSpread)();
    S()[9] = 0xB4;
    Step(1);
}

// original 0x484090 (state 1): every fourth frame (Frame_Counter & 3 == 0) a
// free shard (EffectKind6E_FindShard, its answer stored in the cursor whether
// null or not) set up at the record's point (EffectKind6E_ShardInit) with its
// rise +0x1C = 0x200000 (the shard read back from the cursor). +9 down; at 0,
// +0xC = 0x200000, +9 = 0x40, +1 up.
extern "C" void __cdecl EffectKind6E_Emit(void) {
    if ((Frame_Counter & 3u) == 0) {
        unsigned char* const r = SH_CALL(EffectKind6E_FindShard)();
        SetCell(at::kShardCursor, AddressOf(r));
        if (r != nullptr) {
            SH_CALL(EffectKind6E_ShardInit)(r);
            SetUL(At(Cell(at::kShardCursor)) + 0x1C, 0x200000);
        }
    }
    if (CountDown()) {
        SetUL(S() + 0xC, 0x200000);
        S()[9] = 0x40;
        Step(1);
    }
}

// original 0x4840F0 (state 2): as state 1, each new shard's rise the record's
// +0xC, which drops 0x10000 a shard (+0xC += 0xFFFF0000 first). +9 down; at 0,
// +1 up.
extern "C" void __cdecl EffectKind6E_EmitSlower(void) {
    if ((Frame_Counter & 3u) == 0) {
        unsigned char* const r = SH_CALL(EffectKind6E_FindShard)();
        SetCell(at::kShardCursor, AddressOf(r));
        if (r != nullptr) {
            SH_CALL(EffectKind6E_ShardInit)(r);
            unsigned char* s = S();
            SetUL(s + 0xC, UL(s + 0xC) + 0xFFFF0000u);
            s = S();
            SetUL(At(Cell(at::kShardCursor)) + 0x1C, UL(s + 0xC));
        }
    }
    if (CountDown()) Step(1);
}

// original 0x484150 (state 3): EffectKind6E_DrawShards; none live (al 0):
// Effect_Release and Field_ScriptFlags bit 6 cleared (and word, 0xFFBF). The
// dispatcher draws them again after it.
extern "C" void __cdecl EffectKind6E_Fade(void) {
    if (SH_CALL(EffectKind6E_DrawShards)() == 0) {
        SH_CALL(Effect_Release)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFBFu);
    }
}

// original 0x484170: a shard at Sprite_Current's point - +4 / +8 / +0xC its
// +0x34 / +0x38 / +0x3C; +0 1, +0x1C 0, +1 0, +0x24 (s16) 0x80, +3 0xC0,
// +0x26 2, +2 0x20 (the life).
extern "C" void __cdecl EffectKind6E_ShardInit(unsigned char* shard) {
    SetUL(shard + 4, UL(S() + 0x34));
    SetUL(shard + 8, UL(S() + 0x38));
    const U y = UL(S() + 0x3C);
    shard[0] = 1;
    SetUL(shard + 0xC, y);
    SetUL(shard + 0x1C, 0);
    shard[1] = 0;
    SetWord(shard + 0x24, 0x80);
    shard[3] = 0xC0;
    shard[0x26] = 2;
    shard[2] = 0x20;
}

// original 0x4841C0: the first of the sixteen 0x28-byte shards at
// EffectKind30_Shards whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind6E_FindShard(void) {
    U p = Shards();
    for (unsigned i = 0; i < at::kShardCount; ++i, p += at::kShardStride)
        if (At(p)[0] == 0) return At(p);
    return nullptr;
}

// original 0x4841E0: EffectGte_LoadMapCamera; a draw mode (page (0x3C0, 0),
// dtd 0) committed (Gfx_CommitPrim(2, 0xC)); the sixteen shards stepped with
// the cursor kShardCursor: each live one risen (+0xC += +0x1C), grown (+0x24
// += 8), darkened (+3 -= 6), its life +2 down (0 frees it: +0 = 0) and drawn
// by E3B's shard quad. al 1 when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind6E_DrawShards(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 0x3C0, 0, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    U c = Shards();
    SetCell(at::kShardCursor, c);
    unsigned char any = 0;
    for (unsigned n = at::kShardCount; n != 0; --n) {
        unsigned char* const r = At(c);
        if (r[0] != 0) {
            SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
            unsigned char* cur = At(Cell(at::kShardCursor));
            SetWord(cur + 0x24, Word(cur + 0x24) + 8u);
            cur = At(Cell(at::kShardCursor));
            cur[3] = static_cast<unsigned char>(cur[3] + 0xFA);
            cur = At(Cell(at::kShardCursor));
            cur[2] = static_cast<unsigned char>(cur[2] - 1);
            cur = At(Cell(at::kShardCursor));
            if (cur[2] == 0) {
                cur[0] = 0;
                cur = At(Cell(at::kShardCursor));
            }
            SH_AT(RecordFn, at::kShardQuad)(cur);
            c = Cell(at::kShardCursor);
            any = 1;
        }
        c += at::kShardStride;
        SetCell(at::kShardCursor, c);
    }
    return any;
}

// ===========================================================================
// Kind 0x6D: Effect_KindHandlers[0x6D] (0x655504), EffectKind6D_States (two)
// ===========================================================================

// original 0x4842A0 (Effect_KindHandlers[0x6D], hidden in 0x4841E0):
// jmp [EffectKind6D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6D_Run(void) {
    Dispatch("EffectKind6D_Run", AddressOf(EffectKind6D_States), EffectKind6D_States_count, 1);
}

// original 0x4842C0 (state 0): the block's particles cleared, then set up;
// +9 = 0xFF, +1 up.
extern "C" void __cdecl EffectKind6D_Start(void) {
    SH_CALL(EffectKind6D_ClearParticles)();
    SH_CALL(EffectKind6D_InitParticles)();
    S()[9] = 0xFF;
    Step(1);
}

// original 0x4842E0 (state 1): EffectKind6D_MoveParticles; none live (al 0):
// a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind6D_Move(void) {
    if (SH_CALL(EffectKind6D_MoveParticles)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4842F0: +0 = 0 in the 32 particles of the block 0x92C780 +
// 0x500 * +6 (+6 unbounded: the block is computed, not checked).
extern "C" void __cdecl EffectKind6D_ClearParticles(void) {
    U p = at::kParticles + at::kParticleBlock * S()[6];
    for (unsigned n = at::kParticleCount; n != 0; --n, p += at::kParticleStride) At(p)[0] = 0;
}

// original 0x484320: EffectGte_LoadMapCamera; each live particle of the block
// (+6 read once) moved: +0x18 += +8, +0x1C += +0xC, +0x20 += +0x10, then +0x10
// -= 0x40000 (it falls), +3 -= 4, its shape +4 = Rand & 7, its life +2 down (0
// frees it) and drawn (EffectKind6D_DrawParticle). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind6D_MoveParticles(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* r = At(at::kParticles + at::kParticleBlock * S()[6]);
    unsigned char any = 0;
    for (unsigned n = at::kParticleCount; n != 0; --n, r += at::kParticleStride) {
        if (r[0] == 0) continue;
        const U vy = UL(r + 0x10);
        SetUL(r + 0x18, UL(r + 0x18) + UL(r + 8));
        SetUL(r + 0x1C, UL(r + 0x1C) + UL(r + 0xC));
        SetUL(r + 0x20, UL(r + 0x20) + vy);
        SetUL(r + 0x10, vy + 0xFFFC0000u);
        r[3] = static_cast<unsigned char>(r[3] + 0xFC);
        r[4] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
        r[2] = static_cast<unsigned char>(r[2] - 1);
        if (r[2] == 0) r[0] = 0;
        SH_CALL(EffectKind6D_DrawParticle)(r);
        any = 1;
    }
    return any;
}

// original 0x4843B0: a draw mode (page (0x3C0, 0), dtd 0) linked at the
// particle's (+0x18, +0x1C) (MapView_LinkPrimAt(.., 0, 0xC)); a flat triangle
// (0x5A7570) at the cursor, semi-transparent; its first corner the particle's
// point projected, the other two that corner plus a row of
// kParticleShapes by +4 (fild of each s16, fadd of the projected float);
// the depth copied to all three through the FPU; colour (0, +3, +3); linked at
// (+0x18, +0x1C) with dy -2, 0x2C. +4 past the eight rows: ours aborts.
extern "C" void __cdecl EffectKind6D_DrawParticle(unsigned char* p) {
    DrawMode(1, 0x3C0, 0, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x18), UL(p + 0x1C), 0, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_AT(RecordFn, at::kPolyF3)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p + 0x18), reinterpret_cast<float*>(prim + 8));
    const unsigned k = p[4];
    if (k >= at::kParticleShapeCount)
        bof3::Fatal("EffectKind6D_DrawParticle: +4 is %u, past the eight shapes at 0x%X - the original reads what "
                    "follows (docs/effect_3c.md section 6)",
                    k, (unsigned)at::kParticleShapes);
    const unsigned char* const row = At(at::kParticleShapes + 8 * k);
    FildAdd(SW(row + 0), prim + 8, prim + 0x14);
    FildAdd(SW(row + 2), prim + 0xC, prim + 0x18);
    FildAdd(SW(row + 4), prim + 8, prim + 0x20);
    prim[4] = 0;
    FildAdd(SW(row + 6), prim + 0xC, prim + 0x24);
    FldCopy2(prim + 0x10, prim + 0x28, prim + 0x1C);
    prim[5] = p[3];
    prim[6] = p[3];
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x18), UL(p + 0x1C), -2, 0x2C);
}

// original 0x4844C0: the 32 particles of the block (its address from +6 at
// entry) set up, each from one of two boxes chosen by +6 read again for each:
// +6 0 - x (+0x18) 0x120000 + (Rand & 0x1FF) << 8, z (+0x1C) 0x2E8000 + ..,
// height speed +0x20 (0x228000 - x) << 7, +8 (x - 0x130000) >> 4 less 0x800,
// +0xC (z - 0x2F8000) >> 4, +0x10 -(Rand & 0xFF) << 5; +6 not 0 - x 0x78000
// + .., z 0x318000 + .., +0x20 (0x420000 - z) << 7, +8 (x - 0x88000) >> 4,
// +0xC -0x800, +0x10 as before. Then +0 1, +1 0, +2 0x18, +3 0x60, +4 Rand & 7.
extern "C" void __cdecl EffectKind6D_InitParticles(void) {
    unsigned char* r = At(at::kParticles + at::kParticleBlock * S()[6]);
    for (unsigned n = at::kParticleCount; n != 0; --n, r += at::kParticleStride) {
        if (S()[6] == 0) {
            SetUL(r + 0x18, ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) + 0x1200u) << 8);
            const U z = ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) + 0x2E80u) << 8;
            const U x = UL(r + 0x18);
            SetUL(r + 0x1C, z);
            SetUL(r + 0x20, (0x228000u - x) << 7);
            SetUL(r + 8, Sar(x + 0xFFED0000u, 4));
            SetUL(r + 0xC, Sar(z + 0xFFD08000u, 4));
            SetUL(r + 0x10, (0u - (static_cast<U>(SH_CALL(Rand)()) & 0xFFu)) << 5);
            SetUL(r + 8, UL(r + 8) + 0xFFFFF800u);
        } else {
            SetUL(r + 0x18, ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) + 0x780u) << 8);
            const U z = ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) + 0x3180u) << 8;
            const U x = UL(r + 0x18);
            SetUL(r + 0x1C, z);
            SetUL(r + 0x20, (0x420000u - z) << 7);
            SetUL(r + 8, Sar(x - 0x88000u, 4));
            SetUL(r + 0xC, Sar(z + 0xFFCD8000u, 4));
            const U t = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(r + 0xC, 0xFFFFF800u);
            SetUL(r + 0x10, (0u - t) << 5);
        }
        r[0] = 1;
        r[1] = 0;
        r[2] = 0x18;
        r[3] = 0x60;
        r[4] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
}

// ===========================================================================
// Kind 0x6F: Effect_KindHandlers[0x6F] (0x65550C), EffectKind6F_States (four)
// ===========================================================================

// original 0x4845F0 (Effect_KindHandlers[0x6F], hidden in 0x4844C0):
// jmp [EffectKind6F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6F_Run(void) {
    Dispatch("EffectKind6F_Run", AddressOf(EffectKind6F_States), EffectKind6F_States_count, 1);
}

// original 0x484610 (state 0): Cond_ByteFD 2 with story flag 0x5C clear: +1 =
// 1; else Cond_ByteFD (read again) 3 with flag 0x5D set: +1 = 2; else +1 = 3.
extern "C" void __cdecl EffectKind6F_Start(void) {
    if (Cond_ByteFD == 2 && SH_CALL(Flags_Test)(Story(), 0x5C) == 0) {
        S()[1] = 1;
        return;
    }
    if (Cond_ByteFD == 3 && SH_CALL(Flags_Test)(Story(), 0x5D) != 0) {
        S()[1] = 2;
        return;
    }
    S()[1] = 3;
}

namespace {
// One of the line's four steps: its top at the ground (+0x3E) + 0x100 and +
// 0x200 (+0x20, copied to +0x14), each drawn from (+0xC..) to (+0x18..), then
// BareRet with the two points (a bare ret: nothing).
void Kind6FSegments() {
    for (U top = 0x100; top <= 0x200; top += 0x100) {
        unsigned char* s = S();
        SetUL(s + 0x20, static_cast<U>(SW(s + 0x3E) + static_cast<std::int32_t>(top)) << 16);
        s = S();
        SetUL(s + 0x14, UL(s + 0x20));
        s = S();
        SH_CALL(EffectKind6F_DrawSegment)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    }
    SH_CALL(BareRet)();
}
}  // namespace

// original 0x484670 (state 1): +6 0; the line along z at x 0x5C0000 (+0xC and
// +0x18), from z 0x98000 (+0x10); its ground AreaMap_Elevation(+0xC, (+0x1C +
// +0x10) / 2) to +0x3E (s16) - +0x1C not yet written, the record's own -;
// EffectKind6F_PushParty(0); then two steps of 0x20000: +0x1C = +0x10 +
// 0x20000, the record's point +0x34 / +0x38 the step's middle, the segments
// (Kind6FSegments), +0x10 = +0x1C, +0x1C += 0x20000. Story flag 0x5C set:
// sound 0x206, +1 = 3.
extern "C" void __cdecl EffectKind6F_AlongZ(void) {
    S()[6] = 0;
    SetUL(S() + 0x18, 0x5C0000);
    SetUL(S() + 0xC, 0x5C0000);
    SetUL(S() + 0x10, 0x98000);
    unsigned char* s = S();
    const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0xC)), static_cast<long>(Half(UL(s + 0x1C) + UL(s + 0x10))));
    SetWord(S() + 0x3E, static_cast<U>(e));
    SH_CALL(EffectKind6F_PushParty)(0);
    for (int i = 2; i != 0; --i) {
        s = S();
        SetUL(s + 0x1C, UL(s + 0x10) + 0x20000u);
        s = S();
        SetUL(s + 0x34, UL(s + 0xC));
        s = S();
        SetUL(s + 0x38, Half(UL(s + 0x1C) + UL(s + 0x10)));
        Kind6FSegments();
        s = S();
        SetUL(s + 0x10, UL(s + 0x1C));
        s = S();
        SetUL(s + 0x1C, UL(s + 0x1C) + 0x20000u);
    }
    if (SH_CALL(Flags_Test)(Story(), 0x5C) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 3;
    }
}

// original 0x4847D0 (state 2): the same along x at z 0x440000 (+0x1C and
// +0x10), from x 0x3D8000 (+0xC); the ground AreaMap_Elevation((+0x18 + +0xC)
// / 2, +0x10); EffectKind6F_PushParty(1); the steps on +0x18 / +0xC. Story flag
// 0x5D clear: sound 0x206, +1 = 3.
extern "C" void __cdecl EffectKind6F_AlongX(void) {
    S()[6] = 0;
    SetUL(S() + 0xC, 0x3D8000);
    SetUL(S() + 0x1C, 0x440000);
    SetUL(S() + 0x10, 0x440000);
    unsigned char* s = S();
    const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(Half(UL(s + 0x18) + UL(s + 0xC))), static_cast<long>(UL(s + 0x10)));
    SetWord(S() + 0x3E, static_cast<U>(e));
    SH_CALL(EffectKind6F_PushParty)(1);
    for (int i = 2; i != 0; --i) {
        s = S();
        SetUL(s + 0x18, UL(s + 0xC) + 0x20000u);
        s = S();
        SetUL(s + 0x34, Half(UL(s + 0x18) + UL(s + 0xC)));
        s = S();
        SetUL(s + 0x38, UL(s + 0x10));
        Kind6FSegments();
        s = S();
        SetUL(s + 0xC, UL(s + 0x18));
        s = S();
        SetUL(s + 0x18, UL(s + 0x18) + 0x20000u);
    }
    if (SH_CALL(Flags_Test)(Story(), 0x5D) == 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 3;
    }
}

// original 0x484930 (state 3): Cond_ByteFD 2 with flag 0x5C clear, then
// Cond_ByteFD 3 with flag 0x5D set (each tested): sound 0x206, +1 = 0.
extern "C" void __cdecl EffectKind6F_Watch(void) {
    if (Cond_ByteFD == 2 && SH_CALL(Flags_Test)(Story(), 0x5C) == 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 0;
    }
    if (Cond_ByteFD == 3 && SH_CALL(Flags_Test)(Story(), 0x5D) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        S()[1] = 0;
    }
}

// original 0x4849A0 (which, a byte): 0 - the four cells of kKind6FCellsZ along
// x 0x5C: the record's point +0x34 / +0x38 put at (0x5C0000, cell << 16) and
// Party_MemberAt(+0x34, +0x38, 0); a member there, with Field_ScriptFlags2 &
// 0x1400 clear and Field_Request 0, faces 7 (the record's x at or left of the
// member's) or 3 (ObjTrio +8) and Member_SetState2_8(member, 2). 1 - the
// cells of kKind6FCellsX along z 0x44 (+0x34 cell << 16, +0x38 0x440000), the
// facing 1 or 5 by z. Anything else: nothing. The original keeps each answer
// in its argument's low byte and hands Member_SetState2_8 the whole slot.
extern "C" void __cdecl EffectKind6F_PushParty(unsigned which) {
    U slot = which;
    const unsigned sel = which & 0xFFu;
    if (sel > 1) return;
    const U cells = sel == 0 ? at::kKind6FCellsZ : at::kKind6FCellsX;
    for (unsigned i = 0; i < 4; ++i) {
        const U cell = At(cells + i)[0];
        if (sel == 0) {
            SetUL(S() + 0x34, 0x5C0000);
            SetUL(S() + 0x38, cell << 16);
        } else {
            SetUL(S() + 0x34, cell << 16);
            SetUL(S() + 0x38, 0x440000);
        }
        unsigned char* const s = S();
        const unsigned char m = SH_CALL(Party_MemberAt)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38)), 0);
        slot = (slot & 0xFFFFFF00u) | m;
        if (m == 0xFF) continue;
        if ((Field_ScriptFlags2 & 0x1400u) != 0) continue;
        if (Field_Request != 0) continue;
        unsigned char* const rec = Member(slot & 0xFFu, "EffectKind6F_PushParty");
        if (sel == 0)
            rec[8] = S32(UL(S() + 0x34) - UL(rec + 0x34)) <= 0 ? 7 : 3;
        else
            rec[8] = S32(UL(S() + 0x38) - UL(rec + 0x38)) <= 0 ? 1 : 5;
        SH_CALL(Member_SetState2_8)(slot, 2);
    }
}

// original 0x484B10 (a, b: two points of three dwords): a draw mode (page
// (0x3C0, 0), dtd 1) linked at the record's point (+0x34, +0x38) with dy 1;
// EffectGte_LoadMapCamera; a LINE_F2 at the cursor, opaque, from a to b
// projected, kind 0x6F's colour, linked (.., 1, 0x20); the screen angle of the
// line Math_Ratan2(dy, dx) (each through _ftol, then a float); the two ends'
// sizes EffectGte_ProjectSize(end, {0x40, 0}, out) - out's dword plus
// Frame_Counter & 1 (read after each); EffectKind6F_DrawRibbon(the four screen
// words through _ftol, the sizes, the angle + 0x400 and + 0xC00); a closing
// draw mode (dtd 0) linked at the point.
extern "C" void __cdecl EffectKind6F_DrawSegment(const long* a, const long* b) {
    DrawMode(1, 0x3C0, 0, 1);
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    SH_CALL(EffectGte_ProjectPoint)(a, reinterpret_cast<float*>(prim + 8));
    SH_CALL(EffectGte_ProjectPoint)(b, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = Kind6FColour(0, "EffectKind6F_DrawSegment");
    prim[5] = Kind6FColour(1, "EffectKind6F_DrawSegment");
    prim[6] = Kind6FColour(2, "EffectKind6F_DrawSegment");
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x20);
    const U dx = FldSubFtol(prim + 0x14, prim + 8);
    float fx, fy;
    Fild(S32(dx), &fx);
    const U dy = FldSubFtol(prim + 0x18, prim + 0xC);
    Fild(S32(dy), &fy);
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(fy, fx));
    short size[2] = {0x40, 0};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(a, size, out);
    U wa;
    std::memcpy(&wa, out, 4);
    wa += Frame_Counter & 1u;
    SH_CALL(EffectGte_ProjectSize)(b, size, out);
    U wb;
    std::memcpy(&wb, out, 4);
    wb += Frame_Counter & 1u;
    const U by = FldFtol(prim + 0x18);
    const U bx = FldFtol(prim + 0x14);
    const U ay = FldFtol(prim + 0xC);
    const U ax = FldFtol(prim + 8);
    SH_CALL(EffectKind6F_DrawRibbon)(S32(ax), S32(ay), S32(wa), S32(angle + 0x400u), S32(bx), S32(by), S32(wb),
                                     S32(angle + 0xC00u));
    DrawMode(1, 0x3C0, 0, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
}

// original 0x484D00 (ax, ay, wa, ta, bx, by, wb, tb - every one read as its low
// s16, the angles masked to 16 bits): two POLY_G4s at the cursor (read once),
// semi-transparent, sharing the edge (ax, ay)-(bx, by) in kind 0x6F's colour;
// the far corners black at (Math_Cos, Math_Sin) * radius >> 12 off the ends -
// the first quad at ta round a (radius wa) and tb + 0x800 round b (wb), the
// second (a copy of the first's 0x44 bytes, linked tag and all) at ta + 0x800
// and tb. Each linked at the record's point (.., 1, 0x44); the second is the
// cursor read at entry + 0x44, wherever the link left the cursor.
extern "C" void __cdecl EffectKind6F_DrawRibbon(int ax, int ay, int wa, int ta, int bx, int by, int wb, int tb) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(q);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    const std::int32_t x0 = static_cast<short>(ax), y0 = static_cast<short>(ay);
    const std::int32_t x1 = static_cast<short>(bx), y1 = static_cast<short>(by);
    Fild(x0, q + 8);
    Fild(y0, q + 0xC);
    Fild(x1, q + 0x18);
    Fild(y1, q + 0x1C);
    const U t1 = static_cast<U>(ta) & 0xFFFFu;
    const std::int32_t r1 = static_cast<short>(wa);
    const std::int32_t r2 = static_cast<short>(wb);
    auto corner = [](int trig, std::int32_t radius, std::int32_t base) {
        return S32(Sar(Mul(static_cast<U>(trig), radius), 12) + static_cast<U>(base));
    };
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t1)), r1, x0), q + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t1)), r1, y0), q + 0x2C);
    const U t2 = static_cast<U>(tb) & 0xFFFFu;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2 + 0x800u)), r2, x1), q + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2 + 0x800u)), r2, y1), q + 0x3C);
    q[4] = Kind6FColour(0, "EffectKind6F_DrawRibbon");
    q[5] = Kind6FColour(1, "EffectKind6F_DrawRibbon");
    q[6] = Kind6FColour(2, "EffectKind6F_DrawRibbon");
    q[0x14] = Kind6FColour(0, "EffectKind6F_DrawRibbon");
    q[0x15] = Kind6FColour(1, "EffectKind6F_DrawRibbon");
    q[0x16] = Kind6FColour(2, "EffectKind6F_DrawRibbon");
    q[0x24] = q[0x25] = q[0x26] = 0;
    q[0x34] = q[0x35] = q[0x36] = 0;
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x44);
    unsigned char* const q2 = q + 0x44;
    std::memcpy(q2, q, 0x44);
    const U t3 = t1 + 0x800u;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t3)), r1, x0), q2 + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t3)), r1, y0), q2 + 0x2C);
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2)), r2, x1), q2 + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2)), r2, y1), q2 + 0x3C);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x44);
}

// ===========================================================================
// Kind 0x72: Effect_KindHandlers[0x72] (0x655518), EffectKind72_States (three)
// ===========================================================================

// original 0x484F50 (Effect_KindHandlers[0x72], hidden in 0x484D00):
// jmp [EffectKind72_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind72_Run(void) {
    Dispatch("EffectKind72_Run", AddressOf(EffectKind72_States), EffectKind72_States_count, 1);
}

// original 0x484F70 (state 0; EffectKind1F_Start's twin): the 32 debris
// records of 0x2C at EffectKind30_Shards set up (EffectDebris_InitOne); +9 0,
// +1 up.
extern "C" void __cdecl EffectKind72_Start(void) {
    for (unsigned i = 0; i < at::kDebrisCount; ++i) SH_CALL(EffectDebris_InitOne)(At(Shards() + at::kDebrisStride * i));
    S()[9] = 0;
    Step(1);
}

// original 0x484FA0 (state 1; EffectKind1F_Debris's twin): a draw mode (page
// (0x380, 0x100), dtd 0) committed (Gfx_CommitPrim(1, 0xC));
// EffectGte_LoadMapCamera; each of the 32 debris drawn (EffectDebris_Draw),
// its angle +0x24 up 0x10, its shade +0x2A up 0x20 while the record's +9
// (Sprite_Current read after the draw) is below 4, else down 2; the last
// Sprite_Current read's +9 up 1; +9 (read again) above 0x44: +1 up.
extern "C" void __cdecl EffectKind72_Debris(void) {
    DrawMode(1, 0x380, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* last = nullptr;
    for (unsigned i = 0; i < at::kDebrisCount; ++i) {
        unsigned char* const d = At(Shards() + at::kDebrisStride * i);
        SH_CALL(EffectDebris_Draw)(d);
        last = S();
        SetWord(d + 0x24, Word(d + 0x24) + 0x10u);
        SetWord(d + 0x2A, Word(d + 0x2A) + (last[9] < 4 ? 0x20u : 0xFFFEu));
    }
    last[9] = static_cast<unsigned char>(last[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 0x44) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x485030 (a 0x2C-byte debris record; kinds 0x1E, 0x1F, 0x4B and 0x72
// draw with it): a POLY_G3 at the cursor (read once), semi-transparent; its
// first corner the record's point (+0..+0xB) projected; the other two the
// edges +0x10 / +0x18 (three s16 each) turned in x / z by the angle +0x24
// (Math_Cos / Math_Sin, >> 8) and scaled by +0x28, the height (+0x14 / +0x1C)
// << 12 times the scale, added to the point and projected. Colour (v, v, v >>
// 1) at the first corner, v the shade +0x2A clamped to 0..0xFF; black at the
// others. Gfx_CommitPrim(1, 0x34).
extern "C" void __cdecl EffectDebris_Draw(unsigned char* d) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(d), reinterpret_cast<float*>(prim + 8));
    for (U edge = 0x10, out = 0x18; edge <= 0x18; edge += 8, out += 0x10) {
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        U e = Mul(c1, SW(d + edge));
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        const std::int32_t scale = SW(d + 0x28);
        e = Mul(Sar(e - Mul(s1, SW(d + edge + 2)), 8), scale);
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        U f = Mul(s2, SW(d + edge));
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        f += Mul(c2, SW(d + edge + 2));
        long p[3];
        p[0] = static_cast<long>(e + UL(d + 0));
        p[2] = static_cast<long>(Mul(static_cast<U>(SW(d + edge + 4)) << 12, scale) + UL(d + 8));
        p[1] = static_cast<long>(Mul(Sar(f, 8), scale) + UL(d + 4));
        SH_CALL(EffectGte_ProjectPoint)(p, reinterpret_cast<float*>(prim + out));
    }
    const std::int32_t shade = SW(d + 0x2A);
    const unsigned char v = shade < 0 ? 0 : shade > 0xFF ? 0xFF : d[0x2A];
    prim[4] = v;
    prim[5] = v;
    prim[6] = static_cast<unsigned char>(v >> 1);
    prim[0x14] = prim[0x15] = prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
}

// original 0x4851E0 (a 0x2C-byte debris record; kinds 0x1F and 0x72 set theirs
// up with it - EffectKind1E_DebrisInitOne's twin with 0x20 and 8 + Rand % 8):
// its point +0 / +4 / +8 Sprite_Current's +0x34 / +0x38 / +0x3C; three angles
// Rand & 0xFFF, Rand & 0x3FF, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of
// 0x20; of -0x20; z 0) at +0x10 and +0x18 turned in place by a matrix of the
// three angles (EffectGte_SetDiagonalOne, Gte_RotMatrixX, _Y by the second
// negated, _Z; 0x5A7C70 for each edge - no push or pop of the GTE's matrix,
// unlike kind 0x1E's); the scale +0x28 8 + Rand & 7; +0x20, +0x22, +0x24 (its
// angle), +0x2A (its shade) 0.
extern "C" void __cdecl EffectDebris_InitOne(unsigned char* d) {
    SetUL(d + 0, UL(S() + 0x34));
    SetUL(d + 4, UL(S() + 0x38));
    SetUL(d + 8, UL(S() + 0x3C));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = static_cast<U>(SH_CALL(Rand)()) & 0x3FFu;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(d + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x20)));
    SetWord(d + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x20)));
    SetWord(d + 0x14, 0);
    SetWord(d + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x20)));
    SetWord(d + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x20)));
    Matrix m;
    SetWord(d + 0x1C, 0);
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(d + 0x10), reinterpret_cast<short*>(d + 0x10));
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(d + 0x18), reinterpret_cast<short*>(d + 0x18));
    SetWord(d + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 7u) + 8u);
    SetWord(d + 0x20, 0);
    SetWord(d + 0x22, 0);
    SetWord(d + 0x24, 0);
    SetWord(d + 0x2A, 0);
}

// ===========================================================================
// Kind 0x73: Effect_KindHandlers[0x73] (0x65551C), EffectKind73_States (two)
// by +1, each a sub-state dispatcher by +2
// ===========================================================================

// original 0x4852F0 (Effect_KindHandlers[0x73], hidden in 0x4851E0):
// jmp [EffectKind73_States + +1 * 4], unbounded. +1 is the variant area 132
// spawns (0 alone, 1 the pair).
extern "C" void __cdecl EffectKind73_Run(void) {
    Dispatch("EffectKind73_Run", AddressOf(EffectKind73_States), EffectKind73_States_count, 1);
}

// original 0x485310 (variant 0): jmp [EffectKind73A_States + +2 * 4].
extern "C" void __cdecl EffectKind73_RunA(void) {
    Dispatch("EffectKind73_RunA", AddressOf(EffectKind73A_States), EffectKind73A_States_count, 2);
}

// original 0x485330 (variant 0, sub-state 0): the sparks cleared; +9 = 0x40,
// +2 up, sound 0x20C.
extern "C" void __cdecl EffectKind73A_Start(void) {
    SH_CALL(EffectKind73_ClearSparks)();
    S()[9] = 0x40;
    Step(2);
    SH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x485360 (variant 0, sub-state 1): every fourth count (+9 & 3 0) a
// free spark set up at the record's point; the sparks moved and drawn
// (EffectKind73_MoveSparks, its answer unread); +9 down, at 0 +2 up.
extern "C" void __cdecl EffectKind73A_Emit(void) {
    if ((S()[9] & 3u) == 0) {
        unsigned char* const r = SH_CALL(EffectKind73_FindSpark)();
        if (r != nullptr) SH_CALL(EffectKind73_SparkInit)(r);
    }
    SH_CALL(EffectKind73_MoveSparks)();
    if (CountDown()) Step(2);
}

// original 0x4853A0 (variant 0, sub-state 2) and 0x485430 (variant 1,
// sub-state 1): EffectKind73_MoveSparks; none live (al 0): a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind73A_Fade(void) {
    if (SH_CALL(EffectKind73_MoveSparks)() == 0) SH_CALL(Effect_Release)();
}
extern "C" void __cdecl EffectKind73B_Fade(void) {
    if (SH_CALL(EffectKind73_MoveSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4853B0 (variant 1): jmp [EffectKind73B_States + +2 * 4].
extern "C" void __cdecl EffectKind73_RunB(void) {
    Dispatch("EffectKind73_RunB", AddressOf(EffectKind73B_States), EffectKind73B_States_count, 2);
}

// original 0x4853D0 (variant 1, sub-state 0): +0xB set - the sparks cleared
// first (the pair's first record); sixteen free sparks set up at the record's
// point, each +0x14 0x80 and its angle byte +0x17 its number 0..15 (a number
// is spent when none is free); sound 0x206, +2 up.
extern "C" void __cdecl EffectKind73B_Burst(void) {
    if (S()[0xB] != 0) SH_CALL(EffectKind73_ClearSparks)();
    for (unsigned n = 0; n < 0x10; ++n) {
        unsigned char* const r = SH_CALL(EffectKind73_FindSpark)();
        if (r == nullptr) continue;
        SH_CALL(EffectKind73_SparkInit)(r);
        SetWord(r + 0x14, 0x80);
        r[0x17] = static_cast<unsigned char>(n);
    }
    SH_CALL(Sound_PlayEffect)(0x206);
    Step(2);
}

// original 0x4857C0 (a 0x18-byte spark): at Sprite_Current's point (+4 / +8 /
// +0xC its +0x34 / +0x38 / +0x3C); +0 1, +1 0, +0x14 (s16) 0x80, +3 0x40,
// +0x16 7 (all three colour bits), +2 0x20 (the life).
extern "C" void __cdecl EffectKind73_SparkInit(unsigned char* r) {
    SetUL(r + 4, UL(S() + 0x34));
    SetUL(r + 8, UL(S() + 0x38));
    const U y = UL(S() + 0x3C);
    r[0] = 1;
    SetUL(r + 0xC, y);
    r[1] = 0;
    SetWord(r + 0x14, 0x80);
    r[3] = 0x40;
    r[0x16] = 7;
    r[2] = 0x20;
}

// original 0x485810: the first of the 32 sparks of 0x18 at EffectKind30_Shards
// whose +0 is 0, or null; the cursor kSparkCursor left at it (or past the
// last).
extern "C" unsigned char* __cdecl EffectKind73_FindSpark(void) {
    U p = Shards();
    SetCell(at::kSparkCursor, p);
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        if (At(p)[0] == 0) return At(p);
        p += at::kSparkStride;
        SetCell(at::kSparkCursor, p);
    }
    return nullptr;
}

// original 0x485840: +0 = 0 in the 32 sparks, through the cursor.
extern "C" void __cdecl EffectKind73_ClearSparks(void) {
    SetCell(at::kSparkCursor, Shards());
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        At(Cell(at::kSparkCursor))[0] = 0;
        SetCell(at::kSparkCursor, Cell(at::kSparkCursor) + at::kSparkStride);
    }
}

// original 0x485870: EffectGte_LoadMapCamera; the 32 sparks stepped with the
// cursor: each live one +4 += 0x2000; with the record's +1 0 grown (+0x14 +=
// 8), else spread by its angle byte (+4 += Math_Cos(+0x17 << 8), +0xC +=
// Math_Sin(..) << 8); its shade +3 -= 2, its life +2 down (0 frees it) and drawn
// (EffectKind73_DrawSpark). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind73_MoveSparks(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    U c = Shards();
    SetCell(at::kSparkCursor, c);
    unsigned char any = 0;
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        unsigned char* const r = At(c);
        if (r[0] != 0) {
            SetUL(r + 4, UL(r + 4) + 0x2000u);
            if (S()[1] == 0) {
                unsigned char* const cur = At(Cell(at::kSparkCursor));
                SetWord(cur + 0x14, Word(cur + 0x14) + 8u);
            } else {
                unsigned char* cur = At(Cell(at::kSparkCursor));
                unsigned char* p = cur + 4;
                U v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(static_cast<U>(cur[0x17]) << 8)));
                SetUL(p, UL(p) + v);
                cur = At(Cell(at::kSparkCursor));
                p = cur + 0xC;
                v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(static_cast<U>(cur[0x17]) << 8)));
                SetUL(p, UL(p) + (v << 8));
            }
            unsigned char* cur = At(Cell(at::kSparkCursor));
            cur[3] = static_cast<unsigned char>(cur[3] + 0xFE);
            cur = At(Cell(at::kSparkCursor));
            cur[2] = static_cast<unsigned char>(cur[2] - 1);
            cur = At(Cell(at::kSparkCursor));
            if (cur[2] == 0) {
                cur[0] = 0;
                cur = At(Cell(at::kSparkCursor));
            }
            SH_CALL(EffectKind73_DrawSpark)(cur);
            c = Cell(at::kSparkCursor);
            any = 1;
        }
        c += at::kSparkStride;
        SetCell(at::kSparkCursor, c);
    }
    return any;
}

// original 0x485960 (a 0x18-byte spark): a draw mode (page (0x3C0, 0) abr 2,
// dtd 0) linked at the spark's x, z (+4, +8; dy 3, 0xC); a POLY_FT4 at the
// cursor, semi-transparent: its centre the point +4 projected, its size +0x14
// (both axes) scaled at its depth in place (EffectGte_ProjectSize, size and
// out one local); the corners x - (w >> 1) and + w, y - (h >> 1) and + h on
// the x87 (fild, fsubr, fiadd); the depth to +0x10..+0x40 by mov; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 2, 0x2C0, 0x100); colour +3 in each channel whose bit of
// +0x16 is set (4 red, 2 green, 1 blue); linked (+4, +8, 3, 0x48).
extern "C" void __cdecl EffectKind73_DrawSpark(unsigned char* r) {
    DrawMode(2, 0x3C0, 0, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(r + 4), UL(r + 8), 3, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(r + 4);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {static_cast<short>(Word(r + 0x14)), static_cast<short>(Word(r + 0x14))};
    SH_CALL(EffectGte_ProjectSize)(point, wh, wh);
    const std::int32_t w = wh[0], h = wh[1];
    const std::int32_t hw = static_cast<short>(wh[0] >> 1), hh = static_cast<short>(wh[1] >> 1);
    FildSubr(hw, &screen[0], prim + 8);
    FildSubr(hh, &screen[1], prim + 0xC);
    FildSubrAdd(hw, &screen[0], w, prim + 0x18);
    FildSubr(hh, &screen[1], prim + 0x1C);
    FildSubr(hw, &screen[0], prim + 0x28);
    FildSubrAdd(hh, &screen[1], h, prim + 0x2C);
    FildSubrAdd(hw, &screen[0], w, prim + 0x38);
    FildSubrAdd(hh, &screen[1], h, prim + 0x3C);
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    U depth;
    std::memcpy(&depth, &screen[2], 4);
    SetUL(prim + 0x40, depth);
    SetUL(prim + 0x30, depth);
    SetUL(prim + 0x20, depth);
    SetUL(prim + 0x10, depth);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 2, 0x2C0, 0x100));
    const unsigned char bits = r[0x16];
    const unsigned char red = (bits & 4) ? r[3] : 0;
    const unsigned char green = (bits & 2) ? r[3] : 0;
    const unsigned char blue = (bits & 1) ? r[3] : 0;
    prim[4] = red;
    prim[5] = green;
    prim[6] = blue;
    SH_CALL(MapView_LinkPrimAt)(UL(r + 4), UL(r + 8), 3, 0x48);
}

// ===========================================================================
// Kind 0x74: Effect_KindHandlers[0x74] (0x655520), EffectKind74_States (three)
// ===========================================================================

// original 0x485440 (Effect_KindHandlers[0x74], hidden in 0x4851E0):
// jmp [EffectKind74_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind74_Run(void) {
    Dispatch("EffectKind74_Run", AddressOf(EffectKind74_States), EffectKind74_States_count, 1);
}

// original 0x485460 (state 0): the column's foot +0xC / +0x10 / +0x14 the
// record's point +0x34 / +0x38 / +0x3C, its top +0x1C the foot, its angle
// +0x20 (s16) 0, its width +0x24 0x1000000; +9 = 0x40, +1 up, sound 0x207.
extern "C" void __cdecl EffectKind74_Start(void) {
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0x34));
    s = S();
    SetUL(s + 0x10, UL(s + 0x38));
    s = S();
    SetUL(s + 0x14, UL(s + 0x3C));
    s = S();
    SetUL(s + 0x1C, UL(s + 0x14));
    SetWord(S() + 0x20, 0);
    SetUL(S() + 0x24, 0x1000000);
    S()[9] = 0x40;
    Step(1);
    SH_CALL(Sound_PlayEffect)(0x207);
}

// original 0x4854D0 (state 1): the top +0x1C up 0x1000000, no higher than the
// foot + 0x8000000; the angle +0x20 turned by 0xFE00; drawn
// (EffectKind74_Draw); +9 down, at 0 +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind74_Rise(void) {
    unsigned char* s = S();
    SetUL(s + 0x1C, UL(s + 0x1C) + 0x1000000u);
    s = S();
    const U cap = UL(s + 0x14) + 0x8000000u;
    if (S32(UL(s + 0x1C)) > S32(cap)) {
        SetUL(s + 0x1C, cap);
        s = S();
    }
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    SH_CALL(EffectKind74_Draw)();
    if (CountDown()) {
        S()[9] = 0x10;
        Step(1);
    }
}

// original 0x485530 (state 2): +9 down; at 0 a tail jump to Effect_Release;
// else the angle turned by 0xFE00, the width +0x24 down 0x100000 and a tail
// jump to EffectKind74_Draw.
extern "C" void __cdecl EffectKind74_Fade(void) {
    if (CountDown()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* s = S();
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    s = S();
    SetUL(s + 0x24, UL(s + 0x24) + 0xFFF00000u);
    SH_CALL(EffectKind74_Draw)();
}

// original 0x485570: EffectGte_LoadMapCamera; the column from the foot's
// height (+0x14) up to the top (+0x1C) + 0x1000000 in steps of 0x100000, its
// edge a point at the angle (+0x20, up 0x100 a step) 32 * (Math_Cos,
// Math_Sin) round the foot (+0xC, +0x10 - read once, at entry); each step's
// two heights (the step less the width +0x24, no lower than the foot; the
// step, no higher than the top - +0x14, +0x24, +0x1C read afresh) projected,
// and a POLY_FT4 (opaque, grey 0x80, u 0x1F / 0, v 0xFF / 0xE0, the CLUT
// Gpu_GetClut(0, 0x1E5), the page Gpu_GetTPage(1, 0, 0x1C0, 0x100)) from the
// previous step's two projections to this one's, linked at the edge point
// (dy 2, 0x48).
extern "C" void __cdecl EffectKind74_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    U angle = Word(s + 0x20);
    const U x = UL(s + 0xC), z = UL(s + 0x10);
    U y = UL(s + 0x14);
    long pt[3];
    float low[3], high[3];
    auto project = [&](U a) {
        pt[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) << 5) + x);
        pt[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) << 5) + z);
        unsigned char* c = S();
        U h = y - UL(c + 0x24);
        if (S32(h) < S32(UL(c + 0x14))) h = UL(c + 0x14);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, low);
        c = S();
        h = y;
        if (S32(y) > S32(UL(c + 0x1C))) h = UL(c + 0x1C);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, high);
    };
    project(angle);
    s = S();
    if (S32(y) >= S32(UL(s + 0x1C) + 0x1000000u)) return;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        angle += 0x100;
        y += 0x100000;
        std::memcpy(prim + 0x08, low, 12);
        std::memcpy(prim + 0x18, high, 12);
        project(angle & 0xFFFFu);
        std::memcpy(prim + 0x28, low, 12);
        std::memcpy(prim + 0x38, high, 12);
        prim[0x14] = 0x1F;
        prim[0x15] = 0xFF;
        prim[0x24] = 0x1F;
        prim[0x25] = 0xE0;
        prim[0x34] = 0;
        prim[0x35] = 0xFF;
        prim[0x44] = 0;
        prim[0x45] = 0xE0;
        prim[4] = prim[5] = prim[6] = 0x80;
        SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(1, 0, 0x1C0, 0x100));
        SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0, 0x1E5));
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(pt[0]), static_cast<unsigned long>(pt[1]), 2, 0x48);
        s = S();
    } while (S32(y) < S32(UL(s + 0x1C) + 0x1000000u));
}

// ===========================================================================
// Kind 0x75: Effect_KindHandlers[0x75] (0x655524), EffectKind75_States (three)
// ===========================================================================

// original 0x485BA0 (Effect_KindHandlers[0x75], hidden in 0x485960):
// jmp [EffectKind75_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind75_Run(void) {
    Dispatch("EffectKind75_Run", AddressOf(EffectKind75_States), EffectKind75_States_count, 1);
}

// original 0x485BC0 (state 0): the tile's colour +0x5D..+0x5F 0, +9 = 0xF,
// +1 = 1.
extern "C" void __cdecl EffectKind75_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0xF;
    S()[1] = 1;
}

// original 0x485C00 (state 1): +9 down; not 0: each colour byte up 7; at 0,
// +1 = 2. Then the screen tile (E4D's 0x48CA90) and a tail jump to
// EffectKind75_MarkSprites.
extern "C" void __cdecl EffectKind75_Brighten(void) {
    if (!CountDown()) {
        unsigned char* s = S();
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 7);
        s = S();
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + 7);
        s = S();
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + 7);
    } else {
        S()[1] = 2;
    }
    SH_AT(VoidFn, at::kScreenTile)();
    SH_CALL(EffectKind75_MarkSprites)();
}

// original 0x485C50 (state 2): the screen tile and a tail jump to
// EffectKind75_MarkSprites.
extern "C" void __cdecl EffectKind75_Hold(void) {
    SH_AT(VoidFn, at::kScreenTile)();
    SH_CALL(EffectKind75_MarkSprites)();
}

// original 0x485C60 (a start the cut does not list: kind 0x75's two states
// jump to it; its own frame and ret): Sprite_Current kept; each of the thirty
// Sprite_Objects records made current in turn, and one in use (+0 bit 0) at
// pose 6 (+6) given +0x29 = 5 and drawn again (Sprite_UpdateScreen) - over the
// tile; Sprite_Current put back.
extern "C" void __cdecl EffectKind75_MarkSprites(void) {
    unsigned char* const saved = Sprite_Current;
    unsigned char* o = Sprite_Objects;
    for (unsigned n = at::kSpriteCount; n != 0; --n, o += at::kSpriteStride) {
        Sprite_Current = o;
        if ((o[0] & 1) != 0 && o[6] == 6) {
            o[0x29] = 5;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = saved;
}

void Effect3C_Inject() {
    if (bof3::WantsShadow("effect_3c")) effect_3c::SelfTest();
    BOF3_INJECT(EffectKind6E_Run);
    BOF3_INJECT(EffectKind6E_Start);
    BOF3_INJECT(EffectKind6E_Emit);
    BOF3_INJECT(EffectKind6E_EmitSlower);
    BOF3_INJECT(EffectKind6E_Fade);
    BOF3_INJECT(EffectKind6E_ShardInit);
    BOF3_INJECT(EffectKind6E_FindShard);
    BOF3_INJECT(EffectKind6E_DrawShards);
    BOF3_INJECT(EffectKind6D_Run);
    BOF3_INJECT(EffectKind6D_Start);
    BOF3_INJECT(EffectKind6D_Move);
    BOF3_INJECT(EffectKind6D_ClearParticles);
    BOF3_INJECT(EffectKind6D_MoveParticles);
    BOF3_INJECT(EffectKind6D_DrawParticle);
    BOF3_INJECT(EffectKind6D_InitParticles);
    BOF3_INJECT(EffectKind6F_Run);
    BOF3_INJECT(EffectKind6F_Start);
    BOF3_INJECT(EffectKind6F_AlongZ);
    BOF3_INJECT(EffectKind6F_AlongX);
    BOF3_INJECT(EffectKind6F_Watch);
    BOF3_INJECT(EffectKind6F_PushParty);
    BOF3_INJECT(EffectKind6F_DrawSegment);
    BOF3_INJECT(EffectKind6F_DrawRibbon);
    BOF3_INJECT(EffectKind72_Run);
    BOF3_INJECT(EffectKind72_Start);
    BOF3_INJECT(EffectKind72_Debris);
    BOF3_INJECT(EffectDebris_Draw);
    BOF3_INJECT(EffectDebris_InitOne);
    BOF3_INJECT(EffectKind73_Run);
    BOF3_INJECT(EffectKind73_RunA);
    BOF3_INJECT(EffectKind73A_Start);
    BOF3_INJECT(EffectKind73A_Emit);
    BOF3_INJECT(EffectKind73A_Fade);
    BOF3_INJECT(EffectKind73_RunB);
    BOF3_INJECT(EffectKind73B_Burst);
    BOF3_INJECT(EffectKind73B_Fade);
    BOF3_INJECT(EffectKind74_Run);
    BOF3_INJECT(EffectKind74_Start);
    BOF3_INJECT(EffectKind74_Rise);
    BOF3_INJECT(EffectKind74_Fade);
    BOF3_INJECT(EffectKind74_Draw);
    BOF3_INJECT(EffectKind73_SparkInit);
    BOF3_INJECT(EffectKind73_FindSpark);
    BOF3_INJECT(EffectKind73_ClearSparks);
    BOF3_INJECT(EffectKind73_MoveSparks);
    BOF3_INJECT(EffectKind73_DrawSpark);
    BOF3_INJECT(EffectKind75_Run);
    BOF3_INJECT(EffectKind75_Start);
    BOF3_INJECT(EffectKind75_Brighten);
    BOF3_INJECT(EffectKind75_Hold);
    BOF3_INJECT(EffectKind75_MarkSprites);
}
