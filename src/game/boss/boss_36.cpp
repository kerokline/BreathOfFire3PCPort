// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_36.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_36_callees.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_36::at;

}  // namespace

namespace {

using boss_harness::Handler;

using U = std::uint32_t;

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

// original 0x43EB00: Boss_SetupTable[36]. The hooks: end Boss36_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss36_Setup(void) {
    StoreHooksBossSh(::bof3::Code(bof3::addr::Boss36_End), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43EB20: the end hook: won - chapter step 8, step 1; else step 2.
extern "C" void __cdecl Boss36_End(void) { EndWithStep(8); }

// original 0x43EB40: Boss_SetupTable[43]. As set-up 41: BossHook_EndPickWay,
// BareRet, BareRetZero.
extern "C" void __cdecl Boss43_Setup(void) {
    StoreHooksBossSh(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43EB60: the end hook (0x904B64) of ten set-ups. Won (0x904AE8
// bit 1): the chapter's step 0x8034E5 = 5 and a tail jump to 0x446DE0 (the end
// phase, step 1); otherwise to 0x446E00 (step 2).
extern "C" void __cdecl BossHook_EndPickWay(void) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = 5;
        BH_AT(Handler, boss_h::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_h::at::kEndOther.v)();
}

void Boss36_Inject() {
    BOF3_INJECT(Boss36_Setup);
    BOF3_INJECT(Boss36_End);
    BOF3_INJECT(Boss43_Setup);
    BOF3_INJECT(BossHook_EndPickWay);
}
