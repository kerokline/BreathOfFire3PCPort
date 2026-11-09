// Moved here by tools/move_funcs.py from boss_sc.cpp (docs/source-layout-round.md).
#include "game/boss/boss_balio.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sc.h"
#include "game/boss_sc_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sc::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// A kind's entrance: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8, in that order, 0x939AD8 read before each (no call between).
void StoreKindTables(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(f8);
}

using EntryBossSc = U (__cdecl*)(U);

EntryBossSc EntryAt(U table, unsigned index) { return reinterpret_cast<EntryBossSc>(static_cast<std::uintptr_t>(L(At(table + 4 * index)))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through whatever follows). The jmp
// leaves the stack as the dispatcher found it, so the entry sees the word
// above the return address that the dispatcher saw: ours hands it on (an
// entry that reads it - Port_DroppedCall, a bare ret with a byte - sees what
// it sees under the original), and answers the entry's eax.
U DispatchBossSc(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, at, state, entries, (unsigned)table);
    return EntryAt(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook table: jmp [table + 4 * (word & 0xFF)], 3 entries.
U HookDispatchBossSc(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAt(table, index)(word);
}

// Fights 13 and 16's line: the enemy's action byte, the window pass byte 2,
// then Msg_OpenScript(line).
void Line(unsigned char action, unsigned short line) {
    B(::bof3::Img(at::kAction.v)) = action;
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(line);
}

// Kinds 13 and 14's state 0: the tables, +1 = 2; in fight 13 (0x904AAA 0xD)
// 0x939AD8's words +0xD0, +0xB0 and +0xA4 (HP) = 0xFFFF; then
// Sprite_SetAnimationBank(bank), Sprite_SetAnimation(0), and +0x2A = 1 on
// Sprite_Current read after the calls.
void BalioSunderEnter(U fc, U f4, U f8, unsigned short bank) {
    StoreKindTables(fc, f4, f8);
    Sprite_Current[1] = 2;
    if (EventBattle_Id == 0xD) {
        rv::Store<unsigned>(rv::As<records::BattleEnemy>(Enemy())->base_max_hp, 0xFFFF);
        rv::As<records::BattleEnemy>(Enemy())->max_hp = 0xFFFF;
        rv::As<records::BattleEnemy>(Enemy())->hp = 0xFFFF;
    }
    BH_CALL(Sprite_SetAnimationBank)(bank);
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[0x2A] = 1;
}

// Kinds 13 and 14's hit hooks' floor (Balio's and Sunder's, fights 13 and 16):
// 0x939AD8's HP +0xA4 at 0 made 1.
void HpFloor() {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->hp == 0) enemy_rec->hp = 1;
}

}  // namespace

// original 0x439B10: BossKind_Table[13]: by +1 through BossBalio_Steps (12).
extern "C" unsigned long __cdecl BossBalio_Dispatch(unsigned long through) {
    return DispatchBossSc("BossBalio_Dispatch", ::bof3::Img(0x64CCD0), 12, 1, through);
}

// original 0x439B30: state 0 (BalioSunderEnter above): BossBalio_Anims,
// BossBalio_Hook, BossBalio_F8, bank 0xEE.
extern "C" void __cdecl BossBalio_Enter(void) { BalioSunderEnter(::bof3::Img(0x64CC88), ::bof3::Code(bof3::addr::BossBalio_Hook), ::bof3::Img(0x64CCB8), 0xEE); }

// original 0x439BC0: the +0xF4 hook: by the word's low byte through
// BossBalio_Hooks (3: BossBalio_HookAct, BossBalio_HookHit, BareRet).
extern "C" unsigned long __cdecl BossBalio_Hook(unsigned long word) { return HookDispatchBossSc("BossBalio_Hook", ::bof3::Img(0x64CD00), word); }

// original 0x439BD0: hook entry 0 (the action pick, 0x435BF5): in fight 16
// (0x904AAA 0x10), by the fight's bits 0x904AAD (read once): bit 0 without
// bit 1 - action 0, line 0x26, bit 1 set (the byte read again after the
// call); then (on that byte) bit 4 without bit 5 - action 3, line 0x29, bit 5
// set. The word is not read.
extern "C" void __cdecl BossBalio_HookAct(unsigned) {
    if (EventBattle_Id != 0x10) return;
    unsigned char bits = BossSe_Script;
    if ((bits & 1) && !(bits & 2)) {
        Line(0, 0x26);
        bits = static_cast<unsigned char>(BossSe_Script | 2);
        BossSe_Script = bits;
    }
    if ((bits & 0x10) && !(bits & 0x20)) {
        Line(3, 0x29);
        BossSe_Script = static_cast<unsigned char>(BossSe_Script | 0x20);
    }
}

// original 0x439C40: hook entry 1 (the hit, 0x4367EE): HP 0 made 1.
extern "C" void __cdecl BossBalio_HookHit(unsigned) { HpFloor(); }

void BossBalio_Inject() {
    BOF3_INJECT(BossBalio_Dispatch);
    BOF3_INJECT(BossBalio_Enter);
    BOF3_INJECT(BossBalio_Hook);
    BOF3_INJECT(BossBalio_HookAct);
    BOF3_INJECT(BossBalio_HookHit);
}
