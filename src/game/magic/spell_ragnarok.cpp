// Moved here by tools/move_funcs.py from magic_s25.cpp (docs/source-layout-round.md).
#include "game/magic/spell_ragnarok.h"

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

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

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

// A .data phase table read in place: the original's `call` / `jmp
// [table + phase * 4]`, bounded.
Handler TableEntry(const char* who, std::uint32_t table, unsigned entries, unsigned phase) {
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table 0x%X", who, phase, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))));
}

}  // namespace

#define S25_EXPORT extern "C" __attribute__((disable_tail_calls))

// The sprite's jitter: screen x 0x9F + Rand() & 3.
void Jitter() {
    const int r = MH_CALL(Rand)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<unsigned>((r & 3) + 0x9F));
}

// original 0x4D5300 (Magic_Rows row 38): phase +1 through a six-entry stack
// table - start, wait for the children, fade in, burst, fade out, then the
// engine's 0x43FE80 (the done flag, free).
S25_EXPORT void __cdecl SpellRagnarok_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {
        bof3::addr::SpellRagnarok_Start, bof3::addr::SpellRagnarok_WaitChildren, bof3::addr::SpellRagnarok_FadeIn,
        bof3::addr::SpellRagnarok_Burst, bof3::addr::SpellRagnarok_FadeOut,      bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("SpellRagnarok_Task: phase %u, past the six-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D5350: the sprite to the middle of the target side (0x4FC0E0);
// the owner's facing +8; +0xB 0, +9 0x58, on; two kind-1 children 0x10 - a
// ring (+1 = 1) and a rising sprite (+1 = 0, +9 0x28) - with +0xB counted up
// three times (the ring once, the sprite twice: what the children count down
// before SpellRagnarok_WaitChildren moves on at 2); the CLUT strips at
// 0x80E980 (256 words) and 0x80B980 (16) copied to 0x812980 and 0x80F980 with
// the semi-transparency bit, their first entries without it;
// Gfx_ClutStripDirty; sound 0x100.
S25_EXPORT void __cdecl SpellRagnarok_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    Sprite_Current[8] = Pointer(::bof3::Img(BattleE2_Owner_addr))[8];
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x58;
    Bump(Sprite_Current[1]);
    unsigned slot = MH_CALL(BattleTask_Create)(1, 0x10);
    unsigned char* sc = Sprite_Current;
    unsigned char* child = Task(slot);
    SetPointer(child + 0x80, sc);
    child[1] = 1;
    Bump(sc[0xB]);
    slot = MH_CALL(BattleTask_Create)(1, 0x10);
    sc = Sprite_Current;
    child = Task(slot);
    SetPointer(child + 0x80, sc);
    child[1] = 0;
    child[9] = 0x28;
    Bump(sc[0xB]);
    Bump(Sprite_Current[0xB]);
    for (std::uint32_t i = 0; i < 0x100; ++i) SetWord(Mem(::bof3::Img(0x812980) + 2 * i), Word(Mem(::bof3::Img(0x80E980) + 2 * i)) | 0x8000u);
    for (std::uint32_t i = 0; i < 0x10; ++i) SetWord(Mem(::bof3::Img(0x80F980) + 2 * i), Word(Mem(::bof3::Img(0x80B980) + 2 * i)) | 0x8000u);
    const std::uint16_t first = Word(Mem(::bof3::Img(0x80E980))), first16 = Word(Mem(::bof3::Img(0x80B980)));
    SetWord(Mem(::bof3::Img(0x812980)), first);
    SetWord(Mem(::bof3::Img(0x80F980)), first16);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4D5460 (phase 1): when +0xB is down to 2, sound 0x101, +9 and
// +0xA 0, on.
S25_EXPORT void __cdecl SpellRagnarok_WaitChildren(void) {
    if (Sprite_Current[0xB] != 2) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D54A0 (phase 2): the tint drawn; on odd frames +0xA up by 2
// while below 0x10, +9 up, on at 0x10.
S25_EXPORT void __cdecl SpellRagnarok_FadeIn(void) {
    MH_CALL(SpellRagnarok_DrawScreenTint)();
    if ((Frame_Counter & 1) == 0) return;
    unsigned char* sc = Sprite_Current;
    const unsigned char a = sc[0xA];
    if (a < 0x10) {
        sc[0xA] = static_cast<unsigned char>(a + 2);
        sc = Sprite_Current;
    }
    Bump(sc[9]);
    unsigned char* const again = Sprite_Current;
    if (again[9] == 0x10) Bump(again[1]);
}

// original 0x4D54E0 (phase 3): the tint drawn; once +0xB is 0, the target
// flagged 0x10 and six kind-1 children 0x10 (+1 = 2, the sparks, +0xB 0..5),
// each counted on +0xB; on.
S25_EXPORT void __cdecl SpellRagnarok_Burst(void) {
    MH_CALL(SpellRagnarok_DrawScreenTint)();
    if (Sprite_Current[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(at::kTarget))[0], 0x10);
    for (unsigned char b = 0; b < 6; ++b) {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x10);
        unsigned char* const sc = Sprite_Current;
        unsigned char* const child = Task(slot);
        SetPointer(child + 0x80, sc);
        child[1] = 2;
        child[0xB] = b;
        Bump(sc[0xB]);
    }
    Bump(Sprite_Current[1]);
}

// original 0x4D5560 (phase 4): the tint drawn; every fourth frame +0xA down
// while not 0, +9 down; at 0 the target flagged 0x40 and on.
S25_EXPORT void __cdecl SpellRagnarok_FadeOut(void) {
    MH_CALL(SpellRagnarok_DrawScreenTint)();
    if (Frame_Counter & 3) return;
    unsigned char* sc = Sprite_Current;
    const unsigned char a = sc[0xA];
    if (a != 0) {
        sc[0xA] = static_cast<unsigned char>(a - 1);
        sc = Sprite_Current;
    }
    sc[9] = static_cast<unsigned char>(sc[9] - 1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Bump(Sprite_Current[1]);
}

// original 0x4D55C0: two screen-wide gouraud quads (0..319 x 0..120, 0..319 x
// 120..359) in grey +0xA * 15 (the word 0x903854; +9 * 15 is stored there
// first and overwritten), semi-transparent under tpage 0x35, 0x15 after,
// slot 1. Under DIV-0041 x runs from Widescreen_FillX() to 319 + the columns
// (the PSP's rule for its 319-wide fills, -32..351); 0..319 while the fills
// are unarmed, so the fuzz compares the original's.
S25_EXPORT void __cdecl SpellRagnarok_DrawScreenTint(void) {
    constexpr std::int32_t k120 = 0x42F00000, k359 = 0x43B38000;   // 120.0f, 359.0f
    const auto x0 = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto k319 = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const sc = Sprite_Current;
    unsigned char* p = Gfx_PacketNext;
    SetWord(S(4), sc[9] * 15u);
    SetWord(S(4), sc[0xA] * 15u);
    const std::int32_t corners[2][8] = {{x0, 0, k319, 0, x0, k120, k319, k120},
                                        {x0, k120, k319, k120, x0, k359, k319, k359}};
    for (const auto& c : corners) {
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned k = 0; k < 4; ++k) {
            SetLong(p + 8 + 0x10 * k, c[2 * k]);
            SetLong(p + 0xC + 0x10 * k, c[2 * k + 1]);
        }
        for (unsigned o : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[o] = S(4)[0];
        MH_CALL(Gfx_CommitPrim)(1, 0x44);
        p = Gfx_PacketNext;
    }
    MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// original 0x4D5780 (kind-1 task 0x10): `jmp` through SpellRagnarok_ChildKinds
// (0x65B960, three entries) by +1 - the sprite, the ring, a spark.
S25_EXPORT void __cdecl SpellRagnarok_Child(void) {
    TableEntry("SpellRagnarok_Child", ::bof3::Img(bof3::addr::SpellRagnarok_ChildKinds), 3, Sprite_Current[1])();
}

// original 0x4D57A0 (child kind 0, the rising sprite): the sprite bank pointer
// 0x9039D8 at 0x8E3580 around its phase (+2 through SpellRagnarok_SpritePhases,
// 0x65B96C, five entries; the first is MAGIC130's 0x4E47F0, a count down of +9)
// and, from phase 2 while the slot's bit 0 is set, Sprite_UpdateScreen; then
// 0x8B3580 again.
S25_EXPORT void __cdecl SpellRagnarok_SpriteChild(void) {
    SetLong(Mem(::bof3::Img(0x9039D8)), ::bof3::Img(0x8E3580));
    TableEntry("SpellRagnarok_SpriteChild", ::bof3::Img(bof3::addr::SpellRagnarok_SpritePhases), 5, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if ((sc[0] & 1) && sc[2] >= 2) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(0x9039D8)), ::bof3::Img(0x8B3580));
}

// original 0x4D57E0 (sprite phase 1): the sprite record's set-up - screen
// point (0xA0, 0x12C), +0x24 0x80, +0x25 0x1D, +0x26 0, +0x27 0x1A, +0x28 1,
// scale +0x40 0x30000 / +0x44 0xC000, +0x48 2, +0x5C..+0x5F 0, +0x2A 0,
// +0x29 2, word +0x2C 0, +0x2B 1 - and Sprite_SetAnimation(0); +2 on.
S25_EXPORT void __cdecl SpellRagnarok_SpriteInit(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, 0xA0);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, 0x12C);
    Sprite_Current[0x25] = 0x1D;
    Sprite_Current[0x26] = 0;
    Sprite_Current[0x27] = 0x1A;
    Sprite_Current[0x28] = 1;
    Sprite_Current[0x24] = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x30000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0xC000);
    Sprite_Current[0x48] = 2;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x29] = 2;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sprite_Current[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Bump(Sprite_Current[2]);
}

// original 0x4D58C0 (sprite phase 2): jitter; up 2; scale +0x44 up 0x2000; on
// at 0x18000.
S25_EXPORT void __cdecl SpellRagnarok_SpriteRise(void) {
    Jitter();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) - 2u);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, rv::Current<records::BattleTask>()->scale_y + 0x2000);
    unsigned char* const sc = Sprite_Current;
    if (rv::As<records::BattleTask>(sc)->scale_y >= 0x18000) Bump(sc[2]);
}

// original 0x4D5910 (sprite phase 3): jitter; up 4; below y 0xB2 the owner's
// count +0xB down and on.
S25_EXPORT void __cdecl SpellRagnarok_SpriteClimb(void) {
    Jitter();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) - 4u);
    if (S16(Sprite_Current + 0x30) >= 0xB2) return;
    unsigned char* const owner = Pointer(::bof3::Img(BattleE2_Owner_addr));
    owner[0xB] = static_cast<unsigned char>(owner[0xB] - 1);
    Bump(Sprite_Current[2]);
}

// original 0x4D5960 (sprite phase 4): jitter; up 2; below y 0x80 the owner's
// count down and the slot freed.
S25_EXPORT void __cdecl SpellRagnarok_SpriteLeave(void) {
    Jitter();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) - 2u);
    if (S16(Sprite_Current + 0x30) >= 0x80) return;
    unsigned char* const owner = Pointer(::bof3::Img(BattleE2_Owner_addr));
    owner[0xB] = static_cast<unsigned char>(owner[0xB] - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D59A0 (child kind 1, the ring): +2 through
// SpellRagnarok_RingPhases (0x65B980, four entries: init, grow, MAGIC082's
// 0x4C0680 - on once the owner's +0xB is 2 or less - and MAGIC060's 0x4B1740,
// +9 down to the end); then, while live and past phase 0, the disc and rim
// under the actor matrix.
S25_EXPORT void __cdecl SpellRagnarok_RingChild(void) {
    TableEntry("SpellRagnarok_RingChild", ::bof3::Img(bof3::addr::SpellRagnarok_RingPhases), 4, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SpellRagnarok_DrawDisc)();
    MH_CALL(SpellRagnarok_DrawRim)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D59E0 (ring phase 0): the ring at the field's kind-2 point plus
// (0x60000, 0x60000), the owner's height; radius +0x14 0x100; +9 0x10; on.
S25_EXPORT void __cdecl SpellRagnarok_RingInit(void) {
    rv::Current<records::BattleTask>()->step_x = 0x60000;
    rv::Current<records::BattleTask>()->step_z = 0x60000;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::Current<records::BattleTask>()->step_x + Field_Kind2X);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::Current<records::BattleTask>()->step_z + Field_Kind2Z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->height);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    Sprite_Current[9] = 0x10;
    Bump(Sprite_Current[2]);
}

// original 0x4D5A50 (ring phase 1): radius up 0x40; on at 0xC00.
S25_EXPORT void __cdecl SpellRagnarok_RingGrow(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_y, rv::Current<records::BattleTask>()->step_y + 0x40);
    unsigned char* const sc = Sprite_Current;
    if (rv::As<records::BattleTask>(sc)->step_y == 0xC00) Bump(sc[2]);
}

// original 0x4D5A70: a disc of 64 gouraud triangles of radius (the word) +0x14
// on the matrix's ground plane, grey +9 * 4; tpage 0x55, slot 2.
S25_EXPORT void __cdecl SpellRagnarok_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(S(0), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
    SetWord(S(4), static_cast<unsigned>(sc[9]) << 2);
    int s = MH_CALL(Math_Sin)(0);
    SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, S16(S(0))))));
    int c = MH_CALL(Math_Cos)(0);
    const int y = Sar12(Mul(c, S16(S(0))));
    SetWord(V(0x14), 0);
    SetWord(V(0x12), static_cast<unsigned>(y));
    SetWord(V(0xC), 0);
    SetWord(V(4), 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t ax = Word(V(0x10)), cx = Word(V(0x12));
        SetWord(V(0), 0);
        SetWord(V(2), 0);
        SetWord(V(8), ax);
        SetWord(V(0xA), cx);
        s = MH_CALL(Math_Sin)(a);
        SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, S16(S(0))))));
        c = MH_CALL(Math_Cos)(a);
        SetWord(V(0x12), static_cast<unsigned>(Sar12(Mul(c, S16(S(0))))));
        Rtp3(p, 8, 0x18, 0x28);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned o : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[o] = S(4)[0];
        MH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x4D5C10: the disc's rim - 64 gouraud quads between radius +0x14
// and +0x14 + 0x80, dark inside and grey +9 * 4 at the edge; tpage 0x55,
// slot 2.
S25_EXPORT void __cdecl SpellRagnarok_DrawRim(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(S(0), Word(sc + 0x14));
    SetWord(S(2), Word(sc + 0x14) + 0x80u);
    SetWord(S(4), static_cast<unsigned>(sc[9]) << 2);
    int s = MH_CALL(Math_Sin)(0);
    SetWord(V(8), static_cast<unsigned>(Sar12(Mul(s, S16(S(2))))));
    int c = MH_CALL(Math_Cos)(0);
    SetWord(V(0xA), static_cast<unsigned>(Sar12(Mul(c, S16(S(2))))));
    s = MH_CALL(Math_Sin)(0);
    SetWord(V(0x18), static_cast<unsigned>(Sar12(Mul(s, S16(S(0))))));
    c = MH_CALL(Math_Cos)(0);
    const int y = Sar12(Mul(c, S16(S(0))));
    SetWord(V(0x1C), 0);
    SetWord(V(0x1A), static_cast<unsigned>(y));
    SetWord(V(0x14), 0);
    SetWord(V(0xC), 0);
    SetWord(V(4), 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t ax = Word(V(8)), cx = Word(V(0xA));
            SetWord(V(0), ax);
            SetWord(V(2), cx);
        }
        s = MH_CALL(Math_Sin)(a);
        SetWord(V(8), static_cast<unsigned>(Sar12(Mul(s, S16(S(2))))));
        c = MH_CALL(Math_Cos)(a);
        {
            const int prod = Mul(c, S16(S(2)));
            const std::uint16_t dx = Word(V(0x18));
            SetWord(V(0xA), static_cast<unsigned>(Sar12(prod)));
            const std::uint16_t ax = Word(V(0x1A));
            SetWord(V(0x10), dx);
            SetWord(V(0x12), ax);
        }
        s = MH_CALL(Math_Sin)(a);
        SetWord(V(0x18), static_cast<unsigned>(Sar12(Mul(s, S16(S(0))))));
        c = MH_CALL(Math_Cos)(a);
        SetWord(V(0x1A), static_cast<unsigned>(Sar12(Mul(c, S16(S(0))))));
        Rtp4(p, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned o : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[o] = 1;
        for (unsigned o : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[o] = S(4)[0];
        MH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
}

// original 0x4D5E40 (child kind 2, a spark): +2 through
// SpellRagnarok_SparkPhases (0x65B990, two entries); then, while live and past
// phase 0, the spark under the actor matrix.
S25_EXPORT void __cdecl SpellRagnarok_SparkChild(void) {
    TableEntry("SpellRagnarok_SparkChild", ::bof3::Img(bof3::addr::SpellRagnarok_SparkPhases), 2, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SpellRagnarok_DrawSpark)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D5E80 (spark phase 0): the spark at the field's kind-2 point
// plus SpellRagnarok_SparkOffsets[+0xB] (0x65B998, (x, z) dwords; +0xB is not
// checked), the owner's height; +9 0x60, +0xA 0x40; on.
S25_EXPORT void __cdecl SpellRagnarok_SparkInit(void) {
    const std::uint32_t offsets = ::bof3::Img(bof3::addr::SpellRagnarok_SparkOffsets);
    rv::Store(rv::Current<records::BattleTask>()->x, Long(Mem(offsets + Sprite_Current[0xB] * 8u)) + Field_Kind2X);
    rv::Store(rv::Current<records::BattleTask>()->z, Long(Mem(offsets + 4 + Sprite_Current[0xB] * 8u)) + Field_Kind2Z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->height);
    Sprite_Current[9] = 0x60;
    Sprite_Current[0xA] = 0x40;
    Bump(Sprite_Current[2]);
}

// original 0x4D5EF0 (spark phase 1): +9 up 2, +0xA down; at 0 the slot freed.
S25_EXPORT void __cdecl SpellRagnarok_SparkFade(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 2);
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    if (Sprite_Current[0xA] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D5F20: the spark - one textured flat quad (page (0x340, 0x100),
// clut (0, 0x1E2), the texture (0, 0x80)..(0x20, 0xA0)) of radius +9 * 3 with
// corners at 0x200 / 0x600 / 0xE00 / 0xA00 on the ground plane, grey +0xA *
// 2; slot 3.
S25_EXPORT void __cdecl SpellRagnarok_DrawSpark(void) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const sc = Sprite_Current;
    SetWord(S(0), sc[9] * 3u);
    SetWord(S(4), static_cast<unsigned>(sc[0xA]) << 1);
    const int angles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        const int c = MH_CALL(Math_Cos)(angles[k]);
        SetWord(V(8 * k), static_cast<unsigned>(Sar12(Mul(c, S16(S(0))))));
        const int s = MH_CALL(Math_Sin)(angles[k]);
        SetWord(V(8 * k + 2), static_cast<unsigned>(Sar12(Mul(s, S16(S(0))))));
        SetWord(V(8 * k + 4), 0);
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x14] = 0;
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = S(4)[0];
    p[5] = S(4)[0];
    p[6] = S(4)[0];
    Rtp4(p, 8, 0x18, 0x28, 0x38);
    MH_CALL(Gte_PrimDepths4_10)(p);
    MH_CALL(Gfx_CommitPrim)(3, 0x48);
}

void SpellRagnarok_Inject() {
    BOF3_INJECT(SpellRagnarok_Task);
    BOF3_INJECT(SpellRagnarok_Start);
    BOF3_INJECT(SpellRagnarok_WaitChildren);
    BOF3_INJECT(SpellRagnarok_FadeIn);
    BOF3_INJECT(SpellRagnarok_Burst);
    BOF3_INJECT(SpellRagnarok_FadeOut);
    BOF3_INJECT(SpellRagnarok_DrawScreenTint);
    BOF3_INJECT(SpellRagnarok_Child);
    BOF3_INJECT(SpellRagnarok_SpriteChild);
    BOF3_INJECT(SpellRagnarok_SpriteInit);
    BOF3_INJECT(SpellRagnarok_SpriteRise);
    BOF3_INJECT(SpellRagnarok_SpriteClimb);
    BOF3_INJECT(SpellRagnarok_SpriteLeave);
    BOF3_INJECT(SpellRagnarok_RingChild);
    BOF3_INJECT(SpellRagnarok_RingInit);
    BOF3_INJECT(SpellRagnarok_RingGrow);
    BOF3_INJECT(SpellRagnarok_DrawDisc);
    BOF3_INJECT(SpellRagnarok_DrawRim);
    BOF3_INJECT(SpellRagnarok_SparkChild);
    BOF3_INJECT(SpellRagnarok_SparkInit);
    BOF3_INJECT(SpellRagnarok_SparkFade);
    BOF3_INJECT(SpellRagnarok_DrawSpark);
}
