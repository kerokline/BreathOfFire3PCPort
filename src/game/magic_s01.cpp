// One spell overlay compiled into the exe, round nine group S01
// (docs/magic_s01.md): the PSX's MAGIC001.EMI, Magic_Rows rows 1 and 105,
// 0x498FE0..0x499D74. Read one id down (docs/cut-content.md section 2) the
// sibling labels the two rows Nue Stomp and Jump; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - row 1 (NueStomp_*): the caster's record copied into one child (kind 1,
//     parameter 0x49) that plays the acting ENEMY's animations 0xA, 0xB and
//     0xC through the engine's 0x435A70 (an enemy by battle index - 3): it
//     rises, drops onto the source sprite's place, stomps three times (sound
//     0x203, the target flagged 0x10 at the first), flags the target 0x40,
//     fades out its tint and returns to the caster;
//   - row 105 (Jump_*): the caster's animation 8 (BattleActor_SetAnimation),
//     the caster's record copied into one child (kind 1, 0x4F) that rises,
//     lands over the source sprite, hovers while its shadow (a disc of 16
//     gouraud triangles, radius +0xA x 8) grows, turns toward the caster by
//     Math_Ratan2 at the hit (target flagged 0x40), flies back along that
//     heading and fades;
//   - both tasks wait for the child (+0xB), restore the effect CLUT row and
//     clear the owner's 0x40 bit (Magic001_EndWhenChildDone), then the done
//     flag and free (MagicFx_DoneAndFree).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s01.h"

#include <cstdint>
#include <cstring>

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

// The scratch the draw keeps its radius in (DamageScratch's words +0 and +2)
// and the three SVECTORs of Prim_VertexScratch it projects; both read again
// after every call, as the original reads them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }

short SS(unsigned k) { return static_cast<short>(Word(Mem(kS + k))); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
// A dword added in place, wrapping.
void AddL(unsigned char* a, std::uint32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + v));
}
// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl 0xD` then `sar 0xC` on a dword: twice the value, its top bit lost.
std::uint32_t Twice(int v) { return static_cast<std::uint32_t>(static_cast<int>(static_cast<std::uint32_t>(v) << 13) >> 12); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The engine's 0x435A70 (in no group, unnamed): BattleEnemy_SetAnimation(anim)
// on the enemy of battle index `actor` (0x93B960 + 0x128 (actor - 3),
// unchecked), 0x939AD8 put back after. Row 1 hands it the acting actor.
constexpr std::uint32_t kEnemyAnimation = bof3::addr::BattleEnemy_SetAnimationOf;   // 0x435A70, R3A's (round 14)
using EnemyAnimationFn = void (__cdecl*)(unsigned, unsigned);
void EnemyAnimation(unsigned anim) { MH_AT(EnemyAnimationFn, kEnemyAnimation)(ActorByte(), anim); }

// The phase handlers of other units the children's tables hold
// (docs/magic_s01.md section 3) are ours now and named in the tables: S04's
// AirRaidImage_FadeOut, S10's SacrificeActor_Play.

// This group's functions called directly, as the original calls them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Gte_RotTransPers3 as the original pushes it: one pointer more than
// symbols.toml's prototype carries (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

// The shared start of both rows' tasks (0x499010 / 0x4995E0 after their
// first differences): a child of kind 1 and `parameter`, its first 0x80
// bytes the acting actor's record (a member's at 0x802D40 + 0x14C i below 3,
// else the enemy's at 0x93B960 + 0x128 (i - 3), unchecked), +0x80 this task,
// +1 / +2 / +9 0, +6 1, +5 the parameter. The slot is used as
// BattleTask_Create answers it, unchecked (0xFF, none free, writes past the
// pool: docs/magic_s01.md section 7). The copy is rep movsd's, a dword at a
// time forward.
void SpawnCopy(unsigned parameter) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu;
    const unsigned actor = ActorByte();
    const unsigned char* const from =
        actor < 3 ? Mem(at::kParty + actor * at::kPartyStride)
                  : Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(actor) - 3) * at::kEnemyStride);
    unsigned char* const task = Mem(at::kTasks + slot * at::kTaskStride);
    unsigned char* const self = Sc();
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(task + k, Long(from + k));
    SetLong(task + 0x80, static_cast<std::int32_t>(Key(self)));
    task[1] = 0;
    task[2] = 0;
    task[6] = 1;
    task[5] = static_cast<unsigned char>(parameter);
    task[9] = 0;
}

// The owner's direction byte and position (+0x34..+0x3C) to this task.
void TakeOwnerPlace() {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

// The effect CLUT row from the owner's sprite (its answer to +0x27), the
// owner's +0x28 and +0x24, the row's STP bits set.
void TakeOwnerClut() {
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
}

// The children's motion: the vertical speed +0x14 by its step +0x20, the
// height word +0x3E by the speed's low word.
void Rise() {
    unsigned char* s = Sc();
    AddL(s + 0x14, static_cast<std::uint32_t>(Long(s + 0x20)));
    s = Sc();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
}

// At the top of the rise: over the source sprite (+0x34, +0x38), the height
// +0x3C moved by the source's less the owner's.
void OverSource(unsigned char* s) {
    SetLong(s + 0x34, Long(Source() + 0x34));
    SetLong(Sc() + 0x38, Long(Source() + 0x38));
    const std::uint32_t d = static_cast<std::uint32_t>(Long(Source() + 0x3C)) - static_cast<std::uint32_t>(Long(Owner() + 0x3C));
    AddL(Sc() + 0x3C, d);
}

// The tint +0x5D..+0x5F down by 0x10 a frame until +0x5D is 0x80; then back
// at the owner's position (+0x34..+0x3C), +2 on. `shrink`: the shadow +0xA
// down by 2 while not 0 (Jump's).
void FadeHome(bool shrink) {
    unsigned char* s = Sc();
    const unsigned char tint = s[0x5D];
    if (tint != 0x80) {
        s[0x5D] = static_cast<unsigned char>(tint - 0x10);
        AddB(Sc()[0x5E], 0xF0);
        AddB(Sc()[0x5F], 0xF0);
        s = Sc();
    }
    if (shrink && s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
        s = Sc();
    }
    if (s[0x5D] != 0x80) return;
    SetLong(s + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Inc(Sc()[2]);
}

// The sprite script ticked three times; the last answer counts.
unsigned char TickThrice() {
    MH_CALL(Sprite_ScriptTickOnce)();
    MH_CALL(Sprite_ScriptTickOnce)();
    return MH_CALL(Sprite_ScriptTickOnce)();
}

// One MATRIX block as the original lays it out on its stack, RotTrans
// writing its translation at +0x14 (DIV-0021's padding word at +0x12).
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

#define S01_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Row 1 (Nue Stomp read one id down)

// original 0x498FE0: the kind-2 task. A three-entry stack table by +1:
// NueStomp_Start, Magic001_EndWhenChildDone, MagicFx_DoneAndFree.
S01_EXPORT void __cdecl NueStomp_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::NueStomp_Start, bof3::addr::Magic001_EndWhenChildDone,
                                                 bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("NueStomp_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x499010: the owner's direction and position; +0xB 1 (one child);
// the child (kind 1, 0x49) as a copy of the actor's record; the effect CLUT
// row from the owner; the owner's +0 bit 0x40 set; +1 on.
S01_EXPORT void __cdecl NueStomp_Start(void) {
    TakeOwnerPlace();
    Sc()[0xB] = 1;
    SpawnCopy(0x49);
    TakeOwnerClut();
    Owner()[0] |= 0x40;
    Inc(Sc()[1]);
}

// original 0x499170: the child's kind-1 task, a jmp through
// Magic001_ChildTaskTable (two entries: NueStompChild_Run, JumpChild_Run) by
// +1, unchecked.
S01_EXPORT void __cdecl NueStompChild_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::NueStompChild_Run, bof3::addr::JumpChild_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("NueStompChild_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x499190: an eleven-entry stack table by +2; then while +0 and +2
// are set, the sprite's screen point (Sprite_UpdateScreen).
S01_EXPORT void __cdecl NueStompChild_Run(void) {
    static constexpr std::uint32_t kSteps[11] = {
        bof3::addr::NueStompChild_Begin,  bof3::addr::NueStompChild_Leap,   bof3::addr::NueStompChild_Rise,
        bof3::addr::NueStompChild_Drop,   bof3::addr::NueStompChild_Crouch, bof3::addr::NueStompChild_Bounce,
        bof3::addr::NueStompChild_Hop,    bof3::addr::NueStompChild_Stomp,  bof3::addr::NueStompChild_Land,
        bof3::addr::NueStompChild_Return, bof3::addr::AirRaidImage_FadeOut};
    const unsigned phase = Sc()[2];
    if (phase >= 11) PastTable("NueStompChild_Run", phase, 11);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x499220: the acting enemy's animation 0xA; the owner's CLUT row
// byte +0x27; +9 and +0xA 0; +2 on.
S01_EXPORT void __cdecl NueStompChild_Begin(void) {
    EnemyAnimation(0xA);
    Sc()[0x27] = Owner()[0x27];
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x499260: at the script's end: BattleActor_PlaySound(2, 0), the
// enemy's animation 0xB, the speed +0x14 0x100 with the step -8, +2 on.
S01_EXPORT void __cdecl NueStompChild_Leap(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    EnemyAnimation(0xB);
    SetLong(Sc() + 0x14, 0x100);
    SetLong(Sc() + 0x20, -8);
    Inc(Sc()[2]);
}

// original 0x4992B0: rising; when the speed reaches 0, over the source
// sprite, +9 0x1F, +2 on.
S01_EXPORT void __cdecl NueStompChild_Rise(void) {
    Rise();
    unsigned char* const s = Sc();
    if (Long(s + 0x14) != 0) return;
    OverSource(s);
    Sc()[9] = 0x1F;
    Inc(Sc()[2]);
}

// original 0x499330: falling; +9 down; at 0 sound 0x203, the enemy's
// animation 0xC, the target flagged 0x10, +9 3 (the stomps), +2 on.
S01_EXPORT void __cdecl NueStompChild_Drop(void) {
    Rise();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x203);
    EnemyAnimation(0xC);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Sc()[9] = 3;
    Inc(Sc()[2]);
}

// original 0x4993B0: at the script's end the enemy's animation 0xA, +2 on.
S01_EXPORT void __cdecl NueStompChild_Crouch(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    EnemyAnimation(0xA);
    Inc(Sc()[2]);
}

// original 0x4993E0: the script ticked three times; at its end
// BattleActor_PlaySound(2, 0), the enemy's animation 0xB, the speed 0x140
// with the step -0x40, +0xA 9, +2 on.
S01_EXPORT void __cdecl NueStompChild_Bounce(void) {
    if (TickThrice() == 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    EnemyAnimation(0xB);
    SetLong(Sc() + 0x14, 0x140);
    SetLong(Sc() + 0x20, -0x40);
    Sc()[0xA] = 9;
    Inc(Sc()[2]);
}

// original 0x499440: moving; +0xA down; at 0 +2 on.
S01_EXPORT void __cdecl NueStompChild_Hop(void) {
    Rise();
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Inc(s[2]);
}

// original 0x499480: the script ticked three times; at its end sound 0x203,
// the enemy's animation 0xC, +9 down: at 0 +2 on, else +2 back three (to
// NueStompChild_Crouch).
S01_EXPORT void __cdecl NueStompChild_Stomp(void) {
    if (TickThrice() == 0) return;
    MH_CALL(Sound_PlayById)(0x203);
    EnemyAnimation(0xC);
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0)
        AddB(s[2], 0xFD);
    else
        Inc(s[2]);
}

// original 0x4994E0: at the script's end the target flagged 0x40, +0 bit
// 0x20, the tint +0x5C 1 and +0x5D..+0x5F 0, +2 on.
S01_EXPORT void __cdecl NueStompChild_Land(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Inc(Sc()[2]);
}

// original 0x499540: the tint faded to 0x80, then home (FadeHome).
S01_EXPORT void __cdecl NueStompChild_Return(void) { FadeHome(false); }

// ===========================================================================
// Row 105 (Jump read one id down)

// original 0x4995B0: the kind-2 task. A three-entry stack table by +1:
// Jump_Start, Magic001_EndWhenChildDone, MagicFx_DoneAndFree.
S01_EXPORT void __cdecl Jump_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Jump_Start, bof3::addr::Magic001_EndWhenChildDone,
                                                 bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Jump_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4995E0: as NueStomp_Start, with the caster's animation 8
// (BattleActor_SetAnimation(8, 0)) before the child (kind 1, 0x4F), and the
// task's CLUT entry 31 cleared after the STP bits.
S01_EXPORT void __cdecl Jump_Start(void) {
    TakeOwnerPlace();
    Sc()[0xB] = 1;
    MH_CALL(BattleActor_SetAnimation)(8, 0);
    SpawnCopy(0x4F);
    TakeOwnerClut();
    MH_CALL(SpriteClut_ClearEntry31)(Sc());
    Owner()[0] |= 0x40;
    Inc(Sc()[1]);
}

// original 0x499750 (both rows): once the child has ended (+0xB 0), the effect
// CLUT row restored, the owner's +0 bit 0x40 cleared, +1 on.
S01_EXPORT void __cdecl Magic001_EndWhenChildDone(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x499780: the child's kind-1 task, a jmp through
// Magic001_ChildTaskTable + 4 (one entry: JumpChild_Run) by +1, unchecked.
S01_EXPORT void __cdecl JumpChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("JumpChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::JumpChild_Run)();
}

// original 0x4997A0: a seven-entry stack table by +2; then while +0 and +2
// are set, the sprite's screen point, and below step 5 the shadow: its matrix,
// its disc, the matrix popped.
S01_EXPORT void __cdecl JumpChild_Run(void) {
    static constexpr std::uint32_t kSteps[7] = {bof3::addr::JumpChild_Begin,     bof3::addr::JumpChild_Rise,
                                                bof3::addr::JumpChild_Hover,     bof3::addr::JumpChild_Return,
                                                bof3::addr::SacrificeActor_Play, bof3::addr::JumpChild_Fade,
                                                bof3::addr::AirRaidImage_FadeOut};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("JumpChild_Run", phase, 7);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
    if (Sc()[2] >= 5) return;
    Call0(bof3::addr::JumpChild_PushMatrix);
    Call0(bof3::addr::JumpChild_DrawShadow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x499820: the owner's CLUT row byte +0x27; the speed 0x100 with the
// step -0x10; +9 0, the shadow +0xA 0x10; +2 on.
S01_EXPORT void __cdecl JumpChild_Begin(void) {
    Sc()[0x27] = Owner()[0x27];
    SetLong(Sc() + 0x14, 0x100);
    SetLong(Sc() + 0x20, -0x10);
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x499870: rising, the shadow +0xA down while not 0; when the speed
// reaches 0, over the source sprite, +9 0xF, +2 on.
S01_EXPORT void __cdecl JumpChild_Rise(void) {
    Rise();
    unsigned char* s = Sc();
    if (s[0xA] != 0) {
        Dec(s[0xA]);
        s = Sc();
    }
    if (Long(s + 0x14) != 0) return;
    OverSource(s);
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x499900: falling, the shadow +0xA up to 8; +9 down; at 0
// BattleActor_PlaySound(2, 0), sound 0x203, the speed 0x40 with the step -8,
// the heading +0xC Math_Ratan2(owner x - x, owner z - z), the target flagged
// 0x40, +9 0x10, +2 on.
S01_EXPORT void __cdecl JumpChild_Hover(void) {
    Rise();
    unsigned char* s = Sc();
    if (s[0xA] < 8) {
        Inc(s[0xA]);
        s = Sc();
    }
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Sound_PlayById)(0x203);
    SetLong(Sc() + 0x14, 0x40);
    SetLong(Sc() + 0x20, -8);
    const unsigned char* const o = Owner();
    const unsigned char* const t = Sc();
    const auto dz = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x38)) - static_cast<std::uint32_t>(Long(t + 0x38)));
    const auto dx = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x34)) - static_cast<std::uint32_t>(Long(t + 0x34)));
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    SetLong(Sc() + 0xC, heading);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4999E0: along the heading: x += 2 Math_Sin(+0xC), z += 2
// Math_Cos(+0xC) (each `shl 0xD / sar 0xC`, through the field the call left
// it pointing at); moving; while +9 is below 8 the shadow +0xA up to 0x10;
// +9 down; at 0 +2 on.
S01_EXPORT void __cdecl JumpChild_Return(void) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const int v = MH_CALL(Math_Sin)(Long(s + 0xC));
        AddL(x, Twice(v));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const z = s + 0x38;
        const int v = MH_CALL(Math_Cos)(Long(s + 0xC));
        AddL(z, Twice(v));
    }
    Rise();
    unsigned char* const s = Sc();
    if (s[9] < 8 && s[0xA] < 0x10) Inc(s[0xA]);
    Dec(s[9]);
    if (s[9] != 0) return;
    Inc(s[2]);
}

// original 0x499A80: the tint faded to 0x80 and the shadow shrunk by 2 a
// frame, then home (FadeHome).
S01_EXPORT void __cdecl JumpChild_Fade(void) { FadeHome(true); }

// original 0x499B10: the shadow's matrix pushed: Camera_Matrix x the identity
// rotation (angles 0, 0, 0), translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)), the height the owner's +0x3E below step 2,
// the source sprite's after. The task is read once, after the push.
S01_EXPORT void __cdecl JumpChild_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    const int height = s[2] < 2 ? S16(Owner() + 0x3E) : S16(Source() + 0x3E);
    v[2] = static_cast<short>(-(height / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    MH_CALL(As<RotTransFn>(&::Gte_RotTrans))(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x499BD0: the shadow, a disc of sixteen gouraud triangles in the
// matrix's plane: a draw-mode packet (tpage 0x55) to layer 5; the radius
// +0xA x 8 into scratch words +0 and +2; for each 0x100 of angle from 0x100
// to 0x1000 a triangle of the centre, the previous rim point and the next
// (radius x Math_Sin / Math_Cos >> 12 in Prim_VertexScratch's third
// SVECTOR, its z 0), projected, depths by Gte_PrimDepths3_10B, the centre's
// colour the radius byte (scratch +2) in all three channels and the rim's 1,
// semi-transparent, committed to layer 5; then a draw-mode packet with tpage
// 0x15. The scratch and the vertices are read again after every call.
S01_EXPORT void __cdecl JumpChild_DrawShadow(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(2, static_cast<unsigned>(s[0xA]) << 3);
        SetSW(0, static_cast<unsigned>(s[0xA]) << 3);
    }
    {
        const int v = MH_CALL(Math_Sin)(0);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    {
        const int v = MH_CALL(Math_Cos)(0);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t rim_x = VW(0x10), rim_z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, rim_x);
        SetVW(0xA, rim_z);
        {
            const int v = MH_CALL(Math_Sin)(angle);
            SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        }
        {
            const int v = MH_CALL(Math_Cos)(angle);
            SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        }
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        long depth, flag;
        MH_CALL(As<Rtp3Fn>(&::Gte_RotTransPers3))(VP(0), VP(8), VP(0x10), reinterpret_cast<float*>(p + 8),
                                                  reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                                  &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(2);
        p[5] = SB(2);
        p[6] = SB(2);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void MagicS01_Inject() {
    if (bof3::WantsShadow("magic_s01")) magic_s01::SelfTest();
    BOF3_INJECT(NueStomp_Task);
    BOF3_INJECT(NueStomp_Start);
    BOF3_INJECT(NueStompChild_Task);
    BOF3_INJECT(NueStompChild_Run);
    BOF3_INJECT(NueStompChild_Begin);
    BOF3_INJECT(NueStompChild_Leap);
    BOF3_INJECT(NueStompChild_Rise);
    BOF3_INJECT(NueStompChild_Drop);
    BOF3_INJECT(NueStompChild_Crouch);
    BOF3_INJECT(NueStompChild_Bounce);
    BOF3_INJECT(NueStompChild_Hop);
    BOF3_INJECT(NueStompChild_Stomp);
    BOF3_INJECT(NueStompChild_Land);
    BOF3_INJECT(NueStompChild_Return);
    BOF3_INJECT(Jump_Task);
    BOF3_INJECT(Jump_Start);
    BOF3_INJECT(Magic001_EndWhenChildDone);
    BOF3_INJECT(JumpChild_Task);
    BOF3_INJECT(JumpChild_Run);
    BOF3_INJECT(JumpChild_Begin);
    BOF3_INJECT(JumpChild_Rise);
    BOF3_INJECT(JumpChild_Hover);
    BOF3_INJECT(JumpChild_Return);
    BOF3_INJECT(JumpChild_Fade);
    BOF3_INJECT(JumpChild_PushMatrix);
    BOF3_INJECT(JumpChild_DrawShadow);
}
