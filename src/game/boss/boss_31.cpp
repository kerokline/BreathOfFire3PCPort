// Moved here by tools/move_funcs.py from boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_31.h"

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

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSg(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The exit hooks of set-ups 31 and 32: BossActor_ClearBit40(0), the actor
// BossActor_Find(0) made Field_ActiveMember and Sprite_Current,
// Sprite_SetAnimation(animation).
void ActorZero(unsigned animation) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

}  // namespace

// original 0x43D0C0: Boss_SetupTable[31]. The hooks: end Boss31_End, exit
// Boss31_Exit, event BareRetZero.
extern "C" void __cdecl Boss31_Setup(void) {
    StoreHooksBossSg(::bof3::Code(bof3::addr::Boss31_End), ::bof3::Code(bof3::addr::Boss31_Exit), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43D0E0: the end hook. Won (0x904AE8 bit 1): the chapter's step =
// 0x14 and a tail jump to 0x446DE0 (step 1). Otherwise 0x904AE5 &= 0xBF (the
// battle's music not kept) and a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss31_End(void) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = 0x14;
        BH_AT(Handler, at::kEndWin.v)();
        return;
    }
    EventBattle_Flags &= 0xBF;
    BH_AT(Handler, at::kEndOther.v)();
}

// original 0x43D110: the exit hook: ActorZero(1), then the Sprite_Current of
// after the call +0x2A = 1.
extern "C" void __cdecl Boss31_Exit(void) {
    ActorZero(1);
    Sprite_Current[0x2A] = 1;
}

void Boss31_Inject() {
    BOF3_INJECT(Boss31_Setup);
    BOF3_INJECT(Boss31_End);
    BOF3_INJECT(Boss31_Exit);
}
