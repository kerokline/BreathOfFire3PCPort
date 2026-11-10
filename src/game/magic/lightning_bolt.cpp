// Moved here by tools/move_funcs.py from magic_s22.cpp (docs/source-layout-round.md).
#include "game/magic/lightning_bolt.h"

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

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

void AddVW(unsigned k, unsigned v) { SetVW(k, VW(k) + v); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

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

// The actor-matrix push of 0x4C91E0 / 0x4CAE30 (MagicFx_PushActorMatrix's
// shape with a rotation): Camera_Matrix x the task's, translation RotTrans of
// (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)), rotation `rot`. One MATRIX
// block as the original lays it out on its stack, RotTrans writing its
// translation at +0x14. The rotation is the task's tilt (+0x18, +0x1C as
// shorts, 0) or (0, 0, 0x200); the task is read after the push.
void PushTaskMatrix(bool tilt) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const short rot[4] = {tilt ? S16(s + 0x18) : short{0}, tilt ? S16(s + 0x1C) : short{0}, tilt ? short{0} : short{0x200},
                          0};
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
    S22_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// Jolt_Start / Lightning_Start: +0xB 0; a bolt (kind 1, `parameter`) on every
// live enemy (target bit 6) or party member, its +1 0, +3 the index, its
// position the actor's, +9 its order x 10 + 1, +0x80 this task; the task to
// the last bolt's position (slot 0's when none was made: the slot index starts
// at 0 and is not checked); sound 0x100; +1 on.
void StartBolts(unsigned parameter) {
    Sc()[0xB] = 0;
    unsigned slot = 0;
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned n = enemies ? 8 : 3;
    for (unsigned i = 0; i < n; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        slot = NewTask(parameter);
        unsigned char* const self = Sc();
        const unsigned char* const r = enemies ? EnemyRecord(i) : PartyRecord(i);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        const std::int32_t x = Long(r + 0x34), z = Long(r + 0x38);
        tchild->owner = static_cast<std::int32_t>(Low32(self));
        child[1] = 0;
        child[3] = static_cast<unsigned char>(i);
        tchild->x = x;
        tchild->z = z;
        rv::Store(tchild->height, Long(r + 0x3C));
        child[9] = static_cast<unsigned char>(self[0xB] * 10 + 1);
        Inc(self[0xB]);
    }
    const unsigned char* const last = TaskSlot(slot);
    const records::BattleTask* const tlast = rv::AsConst<records::BattleTask>(last);
    rv::Store(rv::Current<records::BattleTask>()->x, tlast->x);
    rv::Store(rv::Current<records::BattleTask>()->z, tlast->z);
    rv::Store(rv::Current<records::BattleTask>()->height, tlast->height);
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sc()[1]);
}

// JoltBolt_Rise / LightningBolt_Rise once +0xA reaches 0x10: the target's
// tint released and set (-8, -8, -8, 1), its flags 0x10, +9 8, +2 on.
void DarkenTarget(unsigned char* s) {
    if (TargetByte() & 0x40) {
        unsigned char* const r = EnemyRecord(s[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(static_cast<unsigned char>(Sc()[3] + 3), 0x10);
    } else {
        unsigned char* const r = PartyRecord(s[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(Sc()[3], 0x10);
    }
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// The four quads of one row of a bolt's band share their shading: each of the
// twelve colour bytes (+4..+6, +0x14..+0x16, +0x24..+0x26, +0x34..+0x36) is 1,
// SharedScratch +4's low byte or +6's. The first row's top edge (+0x24..,
// +0x34..) is 1.
enum : char { k1 = '1', kA = 'a', kB = 'b' };

void Shade(unsigned char* p, const char (&pattern)[13], bool first_row) {
    static constexpr unsigned kAt[12] = {4, 5, 6, 0x14, 0x15, 0x16, 0x24, 0x25, 0x26, 0x34, 0x35, 0x36};
    for (unsigned k = 0; k < 12; ++k) p[kAt[k]] = pattern[k] == k1 ? 1 : pattern[k] == kA ? SB(4) : SB(6);
    if (first_row)
        for (unsigned k = 6; k < 12; ++k) p[kAt[k]] = 1;
}

// One quad of the band: POLY_G4, semi-transparent, shaded, projected, sorted
// at the task.
void BandQuad(const char (&pattern)[13], bool first_row) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    Shade(p, pattern, first_row);
    Rtp4(p);
    MH_CALL(Gte_PrimDepths4_10B)(p);
    LinkAtSprite(0x44);
}

constexpr char kRow1[13] = "aaabbaaaabba";

constexpr char kRow2[13] = "bba111bba111";

constexpr char kRow3[13] = "bbaaaabbaaaa";

constexpr char kRow4[13] = "111bba111bba";

// The band's start, both spells: SharedScratch +0 the radius `radius`, +2 the
// angle (+0xB & mask) << shift, +4 +0xA x 15 and +6 +0xA x 2 (the shades), +0xC
// `height` - Rand & 3; the first top point (0, sin x radius >> 12, 0); tpage
// 0x35 sorted at the task.
void BandStart(int height, int radius, unsigned mask, unsigned shift) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(radius));
        SetSW(2, static_cast<unsigned>((s[0xB] & mask) << shift));
        SetSW(4, s[0xA] * 15u);
        SetSW(6, s[0xA] * 2u);
    }
    const std::uint32_t r = RandCall();
    SetSW(0xC, static_cast<std::uint32_t>(height) - (r & 3));
    const int v = MH_CALL(Math_Sin)(SS(2));
    unsigned char* const p = Gfx_PacketNext;
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(4, 0);
    MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
}

// A row's radius: `radius` plus or minus Rand & `jitter`, by a coin.
void BandRadius(int radius, int jitter) {
    std::uint32_t r = RandCall();
    if (r & 1) {
        r = RandCall();
        SetSW(0, (r & static_cast<std::uint32_t>(jitter)) + static_cast<std::uint32_t>(radius));
    } else {
        r = RandCall();
        SetSW(0, static_cast<std::uint32_t>(radius) - (r & static_cast<std::uint32_t>(jitter)));
    }
}

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

}  // namespace

// original 0x4CAA70: the kind-2 task. Two entries by +1: Lightning_Start and
// MAGIC131's 0x4E5200.
S22_EXPORT void __cdecl Lightning_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Lightning_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Lightning_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4CAAA0: a bolt of kind-1 task 0x0E on every live target.
S22_EXPORT void __cdecl Lightning_Start(void) { StartBolts(0x0E); }

// original 0x4CAC40: the bolt's kind-1 task, a jmp through
// LightningBolt_TaskTable (one entry) by +1, unchecked.
S22_EXPORT void __cdecl LightningBolt_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("LightningBolt_Task", phase, 1);
    magic_harness::Phase(bof3::addr::LightningBolt_Run)();
}

// original 0x4CAC60: a call through LightningBolt_Steps (four entries) by +2 -
// LightningBolt_Wait, LightningBolt_Rise, JoltBolt_Fade, JoltBolt_End -, the
// screen point; then while +2 and +0 are set, under the bolt's matrix, three
// band rows ((0x18, 0x20, 3), (0x10, 0x50, 0xF), (0x10, 0x80, 0x1F), the angle
// stepping by 8, 2 and 6), one arc, and two flashes 24 pixels either side.
S22_EXPORT void __cdecl LightningBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::LightningBolt_Wait, bof3::addr::LightningBolt_Rise,
                                                bof3::addr::JoltBolt_Fade, bof3::addr::JoltBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("LightningBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call3(bof3::addr::LightningBolt_DrawBand, 0x18, 0x20, 3);
    AddB(Sc()[0xB], 8);
    Call3(bof3::addr::LightningBolt_DrawBand, 0x10, 0x50, 0xF);
    AddB(Sc()[0xB], 2);
    Call0(bof3::addr::LightningBolt_DrawArcs);
    AddB(Sc()[0xB], 6);
    Call3(bof3::addr::LightningBolt_DrawBand, 0x10, 0x80, 0x1F);
    AddB(Sc()[0xB], 0xF0);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + 0x18u);
    Call0(bof3::addr::LightningBolt_DrawFlash);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) - 0x30u);
    Call0(bof3::addr::LightningBolt_DrawFlash);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CAD10 (Jolt's too): +9 down; at 0 +0xB Rand & 0xF, +9 and +0xA
// 0, +2 on.
S22_EXPORT void __cdecl LightningBolt_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4CAD60: +0xA up by 4, +0xB up; at 0x10 the target darkened.
S22_EXPORT void __cdecl LightningBolt_Rise(void) {
    AddB(Sc()[0xA], 4);
    Inc(Sc()[0xB]);
    unsigned char* const s = Sc();
    if (s[0xA] != 0x10) return;
    DarkenTarget(s);
}

// original 0x4CAE30 (Jolt's and Myollnir's too): the bolt's matrix pushed,
// turned (0, 0, 0x200).
S22_EXPORT void __cdecl LightningBolt_PushMatrix(void) { PushTaskMatrix(false); }

// original 0x4CAEE0: one row of Lightning's band - Jolt's with the angle
// ((+0xB + i) & 0x1F) << 7 and the quads' corners stepped twice as far on the
// first and third quads.
S22_EXPORT void __cdecl LightningBolt_DrawBand(int a1, int a2, int a3) {
    BandStart(a1, a2, 0x1F, 7);
    for (int i = 1; i < 0x12; ++i) {
        SetSW(2, static_cast<unsigned>(((Sc()[0xB] + static_cast<unsigned char>(i)) & 0x1F) << 7));
        BandRadius(a2, a3);
        SetSW(0xA, SW(0xC));
        const std::uint32_t r = RandCall();
        const std::uint16_t d2 = static_cast<std::uint16_t>(SW(0xA) << 1);
        const std::uint32_t in = static_cast<std::uint32_t>(a1) - (r & 3);
        SetVW(0x10, 0);
        std::uint16_t top = VW(2);
        SetVW(0x12, top);
        top = static_cast<std::uint16_t>(top - d2);
        SetVW(0x1A, top);
        SetSW(0xC, in);
        const int angle = SS(2);
        const std::uint16_t h = VW(4);
        SetVW(0x14, h);
        SetVW(0x18, 0);
        SetVW(0x1C, h);
        SetVW(0, 0);
        const int v = MH_CALL(Math_Sin)(angle);
        const int y = Mul12(v, SS(0));
        const std::uint32_t in2 = static_cast<std::uint32_t>(SD(0xC)) * 2;
        SetVW(2, static_cast<unsigned>(y));
        const unsigned up = (0u - static_cast<unsigned>(i)) << 6;
        SetVW(4, up);
        SetVW(8, 0);
        SetVW(0xA, static_cast<unsigned>(y) - in2);
        SetVW(0xC, up);
        BandQuad(kRow1, i == 1);
        {
            AddVW(2, (0u - static_cast<std::uint32_t>(SD(0xC))) << 1);
            const std::uint16_t d = SW(0xA);
            AddVW(0xA, 0u - SW(0xC));
            AddVW(0x12, static_cast<unsigned>(d) * 0xFFFEu);
            AddVW(0x1A, 0u - d);
        }
        BandQuad(kRow2, i == 1);
        {
            const std::int32_t c = SD(0xC);
            const std::uint16_t d = SW(0xA);
            AddVW(2, static_cast<std::uint32_t>(c) * 4);
            AddVW(0xA, static_cast<std::uint32_t>(c) * 3);
            AddVW(0x1A, d * 3u);
            AddVW(0x12, d * 4u);
        }
        BandQuad(kRow3, i == 1);
        {
            const std::int32_t c = SD(0xC);
            AddVW(2, SW(0xC));
            AddVW(0xA, static_cast<std::uint32_t>(c) * 2);
            const std::uint16_t d = SW(0xA);
            AddVW(0x12, d);
            AddVW(0x1A, d * 2u);
        }
        BandQuad(kRow4, i == 1);
        AddVW(2, 0u - static_cast<std::uint32_t>(SD(0xC)) * 3);
    }
}

// original 0x4CB4F0: Lightning's arc - Jolt's zigzag with the swing 0xA0 plus
// or minus Rand & 0x1F, tpage 0x35 set once, and every line sorted at the
// task itself.
S22_EXPORT void __cdecl LightningBolt_DrawArcs(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    SetSW(0, 0x80);
    {
        const unsigned char* const s = Sc();
        SetSW(2, ((s[0xB] + 2u) & 0xF) << 8);
        SetSW(8, s[0xA]);
    }
    std::uint32_t r = RandCall();
    SetVW(0, 0);
    SetSW(4, r & 0x3F);
    int v = MH_CALL(Math_Sin)(SS(2));
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(4, 0);
    if (SS(8) <= 1) return;
    for (int i = 1;;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        r = RandCall();
        if (r & 1) {
            r = RandCall();
            SetSW(0, (r & 0x1F) + 0xA0);
        } else {
            r = RandCall();
            SetSW(0, 0xA0 - (r & 0x1F));
        }
        const unsigned angle = ((Sc()[0xB] + static_cast<unsigned>(i) + 2) & 0xF) << 8;
        SetVW(0, 0);
        SetSW(2, angle);
        v = MH_CALL(Math_Sin)(static_cast<int>(angle));
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(4, (0u - static_cast<unsigned>(i)) << 6);
        Rtp1(0, p + 0x18);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x20));
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = 0x20;
        r = RandCall();
        SetSW(4, r & 0x3F);
        p[0x14] = static_cast<unsigned char>(r & 0x3F);
        p[0x16] = 0x20;
        p[0x15] = SB(4);
        LinkAtSprite(0x24);
        if (++i >= SS(8)) break;
    }
}

// original 0x4CB6E0: Lightning's flash, the centre (15, 15, 12) x +0xA.
S22_EXPORT void __cdecl LightningBolt_DrawFlash(void) { Flash(4, false, 15, 12); }

void LightningBolt_Inject() {
    BOF3_INJECT(Lightning_Task);
    BOF3_INJECT(Lightning_Start);
    BOF3_INJECT(LightningBolt_Task);
    BOF3_INJECT(LightningBolt_Run);
    BOF3_INJECT(LightningBolt_Wait);
    BOF3_INJECT(LightningBolt_Rise);
    BOF3_INJECT(LightningBolt_PushMatrix);
    BOF3_INJECT(LightningBolt_DrawBand);
    BOF3_INJECT(LightningBolt_DrawArcs);
    BOF3_INJECT(LightningBolt_DrawFlash);
}
