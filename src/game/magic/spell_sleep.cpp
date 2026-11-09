// Moved here by tools/move_funcs.py from magic_s25.cpp (docs/source-layout-round.md).
#include "game/magic/spell_sleep.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s25.h"
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

std::int32_t SL(unsigned k) { return Long(S(k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// imul (a 32-bit product that wraps), then sar 0xC.
int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

int Sar12(int v) { return v >> 12; }

int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

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

using PrimFn = void (__cdecl*)(unsigned char*);

// libgpu's SetPolyF3 (code 0x20) and SetLineG4 (code 0x5C), unnamed, in no
// group (psx_gpu's neighbours; docs/magic_s25.md section 7).
constexpr ::bof3::image::CodeVa kSetPolyF3{bof3::addr::Gpu_SetPolyF3};

constexpr ::bof3::image::CodeVa kSetLineG4{bof3::addr::Gpu_SetLineG4};

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

// One ring of the dome's vertex pair: (cos(ring) * r * sin(a), cos(ring) * r *
// cos(a), sin(ring) * r + 0x1C0) >> 12 with r the word scratch, into the
// SVECTOR at k. The original calls Cos(ring), Sin / Cos(a) in that order.
void DomePoint(unsigned k, int ring, int a) {
    int c = MH_CALL(Math_Cos)(ring);
    int e = Sar12(Mul(c, SL(0)));
    const int s = MH_CALL(Math_Sin)(a);
    SetWord(V(k), static_cast<unsigned>(Sar12(Mul(e, s))));
    c = MH_CALL(Math_Cos)(ring);
    e = Sar12(Mul(c, SL(0)));
    const int c2 = MH_CALL(Math_Cos)(a);
    SetWord(V(k + 2), static_cast<unsigned>(Sar12(Mul(e, c2))));
}

// The trail's point at step `step` (0..63): the angle ((sin(step << 6) << 9)
// >> 12) + 0x400 into 0x903858; returns it.
int TrailAngle(int step) {
    const int s = MH_CALL(Math_Sin)(step << 6);
    return Sar12(Shl(s, 9)) + 0x400;
}

// original 0x4D2D80 (Magic_Rows row 24): phase +1 through a two-entry stack
// table - SpellSleep_Start, then MAGIC078's 0x4BDC10 (free once the child has
// set the owner's +0xB to 0xFF).
S25_EXPORT void __cdecl SpellSleep_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::SpellSleep_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("SpellSleep_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D2DB0: the effect's sprite to the middle of the target side
// (0x4FC0E0); a kind-1 child 0x19 (SpellSleep_Child) owned by this task, its
// facing +8 from the record 0x904B3C points at, its x / z the field's kind-2
// point (Field_Kind2X / Z), its height this task's + 0x4000000; sound 0x100;
// +0xB 0; on.
S25_EXPORT void __cdecl SpellSleep_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x19);
    const unsigned char* const caster = Pointer(::bof3::Img(MagicS23_ActorSprite_addr));
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = Task(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    SetPointer(child + 0x80, sc);
    child[8] = caster[8];
    tchild->x = Field_Kind2X;
    tchild->z = Field_Kind2Z;
    rv::Store(tchild->height, rv::As<records::BattleTask>(sc)->height + 0x4000000);
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D2E30 (kind-1 task 0x19): phase +1 through SpellSleep_ChildPhases
// (0x65B918, four entries); then, while the slot is live, the draws under two
// matrices - the sway matrix: stem, dome, fan; the turn matrix: shadow fan,
// trail, trail lines.
S25_EXPORT void __cdecl SpellSleep_Child(void) {
    TableEntry("SpellSleep_Child", ::bof3::Img(bof3::addr::SpellSleep_ChildPhases), 4, Sprite_Current[1])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(SpellSleep_PushSwayMatrix)();
    MH_CALL(SpellSleep_DrawStem)();
    MH_CALL(SpellSleep_DrawDome)();
    MH_CALL(SpellSleep_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
    MH_CALL(SpellSleep_PushTurnMatrix)();
    MH_CALL(SpellSleep_DrawShadowFan)();
    MH_CALL(SpellSleep_DrawTrail)();
    MH_CALL(SpellSleep_DrawTrailLines)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D2E80 (child phase 0): +0xB, +9 and +0xA 0; on.
S25_EXPORT void __cdecl SpellSleep_ChildInit(void) {
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D2EB0 (child phase 1): +9 and +0xA up; on when +9 is 0x20.
S25_EXPORT void __cdecl SpellSleep_ChildGrow(void) {
    Bump(Sprite_Current[9]);
    Bump(Sprite_Current[0xA]);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x20) Bump(sc[1]);
}

// original 0x4D2EE0 (child phase 2): +9 up; on when it is 0xC0.
S25_EXPORT void __cdecl SpellSleep_ChildHold(void) {
    Bump(Sprite_Current[9]);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xC0) Bump(sc[1]);
}

// original 0x4D2F00 (child phase 3): +9 up, +0xA down; at 0 the owner's +0xB
// 0xFF (what MAGIC078's end phase waits for) and the slot freed.
S25_EXPORT void __cdecl SpellSleep_ChildFade(void) {
    Bump(Sprite_Current[9]);
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    if (Sprite_Current[0xA] != 0) return;
    Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D2F40: pushes the matrix and loads the actor matrix rocked by
// (sin(((+9 & 0x3F) << 6)) << 9) >> 12 about y when the facing +8 is odd,
// about x when even.
S25_EXPORT void __cdecl SpellSleep_PushSwayMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short angles[3];
    const unsigned char* const sc = Sprite_Current;
    if (sc[8] & 1) {
        angles[0] = 0;
        const int s = MH_CALL(Math_Sin)((sc[9] & 0x3F) << 6);
        angles[1] = static_cast<short>(Shl(s, 9) >> 12);
    } else {
        const int s = MH_CALL(Math_Sin)((sc[9] & 0x3F) << 6);
        angles[1] = 0;
        angles[0] = static_cast<short>(Shl(s, 9) >> 12);
    }
    angles[2] = 0;
    LoadActorMatrix(Sprite_Current, angles);
}

// original 0x4D3030: a LINE_G3 up the matrix's z axis - (0, 0, 0), (0, 0,
// 0x170), (0, 0, 0x180) - shaded +0xA * 6 at the foot and the middle, 1 at the
// tip; sorted at the sprite, tpage 0x35.
S25_EXPORT void __cdecl SpellSleep_DrawStem(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineG3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetWord(V(2), 0);
    SetWord(V(0), 0);
    SetWord(V(4), 0);
    SetWord(V(0xA), 0);
    SetWord(V(8), 0);
    SetWord(V(0xC), 0x170);
    SetWord(V(0x12), 0);
    SetWord(V(0x10), 0);
    SetWord(V(0x14), 0x180);
    Rtp3(p, 8, 0x18, 0x28);
    MH_CALL(Gte_PrimDepths3_10C)(p);
    const std::uint32_t shade = Sprite_Current[0xA] * 6u;
    SetLong(S(0xC), static_cast<std::int32_t>(shade));
    p[4] = static_cast<unsigned char>(shade);
    p[5] = S(0xC)[0];
    p[6] = S(0xC)[0];
    p[0x14] = S(0xC)[0];
    p[0x15] = S(0xC)[0];
    p[0x16] = S(0xC)[0];
    p[0x24] = 1;
    p[0x25] = 1;
    p[0x26] = 1;
    LinkAtSprite(2, 0x34);
}

// original 0x4D3160: a fan of flat triangles round the matrix's z axis - the
// apex (0, 0, 0x180), the rim radius 0x20 at height 0x1C0, 0x100 apart - of
// which the ten from the fifth on are drawn (a counter 0x81 down by 0x10 is
// drawn above 0x11 or below -0x2F); tpage 0x15, semi-transparent (mode
// ((+1 & 1) << 5)) when the phase is odd. Odd phases shade +0xA * 6, 1, 1;
// even ones 0xC0, the counter (or a second one from -0x5F), 0x10.
S25_EXPORT void __cdecl SpellSleep_DrawFan(void) {
    const unsigned semi = Sprite_Current[1] & 1u;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, ((semi & 3) << 5) | 0x15, 0);
    LinkAtSprite(2, 0xC);
    SetLong(S(0), 0x20);
    int s = MH_CALL(Math_Sin)(0);
    SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
    int c = MH_CALL(Math_Cos)(0);
    int ax = Sar12(Mul(c, SL(0)));
    SetWord(V(0x14), 0x1C0);
    SetWord(V(0x12), static_cast<unsigned>(ax));
    int angle = 0x100, low = -0x5F, counter = 0x81;
    do {
        const std::uint16_t cx = Word(V(0x10)), dx = Word(V(0x14));
        SetWord(V(0), 0);
        SetWord(V(2), 0);
        SetWord(V(4), 0x180);
        SetWord(V(8), cx);
        SetWord(V(0xA), static_cast<unsigned>(ax));
        SetWord(V(0xC), dx);
        s = MH_CALL(Math_Sin)(angle);
        SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
        c = MH_CALL(Math_Cos)(angle);
        ax = Sar12(Mul(c, SL(0)));
        SetWord(V(0x14), 0x1C0);
        SetWord(V(0x12), static_cast<unsigned>(ax));
        if (counter > 0x11 || counter < -0x2F) {
            unsigned char* const p = Gfx_PacketNext;
            MH_AT(PrimFn, kSetPolyF3.v)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, semi);
            Rtp3(p, 8, 0x14, 0x20);
            MH_CALL(Gte_PrimDepths3_0C)(p);
            const unsigned char* const sc = Sprite_Current;
            if (sc[1] & 1) {
                p[4] = static_cast<unsigned char>(sc[0xA] * 6);
                p[5] = 1;
                p[6] = 1;
            } else {
                SetLong(S(0xC), counter > 0x11 ? counter : low);
                p[4] = 0xC0;
                p[5] = S(0xC)[0];
                p[6] = 0x10;
            }
            LinkAtSprite(2, 0x2C);
            ax = static_cast<short>(Word(V(0x12)));
        }
        low += 0x10;
        counter -= 0x10;
        angle += 0x100;
    } while (counter > -0x7F);
}

// original 0x4D3350: a dome of flat quads radius 0x20 over height 0x1C0 - four
// bands (latitudes 0x800 down to 0x400 by 0x100) of sixteen, of which those
// with the counter 0xF1 - 0x10 i below 0xC1 are drawn; tpage and mode as the
// fan's; odd phases shade +0xA * 6, 1, 1; even ones 0xC0, the counter (or
// 0x11 + 0x10 i above 0x81), 1.
S25_EXPORT void __cdecl SpellSleep_DrawDome(void) {
    const unsigned semi = Sprite_Current[1] & 1u;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, ((semi & 3) << 5) | 0x15, 0);
    LinkAtSprite(2, 0xC);
    SetLong(S(0), 0x20);
    int top = 0x800;
    do {
        const int bottom = top - 0x100;
        DomePoint(8, bottom, 0);
        int s = MH_CALL(Math_Sin)(bottom);
        SetWord(V(0xC), static_cast<unsigned>(Sar12(Mul(s, SL(0))) + 0x1C0));
        DomePoint(0x18, top, 0);
        s = MH_CALL(Math_Sin)(top);
        SetWord(V(0x1C), static_cast<unsigned>(Sar12(Mul(s, SL(0))) + 0x1C0));
        int a = 0x100, high = 0x11, counter = 0xF1;
        do {
            {
                const std::uint16_t ax = Word(V(8)), cx = Word(V(0xA)), dx = Word(V(0xC));
                SetWord(V(0), ax);
                SetWord(V(2), cx);
                SetWord(V(4), dx);
            }
            DomePoint(8, bottom, a);
            s = MH_CALL(Math_Sin)(bottom);
            {
                const int z = Sar12(Mul(s, SL(0)));
                const std::uint16_t cx = Word(V(0x1A)), dx = Word(V(0x1C));
                SetWord(V(0xC), static_cast<unsigned>(z + 0x1C0));
                SetWord(V(0x10), Word(V(0x18)));
                SetWord(V(0x12), cx);
                SetWord(V(0x14), dx);
            }
            DomePoint(0x18, top, a);
            s = MH_CALL(Math_Sin)(top);
            SetWord(V(0x1C), static_cast<unsigned>(Sar12(Mul(s, SL(0))) + 0x1C0));
            if (counter < 0xC1) {
                unsigned char* const p = Gfx_PacketNext;
                MH_CALL(Gpu_SetPolyF4)(p);
                MH_CALL(Gpu_SetSemiTrans)(p, semi);
                Rtp4(p, 8, 0x14, 0x20, 0x2C);
                MH_CALL(Gte_PrimDepths4_0C)(p);
                const unsigned char* const sc = Sprite_Current;
                if (sc[1] & 1) {
                    p[4] = static_cast<unsigned char>(sc[0xA] * 6);
                    p[5] = 1;
                } else {
                    SetLong(S(0xC), counter > 0x81 ? high : counter);
                    p[4] = 0xC0;
                    p[5] = S(0xC)[0];
                }
                p[6] = 1;
                LinkAtSprite(2, 0x38);
            }
            counter -= 0x10;
            a += 0x100;
            high += 0x10;
        } while (counter > -0xF);
        top = bottom;
    } while (top > 0x400);
}

// original 0x4D36C0: pushes the matrix and loads the actor matrix turned 0x400
// about z when the facing +8 is odd.
S25_EXPORT void __cdecl SpellSleep_PushTurnMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    if (sc[8] & 1) angles[2] = 0x400;
    LoadActorMatrix(sc, angles);
}

// original 0x4D3770: a trail of gouraud quads behind the sprite's swing -
// from step +9 & 0x3F back (+0xA >> 1) + 1 steps, each a quad between the
// swing's points at radius 0x180 (height 0) and 0x1E0 (above), the shade
// ((+0xA >> 1) * 4 + 1) at the head fading by 4 a step; tpage 0x35.
S25_EXPORT void __cdecl SpellSleep_DrawTrail(void) {
    const unsigned char* sc = Sprite_Current;
    SetLong(S(0), 0x180);
    SetLong(S(4), 0x1E0);
    SetLong(S(0xC), static_cast<std::int32_t>((sc[0xA] >> 1) * 4u + 1));
    int step = sc[9] & 0x3F;
    const int length = (sc[0xA] >> 1) + 1;
    {
        const int t = TrailAngle(step);
        SetLong(S(8), t);
        SetWord(V(8), 0);
        int c = MH_CALL(Math_Cos)(t);
        SetWord(V(0xA), static_cast<unsigned>(Sar12(Mul(c, SL(0)))));
        int s = MH_CALL(Math_Sin)(SL(8));
        const int prod = Mul(s, SL(0));
        const std::int32_t a = SL(8);
        SetWord(V(0x18), 0);
        SetWord(V(0xC), static_cast<unsigned>(Sar12(prod)));
        c = MH_CALL(Math_Cos)(a);
        SetWord(V(0x1A), static_cast<unsigned>(Sar12(Mul(c, SL(4)))));
        s = MH_CALL(Math_Sin)(SL(8));
        SetWord(V(0x1C), static_cast<unsigned>(Sar12(Mul(s, SL(4)))));
    }
    sc = Sprite_Current;
    step = (step - 1) & 0x3F;
    if (step == ((sc[9] - length) & 0x3F)) return;
    do {
        const int s0 = MH_CALL(Math_Sin)(step << 6);
        {
            const std::uint16_t cx = Word(V(0xA)), dx = Word(V(0xC));
            const int t = Sar12(Shl(s0, 9)) + 0x400;
            SetWord(V(0), 0);
            SetLong(S(8), t);
            SetWord(V(2), cx);
            SetWord(V(4), dx);
            SetWord(V(8), 0);
            const int c = MH_CALL(Math_Cos)(t);
            SetWord(V(0xA), static_cast<unsigned>(Sar12(Mul(c, SL(0)))));
        }
        {
            const int s = MH_CALL(Math_Sin)(SL(8));
            const int prod = Mul(s, SL(0));
            const std::uint16_t cx = Word(V(0x1A)), dx = Word(V(0x1C));
            SetWord(V(0xC), static_cast<unsigned>(Sar12(prod)));
            const std::int32_t a = SL(8);
            SetWord(V(0x10), 0);
            SetWord(V(0x12), cx);
            SetWord(V(0x14), dx);
            SetWord(V(0x18), 0);
            const int c = MH_CALL(Math_Cos)(a);
            const int prod2 = Mul(c, SL(4));
            const std::int32_t a2 = SL(8);
            SetWord(V(0x1A), static_cast<unsigned>(Sar12(prod2)));
            const int s2 = MH_CALL(Math_Sin)(a2);
            const int prod3 = Mul(s2, SL(4));
            unsigned char* const p0 = Gfx_PacketNext;
            SetWord(V(0x1C), static_cast<unsigned>(Sar12(prod3)));
            MH_CALL(Gpu_SetDrawMode)(p0, 0, 1, 0x35, 0);
        }
        LinkAtSprite(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[5] = 1;
        p[4] = S(0xC)[0];
        p[6] = 1;
        p[0x15] = 1;
        p[0x14] = S(0xC)[0];
        p[0x16] = 1;
        const std::int32_t faded = SL(0xC) - 4;
        SetLong(S(0xC), faded);
        p[0x24] = static_cast<unsigned char>(faded);
        p[0x25] = 1;
        p[0x26] = 1;
        p[0x35] = 1;
        p[0x34] = S(0xC)[0];
        p[0x36] = 1;
        LinkAtSprite(2, 0x44);
        sc = Sprite_Current;
        step = (step - 1) & 0x3F;
    } while (step != ((sc[9] - length) & 0x3F));
}

// original 0x4D3A40: the same trail as lines - a LINE_G4 a step, from (0, cos,
// sin) * the radius in 0x903850 through (0, cos, sin) / 256, (0, cos, sin) *
// 21 / 16 and (0, cos, sin) * 3 / 32 of the step's angle; (+0xA >> 2) + 1
// steps, the shade ((+0xA >> 2) * 12 + 1) fading by 12 a step, the ends 1;
// tpage 0x35.
S25_EXPORT void __cdecl SpellSleep_DrawTrailLines(void) {
    const unsigned char* sc = Sprite_Current;
    const int length = (sc[0xA] >> 2) + 1;
    SetLong(S(0xC), static_cast<std::int32_t>((sc[0xA] >> 2) * 12u + 1));
    int step = sc[9] & 0x3F;
    if (step == ((sc[9] - length) & 0x3F)) return;
    do {
        {
            const int t = TrailAngle(step);
            SetWord(V(0), 0);
            SetLong(S(8), t);
            int c = MH_CALL(Math_Cos)(t);
            int prod = Mul(c, SL(0));
            std::int32_t a = SL(8);
            SetWord(V(2), static_cast<unsigned>(Sar12(prod)));
            int s = MH_CALL(Math_Sin)(a);
            SetWord(V(4), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
            a = SL(8);
            SetWord(V(8), 0);
            c = MH_CALL(Math_Cos)(a);
            a = SL(8);
            SetWord(V(0xA), static_cast<unsigned>(Sar12(Shl(c, 4))));
            s = MH_CALL(Math_Sin)(a);
            a = SL(8);
            SetWord(V(0x10), 0);
            SetWord(V(0xC), static_cast<unsigned>(Sar12(Shl(s, 4))));
            c = MH_CALL(Math_Cos)(a);
            prod = Mul(c, 21);
            a = SL(8);
            SetWord(V(0x12), static_cast<unsigned>(Sar12(Shl(prod, 4))));
            s = MH_CALL(Math_Sin)(a);
            prod = Mul(s, 21);
            SetWord(V(0x18), 0);
            a = SL(8);
            SetWord(V(0x14), static_cast<unsigned>(Sar12(Shl(prod, 4))));
            c = MH_CALL(Math_Cos)(a);
            a = SL(8);
            SetWord(V(0x1A), static_cast<unsigned>(Sar12(Shl(Mul(c, 3), 7))));
            s = MH_CALL(Math_Sin)(a);
            unsigned char* const p0 = Gfx_PacketNext;
            SetWord(V(0x1C), static_cast<unsigned>(Sar12(Shl(Mul(s, 3), 7))));
            MH_CALL(Gpu_SetDrawMode)(p0, 0, 1, 0x35, 0);
        }
        LinkAtSprite(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_AT(PrimFn, kSetLineG4.v)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10C)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = S(0xC)[0];
        p[0x15] = S(0xC)[0];
        p[0x16] = S(0xC)[0];
        p[0x24] = S(0xC)[0];
        p[0x25] = S(0xC)[0];
        p[0x26] = S(0xC)[0];
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        const std::int32_t faded = SL(0xC) - 0xC;
        SetLong(S(0xC), faded);
        LinkAtSprite(2, 0x44);
        sc = Sprite_Current;
        step = (step - 1) & 0x3F;
    } while (step != ((sc[9] - length) & 0x3F));
}

// original 0x4D3CB0: a fan of sixteen gouraud triangles on the ground under
// the swing - the centre (0, r cos, 0x200) with r = 0x180 cos of the swing's
// angle, the rim 0x30 round it - shaded +0xA * 3 at the centre and 1 at the
// rim; tpage 0x55 around it, 0x15 after, through the overlay order table
// (Gfx_CommitPrim slot 5).
S25_EXPORT void __cdecl SpellSleep_DrawShadowFan(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetLong(S(0), 0x180);
    const int t = TrailAngle(Sprite_Current[9] & 0x3F);
    SetLong(S(8), t);
    int c = MH_CALL(Math_Cos)(t);
    const int centre = Sar12(Mul(c, SL(0)));
    SetLong(S(0), 0x30);
    int s = MH_CALL(Math_Sin)(0);
    SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
    c = MH_CALL(Math_Cos)(0);
    int ax = Sar12(Mul(c, SL(0))) + centre;
    SetWord(V(0x12), static_cast<unsigned>(ax));
    SetWord(V(0x14), 0x200);
    int a = 0x100;
    for (;;) {
        const std::uint16_t cx = Word(V(0x10));
        SetWord(V(0), 0);
        SetWord(V(2), static_cast<unsigned>(centre));
        SetWord(V(4), 0x200);
        SetWord(V(8), cx);
        SetWord(V(0xA), static_cast<unsigned>(ax));
        SetWord(V(0xC), 0x200);
        s = MH_CALL(Math_Sin)(a);
        SetWord(V(0x10), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
        c = MH_CALL(Math_Cos)(a);
        const int y = Sar12(Mul(c, SL(0)));
        SetWord(V(0x14), 0x200);
        unsigned char* const p = Gfx_PacketNext;
        SetWord(V(0x12), static_cast<unsigned>(y + centre));
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp3(p, 8, 0x18, 0x28);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const std::uint32_t shade = Sprite_Current[0xA] * 3u;
        SetLong(S(0xC), static_cast<std::int32_t>(shade));
        p[4] = static_cast<unsigned char>(shade);
        p[5] = S(0xC)[0];
        p[6] = S(0xC)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
        a += 0x100;
        if (a >= 0x1100) break;
        ax = static_cast<short>(Word(V(0x12)));
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void SpellSleep_Inject() {
    BOF3_INJECT(SpellSleep_Task);
    BOF3_INJECT(SpellSleep_Start);
    BOF3_INJECT(SpellSleep_Child);
    BOF3_INJECT(SpellSleep_ChildInit);
    BOF3_INJECT(SpellSleep_ChildGrow);
    BOF3_INJECT(SpellSleep_ChildHold);
    BOF3_INJECT(SpellSleep_ChildFade);
    BOF3_INJECT(SpellSleep_PushSwayMatrix);
    BOF3_INJECT(SpellSleep_DrawStem);
    BOF3_INJECT(SpellSleep_DrawFan);
    BOF3_INJECT(SpellSleep_DrawDome);
    BOF3_INJECT(SpellSleep_PushTurnMatrix);
    BOF3_INJECT(SpellSleep_DrawTrail);
    BOF3_INJECT(SpellSleep_DrawTrailLines);
    BOF3_INJECT(SpellSleep_DrawShadowFan);
}
