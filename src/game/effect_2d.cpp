// Effect kinds 0x45..0x49 - round thirteen, wave two, group E2D: the 54
// functions of the cut (analysis/round13_cut.tsv) in 0x4771B0..0x4789C1, each
// read with capstone to its last instruction (docs/effect_2d.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x45  a 72 x 72 panel at (0x82, 0xA) - two filled squares, a border,
//              a grid - and a trace of 36 samples of a sine wave; state 1 slows
//              the wave and grows it while the flag 0x25 of the chapter's row is
//              set, state 2 reads the confirm button and sets or clears the flag
//              0x26 by where the wave stands (chapter 6's run 13 spawns it,
//              scena_sc6.cpp);
//   kind 0x46  a screen-wide tile whose colour steps up and down by 4 over 12
//              frames, five times, then the chapters' counter byte 0x903848 up;
//              every field sprite's screen place redrawn each frame;
//   kind 0x47  two free sprites put beside the leader (or sprite object 2 when
//              +0xB is 0), tinted red and blue, the leader (or object 2) hidden
//              for four frames (area 85's handlers 3 and 4, area_w2b.cpp);
//   kind 0x48  rings of four sparks every four frames, +0xB rings, then the
//              sparks run out (area 52's handlers 6 and 7, area_w1c.cpp);
//   kind 0x49  ten variants picked by +1 at the spawn (chapter 7,
//              Scena07_TakeEffect49): 0 puffs, 1 a CLUT fade, 2 a glow flown to a
//              point, 3 dust, 4 (and 5..9, E2E's) by +2.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, writes a sprite record past the pool on an index the
// callee never gives, or would loop forever, ours aborts with a message
// (docs/effect_2d.md section 2).
#include "game/effect_2d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U AddressOf(const void* p) { return Low32(p); }
void StoreFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
// The x87 at the game's 53-bit precision: a float plus or less a whole number
// rounded once to 53 bits, then to a float at the store.
void StoreSum(unsigned char* p, float a, int b) { StoreFloat(p, static_cast<float>(static_cast<double>(a) + static_cast<double>(b))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + which]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table, or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned which) {
    const unsigned state = Sprite_Current[which];
    if (state >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2d.md section 2)",
                    who, which, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * effect_2d::at::kSparkStride; }

}  // namespace

// ===========================================================================
// Kind 0x45: Effect_KindHandlers[0x45] (0x655464), EffectKind45_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x46: Effect_KindHandlers[0x46] (0x655468), EffectKind46_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x47: Effect_KindHandlers[0x47] (0x65546C), EffectKind47_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x48: Effect_KindHandlers[0x48] (0x655470), EffectKind48_States (three)
// ===========================================================================

// original 0x4781B0 (0x12 bytes): jmp [EffectKind48_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind48_Run(void) { Dispatch("EffectKind48_Run", AddressOf(EffectKind48_States), EffectKind48_States_count, 1); }

// original 0x4781D0 (state 0): another live kind-0x48 record in the pool
// (Sprite_Current read once): this one released. Else +6 (the rings thrown) 0,
// +9 0, the sparks cleared, Sound_PlayEffect(0x203), +1 up.
extern "C" void __cdecl EffectKind48_Start(void) {
    unsigned char* const self = S();
    for (unsigned i = 0; i < at::kEffects; ++i) {
        const unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
        if (e[0] != 0 && e[5] == 0x48 && e != self) {
            SH_CALL(Effect_Release)();
            return;
        }
    }
    self[6] = 0;
    S()[9] = 0;
    SH_CALL(EffectKind48_ClearSparks)();
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x478230 (state 1): a ring spawned when +9 is 0; +9 up, at 4 back to
// 0 and +6 up; the sparks stepped; +6 at +0xB: +1 up.
extern "C" void __cdecl EffectKind48_Burst(void) {
    if (S()[9] == 0) SH_CALL(EffectKind48_SpawnRing)();
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (s[9] == 4) {
        s[9] = 0;
        s[6] = static_cast<unsigned char>(s[6] + 1);
    }
    SH_CALL(EffectKind48_StepSparks)();
    s = S();
    if (s[6] == s[0xB]) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x478280 (state 2): the sparks stepped; none live: Effect_Release (a
// tail jump).
extern "C" void __cdecl EffectKind48_Fade(void) {
    if (SH_CALL(EffectKind48_StepSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478290: four sparks at the angles 0x80, 0x180, 0x280, 0x380, each
// the first free (none: skipped): +0x15 = 1 (live), +0x16 = 8 (its life), x =
// Math_Cos(angle) << 4 + Sprite_Current's x, z = Math_Sin(angle) << 4 + its z
// (Sprite_Current read after each call), the height its height, +0x10 = 0x1800
// (the size), +0x14 = 0xC0 (the shade).
extern "C" void __cdecl EffectKind48_SpawnRing(void) {
    U angle = 0x80;
    for (unsigned k = 0; k < 4; ++k, angle += 0x100) {
        unsigned char* const spark = SH_CALL(EffectKind48_FindFreeSpark)();
        if (spark == nullptr) continue;
        spark[0x15] = 1;
        const U a = angle & 0xFFFFu;
        spark[0x16] = 8;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        SetUL(spark, (c << 4) + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        SetUL(spark + 4, (sn << 4) + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
        const U height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
        SetUL(spark + 0x10, 0x1800);
        SetUL(spark + 8, height);
        spark[0x14] = 0xC0;
    }
}

// original 0x478320: the first of the 16 sparks of 0x18 at EffectKind30_Shards
// whose +0x15 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind48_FindFreeSpark(void) {
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i)
        if (Spark(i)[0x15] == 0) return Spark(i);
    return nullptr;
}

// original 0x478340: +0x15 of the 16 sparks 0.
extern "C" void __cdecl EffectKind48_ClearSparks(void) {
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i) Spark(i)[0x15] = 0;
}

// original 0x478360: the map camera loaded (EffectGte_LoadMapCamera); each live
// spark (+0x15) drawn, then its height +0x3000, its shade - 0x18, its life down
// and at 0 +0x15 = 0. Answers 1 in al when any spark was live, else 0.
extern "C" unsigned char __cdecl EffectKind48_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i) {
        unsigned char* const spark = Spark(i);
        if (spark[0x15] == 0) continue;
        SH_CALL(EffectKind48_DrawSpark)(spark);
        const U height = UL(spark + 8) + 0x3000u;
        const unsigned char shade = static_cast<unsigned char>(spark[0x14] + 0xE8);
        const unsigned char life = static_cast<unsigned char>(spark[0x16] - 1);
        SetUL(spark + 8, height);
        spark[0x14] = shade;
        spark[0x16] = life;
        if (life == 0) spark[0x15] = 0;
        any = 1;
    }
    return any;
}

// original 0x4783C0: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), committed
// Gfx_CommitPrim(1, 0xC)); a POLY_FT4 at the new Gfx_PacketNext,
// semi-transparent. The spark's point (x, z, height << 8) projected
// (EffectGte_ProjectPoint: screen x, y, depth) and its size (+0x10 >> 6, the
// low word) scaled at that depth (EffectGte_ProjectSize: h); the corners x - h
// .. x + h, y - h .. y + h (the x87 at 53 bits, rounded once to floats), the
// depth at each; u 0xE0 / 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0,
// 0x1E3) at +0x16, the page again at +0x26; the colour the shade +0x14;
// Gfx_CommitPrim(1, 0x48). The original hands EffectGte_ProjectSize a size
// whose second word is its uninitialised stack and never reads that quotient;
// ours passes 0 there.
extern "C" void __cdecl EffectKind48_DrawSpark(const unsigned char* spark) {
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    short size[2] = {static_cast<short>(UL(spark + 0x10) >> 6), 0};
    const long point[3] = {Long(spark), Long(spark + 4), static_cast<long>(UL(spark + 8) << 8)};
    short scaled[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, scaled);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    const int h = scaled[0];
    StoreSum(prim + 0x28, screen[0], -h);
    StoreSum(prim + 0x08, screen[0], -h);
    StoreSum(prim + 0x38, screen[0], h);
    StoreSum(prim + 0x18, screen[0], h);
    StoreSum(prim + 0x1C, screen[1], -h);
    StoreSum(prim + 0x0C, screen[1], -h);
    StoreSum(prim + 0x3C, screen[1], h);
    StoreSum(prim + 0x2C, screen[1], h);
    std::memcpy(prim + 0x40, &screen[2], 4);
    std::memcpy(prim + 0x30, &screen[2], 4);
    std::memcpy(prim + 0x20, &screen[2], 4);
    std::memcpy(prim + 0x10, &screen[2], 4);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    prim[4] = spark[0x14];
    prim[5] = spark[0x14];
    prim[6] = spark[0x14];
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ===========================================================================
// Kind 0x49: Effect_KindHandlers[0x49] (0x655474), EffectKind49_Variants (ten)
// ===========================================================================

// original 0x478550 (0x12 bytes): jmp [EffectKind49_Variants + +1 * 4],
// unbounded. +1 is the variant Scena07_TakeEffect49 stored; entries 5..9 are
// group E2E's.
extern "C" void __cdecl EffectKind49_Run(void) {
    Dispatch("EffectKind49_Run", AddressOf(EffectKind49_Variants), EffectKind49_Variants_count, 1);
}

// original 0x478570: variant 0, jmp [EffectKind49_V0States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V0Run(void) {
    Dispatch("EffectKind49_V0Run", AddressOf(EffectKind49_V0States), EffectKind49_V0States_count, 2);
}

// original 0x478590 (variant 0, state 0): the puffs cleared (0x4790C0); the
// dword +0xC = 0x40; +2 up.
extern "C" void __cdecl EffectKind49_V0Start(void) {
    SH_AT(void (__cdecl*)(), at::kPuffsClear.v)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x40);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4785B0 (variant 0, state 1): on a count +0xC with its low three
// bits 0, a free puff (0x47CF20) started (0x4790F0); the puffs stepped
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind49_V0Emit(void) {
    if ((S()[0xC] & 7) == 0) {
        unsigned char* const puff = SH_AT(unsigned char* (__cdecl*)(), at::kPuffFindFree.v)();
        if (puff != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kPuffStart.v)(puff);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep.v)();
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_0c == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4785F0 (variant 0, state 2): the puffs stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V0End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep.v)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478600: variant 1, jmp [EffectKind49_V1States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V1Run(void) {
    Dispatch("EffectKind49_V1Run", AddressOf(EffectKind49_V1States), EffectKind49_V1States_count, 2);
}

// original 0x478620 (variant 1, state 0): the dword +0x10 = -8; +2 up.
extern "C" void __cdecl EffectKind49_V1Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0xFFFFFFF8u);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x478640 (variant 1, state 1): Area85_ClutShift(+0x10); +0x10 up 1 on
// a frame a multiple of 16; at 0 Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V1Fade(void) {
    SH_CALL(Area85_ClutShift)(static_cast<std::int32_t>(rv::As<records::EffectWork>(S())->work_10));
    const U up = (Frame_Counter & 0xFu) == 0 ? 1u : 0u;
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, rv::As<records::EffectWork>(s)->work_10 + up);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_10 == 0) SH_CALL(Effect_Release)();
}

// original 0x478680: variant 2, jmp [EffectKind49_V2States + +2 * 4],
// unbounded; entries 5 and 6 are WeretigerFx_Next (+2 on), 7 Effect_StateRelease.
extern "C" void __cdecl EffectKind49_V2Run(void) {
    Dispatch("EffectKind49_V2Run", AddressOf(EffectKind49_V2States), EffectKind49_V2States_count, 2);
}

// original 0x4786A0 (variant 2, state 0): the glow's word 0x92C49E = 0xC0; the
// dword +0x14 (a speed) 0, then 16 times: +0x14 up 0x800000, x +0x34 up
// 0x10000, the height +0x3C up +0x14 - the glow lifted along its arc at once;
// Sound_PlayEffect(0x202); +0x14 negated; +2 up.
extern "C" void __cdecl EffectKind49_V2Launch(void) {
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), 0xC0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    for (unsigned i = 0; i < 16; ++i) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y) + 0x800000u);
        rv::Current<records::EffectObject>()->x = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) + 0x10000u;
        rv::Current<records::EffectObject>()->height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y);
    }
    SH_CALL(Sound_PlayEffect)(0x202);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0u - static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// Variant 2's glow record 0x92C060 stepped (0x4794D0) and drawn (0x4796B0).
void Glow() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowDraw.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
}
}  // namespace

// original 0x478720 (variant 2, state 1): the glow; x +0x34 down 0x10000, the
// height +0x3C up the speed +0x14, a speed not 0 up 0x800000; at the target
// (+0x34 = +0xC and +0x38 = +0x10) +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind49_V2Fly(void) {
    Glow();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->x = static_cast<std::uint32_t>(so->x) + 0xFFFF0000u;
    so->height = static_cast<std::uint32_t>(so->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    const U speed = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    if (speed != 0) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, speed + 0x800000u);
    if (static_cast<std::uint32_t>(so->x) == rv::As<records::EffectWork>(s)->work_0c && static_cast<std::uint32_t>(so->z) == rv::As<records::EffectWork>(s)->work_10) {
        s[9] = 0x2F;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x4787A0 (variant 2, state 2): the glow; +9 down; at 0
// Sound_PlayEffect(0x203), +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Wait(void) {
    Glow();
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[9] = 0x20;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4787F0 (variant 2, state 3): the glow; its word 0x92C49E up 6; +9
// down; at 0 +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Rise(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), Word(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e)) + 6u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    s[9] = 0x20;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478840 (variant 2, state 4): the glow; its word 0x92C49E down 0xC;
// +9 down; at 0 +2 up.
extern "C" void __cdecl EffectKind49_V2Sink(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), Word(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e)) + 0xFFF4u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478880: variant 3, jmp [EffectKind49_V3States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V3Run(void) {
    Dispatch("EffectKind49_V3Run", AddressOf(EffectKind49_V3States), EffectKind49_V3States_count, 2);
}

// original 0x4788A0 (variant 3, state 0): the height +0x3C = the ground's
// elevation at (+0x34, +0x38) (AreaMap_Elevation, its low word signed) << 16;
// the dust cleared (0x47A110); +9 = 0, +0xC = 0x5F, +2 up;
// Sound_PlayEffect(0x205).
extern "C" void __cdecl EffectKind49_V3Start(void) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long ground = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ground))) << 16);
    SH_AT(void (__cdecl*)(), at::kDustClear.v)();
    S()[9] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x5F);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x478900 (variant 3, state 1): with +9 the step: when the low byte of
// the count +0xC has none of the step's mask bits (0x6545EC[+9]), the word +0x10
// of what 0x6761D0 points at up the step's count (0x6545F4[+9]) and that many
// free dust records (0x47A130) started (0x47A150) - the count re-read by +9
// after each. Then a count with its low nibble 0 moves +9 on; the dust stepped
// (0x47A200); +0xC down, at 0 +2 up. The byte tables are read in place.
extern "C" void __cdecl EffectKind49_V3Burst(void) {
    unsigned char* s = S();
    const unsigned step = s[9];
    if ((s[0xC] & At(::bof3::Img(at::kBurstMask.v))[step]) == 0) {
        const unsigned char add = At(::bof3::Img(at::kBurstCount.v))[step];
        unsigned char* const anchor = At(Effect2d_DustAnchor);
        SetWord(anchor + 0x10, Word(anchor + 0x10) + add);
        s = S();
        if (At(::bof3::Img(at::kBurstCount.v))[s[9]] != 0) {
            unsigned char n = 0;
            do {
                unsigned char* const dust = SH_AT(unsigned char* (__cdecl*)(), at::kDustFindFree.v)();
                if (dust != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kDustStart.v)(dust);
                s = S();
                n = static_cast<unsigned char>(n + 1);
            } while (n < At(::bof3::Img(at::kBurstCount.v))[s[9]]);
        }
    }
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(unsigned char (__cdecl*)(), at::kDustStep.v)();
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_0c == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4789A0 (variant 3, state 2): the dust stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V3End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kDustStep.v)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4789B0: variant 4, jmp [EffectKind49_V4States + +2 * 4], unbounded
// (its four states are group E2E's).
extern "C" void __cdecl EffectKind49_V4Run(void) {
    Dispatch("EffectKind49_V4Run", AddressOf(EffectKind49_V4States), EffectKind49_V4States_count, 2);
}

void Effect2D_Inject() {
    if (bof3::WantsShadow("effect_2d")) effect_2d::SelfTest();
    // DIV-0188 guards the lighthouse gauge's judgement in EffectKind45_Tune
    // and _Aim; Capcom's would judge as before and the cheat would be silently
    // half off.
    if (Cheats_MinigameNoFail() && (!bof3::IsEnabled(bof3::addr::EffectKind45_Tune) || !bof3::IsEnabled(bof3::addr::EffectKind45_Aim)))
        bof3::Fatal("BOF3X_NO_FAIL=1 needs EffectKind45_Tune and EffectKind45_Aim ours, and one is disabled");
    BOF3_INJECT(EffectKind48_Run);
    BOF3_INJECT(EffectKind48_Start);
    BOF3_INJECT(EffectKind48_Burst);
    BOF3_INJECT(EffectKind48_Fade);
    BOF3_INJECT(EffectKind48_SpawnRing);
    BOF3_INJECT(EffectKind48_FindFreeSpark);
    BOF3_INJECT(EffectKind48_ClearSparks);
    BOF3_INJECT(EffectKind48_StepSparks);
    BOF3_INJECT(EffectKind48_DrawSpark);
    BOF3_INJECT(EffectKind49_Run);
    BOF3_INJECT(EffectKind49_V0Run);
    BOF3_INJECT(EffectKind49_V0Start);
    BOF3_INJECT(EffectKind49_V0Emit);
    BOF3_INJECT(EffectKind49_V0End);
    BOF3_INJECT(EffectKind49_V1Run);
    BOF3_INJECT(EffectKind49_V1Start);
    BOF3_INJECT(EffectKind49_V1Fade);
    BOF3_INJECT(EffectKind49_V2Run);
    BOF3_INJECT(EffectKind49_V2Launch);
    BOF3_INJECT(EffectKind49_V2Fly);
    BOF3_INJECT(EffectKind49_V2Wait);
    BOF3_INJECT(EffectKind49_V2Rise);
    BOF3_INJECT(EffectKind49_V2Sink);
    BOF3_INJECT(EffectKind49_V3Run);
    BOF3_INJECT(EffectKind49_V3Start);
    BOF3_INJECT(EffectKind49_V3Burst);
    BOF3_INJECT(EffectKind49_V3End);
    BOF3_INJECT(EffectKind49_V4Run);
}
