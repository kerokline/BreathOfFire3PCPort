// Moved here by tools/move_funcs.py from magic_s27.cpp (docs/source-layout-round.md).
#include "game/magic/whelp_breath_flames.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s27.h"
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

namespace raw = magic_s27::raw;

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

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void AddSW(unsigned k, std::uint32_t v) { SetSW(k, SW(k) + v); }

void SetVW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

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

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

void SetPtr(unsigned char* at, const void* p) {
    SetLong(at, static_cast<std::int32_t>(Low32(p)));
}

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

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using PrimFn = void (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The projections with the arguments the originals push: the depth and flag
// pointers past the vertices (ours reads the first).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S27_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// POLY_G3 / POLY_FT3-shaped: the three points at +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S27_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// POLY_G4 / POLY_FT4: the four points at +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S27_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

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

// One step's width of the Whelp beam: SharedScratch +0 = (sin(angle) x step x
// 2 >> 12) + base, divided by +0xB (signed, the original's idiv), plus 1.
void BeamWidth(std::uint32_t angle, int step, int base) {
    const int v = Sin(static_cast<int>(angle));
    const int r = static_cast<int>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(step) * 2u) >> 12;
    const int width = r + base;
    SetSW(0, static_cast<std::uint32_t>(width));
    const unsigned divisor = Sc()[0xB];
    if (divisor == 0) bof3::Fatal("WhelpBreathBeam_Draw: +0xB is 0 - the original divides by zero here");
    SetSW(0, static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(width)) / static_cast<int>(divisor) + 1));
}

// The four texture v bytes of a step: (+9 x 30 + step) x 8 on the near edge,
// one more on the far.
void BeamV(unsigned char* p, unsigned step) {
    const unsigned char* const s = Sc();
    const auto v0 = static_cast<unsigned char>((s[9] * 0x1E + step) << 3);
    const auto v1 = static_cast<unsigned char>((s[9] * 0x1E + step + 1) << 3);
    p[0x15] = v0;
    p[0x29] = v0;
    p[0x3D] = v1;
    p[0x51] = v1;
}

// The shades of a step's quad: +4 x 12 on the centre line, +4 on the edge;
// `centre_first` says which pair of points is the centre line.
void BeamShade(unsigned char* p, bool centre_first) {
    static constexpr unsigned kFirst[6] = {4, 5, 6, 0x2C, 0x2D, 0x2E};      // points 0 and 2
    static constexpr unsigned kSecond[6] = {0x18, 0x19, 0x1A, 0x40, 0x41, 0x42};   // points 1 and 3
    const unsigned char* const s = Sc();
    const unsigned* const centre = centre_first ? kFirst : kSecond;
    const unsigned* const edge = centre_first ? kSecond : kFirst;
    SetSW(6, (s[4] * 12u) & 0xFFFF);
    for (unsigned i = 0; i < 6; ++i) p[centre[i]] = SB(6);
    SetSW(6, s[4]);
    for (unsigned i = 0; i < 6; ++i) p[edge[i]] = SB(6);
}

}  // namespace

// original 0x4DACD0: the kind-2 task. A three-entry stack table by +1 -
// WhelpBreath_Start, WhelpBreath_WaitBeam, Leech_WaitOrbs (group S17's: held
// until +0xB is 0xFF, then the done flag and free).
S27_EXPORT void __cdecl WhelpBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::WhelpBreath_Start, bof3::addr::WhelpBreath_WaitBeam,
                                                 bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("WhelpBreath_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DAD00: the owner's direction byte and position; three children
// of kind 1, parameter 0x0F: the beam (+1 0, +9 4, this task its +0x80), a
// sprite at the owner (+1 1, +0xB 0) and, unless direction bit 1, another (+1
// 1, +0xB 1); CLUT row 26's three runs, the first and third semi-transparent;
// +0xB and +9 0, +1 on.
S27_EXPORT void __cdecl WhelpBreath_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    {
        const unsigned slot = NewTask(0xF);
        unsigned char* const self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 0;
        c[9] = 4;
    }
    {
        const unsigned slot = NewTask(0xF);
        unsigned char* const owner = Owner();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, owner);
        c[1] = 1;
        c[0xB] = 0;
    }
    if ((Sc()[8] & 2) == 0) {
        const unsigned slot = NewTask(0xF);
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, Owner());
        c[1] = 1;
        c[0xB] = 1;
    }
    static constexpr bool kStp[3] = {true, false, true};
    LightClutRow(3, kStp);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DAE80: held until +0xB is 1 (the beam grown); then two more
// sprites (kind 1, 0x0F, +1 1, this task their +0x80) with +0xB 2 and 3; +1
// on.
S27_EXPORT void __cdecl WhelpBreath_WaitBeam(void) {
    if (Sc()[0xB] != 1) return;
    unsigned char* self = nullptr;
    for (unsigned kind = 2; kind <= 3; ++kind) {
        const unsigned slot = NewTask(0xF);
        self = Sc();
        unsigned char* const c = TaskSlot(slot);
        SetPtr(c + 0x80, self);
        c[1] = 1;
        c[0xB] = static_cast<unsigned char>(kind);
    }
    Inc(self[1]);
}

// original 0x4DAF00: the child's kind-1 task, a jmp through
// WhelpBreathChild_Kinds (two entries) by +1: the beam, the sprites.
S27_EXPORT void __cdecl WhelpBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::WhelpBreathBeam_Run, bof3::addr::WhelpBreathSprite_Run};
    const unsigned kind = Sc()[1];
    if (kind >= 2) PastTable("WhelpBreathChild_Task", kind, 2);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4DAF20: a call through WhelpBreathBeam_Phases (five entries) by
// +2 - Aim, Grow, then MAGIC122's sweep, hold and fade - then, unless the
// owner's +0xB is 0xFF or +2 is 0, the beam (a tail jmp).
S27_EXPORT void __cdecl WhelpBreathBeam_Run(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::WhelpBreathBeam_Aim, bof3::addr::WhelpBreathBeam_Grow,
                                                 bof3::addr::BreathBeam_Hit, bof3::addr::BreathBeam_Hold,
                                                 bof3::addr::BreathBeam_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("WhelpBreathBeam_Run", phase, 5);
    magic_harness::Phase(kPhases[phase])();
    if (Owner()[0xB] == 0xFF) return;
    if (Sc()[2] == 0) return;
    Call0(bof3::addr::WhelpBreathBeam_Draw);
}

// original 0x4DAF50: +9 down; at 0 the aim (above); +0xB 0x11, +9 0, +0xA 1,
// +4 0x10, +2 on; sound 0x100.
S27_EXPORT void __cdecl WhelpBreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Aim();
    Sc()[0xB] = 0x11;
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Sc()[4] = 0x10;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4DB090: +0xA up by three (the beam's length), +9 up; at 8 the
// owner's +0xB 1 (WhelpBreath_WaitBeam's signal) and +2 on.
S27_EXPORT void __cdecl WhelpBreathBeam_Grow(void) {
    AddB(Sc()[0xA], 3);
    Inc(Sc()[9]);
    if (Sc()[9] != 8) return;
    Owner()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4DB0D0: the beam - +0xA steps from the task's screen point along
// an angle that wavers about +0x14 (a sine of (+9 + step) x 0x40), 16 units a
// step; each step two semi-transparent POLY_GT4 either side of the line, of
// width (sin x step x 2 >> 12 + step x 4) / +0xB + 1 at the near end and the
// same for step + 1 at the far; page (0x380, 0x100), CLUT (0, 0x1FA). Tpage
// 0x35 for the beam, 0x15 after it, all committed to slot 3. Screen space:
// the points are written as floats, no projection.
S27_EXPORT void __cdecl WhelpBreathBeam_Draw(void) {
    ModeCommit(3, 0x35);
    {
        const unsigned char* const s = Sc();
        SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
    }
    for (int k = 1; k < Sc()[0xA] + 1; ++k) {
        SetSW(0xC, SW(8));
        SetSW(0xE, SW(0xA));
        {
            const std::uint32_t arg = ((static_cast<std::uint32_t>(Sc()[9] + k) << 2) & 0xFF) << 4;
            const int v = Sin(static_cast<int>(arg));
            const int t = static_cast<int>(static_cast<std::uint32_t>(v) * 11u << 5) >> 12;
            const std::uint32_t angle = (static_cast<std::uint32_t>(t) + SW(2)) & 0xFFF;
            SetSW(4, angle);
            const int s1 = Sin(static_cast<int>(angle));
            AddSW(8, static_cast<std::uint32_t>(Shl12(s1, 4)));
            const int c1 = Cos(SS(4));
            AddSW(0xA, static_cast<std::uint32_t>(Shl12(c1, 4)));
        }
        const unsigned b9 = Sc()[9];
        const std::uint32_t a0 = ((static_cast<std::uint32_t>(k) - b9) & 0x3F) << 6;
        const std::uint32_t a1 = ((static_cast<std::uint32_t>(k) - b9 - 1) & 0x3F) << 6;
        // the first quad: the centre line (+0xC, +0xE) .. (+8, +0xA) and the
        // edge a quarter turn one way
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (SW(2) + 0x400u) & 0xFFF);
        BeamWidth(a1, k - 1, k * 4);
        PutFloat(p + 0x1C, SinKAdd(SS(4), 0, 0xC));
        PutFloat(p + 0x20, CosKAdd(SS(4), 0, 0xE));
        BeamWidth(a0, k, k * 4 + 4);
        PutFloat(p + 0x44, SinKAdd(SS(4), 0, 8));
        PutFloat(p + 0x48, CosKAdd(SS(4), 0, 0xA));
        PutFloat(p + 8, SS(0xC));
        PutFloat(p + 0xC, SS(0xE));
        PutFloat(p + 0x30, SS(8));
        PutFloat(p + 0x34, SS(0xA));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
        p[0x14] = 0x40;
        p[0x28] = 0x80;
        p[0x3C] = 0x40;
        p[0x50] = 0x80;
        BeamV(p, static_cast<unsigned>(k));
        BeamShade(p, true);
        Commit(3, 0x54);
        // the second: the edge the other way, the centre line last
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(4, (SW(2) - 0x400u) & 0xFFF);
        BeamWidth(a1, k - 1, k * 4);
        PutFloat(p + 8, SinKAdd(SS(4), 0, 0xC));
        PutFloat(p + 0xC, CosKAdd(SS(4), 0, 0xE));
        BeamWidth(a0, k, k * 4 + 4);
        PutFloat(p + 0x30, SinKAdd(SS(4), 0, 8));
        PutFloat(p + 0x34, CosKAdd(SS(4), 0, 0xA));
        PutFloat(p + 0x1C, SS(0xC));
        PutFloat(p + 0x20, SS(0xE));
        PutFloat(p + 0x44, SS(8));
        PutFloat(p + 0x48, SS(0xA));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
        p[0x14] = 0;
        p[0x28] = 0x40;
        p[0x3C] = 0;
        p[0x50] = 0x40;
        BeamV(p, static_cast<unsigned>(k));
        BeamShade(p, false);
        Commit(3, 0x54);
    }
    ModeCommit(3, 0x15);
}

// original 0x4DB7A0: the sprite child's step: a draw mode (tpage 0x15, slot
// 3); the sprite bank 0x9039D8 switched to the effects' (0x8E3580) around a
// call through WhelpBreathSprite_Phases (eight entries) by +2, then back
// (0x8B3580).
S27_EXPORT void __cdecl WhelpBreathSprite_Run(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::WhelpBreathSprite_Start, bof3::addr::WhelpBreathSprite_Play, bof3::addr::WhelpBreathSprite_Hold,
        bof3::addr::WhelpBreathFlames_Grow,  bof3::addr::WhelpBreathFlames_End, bof3::addr::WhelpBreathGlow_Grow,
        bof3::addr::WhelpBreathGlow_Hold,    bof3::addr::WhelpBreathGlow_End};
    ModeCommit(3, 0x15);
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(raw::kBankEffects)));
    const unsigned phase = Sc()[2];
    if (phase >= 8) PastTable("WhelpBreathSprite_Run", phase, 8);
    magic_harness::Phase(kPhases[phase])();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(raw::kBankDefault)));
}

// original 0x4DB7F0: the sprite at its owner (direction byte, position);
// sprite fields +0x24 4, +0x25 0x1D, +0x26 0, +0x27 0xA1, +0x28 0, +0x29 3,
// +0x2A the owner's direction bit 0, +0x2B 0, word +0x2C 0; then by +0xB
// (a jump table, 0..3; any other nothing):
//   0: +2 on; with direction bit 1 +0x29 4, +0x2A its bit 0 inverted and
//      animation 3, else animation 0;
//   1: +9 0x78, +2 2, animation 1;
//   2: WhelpBreathFlames_Ages cleared, +9 0, +2 3 (the flames);
//   3: +9 0, +0xA 0x50, +2 5 (the glow).
S27_EXPORT void __cdecl WhelpBreathSprite_Start(void) {
    Sc()[8] = Owner()[8];
    AtOwner();
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0xA1;
    s[0x28] = 0;
    s[0x2A] = static_cast<unsigned char>(Owner()[8] & 1);
    s[0x29] = 3;
    s[0x24] = 4;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    s[0x2B] = 0;
    switch (s[0xB]) {
    case 0:
        Inc(s[2]);
        if (Sc()[8] & 2) {
            unsigned char* const t = Sc();
            t[0x29] = 4;
            t[0x2A] = static_cast<unsigned char>(~t[8] & 1);
            MH_CALL(Sprite_SetAnimation)(3);
        } else {
            MH_CALL(Sprite_SetAnimation)(0);
        }
        break;
    case 1:
        s[9] = 0x78;
        Sc()[2] = 2;
        MH_CALL(Sprite_SetAnimation)(1);
        break;
    case 2:
        for (unsigned i = 0; i < 6; ++i) WhelpBreathFlames_Ages[i] = 0;
        s[9] = 0;
        Sc()[2] = 3;
        break;
    case 3:
        s[9] = 0;
        Sc()[0xA] = 0x50;
        Sc()[2] = 5;
        break;
    default: break;
    }
}

// original 0x4DB950: the sprite's screen point; its script ticked once; at its
// end the task freed.
S27_EXPORT void __cdecl WhelpBreathSprite_Play(void) {
    MH_CALL(Sprite_UpdateScreen)();
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) != 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DB970: the script ticked, the screen point; +9 down; at 0 the
// task freed.
S27_EXPORT void __cdecl WhelpBreathSprite_Hold(void) {
    MH_CALL(Sprite_ScriptTick)();
    MH_CALL(Sprite_UpdateScreen)();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DB9A0: the flames; +9 up; at 0x60 +2 on.
S27_EXPORT void __cdecl WhelpBreathFlames_Grow(void) {
    Call0(bof3::addr::WhelpBreathFlames_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x60) Inc(Sc()[2]);
}

// original 0x4DB9D0: the flames; +9 up; at 0x70 the task freed.
S27_EXPORT void __cdecl WhelpBreathFlames_End(void) {
    Call0(bof3::addr::WhelpBreathFlames_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x70) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DBA00: the glow; +9 up; at 0x10 +2 on.
S27_EXPORT void __cdecl WhelpBreathGlow_Grow(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4DBA30: the glow; +0xA down; at 0 +2 on.
S27_EXPORT void __cdecl WhelpBreathGlow_Hold(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4DBA60: the glow; +9 down; at 0 the task freed.
S27_EXPORT void __cdecl WhelpBreathGlow_End(void) {
    Call0(bof3::addr::WhelpBreathGlow_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DBA90: a draw mode (tpage 0x35, slot 3); for each of the three
// offsets of WhelpBreath_FlameOffsets (dx, dz, dy dwords) and each live enemy:
// +0xB the offset's index, +0xA the enemy's, the pair turned by direction
// (0x446770), the task at the enemy plus the offset, the enemy's direction
// byte, and a column of flames under the task's matrix. Then the six ages of
// WhelpBreathFlames_Ages: each i with 3i below +9 up by one, and in phase 3
// each wrapped to 0..15.
S27_EXPORT void __cdecl WhelpBreathFlames_Draw(void) {
    ModeCommit(3, 0x35);
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const offset = reinterpret_cast<const unsigned char*>(WhelpBreath_FlameOffsets) + 0xC * i;
        for (unsigned e = 0; e < 8; ++e) {
            if ((MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) != 0) continue;
            const unsigned char* const r = EnemyRecord(e);
            const records::BattleEnemy* const tr = rv::AsConst<records::BattleEnemy>(r);
            unsigned char* s = Sc();
            s[0xB] = static_cast<unsigned char>(i);
            s[0xA] = static_cast<unsigned char>(e);
            rv::Store(rv::As<records::BattleTask>(s)->step_x, Long(offset));
            rv::Store(rv::As<records::BattleTask>(s)->step_z, Long(offset + 4));
            MH_AT(PrimFn, raw::kTurnOffset)(s);
            s = Sc();
            rv::Store(rv::As<records::BattleTask>(s)->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(tr->x) +
                                                        static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_x)));
            rv::Store(rv::As<records::BattleTask>(s)->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_z) +
                                                        static_cast<std::uint32_t>(tr->z)));
            rv::Store(rv::As<records::BattleTask>(s)->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(offset + 8)) +
                                                        static_cast<std::uint32_t>(tr->height)));
            s[8] = r[8];
            Call0(bof3::addr::WhelpBreathFlames_PushMatrix);
            Call0(bof3::addr::WhelpBreathFlames_DrawColumn);
            MH_CALL(Gte_PopMatrix)();
        }
    }
    const unsigned char* const s = Sc();
    for (unsigned j = 0; j < 6; ++j) {
        if (3 * j < s[9]) Inc(WhelpBreathFlames_Ages[j]);
        if (s[2] == 3) WhelpBreathFlames_Ages[j] &= 0xF;
    }
}

// original 0x4DBBC0: up to six flames stacked in a column, flame j drawn while
// 3j is below +9 and its age WhelpBreathFlames_Ages[j] below 0x10: a
// semi-transparent POLY_FT4 square turned 45 degrees, half-diagonal age x 4 +
// 0x32, at height age x 15, fading (0x61 - age x 6); page (0x340, 0x100),
// CLUT (0x20, 0x1FA), u 8..0x28, v 0x40..0x60. Slot 3.
S27_EXPORT void __cdecl WhelpBreathFlames_DrawColumn(void) {
    for (unsigned j = 0, k = 0; k < 0x12; k += 3, ++j) {
        if (static_cast<int>(k) >= static_cast<int>(Sc()[9])) continue;
        if (WhelpBreathFlames_Ages[j] >= 0x10) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned age = WhelpBreathFlames_Ages[j];
        SetSW(0, age * 4 + 0x32);
        const std::uint32_t height = age * 15;
        static constexpr int kCorners[4] = {0x200, 0x600, 0xE00, 0xA00};
        for (unsigned c = 0; c < 4; ++c) {
            SetVW(8 * c, static_cast<std::uint32_t>(CosK(kCorners[c], 0)));
            SetVW(8 * c + 2, static_cast<std::uint32_t>(SinK(kCorners[c], 0)));
            SetVW(8 * c + 4, height);
        }
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
        p[0x14] = 8;
        p[0x15] = 0x40;
        p[0x24] = 0x28;
        p[0x25] = 0x40;
        p[0x34] = 8;
        p[0x35] = 0x60;
        p[0x44] = 0x28;
        p[0x45] = 0x60;
        SetSW(6, 0x61u - WhelpBreathFlames_Ages[j] * 6u);
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        Commit(3, 0x48);
    }
}

// original 0x4DBE10: the task's matrix pushed (MagicFx_PushActorMatrix's
// shape): Camera_Matrix x the rotation by direction (+8 0: y 0xC00, 1: x
// 0x400, 2: y 0x400, 3: x 0xC00, else none), translation RotTrans of (x >> 9
// - 0x4000, z >> 9 - 0x4000, -(height / 2)).
S27_EXPORT void __cdecl WhelpBreathFlames_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short rot[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    switch (s[8]) {
    case 0: rot[1] = 0xC00; break;
    case 1: rot[0] = 0x400; break;
    case 2: rot[1] = 0x400; break;
    case 3: rot[0] = 0xC00; break;
    default: break;
    }
    short v[4];
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S27_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4DBF00: for each live enemy, the task at it (direction byte, x,
// z, the height word +0x3E) and the glow under the actor's matrix.
S27_EXPORT void __cdecl WhelpBreathGlow_Draw(void) {
    for (unsigned e = 0; e < 8; ++e) {
        if ((MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) != 0) continue;
        const unsigned char* const r = EnemyRecord(e);
        const records::BattleEnemy* const tr = rv::AsConst<records::BattleEnemy>(r);
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        s[8] = r[8];
        ts->x = tr->x;
        ts->z = tr->z;
        rv::Store<unsigned>(ts->height_whole, tr->height_whole);
        MH_CALL(MagicFx_PushActorMatrix)();
        Call0(bof3::addr::WhelpBreathGlow_DrawFan);
        MH_CALL(Gte_PopMatrix)();
    }
}

// original 0x4DBF70: one semi-transparent POLY_G3 from the actor matrix's
// origin to two points at radius 0x100 + (frame & 0x3F), either side (6 x 0x40)
// of the angle WhelpBreath_GlowAngles[+8] x 0x40 (the direction byte
// unbounded, as in the original); the origin +9 x 10 + 1, the rim 1. Tpage
// 0xD5, slot 5.
S27_EXPORT void __cdecl WhelpBreathGlow_DrawFan(void) {
    SetSW(0, (Frame_Counter & 0x3F) + 0x100);
    ModeCommit(5, 0xD5);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetVW(0, 0);
    SetVW(2, 0);
    SetVW(4, 0);
    {
        const int a = static_cast<int>(((WhelpBreath_GlowAngles[Sc()[8]] - 6u) & 0x3F) << 6);
        SetVW(8, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0xA, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0xC, 0);
    }
    {
        const int a = static_cast<int>(((WhelpBreath_GlowAngles[Sc()[8]] + 6u) & 0x3F) << 6);
        SetVW(0x10, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0x12, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0x14, 0);
    }
    const auto centre = static_cast<unsigned char>(Sc()[9] * 10 + 1);
    p[4] = centre;
    p[5] = centre;
    p[6] = centre;
    for (unsigned i : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[i] = 1;
    Rtp3(p);
    MH_CALL(Gte_PrimDepths3_10B)(p);
    Commit(5, 0x34);
}

void WhelpBreathFlames_Inject() {
    BOF3_INJECT(WhelpBreath_Task);
    BOF3_INJECT(WhelpBreath_Start);
    BOF3_INJECT(WhelpBreath_WaitBeam);
    BOF3_INJECT(WhelpBreathChild_Task);
    BOF3_INJECT(WhelpBreathBeam_Run);
    BOF3_INJECT(WhelpBreathBeam_Aim);
    BOF3_INJECT(WhelpBreathBeam_Grow);
    BOF3_INJECT(WhelpBreathBeam_Draw);
    BOF3_INJECT(WhelpBreathSprite_Run);
    BOF3_INJECT(WhelpBreathSprite_Start);
    BOF3_INJECT(WhelpBreathSprite_Play);
    BOF3_INJECT(WhelpBreathSprite_Hold);
    BOF3_INJECT(WhelpBreathFlames_Grow);
    BOF3_INJECT(WhelpBreathFlames_End);
    BOF3_INJECT(WhelpBreathGlow_Grow);
    BOF3_INJECT(WhelpBreathGlow_Hold);
    BOF3_INJECT(WhelpBreathGlow_End);
    BOF3_INJECT(WhelpBreathFlames_Draw);
    BOF3_INJECT(WhelpBreathFlames_DrawColumn);
    BOF3_INJECT(WhelpBreathFlames_PushMatrix);
    BOF3_INJECT(WhelpBreathGlow_Draw);
    BOF3_INJECT(WhelpBreathGlow_DrawFan);
}
