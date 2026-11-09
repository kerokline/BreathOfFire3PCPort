// Group BSJ of the boss round: fights 52, 54 and 55, kinds 59 and 61, and the
// effect tasks of BattleBossFx_Dispatch's slots 4 and 5 - 44 functions of
// 0x43F7A0..0x441087 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// units K59, B52, F4, K61, B54, B55, F5), each read to its last instruction
// with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Round eleven, wave two; docs/boss_sj.md has them one row
// each. The group's fifth unit, FB93 (the Head Cracker rock), was ours
// already (magic rows, round nine) and nothing here calls it.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 59   D>Lord (area 172)      set-up 52  BOSS052, area 172 row 7
//   slot 4    D>Lord's effect task (BossDLord_HookFx creates it)
//   kind 61   Shroom (area 119)      set-up 54  BOSS054, area 119 row 6
//   set-up 55 BOSS055, area 198 row 7 (kind 62, Myria - group BSF's)
//   slot 5    Myria's effect task (0x440660, kind 62's, creates it)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table - a dispatcher's state byte, a hook's
// word, a stack table by +2, the ten-byte pose tables by the word 0x904B7E,
// BossMyriaFx_Drift by +1, a task slot of 0xFF - ours aborts with a Fatal
// naming the function (the owner's rule, round9 doc section 6); nothing
// reaches it. Every call goes through the harness (BH_CALL / BH_AT / Phase),
// so the start-up fuzz can stand recorders in for the callees; a hook or
// table a function stores is the same literal address.
#include "game/boss_sj.h"

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
using U = std::uint32_t;
using boss_harness::Handler;
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetLong;

using HookFn = void (__cdecl*)(unsigned);
using ForwardBossSj = U (__cdecl*)(U);

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after). The jmp
// leaves the caller's stack word in place and answers what the entry
// answers, so ours hands the word on and answers the entry's eax
// (docs/boss_sc.md section 1.2).
U DispatchBossSj(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<ForwardBossSj>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))(through);
}

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

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries, the entry gets the caller's word whole, a Fatal past
// them.
void DispatchHook(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, word, (unsigned)table);
    reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
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

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entry state (kinds 59 and 61 alike): 0x939AD8's +0xFC (its
// animation bytes), +0xF4 (its hook) and +0xF8 (its sound words), then its
// +0x114 |= 8 - 0x939AD8 read for each; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSj(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// The winning end hooks of set-ups 52 and 55: each party member whose +0 has
// bit 0 - Battle_RemoveFromTurnOrder(+5); then (read after the call) its +0x90
// and +8: Sprite_Current = the member, Sprite_PoseFromSet(+8 + 0x1C with
// +0x90 bit 14, else + 4 - a byte, 0x8C5D80, 0x1800).
void PoseParty() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(member_rec->actor);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(member_rec->direction + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(::bof3::Img(at::kPoseSet.v)), 0x1800);
    }
}

}  // namespace

// ===========================================================================
// Kind 59 (D>Lord, area 172) and set-up 52 (BOSS052, area 172 row 7)
// ===========================================================================

// ===========================================================================
// D>Lord's effect task (BattleBossFx_Dispatch slot 4)
// ===========================================================================

// ===========================================================================
// Kind 61 (Shroom, area 119) and set-up 54 (BOSS054, area 119 row 6)
// ===========================================================================

// ===========================================================================
// Set-up 55 (BOSS055, area 198 row 7; kind 62, Myria, is group BSF's)
// ===========================================================================

// ===========================================================================
// Myria's effect task (BattleBossFx_Dispatch slot 5)
// ===========================================================================

void BossSj_Inject() {
    if (bof3::WantsShadow("boss_sj")) boss_sj::SelfTest();
}
