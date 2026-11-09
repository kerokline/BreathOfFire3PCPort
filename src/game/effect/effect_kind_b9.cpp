// Moved here by tools/move_funcs.py from effect_4f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_b9.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using LD = long double;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }

unsigned char& B(U a) { return At(a)[0]; }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULEffect4f(from + i));
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
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect4f(table + 4 * state)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void DispatchEffect4f(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

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

unsigned char* Spark(U i) { return At(::bof3::Img(EffectKind30_Shards_addr) + i * at::kSparkStride); }

unsigned char* Shard(U i) { return At(::bof3::Img(EffectKind30_Shards_addr) + 0xc0 + i * at::kShardStride); }

// kind 0xB9's glow: group E4E's 0x4901D0 (point, size word, colour byte), raw
// until it merges.
void Glow(unsigned char* s) {
    using Draw = void (__cdecl*)(const long*, unsigned, unsigned);
    SH_AT(Draw, at::kGlowDraw.v)(Point(s + 0x64), Word(s + 0x2E), 6);
}

// The opening of kind 0xB9's states 4..7: the frame +6 up, and on every fourth
// (its value before, & 3 = 0) a spark.
void SparkTick() {
    unsigned char* const s = S();
    const unsigned char frame = s[6];
    s[6] = static_cast<unsigned char>(frame + 1);
    if ((frame & 3) == 0) SH_CALL(EffectKindB9_SpawnSpark)();
}

// One of a shard's two far corners: its edge (the words at +e, +e + 2, +e + 4)
// turned by the angle +0x24 in x / y (sar 8), scaled by the speed +0x28, the z
// << 12 scaled likewise, added to the shard's point - each word read where the
// original reads it, between the calls.
void ShardCorner(unsigned char* shard, unsigned e, unsigned char* v) {
    U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])))));
    U e1 = c * SW(shard + e);
    U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])))));
    U t = s * SW(shard + e + 2);
    const U dx = Sar(e1 - t, 8) * static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->speed));
    const U angle = static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2]));
    const U speed = static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->speed));
    s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    e1 = s * SW(shard + e);
    c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->angle[2])))));
    t = c * SW(shard + e + 2);
    const U dy = Sar(e1 + t, 8) * speed;
    const U dz = (SW(shard + e + 4) << 12) * speed;
    SetUL(v, dx + ULEffect4f(shard));
    SetUL(v + 8, dz + static_cast<std::uint32_t>(rv::As<records::EffectShard>(shard)->height));
    SetUL(v + 4, dy + static_cast<std::uint32_t>(rv::As<records::EffectShard>(shard)->z));
}

}  // namespace

// original 0x493550 (hidden in 0x493370): jmp [EffectKindB9_States + +1 * 4],
// unbounded. States: EffectKind64_Start (E3A's), _Grow, _Hold, _Rise, _Burst,
// _Launch, _Fly, _FlyWait, Effect_StateRelease.
extern "C" void __cdecl EffectKindB9_Run(void) {
    DispatchEffect4f("EffectKindB9_Run", AddressOf(EffectKindB9_States), EffectKindB9_States_count);
}

// original 0x493570 (state 1): the size +0x2E up 0x20; the glow drawn (E4E's
// 0x4901D0(+0x64, +0x2E, 6)); +9 down, at 0 +9 = 0x80, +1 up and
// Sound_PlayEffect(0x214).
extern "C" void __cdecl EffectKindB9_Grow(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, rv::As<records::EffectWork>(S())->work_2e + 0x20u);
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[2], rv::As<records::EffectObject>(S())->angle[2] + 0xC0000u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, rv::As<records::EffectWork>(S())->work_2e + 0xFFFAu);
    Glow(S());
    if (!CountDown()) return;
    S()[9] = 0x20;
    S()[6] = 0;
    NextState();
    SH_CALL(EffectKind64_ClearSparks)();
    rv::Current<records::EffectObject>()->x = rv::As<records::EffectObject>(S())->angle[0];
    rv::Current<records::EffectObject>()->z = rv::As<records::EffectObject>(S())->angle[1];
    rv::Current<records::EffectObject>()->height = rv::As<records::EffectObject>(S())->angle[2];
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, rv::As<records::EffectObject>(s)->angle[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, rv::As<records::EffectObject>(S())->angle[1]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, rv::As<records::EffectObject>(S())->angle[2]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0xFFFFF000u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0xFFFFFC00u);
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
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, rv::As<records::EffectWork>(S())->work_0c + 0xFFFFF800u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, rv::As<records::EffectWork>(S())->work_10 + 0xFFFFFE00u);
    unsigned char* t = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(t)->work_18, static_cast<std::uint32_t>(rv::As<records::EffectObject>(t)->work_18) + rv::As<records::EffectWork>(t)->work_0c);
    t = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(t)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(t)->work_1c) + rv::As<records::EffectWork>(t)->work_10);
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
    if (B(::bof3::Img(at::kCounter.v)) != 3) return;
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
    SetUL(p + 0x18, ULEffect4f(o));
    SetUL(p + 0x1C, ULEffect4f(o + 4));
    SetUL(p + 0x20, ULEffect4f(o + 8));
    SetUL(p + 0x10, ULEffect4f(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    const unsigned char k = static_cast<unsigned char>(shade);
    StF(p + 0x28, F(o) + half);
    StF(p + 0x2C, F(o + 4) - half);
    SetUL(p + 0x38, ULEffect4f(o));
    SetUL(p + 0x3C, ULEffect4f(o + 4));
    SetUL(p + 0x40, ULEffect4f(o + 8));
    SetUL(p + 0x30, ULEffect4f(o + 8));
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
    SetUL(p + 8, ULEffect4f(o));
    SetUL(p + 0xC, ULEffect4f(o + 4));
    StF(p + 0x18, F(o) - half);
    StF(p + 0x1C, F(o + 4) + half);
    SetUL(p + 0x20, ULEffect4f(o + 8));
    SetUL(p + 0x10, ULEffect4f(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, ULEffect4f(o));
    SetUL(p + 0x2C, ULEffect4f(o + 4));
    StF(p + 0x38, F(o) - half);
    StF(p + 0x3C, F(o + 4) + half);
    SetUL(p + 0x40, ULEffect4f(o + 8));
    SetUL(p + 0x30, ULEffect4f(o + 8));
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
    SetUL(q, ULEffect4f(b) - (f << 12));
    SetUL(q + 4, ULEffect4f(b + 4) - (f << 10));
    SetUL(q + 8, ULEffect4f(b + 8));
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
        SetUL(q, ULEffect4f(q) - 0x10000u);
        SetUL(q + 4, ULEffect4f(q + 4) - 0x4000u);
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
        auto* const so = rv::AsConst<records::EffectObject>(s);
        k[0] = 1;
        k[1] = 0;
        k[2] = 0x10;
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->x, static_cast<std::uint32_t>(so->x));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->z, static_cast<std::uint32_t>(so->z));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->height, static_cast<std::uint32_t>(so->height));
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->size, 0x100);
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
        rv::Store<std::uint32_t>(rv::As<records::EffectGlowSpark>(k)->size, rv::As<records::EffectGlowSpark>(k)->size + 0xFFF0u);
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
        rv::Store<std::uint32_t>(rv::As<records::EffectShard>(shard)->angle[2], rv::As<records::EffectShard>(shard)->angle[2] + 0x10u);
    }
}

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
    const auto w = static_cast<std::int16_t>(rv::As<records::EffectShard>(shard)->shade);
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

void EffectKindB9_Inject() {
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
}
