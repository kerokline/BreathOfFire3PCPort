// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_53.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
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

using boss_harness::Handler;

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void SetupBossSi(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The set-ups whose end is BH's BossHook_EndPickWay (fights 40, 42, 44, 45 -
// four copies of one body): exit BareRet, event BareRetZero.
void SetupPickWay() { SetupBossSi(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// The set-ups with an end of their own: exit BareRet, event BareRetZero.
void SetupOwnEnd(U end) { SetupBossSi(end, ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// An end hook of five copies (fights 37, 49, 51, 53): with 0x904AE8 bit 1 (the
// win) the chapter's step 0x8034E5 = step and a tail jump to 0x446DE0 (the end
// phase, step 1); otherwise a tail jump to 0x446E00 (step 2).
void EndStepBossSi(unsigned step) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(step);
        BH_AT(Handler, boss_si::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_si::at::kEndOther.v)();
}

}  // namespace

// original 0x43F040: Boss_SetupTable[40]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss40_Setup(void) { SetupPickWay(); }

// original 0x43F060: Boss_SetupTable[53]. End Boss53_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss53_Setup(void) { SetupOwnEnd(::bof3::Code(bof3::addr::Boss53_End)); }

// original 0x43F080: the end hook: won, the chapter's step 0x26 and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss53_End(void) { EndStepBossSi(0x26); }

void Boss53_Inject() {
    BOF3_INJECT(Boss40_Setup);
    BOF3_INJECT(Boss53_Setup);
    BOF3_INJECT(Boss53_End);
}
