// Moved here by tools/move_funcs.py from boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_weretigr_fx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sg::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's word - reads that word), its eax the dispatcher's.
using ForwardBossSg = U (__cdecl*)(unsigned);

ForwardBossSg EntryBossSg(U table, unsigned index) {
    return reinterpret_cast<ForwardBossSg>(static_cast<std::uintptr_t>(L(At(table + 4 * index))));   // as read: the fuzz swaps the cells
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSg(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryBossSg(table, state)(passed);
}

// call [table + 4 * Sprite_Current +2] (no argument), then, the Sprite_Current
// of after the call with +0 not 0, a tail jump to Sprite_UpdateScreen: the
// effect task's two step dispatchers.
void TaskSteps(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: state byte +2 is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(At(table + 4 * state))))();
    if (Sprite_Current[0] != 0) BH_CALL(Sprite_UpdateScreen)();
}

// An enemy object by a battle index (3..10 an enemy), as the originals index
// it: 0x93B960 + (index - 3) * 0x128, wrapping - unchecked.
unsigned char* EnemyByIndex(unsigned index) { return At(::bof3::Img(Battle_Enemies_addr) + (index - 3u) * at::kEnemyStride); }

}  // namespace

// original 0x43D6D0: the task: jmp [BossWeretigrFx_Steps + 4 * +1] (2: the
// turn, the trail).
extern "C" U __cdecl BossWeretigrFx_Task(unsigned passed) {
    return DispatchBossSg("BossWeretigrFx_Task", AddressOf(BossWeretigrFx_Steps), 2, 1, passed);
}

// original 0x43D6F0: +1 = 0, the turn: TaskSteps through
// BossWeretigrFx_MainSteps (7).
extern "C" void __cdecl BossWeretigrFx_Main(void) { TaskSteps("BossWeretigrFx_Main", AddressOf(BossWeretigrFx_MainSteps), 7); }

// original 0x43D720: step 0. The height to reach +0x20 = +0x3C + 0x4000000;
// Sprite_SetAnimation(2); the Sprite_Current of after the call +0xB = 0x10
// and +2 up by one.
extern "C" void __cdecl BossWeretigrFx_Begin(void) {
    unsigned char* const s = Sprite_Current;
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(s)->work_20, S(static_cast<U>(rv::As<records::BattleTask>(s)->height) + 0x4000000u));
    BH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current[0xB] = 0x10;
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43D750: step 1. When Sprite_ScriptTickOnce answers al not 0:
// Sprite_SetAnimation(3) and the Sprite_Current of after +2 up by one.
extern "C" void __cdecl BossWeretigrFx_AwaitPose(void) {
    if ((BH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    BH_CALL(Sprite_SetAnimation)(3);
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43D770: step 2. While the s32 +0x3C is below +0x20: +0x3C +=
// 0x1000000. Then, once: the facing f = 0x904AAC & 0xFF - with the target
// 0x904B44 a member (below 3) +8 ^= 2 and f ^= 2 -; +0x34 = Field_Kind2X + the
// signed byte at 0x64E4F4 + 2 f, +0x38 = Field_Kind2Z + the one after it;
// Sprite_SetAnimation(4); the Sprite_Current of after +2 up by one. Both ways a
// tail jump to Sprite_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Rise(void) {
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    const U y = static_cast<U>(cur_task->height);
    if (S(y) < cur_task->work_20) {
        rv::Store<std::int32_t>(cur_task->height, S(y + 0x1000000u));
        return BH_CALL(Sprite_ScriptTickOnce)();
    }
    U f;
    if (B(::bof3::Img(at::kTarget.v)) >= 3) {
        f = L(At(::bof3::Img(at::kFacing.v))) & 0xFF;
        cur_task->x = S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kFacingOffsets.v) + 2 * f)))) +
                            static_cast<U>(Field_Kind2X));
        f = L(At(::bof3::Img(at::kFacing.v))) & 0xFF;
    } else {
        s[8] ^= 2;
        f = (L(At(::bof3::Img(at::kFacing.v))) & 0xFF) ^ 2;
        cur_task->x = S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kFacingOffsets.v) + 2 * f)))) +
                            static_cast<U>(Field_Kind2X));
        f = (L(At(::bof3::Img(at::kFacing.v))) & 0xFF) ^ 2;
    }
    rv::Store<std::int32_t>(cur_task->z, S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kFacingOffsets.v) + 2 * f + 1)))) +
                        static_cast<U>(Field_Kind2Z)));
    BH_CALL(Sprite_SetAnimation)(4);
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43D830: step 3. A trail copy: slot n = BattleTask_Create(3, 6);
// unless 0xFF - the task's first 0x80 bytes copied into slot n (rep movsd),
// then its +0 &= 0xBF, kind +6 = 3, +5 = 6, +1 = 1, +2..+4 = 0, owner +0x80 =
// the task, +0xB = the task's +0xB, and the task's +0xB down by one unless 0.
// Then toward the target 0x904B44 (a member 0..2 - ObjTrio - else an enemy
// object, 0x93B960 + (target - 3) * 0x128): MagicFx_StepToward(it, 0x50); the
// target read again for MagicFx_NearSprite3D(it, 0x10000) - near:
// Sprite_SetAnimation(5), Battle_SetTargetFlag40(the target read again), the
// Sprite_Current of after +2 up by one. Last, Sprite_ScriptTickOnce, whose al
// is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Strike(void) {
    const unsigned n = BH_CALL(BattleTask_Create)(3, 6) & 0xFF;
    if (n != 0xFF) {
        unsigned char* const s = Sprite_Current;
        unsigned char* const t = At(::bof3::Img(BattleTask_Slots_addr) + n * at::kTaskStride);
        auto* const cur_task = rv::As<records::BattleTask>(t);
        std::memmove(t, s, 0x80);
        cur_task->flags &= 0xBF;
        cur_task->runner = 3;
        cur_task->kind = 6;
        cur_task->state = 1;
        t[2] = 0;
        t[3] = 0;
        t[4] = 0;
        cur_task->owner = S(Low32(s));
        t[0xB] = s[0xB];
        if (s[0xB] != 0) s[0xB] = static_cast<unsigned char>(s[0xB] - 1);
    }
    const unsigned target = B(::bof3::Img(at::kTarget.v));
    bool near;
    if (target <= 2) {
        BH_CALL(MagicFx_StepToward)(At(::bof3::Img(ObjTrio_addr) + target * at::kPartyStride), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(At(::bof3::Img(ObjTrio_addr) + B(::bof3::Img(at::kTarget.v)) * at::kPartyStride), 0x10000) != 0;
    } else {
        BH_CALL(MagicFx_StepToward)(EnemyByIndex(target), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(EnemyByIndex(B(::bof3::Img(at::kTarget.v))), 0x10000) != 0;
    }
    if (near) {
        BH_CALL(Sprite_SetAnimation)(5);
        BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(at::kTarget.v)));
        Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    }
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43D990: step 4. When Sprite_ScriptTickOnce answers al not 0: the
// leap back to the acting enemy (0x904B34, its object 0x93B960 + (actor - 3) *
// 0x128): the Sprite_Current of after the call +0xC = (its +0x34 - +0x34) / 16,
// +0x10 = (its +0x38 - +0x38) / 16 (signed, toward zero, on the wrapped
// difference), +0x14 = 0x40, +0x20 = -8, +0x18 = (its +0x3C - +0x3C) / 16, +2
// up by one, and a tail jump to Sprite_ScriptTickOnce. Its al (the first or
// the second tick's) is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Leap(void) {
    const U first = BH_CALL(Sprite_ScriptTickOnce)();
    if ((first & 0xFF) == 0) return static_cast<unsigned char>(first);
    auto sixteenth = [](U to, U from) { return S(static_cast<U>(S(to - from) / 16)); };
    unsigned char* s = Sprite_Current;
    records::BattleTask* cur_task = rv::As<records::BattleTask>(s);
    rv::Store<std::int32_t>(cur_task->step_x, sixteenth(static_cast<U>(rv::As<records::BattleEnemy>(EnemyByIndex(B(::bof3::Img(at::kActor.v))))->x), static_cast<U>(cur_task->x)));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<std::int32_t>(cur_task->step_z, sixteenth(static_cast<U>(rv::As<records::BattleEnemy>(EnemyByIndex(B(::bof3::Img(at::kActor.v))))->z), static_cast<U>(cur_task->z)));
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->step_y, 0x40);
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->work_20, -8);
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<std::int32_t>(cur_task->work_18, sixteenth(static_cast<U>(rv::As<records::BattleEnemy>(EnemyByIndex(B(::bof3::Img(at::kActor.v))))->height), static_cast<U>(cur_task->height)));
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DA60: step 5, the flight: +0x34 += +0xC, +0x38 += +0x10, the
// word +0x3E += the word +0x14, then the dword +0x3C += +0x18 (it holds that
// word), +0x14 += +0x20; at +0x14 = -0x40 +2 up by one. A tail jump to
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BossWeretigrFx_Fly(void) {
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    rv::Store<std::int32_t>(cur_task->x, S(static_cast<U>(cur_task->x) + static_cast<U>(cur_task->step_x)));
    rv::Store<std::int32_t>(cur_task->z, S(static_cast<U>(cur_task->z) + static_cast<U>(cur_task->step_z)));
    rv::Store<unsigned>(cur_task->height_whole, cur_task->height_whole + Word(s + 0x14));
    rv::Store<std::int32_t>(cur_task->height, S(static_cast<U>(cur_task->height) + static_cast<U>(cur_task->work_18)));
    rv::Store<std::int32_t>(cur_task->step_y, S(static_cast<U>(cur_task->step_y) + static_cast<U>(cur_task->work_20)));
    if (cur_task->step_y == -0x40) s[2] = static_cast<unsigned char>(s[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DAC0: step 6. The acting enemy's object (0x904B34) +0 &= 0xBF;
// Sprite_Current = the task's owner 0x93B940 for Sprite_SetAnimation(0), then
// back to the task (the one held before); BattleTask_FreeCurrent.
extern "C" void __cdecl BossWeretigrFx_Finish(void) {
    unsigned char* const task = Sprite_Current;
    EnemyByIndex(B(::bof3::Img(at::kActor.v)))[0] &= 0xBF;
    Sprite_Current = At(BattleE2_Owner);
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current = task;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x43DB10: +1 = 1, the trail: TaskSteps through
// BossWeretigrFx_TrailSteps (2).
extern "C" void __cdecl BossWeretigrFx_Trail(void) { TaskSteps("BossWeretigrFx_Trail", AddressOf(BossWeretigrFx_TrailSteps), 2); }

// original 0x43DB40: trail step 0: +0 |= 0x20, +0x5C = 3, Shade, +9 = 8, +2 up
// by one; a tail jump to Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BossWeretigrFx_TrailBegin(void) {
    rv::Current<records::BattleTask>()->flags |= 0x20;
    Sprite_Current[0x5C] = 3;
    Shade();
    Sprite_Current[9] = 8;
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DBA0: trail step 1: Shade; Sprite_ScriptTickOnce; the
// Sprite_Current of after +9 down by one - at 0 before it, a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BossWeretigrFx_TrailFade(void) {
    Shade();
    BH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    s[9] = static_cast<unsigned char>(count - 1);
    if (count == 0) BH_CALL(BattleTask_FreeCurrent)();
}

void BossWeretigrFx_Inject() {
    BOF3_INJECT(BossWeretigrFx_Task);
    BOF3_INJECT(BossWeretigrFx_Main);
    BOF3_INJECT(BossWeretigrFx_Begin);
    BOF3_INJECT(BossWeretigrFx_AwaitPose);
    BOF3_INJECT(BossWeretigrFx_Rise);
    BOF3_INJECT(BossWeretigrFx_Strike);
    BOF3_INJECT(BossWeretigrFx_Leap);
    BOF3_INJECT(BossWeretigrFx_Fly);
    BOF3_INJECT(BossWeretigrFx_Finish);
    BOF3_INJECT(BossWeretigrFx_Trail);
    BOF3_INJECT(BossWeretigrFx_TrailBegin);
    BOF3_INJECT(BossWeretigrFx_TrailFade);
}
