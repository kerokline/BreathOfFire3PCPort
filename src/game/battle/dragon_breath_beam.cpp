// Moved here by tools/move_funcs.py from magic_s27.cpp (docs/source-layout-round.md).
#include "game/battle/dragon_breath_beam.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s27_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap its +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void AddSW(unsigned k, std::uint32_t v) { SetSW(k, SW(k) + v); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl eax, n` then `sar eax, 0xC`.
int Shl12(int a, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

void SetPtr(unsigned char* at, const void* p) {
    SetLong(at, static_cast<std::int32_t>(Low32(p)));
}

int RandCall() { return MH_CALL(Rand)(); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// The originals read the scratch words a product or a sum needs after the
// call: these sequence it so (C++ leaves the order of a call and a read in one
// expression open).
int SinK(int a, unsigned k) {
    const int v = Sin(a);
    return Mul12(v, SS(k));
}

int CosK(int a, unsigned k) {
    const int v = Cos(a);
    return Mul12(v, SS(k));
}

int SinKAdd(int a, unsigned k, unsigned add) {
    const int v = SinK(a, k);
    return v + SS(add);
}

int CosKAdd(int a, unsigned k, unsigned add) {
    const int v = CosK(a, k);
    return v + SS(add);
}

// (sin << n) >> 12 plus a scratch word, 16 bits.
std::uint32_t SinShlW(int a, unsigned n, unsigned add) {
    const int v = Sin(a);
    return static_cast<std::uint32_t>(Shl12(v, n)) + SW(add);
}

std::uint32_t CosShlW(int a, unsigned n, unsigned add) {
    const int v = Cos(a);
    return static_cast<std::uint32_t>(Shl12(v, n)) + SW(add);
}

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn1 = void (__cdecl*)(unsigned);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call1(std::uint32_t address, unsigned a) { MH_WORD(Fn1, address)(a); }

// A draw mode (textured off, dithered, `tpage`) committed to ordering slot
// `slot`, as each draw brackets its primitives.
void ModeCommit(unsigned slot, unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The CLUT row the three spells light: row 26 of Gfx_ClutStrip (entries
// 0x1A00..) from its source, `runs` runs of fifteen entries (the first of
// each 0), each run's STP bit set as `stp` says, and the strip marked dirty.
void LightClutRow(unsigned runs, const bool* stp) {
    for (unsigned r = 0; r < runs; ++r)
        for (unsigned i = 1; i < 0x10; ++i) {
            const unsigned k = 0x1A00 + r * 0x10 + i;
            Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | (stp[r] ? 0x8000 : 0));
        }
    for (unsigned r = 0; r < runs; ++r) Gfx_ClutStrip[0x1A00 + r * 0x10] = 0;
    Gfx_ClutStripDirty = 1;
}

// The task at its owner: +0x34 / +0x38 / +0x3C from the owner's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S27_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The beams' aim, both spells (WhelpBreathBeam_Aim, DragonBreathBeam_Aim once
// +9 runs out): the owner's direction byte; the task to the targets' centre
// (MagicFx_CenterOnSide) and its screen point kept in SharedScratch +0xC /
// +0xE; MagicFx_FormationOffset; the task back at its owner, its screen point
// moved by the words +0xC / +0x10 into +8 / +0xA; +0x14 the angle
// Math_Ratan2(kept x - x, kept y - y) from there to the centre.
void Aim() {
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    MH_CALL(MagicFx_FormationOffset)();
    AtOwner();
    MH_CALL(BattleActor_UpdateScreenXY)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
    const unsigned char* const s = Sc();
    const short x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(8, static_cast<std::uint16_t>(x));
    SetSW(0xA, static_cast<std::uint16_t>(y));
    // Math_Ratan2(y, x) as the original pushes it: first the kept x less x,
    // then the kept y less y.
    const float first = static_cast<float>(SS(0xC) - x);
    const float second = static_cast<float>(SS(0xE) - y);
    const int angle = MH_CALL(Math_Ratan2)(first, second);
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
}

// The Dragon beam's radii of one quad pair: SharedScratch +0 at the near end
// and +2 at the far, `base` (+0xB x 4, or +0xB x 4 + 0x10 for the outer pair)
// or, over the first steps, `per` x step (near: step - 1 while it is below 4;
// far: step while it is below 4); each plus its end's jitter.
void DragonRadii(unsigned base_add, unsigned per_add, int k, unsigned prev, unsigned cur) {
    const unsigned b = Sc()[0xB];
    const std::uint32_t base = b * 4 + base_add;
    SetSW(2, base);
    SetSW(0, base);
    std::uint32_t far = base;
    if (k - 1 < 4) SetSW(0, (b + per_add) * static_cast<std::uint32_t>(k - 1));
    if (k < 4) far = (b + per_add) * static_cast<std::uint32_t>(k);
    AddSW(0, prev);
    SetSW(2, far + cur);
}

// The four points of a Dragon quad: the side points at angle SharedScratch +4
// and radii +0 / +2 about the step's near (+0xC, +0xE) and far (+8, +0xA)
// centres, at `near_at` and `far_at`; the centres themselves at `near_c` and
// `far_c`.
void DragonSides(unsigned char* p, unsigned near_at, unsigned far_at) {
    PutFloat(p + near_at, SinKAdd(SS(4), 0, 0xC));
    PutFloat(p + near_at + 4, CosKAdd(SS(4), 0, 0xE));
    PutFloat(p + far_at, SinKAdd(SS(4), 2, 8));
    PutFloat(p + far_at + 4, CosKAdd(SS(4), 2, 0xA));
}

void DragonCentres(unsigned char* p, unsigned near_c, unsigned far_c) {
    PutFloat(p + near_c, SS(0xC));
    PutFloat(p + near_c + 4, SS(0xE));
    PutFloat(p + far_c, SS(8));
    PutFloat(p + far_c + 4, SS(0xA));
}

// A point's shade: (s, s, s) on the centre line, (s, s / 4, s / 4) on the
// edge, 1 on the outer edge; s the SharedScratch +6 word, / 4 signed.
void ShadeFull(unsigned char* p, unsigned at) {
    p[at] = SB(6);
    p[at + 1] = SB(6);
    p[at + 2] = SB(6);
}

void ShadeRed(unsigned char* p, unsigned at) {
    p[at] = SB(6);
    p[at + 1] = static_cast<unsigned char>(SS(6) / 4);
    p[at + 2] = static_cast<unsigned char>(SS(6) / 4);
}

void ShadeOne(unsigned char* p, unsigned at) {
    p[at] = 1;
    p[at + 1] = 1;
    p[at + 2] = 1;
}

// DragonBreathBeam_DrawLinesA / B: a crackling line of 47 segments (POLY_F2
// lines, semi-transparent) beside the beam, `side` 0x400 or -0x400 from its
// angle. It starts `n` + 1 steps of 12 down the beam (SharedScratch +8 / +0xA,
// kept in the vertex scratch +8 / +0xA), 32 units out; each segment walks
// Rand & 7 along the beam (back to the kept point plus 12 once the walk
// passes 12) and swings out by sin(segment x 0x40) x (0x10 + Rand & 3) >> 12;
// shade ((Rand & 7) + 5) x +4, blue a third of it. Tpage 0x35, 0x15 after,
// slot 3.
void DrawLines(unsigned n, unsigned side) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    for (unsigned i = (n & 0xFF) + 1; i != 0; --i) {
        AddSW(8, static_cast<std::uint32_t>(Mul12(Sin(rv::Current<records::BattleTask>()->step_y), 12)));
        AddSW(0xA, static_cast<std::uint32_t>(Mul12(Cos(rv::Current<records::BattleTask>()->step_y), 12)));
    }
    SetVW(8, SW(8));
    SetVW(0xA, SW(0xA));
    SetSW(4, (Word(Sc() + 0x14) + side) & 0xFFF);
    SetSW(0xC, SinShlW(SS(4), 5, 8));
    SetSW(0xE, CosShlW(SS(4), 5, 0xA));
    SetSW(0, static_cast<std::uint32_t>(Shl12(Sin(0), 4)));
    SetVW(0, static_cast<std::uint32_t>(SinKAdd(SS(4), 0, 0xC)));
    SetVW(2, static_cast<std::uint32_t>(CosKAdd(SS(4), 0, 0xE)));
    {
        const int r = RandCall();
        SetSW(6, ((static_cast<std::uint32_t>(r) & 7) + 5) * Sc()[4]);
    }
    int walked = 0;
    for (unsigned k = 1; k < 0x30; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint32_t step = static_cast<std::uint32_t>(RandCall()) & 7;
        walked += static_cast<int>(static_cast<short>(step));
        SetSW(0, step);
        if (walked < 12) {
            AddSW(8, static_cast<std::uint32_t>(SinK(Long(Sc() + 0x14), 0)));
            AddSW(0xA, static_cast<std::uint32_t>(CosK(rv::Current<records::BattleTask>()->step_y, 0)));
            SetSW(0xC, SinShlW(SS(4), 5, 8));
        } else {
            walked = 0;
            {
                const int v = Sin(Long(Sc() + 0x14));
                SetSW(8, static_cast<std::uint32_t>(Mul12(v, 12)) + VW(8));
            }
            {
                const int c = Cos(rv::Current<records::BattleTask>()->step_y);
                const std::uint16_t x = SW(8);
                const std::uint32_t y = static_cast<std::uint32_t>(Mul12(c, 12)) + VW(0xA);
                SetVW(8, x);
                SetSW(0xA, y);
                SetVW(0xA, y);
            }
            SetSW(0xC, SinShlW(SS(4), 5, 8));
        }
        SetSW(0xE, CosShlW(SS(4), 5, 0xA));
        {
            const std::uint32_t a = (k & 0x1F) << 6;
            SetSW(2, a);
            const int v = Sin(static_cast<int>(static_cast<short>(a)));
            const std::uint32_t swing = (static_cast<std::uint32_t>(RandCall()) & 3) + 0x10;
            const int out = static_cast<int>(static_cast<std::uint32_t>(v) * swing) >> 12;
            PutFloat(p + 8, VS(0));
            SetSW(0, static_cast<std::uint32_t>(out));
            PutFloat(p + 0xC, VS(2));
        }
        {
            const auto x = static_cast<short>(SinKAdd(SS(4), 0, 0xC));
            SetVW(0, static_cast<std::uint16_t>(x));
            PutFloat(p + 0x14, x);
        }
        {
            const auto y = static_cast<short>(CosKAdd(SS(4), 0, 0xE));
            SetVW(2, static_cast<std::uint16_t>(y));
            PutFloat(p + 0x18, y);
        }
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = static_cast<unsigned char>(SS(6) / 3);
        Commit(3, 0x20);
    }
    ModeCommit(3, 0x15);
}

}  // namespace

// original 0x4DC110: the kind-2 task. A two-entry stack table by +1 -
// DragonBreath_Start, Leech_WaitOrbs (held until +0xB is 0xFF).
S27_EXPORT void __cdecl DragonBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::DragonBreath_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("DragonBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DC140: the owner's direction byte and position; the beam (kind
// 1, 0x5A: +1 0, +9 4, this task its +0x80); CLUT row 26's three runs, the
// first and third semi-transparent; sound 0x100; +0xB and +9 0, +1 on.
S27_EXPORT void __cdecl DragonBreath_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    {
        const unsigned slot = NewTask(0x5A);
        unsigned char* const self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 0;
        c[9] = 4;
    }
    static constexpr bool kStp[3] = {true, false, true};
    LightClutRow(3, kStp);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DC260: the beam's kind-1 task, a jmp through
// DragonBreathChild_Kinds (one entry) by +1.
S27_EXPORT void __cdecl DragonBreathBeam_Task(void) {
    const unsigned kind = Sc()[1];
    if (kind >= 1) PastTable("DragonBreathBeam_Task", kind, 1);
    magic_harness::Phase(bof3::addr::DragonBreathBeam_Run)();
}

// original 0x4DC280: a call through DragonBreathBeam_Phases (seven entries) by
// +2; then while +0 and +2 are set: every eighth frame three new jitters
// (+0x5D Rand & 1, +0x5E and +0x5F Rand & 3), six crackling lines - three on
// one side (lengths +0x5D + 3, +0x5E + 7, 0xB - +0x5F steps), three on the
// other (4 - +0x5D, +0x5F + 8, 0xC - +0x5E) - and the beam (a tail jmp).
S27_EXPORT void __cdecl DragonBreathBeam_Run(void) {
    static constexpr std::uint32_t kPhases[7] = {
        bof3::addr::DragonBreathBeam_Aim,      bof3::addr::DragonBreathBeam_Widen, bof3::addr::DragonBreathBeam_Hit,
        bof3::addr::DragonBreathBeam_Brighten, bof3::addr::DragonBreathBeam_Hold,  bof3::addr::DragonBreathBeam_Dim,
        bof3::addr::DragonBreathBeam_End};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("DragonBreathBeam_Run", phase, 7);
    magic_harness::Phase(kPhases[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    if ((Frame_Counter & 7) == 0) {
        Sc()[0x5D] = static_cast<unsigned char>(RandCall() & 1);
        Sc()[0x5E] = static_cast<unsigned char>(RandCall() & 3);
        Sc()[0x5F] = static_cast<unsigned char>(RandCall() & 3);
    }
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(Sc()[0x5D] + 3));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(Sc()[0x5E] + 7));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesA, static_cast<unsigned char>(0xB - Sc()[0x5F]));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(4 - Sc()[0x5D]));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(Sc()[0x5F] + 8));
    Call1(bof3::addr::DragonBreathBeam_DrawLinesB, static_cast<unsigned char>(0xC - Sc()[0x5E]));
    Call0(bof3::addr::DragonBreathBeam_Draw);
}

// original 0x4DC360: +9 down; at 0 the aim (as WhelpBreathBeam_Aim's), the
// three jitters, dword +0xC 0, +4 0, +0xB 0, +9 0x10, +0xA 0, +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Aim();
    Sc()[0x5D] = static_cast<unsigned char>(RandCall() & 1);
    Sc()[0x5E] = static_cast<unsigned char>(RandCall() & 3);
    Sc()[0x5F] = static_cast<unsigned char>(RandCall() & 3);
    rv::Current<records::BattleTask>()->step_x = 0;
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4DC4D0: +0xA (the length) up by 8; at 0x20 +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Widen(void) {
    AddB(Sc()[0xA], 8);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4DC4F0: +0xB (the width) up; at 8 the target flags 0x10 and +2
// on.
S27_EXPORT void __cdecl DragonBreathBeam_Hit(void) {
    Inc(Sc()[0xB]);
    if (Sc()[0xB] != 8) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[2]);
}

// original 0x4DC530: +4 up by two; at 0x10 +2 on (a body seven overlays
// reach).
S27_EXPORT void __cdecl DragonBreathBeam_Brighten(void) {
    AddB(Sc()[4], 2);
    if (Sc()[4] == 0x10) Inc(Sc()[2]);
}

// original 0x4DC550: dword +0xC up; at 0x2E +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Hold(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->step_x = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->step_x) + 1);
    if (rv::Current<records::BattleTask>()->step_x == 0x2E) Inc(Sc()[2]);
}

// original 0x4DC570: +4 down by two; at 0 +2 on.
S27_EXPORT void __cdecl DragonBreathBeam_Dim(void) {
    AddB(Sc()[4], 0xFE);
    if (Sc()[4] == 0) Inc(Sc()[2]);
}

// original 0x4DC590: +0xB down unless 0; +9 down; at 0 the owner's +0xB 0xFF
// (Leech_WaitOrbs's signal) and the task freed.
S27_EXPORT void __cdecl DragonBreathBeam_End(void) {
    if (Sc()[0xB] != 0) Dec(Sc()[0xB]);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DC5D0: the beam - +0xA steps of 12 units from the task's screen
// point along +0x14; each step four semi-transparent POLY_G4, two either side:
// the inner pair from the centre line out to +0xB x 4 (tapered over the first
// four steps), the outer pair from there to +0xB x 4 + 0x10; each edge moved
// by a jitter Rand & 3 (a step's far jitter is the next step's near one);
// shaded by +9 x 15. Tpage 0x35 for the beam, 0x15 after it, slot 3. Screen
// space: the points are written as floats.
S27_EXPORT void __cdecl DragonBreathBeam_Draw(void) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    unsigned cur = 0;
    for (int k = 1; k < Sc()[0xA] + 1; ++k) {
        SetSW(0xC, SW(8));
        SetSW(0xE, SW(0xA));
        AddSW(8, static_cast<std::uint32_t>(Mul12(Sin(rv::Current<records::BattleTask>()->step_y), 12)));
        AddSW(0xA, static_cast<std::uint32_t>(Mul12(Cos(rv::Current<records::BattleTask>()->step_y), 12)));
        {
            const unsigned char* const s = Sc();
            const unsigned b = s[0xB];
            SetSW(2, b * 4);
            SetSW(0, b * 4);
            if (k - 1 < 4) SetSW(0, static_cast<std::uint32_t>(k - 1) * b);
            if (k < 4) SetSW(2, b * static_cast<std::uint32_t>(k));
            SetSW(6, s[9] * 15u);
        }
        const unsigned prev = cur;
        const int r = RandCall();
        AddSW(0, prev);
        cur = static_cast<unsigned>(r) & 3;
        AddSW(2, cur);
        // the inner pair: the centre line and the edge at +0x400
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) + 0x400u) & 0xFFF);
        DragonSides(p, 0x18, 0x38);
        DragonCentres(p, 8, 0x28);
        ShadeFull(p, 4);
        ShadeFull(p, 0x24);
        ShadeRed(p, 0x14);
        ShadeRed(p, 0x34);
        Commit(3, 0x44);
        // and at -0x400
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) - 0x400u) & 0xFFF);
        DragonSides(p, 8, 0x28);
        DragonCentres(p, 0x18, 0x38);
        ShadeFull(p, 0x14);
        ShadeFull(p, 0x34);
        ShadeRed(p, 4);
        ShadeRed(p, 0x24);
        Commit(3, 0x44);
        // the outer pair at +0x400: from the inner edge out
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) + 0x400u) & 0xFFF);
        DragonRadii(0x10, 4, k, prev, cur);
        DragonSides(p, 0x18, 0x38);
        DragonRadii(0, 0, k, prev, cur);
        DragonSides(p, 8, 0x28);
        ShadeRed(p, 4);
        ShadeRed(p, 0x24);
        ShadeOne(p, 0x14);
        ShadeOne(p, 0x34);
        Commit(3, 0x44);
        // and at -0x400
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (Word(Sc() + 0x14) - 0x400u) & 0xFFF);
        DragonRadii(0x10, 4, k, prev, cur);
        DragonSides(p, 8, 0x28);
        DragonRadii(0, 0, k, prev, cur);
        DragonSides(p, 0x18, 0x38);
        ShadeRed(p, 0x14);
        ShadeRed(p, 0x34);
        ShadeOne(p, 4);
        ShadeOne(p, 0x24);
        Commit(3, 0x44);
    }
    ModeCommit(3, 0x15);
}

// original 0x4DD0B0: the crackling line at +0x400 (above), its length the
// argument's low byte.
S27_EXPORT void __cdecl DragonBreathBeam_DrawLinesA(unsigned n) { DrawLines(n, 0x400); }

// original 0x4DD4B0: the same at -0x400.
S27_EXPORT void __cdecl DragonBreathBeam_DrawLinesB(unsigned n) { DrawLines(n, 0xFFFFFC00u); }

void DragonBreathBeam_Inject() {
    BOF3_INJECT(DragonBreath_Task);
    BOF3_INJECT(DragonBreath_Start);
    BOF3_INJECT(DragonBreathBeam_Task);
    BOF3_INJECT(DragonBreathBeam_Run);
    BOF3_INJECT(DragonBreathBeam_Aim);
    BOF3_INJECT(DragonBreathBeam_Widen);
    BOF3_INJECT(DragonBreathBeam_Hit);
    BOF3_INJECT(DragonBreathBeam_Brighten);
    BOF3_INJECT(DragonBreathBeam_Hold);
    BOF3_INJECT(DragonBreathBeam_Dim);
    BOF3_INJECT(DragonBreathBeam_End);
    BOF3_INJECT(DragonBreathBeam_Draw);
    BOF3_INJECT(DragonBreathBeam_DrawLinesA);
    BOF3_INJECT(DragonBreathBeam_DrawLinesB);
}
