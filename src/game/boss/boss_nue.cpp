// Moved here by tools/move_funcs.py from boss_sa.cpp (docs/source-layout-round.md).
#include "game/boss/boss_nue.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
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

using boss_harness::AnswerHandler;

using boss_harness::Hook;

using move_script::At;

using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
template <typename T> U AddressOf(T* p) { return Low32(p); }

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

// Kinds 1 and 39's end: Sprite_ScriptTickOnce (its answer not read); then
// MoveCmd_OpE9 on the field actor tagged 0 (BossActor_Index(0), its object +
// 0x80) with (0, -14, 0x40, 0xFC00, 0xFF, 0); while it answers al not 0,
// nothing more. Then Battle_ClearActorBit(Sprite_Current +5) and 0x904AE8
// bit 1 (the win). Answers whether the walk is over.
bool EndWalk() {
    BH_CALL(Sprite_ScriptTickOnce)();
    const unsigned index = BH_CALL(BossActor_Index)(0);
    unsigned char* const actor = At(::bof3::Img(Sprite_Objects_addr) + 0x80 + index * at::kObjectStride);
    if (BH_CALL(MoveCmd_OpE9)(actor, 0, -0xE, 0x40, 0xFC00, 0xFF, 0) != 0) return false;
    BH_CALL(Battle_ClearActorBit)(Sprite_Current[5]);
    return true;
}

// The hooks' action-pick entry of kinds 1 and 39: an HP of 0xFFFF (the mark
// the hit entry leaves) makes the action's kind 3 and Sprite_Current +4 0.
bool PickSpent() {
    if (rv::As<records::BattleEnemy>(Enemy())->hp != 0xFFFF) return false;
    unsigned char* const s = Sprite_Current;
    B(::bof3::Img(at::kActionKind.v)) = 3;
    s[4] = 0;
    return true;
}

}  // namespace

// original 0x437EE0: BossKind_Table[1]: jmp [BossNue_Steps + 4 * +1] (12).
extern "C" U __cdecl BossNue_Dispatch(void) { return DispatchBossSa("BossNue_Dispatch", AddressOf(BossNue_Steps), 12, 1); }

// original 0x437F00: BossNue_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossNue_Hook, +0xF8 BossNue_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossNue_Enter(void) {
    Install(AddressOf(BossNue_Anims), ::bof3::Code(at::kNueHook.v), AddressOf(BossNue_Cues));
    return EnterTick();
}

// original 0x437F40: BossNue_Steps 9 - jmp [BossNue_EndSteps + 4 * +2] (2).
extern "C" U __cdecl BossNue_EndDispatch(void) { return DispatchBossSa("BossNue_EndDispatch", AddressOf(BossNue_EndSteps), 2, 2); }

// original 0x437F60: BossNue_EndSteps 0 - Sprite_SetAnimation(6),
// Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossNue_EndPose(void) {
    BH_CALL(Sprite_SetAnimation)(6);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x437F80: BossNue_EndSteps 1 - the walk (EndWalk); when it is over,
// 0x904AE8 |= 2 (the win, read after the calls) and a tail jump to
// Effect_Release.
extern "C" void __cdecl BossNue_EndMove(void) {
    if (!EndWalk()) return;
    Battle_EndFlags |= 2;
    BH_CALL(Effect_Release)();
}

// original 0x437FF0: Nue's +0xF4 hook - jmp [BossNue_Hooks + 4 * (word &
// 0xFF)] (3: BossNue_HookPick, BossNue_HookHit, BareRet).
extern "C" U __cdecl BossNue_Hook(unsigned word) { return HookDispatch("BossNue_Hook", AddressOf(BossNue_Hooks), 3, word); }

// original 0x438000: BossNue_Hooks 0 (the hook's call at the action pick,
// 0x435BF5): with HP 0xFFFF, the action's kind 0x904B35 = 3, Sprite_Current
// +4 = 0 and Sound_PlayById(0x602). The word is not read.
extern "C" void __cdecl BossNue_HookPick(unsigned) {
    if (PickSpent()) BH_CALL(Sound_PlayById)(0x602);
}

// original 0x438030: BossNue_Hooks 1 and BossWeretigr_Hooks 1 (the hook's call
// at the hit, 0x4367EE): 0x939AD8's HP +0xA4 less the signed word +0x108 (the
// pending damage), when at or below a quarter of +0xB0 (the maximum, shifted
// right twice; a signed compare): 0x904AAD |= 1 and HP = 0xFFFF. The word is
// not read.
extern "C" void __cdecl BossNue_HookHit(unsigned) {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    const std::int32_t left = static_cast<std::int32_t>(enemy_rec->hp) - static_cast<std::int16_t>(static_cast<std::uint16_t>(enemy_rec->hp_change));
    const std::int32_t quarter = static_cast<std::int32_t>(enemy_rec->max_hp >> 2);
    if (left > quarter) return;
    BossSe_Script |= 1;
    enemy_rec->hp = 0xFFFF;
}

// original 0x438070: BossKind_Table[2]: jmp [BossNue2_Steps + 4 * +1] (12).
extern "C" U __cdecl BossNue2_Dispatch(void) { return DispatchBossSa("BossNue2_Dispatch", AddressOf(BossNue2_Steps), 12, 1); }

// original 0x438090: BossNue2_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossNue2_Hook, +0xF8 BossNue_Cues (kind 1's two tables), +1 = 2,
// Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossNue2_Enter(void) {
    Install(AddressOf(BossNue_Anims), ::bof3::Code(at::kNue2Hook.v), AddressOf(BossNue_Cues));
    return EnterTick();
}

// original 0x4380D0: BossNue2_Steps 6 - jmp [BossNue2_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossNue2_ActDispatch(void) { return DispatchBossSa("BossNue2_ActDispatch", AddressOf(BossNue2_ActSubs), 6, 2); }

// original 0x4380F0: kind 2's +0xF4 hook - jmp [BossNue2_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossNue2_Hook(unsigned word) { return HookDispatch("BossNue2_Hook", AddressOf(BossNue2_Hooks), 3, word); }

// original 0x438100: BossKind_Table[46]: jmp [BossSample1_Steps + 4 * +1] (12).
extern "C" U __cdecl BossSample1_Dispatch(void) {
    return DispatchBossSa("BossSample1_Dispatch", AddressOf(BossSample1_Steps), 12, 1);
}

// original 0x438120: BossSample1_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossSample1_Hook, +0xF8 BossSample1_Cues, then 0x939AD8's +0x114 |= 8,
// +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample1_Enter(void) {
    Install(AddressOf(BossNue_Anims), ::bof3::Code(at::kSample1Hook.v), AddressOf(BossSample1_Cues));
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8u);
    return EnterTick();
}

// original 0x438180: kind 46's +0xF4 hook - jmp [BossSample1_Hooks + 4 *
// (word & 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossSample1_Hook(unsigned word) {
    return HookDispatch("BossSample1_Hook", AddressOf(BossSample1_Hooks), 3, word);
}

void BossNue_Inject() {
    BOF3_INJECT(BossNue_Dispatch);
    BOF3_INJECT(BossNue_Enter);
    BOF3_INJECT(BossNue_EndDispatch);
    BOF3_INJECT(BossNue_EndPose);
    BOF3_INJECT(BossNue_EndMove);
    BOF3_INJECT(BossNue_Hook);
    BOF3_INJECT(BossNue_HookPick);
    BOF3_INJECT(BossNue_HookHit);
    BOF3_INJECT(BossNue2_Dispatch);
    BOF3_INJECT(BossNue2_Enter);
    BOF3_INJECT(BossNue2_ActDispatch);
    BOF3_INJECT(BossNue2_Hook);
    BOF3_INJECT(BossSample1_Dispatch);
    BOF3_INJECT(BossSample1_Enter);
    BOF3_INJECT(BossSample1_Hook);
}
