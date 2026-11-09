// Moved here by tools/move_funcs.py from magic_s14.cpp (docs/source-layout-round.md).
#include "game/magic/tsunami_wave.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/magic_harness.h"
#include "game/map_rim.h"
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
// sixteen bytes (0x903850..0x90385F; words by function) and the four SVECTORs
// of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The party records by the battle index, unchecked (the originals index
// ObjTrio by the actor byte whatever it holds).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers4 gets
// the depth and flag pointers the originals pass; ours reads the first.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S14_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S14_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

}  // namespace

#define S14_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// TsunamiWave_Draw's edge i: the near edge moves to the far one (x 0 and
// 0x1000, y and z the last edge's), the far edge at y (-0x80 - width / 2) x i
// (width +0x10, sar 1), z the amplitude +0x14 x the sine of ((+0xB + i) & 0x1F)
// << 7, sar 12; then the shade by the step: at step 4 the width / 2 (toward
// zero) x `step4`, else `shade`.
void WaveEdge(int i, unsigned step4, unsigned shade) {
    const std::uint16_t y = VW(2), z = VW(4);
    SetVW(0x12, y);
    SetVW(0x1A, y);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        SetVW(0x10, 0);
        SetVW(0x14, z);
        SetVW(0x18, 0x1000);
        SetVW(0x1C, z);
        const unsigned angle = ((static_cast<unsigned>(s[0xB]) + static_cast<unsigned>(i)) & 0x1Fu) << 7;
        SetVW(0, 0);
        SetSW(6, angle);
        const int half = ts->step_z >> 1;
        SetVW(2, static_cast<unsigned>(-0x80 - half) * static_cast<unsigned>(i));
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int w = Mul12(v, SS(0));
        const std::uint16_t y2 = VW(2);
        SetVW(8, 0x1000);
        SetVW(4, static_cast<unsigned>(w));
        SetVW(0xC, static_cast<unsigned>(w));
        SetVW(0xA, y2);
    }
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    if (s[2] == 4) SetSW(2, static_cast<unsigned>(DivPow2(ts->step_z, 1)) * step4);
    else SetSW(2, shade);
}

// One ring round a live actor: +0xB the height index, +0xA the actor's index
// on its side, the position its +0x34 / +0x38 / +0x3C plus the height's
// offsets; the ring's matrix, its quads, the matrix popped.
void RingAt(unsigned height, unsigned index, const unsigned char* actor) {
    const unsigned char* const offset = Mem(::bof3::Img(TsunamiRing_Offsets_addr) + 0xC * height);
    Sc()[0xB] = static_cast<unsigned char>(height);
    Sc()[0xA] = static_cast<unsigned char>(index);
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(U(Long(offset)) + U(rv::As<records::SpriteHead>(actor)->x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(actor)->z) + U(Long(offset + 4))));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(U(Long(offset + 8)) + U(rv::As<records::SpriteHead>(actor)->height)));
    Call0(bof3::addr::TsunamiRing_PushMatrix);
    Call0(bof3::addr::TsunamiRing_DrawQuads);
    MH_CALL(Gte_PopMatrix)();
}

}  // namespace

// original 0x4B59A0: the kind-2 task. A three-entry stack table by +1:
// Tsunami_Start, Tsunami_Rings, and MAGIC077's Leech_WaitOrbs (at the owner's
// signal +0xB 0xFF: the target flagged 0x40, the done flag, free).
S14_EXPORT void __cdecl Tsunami_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Tsunami_Start, bof3::addr::Tsunami_Rings,
                                                 bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Tsunami_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B59D0: the owner's position +0x34 / +0x38 / +0x3C copied; the
// wave (kind 1, 0x11, +0x80 this task, +1 0); CLUT row 26 back from its source
// with the STP bit, and row 2's first 16 words the same, the first of them
// then without it; the strip dirty; sound effect 0x100; +0xB 0, +9 0x18, +1
// on.
S14_EXPORT void __cdecl Tsunami_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    const unsigned slot = NewTask(0x11);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(Sc()));
    child[1] = 0;
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayEffect)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
}

// original 0x4B5AB0: +9 down; at 0 the rings (kind 1, 0x11, +0x80 this task,
// +1 1), the target flagged 0x20, +1 on.
S14_EXPORT void __cdecl Tsunami_Rings(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned slot = NewTask(0x11);
    const unsigned char target = TargetByte();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(Sc()));
    child[1] = 1;
    MH_CALL(Battle_SetTargetFlags)(target, 0x20);
    Inc(Sc()[1]);
}

// original 0x4B5B10: the children's kind-1 task, a jmp through
// TsunamiChild_Kinds (two entries: the wave, the rings) by +1, unchecked.
S14_EXPORT void __cdecl TsunamiChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::TsunamiWave_Run, bof3::addr::TsunamiRings_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("TsunamiChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B5B30: a call through TsunamiWave_Steps (five entries) by +2;
// then, unless the owner's +0xB is 0xFF (the wave has ended), the actor
// matrix, the wave, the matrix popped (a tail jmp).
S14_EXPORT void __cdecl TsunamiWave_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::TsunamiWave_Start, bof3::addr::TsunamiWave_Advance,
                                                bof3::addr::TsunamiWave_Grow, bof3::addr::TsunamiWave_Shrink,
                                                bof3::addr::TsunamiWave_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("TsunamiWave_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    if (Owner()[0xB] == 0xFF) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::TsunamiWave_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B5B60: the wave at the kind-2 point + (-0x12, 0x88) << 16, the
// owner's height; the angle +0xB 0x1A, +9 0x10, the width +0x10 0, the
// amplitude +0x14 0xF0; +2 on.
S14_EXPORT void __cdecl TsunamiWave_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(U(Field_Kind2X) + 0xFFEE0000u));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(U(Field_Kind2Z) + 0x880000u));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0x1A;
    Sc()[9] = 0x10;
    rv::Current<records::BattleTask>()->step_z = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0xF0);
    Inc(Sc()[2]);
}

// original 0x4B5BD0: z down by 0xC000; +9 down; at 0 +2 on.
S14_EXPORT void __cdecl TsunamiWave_Advance(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->z = static_cast<std::int32_t>(U(ts->z) + 0xFFFF4000u);
    }
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4B5C00: the angle down (mod 0x20), the width up by 2; at 0x80 +2
// on.
S14_EXPORT void __cdecl TsunamiWave_Grow(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x1F);
    ts->step_z = static_cast<std::int32_t>(U(ts->step_z) + 2);
    if (ts->step_z == 0x80) Inc(s[2]);
}

// original 0x4B5C30: the angle down (mod 0x20), the amplitude and the width
// down by 2; at a width of 0x20 +2 on.
S14_EXPORT void __cdecl TsunamiWave_Shrink(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x1F);
    rv::Store(ts->step_y, static_cast<std::int32_t>(U(ts->step_y) - 2));
    ts->step_z = static_cast<std::int32_t>(U(ts->step_z) - 2);
    if (ts->step_z == 0x20) Inc(s[2]);
}

// original 0x4B5C70: the amplitude down by 2, the width by 1; at a width of 0
// the owner's +0xB 0xFF (Leech_WaitOrbs's signal) and the task freed.
S14_EXPORT void __cdecl TsunamiWave_End(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    rv::Store(ts->step_y, static_cast<std::int32_t>(U(ts->step_y) - 2));
    ts->step_z = static_cast<std::int32_t>(U(ts->step_z) - 1);
    if (ts->step_z != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B5CB0: the wave - 127 semi-transparent textured quads (tpage
// 0x340 / 0x100, CLUT row 0x1FA) up a sine of amplitude +0x14 whose phase is
// (+0xB + i) & 0x1F, each 0x1000 wide, stepping back in y by (0x80 + width / 2)
// a quad; shaded 0x80 (at step 4 width / 2 x 8), the texture's v stepping with
// (+0xB + i) & 7; then, from edge 0x7E again, two gouraud quads (edges 0x7F
// and 0x80) shaded 0xC0 (at step 4 width / 2 x 12), the last bright at its far
// edge, the one before at its near; between two draw-mode packets on layer 2.
S14_EXPORT void __cdecl TsunamiWave_Draw(void) {
    DrawModeCommit(0xB5, 2);
    {
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
        const unsigned angle = (s[0xB] & 0x1Fu) << 7;
        SetVW(0, 0);
        SetSW(6, angle);
        SetVW(2, 0);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    int i = 1;
    for (; i < 0x80; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        WaveEdge(i, 8, 0x80);
        p[4] = SB(2);
        p[5] = SB(2);
        p[6] = SB(2);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        SetSW(8, (0xEu - ((static_cast<unsigned>(Sc()[0xB]) + static_cast<unsigned>(i)) & 7u)) << 4);
        p[0x14] = 0;
        p[0x24] = 0xF8;
        p[0x15] = SB(8);
        p[0x25] = SB(8);
        p[0x34] = 0;
        p[0x44] = 0xF8;
        p[0x35] = static_cast<unsigned char>(SB(8) + 0x10);
        p[0x45] = static_cast<unsigned char>(SB(8) + 0x10);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
    i -= 2;
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        SetVW(0, 0);
        const unsigned angle = ((static_cast<unsigned>(s[0xB]) + static_cast<unsigned>(i)) & 0x1Fu) << 7;
        SetSW(6, angle);
        const int half = ts->step_z >> 1;
        SetVW(2, static_cast<unsigned>(-0x80 - half) * static_cast<unsigned>(i));
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        ++i;
        SetVW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (; i < 0x81; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        WaveEdge(i, 12, 0xC0);
        if (i == 0x80) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(2);
        } else {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = SB(2);
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        }
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    DrawModeCommit(0x95, 2);
}

// original 0x4B6160: the rings, a jmp through TsunamiRings_Steps (three
// entries: _Start, _Grow, _End) by +2, unchecked.
S14_EXPORT void __cdecl TsunamiRings_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::TsunamiRings_Start, bof3::addr::TsunamiRings_Grow,
                                                bof3::addr::TsunamiRings_End};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("TsunamiRings_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B6180: the four ring phases (0x68478C) 0, +9 0, +2 on.
S14_EXPORT void __cdecl TsunamiRings_Start(void) {
    unsigned char* const s = Sc();
    SetLong(Mem(::bof3::Img(TsunamiRing_Phases_addr)), 0);
    s[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4B61A0: the rings drawn; +9 up; at 0x60 +2 on.
S14_EXPORT void __cdecl TsunamiRings_Grow(void) {
    Call0(bof3::addr::TsunamiRings_Draw);
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0x60) Inc(s[2]);
}

// original 0x4B61D0: the rings drawn; +9 up; at 0x80 the task freed.
S14_EXPORT void __cdecl TsunamiRings_End(void) {
    Call0(bof3::addr::TsunamiRings_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x80) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B6200: a draw-mode packet (tpage 0x35) on layer 3; for each of
// the three heights (0x65AC54, 12 bytes each) a ring round every enemy 0..7
// and then every party member 0..2 that Battle_ActorIsOut says is not out;
// each ring phase whose index x 3 is below +9 one on (and, at step 1, kept
// below 0x10); a draw-mode packet (tpage 0x15) on layer 3.
S14_EXPORT void __cdecl TsunamiRings_Draw(void) {
    DrawModeCommit(0x35, 3);
    for (unsigned height = 0; height < 3; ++height)
        for (unsigned e = 0; e < 8; ++e) {
            if (MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) continue;
            RingAt(height, e, Mem(::bof3::Img(Battle_Enemies_addr) + e * at::kEnemyStride));
        }
    for (unsigned height = 0; height < 3; ++height)
        for (unsigned m = 0; m < 3; ++m) {
            if (MH_CALL(Battle_ActorIsOut)(m) & 0xFF) continue;
            RingAt(height, m, PartyRecord(m));
        }
    {
        const unsigned char* const s = Sc();
        for (unsigned j = 0, c = 0; c < 0xC; c += 3, ++j) {
            unsigned char& phase = Mem(::bof3::Img(TsunamiRing_Phases_addr) + j)[0];
            if (c < s[9]) Inc(phase);
            if (s[2] == 1) phase &= 0xF;
        }
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B63C0: the ring's quads: for each ring phase j (0..3) whose index
// x 3 is below +9 and whose phase b is below 0x10, one semi-transparent
// textured quad (tpage 0x340 / 0x100, CLUT row 0x1E2) at the angles 0x200,
// 0x600, 0xE00, 0xA00 of radius b x 4 + 0x32, at depth -(b x 15), shaded 0x41
// - (b >> 1) x 4, on layer 3.
S14_EXPORT void __cdecl TsunamiRing_DrawQuads(void) {
    for (unsigned j = 0, c = 0; c < 0xC; c += 3, ++j) {
        if (static_cast<int>(c) >= static_cast<int>(Sc()[9])) continue;
        if (Mem(::bof3::Img(TsunamiRing_Phases_addr) + j)[0] >= 0x10) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned b = Mem(::bof3::Img(TsunamiRing_Phases_addr) + j)[0];
            SetSW(0, b * 4 + 0x32);
            SetSW(4, b * 15);
        }
        static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
        for (unsigned v = 0; v < 4; ++v) {
            int t = MH_CALL(Math_Cos)(kAngles[v]);
            SetVW(8 * v, static_cast<unsigned>(Mul12(t, SS(0))));
            t = MH_CALL(Math_Sin)(kAngles[v]);
            SetVW(8 * v + 2, static_cast<unsigned>(Mul12(t, SS(0))));
            SetVW(8 * v + 4, static_cast<unsigned>(-static_cast<int>(SW(4))));
        }
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2));
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0x20;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0x20;
        p[0x44] = 0x20;
        p[0x45] = 0x20;
        const unsigned shade = (0x41u - ((static_cast<unsigned>(Mem(::bof3::Img(TsunamiRing_Phases_addr) + j)[0]) >> 1) << 2)) & 0xFFFF;
        SetSW(2, shade);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = SB(2);
        p[6] = SB(2);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(3, 0x48);
    }
}

void TsunamiWave_Inject() {
    BOF3_INJECT(Tsunami_Task);
    BOF3_INJECT(Tsunami_Start);
    BOF3_INJECT(Tsunami_Rings);
    BOF3_INJECT(TsunamiChild_Task);
    BOF3_INJECT(TsunamiWave_Run);
    BOF3_INJECT(TsunamiWave_Start);
    BOF3_INJECT(TsunamiWave_Advance);
    BOF3_INJECT(TsunamiWave_Grow);
    BOF3_INJECT(TsunamiWave_Shrink);
    BOF3_INJECT(TsunamiWave_End);
    BOF3_INJECT(TsunamiWave_Draw);
    BOF3_INJECT(TsunamiRings_Run);
    BOF3_INJECT(TsunamiRings_Start);
    BOF3_INJECT(TsunamiRings_Grow);
    BOF3_INJECT(TsunamiRings_End);
    BOF3_INJECT(TsunamiRings_Draw);
    BOF3_INJECT(TsunamiRing_DrawQuads);
}
