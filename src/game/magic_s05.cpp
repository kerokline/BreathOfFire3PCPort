// Two spell units compiled into the exe, round nine group S05
// (docs/magic_s05.md): the PSX's MAGIC017.EMI (row 43) and MAGIC018/019.EMI
// (rows 5, 6, 53, 54, 68; MAGIC019's one row sits inside MAGIC018's code, so
// the two are one unit). Read one id down (docs/cut-content.md section 2) the
// sibling's labels are Astral Warp / Shadowwalk (row 43), Giant Growth (5),
// Aura (6), SpiritBlast (53), Double Blow (54) and Multistrike / Triple Blow
// (68) - hypotheses; the names below say only which unit and row, and what
// the code does.
//
// Every row's start does the same thing with different counts: the task takes
// the owner's direction and position, plays the actor's animation 0xC, and
// copies the acting actor's record (0x80 bytes: party 0x802D40 + 0x14C i, or
// enemy 0x93B960 + 0x128 (i - 3)) into new child tasks - the images:
//
//   - MAGIC017 0x4A11E0..0x4A1976: six images (kind 1, 0x21), run by this
//     unit's own ten-step child (Magic017Image_*): each fades in, drifts
//     out, jumps to the source sprite (0x904B4C), comes back, and the one
//     with +0xB 0 plays the strike while the others free themselves; the
//     task waits for them, the fx CLUT row is restored, done;
//   - MAGIC018/019 0x4A1980..0x4A218F: one image (rows 5, 6, 54, 68) or
//     three (row 53) of kind 1, 0x1F - MAGIC008's code, group S06's - with
//     the type +1 of 0, 1, 3, 4 or 5; the task waits for the image to count
//     its +0xB down (to 0xFF, or to 0 for row 53), then the animation 4.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s05.h"

#include <cstdint>

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

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* at, std::int32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + static_cast<std::uint32_t>(v)));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The acting actor's record as the originals index it: a member (0x802D40,
// stride 0x14C) at 0..2, else the enemy by index - 3 (0x93B960, stride
// 0x128), unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor <= 2) return Mem(at::kParty + actor * at::kPartyStride);
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(actor) - 3) * at::kEnemyStride);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// Capcom's, unnamed, in no group: 0x435A70(actor, animation) makes the enemy
// record of battle index `actor` (the byte; 0x93B960 + 0x128 (actor - 3))
// the current enemy 0x939AD8 for a BattleEnemy_SetAnimation(animation), then
// puts the old one back. The originals push the actor byte with stale upper
// bytes; the callee masks them off.
constexpr std::uint32_t kEnemyAnimation = 0x435A70;
using EnemyAnimFn = void (__cdecl*)(unsigned, unsigned);
void EnemyAnimation(unsigned char actor, unsigned animation) { MH_AT(EnemyAnimFn, kEnemyAnimation)(actor, animation); }

// The step handlers of other groups the tables hold, by their addresses (a
// phase is always called through the address its table holds): group E's
// done flag and free (0x43FE80) and target flag 0x40 with the done flag and
// free (0x43F460), round eight's task free (0x4AEE90).
constexpr std::uint32_t kDoneAndFree = bof3::addr::MagicFx_DoneAndFree;
constexpr std::uint32_t kFlagTargetEnd = bof3::addr::MagicFx_FlagTargetEnd;
constexpr std::uint32_t kFreeTask = bof3::addr::BattleFx_FreeTask;

// The kinds-table parameters of the images (BattleTask_Create(1, n)).
constexpr unsigned kImage017 = 0x21;   // Magic017Image_Task
constexpr unsigned kImage008 = 0x1F;   // MAGIC008's (group S06's)

// The start every row shares: the owner's direction +8 and position
// +0x34 / +0x38 / +0x3C to the task, +0xB and +9 0, +1 on, then the actor's
// animation 0xC with `arg`. Each field is read and written through the cells
// afresh, as the originals do.
void TakeOwnerAndAnimate(unsigned arg) {
    {
        const unsigned char facing = Owner()[8];
        Sc()[8] = facing;
    }
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, arg);
}

// One image: a kind-1 task of `parameter` (the slot unchecked: 0xFF, none
// free, would write past the 48 slots), the actor byte read after the call,
// and the actor's record's first 0x80 bytes copied over the slot a dword at
// a time (rep movsd). Its fields are the caller's to set.
unsigned char* SpawnImage(unsigned parameter) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu;
    unsigned char* const t = TaskSlot(slot);
    const unsigned char* const src = ActorRecord(ActorByte());
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(t + k, Long(src + k));
    return t;
}
// The fields each image of MAGIC018/019 gets: +0x80 this task, +1 its type,
// +2 0, +6 1, +5 the parameter.
void SetImage008(unsigned char* t, unsigned char type) {
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(Sc())));
    t[1] = type;
    t[2] = 0;
    t[6] = 1;
    t[5] = static_cast<unsigned char>(kImage008);
}

// The owner's sprite into the fx CLUT row: +0x27 the row CopyToFxRow
// answers, +0x28 / +0x24 the owner's, then the row's STP bits.
void OwnerClutToFxRow() {
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
}

// The image's three tint bytes +0x5D..+0x5F, each moved by `by` (a byte add).
void Tint(unsigned char* s, unsigned by) {
    AddB(s[0x5D], by);
    AddB(s[0x5E], by);
    AddB(s[0x5F], by);
}
// Shown semi-transparent at tint 0xC0: +0 bit 0x20, +0x5C and +0x2B 1.
void ShowTinted(unsigned char* s) {
    s[0] |= 0x20;
    s[0x5C] = 1;
    s[0x2B] = 1;
    s[0x5D] = 0xC0;
    s[0x5E] = 0xC0;
    s[0x5F] = 0xC0;
}

}  // namespace

#define S05_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC017 (row 43)

// original 0x4A11E0: the kind-2 task. A three-entry stack table by +1:
// Magic017_Start, Magic017_Wait, Magic017_End.
S05_EXPORT void __cdecl Magic017_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic017_Start, bof3::addr::Magic017_Wait,
                                                 bof3::addr::Magic017_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic017_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A1210: the shared start with animation (0xC, 0); six images
// (kind 1, 0x21): +0x80 this task, +1 / +2 0, +6 1, +5 0x21, +0xB the image's
// number i, +9 (i & ~1) + 1 (pairs launch together); the task's +0xB up by
// one for each (read after the image's fields, as the original does). Then
// the owner's CLUT to the fx row, the owner's +0 bit 0x40, sound 0x100.
S05_EXPORT void __cdecl Magic017_Start(void) {
    TakeOwnerAndAnimate(0);
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const t = SpawnImage(kImage017);
        unsigned char* const s = Sc();
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
        t[1] = 0;
        t[2] = 0;
        t[6] = 1;
        t[5] = static_cast<unsigned char>(kImage017);
        t[0xB] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>((i & 0xFEu) + 1);
        Inc(s[0xB]);
    }
    OwnerClutToFxRow();
    Owner()[0] |= 0x40;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A13B0 (also MAGIC021's, group S07's): once every image has
// counted +0xB down to 0, the actor's animation (4, 0), the owner's +0 bit
// 0x40 off, +1 on.
S05_EXPORT void __cdecl Magic017_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x4A13E0: the fx CLUT row restored, the effect's done flag, the
// task freed (a tail jmp).
S05_EXPORT void __cdecl Magic017_End(void) {
    MH_CALL(SpriteClut_RestoreFxRow)();
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A1400: the image's kind-1 task, a jmp through
// Magic017Image_TaskTable (one entry) by +1, unchecked. The next dword is
// MAGIC008's table (0x4A2480); ours aborts there. Magic017_Start stores +1 0
// and nothing moves it.
S05_EXPORT void __cdecl Magic017Image_Task(void) {
    const unsigned type = Sc()[1];
    if (type >= 1) PastTable("Magic017Image_Task", type, 1);
    magic_harness::Phase(bof3::addr::Magic017Image_Run)();
}

// original 0x4A1420: a ten-entry stack table by +2 (the nine steps below and
// BattleFx_FreeTask), unchecked; then, while +0 and +2 are both set,
// Sprite_UpdateScreen.
S05_EXPORT void __cdecl Magic017Image_Run(void) {
    static constexpr std::uint32_t kSteps[10] = {
        bof3::addr::Magic017Image_Appear, bof3::addr::Magic017Image_Drift,   bof3::addr::Magic017Image_Leap,
        bof3::addr::Magic017Image_Return, bof3::addr::Magic017Image_Strike,  bof3::addr::Magic017Image_Script,
        bof3::addr::Magic017Image_Reshow, bof3::addr::Magic017Image_Home,    bof3::addr::Magic017Image_Done,
        kFreeTask};
    const unsigned step = Sc()[2];
    if (step >= 10) PastTable("Magic017Image_Run", step, 10);
    magic_harness::Phase(kSteps[step])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A14A0: +9 down; at 0 the image shows at tint 0xC0 with the
// owner's +0x27 (its CLUT row), +0xC 0, +0x10 0x1000 for an odd +0xB or
// -0x1000 for an even one, turned by its direction (0x446770); +9 0x10, +2 on.
S05_EXPORT void __cdecl Magic017Image_Appear(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    ShowTinted(s);
    s[0x27] = Owner()[0x27];
    const unsigned char number = s[0xB];
    SetLong(s + 0xC, 0);
    SetLong(s + 0x10, (number & 1) ? 0x1000 : static_cast<std::int32_t>(0xFFFFF000u));
    Turn(s);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A1560: the position +0x34 / +0x38 on by +0xC / +0x10, the tint
// down by 4; +9 down, at 0 +2 on.
S05_EXPORT void __cdecl Magic017Image_Drift(void) {
    unsigned char* const s = Sc();
    AddLong(s + 0x34, Long(s + 0xC));
    AddLong(s + 0x38, Long(s + 0x10));
    Tint(s, 0xFC);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4A15D0: +9 up; at 0x10 the image jumps to the source sprite
// (0x904B4C): the offset (-0x20000, -0x10000 for an odd +0xB, 0x10000 for an
// even) turned by its direction, added to the source's +0x34 / +0x38, its
// +0x3C the source's; then the step +0xC 0, +0x10 0x1000 (odd) or -0x1000
// turned; sound 0x101 from image 0 (+0xB 0) alone; +9 0x10, +2 on.
S05_EXPORT void __cdecl Magic017Image_Leap(void) {
    Inc(Sc()[9]);
    {
        unsigned char* const s = Sc();
        if (s[9] != 0x10) return;
        SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFE0000u));
        SetLong(s + 0x10, (s[0xB] & 1) ? static_cast<std::int32_t>(0xFFFF0000u) : 0x10000);
        Turn(s);
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Pointer(at::kSource) + 0x34)) +
                                                    static_cast<std::uint32_t>(Long(s + 0xC))));
        SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Pointer(at::kSource) + 0x38)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x10))));
        SetLong(s + 0x3C, Long(Pointer(at::kSource) + 0x3C));
        SetLong(s + 0xC, 0);
        SetLong(s + 0x10, (s[0xB] & 1) ? 0x1000 : static_cast<std::int32_t>(0xFFFFF000u));
        Turn(s);
    }
    if (Sc()[0xB] == 0) MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A16D0: on by the step, the tint up by 4, +9 down; at 0: an
// image other than 0 counts the task's +0xB down and frees itself (a tail
// jmp); image 0 turns solid (+0 bit 0x20, +0x5C, +0x2B and the tint 0), an
// enemy actor's animation 2 (0x435A70), +9 BattleActor_FxSize's answer,
// +2 on.
S05_EXPORT void __cdecl Magic017Image_Return(void) {
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x34, Long(s + 0xC));
        AddLong(s + 0x38, Long(s + 0x10));
        Tint(s, 4);
        Dec(s[9]);
        if (s[9] != 0) return;
        if (s[0xB] != 0) {
            Dec(Owner()[0xB]);
            MH_CALL(BattleTask_FreeCurrent)();
            return;
        }
        s[0] &= 0xDF;
        s[0x5C] = 0;
        s[0x2B] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
    }
    const unsigned char actor = ActorByte();
    if (actor >= 3) EnemyAnimation(actor, 2);
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x4A17A0: while the task's +0xB is 1 (image 0 the last left):
// the image's script ticked (its answer unread), +9 down; at 0 the target
// flagged 0x40, BattleActor_PlaySound(2, 4), +2 on.
S05_EXPORT void __cdecl Magic017Image_Strike(void) {
    if (Owner()[0xB] != 1) return;
    MH_CALL(Sprite_ScriptTickOnce)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(BattleActor_PlaySound)(2, 4);
    Inc(Sc()[2]);
}

// original 0x4A17F0: the script ticked; when it answers its end, +9 8, +2 on.
S05_EXPORT void __cdecl Magic017Image_Script(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4A1810: +9 down; at 0 an enemy actor's animation 0 (0x435A70),
// the image shown at tint 0xC0 again, +9 8, +2 on.
S05_EXPORT void __cdecl Magic017Image_Reshow(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned char actor = ActorByte();
    if (actor >= 3) EnemyAnimation(actor, 0);
    unsigned char* const s = Sc();
    ShowTinted(s);
    s[9] = 8;
    Inc(s[2]);
}

// original 0x4A1890: the tint down by 8, +9 down; at 0 the owner's position
// +0x34 / +0x38 / +0x3C to the image, +9 8, +2 on.
S05_EXPORT void __cdecl Magic017Image_Home(void) {
    unsigned char* const s = Sc();
    Tint(s, 0xF8);
    Dec(s[9]);
    if (s[9] != 0) return;
    SetLong(s + 0x34, Long(Owner() + 0x34));
    SetLong(s + 0x38, Long(Owner() + 0x38));
    SetLong(s + 0x3C, Long(Owner() + 0x3C));
    s[9] = 8;
    Inc(s[2]);
}

// original 0x4A1920: the tint up by 8, +9 down; at 0 the task's +0xB down,
// +2 on (to BattleFx_FreeTask).
S05_EXPORT void __cdecl Magic017Image_Done(void) {
    unsigned char* const s = Sc();
    Tint(s, 8);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    Inc(s[2]);
}

// ===========================================================================
// MAGIC018/019 (rows 5, 6, 53, 54, 68)

// original 0x4A1980 (row 5): a four-entry stack table by +1:
// Magic018Row5_Start, Magic018Row5_Wait, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd (never reached: the third frees the task).
S05_EXPORT void __cdecl Magic018Row5_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Magic018Row5_Start, bof3::addr::Magic018Row5_Wait,
                                                 kDoneAndFree, kFlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic018Row5_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A19C0: the shared start with animation (0xC, 2); one image
// (kind 1, 0x1F) of type 0; the owner's +0 bit 0x40, sound 0x100.
S05_EXPORT void __cdecl Magic018Row5_Start(void) {
    TakeOwnerAndAnimate(2);
    SetImage008(SpawnImage(kImage008), 0);
    Owner()[0] |= 0x40;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A1AF0: once the image has counted +0xB down to 0xFF: the
// actor's animation (4, 0), the owner's bit 0x40 off, +1 on.
S05_EXPORT void __cdecl Magic018Row5_Wait(void) {
    if (Sc()[0xB] != 0xFF) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x4A1B20 (row 6, MAGIC019): a three-entry stack table by +1:
// Magic019_Start, Magic019_End, MagicFx_FlagTargetEnd (never reached:
// Magic019_End frees the task).
S05_EXPORT void __cdecl Magic019_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic019_Start, bof3::addr::Magic019_End, kFlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic019_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A1B50: the shared start with animation (0xC, 2); one image of
// type 1; the owner's CLUT to the fx row with its STP bits, and entry 31 of
// the task's cell cleared (SpriteClut_ClearEntry31). No sound, no bit 0x40.
S05_EXPORT void __cdecl Magic019_Start(void) {
    TakeOwnerAndAnimate(2);
    SetImage008(SpawnImage(kImage008), 1);
    OwnerClutToFxRow();
    MH_CALL(SpriteClut_ClearEntry31)(Sc());
}

// original 0x4A1CC0: once the image has counted +0xB down to 0xFF: the fx
// CLUT row restored, the actor's animation (4, 0), the done flag, the task
// freed (a tail jmp).
S05_EXPORT void __cdecl Magic019_End(void) {
    if (Sc()[0xB] != 0xFF) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A1CF0 (row 53): a four-entry stack table by +1:
// Magic018Row53_Start, Magic018Row53_Wait, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd (never reached).
S05_EXPORT void __cdecl Magic018Row53_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Magic018Row53_Start, bof3::addr::Magic018Row53_Wait,
                                                 kDoneAndFree, kFlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic018Row53_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A1D30: the shared start with animation (0xC, 2); three images
// of type 3 with +0xB their number, the task's +0xB up for each; the owner's
// CLUT to the fx row with its STP bits; the owner's +0 bit 0x40.
S05_EXPORT void __cdecl Magic018Row53_Start(void) {
    TakeOwnerAndAnimate(2);
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const t = SpawnImage(kImage008);
        unsigned char* const s = Sc();
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
        t[1] = 3;
        t[2] = 0;
        t[6] = 1;
        t[5] = static_cast<unsigned char>(kImage008);
        t[0xB] = static_cast<unsigned char>(i);
        Inc(s[0xB]);
    }
    OwnerClutToFxRow();
    Owner()[0] |= 0x40;
}

// original 0x4A1EC0 (also reached from MAGIC015 / 016, group S04's): once
// the images have counted +0xB down to 0: the fx CLUT row restored, the
// actor's animation (4, 0), the owner's bit 0x40 off, +1 on.
S05_EXPORT void __cdecl Magic018Row53_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x4A1F00 (row 54): a four-entry stack table by +1:
// Magic018Row54_Start, Magic018_WaitOneImage, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd (never reached).
S05_EXPORT void __cdecl Magic018Row54_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Magic018Row54_Start, bof3::addr::Magic018_WaitOneImage,
                                                 kDoneAndFree, kFlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic018Row54_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A1F40: the shared start with animation (0xC, 2); one image of
// type 4; the owner's +0 bit 0x40.
S05_EXPORT void __cdecl Magic018Row54_Start(void) {
    TakeOwnerAndAnimate(2);
    SetImage008(SpawnImage(kImage008), 4);
    Owner()[0] |= 0x40;
}

// original 0x4A2060 (row 68): a three-entry stack table by +1:
// Magic018Row68_Start, Magic018_WaitOneImage, MagicFx_DoneAndFree.
S05_EXPORT void __cdecl Magic018Row68_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic018Row68_Start, bof3::addr::Magic018_WaitOneImage,
                                                 kDoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic018Row68_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A2090: +0xB 0, +1 on (the task's direction and position are
// not taken), animation (0xC, 2); one image of type 5; the owner's +0 bit
// 0x40.
S05_EXPORT void __cdecl Magic018Row68_Start(void) {
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    SetImage008(SpawnImage(kImage008), 5);
    Owner()[0] |= 0x40;
}

// original 0x4A2160 (rows 54 and 68): once the image has counted +0xB down
// to 0xFF: the owner's bit 0x40 off first, then the actor's animation (4, 0),
// +1 on.
S05_EXPORT void __cdecl Magic018_WaitOneImage(void) {
    if (Sc()[0xB] != 0xFF) return;
    Owner()[0] &= 0xBF;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Inc(Sc()[1]);
}

void MagicS05_Inject() {
    if (bof3::WantsShadow("magic_s05")) magic_s05::SelfTest();
    BOF3_INJECT(Magic017_Task);
    BOF3_INJECT(Magic017_Start);
    BOF3_INJECT(Magic017_Wait);
    BOF3_INJECT(Magic017_End);
    BOF3_INJECT(Magic017Image_Task);
    BOF3_INJECT(Magic017Image_Run);
    BOF3_INJECT(Magic017Image_Appear);
    BOF3_INJECT(Magic017Image_Drift);
    BOF3_INJECT(Magic017Image_Leap);
    BOF3_INJECT(Magic017Image_Return);
    BOF3_INJECT(Magic017Image_Strike);
    BOF3_INJECT(Magic017Image_Script);
    BOF3_INJECT(Magic017Image_Reshow);
    BOF3_INJECT(Magic017Image_Home);
    BOF3_INJECT(Magic017Image_Done);
    BOF3_INJECT(Magic018Row5_Task);
    BOF3_INJECT(Magic018Row5_Start);
    BOF3_INJECT(Magic018Row5_Wait);
    BOF3_INJECT(Magic019_Task);
    BOF3_INJECT(Magic019_Start);
    BOF3_INJECT(Magic019_End);
    BOF3_INJECT(Magic018Row53_Task);
    BOF3_INJECT(Magic018Row53_Start);
    BOF3_INJECT(Magic018Row53_Wait);
    BOF3_INJECT(Magic018Row54_Task);
    BOF3_INJECT(Magic018Row54_Start);
    BOF3_INJECT(Magic018Row68_Task);
    BOF3_INJECT(Magic018Row68_Start);
    BOF3_INJECT(Magic018_WaitOneImage);
}
