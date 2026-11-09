// Moved here by tools/move_funcs.py from magic_s22.cpp (docs/source-layout-round.md).
#include "game/magic/myollnir.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F; words or dwords by function) and the four
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

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

void AddVW(unsigned k, unsigned v) { SetVW(k, VW(k) + v); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

std::uint32_t MulU(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn3 = void (__cdecl*)(int, int, int);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call3(std::uint32_t address, int a, int b, int c) { MH_WORD(Fn3, address)(a, b, c); }

// The callees with the arguments the originals push. Gte_RotTransPers3 / 4
// get the depth and flag pointers the originals pass; ours reads the first.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S22_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    S22_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}

void Rtp4(unsigned char* prim) {
    long p, flag;
    S22_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S22_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The flashes of 0x4CA8C0 / 0x4CB6E0 / 0x4CC7D0: eight gouraud triangles
// round the task's screen point (+0x2E / +0x30), radius Rand & 7 plus +0xA x
// `per`, kept in SharedScratch as a word or (`dword`) a dword and read back
// after every call; the centre (+0xA x m0, x m0, x m1) in 8 bits, the rim 1.
// Tpage 0x35, sorted at the task.
void Flash(unsigned per, bool dword, unsigned m0, unsigned m1) {
    const std::uint32_t r = RandCall();
    const std::uint32_t radius = (r & 7) + Sc()[0xA] * per;
    if (dword) SetSD(0, radius);
    else SetSW(0, radius);
    DrawMode(0x35);
    LinkAtSprite(0xC);
    const auto scale = [dword] { return dword ? static_cast<int>(SD(0)) : static_cast<int>(SS(0)); };
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Mul12(v, scale()) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Mul12(v, scale()) + S16(Sc() + 0x30));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Mul12(v, scale()) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Mul12(v, scale()) + S16(Sc() + 0x30));
        p[4] = static_cast<unsigned char>(Sc()[0xA] * m0);
        p[5] = static_cast<unsigned char>(Sc()[0xA] * m0);
        p[6] = static_cast<unsigned char>(Sc()[0xA] * m1);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAtSprite(0x34);
    }
}

// The children's circle round the owner: the angle (+0xC & 0x7F) << 5 and the
// radius in SharedScratch (dwords +4, +0), the position the owner's plus cos /
// sin x radius (no shift), each read back after the call before it.
void Orbit(std::uint32_t radius) {
    const std::uint32_t angle = (static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) & 0x7F) << 5;
    SetSD(0, radius);
    SetSD(4, angle);
    int v = MH_CALL(Math_Cos)(static_cast<int>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(MulU(v, SD(0)) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x)));
    v = MH_CALL(Math_Sin)(SD(4));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(MulU(v, SD(0)) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z)));
}

// A circling child's step: +0xB up, the angle +0xC up, the circle.
void OrbitStep(std::uint32_t radius) {
    Inc(Sc()[0xB]);
    rv::Store(rv::Current<records::BattleTask>()->step_x, rv::Current<records::BattleTask>()->step_x + 1);
    Orbit(radius);
}

}  // namespace

// original 0x4CB890: the kind-2 task. A five-entry stack table by +1:
// Myollnir_Start, Myollnir_Darken, Myollnir_WaitChildren, BattleFx_Brighten,
// Myollnir_End.
S22_EXPORT void __cdecl Myollnir_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Myollnir_Start, bof3::addr::Myollnir_Darken,
                                                 bof3::addr::Myollnir_WaitChildren, bof3::addr::BattleFx_Brighten,
                                                 bof3::addr::Myollnir_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Myollnir_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4CB8D0: the task to the source sprite (0x904B4C, read once): its
// direction byte and position; its tint released and set (0, 0, 0, 1), the
// tint's record index to +0xA; nine children of kind-1 task 0x0C - one of kind
// 0 (a bolt), four of kind 1 (orbs, +3 the index, +0xB the index x 4), four of
// kind 2 (rings, +3 the index) - each with +0x80 this task; sound 0x100; +9 8,
// +0xB 0, +1 on.
S22_EXPORT void __cdecl Myollnir_Start(void) {
    unsigned char* const src = Source();
    Sc()[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    const auto child = [](unsigned char kind) {
        unsigned char* const c = TaskSlot(NewTask(0xC));
        records::BattleTask* const tc = rv::As<records::BattleTask>(c);
        tc->owner = static_cast<std::int32_t>(Low32(Sc()));
        c[1] = kind;
        return c;
    };
    child(0);
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const c = child(1);
        c[3] = static_cast<unsigned char>(i);
        c[0xB] = static_cast<unsigned char>(i << 2);
    }
    for (unsigned i = 0; i < 4; ++i) child(2)[3] = static_cast<unsigned char>(i);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 8;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
}

// original 0x4CBA00: the tint record +0xA (MoveScript_TintRecords + 12 n,
// unbounded) one darker in each colour; +9 down; at 0 the target's flags 0x10
// and +1 on.
S22_EXPORT void __cdecl Myollnir_Darken(void) {
    unsigned char* const s = Sc();
    Dec(MoveScript_TintRecords[s[0xA] * 12u + 2]);
    Dec(MoveScript_TintRecords[s[0xA] * 12u + 3]);
    Dec(MoveScript_TintRecords[s[0xA] * 12u + 4]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4CBA80: once a child has finished (+0xB not 0), +9 8 and +1 on.
S22_EXPORT void __cdecl Myollnir_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 0) return;
    s[9] = 8;
    Inc(Sc()[1]);
}

// original 0x4CBAA0: once all nine children have finished (+0xB 9): the
// source's tint released, the target's flash and flag 0x40, the done flag, and
// the task freed.
S22_EXPORT void __cdecl Myollnir_End(void) {
    if (Sc()[0xB] != 9) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CBAE0: the children's kind-1 task, a jmp through
// MyollnirChild_Kinds (three entries) by +1, unchecked: the bolt, the orb, the
// ring.
S22_EXPORT void __cdecl MyollnirChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::MyollnirBolt_Run, bof3::addr::MyollnirOrb_Run,
                                                bof3::addr::MyollnirRing_Run};
    const unsigned kind = Sc()[1];
    if (kind >= 3) PastTable("MyollnirChild_Task", kind, 3);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4CBB00: a call through MyollnirBolt_Steps (four entries) by +2,
// the screen point; while +2 is set, a band row (0x60, 0x20, 7) under the
// bolt's matrix.
S22_EXPORT void __cdecl MyollnirBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MyollnirBolt_Start, bof3::addr::Myollnir_GrowHold,
                                                bof3::addr::MyollnirBolt_Hold, bof3::addr::MyollnirBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MyollnirBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    if (Sc()[2] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call3(bof3::addr::Myollnir_DrawBand, 0x60, 0x20, 7);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CBB40: +0xB Rand & 0xF, +9 and +0xA 0, the owner's position,
// +2 on.
S22_EXPORT void __cdecl MyollnirBolt_Start(void) {
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Inc(Sc()[2]);
}

// original 0x4CBBB0: +0xB up, +9 down; at 0 +2 on.
S22_EXPORT void __cdecl MyollnirBolt_Hold(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4CBBE0: +0xB up, +0xA down; at 0 the owner's count +0xB up and
// the task freed.
S22_EXPORT void __cdecl MyollnirBolt_End(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Inc(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CBC20: a call through MyollnirOrb_Steps (four entries) by +2,
// the screen point; while +0 is set, a band row (0x20, 0x50, 0xF) and a flash
// under the orb's matrix.
S22_EXPORT void __cdecl MyollnirOrb_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MyollnirOrb_Start, bof3::addr::Myollnir_GrowHold,
                                                bof3::addr::MyollnirOrb_Circle, bof3::addr::MyollnirOrb_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MyollnirOrb_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call3(bof3::addr::Myollnir_DrawBand, 0x20, 0x50, 0xF);
    Call0(bof3::addr::Myollnir_DrawFlash);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CBC60: +9 and +0xA 0; the angle +0xC the index x 32; the circle
// of radius 0x28 and the owner's height; +2 on.
S22_EXPORT void __cdecl MyollnirOrb_Start(void) {
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(Sc()[3] << 5));
    Orbit(0x28);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole);
    Inc(Sc()[2]);
}

// original 0x4CBD10 (the bolt's step 1 and the orb's): +0xB up; +0xA up by 2
// below 0x10, else +9 0x3C and +2 on.
S22_EXPORT void __cdecl Myollnir_GrowHold(void) {
    Inc(Sc()[0xB]);
    unsigned char* const s = Sc();
    if (s[0xA] < 0x10) {
        AddB(s[0xA], 2);
        return;
    }
    s[9] = 0x3C;
    Inc(Sc()[2]);
}

// original 0x4CBD40: a step round the circle of radius 0x28; +9 down, at 0 +2
// on.
S22_EXPORT void __cdecl MyollnirOrb_Circle(void) {
    OrbitStep(0x28);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4CBDE0: a step round the circle; +0xA down, at 0 the owner's
// count +0xB up and the task freed.
S22_EXPORT void __cdecl MyollnirOrb_End(void) {
    OrbitStep(0x28);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Inc(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CBE90: one row of Myollnir's band - seventeen steps of four
// gouraud quads as Jolt's, the radius s16 a2 plus or minus Rand & s16 a3 (a
// dword), the corners a1 in (no jitter), the angle (+0xB + i) & 0xF, the shades
// +0xA x 15 kept on the stack; a step whose first quad projects its third point
// at or above the screen's top (y <= 0.0, the float 0x5C41DC) draws that quad
// only and keeps its corners.
S22_EXPORT void __cdecl Myollnir_DrawBand(int a1, int a2, int a3) {
    const auto radius = static_cast<int>(static_cast<short>(a2));
    const auto jitter = static_cast<int>(static_cast<short>(a3));
    const auto in = static_cast<std::uint16_t>(a1);
    SetSD(0, 0x10);
    const unsigned char* const s0 = Sc();
    SetSD(4, (s0[0xB] & 0xFu) << 8);
    const std::uint32_t shade32 = s0[0xA] * 15u;
    SetSD(8, shade32);
    const auto shade = static_cast<unsigned char>(shade32);
    int v = MH_CALL(Math_Sin)(SD(4));
    unsigned char* p = Gfx_PacketNext;
    SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
    SetVW(4, 0);
    MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    for (int i = 1; i < 0x12; ++i) {
        SetSD(4, ((Sc()[0xB] + static_cast<unsigned>(i)) & 0xF) << 8);
        std::uint32_t r = RandCall();
        if (r & 1) {
            r = RandCall();
            SetSD(0, (r & static_cast<std::uint32_t>(jitter)) + static_cast<std::uint32_t>(radius));
        } else {
            r = RandCall();
            SetSD(0, static_cast<std::uint32_t>(radius) - (r & static_cast<std::uint32_t>(jitter)));
        }
        {
            const std::uint16_t top = VW(2), h = VW(4);
            SetVW(0x12, top);
            SetVW(0x1A, static_cast<unsigned>(top) - in);
            SetVW(0x10, 0);
            SetVW(0x14, h);
            SetVW(0x18, 0);
            SetVW(0x1C, h);
            SetVW(0, 0);
        }
        v = MH_CALL(Math_Sin)(SD(4));
        const int y = Mul12(v, SD(0));
        SetVW(8, 0);
        p = Gfx_PacketNext;
        SetVW(2, static_cast<unsigned>(y));
        const unsigned up = (0u - static_cast<unsigned>(i)) << 6;
        SetVW(4, up);
        SetVW(0xA, static_cast<unsigned>(y) - in);
        SetVW(0xC, up);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = SB(8);
        p[5] = SB(8);
        p[6] = SB(8);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = shade;
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x24] = SB(8);
            p[0x25] = SB(8);
            p[0x26] = SB(8);
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = shade;
        }
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        {
            float y2;
            std::memcpy(&y2, p + 0x2C, sizeof y2);
            if (0.0f >= y2) continue;   // fcomp 0x5C41DC (0.0): a NaN draws on
        }
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, 0u - in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned k : {4u, 5u}) p[k] = 1;
        p[6] = shade;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u}) p[k] = 1;
        p[0x26] = i == 1 ? 1 : shade;
        for (unsigned k : {0x34u, 0x35u, 0x36u}) p[k] = 1;
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        const auto in2 = static_cast<std::uint16_t>(static_cast<std::uint32_t>(a1) * 2);
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in2);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = shade;
        p[0x14] = SB(8);
        p[0x15] = SB(8);
        p[0x16] = SB(8);
        p[0x24] = 1;
        p[0x25] = 1;
        if (i == 1) {
            for (unsigned k : {0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x26] = shade;
            p[0x34] = SB(8);
            p[0x35] = SB(8);
            p[0x36] = SB(8);
        }
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u}) p[k] = 1;
        p[0x16] = shade;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u}) p[k] = 1;
        p[0x36] = i == 1 ? 1 : shade;
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        AddVW(2, 0u - in2);
    }
}

// original 0x4CC360: a call through MyollnirRing_Steps (four entries) by +2,
// the screen point; while +2 is set, the ring's arcs and flash under its
// matrix.
S22_EXPORT void __cdecl MyollnirRing_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MyollnirRing_Start, bof3::addr::MyollnirRing_Grow,
                                                bof3::addr::MyollnirRing_Circle, bof3::addr::MyollnirRing_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MyollnirRing_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    if (Sc()[2] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call0(bof3::addr::MyollnirRing_DrawArcs);
    Call0(bof3::addr::Myollnir_DrawFlash);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CC3A0: +0xB Rand & 0xF, +9 and +0xA 0; the angle +0xC the index
// x 32 + 0x10; the circle of radius 0x18 and the owner's height; +2 on.
S22_EXPORT void __cdecl MyollnirRing_Start(void) {
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>((Sc()[3] << 5) + 0x10));
    Orbit(0x18);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole);
    Inc(Sc()[2]);
}

// original 0x4CC460: +0xA up by 2 below 0x10, else +9 0x3C and +2 on.
S22_EXPORT void __cdecl MyollnirRing_Grow(void) {
    unsigned char* const s = Sc();
    if (s[0xA] < 0x10) {
        AddB(s[0xA], 2);
        return;
    }
    s[9] = 0x3C;
    Inc(Sc()[2]);
}

// original 0x4CC480: a step round the circle of radius 0x10; +9 down, at 0 +2
// on.
S22_EXPORT void __cdecl MyollnirRing_Circle(void) {
    OrbitStep(0x10);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4CC520: a step round the circle of radius 0x18; +0xA down by 2,
// at 0 the owner's count +0xB up and the task freed.
S22_EXPORT void __cdecl MyollnirRing_End(void) {
    OrbitStep(0x18);
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Inc(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CC5D0: the ring's arcs - Jolt's zigzag with the scratch as
// dwords: swing 0x80 plus or minus Rand & 0x3F, the count +0xA read back each
// step.
S22_EXPORT void __cdecl MyollnirRing_DrawArcs(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, 0x40);
        SetSD(4, ((s[0xB] + 2u) & 0xF) << 8);
        SetSD(0xC, s[0xA]);
    }
    std::uint32_t r = RandCall();
    SetSD(8, r & 0x3F);
    SetVW(0, 0);
    int v = MH_CALL(Math_Sin)(SD(4));
    const int y0 = Mul12(v, SD(0));
    SetVW(4, 0);
    SetVW(2, static_cast<unsigned>(y0));
    if (SD(0xC) <= 1) return;
    for (int i = 1;;) {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const std::uint32_t x = static_cast<std::uint32_t>(VS(0) >> 3) + static_cast<std::uint32_t>(ts->x);
        const std::uint32_t z = static_cast<std::uint32_t>(VS(2) >> 3) + static_cast<std::uint32_t>(ts->z);
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0xB5, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        r = RandCall();
        if (r & 1) {
            r = RandCall();
            SetSD(0, (r & 0x3F) + 0x80);
        } else {
            r = RandCall();
            SetSD(0, 0x80 - (r & 0x3F));
        }
        const unsigned angle = ((Sc()[0xB] + static_cast<unsigned>(i) + 2) & 0xF) << 8;
        SetVW(0, 0);
        SetSD(4, angle);
        v = MH_CALL(Math_Sin)(static_cast<int>(angle));
        SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
        SetVW(4, (0u - static_cast<unsigned>(i)) << 6);
        Rtp1(0, p + 0x18);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x20));
        p[4] = SB(8);
        p[5] = SB(8);
        p[6] = 0x20;
        r = RandCall();
        SetSD(8, r & 0x3F);
        p[0x14] = static_cast<unsigned char>(r & 0x3F);
        p[0x15] = SB(8);
        p[0x16] = 0x20;
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x24);
        if (++i >= SD(0xC)) break;
    }
}

// original 0x4CC7D0: Myollnir's flash, radius Rand & 7 + +0xA x 3 (a dword),
// the centre (14, 14, 12) x +0xA.
S22_EXPORT void __cdecl Myollnir_DrawFlash(void) { Flash(3, true, 14, 12); }

void Myollnir_Inject() {
    BOF3_INJECT(Myollnir_Task);
    BOF3_INJECT(Myollnir_Start);
    BOF3_INJECT(Myollnir_Darken);
    BOF3_INJECT(Myollnir_WaitChildren);
    BOF3_INJECT(Myollnir_End);
    BOF3_INJECT(MyollnirChild_Task);
    BOF3_INJECT(MyollnirBolt_Run);
    BOF3_INJECT(MyollnirBolt_Start);
    BOF3_INJECT(MyollnirBolt_Hold);
    BOF3_INJECT(MyollnirBolt_End);
    BOF3_INJECT(MyollnirOrb_Run);
    BOF3_INJECT(MyollnirOrb_Start);
    BOF3_INJECT(Myollnir_GrowHold);
    BOF3_INJECT(MyollnirOrb_Circle);
    BOF3_INJECT(MyollnirOrb_End);
    BOF3_INJECT(Myollnir_DrawBand);
    BOF3_INJECT(MyollnirRing_Run);
    BOF3_INJECT(MyollnirRing_Start);
    BOF3_INJECT(MyollnirRing_Grow);
    BOF3_INJECT(MyollnirRing_Circle);
    BOF3_INJECT(MyollnirRing_End);
    BOF3_INJECT(MyollnirRing_DrawArcs);
    BOF3_INJECT(Myollnir_DrawFlash);
}
