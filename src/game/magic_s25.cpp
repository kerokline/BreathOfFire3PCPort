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
// copies. Calls into other groups' units (the effect library's 0x4FC0E0,
// 0x4FBA90, 0x4FBC70; MAGIC078's 0x4BDC10; MAGIC130's 0x4E47F0, MAGIC082's
// 0x4C0680 and MAGIC060's 0x4B1740 through the .data tables) and into the
// unnamed engine helper 0x446770 and libgpu setters 0x5A7570 / 0x5A76F0 go by
// raw address.
//
// No divergence: each is a faithful replacement, except that a dispatch past
// its table aborts where the original would call or jump through whatever
// follows the table (docs/magic_fx_reached.md section 3, the precedent):
// the four task stack tables, the six .data phase tables and the jump table
// of SpellConfuse_PushFacingMatrix (whose default case would build the
// matrix from uninitialised stack).
#include "game/magic_s25.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
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
constexpr std::uint32_t kVertex = 0x9037A0;
constexpr std::uint32_t kScratch = 0x903850;
unsigned char* V(unsigned k) { return Mem(kVertex + k); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(V(k)); }
unsigned char* S(unsigned k) { return Mem(kScratch + k); }
std::int32_t SL(unsigned k) { return Long(S(k)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// imul (a 32-bit product that wraps), then sar 0xC.
int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
int Sar12(int v) { return v >> 12; }
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
unsigned char* Task(unsigned slot) { return Mem(at::kTasks + (slot & 0xFF) * at::kTaskStride); }
void SetPointer(unsigned char* cell, const void* p) {
    SetLong(cell, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
// fild dword, fst dword: an int as the single the original stores.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
#define S25_AS(type, name) ::magic_harness::Call(As<type>(&::name))

// The GTE entry points as the overlays push them (one pointer more than
// symbols.gen.h's prototypes: the flag word).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);
using RtpFn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using RotTransFn = void (__cdecl*)(const short*, long*, long*);
using PrimFn = void (__cdecl*)(unsigned char*);

// libgpu's SetPolyF3 (code 0x20) and SetLineG4 (code 0x5C), unnamed, in no
// group (psx_gpu's neighbours; docs/magic_s25.md section 7).
constexpr std::uint32_t kSetPolyF3 = 0x5A7570;
constexpr std::uint32_t kSetLineG4 = 0x5A76F0;
// The effect library (group L): the effect's sprite to the middle of the
// target side; one step of the sprite towards a point; "is the sprite within
// a box of the point". The engine's 0x446770 turns +0xC / +0x10 by the
// facing +8.
constexpr std::uint32_t kCentreOnTargets = 0x4FC0E0;
constexpr std::uint32_t kStepTowards = 0x4FBA90;
constexpr std::uint32_t kWithin = 0x4FBC70;
constexpr std::uint32_t kTurnByFacing = 0x446770;
// Other units' phases in the stack tables: MAGIC078's "free once the owner's
// +0xB is 0xFF" and the engine's "the done flag, free".
constexpr std::uint32_t kEndWhenChildDone = 0x4BDC10;
constexpr std::uint32_t kDoneAndFree = 0x43FE80;

// The battle bytes these read beyond the harness's names.
constexpr std::uint32_t kCasterRecord = 0x904B3C;   // a record pointer; its +8 is the facing
constexpr std::uint32_t kSideCorner = 0x904AAC;     // low byte: a corner of SpellDepress_Corners

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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
                                dy, size);
}

// The actor matrix every overlay of this group builds on its stack, as
// MagicFx_PushActorMatrix does with angles of its own: translation
// RotTrans((x >> 9) - 0x4000, (z >> 9) - 0x4000, -(height / 2)) of the
// sprite, rotation RotMatrix(angles), times Camera_Matrix, loaded. The
// caller has pushed the matrix; it pops nothing.
void LoadActorMatrix(const unsigned char* sc, const short* angles) {
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
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

// A child's ping-pong of its +0x64 between -0x80 and 0x80 in steps of 8, +0xB
// the direction (Depress).
void Swing() {
    unsigned char* sc = Sprite_Current;
    const std::int32_t w = Long(sc + 0x64);
    if (sc[0xB] == 0) {
        SetLong(sc + 0x64, w + 8);
        sc = Sprite_Current;
        if (Long(sc + 0x64) >= 0x80) sc[0xB] = 1;
    } else {
        SetLong(sc + 0x64, w - 8);
        sc = Sprite_Current;
        if (Long(sc + 0x64) <= -0x80) sc[0xB] = 0;
    }
}

}  // namespace

#define S25_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC107 (row 24): "Sleep"

// original 0x4D2D80 (Magic_Rows row 24): phase +1 through a two-entry stack
// table - SpellSleep_Start, then MAGIC078's 0x4BDC10 (free once the child has
// set the owner's +0xB to 0xFF).
S25_EXPORT void __cdecl SpellSleep_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::SpellSleep_Start, kEndWhenChildDone};
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
    MH_AT(Handler, kCentreOnTargets)();
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x19);
    const unsigned char* const caster = Pointer(kCasterRecord);
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = Task(slot);
    SetPointer(child + 0x80, sc);
    child[8] = caster[8];
    SetLong(child + 0x34, Field_Kind2X);
    SetLong(child + 0x38, Field_Kind2Z);
    SetLong(child + 0x3C, Long(sc + 0x3C) + 0x4000000);
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D2E30 (kind-1 task 0x19): phase +1 through SpellSleep_ChildPhases
// (0x65B918, four entries); then, while the slot is live, the draws under two
// matrices - the sway matrix: stem, dome, fan; the turn matrix: shadow fan,
// trail, trail lines.
S25_EXPORT void __cdecl SpellSleep_Child(void) {
    TableEntry("SpellSleep_Child", bof3::addr::SpellSleep_ChildPhases, 4, Sprite_Current[1])();
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
    Pointer(at::kOwner)[0xB] = 0xFF;
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
            MH_AT(PrimFn, kSetPolyF3)(p);
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

// The trail's point at step `step` (0..63): the angle ((sin(step << 6) << 9)
// >> 12) + 0x400 into 0x903858; returns it.
int TrailAngle(int step) {
    const int s = MH_CALL(Math_Sin)(step << 6);
    return Sar12(Shl(s, 9)) + 0x400;
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
        MH_AT(PrimFn, kSetLineG4)(p);
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

// ===========================================================================
// MAGIC108 (row 25): "Confuse"

// original 0x4D3EA0 (Magic_Rows row 25): phase +1 through a two-entry stack
// table - SpellConfuse_Start, then MAGIC078's 0x4BDC10.
S25_EXPORT void __cdecl SpellConfuse_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::SpellConfuse_Start, kEndWhenChildDone};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("SpellConfuse_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D3ED0: a kind-1 child 0x1A (SpellConfuse_Child) owned by this
// task, with the facing +8 and position of the source sprite 0x904B4C (read
// before the child is made); +0xB 0; on.
S25_EXPORT void __cdecl SpellConfuse_Start(void) {
    const unsigned char* const source = Pointer(at::kSource);
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x1A);
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = Task(slot);
    SetPointer(child + 0x80, sc);
    child[8] = source[8];
    SetLong(child + 0x34, Long(source + 0x34));
    SetLong(child + 0x38, Long(source + 0x38));
    SetLong(child + 0x3C, Long(source + 0x3C));
    sc[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4D3F30 (kind-1 task 0x1A): phase +1 through SpellConfuse_ChildPhases
// (0x65B928, six entries); then, while the slot is live, the quads under the
// spin matrix and the rays under the facing matrix.
S25_EXPORT void __cdecl SpellConfuse_Child(void) {
    TableEntry("SpellConfuse_Child", bof3::addr::SpellConfuse_ChildPhases, 6, Sprite_Current[1])();
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
    SetLong(Sprite_Current + 0xC, 0x20000);
    SetLong(Sprite_Current + 0x10, 0);
    MH_AT(void (__cdecl*)(unsigned char*), kTurnByFacing)(Sprite_Current);
    unsigned char* sc = Sprite_Current;
    SetLong(sc + 0x34, Long(sc + 0x34) + Long(sc + 0xC));
    SetLong(sc + 0x38, Long(sc + 0x38) + Long(sc + 0x10));
    SetWord(sc + 0x3E, Word(sc + 0x3E) + 0xC0u);
    SetLong(sc + 0x18, Long(sc + 0x34));
    SetLong(sc + 0x1C, Long(sc + 0x38));
    SetLong(sc + 0x20, Long(sc + 0x3C));
    const unsigned char actor = Mem(at::kActor)[0];
    const unsigned char* const record = actor <= 2 ? PartyOf(actor) : EnemyOf(actor);
    SetLong(sc + 0x34, Long(record + 0x34));
    SetLong(sc + 0x38, Long(record + 0x38));
    SetLong(sc + 0x3C, Long(record + 0x3C) + 0xC00000);
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
    MH_AT(void (__cdecl*)(int, int, int, int, int), kStepTowards)((Long(sc + 0x18) >> 9) - 0x4000,
                                                                  (Long(sc + 0x1C) >> 9) - 0x4000, Long(sc + 0x20) >> 17, 0,
                                                                  0x20);
    sc = Sprite_Current;
    if (MH_AT(int (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t), kWithin)(
            static_cast<std::uint32_t>(Long(sc + 0x18)), static_cast<std::uint32_t>(Long(sc + 0x1C)),
            static_cast<std::uint32_t>(Long(sc + 0x20)), 0x8000) != 0)
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
    Pointer(at::kOwner)[0xB] = 0xFF;
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

// ===========================================================================
// MAGIC109 (row 12): "Depress"

// original 0x4D49E0 (Magic_Rows row 12): phase +1 through a four-entry stack
// table - start, open, hold, close.
S25_EXPORT void __cdecl SpellDepress_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::SpellDepress_Start, bof3::addr::SpellDepress_Open,
                                                 bof3::addr::SpellDepress_Hold, bof3::addr::SpellDepress_Close};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("SpellDepress_Task: phase %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D4A20: cast on the caster's own side (the target byte 0x80 - the
// party - by a party member, or 0x40 - the enemies - by an enemy), the target
// flagged 0x40, the done flag and the slot freed at once. Otherwise sound
// 0x100; +9 2; the sprite's screen box (x - 2, y + 240 * Gfx_BufferIndex - 2,
// 4 x 4) moved into VRAM at (0x340, 0x100) (DR_MOVE, slot 3); the sprite's
// screen point from SpellDepress_Corners by the side corner 0x904AAC (xor 2
// unless the target is the enemy side); +0x64 0; on.
S25_EXPORT void __cdecl SpellDepress_Start(void) {
    if (Mem(at::kTarget)[0] == 0x80) {
        if (Mem(at::kActor)[0] < 3) Sprite_Current[1] = 1;
    }
    if (Mem(at::kTarget)[0] == 0x40 && Mem(at::kActor)[0] > 2) Sprite_Current[1] = 1;
    // (the original's first test skips the second when the caster is an enemy;
    // the target byte cannot be both, so the two are the same)
    if (Sprite_Current[1] != 0) {
        MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
        Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(Sound_PlayEffect)(0x100);
    Sprite_Current[9] = 2;
    const unsigned char* sc = Sprite_Current;
    short rect[4];
    rect[0] = static_cast<short>(Word(sc + 0x2E) - sc[9]);
    rect[1] = static_cast<short>(Gfx_BufferIndex * 0xF0 + Word(sc + 0x30) - sc[9]);
    rect[2] = static_cast<short>(sc[9] * 2);
    rect[3] = static_cast<short>(sc[9] * 2);
    MH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), 0x340, 0x100);
    MH_CALL(Gfx_CommitPrim)(3, 0x18);
    const std::uint32_t corners = bof3::addr::SpellDepress_Corners;
    unsigned corner;
    if (Mem(at::kTarget)[0] & 0x40) {
        SetWord(Sprite_Current + 0x2E, Word(Mem(corners + (Mem(kSideCorner)[0]) * 8)));
        corner = Mem(kSideCorner)[0];
    } else {
        SetWord(Sprite_Current + 0x2E, Word(Mem(corners + (Mem(kSideCorner)[0] ^ 2u) * 8)));
        corner = Mem(kSideCorner)[0] ^ 2u;
    }
    SetWord(Sprite_Current + 0x30, Word(Mem(corners + 4 + corner * 8)));
    SetLong(Sprite_Current + 0x64, 0);
    Bump(Sprite_Current[1]);
}

// original 0x4D4BA0 (phase 1): +9 up by 2; at 0x54 on, +0xA 0x40; the vortex
// drawn with the twist +0x64.
S25_EXPORT void __cdecl SpellDepress_Open(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 2);
    unsigned char* sc = Sprite_Current;
    if (sc[9] == 0x54) {
        Bump(sc[1]);
        Sprite_Current[0xA] = 0x40;
        sc = Sprite_Current;
    }
    MH_CALL(SpellDepress_DrawVortex)(Long(sc + 0x64));
}

// original 0x4D4BE0 (phase 2): +0xA down; when it was 0 the target flagged
// 0x10 (Battle_SetTargetFlags) and on; the vortex drawn; the twist swung.
S25_EXPORT void __cdecl SpellDepress_Hold(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char was = sc[0xA];
    sc[0xA] = static_cast<unsigned char>(was - 1);
    if (was == 0) {
        MH_CALL(Battle_SetTargetFlags)(Mem(at::kTarget)[0], 0x10);
        Bump(Sprite_Current[1]);
    }
    MH_CALL(SpellDepress_DrawVortex)(Long(Sprite_Current + 0x64));
    Swing();
}

// original 0x4D4C60 (phase 3): +9 down by 2; at 2 the target flagged 0x40,
// the done flag and the slot freed; else the vortex drawn and the twist
// swung.
S25_EXPORT void __cdecl SpellDepress_Close(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 2);
    const unsigned char* const sc = Sprite_Current;
    if (sc[9] == 2) {
        MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
        Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(SpellDepress_DrawVortex)(Long(sc + 0x64));
    Swing();
}

namespace {
// A vertex of the vortex: the sprite's screen point plus a ring entry, as a
// float into the GT4 and the F4 alike (the GT4's first).
void VortexVertex(unsigned char* g, unsigned go, unsigned char* f, unsigned fo, const unsigned char* entry) {
    const int x = S16(Sprite_Current + 0x2E) + Long(entry);
    PutFloat(g + go, x);
    PutFloat(f + fo, x);
    const int y = S16(Sprite_Current + 0x30) + Long(entry + 4);
    PutFloat(g + go + 4, y);
    PutFloat(f + fo + 4, y);
}
// A texture pair from the rings' low bytes plus +9.
void VortexUV(unsigned char* g, unsigned o, std::uint32_t entry) {
    g[o] = static_cast<unsigned char>(Mem(entry)[0] + Sprite_Current[9]);
    g[o + 1] = static_cast<unsigned char>(Mem(entry + 4)[0] + Sprite_Current[9]);
}
}  // namespace

// original 0x4D4CE0 (step, the twist): the target's screen box, grabbed into
// VRAM by SpellDepress_Start, drawn back as a vortex. SpellDepress_Rings
// (0x6989C0): nine rings of sixteen points, ring r of radius (+9 >> 3) * r;
// the box redrawn (DR_MOVE again, then tpage 0x11D, slot 3); the outer ring
// (SpellDepress_Outer, 0x698940) at radius +9; then eight bands inwards, each
// ring (SpellDepress_Inner, 0x698E40) turned by a twist that grows by `step`
// a band, sixteen quads a band - a black F4 under a GT4 of page 0x11D whose
// texture is the rings' own offsets from the box's centre (+9), shaded 0x70
// - 0x10 a band inside and 0x10 more outside; the winding by the sign of the
// task's +0x64. Slot 3 throughout.
S25_EXPORT void __cdecl SpellDepress_DrawVortex(int step) {
    const std::uint32_t rings = bof3::addr::SpellDepress_Rings, outer = bof3::addr::SpellDepress_Outer,
                        inner = bof3::addr::SpellDepress_Inner;
    for (int r = 0; r < 9; ++r) {
        int a = 0;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(rings + 8 * (16 * r + k));
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(Mul(s, Sprite_Current[9] >> 3), r)));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(Mul(c, Sprite_Current[9] >> 3), r)));
            a += 0x100;
        }
    }
    {
        const unsigned char* const sc = Sprite_Current;
        short rect[4];
        rect[0] = static_cast<short>(Word(sc + 0x2E) - sc[9]);
        rect[1] = static_cast<short>(Gfx_BufferIndex * 0xF0 + Word(sc + 0x30) - sc[9]);
        rect[2] = static_cast<short>(sc[9] * 2);
        rect[3] = static_cast<short>(sc[9] * 2);
        MH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), 0x340, 0x100);
    }
    MH_CALL(Gfx_CommitPrim)(3, 0x18);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x11D, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    {
        int a = 0;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(outer + 8 * k);
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(s, Sprite_Current[9])));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(c, Sprite_Current[9])));
            a += 0x100;
        }
    }
    unsigned char shade = 0x70;
    int band_row = 0x70, ring = 7, twist = step, turned = 0;
    for (int band = 8; band != 0; --band) {
        turned += twist;
        int a = turned;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(inner + 8 * k);
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(Mul(s, Sprite_Current[9] >> 3), ring)));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(Mul(c, Sprite_Current[9] >> 3), ring)));
            a += 0x100;
        }
        const auto outside = static_cast<unsigned char>(shade + 0x10);
        for (unsigned k = 1; k <= 16; ++k) {
            unsigned char* const f = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(f);
            f[4] = 0;
            f[5] = 0;
            f[6] = 0;
            unsigned char* const g = Gfx_PacketNext + 0x38;
            MH_CALL(Gpu_SetPolyGT4)(g);
            SetWord(g + 0x2A, 0x11D);
            for (unsigned o : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) g[o] = outside;
            for (unsigned o : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) g[o] = shade;
            const unsigned was = (k - 1) & 0xF, now = k & 0xF;
            const unsigned char* const outer_was = Mem(outer + 8 * was);
            const unsigned char* const outer_now = Mem(outer + 8 * now);
            const unsigned char* const inner_was = Mem(inner + 8 * was);
            const unsigned char* const inner_now = Mem(inner + 8 * now);
            const std::uint32_t tex_was = (was + static_cast<unsigned>(band_row)) << 3;
            const std::uint32_t tex_now = (static_cast<unsigned>(band_row) + now) << 3;
            if (Long(Sprite_Current + 0x64) > 0) {
                VortexVertex(g, 8, f, 8, outer_was);
                VortexVertex(g, 0x1C, f, 0x14, outer_now);
                VortexVertex(g, 0x30, f, 0x20, inner_was);
                VortexVertex(g, 0x44, f, 0x2C, inner_now);
                VortexUV(g, 0x14, rings + 0x80 + tex_was);
                VortexUV(g, 0x28, rings + 0x80 + tex_now);
                VortexUV(g, 0x3C, rings + tex_was);
                VortexUV(g, 0x50, rings + tex_now);
            } else {
                VortexVertex(g, 0x1C, f, 0x14, outer_was);
                VortexVertex(g, 8, f, 8, outer_now);
                VortexVertex(g, 0x44, f, 0x2C, inner_was);
                VortexVertex(g, 0x30, f, 0x20, inner_now);
                VortexUV(g, 0x28, rings + 0x80 + tex_was);
                VortexUV(g, 0x14, rings + 0x80 + tex_now);
                VortexUV(g, 0x50, rings + tex_was);
                VortexUV(g, 0x3C, rings + tex_now);
            }
            MH_CALL(Gfx_CommitPrim)(3, 0x38);
            MH_CALL(Gfx_CommitPrim)(3, 0x54);
        }
        std::memmove(Mem(outer), Mem(inner), 0x80);
        twist += step;
        shade = static_cast<unsigned char>(shade - 0x10);
        --ring;
        band_row -= 0x10;
    }
}

// ===========================================================================
// MAGIC110 (row 38): "Ragnarok"

// original 0x4D5300 (Magic_Rows row 38): phase +1 through a six-entry stack
// table - start, wait for the children, fade in, burst, fade out, then the
// engine's 0x43FE80 (the done flag, free).
S25_EXPORT void __cdecl SpellRagnarok_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::SpellRagnarok_Start,       bof3::addr::SpellRagnarok_WaitChildren,
                                                 bof3::addr::SpellRagnarok_FadeIn,      bof3::addr::SpellRagnarok_Burst,
                                                 bof3::addr::SpellRagnarok_FadeOut,     kDoneAndFree};
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
    MH_AT(Handler, kCentreOnTargets)();
    Sprite_Current[8] = Pointer(at::kOwner)[8];
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
    for (std::uint32_t i = 0; i < 0x100; ++i) SetWord(Mem(0x812980 + 2 * i), Word(Mem(0x80E980 + 2 * i)) | 0x8000u);
    for (std::uint32_t i = 0; i < 0x10; ++i) SetWord(Mem(0x80F980 + 2 * i), Word(Mem(0x80B980 + 2 * i)) | 0x8000u);
    const std::uint16_t first = Word(Mem(0x80E980)), first16 = Word(Mem(0x80B980));
    SetWord(Mem(0x812980), first);
    SetWord(Mem(0x80F980), first16);
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
    MH_CALL(Battle_SetTargetFlags)(Mem(at::kTarget)[0], 0x10);
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
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Bump(Sprite_Current[1]);
}

// original 0x4D55C0: two screen-wide gouraud quads (0..319 x 0..120, 0..319 x
// 120..359) in grey +0xA * 15 (the word 0x903854; +9 * 15 is stored there
// first and overwritten), semi-transparent under tpage 0x35, 0x15 after,
// slot 1.
S25_EXPORT void __cdecl SpellRagnarok_DrawScreenTint(void) {
    constexpr std::int32_t k120 = 0x42F00000, k319 = 0x439F8000, k359 = 0x43B38000;   // 120.0f, 319.0f, 359.0f
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const sc = Sprite_Current;
    unsigned char* p = Gfx_PacketNext;
    SetWord(S(4), sc[9] * 15u);
    SetWord(S(4), sc[0xA] * 15u);
    const std::int32_t corners[2][8] = {{0, 0, k319, 0, 0, k120, k319, k120}, {0, k120, k319, k120, 0, k359, k319, k359}};
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
    TableEntry("SpellRagnarok_Child", bof3::addr::SpellRagnarok_ChildKinds, 3, Sprite_Current[1])();
}

// original 0x4D57A0 (child kind 0, the rising sprite): the sprite bank pointer
// 0x9039D8 at 0x8E3580 around its phase (+2 through SpellRagnarok_SpritePhases,
// 0x65B96C, five entries; the first is MAGIC130's 0x4E47F0, a count down of +9)
// and, from phase 2 while the slot's bit 0 is set, Sprite_UpdateScreen; then
// 0x8B3580 again.
S25_EXPORT void __cdecl SpellRagnarok_SpriteChild(void) {
    SetLong(Mem(0x9039D8), 0x8E3580);
    TableEntry("SpellRagnarok_SpriteChild", bof3::addr::SpellRagnarok_SpritePhases, 5, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if ((sc[0] & 1) && sc[2] >= 2) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(0x9039D8), 0x8B3580);
}

// original 0x4D57E0 (sprite phase 1): the sprite record's set-up - screen
// point (0xA0, 0x12C), +0x24 0x80, +0x25 0x1D, +0x26 0, +0x27 0x1A, +0x28 1,
// scale +0x40 0x30000 / +0x44 0xC000, +0x48 2, +0x5C..+0x5F 0, +0x2A 0,
// +0x29 2, word +0x2C 0, +0x2B 1 - and Sprite_SetAnimation(0); +2 on.
S25_EXPORT void __cdecl SpellRagnarok_SpriteInit(void) {
    SetWord(Sprite_Current + 0x2E, 0xA0);
    SetWord(Sprite_Current + 0x30, 0x12C);
    Sprite_Current[0x25] = 0x1D;
    Sprite_Current[0x26] = 0;
    Sprite_Current[0x27] = 0x1A;
    Sprite_Current[0x28] = 1;
    Sprite_Current[0x24] = 0x80;
    SetLong(Sprite_Current + 0x40, 0x30000);
    SetLong(Sprite_Current + 0x44, 0xC000);
    Sprite_Current[0x48] = 2;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x29] = 2;
    SetWord(Sprite_Current + 0x2C, 0);
    Sprite_Current[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Bump(Sprite_Current[2]);
}

// The sprite's jitter: screen x 0x9F + Rand() & 3.
void Jitter() {
    const int r = MH_CALL(Rand)();
    SetWord(Sprite_Current + 0x2E, static_cast<unsigned>((r & 3) + 0x9F));
}

// original 0x4D58C0 (sprite phase 2): jitter; up 2; scale +0x44 up 0x2000; on
// at 0x18000.
S25_EXPORT void __cdecl SpellRagnarok_SpriteRise(void) {
    Jitter();
    SetWord(Sprite_Current + 0x30, Word(Sprite_Current + 0x30) - 2u);
    SetLong(Sprite_Current + 0x44, Long(Sprite_Current + 0x44) + 0x2000);
    unsigned char* const sc = Sprite_Current;
    if (Long(sc + 0x44) >= 0x18000) Bump(sc[2]);
}

// original 0x4D5910 (sprite phase 3): jitter; up 4; below y 0xB2 the owner's
// count +0xB down and on.
S25_EXPORT void __cdecl SpellRagnarok_SpriteClimb(void) {
    Jitter();
    SetWord(Sprite_Current + 0x30, Word(Sprite_Current + 0x30) - 4u);
    if (S16(Sprite_Current + 0x30) >= 0xB2) return;
    unsigned char* const owner = Pointer(at::kOwner);
    owner[0xB] = static_cast<unsigned char>(owner[0xB] - 1);
    Bump(Sprite_Current[2]);
}

// original 0x4D5960 (sprite phase 4): jitter; up 2; below y 0x80 the owner's
// count down and the slot freed.
S25_EXPORT void __cdecl SpellRagnarok_SpriteLeave(void) {
    Jitter();
    SetWord(Sprite_Current + 0x30, Word(Sprite_Current + 0x30) - 2u);
    if (S16(Sprite_Current + 0x30) >= 0x80) return;
    unsigned char* const owner = Pointer(at::kOwner);
    owner[0xB] = static_cast<unsigned char>(owner[0xB] - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D59A0 (child kind 1, the ring): +2 through
// SpellRagnarok_RingPhases (0x65B980, four entries: init, grow, MAGIC082's
// 0x4C0680 - on once the owner's +0xB is 2 or less - and MAGIC060's 0x4B1740,
// +9 down to the end); then, while live and past phase 0, the disc and rim
// under the actor matrix.
S25_EXPORT void __cdecl SpellRagnarok_RingChild(void) {
    TableEntry("SpellRagnarok_RingChild", bof3::addr::SpellRagnarok_RingPhases, 4, Sprite_Current[2])();
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
    SetLong(Sprite_Current + 0xC, 0x60000);
    SetLong(Sprite_Current + 0x10, 0x60000);
    SetLong(Sprite_Current + 0x34, Long(Sprite_Current + 0xC) + Field_Kind2X);
    SetLong(Sprite_Current + 0x38, Long(Sprite_Current + 0x10) + Field_Kind2Z);
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
    SetLong(Sprite_Current + 0x14, 0x100);
    Sprite_Current[9] = 0x10;
    Bump(Sprite_Current[2]);
}

// original 0x4D5A50 (ring phase 1): radius up 0x40; on at 0xC00.
S25_EXPORT void __cdecl SpellRagnarok_RingGrow(void) {
    SetLong(Sprite_Current + 0x14, Long(Sprite_Current + 0x14) + 0x40);
    unsigned char* const sc = Sprite_Current;
    if (Long(sc + 0x14) == 0xC00) Bump(sc[2]);
}

// original 0x4D5A70: a disc of 64 gouraud triangles of radius (the word) +0x14
// on the matrix's ground plane, grey +9 * 4; tpage 0x55, slot 2.
S25_EXPORT void __cdecl SpellRagnarok_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(S(0), Word(sc + 0x14));
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
    TableEntry("SpellRagnarok_SparkChild", bof3::addr::SpellRagnarok_SparkPhases, 2, Sprite_Current[2])();
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
    const std::uint32_t offsets = bof3::addr::SpellRagnarok_SparkOffsets;
    SetLong(Sprite_Current + 0x34, Long(Mem(offsets + Sprite_Current[0xB] * 8u)) + Field_Kind2X);
    SetLong(Sprite_Current + 0x38, Long(Mem(offsets + 4 + Sprite_Current[0xB] * 8u)) + Field_Kind2Z);
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
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
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2));
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

void MagicS25_Inject() {
    if (bof3::WantsShadow("magic_s25")) magic_s25::SelfTest();
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
    BOF3_INJECT(SpellDepress_Task);
    BOF3_INJECT(SpellDepress_Start);
    BOF3_INJECT(SpellDepress_Open);
    BOF3_INJECT(SpellDepress_Hold);
    BOF3_INJECT(SpellDepress_Close);
    BOF3_INJECT(SpellDepress_DrawVortex);
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
