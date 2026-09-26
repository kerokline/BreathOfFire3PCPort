// Four spell overlays compiled into the exe, round nine group S22
// (docs/magic_s22.md): the PSX's MAGIC096..MAGIC099.EMI, Magic_Rows rows 102,
// 19, 37 and 36. Read one id down (docs/cut-content.md section 2) the
// sibling labels them Blizzard, Jolt, Lightning and Myollnir; the names below
// use those labels as hypotheses, and say what the code does.
//
//   - MAGIC096 0x4C8D40..0x4C9C53: a centre between the live targets, sixteen
//     falling shards (a kind-1 task each), a CLUT row restored;
//   - MAGIC097 0x4C9C60..0x4CAA66 and MAGIC098 0x4CAA70..0x4CB886: a bolt on
//     every live target (kind-1 tasks 0x14 and 0x0E), its band of quads, its
//     arcs of lines and its two flashes; Lightning's bolt shares Jolt's fade
//     and end, Jolt's shares Lightning's wait and matrix;
//   - MAGIC099 0x4CB890..0x4CC96E: the source sprite darkened, nine children
//     of kind-1 task 0x0C in three kinds - a bolt, four orbs, four rings.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence:
// each is a faithful replacement, except that a phase past a task's table
// aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent), and the targets' centre
// aborts where the original divides by zero (every actor on the side out).
#include "game/magic_s22.h"

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
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F; words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
void AddVW(unsigned k, unsigned v) { SetVW(k, VW(k) + v); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
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

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* EnemyRecord(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn3 = void (__cdecl*)(int, int, int);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call3(std::uint32_t address, int a, int b, int c) { MH_AT(Fn3, address)(a, b, c); }

// The callees with the arguments the originals push. Gte_RotTransPers3 / 4
// get the depth and flag pointers the originals pass; ours reads the first.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S22_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    S22_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}
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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The PSX's SetPolyFT3 (POLY_FT3's code 0x24 at +7, the float 0.01 to +0x10,
// +0x20, +0x30) - Capcom's, unnamed, in no group.
constexpr std::uint32_t kSetPolyFT3 = 0x5A7590;
// Rotates the dx / dz pair +0xC / +0x10 of the task it is given by its
// direction byte +8 - Capcom's, unnamed, in no group (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using PrimFn = void (__cdecl*)(unsigned char*);

// The phase handlers of other units a stack table holds (docs/magic_s22.md).
constexpr std::uint32_t kCountDownB = 0x4C2D90;   // MAGIC086: +0xB down, at 0 +2 on
constexpr std::uint32_t kCountDown9 = 0x4E47F0;   // MAGIC130: +9 down, at 0 +2 on
constexpr std::uint32_t kEndWhenNoChildren = 0x4E5200;   // MAGIC131: at +0xB 0 the done flag and free

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S22_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC096 (row 102, Blizzard read one id down)

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
        const auto delay = static_cast<unsigned char>(BlizzardShard_Delays[i] + 1);
        SetLong(child + 0x80, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(self)));
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
                                                 kCountDownB, kCountDown9, bof3::addr::BlizzardShard_End};
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
    SetLong(Sc() + 0xC, Long(reinterpret_cast<const unsigned char*>(BlizzardShard_Offsets) + Sc()[4] * 8u));
    SetLong(Sc() + 0x10, Long(reinterpret_cast<const unsigned char*>(BlizzardShard_Offsets) + Sc()[4] * 8u + 4));
    MH_AT(PrimFn, kTurnOffset)(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x34)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x38)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0x10))));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
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
    const short rot[4] = {tilt ? S16(s + 0x18) : short{0}, tilt ? S16(s + 0x1C) : short{0}, tilt ? short{0} : short{0x200},
                          0};
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
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
        std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(VS(8))) << 9) + static_cast<std::uint32_t>(Long(s + 0x34));
        std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(VS(0xA))) << 9) + static_cast<std::uint32_t>(Long(s + 0x38));
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x95, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_AT(PrimFn, kSetPolyFT3)(p);
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
        x = (static_cast<std::uint32_t>(static_cast<int>(VS(0x10))) << 9) + static_cast<std::uint32_t>(Long(s + 0x34));
        z = (static_cast<std::uint32_t>(static_cast<int>(VS(0x12))) << 9) + static_cast<std::uint32_t>(Long(s + 0x38));
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x95, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 0, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
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
// (>> 9, less 0x4000) and heights summed in DamageScratch, the count at +0xC,
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
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(SD(0) / SD(0xC) + 0x4000) << 9));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(SD(4) / SD(0xC) + 0x4000) << 9));
    SetWord(Sc() + 0x3E, static_cast<unsigned>(SD(8) / SD(0xC)) & 0xFFFF);
}

// ===========================================================================
// MAGIC097 (row 19, Jolt) and MAGIC098 (row 37, Lightning)

namespace {

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
        const std::int32_t x = Long(r + 0x34), z = Long(r + 0x38);
        SetLong(child + 0x80, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(self)));
        child[1] = 0;
        child[3] = static_cast<unsigned char>(i);
        SetLong(child + 0x34, x);
        SetLong(child + 0x38, z);
        SetLong(child + 0x3C, Long(r + 0x3C));
        child[9] = static_cast<unsigned char>(self[0xB] * 10 + 1);
        Inc(self[0xB]);
    }
    const unsigned char* const last = TaskSlot(slot);
    SetLong(Sc() + 0x34, Long(last + 0x34));
    SetLong(Sc() + 0x38, Long(last + 0x38));
    SetLong(Sc() + 0x3C, Long(last + 0x3C));
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
// DamageScratch +4's low byte or +6's. The first row's top edge (+0x24..,
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

// The band's start, both spells: DamageScratch +0 the radius `radius`, +2 the
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

}  // namespace

// original 0x4C9C60: the kind-2 task. Two entries by +1: Jolt_Start and
// MAGIC131's 0x4E5200 (the done flag and free once +0xB, the bolts left, is 0).
S22_EXPORT void __cdecl Jolt_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Jolt_Start, kEndWhenNoChildren};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Jolt_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C9C90: a bolt of kind-1 task 0x14 on every live target.
S22_EXPORT void __cdecl Jolt_Start(void) { StartBolts(0x14); }

// original 0x4C9E30: the bolt's kind-1 task, a jmp through JoltBolt_TaskTable
// (one entry) by +1, unchecked.
S22_EXPORT void __cdecl JoltBolt_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("JoltBolt_Task", phase, 1);
    magic_harness::Phase(bof3::addr::JoltBolt_Run)();
}

// original 0x4C9E50: a call through JoltBolt_Steps (four entries) by +2 -
// LightningBolt_Wait, JoltBolt_Rise, JoltBolt_Fade, JoltBolt_End -, the screen
// point; then while +2 and +0 are set, under the bolt's matrix, three band
// rows (radius 0x20 / 0x40 / 0x60, jitter 7 / 0xF / 0x1F, the angle stepping by
// 4), two arcs, and two flashes 16 pixels either side.
S22_EXPORT void __cdecl JoltBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::LightningBolt_Wait, bof3::addr::JoltBolt_Rise,
                                                bof3::addr::JoltBolt_Fade, bof3::addr::JoltBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("JoltBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    Call0(bof3::addr::LightningBolt_PushMatrix);
    Call3(bof3::addr::JoltBolt_DrawBand, 0xC, 0x20, 7);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::JoltBolt_DrawBand, 0xC, 0x40, 0xF);
    Call0(bof3::addr::JoltBolt_DrawArcs);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::JoltBolt_DrawBand, 0xC, 0x60, 0x1F);
    Call0(bof3::addr::JoltBolt_DrawArcs);
    AddB(Sc()[0xB], 0xF8);
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + 0x10u);
    Call0(bof3::addr::JoltBolt_DrawFlash);
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) - 0x20u);
    Call0(bof3::addr::JoltBolt_DrawFlash);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C9EF0: +0xB up, +0xA up by 4; at 0x10 the target darkened.
S22_EXPORT void __cdecl JoltBolt_Rise(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[0xA], 4);
    unsigned char* const s = Sc();
    if (s[0xA] != 0x10) return;
    DarkenTarget(s);
}

// original 0x4C9FC0 (Lightning's too): +0xB up, +9 down; at 0 the target's
// flag 0x40, its tint released, its flash (BattleActor_Flash), and +2 on.
S22_EXPORT void __cdecl JoltBolt_Fade(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[9]);
    const unsigned char* const s = Sc();
    if (s[9] != 0) return;
    if (TargetByte() & 0x40) {
        MH_CALL(Battle_SetTargetFlag40)(static_cast<unsigned char>(s[3] + 3));
        MH_CALL(Sprite_ReleaseTint)(EnemyRecord(Sc()[3]));
        MH_CALL(BattleActor_Flash)(static_cast<unsigned char>(Sc()[3] + 3));
    } else {
        MH_CALL(Battle_SetTargetFlag40)(s[3]);
        MH_CALL(Sprite_ReleaseTint)(PartyRecord(Sc()[3]));
        MH_CALL(BattleActor_Flash)(Sc()[3]);
    }
    Inc(Sc()[2]);
}

// original 0x4CA070 (Lightning's too): +0xB up, +0xA down by 2; at 0 the
// owner's count +0xB down and the task freed.
S22_EXPORT void __cdecl JoltBolt_End(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CA0B0: one row of Jolt's band - seventeen steps up the bolt,
// four gouraud quads a step round a square of the row's radius (a2 plus or
// minus Rand & a3 each step, turned by sin of ((+0xB + i) & 0xF) << 8), each
// step 0x40 higher and its corners a1 - Rand & 3 in; the lowest step's top
// shaded 1.
S22_EXPORT void __cdecl JoltBolt_DrawBand(int a1, int a2, int a3) {
    BandStart(a1, a2, 0xF, 8);
    for (int i = 1; i < 0x12; ++i) {
        SetSW(2, static_cast<unsigned>(((Sc()[0xB] + static_cast<unsigned char>(i)) & 0xFF & 0xF) << 8));
        BandRadius(a2, a3);
        SetSW(0xA, SW(0xC));
        const std::uint32_t r = RandCall();
        const int angle = SS(2);
        const std::uint32_t in = static_cast<std::uint32_t>(a1) - (r & 3);
        std::uint16_t top = VW(2);
        SetVW(0x12, top);
        top = static_cast<std::uint16_t>(top - SW(0xA));
        SetSW(0xC, in);
        const std::uint16_t h = VW(4);
        SetVW(0x10, 0);
        SetVW(0x14, h);
        SetVW(0x18, 0);
        SetVW(0x1A, top);
        SetVW(0x1C, h);
        SetVW(0, 0);
        const int v = MH_CALL(Math_Sin)(angle);
        const int y = Mul12(v, SS(0));
        SetVW(2, static_cast<unsigned>(y));
        const unsigned up = (0u - static_cast<unsigned>(i)) << 6;
        SetVW(4, up);
        SetVW(8, 0);
        SetVW(0xA, static_cast<unsigned>(y) - SW(0xC));
        SetVW(0xC, up);
        BandQuad(kRow1, i == 1);
        {
            const std::uint16_t d = SW(0xA);
            const std::int32_t in2 = SD(0xC);
            AddVW(0x12, 0u - d);
            AddVW(2, 0u - SW(0xC));
            AddVW(0x1A, static_cast<unsigned>(d) * 0xFFFEu);
            AddVW(0xA, (0u - static_cast<std::uint32_t>(in2)) << 1);
        }
        BandQuad(kRow2, i == 1);
        {
            const std::int32_t in2 = SD(0xC);
            const std::uint16_t d = SW(0xA);
            AddVW(2, static_cast<std::uint32_t>(in2) * 2);
            AddVW(0xA, static_cast<std::uint32_t>(in2) * 3);
            AddVW(0x1A, d * 3u);
            AddVW(0x12, d * 2u);
        }
        BandQuad(kRow3, i == 1);
        {
            const std::int32_t in2 = SD(0xC);
            AddVW(0xA, static_cast<std::uint32_t>(in2));
            const std::uint16_t d = SW(0xA);
            AddVW(0x1A, d);
            AddVW(2, static_cast<std::uint32_t>(in2) * 2);
            AddVW(0x12, d * 2u);
        }
        BandQuad(kRow4, i == 1);
        AddVW(2, 0u - static_cast<std::uint32_t>(SD(0xC)) * 3);
    }
}

// original 0x4CA6B0: Jolt's arcs - a zigzag of +0xA - 1 gouraud lines up the
// bolt from (0, sin of ((+0xB + 2) & 0xF) << 8 x 0x40 >> 12), each step 0x40
// higher and swung by sin of ((+0xB + i + 2) & 0xF) << 8 x (0x40 plus or minus
// Rand & 0x1F); shades Rand & 0x3F, blue 0x20. Tpage 0xB5; each line sorted at
// the task moved by its first point >> 3.
S22_EXPORT void __cdecl JoltBolt_DrawArcs(void) {
    SetSW(0, 0x40);
    {
        const unsigned char* const s = Sc();
        SetSW(2, ((s[0xB] + 2u) & 0xF) << 8);
        SetSW(8, s[0xA]);
    }
    std::uint32_t r = RandCall();
    SetSW(4, r & 0x3F);
    SetVW(0, 0);
    int v = MH_CALL(Math_Sin)(SS(2));
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(4, 0);
    if (SS(8) <= 1) return;
    for (int i = 1;;) {
        const unsigned char* const s = Sc();
        unsigned char* p = Gfx_PacketNext;
        const std::uint32_t x = static_cast<std::uint32_t>(VS(0) >> 3) + static_cast<std::uint32_t>(Long(s + 0x34));
        const std::uint32_t z = static_cast<std::uint32_t>(VS(2) >> 3) + static_cast<std::uint32_t>(Long(s + 0x38));
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
            SetSW(0, (r & 0x1F) + 0x40);
        } else {
            r = RandCall();
            SetSW(0, 0x40 - (r & 0x1F));
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
        p[0x15] = SB(4);
        p[0x16] = 0x20;
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x24);
        if (++i >= SS(8)) break;
    }
}

namespace {

// The flashes of 0x4CA8C0 / 0x4CB6E0 / 0x4CC7D0: eight gouraud triangles
// round the task's screen point (+0x2E / +0x30), radius Rand & 7 plus +0xA x
// `per`, kept in DamageScratch as a word or (`dword`) a dword and read back
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

// original 0x4CA8C0: Jolt's flash, radius Rand & 7 + +0xA x 4 (a word), the
// centre (12, 12, 10) x +0xA.
S22_EXPORT void __cdecl JoltBolt_DrawFlash(void) { Flash(4, false, 12, 10); }

// original 0x4CAA70: the kind-2 task. Two entries by +1: Lightning_Start and
// MAGIC131's 0x4E5200.
S22_EXPORT void __cdecl Lightning_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Lightning_Start, kEndWhenNoChildren};
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
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + 0x18u);
    Call0(bof3::addr::LightningBolt_DrawFlash);
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) - 0x30u);
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

// ===========================================================================
// MAGIC099 (row 36, Myollnir)

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
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    const auto child = [](unsigned char kind) {
        unsigned char* const c = TaskSlot(NewTask(0xC));
        SetLong(c + 0x80, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Sc())));
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
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
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

namespace {

// The children's circle round the owner: the angle (+0xC & 0x7F) << 5 and the
// radius in DamageScratch (dwords +4, +0), the position the owner's plus cos /
// sin x radius (no shift), each read back after the call before it.
void Orbit(std::uint32_t radius) {
    const std::uint32_t angle = (static_cast<std::uint32_t>(Long(Sc() + 0xC)) & 0x7F) << 5;
    SetSD(0, radius);
    SetSD(4, angle);
    int v = MH_CALL(Math_Cos)(static_cast<int>(angle));
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(MulU(v, SD(0)) + static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    v = MH_CALL(Math_Sin)(SD(4));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(MulU(v, SD(0)) + static_cast<std::uint32_t>(Long(Owner() + 0x38))));
}
// A circling child's step: +0xB up, the angle +0xC up, the circle.
void OrbitStep(std::uint32_t radius) {
    Inc(Sc()[0xB]);
    SetLong(Sc() + 0xC, Long(Sc() + 0xC) + 1);
    Orbit(radius);
}

}  // namespace

// original 0x4CBC60: +9 and +0xA 0; the angle +0xC the index x 32; the circle
// of radius 0x28 and the owner's height; +2 on.
S22_EXPORT void __cdecl MyollnirOrb_Start(void) {
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(Sc()[3] << 5));
    Orbit(0x28);
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E));
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
    SetLong(Sc() + 0xC, static_cast<std::int32_t>((Sc()[3] << 5) + 0x10));
    Orbit(0x18);
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E));
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
        const std::uint32_t x = static_cast<std::uint32_t>(VS(0) >> 3) + static_cast<std::uint32_t>(Long(s + 0x34));
        const std::uint32_t z = static_cast<std::uint32_t>(VS(2) >> 3) + static_cast<std::uint32_t>(Long(s + 0x38));
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

void MagicS22_Inject() {
    if (bof3::WantsShadow("magic_s22")) magic_s22::SelfTest();
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
    BOF3_INJECT(Jolt_Task);
    BOF3_INJECT(Jolt_Start);
    BOF3_INJECT(JoltBolt_Task);
    BOF3_INJECT(JoltBolt_Run);
    BOF3_INJECT(JoltBolt_Rise);
    BOF3_INJECT(JoltBolt_Fade);
    BOF3_INJECT(JoltBolt_End);
    BOF3_INJECT(JoltBolt_DrawBand);
    BOF3_INJECT(JoltBolt_DrawArcs);
    BOF3_INJECT(JoltBolt_DrawFlash);
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
