// Moved here by tools/move_funcs.py from boss_se.cpp, boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_35.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_35_callees.h"
#include "game/boss_harness.h"
#include "game/boss_se_callees.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_35::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

using boss_harness::Handler;

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSh(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The end hooks of set-ups 35 and 36: won (0x904AE8 bit 1) - the chapter step
// `step`, then a tail jump to 0x446DE0 (the end phase, step 1); else to
// 0x446E00 (step 2).
void EndWithStep(unsigned char step) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = step;
        BH_AT(Handler, boss_sh::at::kEndWin.v)();
    } else {
        BH_AT(Handler, boss_sh::at::kEndOther.v)();
    }
}

}  // namespace

// original 0x43E730: Boss_SetupTable[35]. The hooks: end Boss35_End, exit
// BossHook_ExitTransition4 (0x43E790, set-up 26's too - group BSE's), event
// Boss35_Event.
extern "C" void __cdecl Boss35_Setup(void) {
    StoreHooksBossSh(::bof3::Code(bof3::addr::Boss35_End), ::bof3::Code(at::kExitTransition4.v), ::bof3::Code(bof3::addr::Boss35_Event));
}

// original 0x43E750: the event hook: code 0 (the low byte) and
// Battle_ActorIsOut(3) (enemy 0) answering al not 0 - 0x904AE8 |= 2 (the win).
// al 0.
extern "C" unsigned char __cdecl Boss35_Event(unsigned code) {
    if ((code & 0xFF) == 0 && (BH_CALL(Battle_ActorIsOut)(3) & 0xFF) != 0)
        Battle_EndFlags = static_cast<unsigned char>(Battle_EndFlags | 2);
    return 0;
}

// original 0x43E770: the end hook: won - chapter step 0x15, step 1; else step 2.
extern "C" void __cdecl Boss35_End(void) { EndWithStep(0x15); }

// original 0x43E790: the exit hook of set-ups 26 and 35 (35 is group BSH's):
// round-flag bit 1 set, Transition_Start(4).
extern "C" void __cdecl BossHook_ExitTransition4(void) {
    B(::bof3::Img(boss_se::at::kFlags.v)) |= 2;
    BH_CALL(Transition_Start)(4);
}

// original 0x43E7A0: Boss_SetupTable[47]. As set-up 41: BossHook_EndPickWay,
// BareRet, BareRetZero.
extern "C" void __cdecl Boss47_Setup(void) {
    StoreHooksBossSh(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

void Boss35_Inject() {
    BOF3_INJECT(Boss35_Setup);
    BOF3_INJECT(Boss35_Event);
    BOF3_INJECT(Boss35_End);
    BOF3_INJECT(BossHook_ExitTransition4);
    BOF3_INJECT(Boss47_Setup);
}
