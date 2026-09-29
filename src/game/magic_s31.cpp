// Four spell overlays compiled into the exe, round nine group S31
// (docs/magic_s31.md): the PSX's MAGIC132, MAGIC137, MAGIC138 and
// MAGIC143.EMI, Magic_Rows rows 139, 101, 118 and 66. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Doom Breath,
// Corona, Main Cannon and Thunder Clap; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC132 0x4E6950..0x4E7413: the caster darkened then brightened through
//     its tint record, one child (kind 1, 0x64) that flies out from the side's
//     centre and draws a stream of 63 textured quads and a two-quad glow;
//   - MAGIC137 0x4E7420..0x4E7FE0: two children (kind 1, 0x4B) - a ray that
//     travels across the field drawing a fan of textured gouraud quads, and a
//     full-screen flash - and, in an event battle whose acting enemy's +0x100
//     is 0x29, a copy of that enemy's record as a third child that plays its
//     animation 3;
//   - MAGIC138 0x4E7FF0..0x4E8656: the caster's script ticked while six shells
//     (kind 1, 0x56) are fired every eighth frame; each flies at the acting
//     actor and, past it, leaves a blast (the same kind, phase 1);
//   - MAGIC143 0x4E8660..0x4E9138: one bolt (kind 1, 0x35) at the source
//     sprite, drawn as Lightning's (group S22) with its own band, arcs and
//     flashes: three band rows, an arc, two flashes.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and Corona's ray
// matrix aborts on a facing past 3, where the original turns by an
// uninitialised stack word (the precedent of group S25's
// SpellConfuse_PushFacingMatrix).
#include "game/magic_s31.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8: not 0 in an event battle
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kFrameSetBlast = 0x8C5D80;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

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
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn3 = void (__cdecl*)(int, int, int);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call3(std::uint32_t address, int a, int b, int c) { MH_AT(Fn3, address)(a, b, c); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// The phase handlers of other units a table holds (docs/magic_s31.md
// section 3): the engine's below by address; the rest ours now and named in
// the tables (S38's MagicFx_CountDownFlag10, S30's
// MagicFx_EndWhenChildrenDone, S11's MagicFx_UncountAndFree, S15's
// ChillRay_Grow / _Shrink, S12's MagicFx_CountDownRelease).
constexpr std::uint32_t kScriptUntilDone = bof3::addr::BattleFx_ScriptUntilDone;  // engine: the script ticked, the sprite queued, +2 on at the done flag

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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}
// A draw-mode packet (tpage `tpage`, dithered) committed to layer 2.
void DrawModeCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// Row 26 of Gfx_ClutStrip back from its source (0x812980 / 0x80E980), each
// word with its semi-transparency bit set.
void RestoreRow26Stp() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// The actor-matrix push of 0x4E6DF0 / 0x4E77D0 (MagicFx_PushActorMatrix's
// shape with a turn about z): Camera_Matrix x the task's, translation
// RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX
// block as the original lays it out on its stack, RotTrans writing its
// translation at +0x14; the task is read once, after the push.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
void PushTurnedMatrix(const unsigned char* s, short turn) {
    const short rot[4] = {0, 0, turn, 0};
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S31_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

}  // namespace

#define S31_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC132 (row 139, Doom Breath read one id down)

// original 0x4E6950: the kind-2 task. A five-entry stack table by +1:
// DoomBreath_Start, _Brighten, _Fade, MAGIC226/227's 0x4F9F70, _End.
S31_EXPORT void __cdecl DoomBreath_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::DoomBreath_Start, bof3::addr::DoomBreath_Brighten,
                                                 bof3::addr::DoomBreath_Fade, bof3::addr::MagicFx_CountDownFlag10,
                                                 bof3::addr::DoomBreath_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("DoomBreath_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E6990: the owner's direction byte; the task to the side's
// centre; one orb (kind 1, 0x64) with +0x80 this task, +1 0, +9 0x18; row 26
// of the CLUT strip back with its STP bits; the actor's sprite (0x904B3C)
// tinted black, its tint slot to +0xA; sound 0x100; +0xB and +9 0, +1 on.
S31_EXPORT void __cdecl DoomBreath_Start(void) {
    {
        const unsigned char facing = Owner()[8];
        Sc()[8] = facing;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    const unsigned slot = NewTask(0x64);
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
    child[1] = 0;
    child[9] = 0x18;
    RestoreRow26Stp();
    MH_CALL(Sprite_ReleaseTint)(Pointer(kActorRecord));
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Pointer(kActorRecord), 0, 0, 0, 1);
    Sc()[0xA] = tint;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4E6A50: the tint record +0xA (MoveScript_TintRecords + 12 n)
// one step brighter in each of its three channels (+2..+4), +9 up; at 0x10 the
// actor sprite's +0x29 2 and +1 on.
S31_EXPORT void __cdecl DoomBreath_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Inc(s[9]);
    if (Sc()[9] != 0x10) return;
    Pointer(kActorRecord)[0x29] = 2;
    Inc(Sc()[1]);
}

// original 0x4E6AD0: the tint record one step darker, +9 down; at 0 the actor
// sprite's tint released, the actor flashed (BattleActor_Flash of the actor
// byte 0x904B34), +9 0x10, +1 on.
S31_EXPORT void __cdecl DoomBreath_Fade(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(kActorRecord));
    MH_CALL(BattleActor_Flash)(Mem(at::kActor)[0]);
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
}

// original 0x4E6B60: once the orb has ended (+0xB 0xFF): the actor sprite's
// +0x29 4, the target flagged 0x40, the effect's done flag, the task freed.
S31_EXPORT void __cdecl DoomBreath_End(void) {
    if (Sc()[0xB] != 0xFF) return;
    Pointer(kActorRecord)[0x29] = 4;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E6BA0: the orb's kind-1 task, a jmp through
// DoomBreathOrb_TaskTable (one entry) by +1, unchecked.
S31_EXPORT void __cdecl DoomBreathOrb_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("DoomBreathOrb_Task", phase, 1);
    magic_harness::Phase(bof3::addr::DoomBreathOrb_Run)();
}

// original 0x4E6BC0: a call through DoomBreathOrb_Steps (five entries) by +2;
// then while +0 and +2 are set: +0xA up past step 1, the orb's matrix, its
// stream and its glow, the matrix popped.
S31_EXPORT void __cdecl DoomBreathOrb_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::DoomBreathOrb_Launch, bof3::addr::DoomBreathOrb_Move,
                                                bof3::addr::DoomBreathOrb_Grow, bof3::addr::DoomBreathOrb_Shrink,
                                                bof3::addr::DoomBreathOrb_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("DoomBreathOrb_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    if (s[2] > 1) Inc(s[0xA]);
    Call0(bof3::addr::DoomBreathOrb_PushMatrix);
    Call0(bof3::addr::DoomBreathOrb_DrawStream);
    Call0(bof3::addr::DoomBreathOrb_DrawGlow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E6C00: +9 down; at 0 the orb starts: the owner's direction; the
// offset (-8, -15) << 16 turned by it, from the field's kind-2 point; the
// height the owner's + 0x800000; the step (0x8000, 0) turned; +0xB 0x1A, +9
// 0x2C, +0xA 0, +0x1C 0, +0x20 0x80, +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Launch(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    s[8] = Owner()[8];
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFF80000u));
    SetLong(Sc() + 0x10, static_cast<std::int32_t>(0xFFF10000u));
    Turn(Sc());
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0xC)) +
                                                    static_cast<std::uint32_t>(Field_Kind2X)));
    }
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0x10)) +
                                                    static_cast<std::uint32_t>(Field_Kind2Z)));
    }
    {
        const std::uint32_t height = static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0x800000u;
        SetLong(Sc() + 0x3C, static_cast<std::int32_t>(height));
    }
    SetLong(Sc() + 0xC, 0x8000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    Sc()[0xB] = 0x1A;
    Sc()[9] = 0x2C;
    Sc()[0xA] = 0;
    SetLong(Sc() + 0x1C, 0);
    SetLong(Sc() + 0x20, 0x80);
    Inc(Sc()[2]);
}

// original 0x4E6D00: the position +0x34 / +0x38 on by the step +0xC / +0x10,
// +9 down; at 0 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Move(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x34)) +
                                                static_cast<std::uint32_t>(Long(s + 0xC))));
    SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x38)) +
                                                static_cast<std::uint32_t>(Long(s + 0x10))));
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4E6D40: the angle +0xB down (mod 0x40), the width +0x1C up by 2;
// at 0x80 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Grow(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x3F);
    SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x1C)) + 2));
    if (Long(s + 0x1C) == 0x80) Inc(s[2]);
}

// original 0x4E6D70: the angle down (mod 0x40), the width +0x1C and the radius
// +0x20 down by 2; at a width of 0x20 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Shrink(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x3F);
    SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x1C)) - 2));
    SetLong(s + 0x20, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x20)) - 2));
    if (Long(s + 0x1C) == 0x20) Inc(s[2]);
}

// original 0x4E6DB0: the width down by 1, the radius by 2; at a width of 0
// the owner's +0xB 0xFF (DoomBreath_End's signal) and the task freed.
S31_EXPORT void __cdecl DoomBreathOrb_End(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x1C)) - 1));
    SetLong(s + 0x20, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x20)) - 2));
    if (Long(s + 0x1C) != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E6DF0: the orb's matrix pushed, turned about z by its direction
// +8 (0x400, 0x800, 0xC00 for 0, 1, 2; 0 for any other).
S31_EXPORT void __cdecl DoomBreathOrb_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const short turn = facing == 0 ? short{0x400} : facing == 1 ? short{0x800} : facing == 2 ? short{0xC00} : short{0};
    PushTurnedMatrix(s, turn);
}

// original 0x4E6EC0: the stream - 63 semi-transparent textured quads (tpage
// 0x340 / 0x100, CLUT row 0x1FA) up a sine of amplitude +0x20 whose phase is
// (+0xB + i) & 0x3F, each 0x40 long and 0xE00 wide, their shade 0x80 (at step
// 4 the width +0x1C / 2 x 8), the texture's v stepping with (+0xA + i) % 14;
// between two draw-mode packets.
S31_EXPORT void __cdecl DoomBreathOrb_DrawStream(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        SetSW(0, Word(s + 0x20));
        const unsigned angle = (s[0xB] & 0x3Fu) << 7;
        SetSW(2, angle);
        if (s[2] == 4) SetSW(4, static_cast<unsigned>(Shl(DivPow2(Long(s + 0x1C), 1), 3)));
        else SetSW(4, 0x80);
        SetVW(0x10, 0);
        SetVW(0x12, 0);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(0x14, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int i = 1; i < 0x40; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t y = VW(0x12), z = VW(0x14);
            const unsigned char* const s = Sc();
            SetVW(0, 0);
            SetVW(2, y);
            SetVW(4, z);
            SetVW(8, 0xE00);
            SetVW(0xA, y);
            SetVW(0xC, z);
            const unsigned angle = ((s[0xB] + static_cast<unsigned>(i)) & 0x3Fu) << 7;
            SetVW(0x10, 0);
            SetSW(2, angle);
            SetVW(0x12, static_cast<unsigned>(i) << 6);
            const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
            const int w = Mul12(v, SS(0));
            const std::uint16_t y2 = VW(0x12);
            SetVW(0x18, 0xE00);
            SetVW(0x14, static_cast<unsigned>(w));
            SetVW(0x1C, static_cast<unsigned>(w));
            p[4] = SB(4);
            SetVW(0x1A, y2);
            p[5] = SB(4);
            p[6] = SB(4);
        }
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
        SetWord(p + 0x26, tpage);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        SetWord(p + 0x16, clut);
        SetSW(6, ((Sc()[0xA] + static_cast<unsigned>(i)) % 14u << 4) + 8);
        p[0x14] = 8;
        p[0x24] = 0xF8;
        p[0x15] = SB(6);
        p[0x25] = SB(6);
        p[0x34] = 8;
        p[0x44] = 0xF8;
        p[0x35] = static_cast<unsigned char>(SB(6) + 0x10);
        p[0x45] = static_cast<unsigned char>(SB(6) + 0x10);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
    DrawModeCommit(0x95);
}

namespace {

// DoomBreathOrb_DrawGlow's shade: 0xC0, or at step 4 the width +0x1C / 2 x 12.
void GlowShade() {
    const unsigned char* const s = Sc();
    if (s[2] == 4) SetSW(4, (static_cast<std::uint32_t>(DivPow2(Long(s + 0x1C), 1)) * 3u) << 2);
    else SetSW(4, 0xC0);
}
// A quarter of the shade word, toward zero, as a byte.
unsigned char Quarter() { return static_cast<unsigned char>(DivPow2(SS(4), 2)); }

}  // namespace

// original 0x4E7130: the glow - two semi-transparent gouraud quads, 0xC00
// wide, 0x40 long, from y -0x80, up the same sine as the stream; the first
// bright at its far edge, the second at its near one (the shade and a quarter
// of it); between two draw-mode packets.
S31_EXPORT void __cdecl DoomBreathOrb_DrawGlow(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        if (s[2] == 4) SetSW(4, static_cast<unsigned>(Shl(DivPow2(Long(s + 0x1C), 1), 3)));
        else SetSW(4, 0x80);
        SetSW(0, Word(s + 0x20));
        const unsigned angle = (s[0xB] & 0x3Fu) << 7;
        SetVW(0x10, 0);
        SetSW(2, angle);
        SetVW(0x12, 0xFF80);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(0x14, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int i = 1; i < 3; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t y = VW(0x12), z = VW(0x14);
            SetVW(2, y);
            SetVW(0xA, y);
            const unsigned char* const s = Sc();
            SetVW(0, 0);
            SetVW(4, z);
            SetVW(8, 0xC00);
            SetVW(0xC, z);
            const unsigned angle = ((s[0xB] + static_cast<unsigned>(i)) & 0x3Fu) << 7;
            SetVW(0x10, 0);
            SetSW(2, angle);
            SetVW(0x12, (static_cast<unsigned>(i) << 6) - 0x40);
            const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
            const int w = Mul12(v, SS(0));
            const std::uint16_t y2 = VW(0x12);
            SetVW(0x18, 0xC00);
            SetVW(0x14, static_cast<unsigned>(w));
            SetVW(0x1C, static_cast<unsigned>(w));
            SetVW(0x1A, y2);
        }
        GlowShade();
        if (i == 1) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            p[0x24] = SB(4);
            p[0x25] = Quarter();
            p[0x26] = Quarter();
            p[0x34] = SB(4);
            p[0x35] = Quarter();
            p[0x36] = Quarter();
        } else {
            p[4] = SB(4);
            p[5] = Quarter();
            p[6] = Quarter();
            p[0x14] = SB(4);
            p[0x15] = Quarter();
            p[0x16] = Quarter();
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    DrawModeCommit(0x95);
}

// ===========================================================================
// MAGIC137 (row 101, Corona read one id down)

// original 0x4E7420: the kind-2 task. A three-entry stack table by +1:
// Corona_Start, Corona_Wait, Corona_End.
S31_EXPORT void __cdecl Corona_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Corona_Start, bof3::addr::Corona_Wait,
                                                 bof3::addr::Corona_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Corona_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E7420's Start, 0x4E7450: +0xB 0, +9 0x18, +1 on; the ray (kind
// 1, 0x4B, +1 0, the owner's direction) and the flash (the same, +1 1), each
// with +0x80 this task and counted in +0xB; row 26 of the CLUT strip back with
// its STP bits. In an event battle (0x904AAA) whose acting enemy's +0x100 is
// 0x29 (the actor byte 0x904B34 - 3, unchecked): a third child whose first
// 0x80 bytes are that enemy's record, +1 2, +2 0, +6 1, +5 0x4B, +9 0, +0x29 3.
S31_EXPORT void __cdecl Corona_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x4B);
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[8] = owner[8];
        Inc(s[0xB]);
    }
    {
        const unsigned slot = NewTask(0x4B);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 1;
        Inc(s[0xB]);
    }
    RestoreRow26Stp();
    if (Mem(kEventBattle)[0] == 0) return;
    if (EnemyRecord(ActorIndex())[0x100] != 0x29) return;
    const unsigned slot = NewTask(0x4B);
    unsigned char* const child = TaskSlot(slot);
    const unsigned char* const record = EnemyRecord(ActorIndex());
    // rep movsd: 0x20 dwords, forward, one at a time (the two can overlap: a
    // party actor's "record" lies among the task slots)
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(record + k));
    child[1] = 2;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x4B;
    child[9] = 0;
    child[0x29] = 3;
}

// original 0x4E75B0: +9 down; at 0 the target flags 0x20 and +1 on.
S31_EXPORT void __cdecl Corona_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x20);
    Inc(Sc()[1]);
}

// original 0x4E75F0: once both children have ended (+0xB 0): the target
// flagged 0x40, +9 0x1E, the effect's done flag, the task freed.
S31_EXPORT void __cdecl Corona_End(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0x1E;
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E7630: the children's kind-1 task, a jmp through
// CoronaChild_Kinds (three entries: the ray, the flash, the enemy copy) by +1,
// unchecked.
S31_EXPORT void __cdecl CoronaChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::CoronaRay_Run, bof3::addr::CoronaFlash_Run,
                                                bof3::addr::CoronaEnemy_Task};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("CoronaChild_Task", phase, 3);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4E7650: a call through CoronaRay_Steps (four entries: _Start,
// MAGIC067's 0x4B6B20 and 0x4B6B70, _Advance) by +2; then while +0 is set the
// ray's matrix, the ray, the matrix popped.
S31_EXPORT void __cdecl CoronaRay_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::CoronaRay_Start, bof3::addr::ChillRay_Grow,
                                                bof3::addr::ChillRay_Shrink, bof3::addr::CoronaRay_Advance};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("CoronaRay_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::CoronaRay_PushMatrix);
    Call0(bof3::addr::CoronaRay_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E7680: the offset (-0x28000, 0) turned by the direction, from
// the field's kind-2 point; the height the ground there + 0x200; sound effect
// 0x100; +0x14 0x2FFF, +0xB 0x34, +9 0, +0xA 0x10, +2 on.
S31_EXPORT void __cdecl CoronaRay_Start(void) {
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFFD8000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0xC)) +
                                                    static_cast<std::uint32_t>(Field_Kind2X)));
    }
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0x10)) +
                                                    static_cast<std::uint32_t>(Field_Kind2Z)));
    }
    const long ground = MH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
    SetWord(Sc() + 0x3E, static_cast<unsigned>(static_cast<std::uint32_t>(ground) + 0x200u) & 0xFFFF);
    MH_CALL(Sound_PlayEffect)(0x100);
    SetLong(Sc() + 0x14, 0x2FFF);
    Sc()[0xB] = 0x34;
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E7730: the angle +0xB on by (+9 >> 6) + 1; on odd frames the
// size +0xA down; the step (0x2000, 0) turned by the direction and added to
// the position; +9 down by 2; at 0x30 the owner's count +0xB down and the
// task freed.
S31_EXPORT void __cdecl CoronaRay_Advance(void) {
    {
        unsigned char* const s = Sc();
        AddB(s[0xB], static_cast<unsigned>(s[9] >> 6) + 1);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) Dec(Sc()[0xA]);
    SetLong(Sc() + 0xC, 0x2000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x34)) +
                                                    static_cast<std::uint32_t>(Long(s + 0xC))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x38)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x10))));
    }
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] != 0x30) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E77D0: the ray's matrix pushed, turned about z by its direction
// +8 through a four-entry jump table (0, 0x400, 0x800, 0xC00). Past 3 the
// original leaves the angle an uninitialised stack word; ours aborts.
S31_EXPORT void __cdecl CoronaRay_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned facing = s[8];
    if (facing > 3) bof3::Fatal("CoronaRay_PushMatrix: facing %u, past the 4-entry jump table", facing);
    PushTurnedMatrix(s, static_cast<short>(facing << 10));
}

namespace {

// One point pair of the ray's fan: (sin, cos) of 0x580 and of 0x280 at the
// radius SS(4), and the height sin(SS(8)) x SS(6), into the vertex pair at k
// (0 or 8, x / y / z).
void RayPoint(unsigned k, int angle) {
    int v = MH_CALL(Math_Sin)(angle);
    SetVW(k, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(angle);
    SetVW(k + 2, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Sin)(SS(8));
    SetVW(k + 4, static_cast<unsigned>(Mul12(v, SS(6))));
}

}  // namespace

// original 0x4E78B0: the ray - +9 - 1 semi-transparent gouraud-textured quads
// (tpage 0x340 / 0x100, CLUT row 0x1FA) between the arcs at 0x580 and 0x280 of
// radius 0x80 + 0x20 i, each lifted by sin(((+0xB + i) & 0x3F) << 6) x +0xA;
// the shade 0x80 (at step 3 (+9 - 0x30) / 3 x 8) ramps up over the first 16
// and down over the last 8; the texture's u scrolls with the frame counter.
// Between two draw-mode packets.
S31_EXPORT void __cdecl CoronaRay_Draw(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        SetSW(2, 0x80);
        SetSW(0, s[9]);
        SetSW(4, 0x80);
        SetSW(6, s[0xA]);
        SetSW(8, (s[0xB] & 0x3Fu) << 6);
    }
    RayPoint(0, 0x580);
    RayPoint(8, 0x280);
    for (int i = 1; SS(0) > i; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned char b = Sc()[0xB];
            SetSW(4, static_cast<unsigned>(i + 4) << 5);
            const std::uint16_t v0 = VW(0), v2 = VW(2), v4 = VW(4), v8 = VW(8), va = VW(0xA), vc = VW(0xC);
            SetVW(0x10, v0);
            SetSW(8, ((b + static_cast<unsigned>(i)) & 0x3Fu) << 6);
            SetVW(0x12, v2);
            SetVW(0x14, v4);
            SetVW(0x18, v8);
            SetVW(0x1A, va);
            SetVW(0x1C, vc);
        }
        RayPoint(0, 0x580);
        RayPoint(8, 0x280);
        {
            const unsigned char* const s = Sc();
            if (s[2] == 3) {
                const int d = static_cast<int>(s[9]) - 0x30;
                SetSW(2, static_cast<unsigned>(Shl(d / 3, 3)));
            }
        }
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(2);
        if (i < 0x10) {
            const auto up = static_cast<unsigned char>(i + 1);
            const auto at = static_cast<unsigned char>(i);
            for (unsigned k : {4u, 5u, 6u}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * up);
            p[0x18] = p[4];
            p[0x19] = p[5];
            p[0x1A] = p[6];
            for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * at);
            p[0x40] = p[0x2C];
            p[0x41] = p[0x2D];
            p[0x42] = p[0x2E];
        }
        if (SS(0) - 8 < i) {
            SetSW(0xA, SW(2));
            const std::uint32_t near = static_cast<std::uint32_t>(DivPow2(SS(0xA), 3)) *
                                       (static_cast<std::uint32_t>(SD(0)) - static_cast<std::uint32_t>(i));
            SetSW(2, near);
            p[4] = static_cast<unsigned char>(near);
            for (unsigned k : {5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = SB(2);
            const std::uint32_t far = (static_cast<std::uint32_t>(SD(0)) - static_cast<std::uint32_t>(i) + 1) *
                                      static_cast<std::uint32_t>(DivPow2(SS(0xA), 3));
            SetSW(2, far);
            p[0x2C] = static_cast<unsigned char>(far);
            for (unsigned k : {0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(2);
        }
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
        SetWord(p + 0x2A, tpage);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        SetWord(p + 0x16, clut);
        {
            const std::uint32_t scroll = ~(static_cast<std::uint32_t>(Frame_Counter) * 2u) + static_cast<std::uint32_t>(i);
            const std::uint32_t u = (0x3Fu - (scroll & 0x3Fu)) << 2;
            SetSW(0xE, u);
            if ((u & 0xFFFF) == 0xFC) SetSW(0xE, 0xFB);
        }
        p[0x14] = 0;
        p[0x15] = SB(0xE);
        p[0x28] = 0xFF;
        p[0x3C] = 0;
        p[0x29] = SB(0xE);
        p[0x3D] = static_cast<unsigned char>(SB(0xE) + 4);
        p[0x50] = 0xFF;
        p[0x51] = static_cast<unsigned char>(SB(0xE) + 4);
        Rtp4(p, 0x14);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x54);
    }
    DrawModeCommit(0x15);
}

// original 0x4E7E30: a call through CoronaFlash_Steps (four entries: _Start,
// MAGIC086's BarrierRing_Grow, MAGIC078's MagicFx_WaitA, MAGIC060's 0x4B1740)
// by +2; then while +0 is set, the flash (a tail jmp).
S31_EXPORT void __cdecl CoronaFlash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::CoronaFlash_Start, bof3::addr::BarrierRing_Grow,
                                                bof3::addr::MagicFx_WaitA, bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("CoronaFlash_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::CoronaFlash_Draw);
}

// original 0x4E7E60: +9 0, +0xA 0x88, +2 on.
S31_EXPORT void __cdecl CoronaFlash_Start(void) {
    Sc()[9] = 0;
    Sc()[0xA] = 0x88;
    Inc(Sc()[2]);
}

// original 0x4E7E80: the flash - one semi-transparent gouraud quad over the
// whole 320 x 240 screen, its bright edge on the side the owner's direction
// +8 faces (bit 0), (+9, +9, +9) there and (+9 x 12, +9 x 4, +9 x 4) at the
// other; between two draw-mode packets.
S31_EXPORT void __cdecl CoronaFlash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    constexpr std::int32_t kRight = 0x439F8000;    // 319.0f
    constexpr std::int32_t kBottom = 0x436F0000;   // 239.0f
    if (Owner()[8] & 1) {
        SetLong(p + 8, 0);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, kRight);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, 0);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, kRight);
    } else {
        SetLong(p + 8, kRight);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, 0);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, kRight);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, 0);
    }
    SetLong(p + 0x3C, kBottom);
    {
        const unsigned char a = Sc()[9];
        for (unsigned k : {0x36u, 6u, 0x35u, 5u, 0x34u, 4u}) p[k] = a;
    }
    {
        const auto b = static_cast<unsigned char>(Sc()[9] * 12);
        p[0x24] = b;
        p[0x14] = b;
    }
    {
        const auto c = static_cast<unsigned char>(Sc()[9] << 2);
        for (unsigned k : {0x26u, 0x25u, 0x16u, 0x15u}) p[k] = c;
    }
    MH_CALL(Gfx_CommitPrim)(2, 0x44);
    DrawModeCommit(0x15);
}

// original 0x4E7F80: the enemy copy's task. A three-entry stack table by +2:
// CoronaEnemy_Start, the engine's 0x43EC10 (the script until the done flag),
// BattleFx_FreeTask.
S31_EXPORT void __cdecl CoronaEnemy_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::CoronaEnemy_Start, kScriptUntilDone,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("CoronaEnemy_Task", phase, 3);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4E7FB0: +0xB and +9 0, the copy's animation 3, its script ticked
// once, the sprite queued; +2 on.
S31_EXPORT void __cdecl CoronaEnemy_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    MH_CALL(Sprite_SetAnimation)(3);
    MH_CALL(Sprite_ScriptTick)();
    MH_CALL(Sprite_QueueOverlay)();
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC138 (row 118, Main Cannon read one id down)

// original 0x4E7FF0: the kind-2 task. A three-entry stack table by +1:
// MainCannon_Start, MainCannon_Fire, MainCannon_End.
S31_EXPORT void __cdecl MainCannon_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::MainCannon_Start, bof3::addr::MainCannon_Fire,
                                                 bof3::addr::MainCannon_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("MainCannon_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

namespace {

// The owner's script ticked with Sprite_Current the owner, Sprite_Current
// put back to the task it was (the originals keep it in a register).
void TickOwner(unsigned char* self) {
    Sprite_Current = Owner();
    MH_CALL(Sprite_ScriptTick)();
    Sprite_Current = self;
}

}  // namespace

// original 0x4E8020: the screen point (0xF0, 0xFA); the first 16 words of CLUT
// row 26 back from the source (no STP bit); +0xB and +9 0; the owner's script
// ticked; +1 on.
S31_EXPORT void __cdecl MainCannon_Start(void) {
    SetWord(Sc() + 0x2E, 0xF0);
    SetWord(Sc() + 0x30, 0xFA);
    for (unsigned k = 0x1A00; k < 0x1A10; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    unsigned char* const self = Sc();
    TickOwner(self);
    Inc(self[1]);
}

// original 0x4E80A0: on every eighth frame: a shell (kind 1, 0x56, +1 0, +2 0)
// at the screen point (0xDC, 0xFA), 0x14 further right for an odd +9, with
// +0xA the shell's number +9 and +0x80 this task, +9 and +0xB up - none when
// no slot is free (0xFF); at +9 6 +1 on; then the owner's script ticked.
S31_EXPORT void __cdecl MainCannon_Fire(void) {
    if (Frame_Counter & 7) return;
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x56) & 0xFFu;
    if (slot != 0xFF) {
        unsigned char* const t = TaskSlot(slot);
        t[1] = 0;
        t[2] = 0;
        unsigned char* const s = Sc();
        SetWord(t + 0x2E, 0xDC);
        SetWord(t + 0x30, 0xFA);
        if (s[9] & 1) SetWord(t + 0x2E, Word(t + 0x2E) + 0x14u);
        t[0xA] = s[9];
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
        Inc(s[9]);
        Inc(Sc()[0xB]);
    }
    unsigned char* const s = Sc();
    if (s[9] == 6) Inc(s[1]);
    TickOwner(Sc());
}

// original 0x4E8160: the owner's script ticked; once every shell has ended
// (+0xB 0): the target flagged 0x40, the effect's done flag, the task freed.
S31_EXPORT void __cdecl MainCannon_End(void) {
    unsigned char* const self = Sc();
    TickOwner(self);
    if (self[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E81A0: the shells' kind-1 task, a jmp through
// MainCannonChild_Kinds (two entries: the shell, the blast) by +1, unchecked.
S31_EXPORT void __cdecl MainCannonChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::MainCannonShell_Run, bof3::addr::MainCannonBlast_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MainCannonChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4E81C0: with the frame-offset table 0x9039D8 the effects'
// (0x8E3580), a three-entry stack table by +2 (_Aim, _Fly, BattleFx_FreeTask);
// the sprite queued while bit 0 of +0 is set; the battle's table (0x8B3580)
// back.
S31_EXPORT void __cdecl MainCannonShell_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MainCannonShell_Aim, bof3::addr::MainCannonShell_Fly,
                                                bof3::addr::BattleFx_FreeTask};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MainCannonShell_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] & 1) MH_CALL(Sprite_QueueOverlay)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

namespace {

// (d << 4) / 8, toward zero.
std::int32_t Step8(int d) { return DivPow2(Shl(d, 4), 3); }

}  // namespace

// original 0x4E8220: the shell's step toward the acting actor (0x904B34: a
// party member's record at 0..2, else the enemy's by index - 3, unchecked):
// x (the actor's +0x2E - the shell's + 0x28, or + 0x14 for an even owner +9)
// x 2, y (the actor's +0x30 - the shell's) x 2, both in sixteenths; the
// position +0x18 / +0x1C in sixteenths; its sprite fields (+0x24..+0x2D,
// +0x48, +0x5C..+0x5F); animation (target - 1) & 1; +9 3, +2 on.
S31_EXPORT void __cdecl MainCannonShell_Aim(void) {
    const unsigned char actor = static_cast<unsigned char>(Long(Mem(at::kActor)));
    const unsigned char odd = Owner()[9] & 1;
    int ay;
    if (actor <= 2) {
        const int ax = S16(PartyRecord(actor) + 0x2E);
        unsigned char* const s = Sc();
        SetLong(s + 0xC, Step8(ax - S16(s + 0x2E) + (odd ? 0x28 : 0x14)));
        ay = S16(PartyRecord(ActorIndex()) + 0x30);
    } else {
        unsigned char* const s = Sc();
        const int ax = S16(EnemyRecord(actor) + 0x2E);
        SetLong(s + 0xC, Step8(ax - S16(s + 0x2E) + (odd ? 0x28 : 0x14)));
        ay = S16(EnemyRecord(ActorIndex()) + 0x30);
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Step8(ay - S16(s + 0x30)));
    }
    SetLong(Sc() + 0x18, Shl(S16(Sc() + 0x2E), 4));
    SetLong(Sc() + 0x1C, Shl(S16(Sc() + 0x30), 4));
    Sc()[0x25] = 0x1E;
    Sc()[0x26] = 0;
    Sc()[0x48] = 1;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 3;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>((TargetByte() - 1) & 1));
    Sc()[9] = 3;
    Inc(Sc()[2]);
}

// original 0x4E8440: the screen point from the position (sixteenths), the
// position on by the step; until the shell is within 0x14 (a party actor) or
// 0x28 (an enemy) above the actor's +0x30: a blast (kind 1, 0x56, +1 1, +2 0)
// at the shell's point with its +0xA and the owner as its +0x80, the shell's
// +2 on. Then the shell's script ticked.
S31_EXPORT void __cdecl MainCannonShell_Fly(void) {
    SetWord(Sc() + 0x2E, static_cast<unsigned>(Long(Sc() + 0x18) >> 4) & 0xFFFF);
    SetWord(Sc() + 0x30, static_cast<unsigned>(Long(Sc() + 0x1C) >> 4) & 0xFFFF);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x18, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x18)) +
                                                    static_cast<std::uint32_t>(Long(s + 0xC))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x1C)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x10))));
    }
    const unsigned char actor = static_cast<unsigned char>(Long(Mem(at::kActor)));
    const int limit = actor <= 2 ? S16(PartyRecord(actor) + 0x30) - 0x14 : S16(EnemyRecord(actor) + 0x30) - 0x28;
    if (S16(Sc() + 0x30) < limit) {
        const unsigned slot = NewTask(0x56);
        unsigned char* const t = TaskSlot(slot);
        unsigned char* const s = Sc();
        t[1] = 1;
        t[2] = 0;
        SetWord(t + 0x2E, Word(s + 0x2E));
        SetWord(t + 0x30, Word(s + 0x30));
        t[0xA] = s[0xA];
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(Owner())));
        Inc(s[2]);
    }
    MH_CALL(Sprite_ScriptTick)();
}

// original 0x4E8550: with the frame-offset table 0x9039D8 the blast's
// (0x8C5D80), a three-entry stack table by +2 (_Start, _Play, MAGIC058's
// 0x4AF490); the sprite's screen update while +0 is set; the battle's table
// back.
S31_EXPORT void __cdecl MainCannonBlast_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MainCannonBlast_Start, bof3::addr::MainCannonBlast_Play,
                                                bof3::addr::MagicFx_UncountAndFree};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBlast));
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MainCannonBlast_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4E85B0: the blast's sprite fields (+0x24..+0x2D), animation 5,
// sound effect 0x100 + (+0xA & 1); +2 on.
S31_EXPORT void __cdecl MainCannonBlast_Start(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x27] = 0xB4;
    Sc()[0x28] = 0;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    MH_CALL(Sprite_SetAnimation)(5);
    MH_CALL(Sound_PlayEffect)(static_cast<unsigned short>((Sc()[0xA] & 1u) + 0x100));
    Inc(Sc()[2]);
}

// original 0x4E8640: the script ticked; at its end +2 on, else the sprite
// queued (a tail jmp).
S31_EXPORT void __cdecl MainCannonBlast_Play(void) {
    if (MH_CALL(Sprite_ScriptTick)() != 0) {
        Inc(Sc()[2]);
        return;
    }
    MH_CALL(Sprite_QueueOverlay)();
}

// ===========================================================================
// MAGIC143 (row 66, Thunder Clap read one id down)

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
    const unsigned char* const source = Pointer(at::kSource);
    const unsigned slot = NewTask(0x35);
    unsigned char* const t = TaskSlot(slot);
    unsigned char* const s = Sc();
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
    t[1] = 0;
    SetLong(t + 0x34, Long(source + 0x34));
    SetLong(t + 0x38, Long(source + 0x38));
    SetLong(t + 0x3C, Long(source + 0x3C));
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
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + 0x10u);
    Call0(bof3::addr::ThunderClapBolt_DrawFlash);
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) - 0x20u);
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
    MH_CALL(Sprite_ReleaseTint)(Pointer(at::kSource));
    MH_CALL(Sprite_SetTint)(Pointer(at::kSource), 0xF8, 0xF8, 0xF8, 1);
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
    MH_CALL(Sprite_ReleaseTint)(Pointer(at::kSource));
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

void MagicS31_Inject() {
    if (bof3::WantsShadow("magic_s31")) magic_s31::SelfTest();
    BOF3_INJECT(DoomBreath_Task);
    BOF3_INJECT(DoomBreath_Start);
    BOF3_INJECT(DoomBreath_Brighten);
    BOF3_INJECT(DoomBreath_Fade);
    BOF3_INJECT(DoomBreath_End);
    BOF3_INJECT(DoomBreathOrb_Task);
    BOF3_INJECT(DoomBreathOrb_Run);
    BOF3_INJECT(DoomBreathOrb_Launch);
    BOF3_INJECT(DoomBreathOrb_Move);
    BOF3_INJECT(DoomBreathOrb_Grow);
    BOF3_INJECT(DoomBreathOrb_Shrink);
    BOF3_INJECT(DoomBreathOrb_End);
    BOF3_INJECT(DoomBreathOrb_PushMatrix);
    BOF3_INJECT(DoomBreathOrb_DrawStream);
    BOF3_INJECT(DoomBreathOrb_DrawGlow);
    BOF3_INJECT(Corona_Task);
    BOF3_INJECT(Corona_Start);
    BOF3_INJECT(Corona_Wait);
    BOF3_INJECT(Corona_End);
    BOF3_INJECT(CoronaChild_Task);
    BOF3_INJECT(CoronaRay_Run);
    BOF3_INJECT(CoronaRay_Start);
    BOF3_INJECT(CoronaRay_Advance);
    BOF3_INJECT(CoronaRay_PushMatrix);
    BOF3_INJECT(CoronaRay_Draw);
    BOF3_INJECT(CoronaFlash_Run);
    BOF3_INJECT(CoronaFlash_Start);
    BOF3_INJECT(CoronaFlash_Draw);
    BOF3_INJECT(CoronaEnemy_Task);
    BOF3_INJECT(CoronaEnemy_Start);
    BOF3_INJECT(MainCannon_Task);
    BOF3_INJECT(MainCannon_Start);
    BOF3_INJECT(MainCannon_Fire);
    BOF3_INJECT(MainCannon_End);
    BOF3_INJECT(MainCannonChild_Task);
    BOF3_INJECT(MainCannonShell_Run);
    BOF3_INJECT(MainCannonShell_Aim);
    BOF3_INJECT(MainCannonShell_Fly);
    BOF3_INJECT(MainCannonBlast_Run);
    BOF3_INJECT(MainCannonBlast_Start);
    BOF3_INJECT(MainCannonBlast_Play);
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
