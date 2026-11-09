// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sa.cpp (docs/source-layout-round.md).
#include "game/boss/boss_gary.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_gary_callees.h"
#include "game/boss_h.h"
#include "game/boss_h_callees.h"
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

namespace at = boss_gary::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::AnswerHandler;

using boss_harness::Hook;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
template <typename T> U AddressOf(T* p) { return Low32(p); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through whatever follows). The handler's eax passes through
// the jmp; EnemyRunAll does not read it.
U DispatchBossSa(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return reinterpret_cast<AnswerHandler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)] - the handler gets
// the same word (the jmp leaves the caller's argument in place).
U HookDispatch(const char* who, U table, unsigned entries, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= entries)
        bof3::Fatal("%s: hook word %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, i, entries, (unsigned)table);
    return reinterpret_cast<Hook>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// A kind's state 0 (0x437A30's shape, five kinds): 0x939AD8's +0xFC (the
// animation bytes), +0xF4 (the hook), +0xF8 (the cue words) - 0x939AD8 read
// before each store -, Sprite_Current +1 = 2 (the idle step), and a tail jump
// to Sprite_ScriptTick (its al the answer).
void Install(U anims, U hook, U cues) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(anims);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(cues);
}

unsigned char EnterTick() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// Gary's and Mogu's end: Sprite_Current's +0x38 up by 0x8000 (half a unit),
// its ground word +0x3E the map's elevation there, animation bank 0x5F,
// 0x939AD8's +0xFC the kind's second animation bytes, +0x2A (the flip) 0,
// then the animation - Sprite_Current read again after each call.
void EndStep(U anims, unsigned animation) {
    AddLong(Sprite_Current + 0x38, 0x8000);
    unsigned char* const s = Sprite_Current;
    const long ground = BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground) & 0xFFFF);
    BH_CALL(Sprite_SetAnimationBank)(0x5F);
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(anims);
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

}  // namespace

// original 0x437A10: BossKind_Table[6]: jmp [BossGary_Steps + 4 * +1] (12).
extern "C" U __cdecl BossGary_Dispatch(void) { return DispatchBossSa("BossGary_Dispatch", AddressOf(BossGary_Steps), 12, 1); }

// original 0x437A30: BossGary_Steps 0 - +0xFC BossGary_Anims, +0xF4
// BossGary_Hook, +0xF8 BossGary_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGary_Enter(void) {
    Install(AddressOf(BossGary_Anims), ::bof3::Code(at::kGaryHook.v), AddressOf(BossGary_Cues));
    return EnterTick();
}

// original 0x437A70: BossGary_Steps 6 - jmp [BossGary_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossGary_ActDispatch(void) { return DispatchBossSa("BossGary_ActDispatch", AddressOf(BossGary_ActSubs), 6, 2); }

// original 0x437A90: BossGary_Steps 11 - jmp [BossGary_EndSteps + 4 * +2] (3).
extern "C" U __cdecl BossGary_EndDispatch(void) { return DispatchBossSa("BossGary_EndDispatch", AddressOf(BossGary_EndSteps), 3, 2); }

// original 0x437AB0: BossGary_EndSteps 0 - BattleEnemy_SetAnimation(4),
// MoveCmd_TestFB(0x63, 0x14) (its answer not read), Sprite_Current (read
// after the calls) +2 up by one.
extern "C" void __cdecl BossGary_EndStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    BH_CALL(MoveCmd_TestFB)(0x63, 0x14);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x437AD0: BossGary_EndSteps 1 - once Mogu's count 0x904B7E is 0:
// the end step (BossGary_AnimsEnd, animation 1), 0x904AE8 |= 4 (read after
// the call), MoveCmd_TestFB(0x64, 0x14), +2 up by one. Every frame a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGary_EndAwait(void) {
    if (BossSa_EndCount == 0) {
        EndStep(AddressOf(BossGary_AnimsEnd), 1);
        Battle_EndFlags |= 4;
        BH_CALL(MoveCmd_TestFB)(0x64, 0x14);
        rv::Current<records::BattleEnemy>()->step += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x437B60: Gary's +0xF4 hook - jmp [BossGary_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossGary_Hook(unsigned word) { return HookDispatch("BossGary_Hook", AddressOf(BossGary_Hooks), 3, word); }

// original 0x437B70: BossKind_Table[7]: jmp [BossMogu_Steps + 4 * +1] (12).
extern "C" U __cdecl BossMogu_Dispatch(void) { return DispatchBossSa("BossMogu_Dispatch", AddressOf(BossMogu_Steps), 12, 1); }

// original 0x437B90: BossMogu_Steps 0 - +0xFC BossMogu_Anims, +0xF4
// BossMogu_Hook, +0xF8 BossMogu_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMogu_Enter(void) {
    Install(AddressOf(BossMogu_Anims), ::bof3::Code(at::kMoguHook.v), AddressOf(BossMogu_Cues));
    return EnterTick();
}

// original 0x437BD0: BossMogu_Steps 6 - jmp [BossMogu_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossMogu_ActDispatch(void) { return DispatchBossSa("BossMogu_ActDispatch", AddressOf(BossMogu_ActSubs), 6, 2); }

// original 0x437BF0: BossMogu_Steps 11 - jmp [BossMogu_EndSteps + 4 * +2] (3).
extern "C" U __cdecl BossMogu_EndDispatch(void) { return DispatchBossSa("BossMogu_EndDispatch", AddressOf(BossMogu_EndSteps), 3, 2); }

// original 0x437C10: BossMogu_EndSteps 0 - BattleEnemy_SetAnimation(4), the
// count 0x904B7E = 0x3C, Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossMogu_EndStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    BossSa_EndCount = 0x3C;
    cur_enemy->step += 1;
}

// original 0x437C30: BossMogu_EndSteps 1 - the count 0x904B7E down by one;
// at 0 the end step (BossMogu_AnimsEnd, animation 0) and +2 up by one. Every
// frame a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMogu_EndCount(void) {
    const unsigned count = (BossSa_EndCount - 1u) & 0xFFFF;
    BossSa_EndCount = count;
    if (count == 0) {
        EndStep(AddressOf(BossMogu_AnimsEnd), 0);
        rv::Current<records::BattleEnemy>()->step += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x437CA0: jmp Sprite_ScriptTick - a state entry of kinds 3, 6, 7,
// 40 and 8..11 (BossTorast_DeathSubs 1): the sprite's script ticked, its al
// the answer.
extern "C" unsigned char __cdecl BossOp_ScriptTick(void) { return BH_CALL(Sprite_ScriptTick)(); }

// original 0x437CB0: Mogu's +0xF4 hook - jmp [BossMogu_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossMogu_Hook(unsigned word) { return HookDispatch("BossMogu_Hook", AddressOf(BossMogu_Hooks), 3, word); }

void BossGary_Inject() {
    BOF3_INJECT(BossGary_Dispatch);
    BOF3_INJECT(BossGary_Enter);
    BOF3_INJECT(BossGary_ActDispatch);
    BOF3_INJECT(BossGary_EndDispatch);
    BOF3_INJECT(BossGary_EndStart);
    BOF3_INJECT(BossGary_EndAwait);
    BOF3_INJECT(BossGary_Hook);
    BOF3_INJECT(BossMogu_Dispatch);
    BOF3_INJECT(BossMogu_Enter);
    BOF3_INJECT(BossMogu_ActDispatch);
    BOF3_INJECT(BossMogu_EndDispatch);
    BOF3_INJECT(BossMogu_EndStart);
    BOF3_INJECT(BossMogu_EndCount);
    BOF3_INJECT(BossOp_ScriptTick);
    BOF3_INJECT(BossMogu_Hook);
}
