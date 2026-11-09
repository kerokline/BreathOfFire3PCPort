// Moved here by tools/move_funcs.py from magic_s31.cpp (docs/source-layout-round.md).
#include "game/magic/thunder_clap_bolt.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s31.h"
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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

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

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

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

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers4 gets
// the depth and flag pointers the originals pass; ours reads the first.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S31_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    S31_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0x10 a gouraud or flat-textured quad's, 0x14 a gouraud-textured one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S31_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

}  // namespace

#define S31_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E8660: the kind-2 task. Two entries by +1: ThunderClap_Start
// and MAGIC131's 0x4E5200 (the done flag and free once +0xB is 0).
S31_EXPORT void __cdecl ThunderClap_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::ThunderClap_Start,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ThunderClap_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E8690: one bolt (kind 1, 0x35) at the source sprite's position
// (0x904B4C, read before the task is made), +0x80 this task, +1 0; +0xB 1;
// sound 0x100; +1 on.
S31_EXPORT void __cdecl ThunderClap_Start(void) {
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    const unsigned slot = NewTask(0x35);
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    unsigned char* const s = Sc();
    tt->owner = static_cast<std::int32_t>(Key(s));
    t[1] = 0;
    tt->x = rv::As<records::SpriteHead>(source)->x;
    tt->z = rv::As<records::SpriteHead>(source)->z;
    rv::Store(tt->height, rv::As<records::SpriteHead>(source)->height);
    s[0xB] = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sc()[1]);
}

// original 0x4E8700: the bolt's kind-1 task, a jmp through
// ThunderClapBolt_TaskTable (one entry) by +1, unchecked.
S31_EXPORT void __cdecl ThunderClapBolt_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("ThunderClapBolt_Task", phase, 1);
    magic_harness::Phase(bof3::addr::ThunderClapBolt_Run)();
}

// original 0x4E8720: a call through ThunderClapBolt_Steps (four entries:
// _Start, _Rise, _Fade, group S22's JoltBolt_End) by +2, the screen point;
// then while +2 and +0 are set, under Lightning's bolt matrix, three band rows
// ((0x30, 0x10, 3), (0x20, 0x50, 0xF), (0x18, 0x80, 0x1F), the angle stepping
// by 4, 2 and 4), one arc, and two flashes 16 pixels either side.
S31_EXPORT void __cdecl ThunderClapBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ThunderClapBolt_Start, bof3::addr::ThunderClapBolt_Rise,
                                                bof3::addr::ThunderClapBolt_Fade, bof3::addr::JoltBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ThunderClapBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call3(bof3::addr::ThunderClapBolt_DrawBand, 0x30, 0x10, 3);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::ThunderClapBolt_DrawBand, 0x20, 0x50, 0xF);
    AddB(Sc()[0xB], 2);
    Call0(bof3::addr::ThunderClapBolt_DrawArcs);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::ThunderClapBolt_DrawBand, 0x18, 0x80, 0x1F);
    AddB(Sc()[0xB], 0xF6);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + 0x10u);
    Call0(bof3::addr::ThunderClapBolt_DrawFlash);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) - 0x20u);
    Call0(bof3::addr::ThunderClapBolt_DrawFlash);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E87D0: +0xB Rand & 0xF, +9 and +0xA 0, +2 on.
S31_EXPORT void __cdecl ThunderClapBolt_Start(void) {
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4E8800: +0xA up by 4, +0xB up; at 0x10 the source sprite's tint
// released and set (-8, -8, -8, 1), the target flags 0x10, +9 0x20, +2 on.
S31_EXPORT void __cdecl ThunderClapBolt_Rise(void) {
    AddB(Sc()[0xA], 4);
    Inc(Sc()[0xB]);
    if (Sc()[0xA] != 0x10) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(Sprite_SetTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0xF8, 0xF8, 0xF8, 1);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4E8870: +0xB up, +9 down; at 0 the target flagged 0x40, the source
// sprite's tint released, the target flashed, +2 on.
S31_EXPORT void __cdecl ThunderClapBolt_Fade(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[2]);
}

// original 0x4E88D0: one row of the bolt's band - 23 steps of four gouraud
// quads (Myollnir's, group S22, without its screen-top test): the radius s16
// a2 plus or minus Rand & s16 a3 (a dword; the first a2 / 2), the corners a1
// in, each step 0x30 higher, the angle ((+0xB + i) & 0xF) << 8; the shades
// +0xA x 8 and +0xA x 13 (Scratch_Swap, read back at every use).
S31_EXPORT void __cdecl ThunderClapBolt_DrawBand(int a1, int a2, int a3) {
    const auto radius = static_cast<int>(static_cast<short>(a2));
    const auto jitter = static_cast<int>(static_cast<short>(a3));
    const auto in = static_cast<std::uint16_t>(a1);
    SetSD(0, static_cast<std::uint32_t>(radius / 2));
    {
        const unsigned char* const s0 = Sc();
        SetSD(4, (s0[0xB] & 0xFu) << 8);
        SetSD(8, static_cast<std::uint32_t>(s0[0xA]) << 3);
        SetSD(0xC, s0[0xA] * 13u);
    }
    int v = MH_CALL(Math_Sin)(SD(4));
    unsigned char* p = Gfx_PacketNext;
    SetVW(2, static_cast<unsigned>(Mul12(v, SD(0))));
    SetVW(4, 0);
    MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    for (int i = 1; i < 0x18; ++i) {
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
        const unsigned up = static_cast<unsigned>(-(3 * i)) << 4;
        SetVW(2, static_cast<unsigned>(y));
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
        p[0x16] = SB(0xC);
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x24] = SB(8);
            p[0x25] = SB(8);
            p[0x26] = SB(8);
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = SB(0xC);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, 0u - in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = SB(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u}) p[k] = 1;
        p[0x26] = i == 1 ? 1 : SB(0xC);
        for (unsigned k : {0x34u, 0x35u, 0x36u}) p[k] = 1;
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        const auto in2 = static_cast<std::uint16_t>(static_cast<std::uint32_t>(a1) * 2);
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in2);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = SB(0xC);
        p[0x14] = SB(8);
        p[0x15] = SB(8);
        p[0x16] = SB(8);
        p[0x24] = 1;
        p[0x25] = 1;
        if (i == 1) {
            for (unsigned k : {0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x26] = SB(0xC);
            p[0x34] = SB(8);
            p[0x35] = SB(8);
            p[0x36] = SB(8);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u}) p[k] = 1;
        p[0x16] = SB(0xC);
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u}) p[k] = 1;
        p[0x36] = i == 1 ? 1 : SB(0xC);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        AddVW(2, 0u - in2);
    }
}

// original 0x4E8DA0: the bolt's arc - Myollnir's ring arcs (group S22) with
// the swing 0xA0 plus or minus Rand & 0x1F: +0xA - 1 gouraud lines up a
// zigzag, each sorted at the task's position plus the point / 8, the count
// +0xA read back each step.
S31_EXPORT void __cdecl ThunderClapBolt_DrawArcs(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, 0x80);
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
            SetSD(0, (r & 0x1F) + 0xA0);
        } else {
            r = RandCall();
            SetSD(0, 0xA0 - (r & 0x1F));
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

// original 0x4E8FA0: a flash - eight semi-transparent gouraud triangles round
// the task's screen point (+0x2E / +0x30), radius Rand & 7 + +0xA x 4 (a dword,
// read back after every call); the centre (+0xA x 8, x 8, x 6) in 8 bits, the
// rim 1; tpage 0x35, sorted at the task.
S31_EXPORT void __cdecl ThunderClapBolt_DrawFlash(void) {
    const std::uint32_t r = RandCall();
    SetSD(0, (r & 7) + Sc()[0xA] * 4u);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Mul12(v, SD(0)) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Mul12(v, SD(0)) + S16(Sc() + 0x30));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Mul12(v, SD(0)) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Mul12(v, SD(0)) + S16(Sc() + 0x30));
        p[4] = static_cast<unsigned char>(Sc()[0xA] << 3);
        p[5] = static_cast<unsigned char>(Sc()[0xA] << 3);
        p[6] = static_cast<unsigned char>(Sc()[0xA] * 6);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAtSprite(0x34);
    }
}

void ThunderClapBolt_Inject() {
    BOF3_INJECT(ThunderClap_Task);
    BOF3_INJECT(ThunderClap_Start);
    BOF3_INJECT(ThunderClapBolt_Task);
    BOF3_INJECT(ThunderClapBolt_Run);
    BOF3_INJECT(ThunderClapBolt_Start);
    BOF3_INJECT(ThunderClapBolt_Rise);
    BOF3_INJECT(ThunderClapBolt_Fade);
    BOF3_INJECT(ThunderClapBolt_DrawBand);
    BOF3_INJECT(ThunderClapBolt_DrawArcs);
    BOF3_INJECT(ThunderClapBolt_DrawFlash);
}
