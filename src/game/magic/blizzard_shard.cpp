// Moved here by tools/move_funcs.py from magic_s22.cpp (docs/source-layout-round.md).
#include "game/magic/blizzard_shard.h"

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

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S22_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp3(unsigned char* prim) {
    long p, flag;
    S22_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
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

// The PSX's SetPolyFT3 (POLY_FT3's code 0x24 at +7, the float 0.01 to +0x10,
// +0x20, +0x30) - Capcom's, unnamed, in no group.
constexpr ::bof3::image::CodeVa kSetPolyFT3{bof3::addr::Gpu_SetPolyFT3};

// Rotates the dx / dz pair +0xC / +0x10 of the task it is given by its
// direction byte +8 - Capcom's, unnamed, in no group (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using PrimFn = void (__cdecl*)(unsigned char*);

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

}  // namespace

// original 0x4C8D40: the kind-2 task. A three-entry stack table by +1:
// Blizzard_Start, Blizzard_Wait, BattleFx_Finish.
S22_EXPORT void __cdecl Blizzard_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Blizzard_Start, bof3::addr::Blizzard_Wait,
                                                 bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Blizzard_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C8D70: the task at the targets' centre, the owner's direction
// byte, +0xB 0, +9 8, +0xA 0, +1 on; sixteen shards (kind 1, 0x4C), each with
// +4 its index, +9 its delay BlizzardShard_Delays[i] + 1, +0x80 this task, and
// +0xB counting them; then row 26 of Gfx_ClutStrip back from its source,
// Gfx_ClutStripDirty, and sound 0x100.
S22_EXPORT void __cdecl Blizzard_Start(void) {
    Call0(bof3::addr::Blizzard_CenterOnTargets);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[8] = Owner()[8];
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned slot = NewTask(0x4C);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        const auto delay = static_cast<unsigned char>(BlizzardShard_Delays[i] + 1);
        tchild->owner = static_cast<std::int32_t>(Low32(self));
        child[9] = delay;
        child[4] = static_cast<unsigned char>(i);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C8E30: +9 down; at 0 the target flags 0x200 and +1 on.
S22_EXPORT void __cdecl Blizzard_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x200);
    Inc(Sc()[1]);
}

// original 0x4C8E70: the shard's kind-1 task, a jmp through
// BlizzardShard_TaskTable (one entry) by +1, unchecked.
S22_EXPORT void __cdecl BlizzardShard_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BlizzardShard_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BlizzardShard_Run)();
}

// original 0x4C8E90: a five-entry stack table by +2 - Launch, Grow, MAGIC086's
// count-down of +0xB, MAGIC130's of +9, End - then, while +0 and +2 are set,
// the shard's crystal under its own matrix and its fan and ring under the
// actor's.
S22_EXPORT void __cdecl BlizzardShard_Run(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::BlizzardShard_Launch, bof3::addr::BlizzardShard_Grow,
                                                 bof3::addr::BarrierRing_Hold, bof3::addr::MagicFx_CountDown9,
                                                 bof3::addr::BlizzardShard_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("BlizzardShard_Run", phase, 5);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::BlizzardShard_PushMatrix);
    Call0(bof3::addr::BlizzardShard_DrawCrystal);
    MH_CALL(Gte_PopMatrix)();
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BlizzardShard_DrawFan);
    Call0(bof3::addr::BlizzardShard_DrawRing);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C8F10: +9 down; at 0 the shard starts: the owner's direction
// byte; the offset pair BlizzardShard_Offsets[+4] to +0xC / +0x10, turned by
// the direction (0x446770); the position the owner's plus that offset, the
// owner's height; for directions 0..3 a random tilt (+0x18 / +0x1C, one of
// them 0x80.. or 0xE80.. by the shard's parity, the other -0x80..0x7F masked
// to 0xFFF, and one time in sixteen the first re-rolled to -0x80..0x7F); +0xB
// 0x1E - BlizzardShard_Delays[+4]; +9 and +0xA 0; +2 on. The index +4 is
// unbounded, as in the original.
S22_EXPORT void __cdecl BlizzardShard_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->step_x, Long(reinterpret_cast<const unsigned char*>(BlizzardShard_Offsets) + Sc()[4] * 8u));
    rv::Store(rv::Current<records::BattleTask>()->step_z, Long(reinterpret_cast<const unsigned char*>(BlizzardShard_Offsets) + Sc()[4] * 8u + 4));
    MH_AT(PrimFn, kTurnOffset.v)(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z) +
                                                   static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z)));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    const unsigned char* const s = Sc();
    const unsigned direction = s[8];
    if (direction <= 3) {
        const bool odd = (s[4] & 1) != 0;
        // Directions 0 and 2 tilt +0x18 first, 1 and 3 +0x1C; 0 and 1 take
        // 0x80.. for an odd shard, 2 and 3 for an even one.
        const bool low = (direction < 2) == odd;
        const unsigned first = (direction & 1) ? 0x1C : 0x18;
        const unsigned second = (direction & 1) ? 0x18 : 0x1C;
        std::uint32_t r = RandCall();
        SetLong(Sc() + first, static_cast<std::int32_t>((r & 0xFF) + (low ? 0x80u : 0xE80u)));
        r = RandCall();
        SetLong(Sc() + second, static_cast<std::int32_t>(((r & 0xFF) - 0x80u) & 0xFFF));
        r = RandCall();
        if ((r & 0xF) == 0) {
            r = RandCall();
            SetLong(Sc() + first, static_cast<std::int32_t>((r & 0xFF) - 0x80u));
        }
    }
    unsigned char* const t = Sc();
    t[0xB] = static_cast<unsigned char>(0x1E - BlizzardShard_Delays[t[4]]);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4C9170: +9 and +0xA up by two; +2 on once +9 is
// BlizzardShard_Sizes[+4].
S22_EXPORT void __cdecl BlizzardShard_Grow(void) {
    AddB(Sc()[9], 2);
    AddB(Sc()[0xA], 2);
    const unsigned char* const s = Sc();
    if (s[9] == BlizzardShard_Sizes[s[4]]) Inc(Sc()[2]);
}

// original 0x4C91B0: +0xA down; at 0 the owner's count +0xB down and the task
// freed.
S22_EXPORT void __cdecl BlizzardShard_End(void) {
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C91E0: the shard's matrix pushed, turned by its tilt (+0x18,
// +0x1C as shorts, 0).
S22_EXPORT void __cdecl BlizzardShard_PushMatrix(void) { PushTaskMatrix(true); }

// original 0x4C9290: the crystal - four facets round the shard (angles 0,
// 0x400, 0xC00: the one at 0x800 skipped), each a textured triangle from the
// tip (0, 0, -34 x size) to two points of radius 6 x size at height -24 x size
// and a textured quad from those down to radius 9 x size at 0; page (0x340,
// 0x100), CLUT (0, 0x1FA), u from the facet's index x 0x40, the triangle's
// shade (8 - index) x 16. Tpage 0x95; each primitive sorted at the shard's
// position moved by its first rim point << 9.
S22_EXPORT void __cdecl BlizzardShard_DrawCrystal(void) {
    {
        const unsigned size = Sc()[9];
        SetSW(4, size * 34);
        SetSW(6, size * 24);
        SetSW(0, size * 6);
        SetSW(2, size * 9);
    }
    unsigned count = 0;
    for (int angle = 0; angle < 0x1000; angle += 0x400, ++count) {
        if (angle == 0x800) continue;
        SetVW(4, 0u - static_cast<std::uint32_t>(SD(4)));
        SetVW(0, 0);
        SetVW(2, 0);
        int v = MH_CALL(Math_Sin)(angle);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(0xC, 0u - SW(6));
        v = MH_CALL(Math_Sin)(angle + 0x400);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(angle + 0x400);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(0x14, 0u - SW(6));
        unsigned char* p = Gfx_PacketNext;
        const unsigned char* s = Sc();
        std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(VS(8))) << 9) + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->x);
        std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(VS(0xA))) << 9) + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->z);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x95, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_AT(PrimFn, kSetPolyFT3.v)(p);
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 0, 0x340, 0x100));
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        const auto u = static_cast<unsigned char>(count << 6);
        SetWord(p + 0x16, clut);
        p[0x15] = 0x40;
        p[0x14] = static_cast<unsigned char>(u + 0x20);
        p[0x24] = u;
        p[0x25] = 0x60;
        p[0x34] = static_cast<unsigned char>(u + 0x3F);
        p[0x35] = 0x60;
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10)(p);
        {
            const std::uint32_t shade = (8u - count) << 4;
            SetSW(0xE, shade);
            p[4] = static_cast<unsigned char>(shade);
            p[5] = SB(0xE);
            p[6] = SB(0xE);
        }
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x38);
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        SetVW(4, VW(0xC));
        SetVW(8, VW(0x10));
        SetVW(0xA, VW(0x12));
        SetVW(0xC, VW(0x14));
        v = MH_CALL(Math_Sin)(angle);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(angle);
        SetVW(0x14, 0);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Sin)(angle + 0x400);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(angle + 0x400);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
        SetVW(0x1C, 0);
        s = Sc();
        x = (static_cast<std::uint32_t>(static_cast<int>(VS(0x10))) << 9) + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->x);
        z = (static_cast<std::uint32_t>(static_cast<int>(VS(0x12))) << 9) + static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->z);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x95, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 0, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        p[0x24] = static_cast<unsigned char>(u - 0x68);
        p[0x44] = static_cast<unsigned char>(u - 0x68);
        p[0x14] = u;
        p[0x15] = 0x60;
        p[0x25] = 0x60;
        p[0x34] = u;
        p[0x35] = 0x9F;
        p[0x45] = 0x9F;
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        p[4] = SB(0xE);
        p[5] = SB(0xE);
        p[6] = SB(0xE);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x48);
    }
}

// original 0x4C96A0: a fan of eight gouraud triangles round the actor matrix's
// origin, radius (+0xA + 1) x 16; the centre (7a, 7a, 8a) and the rim (9a, 7a,
// 9a) for a = +0xA. Tpage 0x35, sorted at the task.
S22_EXPORT void __cdecl BlizzardShard_DrawFan(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned a = Sc()[0xA];
        SetSW(0, (a + 1) << 4);
        SetSW(0xA, a << 3);
        SetSW(0xC, a * 9);
        SetSW(0xE, a * 7);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
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
        p[4] = SB(0xE);
        p[5] = SB(0xE);
        p[6] = SB(0xA);
        p[0x14] = SB(0xC);
        p[0x15] = SB(0xE);
        p[0x16] = SB(0xC);
        p[0x24] = SB(0xC);
        p[0x25] = SB(0xE);
        p[0x26] = SB(0xC);
        LinkAtSprite(0x34);
    }
}

// original 0x4C9890: a ring of eight gouraud quads round the actor matrix's
// origin, inner radius (+0xA + 8) x 16 dark, outer (+0xA + 1) x 16 in (9a, 7a,
// 9a). Tpage 0x35, sorted at the task.
S22_EXPORT void __cdecl BlizzardShard_DrawRing(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned a = Sc()[0xA];
        SetSW(0, (a + 1) << 4);
        SetSW(2, (a + 8) << 4);
        SetSW(0xC, a * 9);
        SetSW(0xE, a * 7);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        v = MH_CALL(Math_Sin)(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(a);
        {
            const std::uint16_t x = VW(0x18), y = VW(0x1A);
            SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xC);
        p[0x25] = SB(0xE);
        p[0x26] = SB(0xC);
        p[0x34] = SB(0xC);
        p[0x35] = SB(0xE);
        p[0x36] = SB(0xC);
        LinkAtSprite(0x44);
    }
}

// original 0x4C9AF0: the task to the centre of the live targets - every enemy
// when the target byte has bit 6, else every party member - their x and z
// (>> 9, less 0x4000) and heights summed in SharedScratch, the count at +0xC,
// each re-read from memory; +0x34 / +0x38 the averages (+ 0x4000) << 9, +0x3E
// the average height. Every actor out divides by zero in the original: ours
// aborts there (docs/magic_s22.md section 6).
S22_EXPORT void __cdecl Blizzard_CenterOnTargets(void) {
    const bool enemies = (TargetByte() & 0x40) != 0;
    SetSD(0xC, 0);
    SetSD(8, 0);
    SetSD(4, 0);
    SetSD(0, 0);
    const unsigned n = enemies ? 8 : 3;
    for (unsigned i = 0; i < n; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned char* const r = enemies ? EnemyRecord(i) : PartyRecord(i);
        SetSD(0, static_cast<std::uint32_t>(SD(0)) + static_cast<std::uint32_t>(Long(r + 0x34) >> 9) - 0x4000u);
        SetSD(4, static_cast<std::uint32_t>(SD(4)) + static_cast<std::uint32_t>(Long(r + 0x38) >> 9) - 0x4000u);
        SetSD(8, static_cast<std::uint32_t>(SD(8)) + static_cast<std::uint32_t>(static_cast<int>(S16(r + 0x3E))));
        SetSD(0xC, static_cast<std::uint32_t>(SD(0xC)) + 1);
    }
    if (SD(0xC) == 0)
        bof3::Fatal("Blizzard_CenterOnTargets: every %s is out - the original divides by zero here",
                    enemies ? "enemy" : "party member");
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(SD(0) / SD(0xC) + 0x4000) << 9));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(SD(4) / SD(0xC) + 0x4000) << 9));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(SD(8) / SD(0xC)) & 0xFFFF);
}

void BlizzardShard_Inject() {
    BOF3_INJECT(Blizzard_Task);
    BOF3_INJECT(Blizzard_Start);
    BOF3_INJECT(Blizzard_Wait);
    BOF3_INJECT(BlizzardShard_Task);
    BOF3_INJECT(BlizzardShard_Run);
    BOF3_INJECT(BlizzardShard_Launch);
    BOF3_INJECT(BlizzardShard_Grow);
    BOF3_INJECT(BlizzardShard_End);
    BOF3_INJECT(BlizzardShard_PushMatrix);
    BOF3_INJECT(BlizzardShard_DrawCrystal);
    BOF3_INJECT(BlizzardShard_DrawFan);
    BOF3_INJECT(BlizzardShard_DrawRing);
    BOF3_INJECT(Blizzard_CenterOnTargets);
}
