// Moved here by tools/move_funcs.py from boss_sa.cpp (docs/source-layout-round.md).
#include "game/boss/boss_02.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sa.h"
#include "game/boss_sa_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sa::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// Set-ups 2 and 3's end hook: without 0x904AE8 bits 1 and 2 the end phase's
// step 2 (0x446E00); with either, what `with` does, MoveScript counter 0 and
// the music track, and step 1 (0x446DE0).
template <typename F> void EndByBits(F with, unsigned counter, unsigned track) {
    if ((Battle_EndFlags & 6) == 0) {
        BH_AT(Handler, at::kEndOther.v)();
        return;
    }
    with();
    B(::bof3::Img(at::kMoveVar3.v)) = static_cast<unsigned char>(counter);
    Music_Track = static_cast<unsigned char>(track);
    BH_AT(Handler, at::kEndWin.v)();
}

void SetHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

}  // namespace

// original 0x438190: Boss_SetupTable[2] - BattleHook_End Boss02_End,
// BattleHook_Exit BareRet, BattleHook_Event Boss02_Event.
extern "C" void __cdecl Boss02_Setup(void) { SetHooks(::bof3::Code(at::kBoss02End.v), ::bof3::Code(at::kBareRet.v), ::bof3::Code(at::kBoss02Event.v)); }

// original 0x4381B0: fight 2's event hook: at phase code 0 (its low byte)
// with 0x904AE8 bit 1 (the win), the window records' pass 0x802D20 = 2 and
// Msg_OpenScript(0x19). al 0.
extern "C" unsigned char __cdecl Boss02_Event(unsigned phase) {
    if ((phase & 0xFF) == 0 && (Battle_EndFlags & 2)) {
        BossSa_WindowPass = 2;
        BH_CALL(Msg_OpenScript)(0x19);
    }
    return 0;
}

// original 0x4381E0: fight 2's end hook: with 0x904AE8 bit 1 or 2,
// BossActor_Clear(0), counter 0x24, track 0x1B, step 1; else step 2.
extern "C" void __cdecl Boss02_End(void) {
    EndByBits([] { BH_CALL(BossActor_Clear)(0); }, 0x24, 0x1B);
}

// original 0x438210: Boss_SetupTable[3] - BattleHook_End Boss03_End,
// BattleHook_Exit BossHook_ExitActor0Bit40, BattleHook_Event BareRetZero.
extern "C" void __cdecl Boss03_Setup(void) { SetHooks(::bof3::Code(at::kBoss03End.v), ::bof3::Code(at::kExitActor0Bit40.v), ::bof3::Code(at::kBareRetZero.v)); }

// original 0x438230: fight 3's end hook: with 0x904AE8 bit 1 or 2,
// BossActor_CopyFrom(0, enemy 0's object, 0) (its pose), counter 0x6D, track
// 0x18, step 1; else step 2.
extern "C" void __cdecl Boss03_End(void) {
    EndByBits([] { BH_CALL(BossActor_CopyFrom)(0, At(::bof3::Img(Battle_Enemies_addr)), 0); }, 0x6D, 0x18);
}

// original 0x438270: Boss_SetupTable[39] - BattleHook_End
// BossHook_EndPickWay, BattleHook_Exit BareRet, BattleHook_Event BareRetZero.
extern "C" void __cdecl Boss39_Setup(void) { SetHooks(::bof3::Code(at::kEndPickWay.v), ::bof3::Code(at::kBareRet.v), ::bof3::Code(at::kBareRetZero.v)); }

void Boss02_Inject() {
    BOF3_INJECT(Boss02_Setup);
    BOF3_INJECT(Boss02_Event);
    BOF3_INJECT(Boss02_End);
    BOF3_INJECT(Boss03_Setup);
    BOF3_INJECT(Boss03_End);
    BOF3_INJECT(Boss39_Setup);
}
