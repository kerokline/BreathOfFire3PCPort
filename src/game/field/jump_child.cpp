// Moved here by tools/move_funcs.py from magic_s01.cpp (docs/source-layout-round.md).
#include "game/field/jump_child.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s01.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the draw keeps its radius in (SharedScratch's words +0 and +2)
// and the three SVECTORs of Prim_VertexScratch it projects; both read again
// after every call, as the original reads them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

short SS(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(kS.v) + k))); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// A dword added in place, wrapping.
void AddL(unsigned char* a, std::uint32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl 0xD` then `sar 0xC` on a dword: twice the value, its top bit lost.
std::uint32_t Twice(int v) { return static_cast<std::uint32_t>(static_cast<int>(static_cast<std::uint32_t>(v) << 13) >> 12); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// This group's functions called directly, as the original calls them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Gte_RotTransPers3 as the original pushes it: one pointer more than
// symbols.toml's prototype carries (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

// The children's motion: the vertical speed +0x14 by its step +0x20, the
// height word +0x3E by the speed's low word.
void Rise() {
    unsigned char* s = Sc();
    AddL(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
}

// At the top of the rise: over the source sprite (+0x34, +0x38), the height
// +0x3C moved by the source's less the owner's.
void OverSource(unsigned char* s) {
    rv::Store(rv::As<records::SpriteHead>(s)->x, rv::As<records::SpriteHead>(Source())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Source())->z);
    const std::uint32_t d = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Source())->height) - static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height);
    AddL(Sc() + 0x3C, d);
}

// The tint +0x5D..+0x5F down by 0x10 a frame until +0x5D is 0x80; then back
// at the owner's position (+0x34..+0x3C), +2 on. `shrink`: the shadow +0xA
// down by 2 while not 0 (Jump's).
void FadeHome(bool shrink) {
    unsigned char* s = Sc();
    const unsigned char tint = s[0x5D];
    if (tint != 0x80) {
        s[0x5D] = static_cast<unsigned char>(tint - 0x10);
        AddB(Sc()[0x5E], 0xF0);
        AddB(Sc()[0x5F], 0xF0);
        s = Sc();
    }
    if (shrink && s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
        s = Sc();
    }
    if (s[0x5D] != 0x80) return;
    rv::Store(rv::As<records::BattleTask>(s)->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Inc(Sc()[2]);
}

// One MATRIX block as the original lays it out on its stack, RotTrans
// writing its translation at +0x14 (DIV-0021's padding word at +0x12).
struct Matrix {
    short m[10];
    long t[3];
};

}  // namespace

#define S01_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x499750 (both rows): once the child has ended (+0xB 0), the effect
// CLUT row restored, the owner's +0 bit 0x40 cleared, +1 on.
S01_EXPORT void __cdecl Magic001_EndWhenChildDone(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x499780: the child's kind-1 task, a jmp through
// Magic001_ChildTaskTable + 4 (one entry: JumpChild_Run) by +1, unchecked.
S01_EXPORT void __cdecl JumpChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("JumpChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::JumpChild_Run)();
}

// original 0x4997A0: a seven-entry stack table by +2; then while +0 and +2
// are set, the sprite's screen point, and below step 5 the shadow: its matrix,
// its disc, the matrix popped.
S01_EXPORT void __cdecl JumpChild_Run(void) {
    static constexpr std::uint32_t kSteps[7] = {bof3::addr::JumpChild_Begin,     bof3::addr::JumpChild_Rise,
                                                bof3::addr::JumpChild_Hover,     bof3::addr::JumpChild_Return,
                                                bof3::addr::SacrificeActor_Play, bof3::addr::JumpChild_Fade,
                                                bof3::addr::AirRaidImage_FadeOut};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("JumpChild_Run", phase, 7);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
    if (Sc()[2] >= 5) return;
    Call0(bof3::addr::JumpChild_PushMatrix);
    Call0(bof3::addr::JumpChild_DrawShadow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x499820: the owner's CLUT row byte +0x27; the speed 0x100 with the
// step -0x10; +9 0, the shadow +0xA 0x10; +2 on.
S01_EXPORT void __cdecl JumpChild_Begin(void) {
    Sc()[0x27] = Owner()[0x27];
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -0x10);
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x499870: rising, the shadow +0xA down while not 0; when the speed
// reaches 0, over the source sprite, +9 0xF, +2 on.
S01_EXPORT void __cdecl JumpChild_Rise(void) {
    Rise();
    unsigned char* s = Sc();
    if (s[0xA] != 0) {
        Dec(s[0xA]);
        s = Sc();
    }
    if (rv::As<records::BattleTask>(s)->step_y != 0) return;
    OverSource(s);
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x499900: falling, the shadow +0xA up to 8; +9 down; at 0
// BattleActor_PlaySound(2, 0), sound 0x203, the speed 0x40 with the step -8,
// the heading +0xC Math_Ratan2(owner x - x, owner z - z), the target flagged
// 0x40, +9 0x10, +2 on.
S01_EXPORT void __cdecl JumpChild_Hover(void) {
    Rise();
    unsigned char* s = Sc();
    if (s[0xA] < 8) {
        Inc(s[0xA]);
        s = Sc();
    }
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Sound_PlayById)(0x203);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x40);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -8);
    const unsigned char* const o = Owner();
    const unsigned char* const t = Sc();
    const records::BattleTask* const tt = rv::AsConst<records::BattleTask>(t);
    const auto dz = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->z) - static_cast<std::uint32_t>(tt->z));
    const auto dx = static_cast<int>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(o)->x) - static_cast<std::uint32_t>(tt->x));
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    rv::Current<records::BattleTask>()->step_x = heading;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4999E0: along the heading: x += 2 Math_Sin(+0xC), z += 2
// Math_Cos(+0xC) (each `shl 0xD / sar 0xC`, through the field the call left
// it pointing at); moving; while +9 is below 8 the shadow +0xA up to 0x10;
// +9 down; at 0 +2 on.
S01_EXPORT void __cdecl JumpChild_Return(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        unsigned char* const x = s + 0x34;
        const int v = MH_CALL(Math_Sin)(ts->step_x);
        AddL(x, Twice(v));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        unsigned char* const z = s + 0x38;
        const int v = MH_CALL(Math_Cos)(ts->step_x);
        AddL(z, Twice(v));
    }
    Rise();
    unsigned char* const s = Sc();
    if (s[9] < 8 && s[0xA] < 0x10) Inc(s[0xA]);
    Dec(s[9]);
    if (s[9] != 0) return;
    Inc(s[2]);
}

// original 0x499A80: the tint faded to 0x80 and the shadow shrunk by 2 a
// frame, then home (FadeHome).
S01_EXPORT void __cdecl JumpChild_Fade(void) { FadeHome(true); }

// original 0x499B10: the shadow's matrix pushed: Camera_Matrix x the identity
// rotation (angles 0, 0, 0), translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)), the height the owner's +0x3E below step 2,
// the source sprite's after. The task is read once, after the push.
S01_EXPORT void __cdecl JumpChild_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    short v[4];
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    const int height = s[2] < 2 ? S16(Owner() + 0x3E) : S16(Source() + 0x3E);
    v[2] = static_cast<short>(-(height / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    MH_CALL(As<RotTransFn>(&::Gte_RotTrans))(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x499BD0: the shadow, a disc of sixteen gouraud triangles in the
// matrix's plane: a draw-mode packet (tpage 0x55) to layer 5; the radius
// +0xA x 8 into scratch words +0 and +2; for each 0x100 of angle from 0x100
// to 0x1000 a triangle of the centre, the previous rim point and the next
// (radius x Math_Sin / Math_Cos >> 12 in Prim_VertexScratch's third
// SVECTOR, its z 0), projected, depths by Gte_PrimDepths3_10B, the centre's
// colour the radius byte (scratch +2) in all three channels and the rim's 1,
// semi-transparent, committed to layer 5; then a draw-mode packet with tpage
// 0x15. The scratch and the vertices are read again after every call.
S01_EXPORT void __cdecl JumpChild_DrawShadow(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(2, static_cast<unsigned>(s[0xA]) << 3);
        SetSW(0, static_cast<unsigned>(s[0xA]) << 3);
    }
    {
        const int v = MH_CALL(Math_Sin)(0);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    {
        const int v = MH_CALL(Math_Cos)(0);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t rim_x = VW(0x10), rim_z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, rim_x);
        SetVW(0xA, rim_z);
        {
            const int v = MH_CALL(Math_Sin)(angle);
            SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        }
        {
            const int v = MH_CALL(Math_Cos)(angle);
            SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        }
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        long depth, flag;
        MH_CALL(As<Rtp3Fn>(&::Gte_RotTransPers3))(VP(0), VP(8), VP(0x10), reinterpret_cast<float*>(p + 8),
                                                  reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                                  &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(2);
        p[5] = SB(2);
        p[6] = SB(2);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void JumpChild_Inject() {
    BOF3_INJECT(Magic001_EndWhenChildDone);
    BOF3_INJECT(JumpChild_Task);
    BOF3_INJECT(JumpChild_Run);
    BOF3_INJECT(JumpChild_Begin);
    BOF3_INJECT(JumpChild_Rise);
    BOF3_INJECT(JumpChild_Hover);
    BOF3_INJECT(JumpChild_Return);
    BOF3_INJECT(JumpChild_Fade);
    BOF3_INJECT(JumpChild_PushMatrix);
    BOF3_INJECT(JumpChild_DrawShadow);
}
