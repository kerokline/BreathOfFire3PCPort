// Moved here by tools/move_funcs.py from magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/howling_child.h"

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

namespace addr = bof3::addr;

using magic_harness::Mem;

using move_script::Long;

using move_script::SetLong;

unsigned char* Sc() { return Sprite_Current; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3 (a party index lands below the
// enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

// MAGIC054's walk counts the enemies from 0.
unsigned char* EnemyByOrder(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// A task's copy of a record's first 0x80 bytes (`rep movsd`: dword by dword,
// forward).
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
}

using AnimFn = void (__cdecl*)(std::uint32_t, std::uint32_t);

// The engine's enemy animations, unnamed, in no group: an enemy by battle
// index - 3 (the index's low byte) given animation `arg` through
// BattleEnemy_SetAnimation; 0x435A20 makes it Sprite_Current for the call,
// 0x435A70 does not (docs/magic_s01.md).
constexpr ::bof3::image::CodeVa kEnemyAnimCurrent{bof3::addr::BattleEnemy_SetAnimationAs};   // 0x435A20

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A child (kind 1, 0x2C) holding a copy of the record's first 0x80 bytes,
// owned by this task, +1 / +2 0, +6 1, +5 0x2C, +0xB the record's number;
// counted in +0xB; the record's +0 | 0x40.
void CopyToChild(unsigned char* rec, unsigned number) {
    const unsigned slot = NewTask(0x2C);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    CopyRecord(child, rec);
    unsigned char* const self = Sc();
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x2C;
    child[0xB] = static_cast<unsigned char>(number);
    Inc(self[0xB]);
    rec[0] = static_cast<unsigned char>(rec[0] | 0x40);
}

// The record of the child's number +0xB (unchecked): the enemy when the
// target byte has 0x40, else the party member.
unsigned char* CopiedRecord() {
    const unsigned i = Sc()[0xB];
    return (TargetByte() & 0x40) != 0 ? EnemyByOrder(i) : PartyRecord(i);
}

// The scale steps: +0xC / +0x10 on by +0x18 / +0x1C, the sprite's scale
// +0x40 / +0x44 on by them.
void ScaleStep() {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    AddLong(s + 0xC, static_cast<std::uint32_t>(ts->work_18));
    AddLong(s + 0x10, static_cast<std::uint32_t>(ts->work_1c));
    AddLong(s + 0x40, static_cast<std::uint32_t>(ts->step_x));
    AddLong(s + 0x44, static_cast<std::uint32_t>(ts->step_z));
}

}  // namespace

// original 0x4ACAB0: the kind-2 task. A four-entry stack table by +1:
// Howling_Start, group S23's Simoon_Wait, MagicFx_DoneAndFree, group S26's
// Magic114_End.
S10_EXPORT void __cdecl Howling_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Howling_Start, addr::Simoon_Wait, addr::MagicFx_DoneAndFree,
                                                 addr::Magic114_End};
    Dispatch(kPhases, 4, Sc()[1], "Howling_Task");
}

// original 0x4ACAF0: in an event battle of kind 0x37 only the target flags
// 0x10, +1 3 (Magic114_End) and +9 0x1E. Else +0xB 0, +1 on, and every live
// actor of the target's side (the enemies when its 0x40 bit is set, else the
// party) given an animation - an enemy 4 through the engine's 0x435A20, a
// member its +8 + 0x10 - and copied into a child; sound 0x100.
S10_EXPORT void __cdecl Howling_Start(void) {
    if (Mem(::bof3::Img(EventBattle_Id_addr))[0] == 0x37) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        Sc()[1] = 3;
        Sc()[9] = 0x1E;
        return;
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    if ((TargetByte() & 0x40) != 0) {
        for (unsigned i = 0; i < 8; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i + 3) & 0xFF) != 0) continue;
            MH_AT(AnimFn, kEnemyAnimCurrent.v)(i + 3, 4);
            CopyToChild(EnemyByOrder(i), i);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i) & 0xFF) != 0) continue;
            unsigned char* const self = Sprite_Current;
            unsigned char* const rec = PartyRecord(i);
            Sprite_Current = rec;
            MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rec[8] + 0x10));
            Sprite_Current = self;
            CopyToChild(rec, i);
        }
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4ACCE0: the child's kind-1 task, a jmp through
// HowlingChild_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl HowlingChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::HowlingChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "HowlingChild_Task");
}

// original 0x4ACD00: a six-entry stack table by +2 (_Start, _Out, _Hold,
// _Back, _End, MAGIC058's 0x4AF490); the screen update while +0 is set.
S10_EXPORT void __cdecl HowlingChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {addr::HowlingChild_Start, addr::HowlingChild_Out,
                                                addr::HowlingChild_Hold,  addr::HowlingChild_Back,
                                                addr::HowlingChild_End,   addr::MagicFx_UncountAndFree};
    Dispatch(kSteps, 6, Sc()[2], "HowlingChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4ACD60: the child at its record's position; the scale 0x10000
// and its steps (-0xC60, 0x1080) growing by (0x60, -0x80); +0x48 2; +2 on.
S10_EXPORT void __cdecl HowlingChild_Start(void) {
    const unsigned char* const rec = CopiedRecord();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = rv::As<records::SpriteHead>(rec)->x;
    ts->z = rv::As<records::SpriteHead>(rec)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(rec)->height);
    rv::Store(ts->scale_x, 0x10000);
    rv::Store(ts->scale_y, 0x10000);
    s[0x48] = 2;
    ts->step_x = static_cast<std::int32_t>(0xFFFFF3A0u);
    ts->step_z = 0x1080;
    ts->work_18 = 0x60;
    ts->work_1c = static_cast<std::int32_t>(0xFFFFFF80u);
    Inc(s[2]);
}

// original 0x4ACE70: the script ticked (its answer unread), a scale step;
// once +0xC is 0, +9 0x10 and +2 on.
S10_EXPORT void __cdecl HowlingChild_Out(void) {
    MH_CALL(Sprite_ScriptTick)();
    ScaleStep();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (ts->step_x != 0) return;
    s[9] = 0x10;
    Inc(s[2]);
}

// original 0x4ACED0: the script ticked; +9 down; at 0 the steps (0x60,
// -0x80) again and +2 on.
S10_EXPORT void __cdecl HowlingChild_Hold(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (s[9] != 0) return;
    ts->work_18 = 0x60;
    ts->work_1c = static_cast<std::int32_t>(0xFFFFFF80u);
    Inc(s[2]);
}

// original 0x4ACF10: the script ticked, a scale step; at +0xC 0xC00 +2 on.
S10_EXPORT void __cdecl HowlingChild_Back(void) {
    MH_CALL(Sprite_ScriptTick)();
    ScaleStep();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (ts->step_x == 0xC00) Inc(s[2]);
}

// original 0x4ACF70: the script ticked once; at its end the record's 0x40
// bit cleared and +2 on.
S10_EXPORT void __cdecl HowlingChild_End(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    unsigned char* const rec = CopiedRecord();
    rec[0] = static_cast<unsigned char>(rec[0] & 0xBF);
    Inc(Sc()[2]);
}

void HowlingChild_Inject() {
    BOF3_INJECT(Howling_Task);
    BOF3_INJECT(Howling_Start);
    BOF3_INJECT(HowlingChild_Task);
    BOF3_INJECT(HowlingChild_Run);
    BOF3_INJECT(HowlingChild_Start);
    BOF3_INJECT(HowlingChild_Out);
    BOF3_INJECT(HowlingChild_Hold);
    BOF3_INJECT(HowlingChild_Back);
    BOF3_INJECT(HowlingChild_End);
}
