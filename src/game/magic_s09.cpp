// Four spell overlays compiled into the exe, round nine group S09
// (docs/magic_s09.md): the PSX's MAGIC045, MAGIC046 / MAGIC047 (the linker
// kept one copy of the two files' code), MAGIC048 and MAGIC050.EMI,
// Magic_Rows rows 61, 62 / 63, 41 and 31. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Bone Dart,
// Firebreath / Icebreath, Dream Breath and Pollen / Venom Breath; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC045 0x4A9830..0x4A9FD8: two children (kind 1, 0x31) - a dart that
//     flies at the source sprite, flags the target and bounces back, and a
//     shadow that follows it along the ground - unless the actor and the
//     target are on one side, which only flags the target;
//   - MAGIC046/047 0x4A9FE0..0x4AAC36: an emitter child (kind 1, 0x32) that
//     takes motes from a pool of its own (ElemBreath_Motes, 48 records) every
//     other frame; the task walks the pool, each mote a textured quad and a
//     flare of eight triangles drifting out along a sine; in an event battle
//     whose current enemy's +0x100 is 0x29 a second child plays the task's
//     animation 2;
//   - MAGIC048 0x4AAC40..0x4AB3EB: twelve motes (kind 1, 0x1E) that fly from
//     the caster to points over the target side's mean height, each a band of
//     32 gouraud quads;
//   - MAGIC050 0x4AB3F0..0x4ABC95: ten motes (kind 1, 4) placed round the
//     field's kind-2 point, each a fan, a ring and sixteen dots in screen
//     space, shaded by the ability word 0x904B80 (0x32 one way, else the
//     other: Pollen and Venom Breath read one id down).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and
// DreamBreath_TargetHeight aborts where the original's idiv faults (no target
// in: the live-target divide, docs/takeover-queue-round9.md section 7).
#include "game/magic_s09.h"

#include <climits>
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
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kFrameSet = 0x9039D8;         // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kEventBattle = 0x904AAA;      // u8: not 0 in an event battle
constexpr std::uint32_t kCurrentEnemy = 0x939AD8;     // unsigned char *: the enemy the per-frame loops set last
constexpr std::uint32_t kAbilityId = 0x904B80;        // u16: the ability id (0x32 Pollen, 0x33 Venom Breath)

// This group's .data (symbols.toml [[data]]).
constexpr std::uint32_t kMotePool = 0x67DE40;         // ElemBreath_Motes: 48 records of 0x84 bytes
constexpr unsigned kMoteRecords = 48;
constexpr std::uint32_t kPollenDelays = 0x65A988;     // Pollen_Delays: 10 bytes
constexpr std::uint32_t kPollenOffsets = 0x65A998;    // PollenMote_Offsets: (dx, dz) dword pairs

// The phase handlers of other units a table holds (docs/magic_s09.md
// section 3): the engine's below by address; the rest ours now and named in
// the tables (S30's MagicFx_EndWhenChildrenDone, BattleFx_Finish, S24's
// MagicFx_EndWithChildren, BattleFx_FreeTask, S37's Combustion_Wait, S11's
// MagicFx_UncountAndFree, C2's HolocaustBeam_Grow).
constexpr std::uint32_t kScriptUntilDone = bof3::addr::BattleFx_ScriptUntilDone;  // engine: the script ticked, the sprite queued, +2 on at the done flag

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }
// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }
// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) - static_cast<std::uint32_t>(v >> 31)) >> 1; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolRecord(unsigned index) { return Mem(kMotePool + index * 0x84u); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The projections with the arguments the originals push: the depth and a
// flag pointer past the declared ones (cdecl: the caller pops them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S09_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}
// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's +8 and position (+0x34 / +0x38 / +0x3C) the owner's.
void FacingAndPositionFromOwner() {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

// Gfx_ClutStrip 0x1A00..0x1A0F (row 26's first sixteen words) back from its
// source with the semi-transparency bit, then the first word plain; the
// strip marked dirty first.
void RestoreRow26Head() {
    Gfx_ClutStripDirty = 1;
    for (unsigned k = 0; k < 0x10; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
}

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

// Camera_Matrix x the rotation of `angles`, translated by RotTrans of `v`,
// set as the GTE's (the two matrix pushes' common tail). The original pushes
// a third argument (a flag pointer) to Gte_RotTrans, which takes two.
void SetTurnedMatrix(const short* v, const short* angles) {
    Matrix m;
    long flag;
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S09_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// The flying sprites' sway: *at up by the low 20 bits of v x 7 (`lea` x 8, sub,
// `shl 0xC`, `sar 0xC`).
void AddSway(unsigned char* at, int v) { AddLong(at, U(ShlSar(Imul(v, 7), 12, 12))); }

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC045 (row 61, Bone Dart read one id down)

// original 0x4A9830: the kind-2 task. A two-entry stack table by +1:
// BoneDart_Start, MAGIC131's MagicFx_EndWhenChildrenDone.
S09_EXPORT void __cdecl BoneDart_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::BoneDart_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 2, Sc()[1], "BoneDart_Task");
}

// original 0x4A9860: the actor and the target on one side (both below 3 or
// both above 2): the target flagged 0x40, the done flag, the task freed.
// Else the owner's direction and position; two children (kind 1, 0x31): the
// dart (+1 0) and its shadow (+1 1, +0xB the dart's slot), each +0x80 this
// task; row 26's first sixteen CLUT words back with their STP bits; +0xB 2
// (two children), +1 on.
S09_EXPORT void __cdecl BoneDart_Start(void) {
    const unsigned char target = TargetByte();
    const unsigned char actor = Mem(at::kActor)[0];
    const bool same_side = (target < 3 && actor < 3) || (target > 2 && actor > 2);
    if (same_side) {
        MH_CALL(Battle_SetTargetFlag40)(target);
        Mem(at::kFlags)[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    FacingAndPositionFromOwner();
    const unsigned dart = NewTask(0x31);
    {
        unsigned char* const child = TaskSlot(dart);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
        child[1] = 0;
    }
    const unsigned shadow = NewTask(0x31);
    unsigned char* const self = Sc();
    {
        unsigned char* const child = TaskSlot(shadow);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 1;
        child[0xB] = static_cast<unsigned char>(dart);
    }
    RestoreRow26Head();
    self[0xB] = 2;
    Inc(Sc()[1]);
}

// original 0x4A99A0: the children's kind-1 task, a jmp through
// BoneDartChild_Kinds (two entries) by +1, unchecked.
S09_EXPORT void __cdecl BoneDartChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::BoneDartShaft_Run, bof3::addr::BoneDartShadow_Run};
    Dispatch(kKinds, 2, Sc()[1], "BoneDartChild_Task");
}

// The children's run: the effects' frame-offset table in 0x9039D8 round a
// call through `steps` (four entries) by +2; the screen update while +0 and
// +2 are set.
static void BoneDartChildRun(const std::uint32_t* steps, const char* who) {
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(steps, 4, Sc()[2], who);
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4A99C0: the dart, through BoneDartShaft_Steps (the last
// MAGIC058's 0x4AF490: the owner's count down, the task freed).
S09_EXPORT void __cdecl BoneDartShaft_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BoneDartShaft_Start, bof3::addr::BoneDartShaft_Fly,
                                                bof3::addr::BoneDartShaft_Bounce, bof3::addr::MagicFx_UncountAndFree};
    BoneDartChildRun(kSteps, "BoneDartShaft_Run");
}

// original 0x4A9A00: the owner's direction; (0x10000, 0) turned by it onto the
// owner's position, 0x100 above it; the sprite set up (fields +0x24..+0x2C,
// scale +0x40 / +0x44 0x10000, +0x48 1, no tint); animation 0; +9 0, +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x10000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) + U(Long(s + 0xC))));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) + U(Long(s + 0x10))));
        SetWord(s + 0x3E, (Word(Owner() + 0x3E) + 0x100u) & 0xFFFF);
        s[0x25] = 0x1D;
        s[0x26] = 0;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 1;
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 1;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4A9B30: the script ticked; a step toward the source sprite's
// point (0x904B4C: its position and (height + 0xC0) / 2); near it (within
// 0x8000): the round's flag 0x2000, the target flagged 0x40, sound 0x203, the
// sprite's +0 bit 5 and tint mode 1 (no tint), the heading back to the owner
// (Math_Ratan2 of the owner's offset), +0x14 0x40 and +0x20 -8 (the rise and
// its pull), +9 0x10, +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Fly(void) {
    MH_CALL(Sprite_ScriptTick)();
    {
        const unsigned char* const src = Pointer(at::kSource);
        const std::int32_t x = (Long(src + 0x34) >> 9) - 0x4000;
        const std::int32_t z = (Long(src + 0x38) >> 9) - 0x4000;
        const std::int32_t y = (S16(src + 0x3E) + 0xC0) >> 1;
        MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x30);
    }
    if (MH_CALL(MagicFx_NearSprite)(Pointer(at::kSource), 0x8000) == 0) return;
    const unsigned char target = TargetByte();
    Mem(at::kFlags + 1)[0] |= 0x20;
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(Sound_PlayById)(0x203);
    {
        unsigned char* const s = Sc();
        s[0] |= 0x20;
        s[0x5C] = 1;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        const unsigned char* const o = Owner();
        const std::int32_t dz = static_cast<std::int32_t>(U(Long(o + 0x38)) - U(Long(s + 0x38)));
        const std::int32_t dx = static_cast<std::int32_t>(U(Long(o + 0x34)) - U(Long(s + 0x34)));
        const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        unsigned char* const t = Sc();
        SetLong(t + 0xC, heading);
        SetLong(t + 0x14, 0x40);
        SetLong(t + 0x20, -8);
        t[9] = 0x10;
        Inc(t[2]);
    }
}

// original 0x4A9B30's sibling 0x4A9C60: the script ticked on odd frames; the
// tint 6 darker; along the heading +0xC by twice its sine and cosine (each
// address taken before its call); the rise +0x14 pulled by +0x20 and added to
// the height; +9 down, at 0 +2 on.
S09_EXPORT void __cdecl BoneDartShaft_Bounce(void) {
    if (Frame_Counter & 1) MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        AddB(s[0x5D], 0xFA);
        AddB(s[0x5E], 0xFA);
        AddB(s[0x5F], 0xFA);
    }
    {
        unsigned char* const p = Sc() + 0x34;
        const int v = Sin(Long(Sc() + 0xC));
        AddLong(p, U(ShlSar(v, 13, 12)));
    }
    {
        unsigned char* const p = Sc() + 0x38;
        const int v = Cos(Long(Sc() + 0xC));
        AddLong(p, U(ShlSar(v, 13, 12)));
    }
    unsigned char* const s = Sc();
    AddLong(s + 0x14, U(Long(s + 0x20)));
    SetWord(s + 0x3E, (Word(s + 0x3E) + Word(s + 0x14)) & 0xFFFF);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4A9D10: the shadow, through BoneDartShadow_Steps (the last
// MAGIC058's 0x4AF490).
S09_EXPORT void __cdecl BoneDartShadow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BoneDartShadow_Start, bof3::addr::BoneDartShadow_Follow,
                                                bof3::addr::BoneDartShadow_Fade, bof3::addr::MagicFx_UncountAndFree};
    BoneDartChildRun(kSteps, "BoneDartShadow_Run");
}

// original 0x4A9D50: the owner's position and height; the sprite set up as
// the dart's but +0x2B 0, +0 bit 5 and tint mode 2 at 0xC0; animation 0; +9
// 0, +2 on.
S09_EXPORT void __cdecl BoneDartShadow_Start(void) {
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Long(Owner() + 0x34));
        SetLong(s + 0x38, Long(Owner() + 0x38));
        SetWord(s + 0x3E, Word(Owner() + 0x3E));
        s[0x25] = 0x1D;
        s[0x26] = 0;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 1;
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
        s[0] |= 0x20;
        s[0x5C] = 2;
        s[0x5D] = 0xC0;
        s[0x5E] = 0xC0;
        s[0x5F] = 0xC0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// The shadow under the dart (the task slot +0xB, unchecked: 155 and above lie
// past the image, a fault in both): its x / z, the ground's height there, the
// dart's +0x3C when its height is below the ground's; +2 on when the dart's
// +2 is `step`.
static void BoneDartShadowFollow(unsigned step) {
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Long(TaskSlot(s[0xB]) + 0x34));
        SetLong(s + 0x38, Long(TaskSlot(s[0xB]) + 0x38));
    }
    const long ground = MH_CALL(AreaMap_Elevation)(Long(Sc() + 0x34), Long(Sc() + 0x38));
    SetWord(Sc() + 0x3E, static_cast<std::uint32_t>(ground) & 0xFFFF);
    unsigned char* const s = Sc();
    {
        const unsigned char* const dart = TaskSlot(s[0xB]);
        if (S16(dart + 0x3E) < S16(s + 0x3E)) SetLong(s + 0x3C, Long(dart + 0x3C));
    }
    if (TaskSlot(s[0xB])[2] == step) Inc(s[2]);
}

// original 0x4A9E50: the script ticked; under the dart until its +2 is 2.
S09_EXPORT void __cdecl BoneDartShadow_Follow(void) {
    MH_CALL(Sprite_ScriptTick)();
    BoneDartShadowFollow(2);
}

// original 0x4A9F00: the script ticked on odd frames; the tint 3 darker;
// under the dart until its +2 is 3.
S09_EXPORT void __cdecl BoneDartShadow_Fade(void) {
    if (Frame_Counter & 1) MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        AddB(s[0x5D], 0xFD);
        AddB(s[0x5E], 0xFD);
        AddB(s[0x5F], 0xFD);
    }
    BoneDartShadowFollow(3);
}

// ===========================================================================
// MAGIC046 / MAGIC047 (rows 62 / 63, Firebreath / Icebreath read one id down)

// original 0x4A9FE0: the kind-2 task. A two-entry stack table by +1 -
// ElemBreath_Start, BattleFx_Finish - then the mote pool walked: every record
// with bit 0 becomes Sprite_Current, its +0x80 the owner, for
// ElemBreathMote_Run; both put back after each (as read after the phase
// call).
S09_EXPORT void __cdecl ElemBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::ElemBreath_Start, bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "ElemBreath_Task");
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(bof3::addr::ElemBreathMote_Run);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

// original 0x4AA060: the pool's records +0..+2 cleared; the owner's direction
// and position; the emitter (kind 1, 0x32, +1 0, +0x80 this task); in an event
// battle whose current enemy (0x939AD8) has +0x100 0x29 a second child (+1 1);
// row 26's first sixteen CLUT words back with their STP bits; sound 0x100;
// +0xB 1, +1 on.
S09_EXPORT void __cdecl ElemBreath_Start(void) {
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    FacingAndPositionFromOwner();
    {
        const unsigned slot = NewTask(0x32);
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
        child[1] = 0;
    }
    if (Mem(kEventBattle)[0] != 0 && Pointer(kCurrentEnemy)[0x100] == 0x29) {
        const unsigned slot = NewTask(0x32);
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
        child[1] = 1;
    }
    RestoreRow26Head();
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 1;
    Inc(Sc()[1]);
}

// original 0x4AA1A0: the children's kind-1 task, a jmp through
// ElemBreathChild_Kinds (two entries) by +1, unchecked.
S09_EXPORT void __cdecl ElemBreathChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ElemBreathEmitter_Task, bof3::addr::ElemBreathEnemy_Task};
    Dispatch(kKinds, 2, Sc()[1], "ElemBreathChild_Task");
}

// original 0x4AA1C0: the emitter. A three-entry stack table by +2:
// ElemBreathEmitter_Start, _Emit, MAGIC104's MagicFx_EndWithChildren.
S09_EXPORT void __cdecl ElemBreathEmitter_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathEmitter_Start, bof3::addr::ElemBreathEmitter_Emit,
                                                bof3::addr::MagicFx_EndWithChildren};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathEmitter_Task");
}

// original 0x4AA1F0: the owner's direction and position; +0xB 0, +9 0, +2 on.
S09_EXPORT void __cdecl ElemBreathEmitter_Start(void) {
    FacingAndPositionFromOwner();
    unsigned char* const s = Sc();
    s[0xB] = 0;
    s[9] = 0;
    Inc(s[2]);
}

// original 0x4AA260: on odd frames a mote from the pool (its "none free" 0xFF
// unchecked, as in the original: record 255 lies at 0x6861BC, past the pool):
// +0x80 this task, +0xB our +9 (its phase along the sway), +0xA its life (8,
// shortening by one each four frames past +9 0x64); our count +0xB up. Every
// frame +9 up; past 0x80 +2 on.
S09_EXPORT void __cdecl ElemBreathEmitter_Emit(void) {
    if (Frame_Counter & 1) {
        const unsigned index = MH_AT(ByteFn, bof3::addr::ElemBreathMote_Alloc)() & 0xFFu;
        unsigned char* const s = Sc();
        unsigned char* const rec = PoolRecord(index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(s)));
        rec[0xB] = s[9];
        const unsigned char age = s[9];
        rec[0xA] = age < 0x64 ? static_cast<unsigned char>(8) : static_cast<unsigned char>(8 - ((age - 0x64) >> 2));
        Inc(s[0xB]);
    }
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] > 0x80) Inc(s[2]);
}

// original 0x4AA2F0: the second child. A three-entry stack table by +2:
// ElemBreathEnemy_Start, the engine's 0x43EC10 (the script until its done
// flag), BattleFx_FreeTask.
S09_EXPORT void __cdecl ElemBreathEnemy_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathEnemy_Start, kScriptUntilDone,
                                                bof3::addr::BattleFx_FreeTask};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathEnemy_Task");
}

// original 0x4AA320: Sprite_Current made the owner (the breath task; not put
// back); its +0xB and +9 0; animation 2; its +1 and +2 1.
S09_EXPORT void __cdecl ElemBreathEnemy_Start(void) {
    unsigned char* const o = Owner();
    Sprite_Current = o;
    o[0xB] = 0;
    Sc()[9] = 0;
    MH_CALL(Sprite_SetAnimation)(2);
    Sc()[1] = 1;
    Sc()[2] = 1;
}

// original 0x4AA360: a mote (Sprite_Current a pool record). A three-entry
// stack table by +2 - ElemBreathMote_Start, _Flow, _Fade -; then while +0 is
// set its matrix, its flare, the matrix popped.
S09_EXPORT void __cdecl ElemBreathMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ElemBreathMote_Start, bof3::addr::ElemBreathMote_Flow,
                                                bof3::addr::ElemBreathMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "ElemBreathMote_Run");
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::ElemBreathMote_PushMatrix);
    Call0(bof3::addr::ElemBreathMote_DrawFlare);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AA3B0: the task to the side's centre; its angle from the owner
// (Math_Ratan2 of the offset); the owner's direction; an offset by the acting
// enemy's byte +0x8C (unchecked: a party actor reads below the enemy records)
// - 0x10 and 0x72 farther and higher - turned by the direction onto the
// owner's position; +0x14 the angle and 0xFFF, +9 0, +2 on.
S09_EXPORT void __cdecl ElemBreathMote_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    int angle;
    {
        const unsigned char* const s = Sc();
        const unsigned char* const o = Owner();
        const std::int32_t dz = static_cast<std::int32_t>(U(Long(s + 0x38)) - U(Long(o + 0x38)));
        const std::int32_t dx = static_cast<std::int32_t>(U(Long(s + 0x34)) - U(Long(o + 0x34)));
        angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    }
    Sc()[8] = Owner()[8];
    {
        const unsigned kind = EnemyRecord(Mem(at::kActor)[0])[0x8C];
        std::int32_t reach, lift;
        if (kind == 0x10) {
            reach = 0x10000;
            lift = 0x800000;
        } else if (kind == 0x72) {
            reach = 0x1C000;
            lift = 0x1000000;
        } else {
            reach = 0x6000;
            lift = 0x400000;
        }
        unsigned char* const s = Sc();
        SetLong(s + 0xC, reach);
        SetLong(s + 0x10, 0);
        SetLong(s + 0x14, lift);
    }
    Turn(Sc());
    unsigned char* const s = Sc();
    const unsigned char* const o = Owner();
    SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(o + 0x34)) + U(Long(s + 0xC))));
    SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(o + 0x38)) + U(Long(s + 0x10))));
    SetLong(s + 0x3C, static_cast<std::int32_t>(U(Long(o + 0x3C)) + U(Long(s + 0x14))));
    SetLong(s + 0x14, static_cast<std::int32_t>(U(angle) & 0xFFF));
    s[9] = 0;
    Inc(s[2]);
}

// The motes' drift, one frame: the sway angle ((+0xB + +9) & 0x3F) << 6 kept
// in word 0x903852; the heading +0xC the angle +0x14 plus sin x 300 >> 12; the
// position along it by 7 x sin / cos (each address taken before its call);
// the screen point; the quad drawn.
static void ElemBreathMoteDrift() {
    {
        const unsigned char* const s = Sc();
        const unsigned angle = (static_cast<unsigned>(static_cast<unsigned char>(s[0xB] + s[9])) & 0x3F) << 6;
        SetSW(2, angle);
        const int v = Sin(static_cast<short>(angle));
        unsigned char* const t = Sc();
        SetLong(t + 0xC, static_cast<std::int32_t>(U(Imul(v, 300) >> 12) + U(Long(t + 0x14))));
    }
    {
        unsigned char* const p = Sc() + 0x34;
        AddSway(p, Sin(Long(Sc() + 0xC)));
    }
    {
        unsigned char* const p = Sc() + 0x38;
        AddSway(p, Cos(Long(Sc() + 0xC)));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::ElemBreathMote_Draw);
}

// original 0x4AA510: the drift; +9 up, at 0x10 +2 on.
S09_EXPORT void __cdecl ElemBreathMote_Flow(void) {
    ElemBreathMoteDrift();
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x4AA5D0: the drift; +9 up, the life +0xA down; at 0 the emitter's
// count +0xB down and the record freed (+0..+4 0).
S09_EXPORT void __cdecl ElemBreathMote_Fade(void) {
    ElemBreathMoteDrift();
    unsigned char* const s = Sc();
    Inc(s[9]);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4AA6D0: a draw-mode packet (tpage 0x35) linked at the task; a
// semi-transparent textured gouraud quad (tpage 0x340 / 0x100 at 1, CLUT row
// 0x1FA) round the screen point: radius word 0x903850 ((+9 + 1) x 6 below 8,
// then +9 x 2 + 0x20), centre words 0x903854 / 0x903856, shade 0x903858 +0xA
// x 20; the corners at 0xC00, 0x800, 0 and 0x400 (sin x, cos y; the scratch
// read again after every call) as floats; linked (0x54).
S09_EXPORT void __cdecl ElemBreathMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const unsigned char* const s = Sc();
        const unsigned age = s[9];
        SetSW(0, age < 8 ? (age + 1) * 6 : age * 2 + 0x20);
        SetSW(8, s[0xA] * 20u);
        SetSW(4, Word(s + 0x2E));
        SetSW(6, Word(s + 0x30));
    }
    static constexpr struct { int angle; unsigned x, y; } kCorners[4] = {
        {0xC00, 8, 0xC}, {0x800, 0x1C, 0x20}, {0, 0x30, 0x34}, {0x400, 0x44, 0x48}};
    for (const auto& c : kCorners) {
        int v = Sin(c.angle);
        PutFloat(p + c.x, Mul12(v, SS(0)) + SS(4));
        v = Cos(c.angle);
        PutFloat(p + c.y, Mul12(v, SS(0)) + SS(6));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
    SetWord(p + 0x2A, tpage & 0xFFFF);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
    SetWord(p + 0x16, clut & 0xFFFF);
    p[0x14] = 1;
    p[0x15] = 1;
    p[0x28] = 0x1F;
    p[0x29] = 1;
    p[0x3C] = 1;
    p[0x3D] = 0x1F;
    p[0x50] = 0x1F;
    p[0x51] = 0x1F;
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(8);
    LinkAtSprite(0x54);
}

// original 0x4AA980: the mote's matrix pushed: its x / z, the owner's height,
// no rotation.
S09_EXPORT void __cdecl ElemBreathMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    short v[4];
    const unsigned char* const s = Sc();
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-Half(S16(Owner() + 0x3E)));
    v[3] = 0;
    SetTurnedMatrix(v, angles);
}

// original 0x4AAA30: a draw-mode packet (tpage 0x55) committed to layer 5;
// radius word 0x903850 (+9 + 6) x 10, shade 0x903858 +0xA x 6; eight
// semi-transparent gouraud triangles fanned from the origin (Prim_VertexScratch:
// the origin, the last edge, the next at the angle, the scratch read again
// after every call), projected (Gte_RotTransPers3, Gte_PrimDepths3_10B) and
// committed to layer 5; a closing draw-mode packet (tpage 0x15).
S09_EXPORT void __cdecl ElemBreathMote_DrawFlare(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, (s[9] + 6u) * 10u);
        SetSW(8, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0x10, U(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x12, U(Mul12(v, SS(0))));
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t lx = VW(0x10), ly = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, lx);
        SetVW(0xA, ly);
        v = Sin(a);
        SetVW(0x10, U(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, U(Mul12(v, SS(0))));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        long depth, flag;
        S09_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(8);
        p[5] = SB(8);
        p[6] = SB(8);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4AABE0: the mote pool's alloc: the first of its 48 records with
// bit 0 clear gets it, its index in al; 0xFF when all are taken.
S09_EXPORT unsigned char __cdecl ElemBreathMote_Alloc(void) {
    for (unsigned i = 0; i < kMoteRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC048 (row 41, Dream Breath read one id down)

// original 0x4AAC40: the kind-2 task. A three-entry stack table by +1:
// DreamBreath_Start, MAGIC222's 0x4F7320, BattleFx_Finish.
S09_EXPORT void __cdecl DreamBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::DreamBreath_Start, bof3::addr::Combustion_Wait,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "DreamBreath_Task");
}

// original 0x4AAC70: +0xB 0; the owner's direction and position; +9 0xA, +1
// on; twelve motes (kind 1, 0x1E; the slots unchecked): +0x80 this task, +1
// 0, +0xB the index i, +9 ((i ^ 0xF) + 0x1D) << 3 (a byte: the delays), +8
// ours; our count +0xB up each.
S09_EXPORT void __cdecl DreamBreath_Start(void) {
    Sc()[0xB] = 0;
    FacingAndPositionFromOwner();
    Sc()[9] = 0xA;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned slot = NewTask(0x1E);
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(static_cast<unsigned char>((i ^ 0xF) + 0x1D) << 3);
        child[8] = s[8];
        Inc(s[0xB]);
    }
}

// original 0x4AAD50: the motes' kind-1 task, a jmp through
// DreamBreathMote_TaskTable (one entry) by +1, unchecked.
S09_EXPORT void __cdecl DreamBreathMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::DreamBreathMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "DreamBreathMote_Task");
}

// original 0x4AAD70: a call through DreamBreathMote_Steps (three entries) by
// +2; then while +2 and +0 are set the mote's matrix, its band, the matrix
// popped.
S09_EXPORT void __cdecl DreamBreathMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::DreamBreathMote_Start, bof3::addr::DreamBreathMote_Fly,
                                                bof3::addr::DreamBreathMote_Land};
    Dispatch(kSteps, 3, Sc()[2], "DreamBreathMote_Run");
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    Call0(bof3::addr::DreamBreathMote_PushMatrix);
    Call0(bof3::addr::DreamBreathMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AADB0: +9 down; at 0: the target side's mean height into
// +0x3E (DreamBreath_TargetHeight); (0x10000, 0) turned onto the owner's
// position; the destination +0xC / +0x10 (0x80000, 0) turned, from the
// field's kind-2 point, +0x14 that height + 0x100; our height the owner's +
// 0x100; +9 0, +0xA 0, +2 on.
S09_EXPORT void __cdecl DreamBreathMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Call0(bof3::addr::DreamBreath_TargetHeight);
    SetLong(Sc() + 0xC, 0x10000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) + U(Long(s + 0xC))));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) + U(Long(s + 0x10))));
        SetLong(s + 0xC, 0x80000);
        SetLong(s + 0x10, 0);
    }
    Turn(Sc());
    unsigned char* const s = Sc();
    AddLong(s + 0xC, U(Field_Kind2X));
    AddLong(s + 0x10, U(Field_Kind2Z));
    SetLong(s + 0x14, S16(s + 0x3E) + 0x100);
    SetWord(s + 0x3E, (Word(Owner() + 0x3E) + 0x100u) & 0xFFFF);
    s[9] = 0;
    s[0xA] = 0;
    Inc(s[2]);
}

// A step toward the destination (+0xC, +0x10, +0x14 / 2) at 0x10.
static void DreamBreathMoteStep() {
    const unsigned char* const s = Sc();
    const std::int32_t x = (Long(s + 0xC) >> 9) - 0x4000;
    const std::int32_t z = (Long(s + 0x10) >> 9) - 0x4000;
    const std::int32_t y = Long(s + 0x14) >> 1;
    MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x10);
}

// original 0x4AAEB0: +9 up; a step toward the destination; the shade +0xA up
// to 0x10; near it (MagicFx_NearPoint3D within 0x60000) +2 on.
S09_EXPORT void __cdecl DreamBreathMote_Fly(void) {
    Inc(Sc()[9]);
    DreamBreathMoteStep();
    unsigned char* const s = Sc();
    if (s[0xA] != 0x10) Inc(s[0xA]);
    if (MH_CALL(MagicFx_NearPoint3D)(U(Long(s + 0xC)), U(Long(s + 0x10)), U(Long(s + 0x3C)), 0x60000) != 0) Inc(Sc()[2]);
}

// original 0x4AAF50: a step toward the destination; +9 up, the shade down;
// at 0 the owner's count down and the task freed.
S09_EXPORT void __cdecl DreamBreathMote_Land(void) {
    DreamBreathMoteStep();
    unsigned char* const s = Sc();
    Inc(s[9]);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AAFD0: a draw-mode packet (tpage 0x35) linked at the task; the
// inner radius word 0x903852 +9 x 4, the outer 0x903854 +9 x 6 (past +9 8
// plus Rand & 0xF), shades 0x903856 +0xA x 8 and 0x903858 +0xA x 4; 32
// semi-transparent gouraud quads round the y-z plane between the two radii
// (Prim_VertexScratch: the last and next outer, the last and next inner edge;
// the scratch read again after every call), projected (Gte_RotTransPers4,
// Gte_PrimDepths4_10B) and linked at the task (0x44).
S09_EXPORT void __cdecl DreamBreathMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned char* s = Sc();
        SetSW(2, s[9] * 4u);
        SetSW(4, s[9] * 6u);
        if (s[9] > 8) {
            const std::uint32_t r = RandCall();
            SetSW(4, SW(4) + (r & 0xF));
            s = Sc();
        }
        SetSW(6, s[0xA] * 8u);
        SetSW(8, s[0xA] * 4u);
    }
    SetVW(0, 0);
    SetVW(8, 0);
    int v = Cos(0);
    SetVW(0xA, U(Mul12(v, SS(4))));
    v = Sin(0);
    SetVW(0xC, U(Mul12(v, SS(4))));
    SetVW(0x10, 0);
    SetVW(0x18, 0);
    v = Cos(0);
    SetVW(0x1A, U(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x1C, U(Mul12(v, SS(2))));
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t oy = VW(0xA), oz = VW(0xC);
            SetVW(2, oy);
            SetVW(4, oz);
        }
        v = Cos(a);
        SetVW(0xA, U(Mul12(v, SS(4))));
        v = Sin(a);
        {
            const int oz = Mul12(v, SS(4));
            const std::uint16_t iy = VW(0x1A);
            SetVW(0xC, U(oz));
            const std::uint16_t iz = VW(0x1C);
            SetVW(0x12, iy);
            SetVW(0x14, iz);
        }
        v = Cos(a);
        SetVW(0x1A, U(Mul12(v, SS(2))));
        v = Sin(a);
        SetVW(0x1C, U(Mul12(v, SS(2))));
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(8);
        p[0x34] = SB(6);
        p[0x35] = SB(8);
        p[0x36] = SB(8);
        long depth, flag;
        S09_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38, &depth,
                                         &flag);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
    }
}

// original 0x4AB250: the mote's matrix pushed, turned about z toward its
// destination (Math_Ratan2 of (+0x10 - +0x38, +0xC - +0x34), & 0xFFF): its x /
// z and its own height, read after the angle.
S09_EXPORT void __cdecl DreamBreathMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short angles[3] = {0, 0, 0};
    {
        const unsigned char* const s = Sc();
        const std::int32_t dx = static_cast<std::int32_t>(U(Long(s + 0xC)) - U(Long(s + 0x34)));
        const std::int32_t dz = static_cast<std::int32_t>(U(Long(s + 0x10)) - U(Long(s + 0x38)));
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dz), static_cast<float>(dx));
        angles[2] = static_cast<short>(angle & 0xFFF);
    }
    short v[4];
    const unsigned char* const s = Sc();
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-Half(S16(s + 0x3E)));
    v[3] = 0;
    SetTurnedMatrix(v, angles);
}

// original 0x4AB330: the mean height of the target side's actors still in
// (dwords 0x903850 the sum, 0x903854 the count, each read again after every
// Battle_ActorIsOut): the eight enemies with the target's bit 0x40, else the
// three party members; into word +0x3E. The original's idiv faults with none
// in (and on the one overflowing quotient); ours aborts there (the
// live-target divide, docs/takeover-queue-round9.md section 7).
S09_EXPORT void __cdecl DreamBreath_TargetHeight(void) {
    const unsigned char target = TargetByte();
    SetSD(4, 0);
    SetSD(0, 0);
    if (target & 0x40) {
        for (unsigned i = 0; i < 8; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i + 3) & 0xFF) != 0) continue;
            SetSD(0, U(SD(0)) + U(S16(Mem(at::kEnemies + i * at::kEnemyStride + 0x3E))));
            SetSD(4, U(SD(4)) + 1);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i) & 0xFF) != 0) continue;
            SetSD(0, U(SD(0)) + U(S16(PartyRecord(i) + 0x3E)));
            SetSD(4, U(SD(4)) + 1);
        }
    }
    const std::int32_t sum = SD(0);
    unsigned char* const s = Sc();
    const std::int32_t count = SD(4);
    if (count == 0) bof3::Fatal("DreamBreath_TargetHeight: no actor of the target side in (the original's idiv faults)");
    if (sum == INT_MIN && count == -1) bof3::Fatal("DreamBreath_TargetHeight: the mean overflows (the original's idiv faults)");
    SetWord(s + 0x3E, static_cast<std::uint32_t>(sum / count) & 0xFFFF);
}

// ===========================================================================
// MAGIC050 (row 31, Pollen / Venom Breath read one id down)

// original 0x4AB3F0: the kind-2 task. A three-entry stack table by +1:
// Pollen_Start, Pollen_Sounds, BattleFx_Finish.
S09_EXPORT void __cdecl Pollen_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Pollen_Start, bof3::addr::Pollen_Sounds,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Pollen_Task");
}

// original 0x4AB420: +0xB 0; the task to the side's centre; the owner's
// direction and x / z; +9 0, +0xB 0, +1 on; ten motes (kind 1, 4; the slots
// unchecked): +0x80 this task, +1 0, +0xB the index i, +9 Pollen_Delays[i] + 1,
// +8 and the position ours; our count +0xB up each; sound 0x100.
S09_EXPORT void __cdecl Pollen_Start(void) {
    Sc()[0xB] = 0;
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 10; ++i) {
        const unsigned slot = NewTask(4);
        const unsigned char delay = Mem(kPollenDelays + i)[0];
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(delay + 1);
        child[8] = s[8];
        SetLong(child + 0x34, Long(s + 0x34));
        SetLong(child + 0x38, Long(s + 0x38));
        SetLong(child + 0x3C, Long(s + 0x3C));
        Inc(s[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4AB520: sound 0x101 at +9 8 and 0x18, 0x102 at 0x10; +9 up, at
// 0x19 +1 on.
S09_EXPORT void __cdecl Pollen_Sounds(void) {
    const unsigned char t = Sc()[9];
    if (t == 8 || t == 0x18) MH_CALL(Sound_PlayById)(0x101);
    else if (t == 0x10) MH_CALL(Sound_PlayById)(0x102);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] == 0x19) Inc(s[1]);
}

// original 0x4AB570: the motes' kind-1 task, a jmp through
// PollenMote_TaskTable (one entry) by +1, unchecked.
S09_EXPORT void __cdecl PollenMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::PollenMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "PollenMote_Task");
}

// original 0x4AB590: a call through PollenMote_Steps (three entries, the
// second group C2's HolocaustBeam_Grow) by +2; then while +2 and +0 are set
// the screen point, the fan, the ring and the dots.
S09_EXPORT void __cdecl PollenMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::PollenMote_Start, bof3::addr::HolocaustBeam_Grow,
                                                bof3::addr::PollenMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "PollenMote_Run");
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::PollenMote_DrawFan);
    Call0(bof3::addr::PollenMote_DrawRing);
    Call0(bof3::addr::PollenMote_DrawSparks);
}

// original 0x4AB5D0: +9 down; at 0: (+0xC, +0x10) PollenMote_Offsets' pair by
// +0xB (read in place, unchecked), turned by the direction, from the field's
// kind-2 point; the first mote flags the target 0x10; +9 0x20, +0xA 2, +2 on.
S09_EXPORT void __cdecl PollenMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0xC, Long(Mem(kPollenOffsets + 8u * Sc()[0xB])));
    SetLong(Sc() + 0x10, Long(Mem(kPollenOffsets + 4 + 8u * Sc()[0xB])));
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(U(Long(s + 0xC)) + U(Field_Kind2X)));
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(s + 0x10)) + U(Field_Kind2Z)));
        if (s[0xB] == 0) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    }
    unsigned char* const s = Sc();
    s[9] = 0x20;
    s[0xA] = 2;
    Inc(s[2]);
}

// original 0x4AB680: +0xA up, +9 down; at 0 the owner's count down and the
// task freed.
S09_EXPORT void __cdecl PollenMote_Fade(void) {
    unsigned char* const s = Sc();
    Inc(s[0xA]);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The ability word: 0x32 (Pollen read one id down) or another (Venom Breath).
bool PollenShades() { return Word(Mem(kAbilityId)) == 0x32; }

// A screen-space point at radius dword 0x903850 + `r` of angle `a` from the
// task's screen point, as floats: the radius and the point read after the
// call, as the original reads them.
void PutSinX(unsigned char* at, int a, unsigned r) {
    const int v = Sin(a);
    PutFloat(at, (Imul(v, SD(r)) >> 12) + S16(Sc() + 0x2E));
}
void PutCosY(unsigned char* at, int a, unsigned r) {
    const int v = Cos(a);
    PutFloat(at, (Imul(v, SD(r)) >> 12) + S16(Sc() + 0x30));
}

}  // namespace

// original 0x4AB6C0: radius dword 0x903850 +0xA; shades 0x903858 (+9 x 3 for
// ability 0x32, else x 5) and 0x90385C (+9); per sixteenth of a turn a
// draw-mode packet (tpage 0x35) and a semi-transparent gouraud triangle from
// the screen point to two points on the circle, linked at the task (0x34).
S09_EXPORT void __cdecl PollenMote_DrawFan(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, s[0xA]);
        SetSD(8, PollenShades() ? s[9] * 3u : s[9] * 5u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000;) {
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutSinX(p + 0x18, a, 0);
        PutCosY(p + 0x1C, a, 0);
        a += 0x100;
        PutSinX(p + 0x28, a, 0);
        PutCosY(p + 0x2C, a, 0);
        if (PollenShades()) {
            p[4] = SB(0xC);
            p[5] = SB(8);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(8);
            p[0x16] = 1;
            p[0x24] = SB(8);
            p[0x25] = SB(8);
            p[0x26] = 1;
        } else {
            p[5] = 1;
            p[4] = SB(0xC);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(0xC);
            p[0x16] = SB(8);
            p[0x24] = SB(8);
            p[0x25] = SB(0xC);
            p[0x26] = SB(8);
        }
        LinkAtSprite(0x34);
    }
}

// original 0x4AB8E0: radii dword 0x903850 +0xA and 0x903854 +0xA x 1.5;
// shades 0x903858 (+9 x 4 for ability 0x32, else x 6) and 0x90385C (+9); per
// sixteenth of a turn a draw-mode packet (tpage 0x35) and a semi-transparent
// gouraud quad of the ring between them, the outer edge shaded, the inner 1,
// linked at the task (0x44).
S09_EXPORT void __cdecl PollenMote_DrawRing(void) {
    {
        const unsigned char* const s = Sc();
        const unsigned r = s[0xA];
        SetSD(0, r);
        SetSD(4, (r >> 1) + r);
        SetSD(8, PollenShades() ? s[9] * 4u : s[9] * 6u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000;) {
        const int b = a + 0x100;
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutSinX(p + 8, a, 0);
        PutCosY(p + 0xC, a, 0);
        PutSinX(p + 0x18, b, 0);
        PutCosY(p + 0x1C, b, 0);
        PutSinX(p + 0x28, a, 4);
        PutCosY(p + 0x2C, a, 4);
        PutSinX(p + 0x38, b, 4);
        PutCosY(p + 0x3C, b, 4);
        if (PollenShades()) {
            p[4] = SB(0xC);
            p[5] = SB(8);
            p[6] = SB(0xC);
            p[0x14] = SB(0xC);
            p[0x15] = SB(8);
            p[0x16] = SB(0xC);
        } else {
            p[4] = SB(8);
            p[5] = SB(0xC);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(0xC);
            p[0x16] = SB(8);
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        LinkAtSprite(0x44);
        a = b;
    }
}

// original 0x4ABB70: radius dword 0x903850 +0xA x 3; colour 0x903858 (+9 x 6)
// and 0x90385C (+9), their order by the ability word; per sixteenth of a turn
// a draw-mode packet (tpage 0x35) and a semi-transparent one-pixel tile on the
// circle, linked at the task (0x14).
S09_EXPORT void __cdecl PollenMote_DrawSparks(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, s[0xA] * 3u);
        SetSD(8, s[9] * 6u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000; a += 0x100) {
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile1)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutSinX(p + 8, a, 0);
        PutCosY(p + 0xC, a, 0);
        p[4] = SB(8);
        if (PollenShades()) {
            p[5] = SB(8);
            p[6] = SB(0xC);
        } else {
            p[5] = SB(0xC);
            p[6] = SB(8);
        }
        LinkAtSprite(0x14);
    }
}

void MagicS09_Inject() {
    if (bof3::WantsShadow("magic_s09")) magic_s09::SelfTest();
    BOF3_INJECT(BoneDart_Task);
    BOF3_INJECT(BoneDart_Start);
    BOF3_INJECT(BoneDartChild_Task);
    BOF3_INJECT(BoneDartShaft_Run);
    BOF3_INJECT(BoneDartShaft_Start);
    BOF3_INJECT(BoneDartShaft_Fly);
    BOF3_INJECT(BoneDartShaft_Bounce);
    BOF3_INJECT(BoneDartShadow_Run);
    BOF3_INJECT(BoneDartShadow_Start);
    BOF3_INJECT(BoneDartShadow_Follow);
    BOF3_INJECT(BoneDartShadow_Fade);
    BOF3_INJECT(ElemBreath_Task);
    BOF3_INJECT(ElemBreath_Start);
    BOF3_INJECT(ElemBreathChild_Task);
    BOF3_INJECT(ElemBreathEmitter_Task);
    BOF3_INJECT(ElemBreathEmitter_Start);
    BOF3_INJECT(ElemBreathEmitter_Emit);
    BOF3_INJECT(ElemBreathEnemy_Task);
    BOF3_INJECT(ElemBreathEnemy_Start);
    BOF3_INJECT(ElemBreathMote_Run);
    BOF3_INJECT(ElemBreathMote_Start);
    BOF3_INJECT(ElemBreathMote_Flow);
    BOF3_INJECT(ElemBreathMote_Fade);
    BOF3_INJECT(ElemBreathMote_Draw);
    BOF3_INJECT(ElemBreathMote_PushMatrix);
    BOF3_INJECT(ElemBreathMote_DrawFlare);
    BOF3_INJECT(ElemBreathMote_Alloc);
    BOF3_INJECT(DreamBreath_Task);
    BOF3_INJECT(DreamBreath_Start);
    BOF3_INJECT(DreamBreathMote_Task);
    BOF3_INJECT(DreamBreathMote_Run);
    BOF3_INJECT(DreamBreathMote_Start);
    BOF3_INJECT(DreamBreathMote_Fly);
    BOF3_INJECT(DreamBreathMote_Land);
    BOF3_INJECT(DreamBreathMote_Draw);
    BOF3_INJECT(DreamBreathMote_PushMatrix);
    BOF3_INJECT(DreamBreath_TargetHeight);
    BOF3_INJECT(Pollen_Task);
    BOF3_INJECT(Pollen_Start);
    BOF3_INJECT(Pollen_Sounds);
    BOF3_INJECT(PollenMote_Task);
    BOF3_INJECT(PollenMote_Run);
    BOF3_INJECT(PollenMote_Start);
    BOF3_INJECT(PollenMote_Fade);
    BOF3_INJECT(PollenMote_DrawFan);
    BOF3_INJECT(PollenMote_DrawRing);
    BOF3_INJECT(PollenMote_DrawSparks);
}
