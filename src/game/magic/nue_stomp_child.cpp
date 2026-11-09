// Moved here by tools/move_funcs.py from magic_s01.cpp (docs/source-layout-round.md).
#include "game/magic/nue_stomp_child.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s01.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
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

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A dword added in place, wrapping.
void AddL(unsigned char* a, std::uint32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + v));
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The engine's 0x435A70 (in no group, unnamed): BattleEnemy_SetAnimation(anim)
// on the enemy of battle index `actor` (0x93B960 + 0x128 (actor - 3),
// unchecked), 0x939AD8 put back after. Row 1 hands it the acting actor.
constexpr ::bof3::image::CodeVa kEnemyAnimation{bof3::addr::BattleEnemy_SetAnimationOf};   // 0x435A70, R3A's (round 14)

using EnemyAnimationFn = void (__cdecl*)(unsigned, unsigned);

void EnemyAnimation(unsigned anim) { MH_AT(EnemyAnimationFn, kEnemyAnimation.v)(ActorByte(), anim); }

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
        actor < 3 ? Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride)
                  : Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(actor) - 3) * at::kEnemyStride);
    unsigned char* const task = Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride);
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
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
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
    AddL(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
}

// At the top of the rise: over the source sprite (+0x34, +0x38), the height
// +0x3C moved by the source's less the owner's.
void OverSource(unsigned char* s) {
    rv::Store(rv::As<records::SpriteHead>(s)->x, rv::As<records::SpriteHead>(Source())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Source())->z);
    const std::uint32_t d = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Source())->height) - static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height);
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
    rv::Store(rv::As<records::BattleTask>(s)->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Inc(Sc()[2]);
}

// The sprite script ticked three times; the last answer counts.
unsigned char TickThrice() {
    MH_CALL(Sprite_ScriptTickOnce)();
    MH_CALL(Sprite_ScriptTickOnce)();
    return MH_CALL(Sprite_ScriptTickOnce)();
}

}  // namespace

#define S01_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -8);
    Inc(Sc()[2]);
}

// original 0x4992B0: rising; when the speed reaches 0, over the source
// sprite, +9 0x1F, +2 on.
S01_EXPORT void __cdecl NueStompChild_Rise(void) {
    Rise();
    unsigned char* const s = Sc();
    if (rv::As<records::BattleTask>(s)->step_y != 0) return;
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
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x140);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -0x40);
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

void NueStompChild_Inject() {
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
}
