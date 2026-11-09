// Moved here by tools/move_funcs.py from boss_sd.cpp, boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_23.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_23_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sd_callees.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_23::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// The end phase's steps (nobody's; the harness's standard set).
void EndStep(U step) { BH_WORD(Handler, step)(); }

// Set-ups 17 and 21's end hooks: the win sets the field's move-script counter
// 0 (`counter`) and takes the end phase's step 1, else step 2.
void EndCounter(unsigned char counter) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kCounter0.v)) = counter;
        EndStep(boss_sd::at::kEndWin.v);
        return;
    }
    EndStep(boss_sd::at::kEndOther.v);
}

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSe(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

}  // namespace

// original 0x43B710: Boss_SetupTable[23]. The hooks: end 0x43B730 (set-up 21's
// too, group BSD's), exit BossHook_ExitClearActors012, event BareRetZero.
extern "C" void __cdecl Boss23_Setup(void) {
    StoreHooksBossSe(::bof3::Code(at::kEnd21.v), ::bof3::Code(bof3::addr::BossHook_ExitClearActors012), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43B730: the end hook of set-ups 21 and 23: the win sets the
// field's move-script counter 0 to 0xA and jumps to the end phase's step 1;
// else step 2.
extern "C" void __cdecl Boss21_End(void) { EndCounter(0xA); }

// original 0x43B750: the exit hook of set-ups 23 and 30: BossActor_Clear(0),
// (1), (2).
extern "C" void __cdecl BossHook_ExitClearActors012(void) {
    BH_CALL(BossActor_Clear)(0);
    BH_CALL(BossActor_Clear)(1);
    BH_CALL(BossActor_Clear)(2);
}

void Boss23_Inject() {
    BOF3_INJECT(Boss23_Setup);
    BOF3_INJECT(Boss21_End);
    BOF3_INJECT(BossHook_ExitClearActors012);
}
