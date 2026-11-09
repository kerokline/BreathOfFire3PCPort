// Round eleven group BSA: 49 functions of the boss band's first units
// (tools/boss_rows.py --groups, 2026-09-28; analysis/boss_funcs.tsv's group
// column BSA), each read to its last instruction with capstone (2026-09-28)
// and taken through the boss harness (boss_harness.h). docs/boss_sa.md has
// them one row each. The enemy and fight names are the disc's, as
// tools/boss_rows.py --disc prints them - not memory of the game:
//
//   - fight 1 (BOSS001, area 24 row 4): its set-up and three hooks, and its
//     two kinds, 6 (Gary) and 7 (Mogu): each a state table on EnemyOp_Steps'
//     shape with its own entrance (state 0), its own action table (state 6,
//     the generic subs with BossOp_Death at 4) and its own state 11 - a
//     scripted end, Mogu counting 0x3C frames in 0x904B7E, Gary waiting for
//     that count, both then stepping their +0x38 and taking a new pose;
//   - kinds 1 and 2 (Nue, areas 23 and 22) and 46 (Sample 1, area 158), and
//     the three set-ups of BOSS002 (fights 2, 3 and 39, row 7 of those
//     areas: which fight is which area the tool leaves to the reader, and
//     this code never reads 0x904AAA). Kind 1 has a state 9 of its own (the
//     field actor tagged 0 walked by MoveCmd_OpE9, then the win) and a hook
//     table: at the action pick, an HP of 0xFFFF sets action kind 3; at the
//     hit, HP less the pending damage at or below a quarter of the maximum
//     sets 0x904AAD bit 0 and the HP to 0xFFFF;
//   - kind 39 (Weretigr, area 35, fight 33's): its state 4 (an effect task of
//     the kind-3 dispatcher, two cues, the turn closed) and states 9's end
//     like kind 1's, which pays the enemy's experience; its hook table's hit
//     entry is kind 1's body (0x438030, the one function two units share).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers index their tables unchecked in the original; ours abort past
// them with a Fatal naming the function (the owner's rule for an unchecked
// index, round9 doc section 6; no route reaches it). Every call goes through
// the harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in
// for the callees; every hook and table pointer the originals store is stored
// here as the same literal.
#include "game/boss_sa.h"

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
using U = std::uint32_t;
using boss_harness::AnswerHandler;
using boss_harness::Handler;
using boss_harness::Hook;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
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

void SetHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

}  // namespace

// ============================================================================
// Kind 6 (Gary, area 24; fight 1)
// ============================================================================

// ============================================================================
// Kind 7 (Mogu, area 24; fight 1)
// ============================================================================

// ============================================================================
// Fight 1 (BOSS001, area 24 row 4: kinds 6 and 7)
// ============================================================================

// ============================================================================
// Kind 1 (Nue, area 23)
// ============================================================================

// ============================================================================
// Kind 2 (Nue, area 22)
// ============================================================================

// ============================================================================
// Kind 46 (Sample 1, area 158)
// ============================================================================

// ============================================================================
// Fights 2, 3 and 39 (BOSS002, row 7 of areas 22, 23 and 158)
// ============================================================================

// ============================================================================
// Kind 39 (Weretigr, area 35; fight 33)
// ============================================================================

void BossSa_Inject() {
    if (bof3::WantsShadow("boss_sa")) boss_sa::SelfTest();
}
