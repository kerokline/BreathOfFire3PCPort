// Moved here by tools/move_funcs.py from boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_angler_fx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sh::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's word -
// reads that word), its eax the dispatcher's answer.
using EntryBossSh = U (__cdecl*)(U);

EntryBossSh EntryAtBossSh(U table, unsigned index) {
    return reinterpret_cast<EntryBossSh>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSh(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryAtBossSh(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

}  // namespace

// original 0x43EB80: the kind-3 dispatcher's slot 3 (BattleBossFx_Dispatch by
// the slot's +5): jmp [BossAnglerFx_States + 4 * +1] - one entry,
// BossAnglerFx_Run.
extern "C" unsigned long __cdecl BossAnglerFx_Dispatch(unsigned long through) {
    return DispatchBossSh("BossAnglerFx_Dispatch", AddressOf(BossAnglerFx_States), 1, 1, through);
}

// original 0x43EBA0: by Sprite_Current +2 through a three-entry table built on
// the stack - BossAnglerFx_Start, BattleFx_ScriptUntilDone, BattleFx_FreeTask;
// `call [esp + 4 * +2]`, unchecked (past 2 the original calls whatever the
// stack holds above the table: ours aborts). The entry's eax the answer.
extern "C" unsigned long __cdecl BossAnglerFx_Run(void) {
    const unsigned step = Sprite_Current[2];
    using Step = U (__cdecl*)();
    switch (step) {
    case 0: return BH_AT(Step, bof3::addr::BossAnglerFx_Start)();
    case 1: return BH_AT(Step, bof3::addr::BattleFx_ScriptUntilDone)();
    case 2: return BH_CALL(BattleFx_FreeTask)();
    default:
        bof3::Fatal("BossAnglerFx_Run: +2 is %u, past its 3 steps - the original calls through the stack above its table "
                    "(docs/boss_sh.md section 6)",
                    step);
    }
}

// original 0x43EBD0: step 0. Sprite_Current +0xB = 0 and +9 = 0 (read again);
// Sprite_SetAnimation(1); Sprite_ScriptTick and Sprite_QueueOverlay (answers
// not read); +2 up (Sprite_Current read again).
extern "C" void __cdecl BossAnglerFx_Start(void) {
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    BH_CALL(Sprite_SetAnimation)(1);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43EC10: step 1 of the Angler's task and of three spell tasks'
// (magic_s09 / s15 / s31 call it by its address): Sprite_ScriptTick and
// Sprite_QueueOverlay (answers not read); with the round flags' bit 2 (the
// done flag an enemy's action end sets), +2 up.
extern "C" void __cdecl BattleFx_ScriptUntilDone(void) {
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (B(::bof3::Img(at::kFlags.v)) & 4) Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

void BossAnglerFx_Inject() {
    BOF3_INJECT(BossAnglerFx_Dispatch);
    BOF3_INJECT(BossAnglerFx_Run);
    BOF3_INJECT(BossAnglerFx_Start);
    BOF3_INJECT(BattleFx_ScriptUntilDone);
}
