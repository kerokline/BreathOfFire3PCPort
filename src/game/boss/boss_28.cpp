// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sf.cpp (docs/source-layout-round.md).
#include "game/boss/boss_28.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_28_callees.h"
#include "game/boss_h.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sf.h"
#include "game/boss_sf_callees.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_28::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSf(U end, U exit, U event) {
    BattleHook_End = end;
    BattleHook_Exit = exit;
    BattleHook_Event = event;
}

}  // namespace

// original 0x43C9D0: Boss_SetupTable[28]. The hooks: end Boss28_End, exit
// BossHook_ExitClearActor0, event BareRetZero (BH's).
extern "C" void __cdecl Boss28_Setup(void) {
    StoreHooksBossSf(::bof3::Code(bof3::addr::Boss28_End), ::bof3::Code(bof3::addr::BossHook_ExitClearActor0), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43C9F0: xor al, al; ret - the event hook (0x904B6C) of 39
// set-ups: al 0, so BattleRoundEnd_NextRound's "al 0xFF holds the round"
// never holds. The original clears al alone; ours all of eax (callers read al).
extern "C" unsigned char __cdecl BareRetZero(unsigned) { return 0; }

// original 0x43CA00: the end hook. The win (0x904AE8 bit 1): movement-script
// variable 3 = 0x1E and a tail jump to 0x446DE0 (step 1); else 0x446E00.
extern "C" void __cdecl Boss28_End(void) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kScriptVar3.v)) = 0x1E;
        BH_AT(Handler, boss_sf::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sf::at::kEndOther.v)();
}

void Boss28_Inject() {
    BOF3_INJECT(Boss28_Setup);
    BOF3_INJECT(BareRetZero);
    BOF3_INJECT(Boss28_End);
}
