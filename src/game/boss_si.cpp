// Group BSI of the boss round: fights 37, 38, 40, 42, 44, 45, 49, 50, 51 and
// 53, enemy kinds 45, 47, 49, 51, 52, 56, 57, 58 and 60, and the kind-3 effect
// dispatcher's slot 7 - 50 functions of 0x43ECC0..0x43F79B, the ones
// tools/boss_rows.py's group column gave BSI (analysis/boss_funcs.tsv,
// 2026-09-28; the 51st, MagicFx_DoneAndFree 0x43FE80, was already ours and is
// reached here through a stack table), each read to its last instruction
// with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Round eleven, wave two; docs/boss_si.md has them one row
// each.
//
// Names come from the US disc's area records as tools/boss_rows.py --disc
// prints them, never from memory of the game:
//
//   set-up 37  BOSS037, area 144 row 7 (kind 44, Elder - group BSH's)
//   kind 45    Ammonite (area 134)       set-up 38  BOSS038, area 134 row 7
//   kind 51    Sample 6 (area 162)       set-up 44  BOSS038, row 6 (area 162)
//   kind 47    Sample 2 (area 159)       set-ups 40 and 53  BOSS040, row 7
//   kind 60    HugeSlug (area 119)         (areas 119 and 159)
//   kind 49    Sample 4 (area 161)       set-up 42  BOSS042, row 7
//   kind 52    Sample 7 (area 163)       set-ups 45 and 49  BOSS049, row 7
//   kind 56    Manmo (area 196)            (areas 163 and 196)
//   kind 57    Chimera (area 197)        set-up 50  BOSS050, row 7
//   kind 58    Arwan (area 142)          set-up 51  BOSS051, row 7
//   F7         the kind-3 effect dispatcher's slot 7: the task kind 58's
//              state 4 creates (BattleTask_Create(3, 7), the only creator)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6), and BossArwan_State4Fx aborts where the original would store
// into a task slot past the 48; nothing reaches either. A kind's dispatcher
// hands its entry the word the original's jmp leaves on the stack, and answers
// what the entry answers. Every call goes through the harness (BH_CALL /
// BH_AT / Phase), so the start-up fuzz can stand recorders in for the
// callees; a hook or table a function stores is the same literal address.
#include "game/boss_si.h"

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
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using EntryBossSi = unsigned long (__cdecl*)(unsigned);
unsigned long CallEntryBossSi(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<EntryBossSi>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long DispatchBossSi(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return CallEntryBossSi(table, state, word);
}

// mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 * eax]: an enemy's +0xF4
// hook by its word's low byte (3 entries in every table of the group), the
// word itself left for the entry.
unsigned long HookDispatchBossSi(const char* who, U table, unsigned word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook word %u is past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, index, (unsigned)table);
    return CallEntryBossSi(table, index, word);
}

// A kind's state 0, the part every kind of the group shares: 0x939AD8's (read
// again for each store) +0xFC = its animation bytes, +0xF4 = its hook, +0xF8 =
// its cue words, then its +0x114 |= 8 (the dword), and Sprite_Current +1 = 2.
void EnterStores(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8u);
    Sprite_Current[1] = 2;
}
// ... and a tail jump to Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSi(U fc, U hook, U f8) {
    EnterStores(fc, hook, f8);
    return BH_CALL(Sprite_ScriptTick)();
}

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

// ===========================================================================
// Set-up 37 (BOSS037, area 144 row 7; its kind 44, Elder, is group BSH's)
// ===========================================================================

// ===========================================================================
// Kind 45 (Ammonite, area 134) and kind 51 (Sample 6, area 162); set-ups 38
// (BOSS038, area 134 row 7) and 44 (BOSS038 row 6, area 162)
// ===========================================================================

// ===========================================================================
// Kind 47 (Sample 2, area 159) and kind 60 (HugeSlug, area 119); set-ups 40
// and 53 (BOSS040, row 7)
// ===========================================================================

// ===========================================================================
// Kind 49 (Sample 4, area 161); set-up 42 (BOSS042, row 7)
// ===========================================================================

// ===========================================================================
// Kind 52 (Sample 7, area 163) and kind 56 (Manmo, area 196); set-ups 45 and
// 49 (BOSS049, row 7)
// ===========================================================================

// ===========================================================================
// Kind 57 (Chimera, area 197); set-up 50 (BOSS050, row 7)
// ===========================================================================

// ===========================================================================
// Kind 58 (Arwan, area 142), its effect task (the kind-3 dispatcher's slot 7)
// and set-up 51 (BOSS051, row 7)
// ===========================================================================

// ===========================================================================

void BossSi_Inject() {
    if (bof3::WantsShadow("boss_si")) boss_si::SelfTest();
}
