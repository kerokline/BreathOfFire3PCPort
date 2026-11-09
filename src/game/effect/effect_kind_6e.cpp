// Moved here by tools/move_funcs.py from effect_3b.cpp, effect_3c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_6e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_6e_callees.h"
#include "game/effect_3b.h"
#include "game/effect_3b_callees.h"
#include "game/effect_3c.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_6e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul (32 bits kept)
U Mul(int a, U b) { return static_cast<U>(a) * b; }

// `fild dword [h]; fsubr dword [c]; fstp dword [o]`: c - h
void FildSubr(U h, const void* c, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(h);
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(i) : "st", "memory");
}

// `fild dword [h]; fsubr dword [c]; fiadd dword [w]; fstp dword [o]`: c - h + w
void FildSubrIadd(U h, const void* c, U w, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(h), j = static_cast<std::int32_t>(w);
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfiaddl %3\n\tfstps (%0)"
                     :
                     : "r"(o), "r"(c), "m"(i), "m"(j)
                     : "st", "memory");
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp / call
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original goes
// through the dword after - the next table.
void Through(const char* who, U table, unsigned entries, unsigned index, unsigned byte) {
    if (index >= entries)
        bof3::Fatal("%s: +%u is %u, past the %u entries of 0x%X - the original goes through the dword after "
                    "(docs/effect_3b.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * index)))();
}

unsigned char* Spark() { return reinterpret_cast<unsigned char*>(Effect3c_ShardCursor); }

using RecordFn = void (__cdecl*)(unsigned char*);

using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

U Shards() { return AddressOf(EffectKind30_Shards); }

// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}

// `fld dword [a]; fst dword [o1]; fstp dword [o2]` (a copy through the FPU,
// which turns a signalling NaN quiet)
void FldCopy2(const void* a, void* o1, void* o2) {
    __asm__ volatile("flds (%0)\n\tfsts (%1)\n\tfstps (%2)" : : "r"(a), "r"(o1), "r"(o2) : "st", "memory");
}

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

}  // namespace

// original 0x483C10 (cdecl, no arguments; state 0's, and E3C's 0x484070): the
// cursor 0x67626C at EffectKind30_Shards, then for sixteen records of 0x28 (the
// cursor read again for every access, stepped in memory): +4 / +8 / +0xC
// Sprite_Current's +0x34 / +0x38 / +0x3C; a distance (Rand() & 0x7F) << 2 and an
// angle (Rand() & 0x3FF) + 0x200; +0x14 = cos * distance sar 7, +0x18 = sin *
// distance sar 7; +0 = 1, +1 = 0, the word +0x24 = 0, +3 = 0x40, +0x26 = 7, +2 =
// 4.
extern "C" void __cdecl EffectKind6C_ScatterSparks(void) {
    Effect3c_ShardCursor = ::bof3::Img(EffectKind30_Shards_addr);
    for (unsigned left = effect_3b::at::kSparkCount; left != 0; --left) {
        SetUL(Spark() + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
        SetUL(Spark() + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
        SetUL(Spark() + 0xC, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
        const U distance = (static_cast<U>(SH_CALL(Rand)()) & 0x7F) << 2;
        const U angle = ((static_cast<U>(SH_CALL(Rand)()) & 0x3FF) + 0x200) & 0xFFFF;
        // the cursor read after each call (kept out of the call's expression:
        // C++ leaves the order of an expression's operands open)
        const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
        SetUL(Spark() + 0x14, Sar(Mul(c, distance), 7));
        const int n = SH_CALL(Math_Sin)(static_cast<int>(angle));
        SetUL(Spark() + 0x18, Sar(Mul(n, distance), 7));
        Spark()[0] = 1;
        Spark()[1] = 0;
        SetWord(Spark() + 0x24, 0);
        Spark()[3] = 0x40;
        Spark()[0x26] = 7;
        Spark()[2] = 4;
        Effect3c_ShardCursor = Effect3c_ShardCursor + effect_3b::at::kSparkStride;
    }
}

// original 0x483D10 (state 1's; answers al): the map camera
// (EffectGte_LoadMapCamera), a draw mode (page Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 0) in slot 2; for the sixteen spark records (the cursor 0x67626C stepped
// in memory, a register copy tested) each live one (+0 not 0): its state
// through EffectKind6C_SparkStates by +1 (a call, unbounded), its quad
// (EffectKind6C_DrawSpark of the cursor, read again), the cursor read back;
// al 1 when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind6C_DrawSparks(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char any = 0;
    U c = ::bof3::Img(EffectKind30_Shards_addr);
    Effect3c_ShardCursor = c;
    for (unsigned n = effect_3b::at::kSparkCount; n != 0; --n) {
        if (At(c)[0] != 0) {
            Through("EffectKind6C_DrawSparks", AddressOf(EffectKind6C_SparkStates), EffectKind6C_SparkStates_count,
                    At(c)[1], 1);
            SH_CALL(EffectKind6C_DrawSpark)(Spark());
            c = Effect3c_ShardCursor;
            any = 1;
        }
        c += effect_3b::at::kSparkStride;
        Effect3c_ShardCursor = c;
    }
    return any;
}

// original 0x483DA0 (cdecl; called by 0x483D10 and E3C's 0x4841E0): a spark's
// semi-transparent POLY_FT4 at the packet cursor (read once): its point +4
// projected (EffectGte_ProjectPoint, x / y / depth), its size the word +0x24
// handed twice as (w, h) and scaled at that depth into the same two words
// (EffectGte_ProjectSize); corners x - (w sar 1) and that + w, y - (h sar 1) and
// that + h (fild, fsubr, fiadd); the depth copied to +0x10 / +0x20 / +0x30 /
// +0x40; u 0xE0 / 0xFF, v 0x30 / 0x4F; CLUT Gpu_GetClut(0x50, 0x1E3), page
// Gpu_GetTPage(0, 1, 0x2C0, 0x100); red / green / blue the spark's +3 where
// +0x26 has bit 2 / 1 / 0 (read after the page), else 0; slot 2, 0x48.
extern "C" void __cdecl EffectKind6C_DrawSpark(unsigned char* spark) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char screen[12];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(spark + 4), reinterpret_cast<float*>(screen));
    short size[2];
    size[0] = static_cast<short>(Word(spark + 0x24));
    size[1] = size[0];
    SH_CALL(EffectGte_ProjectSize)(reinterpret_cast<const long*>(spark + 4), size, size);
    const U w = S16(static_cast<unsigned short>(size[0])), h = S16(static_cast<unsigned short>(size[1]));
    const U hw = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(size[0]) >> 1));
    const U hh = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(size[1]) >> 1));
    FildSubr(hw, screen, p + 8);
    FildSubr(hh, screen + 4, p + 0xC);
    FildSubrIadd(hw, screen, w, p + 0x18);
    FildSubr(hh, screen + 4, p + 0x1C);
    FildSubr(hw, screen, p + 0x28);
    FildSubrIadd(hh, screen + 4, h, p + 0x2C);
    FildSubrIadd(hw, screen, w, p + 0x38);
    p[0x15] = 0x30;
    p[0x25] = 0x30;
    p[0x14] = 0xE0;
    p[0x24] = 0xFF;
    p[0x34] = 0xE0;
    p[0x35] = 0x4F;
    p[0x44] = 0xFF;
    p[0x45] = 0x4F;
    FildSubrIadd(hh, screen + 4, h, p + 0x3C);
    std::memcpy(p + 0x40, screen + 8, 4);
    std::memcpy(p + 0x30, screen + 8, 4);
    std::memcpy(p + 0x20, screen + 8, 4);
    std::memcpy(p + 0x10, screen + 8, 4);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0x50, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    const unsigned char bits = spark[0x26];
    p[4] = (bits & 4) ? spark[3] : 0;
    p[5] = (bits & 2) ? spark[3] : 0;
    p[6] = (bits & 1) ? spark[3] : 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x483FA0 (EffectKind6C_SparkStates[0]; the spark at the cursor): +4
// += +0x14 << 1, +8 += +0x18 << 1, the word +0x24 up 0x80; +2 down, at 0 +2 =
// 0x40 and +1 up.
extern "C" void __cdecl EffectKind6C_SparkFly(void) {
    SetUL(Spark() + 4, UL(Spark() + 4) + (UL(Spark() + 0x14) << 1));
    SetUL(Spark() + 8, UL(Spark() + 8) + (UL(Spark() + 0x18) << 1));
    SetWord(Spark() + 0x24, Word(Spark() + 0x24) + 0x80);
    Spark()[2] = static_cast<unsigned char>(Spark()[2] - 1);
    if (Spark()[2] != 0) return;
    Spark()[2] = 0x40;
    Spark()[1] = static_cast<unsigned char>(Spark()[1] + 1);
}

// original 0x484000 (EffectKind6C_SparkStates[1]): +4 += +0x14, +8 += +0x18,
// +3 down, +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectKind6C_SparkFade(void) {
    SetUL(Spark() + 4, UL(Spark() + 4) + UL(Spark() + 0x14));
    SetUL(Spark() + 8, UL(Spark() + 8) + UL(Spark() + 0x18));
    Spark()[3] = static_cast<unsigned char>(Spark()[3] - 1);
    Spark()[2] = static_cast<unsigned char>(Spark()[2] - 1);
    if (Spark()[2] == 0) Spark()[0] = 0;
}

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
    SH_AT(VoidFn, at::kShardsSpread.v)();
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
        Effect3c_ShardCursor = AddressOf(r);
        if (r != nullptr) {
            SH_CALL(EffectKind6E_ShardInit)(r);
            SetUL(At(Effect3c_ShardCursor) + 0x1C, 0x200000);
        }
    }
    if (CountDown()) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x200000);
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
        Effect3c_ShardCursor = AddressOf(r);
        if (r != nullptr) {
            SH_CALL(EffectKind6E_ShardInit)(r);
            unsigned char* s = S();
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 0xFFFF0000u);
            s = S();
            SetUL(At(Effect3c_ShardCursor) + 0x1C, rv::As<records::EffectWork>(s)->work_0c);
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
    SetUL(shard + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(shard + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    const U y = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
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
    Effect3c_ShardCursor = c;
    unsigned char any = 0;
    for (unsigned n = at::kShardCount; n != 0; --n) {
        unsigned char* const r = At(c);
        if (r[0] != 0) {
            SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
            unsigned char* cur = At(Effect3c_ShardCursor);
            SetWord(cur + 0x24, Word(cur + 0x24) + 8u);
            cur = At(Effect3c_ShardCursor);
            cur[3] = static_cast<unsigned char>(cur[3] + 0xFA);
            cur = At(Effect3c_ShardCursor);
            cur[2] = static_cast<unsigned char>(cur[2] - 1);
            cur = At(Effect3c_ShardCursor);
            if (cur[2] == 0) {
                cur[0] = 0;
                cur = At(Effect3c_ShardCursor);
            }
            SH_AT(RecordFn, at::kShardQuad.v)(cur);
            c = Effect3c_ShardCursor;
            any = 1;
        }
        c += at::kShardStride;
        Effect3c_ShardCursor = c;
    }
    return any;
}

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
    U p = ::bof3::Img(at::kParticles.v) + at::kParticleBlock * S()[6];
    for (unsigned n = at::kParticleCount; n != 0; --n, p += at::kParticleStride) At(p)[0] = 0;
}

// original 0x484320: EffectGte_LoadMapCamera; each live particle of the block
// (+6 read once) moved: +0x18 += +8, +0x1C += +0xC, +0x20 += +0x10, then +0x10
// -= 0x40000 (it falls), +3 -= 4, its shape +4 = Rand & 7, its life +2 down (0
// frees it) and drawn (EffectKind6D_DrawParticle). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind6D_MoveParticles(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* r = At(::bof3::Img(at::kParticles.v) + at::kParticleBlock * S()[6]);
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
    SH_AT(RecordFn, at::kPolyF3.v)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p + 0x18), reinterpret_cast<float*>(prim + 8));
    const unsigned k = p[4];
    if (k >= at::kParticleShapeCount)
        bof3::Fatal("EffectKind6D_DrawParticle: +4 is %u, past the eight shapes at 0x%X - the original reads what "
                    "follows (docs/effect_3c.md section 6)",
                    k, at::kParticleShapes.v);
    const unsigned char* const row = At(::bof3::Img(at::kParticleShapes.v) + 8 * k);
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
    unsigned char* r = At(::bof3::Img(at::kParticles.v) + at::kParticleBlock * S()[6]);
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

void EffectKind6eFromEffect3b_Inject() {
    BOF3_INJECT(EffectKind6C_ScatterSparks);
    BOF3_INJECT(EffectKind6C_DrawSparks);
    BOF3_INJECT(EffectKind6C_DrawSpark);
    BOF3_INJECT(EffectKind6C_SparkFly);
    BOF3_INJECT(EffectKind6C_SparkFade);
}

void EffectKind6eFromEffect3c_Inject() {
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
}
