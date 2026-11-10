// Moved here by tools/move_funcs.py from magic_s25.cpp (docs/source-layout-round.md).
#include "game/magic/spell_confuse.h"

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

using magic_harness::EnemyOf;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::PartyOf;

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

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);

using RtpFn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using RotTransFn = void (__cdecl*)(const short*, long*, long*);

// The effect library (group L), by name: MagicFx_CenterOnSide (the effect's
// sprite to the middle of the target side), MagicFx_StepTowardPoint (one step
// towards a point), MagicFx_NearPoint3D ("is the sprite within a box of the
// point"). The engine's 0x446770 turns +0xC / +0x10 by the facing +8.
constexpr ::bof3::image::CodeVa kTurnByFacing{bof3::addr::Battle_TurnVectorC};

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

// A quad of four points (0, cos a, sin a) * the radius in 0x903850 at the
// angles given, as the original stores them (x 0 before each Cos).
void RadiusQuad(int a0, int a1, int a2, int a3) {
    const int angles[4] = {a0, a1, a2, a3};
    for (unsigned k = 0; k < 4; ++k) {
        SetWord(V(8 * k), 0);
        const int c = MH_CALL(Math_Cos)(angles[k]);
        SetWord(V(8 * k + 2), static_cast<unsigned>(Sar12(Mul(c, SL(0)))));
        const int s = MH_CALL(Math_Sin)(angles[k]);
        SetWord(V(8 * k + 4), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
    }
}

// The two quads' shading: a (0x903854), 1 or a by corner.
void QuadShade(unsigned char* p) {
    p[5] = 1;
    p[6] = S(4)[0];
    p[0x14] = S(4)[0];
    p[0x15] = S(4)[0];
    p[0x16] = 1;
    p[0x25] = 1;
    p[0x24] = S(4)[0];
    p[0x26] = S(4)[0];
    p[0x34] = S(4)[0];
    p[0x35] = 1;
    p[0x36] = 1;
}

// original 0x4D3EA0 (Magic_Rows row 25): phase +1 through a two-entry stack
// table - SpellConfuse_Start, then MAGIC078's 0x4BDC10.
S25_EXPORT void __cdecl SpellConfuse_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::SpellConfuse_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("SpellConfuse_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D3ED0: a kind-1 child 0x1A (SpellConfuse_Child) owned by this
// task, with the facing +8 and position of the source sprite 0x904B4C (read
// before the child is made); +0xB 0; on.
S25_EXPORT void __cdecl SpellConfuse_Start(void) {
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x1A);
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = Task(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    SetPointer(child + 0x80, sc);
    child[8] = source[8];
    tchild->x = rv::As<records::SpriteHead>(source)->x;
    tchild->z = rv::As<records::SpriteHead>(source)->z;
    rv::Store(tchild->height, rv::As<records::SpriteHead>(source)->height);
    sc[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D3F30 (kind-1 task 0x1A): phase +1 through SpellConfuse_ChildPhases
// (0x65B928, six entries); then, while the slot is live, the quads under the
// spin matrix and the rays under the facing matrix.
S25_EXPORT void __cdecl SpellConfuse_Child(void) {
    TableEntry("SpellConfuse_Child", ::bof3::Img(bof3::addr::SpellConfuse_ChildPhases), 6, Sprite_Current[1])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(SpellConfuse_PushSpinMatrix)();
    MH_CALL(SpellConfuse_DrawQuads)();
    MH_CALL(Gte_PopMatrix)();
    MH_CALL(SpellConfuse_PushFacingMatrix)();
    MH_CALL(SpellConfuse_DrawRays)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D3F70 (child phase 0): a step (0x20000, 0) turned by the facing
// (0x446770) and taken once; the height +0xC0; that point kept as the start
// (+0x18 / +0x1C / +0x20); the sprite put on the acting actor 0x904B34 (party
// record 0..2, else enemy - 3) raised 0xC00000; +0xB, +9, +0xA 0; on; sound
// 0x100.
S25_EXPORT void __cdecl SpellConfuse_ChildInit(void) {
    rv::Current<records::BattleTask>()->step_x = 0x20000;
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_AT(void (__cdecl*)(unsigned char*), kTurnByFacing.v)(Sprite_Current);
    unsigned char* sc = Sprite_Current;
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    tsc->x = tsc->x + tsc->step_x;
    tsc->z = tsc->z + tsc->step_z;
    rv::Store<unsigned>(tsc->height_whole, tsc->height_whole + 0xC0u);
    tsc->work_18 = tsc->x;
    tsc->work_1c = tsc->z;
    rv::Store(tsc->work_20, tsc->height);
    const unsigned char actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const record = actor <= 2 ? PartyOf(actor) : EnemyOf(actor);
    tsc->x = Long(record + 0x34);
    tsc->z = Long(record + 0x38);
    rv::Store(tsc->height, Long(record + 0x3C) + 0xC00000);
    sc[0xB] = 0;
    sc[9] = 0;
    sc[0xA] = 0;
    Bump(sc[1]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4D4080 (child phase 1): +0xA up; a step towards the start point
// ((x >> 9) - 0x4000, (z >> 9) - 0x4000, height >> 17) at speed 0x20
// (0x4FBA90; the original passes that VECTOR by value with its pad word
// uninitialised, ours 0 - the callee reads three); on once the sprite is
// within 0x8000 of the start (0x4FBC70, its answer tested whole).
S25_EXPORT void __cdecl SpellConfuse_ChildFly(void) {
    Bump(Sprite_Current[0xA]);
    const unsigned char* sc = Sprite_Current;
    MH_AT(void (__cdecl*)(int, int, int, int, int), bof3::addr::MagicFx_StepTowardPoint)(
        (rv::As<records::BattleTask>(sc)->work_18 >> 9) - 0x4000, (rv::As<records::BattleTask>(sc)->work_1c >> 9) - 0x4000, rv::As<records::BattleTask>(sc)->work_20 >> 17, 0, 0x20);
    sc = Sprite_Current;
    if (MH_AT(int (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t),
              bof3::addr::MagicFx_NearPoint3D)(static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_18),
                                               static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_1c),
                                               static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_20), 0x8000) != 0)
        Bump(Sprite_Current[1]);
}

// original 0x4D4100 (child phase 2): +0xA up; each 0x20, sound 0x101, +0xA 0,
// on.
S25_EXPORT void __cdecl SpellConfuse_ChildChime(void) {
    Bump(Sprite_Current[0xA]);
    if (Sprite_Current[0xA] & 0x1F) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D4140 (child phase 3): +9 up by 8; on at 0x80.
S25_EXPORT void __cdecl SpellConfuse_ChildGrow(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 8);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x80) Bump(sc[1]);
}

// original 0x4D4160 (child phase 4): +0xA up; on at 0x60.
S25_EXPORT void __cdecl SpellConfuse_ChildHold(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] == 0x60) Bump(sc[1]);
}

// original 0x4D4180 (child phase 5): +0xA up; at 0x80 the owner's +0xB 0xFF
// and the slot freed.
S25_EXPORT void __cdecl SpellConfuse_ChildEnd(void) {
    Bump(Sprite_Current[0xA]);
    if (Sprite_Current[0xA] != 0x80) return;
    Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D41B0: pushes the matrix and loads the actor matrix turned by
// the facing +8 (a jump table of four): with n = (~+0xA & 0x1F) << 7, the
// angles (n, 0, 0x800), (0, n, 0xC00), (n, 0, 0), (0, n, 0x400). A facing past
// 3 would take the angles from uninitialised stack; ours aborts.
S25_EXPORT void __cdecl SpellConfuse_PushFacingMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const sc = Sprite_Current;
    const auto n = static_cast<short>(((~sc[0xA]) & 0x1F) << 7);
    short angles[3];
    switch (sc[8]) {
    case 0: angles[0] = n; angles[1] = 0; angles[2] = 0x800; break;
    case 1: angles[0] = 0; angles[1] = n; angles[2] = 0xC00; break;
    case 2: angles[0] = n; angles[1] = 0; angles[2] = 0; break;
    case 3: angles[0] = 0; angles[1] = n; angles[2] = 0x400; break;
    default: bof3::Fatal("SpellConfuse_PushFacingMatrix: facing %u, past the four-entry jump table", sc[8]);
    }
    LoadActorMatrix(sc, angles);
}

// original 0x4D4300: +9 rays (LINE_F2), the counter i (from 0) kept in
// 0x903850: ray i from the point the last one ended at - (+0xA, 0, 0) for the
// first - to (+0xA - (i + 1) or 0, cos, sin) * i of the angle 0x80 (i + 1);
// tpage 0x15, semi-transparent when the phase is 5; shaded (0x80 - +0xA) * 2,
// * 3, * 2 in phase 5, else (0x80, 0xC0, 0x80); sorted at the sprite, dy 0.
S25_EXPORT void __cdecl SpellConfuse_DrawRays(void) {
    const unsigned char* sc = Sprite_Current;
    const unsigned char phase = sc[1];
    SetLong(S(0), 0);
    SetWord(V(0), sc[0xA]);
    const unsigned semi = phase == 5 ? 1u : 0u;
    const int c0 = MH_CALL(Math_Cos)(0);
    SetWord(V(2), static_cast<unsigned>(Sar12(Mul(c0, SL(0)))));
    const int s0 = MH_CALL(Math_Sin)(0);
    std::int32_t count = SL(0);
    SetWord(V(4), static_cast<unsigned>(Sar12(Mul(s0, count))));
    if (count >= static_cast<int>(Sprite_Current[9])) return;
    const unsigned tpage = ((semi & 3) << 5) | 0x15;
    int from = 1, angle = 0x80;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
        LinkAtSprite(0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, semi);
        long depth, flag;
        S25_AS(RtpFn, Gte_RotTransPers)(VP(0), p + 8, &depth, &flag);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        const unsigned char a = Sprite_Current[0xA];
        SetWord(V(0), static_cast<int>(a) > from ? static_cast<unsigned>(a - from) : 0u);
        const int c = MH_CALL(Math_Cos)(angle);
        SetWord(V(2), static_cast<unsigned>(Sar12(Mul(c, SL(0)))));
        const int s = MH_CALL(Math_Sin)(angle);
        SetWord(V(4), static_cast<unsigned>(Sar12(Mul(s, SL(0)))));
        S25_AS(RtpFn, Gte_RotTransPers)(VP(0), p + 0x14, &depth, &flag);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        sc = Sprite_Current;
        if (sc[1] == 5) {
            SetLong(S(4), static_cast<std::int32_t>((0x80 - sc[0xA]) * 3));
            const auto swap = static_cast<std::int32_t>((0x80 - sc[0xA]) * 2);
            SetLong(S(0xC), swap);
            p[4] = static_cast<unsigned char>(swap);
            p[5] = S(4)[0];
            p[6] = S(0xC)[0];
        } else {
            p[4] = 0x80;
            p[5] = 0xC0;
            p[6] = 0x80;
        }
        LinkAtSprite(0, 0x20);
        count = SL(0) + 1;
        ++from;
        angle += 0x80;
        SetLong(S(0), count);
    } while (count < static_cast<int>(Sprite_Current[9]));
}

// original 0x4D4510: pushes the matrix and loads the actor matrix spun by n =
// (+0xA & 0x1F) << 7: (0, n, 0x400) when the facing +8 is odd, (n, 0, 0) when
// even.
S25_EXPORT void __cdecl SpellConfuse_PushSpinMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const sc = Sprite_Current;
    const auto n = static_cast<short>((sc[0xA] & 0x1F) << 7);
    short angles[3];
    if (sc[8] & 1) {
        angles[0] = 0;
        angles[1] = n;
        angles[2] = 0x400;
    } else {
        angles[0] = n;
        angles[1] = 0;
        angles[2] = 0;
    }
    LoadActorMatrix(sc, angles);
}

// original 0x4D45F0: two gouraud quads across the matrix's origin (corners at
// 0x200 / 0x600 / 0xE00 / 0xA00 and 0 / 0x400 / 0xC00 / 0x800) of radius +0xA
// * 8 up to 0xC0 in phase 1, else 0xC0; shaded (0x80 - +0xA) * 3 in phase 5,
// else 0x60; tpage 0x35, sorted at the sprite.
S25_EXPORT void __cdecl SpellConfuse_DrawQuads(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0, 0xC);
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* sc = Sprite_Current;
    if (sc[1] == 1) {
        const int r = sc[0xA] * 8;
        SetLong(S(0), r);
        if (r > 0xC0) SetLong(S(0), 0xC0);
    } else {
        SetLong(S(0), 0xC0);
    }
    RadiusQuad(0x200, 0x600, 0xE00, 0xA00);
    Rtp4(p, 8, 0x18, 0x28, 0x38);
    MH_CALL(Gte_PrimDepths4_10B)(p);
    sc = Sprite_Current;
    const std::int32_t shade = sc[1] == 5 ? static_cast<std::int32_t>((0x80 - sc[0xA]) * 3) : 0x60;
    SetLong(S(4), shade);
    p[4] = static_cast<unsigned char>(shade);
    QuadShade(p);
    LinkAtSprite(0, 0x44);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0, 0xC);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    RadiusQuad(0, 0x400, 0xC00, 0x800);
    Rtp4(p, 8, 0x18, 0x28, 0x38);
    MH_CALL(Gte_PrimDepths4_10B)(p);
    p[4] = S(4)[0];
    QuadShade(p);
    LinkAtSprite(0, 0x44);
}

void SpellConfuse_Inject() {
    BOF3_INJECT(SpellConfuse_Task);
    BOF3_INJECT(SpellConfuse_Start);
    BOF3_INJECT(SpellConfuse_Child);
    BOF3_INJECT(SpellConfuse_ChildInit);
    BOF3_INJECT(SpellConfuse_ChildFly);
    BOF3_INJECT(SpellConfuse_ChildChime);
    BOF3_INJECT(SpellConfuse_ChildGrow);
    BOF3_INJECT(SpellConfuse_ChildHold);
    BOF3_INJECT(SpellConfuse_ChildEnd);
    BOF3_INJECT(SpellConfuse_PushFacingMatrix);
    BOF3_INJECT(SpellConfuse_DrawRays);
    BOF3_INJECT(SpellConfuse_PushSpinMatrix);
    BOF3_INJECT(SpellConfuse_DrawQuads);
}
