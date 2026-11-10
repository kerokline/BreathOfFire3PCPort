// Moved here by tools/move_funcs.py from boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_d_lord_fx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sj::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using boss_harness::Phase;

using move_script::At;

using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// An effect task's dispatcher: the same jmp by +1 through a table of the
// task's states, which take no word.
void DispatchTask(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// A stack table: sub esp, 4n; mov [esp + 4i], handler i; call [esp + 4 * +2].
// The handlers are the group's own (or BattleFx_FreeTask), called by their
// addresses; past n the original calls through its caller's frame.
void Steps(const char* who, const U* handlers, unsigned n) {
    const unsigned step = Sprite_Current[2];
    if (step >= n)
        bof3::Fatal("%s: step byte +2 is %u, past its stack table of %u - the original calls through its caller's frame "
                    "(docs/boss_sj.md section 6)",
                    who, step, n);
    Phase(handlers[step])();
}

}  // namespace

// original 0x43FAC0: slot 4: jmp [BossDLordFx_States + 4 * Sprite_Current +1]
// (one entry).
extern "C" void __cdecl BossDLordFx_Dispatch(void) { DispatchTask("BossDLordFx_Dispatch", AddressOf(BossDLordFx_States), 1); }

// original 0x43FAE0: BossDLordFx_States 0: by +2 through a stack table of
// BossDLordFx_Start, BossDLordFx_Count, BattleFx_FreeTask.
extern "C" void __cdecl BossDLordFx_StepDispatch(void) {
    static const U kSteps[] = {bof3::addr::BossDLordFx_Start, bof3::addr::BossDLordFx_Count, bof3::addr::BattleFx_FreeTask};
    Steps("BossDLordFx_StepDispatch", kSteps, 3);
}

// original 0x43FB10: step 0. Sprite_Current +0xB = 0; +9 = the effect size of
// enemy 0's data record (the byte 0x8C5652 + 0x8C * enemy 0's +0xF0, as
// BattleActor_FxSize reads it - unchecked, the record index is the enemy's
// own); Sprite_SetAnimation(4), Sprite_ScriptTick, Sprite_QueueOverlay; +2 up
// by one (Sprite_Current read after the calls).
extern "C" void __cdecl BossDLordFx_Start(void) {
    Sprite_Current[0xB] = 0;
    const unsigned type = Battle_Enemies[0].data_id;
    Sprite_Current[9] = At(::bof3::Img(at::kEnemyFxSize.v) + type * at::kEnemyDataStride)[0];
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    ++Sprite_Current[2];
}

// original 0x43FB60: step 1. The count +9: at 0xFF nothing; at 0
// Sound_PlayEffect(0x603) and (Sprite_Current read after the call) +9 = 0xFF;
// else down by one. Sprite_ScriptTick, Sprite_QueueOverlay; with round-flag
// bit 2 (0x904AA8) +2 up by one.
extern "C" void __cdecl BossDLordFx_Count(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count != 0xFF) {
        if (count == 0) {
            BH_CALL(Sound_PlayEffect)(0x603);
            Sprite_Current[9] = 0xFF;
        } else {
            s[9] = static_cast<unsigned char>(count - 1);
        }
    }
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (B(::bof3::Img(at::kFlags.v)) & 4) ++Sprite_Current[2];
}

void BossDLordFx_Inject() {
    BOF3_INJECT(BossDLordFx_Dispatch);
    BOF3_INJECT(BossDLordFx_StepDispatch);
    BOF3_INJECT(BossDLordFx_Start);
    BOF3_INJECT(BossDLordFx_Count);
}
