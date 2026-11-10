// Moved here by tools/move_funcs.py from boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_24.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_se::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

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

// original 0x43B870: Boss_SetupTable[24]. The hooks: end Boss24_End, exit
// Boss24_Exit, event BareRetZero.
extern "C" void __cdecl Boss24_Setup(void) {
    StoreHooksBossSe(::bof3::Code(bof3::addr::Boss24_End), ::bof3::Code(bof3::addr::Boss24_Exit), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43B890: the end hook. 0x904AE8 read; won (bit 1): variable 3 =
// 5, 0x904AE8 = the value read | 8, Music_Track = 0x44, a tail jump to
// 0x446DE0 (step 1). Otherwise a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss24_End(void) {
    const unsigned char end = Battle_EndFlags;
    if ((end & 2) == 0) {
        BH_AT(Handler, at::kEndOther.v)();
        return;
    }
    B(::bof3::Img(at::kScriptVar3.v)) = 5;
    Battle_EndFlags = static_cast<unsigned char>(end | 8);
    Music_Track = 0x44;
    BH_AT(Handler, at::kEndWin.v)();
}

// original 0x43B8C0: the exit hook: BossActor_Clear(3).
extern "C" void __cdecl Boss24_Exit(void) { BH_CALL(BossActor_Clear)(3); }

// original 0x43B8D0: Boss_SetupTable[48]. The hooks: end BossHook_EndPickWay,
// exit BareRet, event BareRetZero.
extern "C" void __cdecl Boss48_Setup(void) {
    StoreHooksBossSe(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

void Boss24_Inject() {
    BOF3_INJECT(Boss24_Setup);
    BOF3_INJECT(Boss24_End);
    BOF3_INJECT(Boss24_Exit);
    BOF3_INJECT(Boss48_Setup);
}
