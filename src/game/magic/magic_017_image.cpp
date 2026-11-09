// Moved here by tools/move_funcs.py from magic_s05.cpp (docs/source-layout-round.md).
#include "game/magic/magic_017_image.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s05.h"
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

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::int32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + static_cast<std::uint32_t>(v)));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The acting actor's record as the originals index it: a member (0x802D40,
// stride 0x14C) at 0..2, else the enemy by index - 3 (0x93B960, stride
// 0x128), unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor <= 2) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(actor) - 3) * at::kEnemyStride);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

// Capcom's, unnamed, in no group: 0x435A70(actor, animation) makes the enemy
// record of battle index `actor` (the byte; 0x93B960 + 0x128 (actor - 3))
// the current enemy 0x939AD8 for a BattleEnemy_SetAnimation(animation), then
// puts the old one back. The originals push the actor byte with stale upper
// bytes; the callee masks them off.
constexpr ::bof3::image::CodeVa kEnemyAnimation{bof3::addr::BattleEnemy_SetAnimationOf};   // 0x435A70, R3A's (round 14)

using EnemyAnimFn = void (__cdecl*)(unsigned, unsigned);

void EnemyAnimation(unsigned char actor, unsigned animation) { MH_AT(EnemyAnimFn, kEnemyAnimation.v)(actor, animation); }

constexpr ::bof3::image::CodeVa kFreeTask{bof3::addr::BattleFx_FreeTask};

// The kinds-table parameters of the images (BattleTask_Create(1, n)).
constexpr unsigned kImage017 = 0x21;   // Magic017Image_Task

// The start every row shares: the owner's direction +8 and position
// +0x34 / +0x38 / +0x3C to the task, +0xB and +9 0, +1 on, then the actor's
// animation 0xC with `arg`. Each field is read and written through the cells
// afresh, as the originals do.
void TakeOwnerAndAnimate(unsigned arg) {
    {
        const unsigned char facing = Owner()[8];
        Sc()[8] = facing;
    }
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
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
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
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
        kFreeTask.v};
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
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (s[9] != 0) return;
    ShowTinted(s);
    s[0x27] = Owner()[0x27];
    const unsigned char number = s[0xB];
    ts->step_x = 0;
    ts->step_z = (number & 1) ? 0x1000 : static_cast<std::int32_t>(0xFFFFF000u);
    Turn(s);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A1560: the position +0x34 / +0x38 on by +0xC / +0x10, the tint
// down by 4; +9 down, at 0 +2 on.
S05_EXPORT void __cdecl Magic017Image_Drift(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    AddLong(s + 0x34, ts->step_x);
    AddLong(s + 0x38, ts->step_z);
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
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        if (s[9] != 0x10) return;
        ts->step_x = static_cast<std::int32_t>(0xFFFE0000u);
        ts->step_z = (s[0xB] & 1) ? static_cast<std::int32_t>(0xFFFF0000u) : 0x10000;
        Turn(s);
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->x) +
                                                    static_cast<std::uint32_t>(ts->step_x));
        ts->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->z) +
                                                    static_cast<std::uint32_t>(ts->step_z));
        rv::Store(ts->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->height);
        ts->step_x = 0;
        ts->step_z = (s[0xB] & 1) ? 0x1000 : static_cast<std::int32_t>(0xFFFFF000u);
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
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        AddLong(s + 0x34, ts->step_x);
        AddLong(s + 0x38, ts->step_z);
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
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    Tint(s, 0xF8);
    Dec(s[9]);
    if (s[9] != 0) return;
    ts->x = rv::As<records::SpriteHead>(Owner())->x;
    ts->z = rv::As<records::SpriteHead>(Owner())->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(Owner())->height);
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

void Magic017Image_Inject() {
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
}
