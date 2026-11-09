// Battle set-up: group BA of the seventh round, nineteen functions of the
// battle engine (PC 0x42E400..0x4551A0, the PSX's BATTLE.EMI compiled into
// the exe), each read whole against its PSX twin. docs/battle_setup.md.
//
//   - The per-turn status chain: the sub-state machine 0x430510 (not ours)
//     calls nine steps one per state, stopping at the first that answers 1
//     (it showed something): BattleStep_Expire4000 0x430640, _Restore800
//     0x430790, _PartyWake40 0x430890, _EnemyWake40 0x430970, _PartyWake20
//     0x430A50, _EnemyWake20 0x430B30, _Status80 0x430C10, _HpDrift 0x430D40,
//     _ApUpkeep 0x430F40 (PSX 0x801D524C .. 0x801D62A8, the same order).
//     Each skips an actor Battle_ActorSkipped 0x431030 names.
//   - Battle_TickCounters 0x4303D0 (the counters those steps test), and
//     Battle_ClearActingFlags 0x4301B0 (the acting actor's flags 0x40 / 0x80).
//   - Battle_MarkFasterSide 0x4453C0 with its tests Battle_ActorStanding
//     0x445550 and Battle_PartyOutpaces 0x4455C0 (PSX 0x801DB020 ..).
//   - Battle_ClearStatus 0x44F4B0 (PSX 0x800A0680), Battle_OpenMsgWindow
//     0x444310, Battle_ReturnQueuedItem 0x446EA0, Battle_OpeningMessage
//     0x44AA00.
//
// Every call goes through battle_setup::g (battle_setup_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/battle_setup.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_setup_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"

namespace battle_setup {

namespace {
template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kActorAbsent.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kActorCanAct.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kSetPending.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kMessage.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kMessageAt.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kStatusTint.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kPartyName.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kEnemyName.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kWakeRoll.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kSetHpChange.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kEnemyOutpaces.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kReturnItem.v)),
    Sprite_ReleaseTint,
    Msg_SystemPtr,
    Window_Alloc,
    Rand,
    Battle_ActorSkipped,
    Battle_OpenMsgWindow,
    Battle_ClearStatus,
    Battle_ActorStanding,
    Battle_PartyOutpaces,
};
Callees g = kOriginals;

}  // namespace battle_setup

using namespace battle_setup;

namespace {

using move_script::At;
// move_script::Long is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* P(unsigned actor) { return move_script::At(battle_setup::at::Party(actor)); }
unsigned char* E(unsigned actor) { return move_script::At(battle_setup::at::Enemy(actor)); }
bool Yes(unsigned al) { return (al & 0xFFu) != 0; }

}  // namespace

// ===========================================================================
// The small helpers

// ===========================================================================
// The set-up and per-turn bookkeeping

// ===========================================================================
// The chain's nine steps (0x430510 calls them in this order)

// ===========================================================================

void BattleSetup_Inject() {
    if (bof3::WantsShadow("battle_setup")) battle_setup::SelfTest();
}
