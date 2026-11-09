// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_arwan_fx.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_si.h"
#include "game/boss_si_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_si::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
unsigned char* Owner() { return At(BattleE2_Owner); }

}  // namespace

// original 0x43F5F0: the kind-3 dispatcher's slot 7 (BattleBossFx_Dispatch),
// run by BattleTask_RunAll with Sprite_Current the task slot: call [esp + 4 *
// Sprite_Current +1] through a stack table of four - BossArwanFx_Start,
// BossArwanFx_Count, BossArwanFx_Finish, MagicFx_DoneAndFree -, a Fatal past
// them (the original calls through its caller's frame); the entry's eax back.
extern "C" unsigned long __cdecl BossArwanFx_Dispatch(void) {
    static const U kSteps[] = {at::kFxStart.v, at::kFxCount.v, at::kFxFinish.v, at::kFxDone.v};
    const unsigned step = rv::Current<records::BattleTask>()->state;
    if (step >= 4)
        bof3::Fatal("BossArwanFx_Dispatch: step +1 is %u, past the 4 of its stack table - the original calls through its "
                    "caller's frame (docs/boss_si.md section 6)",
                    step);
    return BH_WORD(unsigned long (__cdecl*)(), kSteps[step])();
}

// original 0x43F630: the task's step 0. +1 up by one; +9 = the +0x8A byte of
// the enemy data record ENEMY 0's +0xF0 indexes (0x93BA50, whatever enemy owns
// the task); +0xA = 0x2D; then on the owner (0x93B940, made Sprite_Current
// for the rest): +0x24 |= 0x80, the words +0x2E += 0x30 and +0x30 -= 0x10,
// +0x29 = 2, Sprite_SetAnimation(2); Sprite_Current put back.
extern "C" void __cdecl BossArwanFx_Start(void) {
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    Sprite_Current[9] = At(::bof3::Img(at::kEnemyDataCount.v) + at::kEnemyDataStride * Battle_Enemies[0].data_id)[0];
    Sprite_Current[0xA] = 0x2D;
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    Sprite_Current[0x24] |= 0x80;
    SetWord(Sprite_Current + 0x2E, (Word(Sprite_Current + 0x2E) + 0x30) & 0xFFFF);
    SetWord(Sprite_Current + 0x30, (Word(Sprite_Current + 0x30) - 0x10) & 0xFFFF);
    Sprite_Current[0x29] = 2;
    BH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current = task;
}

// original 0x43F6C0: the task's step 1. Sprite_ScriptTickOnce on the owner
// (Sprite_Current put back after); the task's +9 counts down to 0, where
// Sound_PlayEffect(0x601) and +9 = 0xFF (on the Sprite_Current of after the
// call), and stays at 0xFF; then +0xA (the Sprite_Current of now) counts down,
// and at 0 the owner (read again for each) +1 = 5, +2 = 0,
// Battle_SetTargetFlag40(the target 0x904B44) and the task's +1 (read after
// the call) up by one.
extern "C" void __cdecl BossArwanFx_Count(void) {
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    BH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = task;
    const unsigned char count = task[9];
    if (count != 0xFF) {
        if (count == 0) {
            BH_CALL(Sound_PlayEffect)(0x601);
            Sprite_Current[9] = 0xFF;
        } else {
            task[9] = static_cast<unsigned char>(count - 1);
        }
    }
    unsigned char* const now = Sprite_Current;
    const unsigned char wait = now[0xA];
    if (wait != 0) {
        now[0xA] = static_cast<unsigned char>(wait - 1);
        return;
    }
    Owner()[1] = 5;
    Owner()[2] = 0;
    BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(at::kTarget.v)));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x43F750: the task's step 2. Sprite_ScriptTickOnce on the owner;
// when it answers al not 0: Sprite_SetAnimation(0), then on the Sprite_Current
// of after the call +0x24 &= 0x7F and +0x29 = 4, and the task's +1 up by one.
// Sprite_Current put back to the task either way.
extern "C" void __cdecl BossArwanFx_Finish(void) {
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    if (BH_CALL(Sprite_ScriptTickOnce)() != 0) {
        BH_CALL(Sprite_SetAnimation)(0);
        Sprite_Current[0x24] &= 0x7F;
        Sprite_Current[0x29] = 4;
        task[1] = static_cast<unsigned char>(task[1] + 1);
    }
    Sprite_Current = task;
}

void BossArwanFx_Inject() {
    BOF3_INJECT(BossArwanFx_Dispatch);
    BOF3_INJECT(BossArwanFx_Start);
    BOF3_INJECT(BossArwanFx_Count);
    BOF3_INJECT(BossArwanFx_Finish);
}
