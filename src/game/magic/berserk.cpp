// Moved here by tools/move_funcs.py from magic_s08.cpp (docs/source-layout-round.md).
#include "game/magic/berserk.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `cdq / and edx, 3 / add / sar 2`: a signed divide by 4 toward zero.
int Div4(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) + (static_cast<std::uint32_t>(v >> 31) & 3u)) >> 2; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* TintRecord(unsigned index) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + index * 12u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The task's position +0x34 / +0x38 / +0x3C from the owner's.
void OwnerPosition() {
    const unsigned char* const o = Owner();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = rv::As<records::SpriteHead>(o)->x;
    ts->z = rv::As<records::SpriteHead>(o)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(o)->height);
}

// The callees with the arguments the originals push: the projections get the
// depth and flag pointers (RotTransPers3 takes one of them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S08_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four vertex-scratch SVECTORs projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}

void Rtp3(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A6440: the kind-2 task. A six-entry stack table by +1:
// Berserk_Start, _Tint, _Brighten, _WaitChildren, _Fade, BattleFx_Finish.
S08_EXPORT void __cdecl Berserk_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Berserk_Start,         bof3::addr::Berserk_Tint,
                                                 bof3::addr::Berserk_Brighten,      bof3::addr::Berserk_WaitChildren,
                                                 bof3::addr::Berserk_Fade,          bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 6, Sc()[1], "Berserk_Task");
}

// original 0x4A6490: the owner's position; +0xB 0, +9 0xA, +0xA 0, +1 on;
// eight rings (kind 1, 0x3D; +1 0, +9 the delay 1, 9, .., 0x39) and a glow
// (+1 1), each owned by this task and counted in +0xB; sound 0x100.
S08_EXPORT void __cdecl Berserk_Start(void) {
    OwnerPosition();
    Sc()[0xB] = 0;
    Sc()[9] = 0xA;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    for (unsigned delay = 1; delay < 0x41; delay += 8) {
        const unsigned slot = NewTask(0x3D);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        child[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
    const unsigned slot = NewTask(0x3D);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 1;
    Inc(self[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A6580: +9 down; at 0 the source sprite (0x904B4C) tinted black,
// its tint slot to +9, +1 on.
S08_EXPORT void __cdecl Berserk_Tint(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[9] = tint;
    Inc(Sc()[1]);
}

// original 0x4A65D0: the tint record +9 brighter (+2 by 2, +3 and +4 by 1),
// +0xA up; at 8 +1 on.
S08_EXPORT void __cdecl Berserk_Brighten(void) {
    unsigned char* const s = Sc();
    AddB(TintRecord(s[9])[2], 2);
    AddB(TintRecord(s[9])[3], 1);
    AddB(TintRecord(s[9])[4], 1);
    Inc(s[0xA]);
    if (Sc()[0xA] == 8) Inc(Sc()[1]);
}

// original 0x4A6640: +1 on once one child is left (+0xB 1).
S08_EXPORT void __cdecl Berserk_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 1) Inc(s[1]);
}

// original 0x4A6650: the tint record darker by the same steps, +0xA down; at
// 0 the source's tint released, the target flashed, +1 on.
S08_EXPORT void __cdecl Berserk_Fade(void) {
    unsigned char* const s = Sc();
    AddB(TintRecord(s[9])[2], 0xFE);
    Dec(TintRecord(s[9])[3]);
    Dec(TintRecord(s[9])[4]);
    Dec(s[0xA]);
    if (Sc()[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x4A66E0: the children's kind-1 task, a jmp through
// BerserkChild_Kinds (two entries: the ring, the glow) by +1.
S08_EXPORT void __cdecl BerserkChild_Task(void) { CellCall(::bof3::Img(0x65A82C), 2, Sc()[1], "BerserkChild_Task"); }

// original 0x4A6700: a ring. A three-entry stack table by +2 (_Start, _Grow,
// _Fade); then while +0 bit 0 and +2: the actor matrix, the ring, the pop.
S08_EXPORT void __cdecl BerserkRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BerserkRing_Start, bof3::addr::BerserkRing_Grow,
                                                bof3::addr::BerserkRing_Fade};
    StackCall(kSteps, 3, Sc()[2], "BerserkRing_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BerserkRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A6750: +9 (the delay) down; at 0 the owner's position, +9 and
// +0xA 0, +2 on.
S08_EXPORT void __cdecl BerserkRing_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    OwnerPosition();
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A67C0: +0xA up by 4; at 0x20 +2 on.
S08_EXPORT void __cdecl BerserkRing_Grow(void) {
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4A67E0: +9 up by 4, +0xA down; at 0 the owner's +0xB down and
// the task freed.
S08_EXPORT void __cdecl BerserkRing_Fade(void) {
    AddB(Sc()[9], 4);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A6820: the glow. A four-entry stack table by +2 (_Start,
// MagicFx_CountUp9By2, BarrierLine_Wait, MAGIC060's 0x4B1740); then while +0
// bit 0 and +2: the actor matrix, the glow, the pop.
S08_EXPORT void __cdecl BerserkGlow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BerserkGlow_Start, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::BarrierLine_Wait, bof3::addr::MagicFx_CountDownRelease};
    StackCall(kSteps, 4, Sc()[2], "BerserkGlow_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BerserkGlow_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A6880: the owner's position, +9 0, +2 on.
S08_EXPORT void __cdecl BerserkGlow_Start(void) {
    OwnerPosition();
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4A68D0: a ring of 64 semi-transparent gouraud quads (tpage 0x35,
// layer 0): the lower edge radius 0x80 on the ground, the upper edge radius
// +9 - sin x 16 + 0xA0 at height +0xA x 8 + 0x20 - sin x 8, the angle of
// each ring (+9 + i + Rand & 3) & 0xF; bright (0xC0, 0x30, 0x30) below,
// dark above; linked at the task's x / z each moved by the lower x << 9.
S08_EXPORT void __cdecl BerserkRing_Draw(void) {
    std::uint32_t r = RandCall();
    unsigned char* s = Sc();
    SetSW(0, 0x80);
    const unsigned first = (((r & 3) + s[9]) & 0xF) << 8;
    SetSW(4, first);
    int v = MH_CALL(Math_Sin)(static_cast<int>(first));
    s = Sc();
    SetSW(2, s[9] - static_cast<unsigned>(Shl(v, 4) >> 12) + 0xA0);
    v = MH_CALL(Math_Sin)(SS(4));
    s = Sc();
    SetSW(8, s[0xA] * 8u + 0x20 - static_cast<unsigned>(Shl(v, 3) >> 12));
    v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0u - SW(8));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x14, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x1C, 0);
    for (unsigned i = 1; i < 0x41; ++i) {
        r = RandCall();
        s = Sc();
        unsigned angle = (((r & 3) + s[9] + i) & 0xF) << 8;
        if (i == 0x40) angle = first;
        SetSW(4, angle);
        v = MH_CALL(Math_Sin)(static_cast<int>(angle));
        s = Sc();
        SetSW(2, s[9] - static_cast<unsigned>(Shl(v, 4) >> 12) + 0xA0);
        v = MH_CALL(Math_Sin)(SS(4));
        s = Sc();
        SetSW(8, s[0xA] * 8u + 0x20 - static_cast<unsigned>(Shl(v, 3) >> 12));
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        SetVW(4, VW(0xC));
        const int around = static_cast<int>(i << 6);
        v = MH_CALL(Math_Cos)(around);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Sin)(around);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        SetVW(0xC, 0u - SW(8));
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        v = MH_CALL(Math_Cos)(around);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Sin)(around);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        s = Sc();
        const std::uint32_t lift = static_cast<std::uint32_t>(Shl(VS(0x18), 9));
        const std::uint32_t x = static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->x) + lift;
        const std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->z) + lift;
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 0xC0;
        p[0x25] = 0x30;
        p[0x26] = 0x30;
        p[0x34] = 0xC0;
        p[0x35] = 0x30;
        p[0x36] = 0x30;
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
}

// original 0x4A6C10: a glow of sixteen semi-transparent gouraud triangles
// (tpage 0x35, layer 5) round the centre, radius 0x88; the centre dark (1),
// the rim +9 x 8 in red and a quarter of it in green and blue.
S08_EXPORT void __cdecl BerserkGlow_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* const s = Sc();
    SetSW(0, 0x88);
    SetSW(6, s[9] * 8u);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(6);
        p[0x15] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x16] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x24] = SB(6);
        p[0x25] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x26] = static_cast<unsigned char>(Div4(SS(6)));
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void Berserk_Inject() {
    BOF3_INJECT(Berserk_Task);
    BOF3_INJECT(Berserk_Start);
    BOF3_INJECT(Berserk_Tint);
    BOF3_INJECT(Berserk_Brighten);
    BOF3_INJECT(Berserk_WaitChildren);
    BOF3_INJECT(Berserk_Fade);
    BOF3_INJECT(BerserkChild_Task);
    BOF3_INJECT(BerserkRing_Run);
    BOF3_INJECT(BerserkRing_Start);
    BOF3_INJECT(BerserkRing_Grow);
    BOF3_INJECT(BerserkRing_Fade);
    BOF3_INJECT(BerserkGlow_Run);
    BOF3_INJECT(BerserkGlow_Start);
    BOF3_INJECT(BerserkRing_Draw);
    BOF3_INJECT(BerserkGlow_Draw);
}
