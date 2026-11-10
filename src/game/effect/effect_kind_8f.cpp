// Moved here by tools/move_funcs.py from effect_4c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_8f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

using RecordFn = void (__cdecl*)(unsigned char*);

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

U Shards() { return AddressOf(EffectKind30_Shards); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, U b) { return a * b; }

// movsx eax, ax; shl eax, 0x10: a height word into the high half.
U High(long h) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(h))) << 16; }

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

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one; answers whether it is now 0 (Sprite_Current read for each access).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// A shard's sized quad (kinds 0x8F and 0x90): a POLY_FT4 at the cursor,
// semi-transparent; its centre the shard's point +8 projected, its size +4
// (both axes) scaled at its depth in place (EffectGte_ProjectSize, size and
// out one local); the corners x - (w >> 1) and + w, y - (h >> 1) and + h on
// the x87 (fild, fsubr, fiadd); the depth to +0x10..+0x40 by mov; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 1, 0x2C0, 0x100). Answers the primitive; the caller colours
// and links or commits it.
unsigned char* ShardQuad(unsigned char* r) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(r + 8);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {static_cast<short>(Word(r + 4)), static_cast<short>(Word(r + 4))};
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
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    return prim;
}

// The column kinds 0x74 (E3C's EffectKind74_Draw) and 0x93 draw, `grey` its
// colour: EffectGte_LoadMapCamera; from the foot's height (+0x14) up to the
// top (+0x1C) + 0x1000000 in steps of 0x100000, its edge a point at the angle
// (+0x20, up 0x100 a step) 32 * (Math_Cos, Math_Sin) round the foot (+0xC,
// +0x10 - read once, at entry); each step's two heights (the step less the
// width +0x24, no lower than the foot; the step, no higher than the top -
// +0x14, +0x24, +0x1C read afresh) projected, and a POLY_FT4 (opaque, u 0x1F
// / 0, v 0xFF / 0xE0, the CLUT Gpu_GetClut(0, 0x1E5), the page
// Gpu_GetTPage(1, 0, 0x1C0, 0x100)) from the previous step's two projections
// to this one's, linked at the edge point (dy 2, 0x48).
void Column(unsigned char grey) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    U angle = Word(s + 0x20);
    const U x = rv::As<records::EffectWork>(s)->work_0c, z = rv::As<records::EffectWork>(s)->work_10;
    U y = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    long pt[3];
    float low[3], high[3];
    auto project = [&](U a) {
        pt[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) << 5) + x);
        pt[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) << 5) + z);
        unsigned char* c = S();
        U h = y - UL(c + 0x24);
        if (S32(h) < S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->step_y))) h = static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->step_y);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, low);
        c = S();
        h = y;
        if (S32(y) > S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->work_1c))) h = static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->work_1c);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, high);
    };
    project(angle);
    s = S();
    if (S32(y) >= S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u)) return;
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
        prim[4] = prim[5] = prim[6] = grey;
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(1, 0, 0x1C0, 0x100));
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0, 0x1E5));
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(pt[0]), static_cast<unsigned long>(pt[1]), 2, 0x48);
        s = S();
    } while (S32(y) < S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u));
}

}  // namespace

// original 0x48B930 (Effect_KindHandlers[0x8F], hidden in 0x48B870):
// jmp [EffectKind8F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8F_Run(void) {
    Dispatch("EffectKind8F_Run", AddressOf(EffectKind8F_States), EffectKind8F_States_count);
}

// original 0x48B950 (state 0): the sixteen shards cleared, +1 up.
extern "C" void __cdecl EffectKind8F_Start(void) {
    SH_CALL(EffectKind8F_ClearShards)();
    Step();
}

// original 0x48B960 (state 1): shards 6, 9 and 0xD spawned; +9 = 0x3C, +1 up.
extern "C" void __cdecl EffectKind8F_Burst(void) {
    SH_CALL(EffectKind8F_SpawnShard)(6);
    SH_CALL(EffectKind8F_SpawnShard)(9);
    SH_CALL(EffectKind8F_SpawnShard)(0xD);
    S()[9] = 0x3C;
    Step();
}

// original 0x48B990 (state 2): the shards moved; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind8F_Rise(void) {
    SH_CALL(EffectKind8F_MoveShards)();
    if (CountDown()) Step();
}

// original 0x48B9C0 (state 3): +9 = 0x96, +1 up.
extern "C" void __cdecl EffectKind8F_Arm(void) {
    S()[9] = 0x96;
    Step();
}

// original 0x48B9E0 (state 4): every fifteenth count (+9 % 15 0) two shards
// spawned in a shuffled order; the shards moved; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind8F_Trickle(void) {
    if (S()[9] % 15u == 0) SH_CALL(EffectKind8F_SpawnShuffled)(2);
    SH_CALL(EffectKind8F_MoveShards)();
    if (CountDown()) Step();
}

// original 0x48BA30 (state 5): the shards moved; none live: +1 up.
extern "C" void __cdecl EffectKind8F_Settle(void) {
    if (SH_CALL(EffectKind8F_MoveShards)() == 0) Step();
}

// original 0x48BA50 (state 6): the frame word +0x2E and the wait word +0x30 0
// (each through Sprite_Current read again), +1 up.
extern "C" void __cdecl EffectKind8F_ResetClock(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    Step();
}

// original 0x48BA70 (state 7): the frame word +0x2E (s16; the record read
// once, at entry) at 0x1A4 or more: +1 up. Below: the stage byte 0x676290 by
// it (below 0x3C 0, 0xB4 1, 0x12C 2, 0x168 3, else 4); the wait word +0x30 0:
// as many shards as kStageCounts[stage] spawned shuffled and the wait
// kStageWaits[stage] (the stage read back from its cell each time), else the
// wait down one; the frame word up (Sprite_Current read again). Then a tail
// jump to the shards' move.
extern "C" void __cdecl EffectKind8F_Sequence(void) {
    unsigned char* const s = S();
    const std::int32_t frame = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e));
    if (frame >= 0x1A4) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
    } else {
        unsigned char stage;
        if (frame < 0x3C)
            stage = 0;
        else if (frame < 0xB4)
            stage = 1;
        else if (frame < 0x12C)
            stage = 2;
        else
            stage = frame >= 0x168 ? 4 : 3;
        Effect4c_Stage = stage;
        const unsigned wait = rv::As<records::EffectWork>(s)->work_30;
        if (wait == 0) {
            const unsigned a = Effect4c_Stage;
            if (a >= at::kStageCount)
                bof3::Fatal("EffectKind8F_Sequence: the stage byte 0x%X is %u, past the five rows of 0x%X - the "
                            "original reads on (docs/effect_4c.md section 6)",
                            (unsigned)Effect4c_Stage_addr, a, at::kStageCounts.v);
            SH_CALL(EffectKind8F_SpawnShuffled)(At(::bof3::Img(at::kStageCounts.v) + a)[0]);
            const unsigned b = Effect4c_Stage;
            if (b >= at::kStageCount)
                bof3::Fatal("EffectKind8F_Sequence: the stage byte 0x%X is %u, past the five rows of 0x%X - the "
                            "original reads on (docs/effect_4c.md section 6)",
                            (unsigned)Effect4c_Stage_addr, b, at::kStageWaits.v);
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, At(::bof3::Img(at::kStageWaits.v) + b)[0]);
        } else {
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, wait - 1);
        }
        unsigned char* const t = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(t)->work_2e, rv::As<records::EffectWork>(t)->work_2e + 1u);
    }
    SH_CALL(EffectKind8F_MoveShards)();
}

// original 0x48BB30 (state 8): the shards moved; none live: a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind8F_Fade(void) {
    if (SH_CALL(EffectKind8F_MoveShards)() == 0) SH_CALL(Effect_Release)();
}

// original 0x48BB40 (i, a byte): a free shard (E3C's EffectKind6E_FindShard);
// none: nothing. Else +0 1, +1 0, +2 0, +3 0x20, +4 (s16) 0x80, its x +8 and
// z +0xC kShardStarts' row i << 8, +0x10 0, its speeds +0x18 / +0x1C
// kShardSpeeds' row i << 5, +0x20 0. i past the sixteen rows: ours aborts.
extern "C" void __cdecl EffectKind8F_SpawnShard(unsigned i) {
    unsigned char* const r = SH_CALL(EffectKind6E_FindShard)();
    if (r == nullptr) return;
    const unsigned k = i & 0xFFu;
    if (k >= at::kShardRowCount)
        bof3::Fatal("EffectKind8F_SpawnShard: row %u, past the sixteen of 0x%X - the original reads what follows "
                    "(docs/effect_4c.md section 6)",
                    k, at::kShardStarts.v);
    r[0] = 1;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0x20;
    SetWord(r + 4, 0x80);
    SetUL(r + 8, static_cast<U>(SW(At(::bof3::Img(at::kShardStarts.v) + 4 * k))) << 8);
    SetUL(r + 0xC, static_cast<U>(SW(At(::bof3::Img(at::kShardStarts.v) + 4 * k + 2))) << 8);
    SetUL(r + 0x10, 0);
    SetUL(r + 0x18, static_cast<U>(SW(At(::bof3::Img(at::kShardSpeeds.v) + 4 * k))) << 5);
    SetUL(r + 0x1C, static_cast<U>(SW(At(::bof3::Img(at::kShardSpeeds.v) + 4 * k + 2))) << 5);
    SetUL(r + 0x20, 0);
}

// original 0x48BBB0 (n, a byte): the rows 0..15 in a local, shuffled by
// sixteen swaps of two Rand & 0xF; the first n spawned (EffectKind8F_SpawnShard)
// in that order. n past sixteen: ours aborts (the original reads its own
// frame past the local).
extern "C" void __cdecl EffectKind8F_SpawnShuffled(unsigned n) {
    unsigned char order[16];
    for (unsigned i = 0; i < 16; ++i) order[i] = static_cast<unsigned char>(i);
    for (unsigned k = 16; k != 0; --k) {
        const unsigned a = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        const unsigned b = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        const unsigned char x = order[a];
        order[a] = order[b];
        order[b] = x;
    }
    const unsigned count = n & 0xFFu;
    if (count == 0) return;
    if (count > 16)
        bof3::Fatal("EffectKind8F_SpawnShuffled: %u shards, past the sixteen of its local order - the original reads "
                    "its own frame (docs/effect_4c.md section 6)",
                    count);
    for (unsigned i = 0; i < count; ++i) SH_CALL(EffectKind8F_SpawnShard)(order[i]);
}

// original 0x48BC40: +0 = 0 in the sixteen shards.
extern "C" void __cdecl EffectKind8F_ClearShards(void) {
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard8FCount; n != 0; --n, r += at::kShardStride) r[0] = 0;
}

// original 0x48BC60: EffectGte_LoadMapCamera; each live shard of the sixteen
// stepped by its phase +1: 0 - +3 up 4, +2 up, at 0x10 the phase 1; 1 - +3
// down 4, +2 up, at 0x20 freed (+0 = 0); then x +8 += +0x18, z +0xC += +0x1C,
// its size +4 (s16) up 8, its height +0x10 AreaMap_Elevation(x, z) << 16, and
// drawn (EffectKind8F_DrawShard). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind8F_MoveShards(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard8FCount; n != 0; --n, r += at::kShardStride) {
        if (r[0] == 0) continue;
        any = 1;
        if (r[1] == 0) {
            r[3] = static_cast<unsigned char>(r[3] + 4);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x10) r[1] = static_cast<unsigned char>(r[1] + 1);
        } else if (r[1] == 1) {
            r[3] = static_cast<unsigned char>(r[3] + 0xFC);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x20) r[0] = 0;
        }
        const U x = UL(r + 8) + UL(r + 0x18);
        SetUL(r + 8, x);
        SetWord(r + 4, Word(r + 4) + 8u);
        const U z = UL(r + 0xC) + UL(r + 0x1C);
        SetUL(r + 0xC, z);
        const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(r + 0x10, High(h));
        SH_CALL(EffectKind8F_DrawShard)(r);
    }
    return any;
}

// original 0x48BD10 (a shard): its sized quad (ShardQuad); colour (+3, (+3 >>
// 2) * 3, +3 >> 1); linked at (+8, +0xC) (dy 3, 0x48).
extern "C" void __cdecl EffectKind8F_DrawShard(unsigned char* r) {
    unsigned char* const prim = ShardQuad(r);
    prim[4] = r[3];
    prim[5] = static_cast<unsigned char>((r[3] >> 2) * 3);
    prim[6] = static_cast<unsigned char>(r[3] >> 1);
    SH_CALL(MapView_LinkPrimAt)(UL(r + 8), UL(r + 0xC), 3, 0x48);
}

// original 0x48BF00 (Effect_KindHandlers[0x90], hidden in 0x48BD10):
// jmp [EffectKind90_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind90_Run(void) {
    Dispatch("EffectKind90_Run", AddressOf(EffectKind90_States), EffectKind90_States_count);
}

// original 0x48BF20 (state 0): the tag 0x676294 0 (no kind-0x90 record yet):
// the 32 shards cleared. +9 = 0x80; the tag up, its low byte the record's +6;
// +1 up.
extern "C" void __cdecl EffectKind90_Start(void) {
    if (static_cast<std::int32_t>(Effect4c_Tag) == 0) SH_CALL(EffectKind90_ClearShards)();
    S()[9] = 0x80;
    const U tag = Effect4c_Tag + 1;
    unsigned char* const s = S();
    Effect4c_Tag = tag;
    s[6] = static_cast<unsigned char>(tag);
    Step();
}

// original 0x48BF60 (state 1): every fourth count (+9 & 3 0) a shard emitted
// (EffectKind90_EmitOne); +9 down, at 0 +1 up; a tail jump to the shards'
// move.
extern "C" void __cdecl EffectKind90_Emit(void) {
    unsigned char* s = S();
    if ((s[9] & 3u) == 0) {
        SH_CALL(EffectKind90_EmitOne)();
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(EffectKind90_MoveShards)();
}

// original 0x48BFA0 (state 2): the shards moved; none of this record's live:
// a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind90_Fade(void) {
    if (SH_CALL(EffectKind90_MoveShards)() == 0) SH_CALL(Effect_Release)();
}

// original 0x48BFB0 (Effect_KindHandlers[0x93], hidden in 0x48BD10):
// jmp [EffectKind93_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind93_Run(void) {
    Dispatch("EffectKind93_Run", AddressOf(EffectKind93_States), EffectKind93_States_count);
}

// original 0x48BFD0 (state 0; E3C's EffectKind74_Start but the pool cleared
// first and the sound): the 32 shards cleared; the column's foot +0xC / +0x10
// / +0x14 the record's point +0x34 / +0x38 / +0x3C, its top +0x1C the foot,
// its angle +0x20 (s16) 0, its width +0x24 0x1000000; +9 = 0x40, +1 up, sound
// 0x208.
extern "C" void __cdecl EffectKind93_Start(void) {
    SH_CALL(EffectKind90_ClearShards)();
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    SetWord(S() + 0x20, 0);
    SetUL(S() + 0x24, 0x1000000);
    S()[9] = 0x40;
    Step();
    SH_CALL(Sound_PlayEffect)(0x208);
}

// original 0x48C040 (state 1; EffectKind74_Rise's twin): the top +0x1C up
// 0x1000000, no higher than the foot + 0x8000000; the angle +0x20 turned by
// 0xFE00; drawn (EffectKind93_Draw); +9 down, at 0 +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind93_Rise(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u);
    s = S();
    const U cap = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) + 0x8000000u;
    if (S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c)) > S32(cap)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, cap);
        s = S();
    }
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    SH_CALL(EffectKind93_Draw)();
    if (CountDown()) {
        S()[9] = 0x10;
        Step();
    }
}

// original 0x48C0A0 (state 2; EffectKind74_Fade's twin): +9 down; at 0 a tail
// jump to Effect_Release; else the angle turned by 0xFE00, the width +0x24
// down 0x100000 and a tail jump to EffectKind93_Draw.
extern "C" void __cdecl EffectKind93_Fade(void) {
    if (CountDown()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* s = S();
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    s = S();
    SetUL(s + 0x24, UL(s + 0x24) + 0xFFF00000u);
    SH_CALL(EffectKind93_Draw)();
}

// original 0x48C0E0 (Effect_KindHandlers[0x99], hidden in 0x48BD10):
// jmp [EffectKind99_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind99_Run(void) {
    Dispatch("EffectKind99_Run", AddressOf(EffectKind99_States), EffectKind99_States_count);
}

// original 0x48C100 (state 0): the disc's centre +0xC = the record's +0x34 -
// 0x10000, +0x10 = +0x38 - 0x10000 (Sprite_Current read again; both written
// through the record read first), its height +0x14 AreaMap_Elevation(+0xC,
// +0x10) << 16, its radius +0x1C 0; +9 = 0xA, +1 up.
extern "C" void __cdecl EffectKind99_Start(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    unsigned char* const c = s + 0xC;
    SetUL(c, static_cast<std::uint32_t>(so->x) - 0x10000u);
    const U x = UL(c);
    const U z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) + 0xFFFF0000u;
    SetUL(c + 4, z);
    const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
    SetUL(c + 8, High(h));
    SetUL(c + 0x10, 0);
    S()[9] = 0xA;
    Step();
}

// original 0x48C160 (state 1): the disc drawn (EffectKind99_DrawDisc(+0xC,
// +0x1C)), its radius +0x1C up 0x1199 (through the record read first); +9
// down (read again), at 0 sound 0x20F and +1 up.
extern "C" void __cdecl EffectKind99_Spread(void) {
    unsigned char* const s = S();
    const U radius = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c);
    unsigned char* const c = s + 0xC;
    SH_CALL(EffectKind99_DrawDisc)(reinterpret_cast<const long*>(c), static_cast<long>(radius));
    SetUL(c + 0x10, UL(c + 0x10) + 0x1199u);
    if (CountDown()) {
        SH_CALL(Sound_PlayEffect)(0x20F);
        Step();
    }
}

// original 0x48C1C0: a free shard (EffectKind90_FindShard); none: nothing.
// Else +1 0, +0 the record's +6 (its tag), +2 0, +3 0x80, +4 (s16) 0x80, x +8
// the record's +0x34, +0x10 0, z +0xC the record's +0x38 (Sprite_Current read
// for each), its rise +0x18 0x1800, +0x1C 0, +0x20 0.
extern "C" void __cdecl EffectKind90_EmitOne(void) {
    unsigned char* const r = SH_CALL(EffectKind90_FindShard)();
    if (r == nullptr) return;
    const unsigned char tag = S()[6];
    r[1] = 0;
    r[0] = tag;
    r[2] = 0;
    r[3] = 0x80;
    SetWord(r + 4, 0x80);
    SetUL(r + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    const U z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z);
    SetUL(r + 0x10, 0);
    SetUL(r + 0xC, z);
    SetUL(r + 0x18, 0x1800);
    SetUL(r + 0x1C, 0);
    SetUL(r + 0x20, 0);
}

// original 0x48C220: the first of the 32 shards of 0x28 at EffectKind30_Shards
// whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind90_FindShard(void) {
    unsigned char* r = At(Shards());
    for (unsigned i = 0; i < at::kShard90Count; ++i, r += at::kShardStride)
        if (r[0] == 0) return r;
    return nullptr;
}

// original 0x48C240: +0 = 0 in the 32 shards (kinds 0x90 and 0x93).
extern "C" void __cdecl EffectKind90_ClearShards(void) {
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard90Count; n != 0; --n, r += at::kShardStride) r[0] = 0;
}

// original 0x48C260: EffectGte_LoadMapCamera; each of the 32 shards whose +0
// is Sprite_Current's +6 (read again for each - a free shard's 0 matches a tag
// of 0) stepped by its phase +1 through a bounded switch (0x48C354, four
// cases; above 3 none): 0 - its size +4 (s16) up 4, +2 up, at 8 the phase 1;
// 1 - +3 down 8, +2 up, at 0x10 the phase 2; 2 - the size down 0x10, +2 up,
// at 0x18 the phase 3; 3 - the size down 0x10, +2 up, at 0x20 freed (+0 = 0).
// Then x +8 += +0x18, z +0xC += +0x1C, the size up 8, its height +0x10
// AreaMap_Elevation(x, z) << 16, and drawn (EffectKind90_DrawShard). al 1
// when any matched.
extern "C" unsigned char __cdecl EffectKind90_MoveShards(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard90Count; n != 0; --n, r += at::kShardStride) {
        if (r[0] != S()[6]) continue;
        any = 1;
        switch (r[1]) {
        case 0:
            SetWord(r + 4, Word(r + 4) + 4u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 8) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 1:
            r[3] = static_cast<unsigned char>(r[3] + 0xF8);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x10) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 2:
            SetWord(r + 4, Word(r + 4) + 0xFFF0u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x18) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 3:
            SetWord(r + 4, Word(r + 4) + 0xFFF0u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x20) r[0] = 0;
            break;
        default: break;
        }
        const U x = UL(r + 8) + UL(r + 0x18);
        SetWord(r + 4, Word(r + 4) + 8u);
        const U z = UL(r + 0xC) + UL(r + 0x1C);
        SetUL(r + 8, x);
        SetUL(r + 0xC, z);
        const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(r + 0x10, High(h));
        SH_CALL(EffectKind90_DrawShard)(r);
    }
    return any;
}

// original 0x48C370 (a shard): its sized quad (ShardQuad); colour +3 in all
// three channels; committed (Gfx_CommitPrim(1, 0x48)), not linked.
extern "C" void __cdecl EffectKind90_DrawShard(unsigned char* r) {
    unsigned char* const prim = ShardQuad(r);
    const unsigned char c = r[3];
    prim[6] = c;
    prim[5] = c;
    prim[4] = c;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x48C550: E3C's EffectKind74_Draw at grey 0xC0 (that one 0x80):
// the column of Column above.
extern "C" void __cdecl EffectKind93_Draw(void) { Column(0xC0); }

// original 0x48C7A0 (point, radius): EffectGte_LoadMapCamera; four fans
// (EffectKind99_DrawFan) at the angles 0, 0x400, 0x800, 0xC00, each linked at
// the point offset by kFanDx / kFanDz's dword i.
extern "C" void __cdecl EffectKind99_DrawDisc(const long* point, long radius) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kFanCount; ++i) {
        const long dz = Long(At(::bof3::Img(at::kFanDz.v) + 4 * i));
        const long dx = Long(At(::bof3::Img(at::kFanDx.v) + 4 * i));
        SH_CALL(EffectKind99_DrawFan)(point, radius, i << 10, dx, dz);
    }
}

// original 0x48C7F0 (point, radius, angle - its low word -, dx, dz): the
// centre (the point) projected; the edge at the angle - (Math_Cos * radius
// >> 12) + x, (Math_Sin * radius >> 12) + z, the point's height (each word of
// the point read after the call before it) - projected; four times: a draw
// mode (page (0x380, 0x100), abr 2, dtd 0) linked at (x + dx, z + dz) (dy 0,
// 0x38; the point read after the mode); a flat triangle (0x5A7570) at the
// cursor, semi-transparent: the centre (dwords copied), the last edge, the
// angle up 0x100 and the edge there projected; grey 0x80; linked again at
// the same cell.
extern "C" void __cdecl EffectKind99_DrawFan(const long* point, long radius, unsigned angle, long dx, long dz) {
    float centre[3], edge[3];
    long v[3];
    SH_CALL(EffectGte_ProjectPoint)(point, centre);
    const U r = static_cast<U>(radius);
    U a = angle & 0xFFFFu;
    const U cosine = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    v[0] = static_cast<long>(Sar(Mul(cosine, r), 12) + static_cast<U>(point[0]));
    const U sine = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    v[1] = static_cast<long>(Sar(Mul(sine, r), 12) + static_cast<U>(point[1]));
    v[2] = point[2];
    SH_CALL(EffectGte_ProjectPoint)(v, edge);
    U turn = angle;
    for (unsigned n = 4; n != 0; --n) {
        DrawMode(2, 0x380, 0x100, 0);
        const U lx = static_cast<U>(point[0]) + static_cast<U>(dx);
        const U lz = static_cast<U>(point[1]) + static_cast<U>(dz);
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x38);
        unsigned char* const prim = Gfx_PacketNext;
        SH_AT(RecordFn, at::kPolyF3.v)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, centre, 12);
        std::memcpy(prim + 0x14, edge, 12);
        turn += 0x100;
        a = turn & 0xFFFFu;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        v[0] = static_cast<long>(Sar(Mul(c, r), 12) + static_cast<U>(point[0]));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        v[1] = static_cast<long>(Sar(Mul(s, r), 12) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(v, edge);
        std::memcpy(prim + 0x20, edge, 12);
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0x80;
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x38);
    }
}

void EffectKind8f_Inject() {
    BOF3_INJECT(EffectKind8F_Run);
    BOF3_INJECT(EffectKind8F_Start);
    BOF3_INJECT(EffectKind8F_Burst);
    BOF3_INJECT(EffectKind8F_Rise);
    BOF3_INJECT(EffectKind8F_Arm);
    BOF3_INJECT(EffectKind8F_Trickle);
    BOF3_INJECT(EffectKind8F_Settle);
    BOF3_INJECT(EffectKind8F_ResetClock);
    BOF3_INJECT(EffectKind8F_Sequence);
    BOF3_INJECT(EffectKind8F_Fade);
    BOF3_INJECT(EffectKind8F_SpawnShard);
    BOF3_INJECT(EffectKind8F_SpawnShuffled);
    BOF3_INJECT(EffectKind8F_ClearShards);
    BOF3_INJECT(EffectKind8F_MoveShards);
    BOF3_INJECT(EffectKind8F_DrawShard);
    BOF3_INJECT(EffectKind90_Run);
    BOF3_INJECT(EffectKind90_Start);
    BOF3_INJECT(EffectKind90_Emit);
    BOF3_INJECT(EffectKind90_Fade);
    BOF3_INJECT(EffectKind93_Run);
    BOF3_INJECT(EffectKind93_Start);
    BOF3_INJECT(EffectKind93_Rise);
    BOF3_INJECT(EffectKind93_Fade);
    BOF3_INJECT(EffectKind99_Run);
    BOF3_INJECT(EffectKind99_Start);
    BOF3_INJECT(EffectKind99_Spread);
    BOF3_INJECT(EffectKind90_EmitOne);
    BOF3_INJECT(EffectKind90_FindShard);
    BOF3_INJECT(EffectKind90_ClearShards);
    BOF3_INJECT(EffectKind90_MoveShards);
    BOF3_INJECT(EffectKind90_DrawShard);
    BOF3_INJECT(EffectKind93_Draw);
    BOF3_INJECT(EffectKind99_DrawDisc);
    BOF3_INJECT(EffectKind99_DrawFan);
}
