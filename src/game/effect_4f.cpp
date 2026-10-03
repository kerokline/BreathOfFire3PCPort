// Round thirteen group E4F (docs/effect_4f.md): the 45 functions of
// analysis/round13_cut.tsv's group E4F, the one its band holds that no list has
// (0x492AF0) and three unplaced rows of the band that serve its kinds (0x492530,
// 0x492750, 0x492CF0) - 49, 0x491D70..0x494026, each read with capstone to its
// last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare). What each kind is, as far as the code
// says (spawners of ours: kind 0xAC area 198's Area198_Shake, 0xB9 area 188's
// Area188_SpawnEffectB9, 0xBA area 121's Area121_SpawnEffectBA; the others'
// spawners are not ours):
//
//   kind 0xAA  a dispatcher only here (its three states are catalog part 6)
//   kind 0xAB  a copy of kind 0x81 (effect_3d) with eight sources: drops from
//              the cells EffectKindAB_SourceCells gives, each a red or grey
//              dot and a 3 by 3 tile linked into the map
//   kind 0xAC  a dispatcher only here
//   kind 0xAD  a dispatcher; its state 1 (part 6) draws an arc of 16 POLY_G4
//              between two points (EffectKindAD_DrawArc); Effect_StateNext
//   kind 0xAE  an ellipse of 16 POLY_G3 (Effect_DrawEllipse, which kind 0x20
//              draws too) that sinks, widens, narrows and shrinks away
//   kind 0xAF  a red full-screen tile that brightens and fades, again while
//              Draw_PassFlags is set
//   kind 0xB0  sixteen bars (catalog part 6 draws them) started one by one
//   kind 0xB1  the camera swayed: Camera_Angles[1] up and down by 4 a frame
//              between -0xF0 and 0xF0, the view's elevation with it
//   kind 0xB9  a copy of kind 0x64 (effect_3a): a glow at the leader's point
//              that grows, holds and rises (E4E's glow draw); sixteen shards,
//              eight sparks and a trail - kind 0x64 calls this group's spark
//              and shard helpers too
//   kind 0xBA  a dispatcher, its start, and the line its state 1 (part 6) draws
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; where the original jumps through a state table past its
// end ours aborts with a message (docs/effect_4f.md section 6). The x87
// arithmetic is the originals' instructions (inline assembly) or long double,
// which the compiler keeps on the x87 at the control word's precision, as the
// originals' fild / fadd / fsub / fstp chains are.
#include "game/effect_4f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4f::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }
// movsx r32, r16 of a register's low word.
U Sx16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it --------------------------------------------------
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// fild dword v; fadd dword [f]; fstp dword [to]
void FildAdd(unsigned char* to, U v, const unsigned char* f) {
    const auto i = static_cast<std::int32_t>(v);
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f)));
}
// fld dword [a]; fsub dword [b]; fstp dword [to]
void FldSub(unsigned char* to, const unsigned char* a, const unsigned char* b) {
    __asm__ volatile("flds %1\n\tfsubs %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }
const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

using Handler = scenario_harness::Handler;

// The entry `state` of a kind's table (read in place: the fuzz swaps the cells
// for recorders); a Fatal past the table's own length, where the original jumps
// (or calls) through the dword after - the next kind's table.
Handler Entry(const char* who, U table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4f.md section 6)",
                    who, state, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)));
}
// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void Dispatch(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one; true when it reached 0 (Sprite_Current read for each access,
// as the originals).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

unsigned char* Drop(U i) { return At(at::kDrops + i * at::kDropStride); }
unsigned char* Source(U i) { return At(at::kSources + i * at::kSourceStride); }
unsigned char* Bar(U i) { return At(at::kBars + i * at::kBarStride); }
unsigned char* Spark(U i) { return At(at::kSparks + i * at::kSparkStride); }
unsigned char* Shard(U i) { return At(at::kShards + i * at::kShardStride); }

// kind 0xB9's glow: group E4E's 0x4901D0 (point, size word, colour byte), raw
// until it merges.
void Glow(unsigned char* s) {
    using Draw = void (__cdecl*)(const long*, unsigned, unsigned);
    SH_AT(Draw, at::kGlowDraw)(Point(s + 0x64), Word(s + 0x2E), 6);
}

// The opening of kind 0xB9's states 4..7: the frame +6 up, and on every fourth
// (its value before, & 3 = 0) a spark.
void SparkTick() {
    unsigned char* const s = S();
    const unsigned char frame = s[6];
    s[6] = static_cast<unsigned char>(frame + 1);
    if ((frame & 3) == 0) SH_CALL(EffectKindB9_SpawnSpark)();
}

}  // namespace

// ===========================================================================
// The dispatchers whose states are all another part's (kinds 0xAA, 0xAC)
// ===========================================================================

// original 0x491D70 (hidden in 0x4918B0): jmp [EffectKindAA_States + +1 * 4],
// unbounded. Its three states are catalog part 6 rows (0x491D90, 0x491DB0,
// 0x491DF0).
extern "C" void __cdecl EffectKindAA_Run(void) {
    Dispatch("EffectKindAA_Run", AddressOf(EffectKindAA_States), EffectKindAA_States_count);
}

// original 0x4925A0 (hidden in 0x492400): jmp [EffectKindAC_States + +1 * 4],
// unbounded. Its three states are catalog part 6 rows (0x4925C0, 0x4925E0,
// 0x492620).
extern "C" void __cdecl EffectKindAC_Run(void) {
    Dispatch("EffectKindAC_Run", AddressOf(EffectKindAC_States), EffectKindAC_States_count);
}

// ===========================================================================
// Kind 0xAB: Effect_KindHandlers[0xAB] (0x6555FC), EffectKindAB_States (three):
// a copy of kind 0x81's drops (effect_3d.cpp) with eight sources
// ===========================================================================

// original 0x492510 (hidden in 0x492400): jmp [EffectKindAB_States + +1 * 4],
// unbounded. States: EffectKindAB_Start, 0x492580 (part 6: Draw_PassFlags 0 -
// +1 up; else EffectKindAB_Emit and a tail jmp to EffectKindAB_MoveDrops),
// Effect_StateRelease.
extern "C" void __cdecl EffectKindAB_Run(void) {
    Dispatch("EffectKindAB_Run", AddressOf(EffectKindAB_States), EffectKindAB_States_count);
}

// original 0x492530 (state 0; hidden in 0x492400, catalog part 6): the record's
// point +0x34 / +0x38 / +0x3C the leader's (ObjTrio +0x34..); the drops cleared
// (EffectKind81_ClearDrops), the eight sources placed; +9 = 0x78; +1 up.
extern "C" void __cdecl EffectKindAB_Start(void) {
    SetUL(S() + 0x34, UL(at::kLeaderPoint));
    SetUL(S() + 0x38, UL(at::kLeaderPoint + 4));
    SetUL(S() + 0x3C, UL(at::kLeaderPoint + 8));
    SH_CALL(EffectKind81_ClearDrops)();
    SH_CALL(EffectKindAB_PlaceSources)();
    S()[9] = 0x78;
    NextState();
}

// original 0x492AF0 (inside the cut's 0x492AA0 extent, no list's; reached by a
// tail jmp from kind 0xAB's state 1 0x492580): EffectKind81_MoveDrops without its
// moving count - the map camera; each drop in use: its speed +0x14 0x20000 more,
// its height +0xC moved by the new speed, its blink bit +3 flipped, drawn
// (EffectKindAB_DrawDrop), its life +2 down - at 0 its +0 and +1 cleared. al 1
// when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKindAB_MoveDrops(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = Drop(i);
        if (d[0] == 0) continue;
        const U speed = UL(d + 0x14) + 0x20000u;
        const U height = UL(d + 0xC);
        any = 1;
        const auto blink = static_cast<unsigned char>(d[3] ^ 1u);
        SetUL(d + 0x14, speed);
        SetUL(d + 0xC, height + speed);
        d[3] = blink;
        SH_CALL(EffectKindAB_DrawDrop)(d);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) {
            d[0] = 0;
            d[1] = 0;
        }
    }
    return any;
}

// original 0x492B60 (cdecl): a drop (+4 x, z, height) drawn - a draw mode (page
// Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 0) linked into the map at its x, z
// (MapView_LinkPrimAt, dy 0, 0xC); a semi-transparent one-pixel tile at its
// projection, (0x80, 0x80, 0x80) with the blink bit +3 set, else (0x80, 0, 0),
// linked (0x14); a semi-transparent 3 by 3 tile one up and left of it, (0x20,
// 0x20, 0x20) or (0x20, 0, 0), linked (0x1C). Neither tile's +0x10 is written.
extern "C" void __cdecl EffectKindAB_DrawDrop(unsigned char* drop) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0xC);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(drop + 4), Out(o));
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    Copy12(p + 8, o);
    if (drop[3] != 0) {
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
    } else {
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
    }
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x14);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    FldSub(p + 8, o, At(at::kOne));
    SetBits(p + 0x14, 0x40400000u);   // 3.0f
    SetBits(p + 0x18, 0x40400000u);
    FldSub(p + 0xC, o + 4, At(at::kOne));
    if (drop[3] != 0) {
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0x20;
    } else {
        p[4] = 0x20;
        p[5] = 0;
        p[6] = 0;
    }
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x1C);
}

// original 0x492C80 (cdecl): EffectKind81_PlaceSources for eight - the sources
// at 0x92D780 (0x14 each): +0 1, +1 0, +2 (Rand & 0xF) - 0x10, +3 Rand & 0x1F
// (the first wait), +4 / +8 the cell EffectKindAB_SourceCells gives (s8 x, z <<
// 16), +0xC the ground's height there (AreaMap_Elevation's low word << 16).
extern "C" void __cdecl EffectKindAB_PlaceSources(void) {
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = Source(i);
        src[0] = 1;
        src[1] = 0;
        src[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0xFu) - 0x10u);
        const U r = static_cast<U>(SH_CALL(Rand)());
        const U z = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kSourceCells + 2 * i + 1)))) << 16;
        src[3] = static_cast<unsigned char>(r & 0x1Fu);
        const U x = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kSourceCells + 2 * i)))) << 16;
        SetUL(src + 4, x);
        SetUL(src + 8, z);
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(src + 0xC, Sx16(static_cast<U>(ground)) << 16);
    }
}

// original 0x492CF0 (cdecl; catalog part 7, unplaced): EffectKind81_Emit for
// eight - each source on (+0): its wait +3 down; when it was 0, two drops from
// free records (EffectKind81_FindFreeDrop; none, none) - +0 1, +2 life 0x20 +
// (Rand & 0x1F), +4 / +8 the source's x / z less 0x10000 and plus Rand's low
// byte << 8, +0xC its height, +0x14 0, +3 Rand & 1 - and the wait Rand & 7. al 1
// when any source was on, else 0.
extern "C" unsigned char __cdecl EffectKindAB_Emit(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = Source(i);
        if (src[0] == 0) continue;
        const unsigned wait = src[3];
        any = 1;
        src[3] = static_cast<unsigned char>(wait - 1);
        if (wait != 0) continue;
        for (unsigned n = 0; n < 2; ++n) {
            unsigned char* const d = SH_CALL(EffectKind81_FindFreeDrop)();
            if (d == nullptr) continue;
            d[0] = 1;
            d[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) + 0x20u);
            const U rx = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 4, ((rx - 0x100u) << 8) + UL(src + 4));
            const U rz = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 8, ((rz - 0x100u) << 8) + UL(src + 8));
            SetUL(d + 0xC, UL(src + 0xC));
            SetUL(d + 0x14, 0);
            d[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 1u);
        }
        src[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
    return any;
}

// ===========================================================================
// Kind 0xAD: Effect_KindHandlers[0xAD] (0x655604), EffectKindAD_States (five)
// ===========================================================================

// original 0x492660 (hidden in 0x492400): jmp [EffectKindAD_States + +1 * 4],
// unbounded. States: 0x492680, 0x492710 (part 6), Effect_StateNext twice,
// Effect_StateRelease.
extern "C" void __cdecl EffectKindAD_Run(void) {
    Dispatch("EffectKindAD_Run", AddressOf(EffectKindAD_States), EffectKindAD_States_count);
}

// original 0x492750 (hidden in 0x492400, catalog part 6; kind 0xAD's states 2
// and 3, and EffectKind60_States' too): +1 up.
extern "C" void __cdecl Effect_StateNext(void) { NextState(); }

namespace {
// The arc's offset: (v * 5 << 6) sar 4 (lea, shl, sar on the 32 bits) - v * 20.
U ArcOffset(int v) { return Sar((static_cast<U>(v) * 5u) << 6, 4); }
}  // namespace

// original 0x492DC0 (cdecl; kind 0xAD's state 1 0x492710 draws it with the
// record's +0x34 and +0xC and its bytes +0x5D, +0x5E): a draw mode (Gpu_GetTPage
// (0, 1, 0x380, 0x100), dtd 1, committed to slot 1); about `a` and `b` each a
// point at (cos, sin) x 20 from the angle 0xFE00, projected; then 16 semi-
// transparent POLY_G4, the angle 0x80 more each: the last two projections and
// the new two, shaded `inner` at a's and `outer` at b's, committed 0x44 to slot
// 1. The second vertex's depth (+0x20) is b's projection's y, not its depth: the
// original copies the y twice (docs/effect_4f.md section 6, a latent defect kept).
extern "C" void __cdecl EffectKindAD_DrawArc(const long* a, const long* b, unsigned inner, unsigned outer) {
    DrawMode(1, 0x380, 0x100, 1, 1);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char o1[12], o2[12], q[12], r[12];
    U angle = 0xFE00;
    {
        const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
        SetUL(q, ArcOffset(c) + UL(pa));
        const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
        SetUL(q + 4, ArcOffset(s) + UL(pa + 4));
        SetUL(q + 8, UL(pa + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
    }
    {
        const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
        SetUL(r, ArcOffset(c) + UL(pb));
        const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
        SetUL(r + 4, ArcOffset(s) + UL(pb + 4));
        SetUL(r + 8, UL(pb + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(r), Out(o2));
    }
    const unsigned char shadeA = static_cast<unsigned char>(inner);
    const unsigned char shadeB = static_cast<unsigned char>(outer);
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        angle += 0x80;
        SetUL(p + 8, UL(o1));
        SetUL(p + 0xC, UL(o1 + 4));
        SetUL(p + 0x10, UL(o1 + 8));
        SetUL(p + 0x18, UL(o2));
        SetUL(p + 0x1C, UL(o2 + 4));
        SetUL(p + 0x20, UL(o2 + 4));   // the original's: the y again, not the depth o2 + 8
        const int t = static_cast<int>(angle & 0xFFFFu);
        {
            const int c = SH_CALL(Math_Cos)(t);
            SetUL(q, ArcOffset(c) + UL(pa));
            const int s = SH_CALL(Math_Sin)(t);
            SetUL(q + 4, ArcOffset(s) + UL(pa + 4));
            SetUL(q + 8, UL(pa + 8));
            SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
        }
        {
            const int c = SH_CALL(Math_Cos)(t);
            SetUL(r, ArcOffset(c) + UL(pb));
            const int s = SH_CALL(Math_Sin)(t);
            SetUL(r + 4, ArcOffset(s) + UL(pb + 4));
            SetUL(r + 8, UL(pb + 8));
            SH_CALL(EffectGte_ProjectPoint)(Point(r), Out(o2));
        }
        SetUL(p + 0x28, UL(o1));
        SetUL(p + 0x2C, UL(o1 + 4));
        SetUL(p + 0x30, UL(o1 + 8));
        SetUL(p + 0x38, UL(o2));
        SetUL(p + 0x3C, UL(o2 + 4));
        SetUL(p + 0x40, UL(o2 + 8));
        p[4] = shadeA;
        p[5] = shadeA;
        p[6] = shadeA;
        p[0x14] = shadeB;
        p[0x15] = shadeB;
        p[0x16] = shadeB;
        p[0x24] = shadeA;
        p[0x25] = shadeA;
        p[0x26] = shadeA;
        p[0x34] = shadeB;
        p[0x35] = shadeB;
        p[0x36] = shadeB;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// ===========================================================================
// Kind 0xAE: Effect_KindHandlers[0xAE] (0x655608), EffectKindAE_States (five)
// ===========================================================================

// original 0x492760 (hidden in 0x492400): jmp [EffectKindAE_States + +1 * 4],
// unbounded. States: 0x492780 (part 6), _Sink, _Widen, _Narrow, _Shrink.
extern "C" void __cdecl EffectKindAE_Run(void) {
    Dispatch("EffectKindAE_Run", AddressOf(EffectKindAE_States), EffectKindAE_States_count);
}

// original 0x492880 (state 1): the height +0x3C down 0x1000000; drawn
// (EffectKindAE_Draw); +9 down, at 0 +9 = 4, +1 up.
extern "C" void __cdecl EffectKindAE_Sink(void) {
    unsigned char* const s = S();
    SetUL(s + 0x3C, UL(s + 0x3C) + 0xFF000000u);
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 4;
    NextState();
}

// original 0x4928C0 (state 2): the width +0x2E up 0x40, the height +0x30 down
// 0x40; drawn; +9 down, at 0 +9 = 2, +1 up.
extern "C" void __cdecl EffectKindAE_Widen(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0x40u);
    SetWord(S() + 0x30, Word(S() + 0x30) + 0xFFC0u);
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 2;
    NextState();
}

// original 0x492900 (state 3): the width down 0x40, the height up 0x40; drawn;
// +9 down, at 0 +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAE_Narrow(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0xFFC0u);
    SetWord(S() + 0x30, Word(S() + 0x30) + 0x40u);
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 0x10;
    NextState();
}

// original 0x492940 (state 4): the width and the height down 0x10; drawn; +9
// down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindAE_Shrink(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0xFFF0u);
    SetWord(S() + 0x30, Word(S() + 0x30) + 0xFFF0u);
    SH_CALL(EffectKindAE_Draw)();
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x493010: a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1,
// committed to slot 1); the record's point +0x34.. copied to a local and the
// ellipse drawn there (Effect_DrawEllipse(local, +0x2E, +0x30, +0x32, 0xC0, 0)).
extern "C" void __cdecl EffectKindAE_Draw(void) {
    DrawMode(1, 0x380, 0x100, 1, 1);
    unsigned char* const s = S();
    alignas(4) unsigned char point[12];
    Copy12(point, s + 0x34);
    SH_CALL(Effect_DrawEllipse)(Point(point), Word(s + 0x2E), Word(s + 0x30), Word(s + 0x32), 0xC0, 0);
}

// original 0x493090 (cdecl; EffectKindAE_Draw's and EffectKind20_Draw's): the
// map camera; `point` projected (o) and its size {w, h} projected
// (EffectGte_ProjectSize) to the radii {rx, ry}, each one more on odd frames
// (Frame_Counter & 1, f); a fan of 16 semi-transparent POLY_G3 round o - the
// rim at the ellipse (cos a * rx, sin a * ry) sar 12 (each an s16) turned by
// `angle`, a from 0 by 0x100 - the centre (shade, shade, f ? shade : 0), the rim
// (rim, rim, f ? rim : 0); each committed 0x34 to slot 1.
extern "C" void __cdecl Effect_DrawEllipse(const long* point, unsigned w, unsigned h, unsigned angle, unsigned shade,
                                           unsigned rim) {
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short size[2] = {static_cast<short>(w), static_cast<short>(h)};
    alignas(4) short radius[2];
    SH_CALL(EffectGte_ProjectSize)(point, size, radius);
    const unsigned char odd = static_cast<unsigned char>(Frame_Counter & 1u);
    radius[0] = static_cast<short>(radius[0] + odd);
    radius[1] = static_cast<short>(radius[1] + odd);
    const U theta = angle & 0xFFFFu;
    const unsigned char c0 = static_cast<unsigned char>(shade);
    const unsigned char c1 = static_cast<unsigned char>(rim);
    // the angle a: the original keeps the sum unmasked and pushes it & 0xFFFF
    // (both the same below 0x10000; it ends at 0x1000)
    U a = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        // the first rim point, at a
        U x = Sx16(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a)), static_cast<U>(static_cast<std::int32_t>(radius[0])), 12));
        U y = Sx16(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a)), static_cast<U>(static_cast<std::int32_t>(radius[1])), 12));
        U cx = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * x;
        U sy = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x18, Sar(cx - sy, 12), o);
        U sx = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * x;
        U cy = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * y;
        a += 0x100;
        const U next = a & 0xFFFFu;
        FildAdd(p + 0x1C, Sar(sx + cy, 12), o + 4);
        // the second, at a + 0x100
        x = Sx16(MulSar(SH_CALL(Math_Cos)(static_cast<int>(next)), static_cast<U>(static_cast<std::int32_t>(radius[0])), 12));
        y = Sx16(MulSar(SH_CALL(Math_Sin)(static_cast<int>(next)), static_cast<U>(static_cast<std::int32_t>(radius[1])), 12));
        cx = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * x;
        sy = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x28, Sar(cx - sy, 12), o);
        sx = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * x;
        cy = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x2C, Sar(sx + cy, 12), o + 4);
        p[4] = c0;
        p[5] = c0;
        p[6] = odd != 0 ? c0 : 0;
        p[0x14] = c1;
        p[0x15] = c1;
        p[0x16] = odd != 0 ? c1 : 0;
        p[0x24] = c1;
        p[0x25] = c1;
        p[0x26] = odd != 0 ? c1 : 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// ===========================================================================
// Kind 0xAF: Effect_KindHandlers[0xAF] (0x65560C), EffectKindAF_States (three)
// ===========================================================================

// original 0x492980 (hidden in 0x492400): jmp [EffectKindAF_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindAF_Run(void) {
    Dispatch("EffectKindAF_Run", AddressOf(EffectKindAF_States), EffectKindAF_States_count);
}

// original 0x4929A0 (state 0): +9 = 0, +6 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAF_Start(void) {
    S()[9] = 0;
    S()[6] = 0x10;
    NextState();
}

// original 0x4929C0 (state 1): +9 up 0x10; at 0 (the byte wrapped) the screen
// drawn at 0xFF and +1 up, else drawn at +9.
extern "C" void __cdecl EffectKindAF_FadeIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 0x10);
    s = S();
    const unsigned char shade = s[9];
    if (shade == 0) {
        SH_CALL(EffectKindAF_DrawScreen)(0xFF);
        NextState();
        return;
    }
    SH_CALL(EffectKindAF_DrawScreen)(shade);
}

// original 0x492A00 (state 2): +9 down 0x10; while Draw_PassFlags is set the
// screen drawn at +9; at +9 0: Effect_Release (a tail jmp) when Draw_PassFlags
// is 0, else +1 down (state 1 again).
extern "C" void __cdecl EffectKindAF_FadeOut(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 0xF0);
    if (Draw_PassFlags != 0) SH_CALL(EffectKindAF_DrawScreen)(S()[9]);
    s = S();
    if (s[9] != 0) return;
    if (Draw_PassFlags == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] - 1);
}

// original 0x4932E0 (cdecl): a semi-transparent tile over the screen - (0, 0),
// 320.0 by 240.0 (the floats at +0x14 / +0x18; +0x10 not written), the colour
// (shade, 0, 0) - committed 0x1C to slot 1. The 320.0 is DIV-0041's site
// 0x493308, not widened (docs/effect_4f.md section 6).
extern "C" void __cdecl EffectKindAF_DrawScreen(unsigned shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char c = static_cast<unsigned char>(shade);
    SetUL(p + 8, 0);
    SetUL(p + 0xC, 0);
    SetBits(p + 0x14, 0x43A00000u);   // 320.0f
    SetBits(p + 0x18, 0x43700000u);   // 240.0f
    p[4] = c;
    p[5] = 0;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// ===========================================================================
// Kind 0xB0: Effect_KindHandlers[0xB0] (0x655610), EffectKindB0_States (three)
// ===========================================================================

// original 0x492A50 (hidden in 0x492400): jmp [EffectKindB0_States + +1 * 4],
// unbounded. States: _Start, _Spawn, Effect_StateRelease.
extern "C" void __cdecl EffectKindB0_Run(void) {
    Dispatch("EffectKindB0_Run", AddressOf(EffectKindB0_States), EffectKindB0_States_count);
}

// original 0x492A70 (state 0): the word +0x2E = 0x12C, +9 = 0; the bars
// cleared; +1 up.
extern "C" void __cdecl EffectKindB0_Start(void) {
    SetWord(S() + 0x2E, 0x12C);
    S()[9] = 0;
    SH_CALL(EffectKindB0_ClearBars)();
    NextState();
}

// original 0x492AA0 (state 1): at +9 0 a bar started (EffectKindB0_NewBar) and
// +9 = (Rand & 0xF) + 0x18, else +9 down; while Draw_PassFlags is set the bars
// stepped (EffectKindB0_StepBars(0xF0), its answer unread) - and +1 up when
// Draw_PassFlags is 0, before or after.
extern "C" void __cdecl EffectKindB0_Spawn(void) {
    unsigned char* s = S();
    unsigned char count = s[9];
    if (count == 0) {
        SH_CALL(EffectKindB0_NewBar)();
        const U r = static_cast<U>(SH_CALL(Rand)());
        s = S();
        count = static_cast<unsigned char>((r & 0xFu) + 0x18u);
    } else {
        count = static_cast<unsigned char>(count - 1);
    }
    s[9] = count;
    if (Draw_PassFlags != 0) {
        SH_CALL(EffectKindB0_StepBars)(0xF0);
        if (Draw_PassFlags != 0) return;
    }
    NextState();
}

// original 0x493330: the sixteen bars of 6 at 0x92D820 out of use.
extern "C" void __cdecl EffectKindB0_ClearBars(void) {
    for (U i = 0; i < at::kBarCount; ++i) Bar(i)[0] = 0;
}

// original 0x493350: the first free bar (+0 0) of the sixteen in use, its
// state +2 0; none free, nothing.
extern "C" void __cdecl EffectKindB0_NewBar(void) {
    for (U i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] != 0) continue;
        b[0] = 1;
        b[2] = 0;
        return;
    }
}

// original 0x493370 (cdecl): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100),
// dtd 1, committed to slot 7); the map camera; each bar in use by its state +2:
// 0 - +1 = 4, +3 = 4, the word +4 = `length`'s low word, +2 = 1; 1 - +2 = 2; 2
// - the word +4 down 8, below 0 the bar out of use (+0 = 0); then drawn (0x4920F0,
// catalog part 6) unless its state is 0. al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKindB0_StepBars(unsigned length) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (U i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) continue;
        any = 1;
        switch (b[2]) {
        case 0: {
            const unsigned char state = b[2];
            b[1] = 4;
            b[3] = 4;
            SetWord(b + 4, length & 0xFFFFu);
            b[2] = static_cast<unsigned char>(state + 1);
            break;
        }
        case 1: b[2] = 2; break;
        case 2:
            SetWord(b + 4, Word(b + 4) + 0xFFF8u);
            if (static_cast<std::int16_t>(Word(b + 4)) < 0) b[0] = 0;
            break;
        default: break;
        }
        if (b[2] != 0) SH_AT(void (__cdecl*)(unsigned char*), at::kBarDraw)(b);
    }
    return any;
}

// ===========================================================================
// Kind 0xB1: Effect_KindHandlers[0xB1] (0x655614), EffectKindB1_States (three)
// ===========================================================================

// original 0x493430 (hidden in 0x493370): call [EffectKindB1_States + +1 * 4],
// unbounded; then MapView_Redraw = 2.
extern "C" void __cdecl EffectKindB1_Run(void) {
    Entry("EffectKindB1_Run", AddressOf(EffectKindB1_States), EffectKindB1_States_count)();
    MapView_Redraw = 2;
}

// original 0x493450 (state 0): CameraTurn_Angles = Camera_Angles (each s16 <<
// 16), CameraTurn_Steps' three 0; MapView_Redraw = +9 (the dispatcher sets 2
// after); +1 = 1.
extern "C" void __cdecl EffectKindB1_Start(void) {
    const U a0 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles));
    const U a1 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles + 1));
    const U a2 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles + 2));
    CameraTurn_Angles[0] = static_cast<long>(a0 << 16);
    CameraTurn_Steps[0] = 0;
    CameraTurn_Steps[1] = 0;
    CameraTurn_Steps[2] = 0;
    CameraTurn_Angles[1] = static_cast<long>(a1 << 16);
    CameraTurn_Angles[2] = static_cast<long>(a2 << 16);
    unsigned char* const s = S();
    MapView_Redraw = s[9];
    s[1] = 1;
}

// original 0x4934B0 (state 1): Camera_Angles[1] up 4; MapView_SetElevation(it +
// 0xF0, a word); past 0xF0 (an s16, read again): Sound_PlayEffect(0x203) unless
// Field_Request is set, +1 = 2.
extern "C" void __cdecl EffectKindB1_Sway(void) {
    const U tilt = (Word(At(at::kTilt)) + 4u) & 0xFFFFu;
    SetWord(At(at::kTilt), tilt);
    SH_CALL(MapView_SetElevation)(static_cast<int>((tilt + 0xF0u) & 0xFFFFu));
    if (static_cast<std::int16_t>(Word(At(at::kTilt))) <= 0xF0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = 2;
}

// original 0x493500 (state 2): Camera_Angles[1] down 4; MapView_SetElevation(it
// + 0xF0, a word); below -0xF0 (an s16, read again): Sound_PlayEffect(0x203)
// unless Field_Request is set, +1 = 1.
extern "C" void __cdecl EffectKindB1_SwayBack(void) {
    const U tilt = (Word(At(at::kTilt)) + 0xFFFCu) & 0xFFFFu;
    SetWord(At(at::kTilt), tilt);
    SH_CALL(MapView_SetElevation)(static_cast<int>((tilt + 0xF0u) & 0xFFFFu));
    if (static_cast<std::int16_t>(Word(At(at::kTilt))) >= -0xF0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = 1;
}

// ===========================================================================
// Kind 0xB9: Effect_KindHandlers[0xB9] (0x655634), EffectKindB9_States (nine):
// a copy of kind 0x64 (effect_3a.cpp)
// ===========================================================================

// original 0x493550 (hidden in 0x493370): jmp [EffectKindB9_States + +1 * 4],
// unbounded. States: EffectKind64_Start (E3A's), _Grow, _Hold, _Rise, _Burst,
// _Launch, _Fly, _FlyWait, Effect_StateRelease.
extern "C" void __cdecl EffectKindB9_Run(void) {
    Dispatch("EffectKindB9_Run", AddressOf(EffectKindB9_States), EffectKindB9_States_count);
}

// original 0x493570 (state 1): the size +0x2E up 0x20; the glow drawn (E4E's
// 0x4901D0(+0x64, +0x2E, 6)); +9 down, at 0 +9 = 0x80, +1 up and
// Sound_PlayEffect(0x214).
extern "C" void __cdecl EffectKindB9_Grow(void) {
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0x20u);
    Glow(S());
    if (!CountDown()) return;
    S()[9] = 0x80;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x214);
}

// original 0x4935D0 (state 2): the glow drawn; +9 down, at 0 +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKindB9_Hold(void) {
    Glow(S());
    if (!CountDown()) return;
    S()[9] = 0x20;
    NextState();
}

// original 0x493610 (state 3): the glow's height +0x6C up 0xC0000; its size down
// 6; the glow drawn; +9 down, at 0: +9 = 0x20, +6 = 0, +1 up; the sparks cleared
// (EffectKind64_ClearSparks); the record's point +0x34.. the glow's; the 16
// shards at 0x92C040 started (EffectKind64_InitShard); Sound_PlayEffect(0x215).
extern "C" void __cdecl EffectKindB9_Rise(void) {
    SetUL(S() + 0x6C, UL(S() + 0x6C) + 0xC0000u);
    SetWord(S() + 0x2E, Word(S() + 0x2E) + 0xFFFAu);
    Glow(S());
    if (!CountDown()) return;
    S()[9] = 0x20;
    S()[6] = 0;
    NextState();
    SH_CALL(EffectKind64_ClearSparks)();
    SetUL(S() + 0x34, UL(S() + 0x64));
    SetUL(S() + 0x38, UL(S() + 0x68));
    SetUL(S() + 0x3C, UL(S() + 0x6C));
    for (U i = 0; i < at::kShardCount; ++i) SH_CALL(EffectKind64_InitShard)(Shard(i));
    SH_CALL(Sound_PlayEffect)(0x215);
}

// original 0x4936D0 (state 4): a spark every fourth frame; the shards and the
// sparks drawn (EffectKindB9_DrawShards, EffectKindB9_StepSparks); +9 down, at
// 0: the trail's head +0x18.. the glow's point, its speeds +0xC = -0x1000 and
// +0x10 = -0x400, +1 up.
extern "C" void __cdecl EffectKindB9_Burst(void) {
    SparkTick();
    SH_CALL(EffectKindB9_DrawShards)();
    SH_CALL(EffectKindB9_StepSparks)();
    if (!CountDown()) return;
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x64));
    SetUL(S() + 0x1C, UL(S() + 0x68));
    SetUL(S() + 0x20, UL(S() + 0x6C));
    SetUL(S() + 0xC, 0xFFFFF000u);
    SetUL(S() + 0x10, 0xFFFFFC00u);
    NextState();
}

// original 0x493750 (state 5): the same spark and draws; +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKindB9_Launch(void) {
    SparkTick();
    SH_CALL(EffectKindB9_DrawShards)();
    SH_CALL(EffectKindB9_StepSparks)();
    S()[9] = 0x20;
    NextState();
}

// original 0x493790 (state 6): the spark; the sparks and the shards drawn; the
// trail (EffectKindB9_DrawTrail(+0x18, +0x34, 0x40)); the speeds +0xC down
// 0x800 and +0x10 down 0x200, the head +0x18 / +0x1C moved by them; the shade
// +3 down 4; +9 down, at 0 +9 = 0x10 (kind 0x64's: 0x80), +1 up.
extern "C" void __cdecl EffectKindB9_Fly(void) {
    SparkTick();
    SH_CALL(EffectKindB9_StepSparks)();
    SH_CALL(EffectKindB9_DrawShards)();
    unsigned char* const s = S();
    SH_CALL(EffectKindB9_DrawTrail)(Point(s + 0x18), Point(s + 0x34), 0x40);
    SetUL(S() + 0xC, UL(S() + 0xC) + 0xFFFFF800u);
    SetUL(S() + 0x10, UL(S() + 0x10) + 0xFFFFFE00u);
    unsigned char* t = S();
    SetUL(t + 0x18, UL(t + 0x18) + UL(t + 0xC));
    t = S();
    SetUL(t + 0x1C, UL(t + 0x1C) + UL(t + 0x10));
    S()[3] = static_cast<unsigned char>(S()[3] + 0xFC);
    if (!CountDown()) return;
    S()[9] = 0x10;
    NextState();
}

// original 0x493850 (state 7): the spark; the sparks and the shards drawn; the
// trail (+0x18, +0x34, 0x40); while the chapter's count 0x903848 is 3:
// Sound_PlayEffect(0x216) at +9 0x10, +9 down, at 0 the shade +3 = 0x80 and +1
// up (to Effect_StateRelease).
extern "C" void __cdecl EffectKindB9_FlyWait(void) {
    SparkTick();
    SH_CALL(EffectKindB9_StepSparks)();
    SH_CALL(EffectKindB9_DrawShards)();
    unsigned char* const s = S();
    SH_CALL(EffectKindB9_DrawTrail)(Point(s + 0x18), Point(s + 0x34), 0x40);
    if (B(at::kCounter) != 3) return;
    if (S()[9] == 0x10) SH_CALL(Sound_PlayEffect)(0x216);
    if (!CountDown()) return;
    S()[3] = 0x80;
    NextState();
}

// original 0x4938E0: EffectKind64_DrawTrail's copy (the same instructions but
// one store order) - two semi-transparent POLY_G4 along the trail from `from`
// projected (a) to `to` projected (b), h the float at 0x5C41B8, each committed
// 0x44 to slot 2; then up to eight TILE_1 dots, (shade, shade, 0), from `to` less
// (Frame_Counter & 0xF) << 12 on x and << 10 on z, stepping 0x10000 and 0x4000
// back, while x is not below from's.
extern "C" void __cdecl EffectKindB9_DrawTrail(const long* from, const long* to, unsigned shade) {
    const unsigned char* const a = Bytes(from);
    const unsigned char* const b = Bytes(to);
    const LD half = F(At(at::kOne));
    alignas(4) unsigned char o[12];
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    StF(p + 8, F(o) + half);
    StF(p + 0xC, F(o + 4) - half);
    SetUL(p + 0x18, UL(o));
    SetUL(p + 0x1C, UL(o + 4));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    const unsigned char k = static_cast<unsigned char>(shade);
    StF(p + 0x28, F(o) + half);
    StF(p + 0x2C, F(o + 4) - half);
    SetUL(p + 0x38, UL(o));
    SetUL(p + 0x3C, UL(o + 4));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = k;
    p[0x15] = k;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = k;
    p[0x35] = k;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    StF(p + 0x18, F(o) - half);
    StF(p + 0x1C, F(o + 4) + half);
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, UL(o));
    SetUL(p + 0x2C, UL(o + 4));
    StF(p + 0x38, F(o) - half);
    StF(p + 0x3C, F(o + 4) + half);
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = k;
    p[5] = k;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = k;
    p[0x25] = k;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    const U f = Frame_Counter & 0xFu;
    alignas(4) unsigned char q[12];
    SetUL(q, UL(b) - (f << 12));
    SetUL(q + 4, UL(b + 4) - (f << 10));
    SetUL(q + 8, UL(b + 8));
    for (unsigned char dot = 0; dot < 8; ++dot) {
        if (Long(q) < Long(a)) return;
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o);
        p[4] = k;
        p[5] = k;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        SetUL(q, UL(q) - 0x10000u);
        SetUL(q + 4, UL(q + 4) - 0x4000u);
    }
}

// original 0x493B50 (kind 0x64's states 4..7 call it too): the first free of the
// eight sparks of 0x18 at 0x92BF80 - +0 1, +1 0, +2 life 0x10, +4.. the record's
// point +0x34.., the size +0x14 0x100, the colours +0x16 0x40 and +0x17 0; none
// free, nothing.
extern "C" void __cdecl EffectKindB9_SpawnSpark(void) {
    for (U i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] != 0) continue;
        const unsigned char* const s = S();
        k[0] = 1;
        k[1] = 0;
        k[2] = 0x10;
        SetUL(k + 4, UL(s + 0x34));
        SetUL(k + 8, UL(s + 0x38));
        SetUL(k + 0xC, UL(s + 0x3C));
        SetWord(k + 0x14, 0x100);
        k[0x16] = 0x40;
        k[0x17] = 0;
        return;
    }
}

// original 0x493BA0: EffectKind64_StepSparks' copy - EffectGte_LoadMapCamera;
// each spark in use: its size +0x14 down 0x10, its life +2 down (at 0 out of
// use), drawn (EffectKind64_DrawSpark). Answers 1 when a spark was in use, else 0.
extern "C" unsigned char __cdecl EffectKindB9_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned char life = k[2];
        SetWord(k + 0x14, Word(k + 0x14) + 0xFFF0u);
        any = 1;
        k[2] = static_cast<unsigned char>(life - 1);
        if (k[2] == 0) k[0] = 0;
        SH_CALL(EffectKind64_DrawSpark)(k);
    }
    return any;
}

// original 0x493BF0: EffectKind64_DrawShards' copy - a draw mode
// (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1, committed to slot 2);
// EffectGte_LoadMapCamera; each of the 16 shards at 0x92C040 drawn
// (EffectKindB9_DrawShard) and its angle +0x24 up 0x10.
extern "C" void __cdecl EffectKindB9_DrawShards(void) {
    DrawMode(1, 0x380, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kShardCount; ++i) {
        unsigned char* const shard = Shard(i);
        SH_CALL(EffectKindB9_DrawShard)(shard);
        SetWord(shard + 0x24, Word(shard + 0x24) + 0x10u);
    }
}

namespace {
// One of a shard's two far corners: its edge (the words at +e, +e + 2, +e + 4)
// turned by the angle +0x24 in x / y (sar 8), scaled by the speed +0x28, the z
// << 12 scaled likewise, added to the shard's point - each word read where the
// original reads it, between the calls.
void ShardCorner(unsigned char* shard, unsigned e, unsigned char* v) {
    U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(SW(shard + 0x24))));
    U e1 = c * SW(shard + e);
    U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(SW(shard + 0x24))));
    U t = s * SW(shard + e + 2);
    const U dx = Sar(e1 - t, 8) * SW(shard + 0x28);
    const U angle = SW(shard + 0x24);
    const U speed = SW(shard + 0x28);
    s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    e1 = s * SW(shard + e);
    c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(SW(shard + 0x24))));
    t = c * SW(shard + e + 2);
    const U dy = Sar(e1 + t, 8) * speed;
    const U dz = (SW(shard + e + 4) << 12) * speed;
    SetUL(v, dx + UL(shard));
    SetUL(v + 8, dz + UL(shard + 8));
    SetUL(v + 4, dy + UL(shard + 4));
}
}  // namespace

// original 0x493C60 (cdecl; EffectKind64_DrawShards' and EffectKindB9_DrawShards'):
// a shard of 0x2C (EffectKind64_InitShard's) drawn as a semi-transparent POLY_G3:
// its point projected, and its two edges (+0x10, +0x18) turned and scaled
// (ShardCorner) and projected; the point's colour (c, c, c >> 1) for c its shade
// +0x2A clamped to 0..0xFF, the corners black; committed 0x34 to slot 2.
extern "C" void __cdecl EffectKindB9_DrawShard(unsigned char* shard) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(Point(shard), Out(p + 8));
    alignas(4) unsigned char v[12];
    ShardCorner(shard, 0x10, v);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x18));
    ShardCorner(shard, 0x18, v);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x28));
    const auto w = static_cast<std::int16_t>(Word(shard + 0x2A));
    unsigned char c;
    if (w < 0)
        c = 0;
    else if (w > 0xFF)
        c = 0xFF;
    else
        c = shard[0x2A];
    p[4] = c;
    p[5] = c;
    p[6] = static_cast<unsigned char>(c >> 1);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x34);
}

// ===========================================================================
// Kind 0xBA: Effect_KindHandlers[0xBA] (0x655638), EffectKindBA_States (three)
// ===========================================================================

// original 0x493E10 (hidden in 0x493C60): jmp [EffectKindBA_States + +1 * 4],
// unbounded. States: _Start, 0x493E50 (part 6: the line drawn from the leader's
// ring), Effect_StateRelease.
extern "C" void __cdecl EffectKindBA_Run(void) {
    Dispatch("EffectKindBA_Run", AddressOf(EffectKindBA_States), EffectKindBA_States_count);
}

// original 0x493E30 (state 0): the word +0x2E = 0; +1 up; Sound_PlayEffect(0x215).
extern "C" void __cdecl EffectKindBA_Start(void) {
    SetWord(S() + 0x2E, 0);
    NextState();
    SH_CALL(Sound_PlayEffect)(0x215);
}

// original 0x493F70 (cdecl; kind 0xBA's state 1 0x493E50 draws it with the
// record's +0x34 and +0xC and 0x40): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1, committed to slot 1); the map camera; a semi-transparent LINE_F2 from
// `from` projected to `to` projected, (shade, shade, 0), committed 0x20 to slot 1.
extern "C" void __cdecl EffectKindBA_DrawLine(const long* from, const long* to, unsigned shade) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    Copy12(p + 8, o);
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    Copy12(p + 0x14, o);
    p[4] = static_cast<unsigned char>(shade);
    p[5] = static_cast<unsigned char>(shade);
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

void Effect4F_Inject() {
    if (bof3::WantsShadow("effect_4f")) effect_4f::SelfTest();
    BOF3_INJECT(EffectKindAA_Run);
    BOF3_INJECT(EffectKindAB_Run);
    BOF3_INJECT(EffectKindAB_Start);
    BOF3_INJECT(EffectKindAB_MoveDrops);
    BOF3_INJECT(EffectKindAB_DrawDrop);
    BOF3_INJECT(EffectKindAB_PlaceSources);
    BOF3_INJECT(EffectKindAB_Emit);
    BOF3_INJECT(EffectKindAC_Run);
    BOF3_INJECT(EffectKindAD_Run);
    BOF3_INJECT(Effect_StateNext);
    BOF3_INJECT(EffectKindAD_DrawArc);
    BOF3_INJECT(EffectKindAE_Run);
    BOF3_INJECT(EffectKindAE_Sink);
    BOF3_INJECT(EffectKindAE_Widen);
    BOF3_INJECT(EffectKindAE_Narrow);
    BOF3_INJECT(EffectKindAE_Shrink);
    BOF3_INJECT(EffectKindAE_Draw);
    BOF3_INJECT(Effect_DrawEllipse);
    BOF3_INJECT(EffectKindAF_Run);
    BOF3_INJECT(EffectKindAF_Start);
    BOF3_INJECT(EffectKindAF_FadeIn);
    BOF3_INJECT(EffectKindAF_FadeOut);
    BOF3_INJECT(EffectKindAF_DrawScreen);
    BOF3_INJECT(EffectKindB0_Run);
    BOF3_INJECT(EffectKindB0_Start);
    BOF3_INJECT(EffectKindB0_Spawn);
    BOF3_INJECT(EffectKindB0_ClearBars);
    BOF3_INJECT(EffectKindB0_NewBar);
    BOF3_INJECT(EffectKindB0_StepBars);
    BOF3_INJECT(EffectKindB1_Run);
    BOF3_INJECT(EffectKindB1_Start);
    BOF3_INJECT(EffectKindB1_Sway);
    BOF3_INJECT(EffectKindB1_SwayBack);
    BOF3_INJECT(EffectKindB9_Run);
    BOF3_INJECT(EffectKindB9_Grow);
    BOF3_INJECT(EffectKindB9_Hold);
    BOF3_INJECT(EffectKindB9_Rise);
    BOF3_INJECT(EffectKindB9_Burst);
    BOF3_INJECT(EffectKindB9_Launch);
    BOF3_INJECT(EffectKindB9_Fly);
    BOF3_INJECT(EffectKindB9_FlyWait);
    BOF3_INJECT(EffectKindB9_DrawTrail);
    BOF3_INJECT(EffectKindB9_SpawnSpark);
    BOF3_INJECT(EffectKindB9_StepSparks);
    BOF3_INJECT(EffectKindB9_DrawShards);
    BOF3_INJECT(EffectKindB9_DrawShard);
    BOF3_INJECT(EffectKindBA_Run);
    BOF3_INJECT(EffectKindBA_Start);
    BOF3_INJECT(EffectKindBA_DrawLine);
}
