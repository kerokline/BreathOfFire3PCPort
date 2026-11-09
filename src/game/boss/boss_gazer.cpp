// Moved here by tools/move_funcs.py from boss_sf.cpp (docs/source-layout-round.md).
#include "game/boss/boss_gazer.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sf.h"
#include "game/boss_sf_callees.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sf::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

U Key(const void* p) { return Low32(p); }

void SetCell(unsigned char* p, U value) { SetLong(p, static_cast<std::int32_t>(value)); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's code - reads that word), its eax passed back.
using ForwardBossSf = U (__cdecl*)(unsigned);

ForwardBossSf EntryBossSf(U table, unsigned index) { return reinterpret_cast<ForwardBossSf>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index))))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through the dword after).
U DispatchBossSf(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryBossSf(table, state)(passed);
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)], three entries, a
// Fatal past them; the entry gets the hook's word.
U HookDispatchBossSf(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s: hook word %u, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, i, (unsigned)table);
    return EntryBossSf(table, i)(word);
}

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/boss_sf.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    if (n >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/boss_sf.md section 6)",
                    who, n, n);
    return At(::bof3::Img(BattleTask_Slots_addr) + n * at::kTaskStride);
}

// The area's enemy data record the current enemy's +0xF0 names, byte `field`
// (+0x8A the state 4 count, +0x8B state 7's).
unsigned char EnemyDataByte(unsigned field) {
    const unsigned index = rv::As<records::BattleEnemy>(Enemy())->data_id;
    return At(::bof3::Img(at::kEnemyData.v) + index * at::kEnemyDataStride)[field];
}

// An enemy state's sound: 0x437450 with the first word of the current
// enemy's +0xF8 table.
void EnemySound() { BH_AT(void (__cdecl*)(unsigned), at::kEnemySound.v)(Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table))))); }

}  // namespace

// original 0x43C740: jmp [Boss26Fx_States + 4 * Sprite_Current +1] (2).
extern "C" unsigned __cdecl Boss26Fx_Dispatch(unsigned passed) {
    return DispatchBossSf("Boss26Fx_Dispatch", AddressOf(Boss26Fx_States), 2, 1, passed);
}

// original 0x43C760: state 0 - with the window byte 0x803433 clear and
// 0x904AE9 bit 1 clear, +1 up by one.
extern "C" void __cdecl Boss26Fx_Wait(void) {
    if (WindowRecords[20].state == 0 && (BattleMisc_BannerKinds & 2) == 0) rv::Current<records::BattleTask>()->state += 1;
}

// original 0x43C780: state 1 - BattleWin_DrawMediumBox(0x6B, 0x10); the text
// at [0x669D20] at (0x7A, 0x12); Crt_sprintf(Text_Records[1],
// Boss26Fx_CountFormat, 0x15 - the turn counter 0x904B90) and
// Text_DrawFont12(0x92, 0x12, 4, it); the text at [0x669D24] at (0xAA, 0x12).
// Then with 0x803433 set or 0x904AE9 bit 1, +1 down by one (back to state 0);
// at phase 5 (0x904AA0) a tail jump to BattleTask_FreeCurrent.
//
// DIVERGENCE DIV-0119 (DIV-0064's group 18): under a language overlay that
// has written the turn counter, the PlayStation disc's own layout instead -
// the count at the disc's x, then one word at its x, drawn whole: the
// plural, or the singular when the disc's test on the turn 0x904B90 says so
// (the European discs' `turn  left`, the French `Rest   tour`). The layout
// slot holds the count's x, the word's x, the test ('=' the turn equal to
// the value, '>' at least it, 'n' none) and the value. US BOSS025.EMI
// 0x800C3088: the box (0x6B, 0x10), "%2d" of 0x15 - turn at (0x70, 0x12),
// `turns left` at (0x80, 0x12); French: the words at 0x70, the count at
// 0x94 between them. The disc draws the count through 0x8014FF0C, which
// takes the dialogue font's 8 x 12 cells (code - 0x30, 31 a row from v 0x48
// of page 0x2F) in CLUT 0x7800 | 4 and moves the pen 8 - so ours draws it
// with Text_DrawAt in colour 4, the overlay's copy of those cells, and not
// the 12 px numerals, whose two digits ran into the words (issue 133).
extern "C" void __cdecl Boss26Fx_DrawCount(void) {
    BH_CALL(BattleWin_DrawMediumBox)(0x6B, 0x10);
    if (Labels_Written(18)) {
        const unsigned char* const layout = Labels_Slot(18, 2);
        const U turn = static_cast<U>(Long(At(::bof3::Img(at::kTurn.v))));
        const auto left = static_cast<std::int32_t>(0x15u - turn);   // x86's wrap
        BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextRow1.v))), reinterpret_cast<const char*>(At(::bof3::Img(Boss26Fx_CountFormat_addr))), left);
        BH_CALL(Text_DrawAt)(layout[0], 0x12, 4, 0xFF, At(::bof3::Img(at::kTextRow1.v)));
        const bool singular = (layout[2] == '=' && turn == layout[3]) || (layout[2] == '>' && turn >= layout[3]);
        BH_CALL(Text_DrawAt)(layout[1], 0x12, 0, 0xFF, Labels_Slot(18, singular ? 1 : 0));
    } else {
        BH_CALL(Text_DrawAt)(0x7A, 0x12, 0, 4, At(BossSf_CountText0));
        const auto left = static_cast<std::int32_t>(0x15u - static_cast<U>(Long(At(::bof3::Img(at::kTurn.v)))));   // x86's wrap
        BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextRow1.v))), reinterpret_cast<const char*>(At(::bof3::Img(Boss26Fx_CountFormat_addr))), left);
        BH_CALL(Text_DrawFont12)(0x92, 0x12, 4, At(::bof3::Img(at::kTextRow1.v)));
        BH_CALL(Text_DrawAt)(0xAA, 0x12, 0, 4, At(BossSf_CountText1));
    }
    if (WindowRecords[20].state != 0 || (BattleMisc_BannerKinds & 2) != 0) rv::Current<records::BattleTask>()->state -= 1;
    if (B(::bof3::Img(at::kPhase.v)) == 5) BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x43C810: BossKind_Table[33]. jmp [BossGazer_States + 4 *
// Sprite_Current +1] (12: its entry, its states 4 and 5, the generic enemy
// states).
extern "C" unsigned __cdecl BossGazer_Dispatch(unsigned passed) {
    return DispatchBossSf("BossGazer_Dispatch", AddressOf(BossGazer_States), 12, 1, passed);
}

// original 0x43C830: state 0. 0x939AD8's +0xFC = BossGazer_Anims, +0xF4 =
// BossGazer_Hook, +0xF8 = BossGazer_Sounds (0x939AD8 read for each);
// Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick, whose al is the
// answer.
extern "C" unsigned char __cdecl BossGazer_Enter(void) {
    SetCell(Enemy() + 0xFC, AddressOf(BossGazer_Anims));
    SetCell(Enemy() + 0xF4, ::bof3::Code(bof3::addr::BossGazer_Hook));
    SetCell(Enemy() + 0xF8, AddressOf(BossGazer_Sounds));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43C870: state 4 (the generic table's turn start). jmp
// [BossGazer_State4Steps + 4 * Sprite_Current +2] (3).
extern "C" unsigned __cdecl BossGazer_State4Dispatch(unsigned passed) {
    return DispatchBossSf("BossGazer_State4Dispatch", AddressOf(BossGazer_State4Steps), 3, 2, passed);
}

// original 0x43C890: state 4 step 0 - +9 = the enemy data record's +0x8A (the
// record the current enemy's +0xF0 names); BattleEnemy_SetAnimation(2);
// +2 up by one.
extern "C" void __cdecl BossGazer_State4Start(void) {
    Sprite_Current[9] = EnemyDataByte(0x8A);
    BH_CALL(BattleEnemy_SetAnimation)(2);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x43C8D0: state 4 step 1 - +9 down by one; at 0 the enemy's sound
// (0x437450 with its +0xF8 table's first word). Then BattleEnemy_ScriptTick;
// when it answers, BattleTask_Create(3, 2) - the Gazer's effect task (F2) -
// with its +1 = 0 and its owner +0x80 = Sprite_Current (read after the call),
// and +2 up by one.
extern "C" void __cdecl BossGazer_State4Wait(void) {
    Sprite_Current[9] -= 1;
    if (Sprite_Current[9] == 0) EnemySound();
    if (BH_CALL(BattleEnemy_ScriptTick)() == 0) return;
    unsigned char* const task = TaskSlot("BossGazer_State4Wait", BH_CALL(BattleTask_Create)(3, 2) & 0xFF);
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    task[1] = 0;
    SetCell(task + 0x80, Key(s));
    cur_enemy->step += 1;
}

// original 0x43C950: state 5. jmp [BossGazer_State5Steps + 4 * Sprite_Current
// +2] (2).
extern "C" unsigned __cdecl BossGazer_State5Dispatch(unsigned passed) {
    return DispatchBossSf("BossGazer_State5Dispatch", AddressOf(BossGazer_State5Steps), 2, 2, passed);
}

// original 0x43C970: state 5 step 0 - Sprite_SetAnimation(2); the target
// 0x904B44 = 0x80 and Battle_SetTargetFlag40(0x80); +2 up by one.
extern "C" void __cdecl BossGazer_State5Start(void) {
    BH_CALL(Sprite_SetAnimation)(2);
    B(::bof3::Img(at::kTarget.v)) = 0x80;
    BH_CALL(Battle_SetTargetFlag40)(0x80);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x43C9A0: state 5 step 1 (and Myria's, BossMyria_State5Steps 1) -
// when BattleEnemy_ScriptTick answers: 0x904AA8 |= 4, 0x4376F0, and a tail
// jump to 0x4376A0 (the action's end: +1 = 2, +2 = 0).
extern "C" void __cdecl BossGazer_State5Close(void) {
    if (BH_CALL(BattleEnemy_ScriptTick)() == 0) return;
    B(::bof3::Img(at::kRoundFlags.v)) |= 4;
    BH_AT(Handler, at::kEnemyActChance.v)();
    BH_AT(Handler, at::kEnemyActEnd.v)();
}

// original 0x43C9C0: the +0xF4 hook: by the word's low byte through
// BossGazer_Hooks (3, all BareRet).
extern "C" unsigned __cdecl BossGazer_Hook(unsigned word) { return HookDispatchBossSf("BossGazer_Hook", AddressOf(BossGazer_Hooks), word); }

void BossGazer_Inject() {
    BOF3_INJECT(Boss26Fx_Dispatch);
    BOF3_INJECT(Boss26Fx_Wait);
    BOF3_INJECT(Boss26Fx_DrawCount);
    BOF3_INJECT(BossGazer_Dispatch);
    BOF3_INJECT(BossGazer_Enter);
    BOF3_INJECT(BossGazer_State4Dispatch);
    BOF3_INJECT(BossGazer_State4Start);
    BOF3_INJECT(BossGazer_State4Wait);
    BOF3_INJECT(BossGazer_State5Dispatch);
    BOF3_INJECT(BossGazer_State5Start);
    BOF3_INJECT(BossGazer_State5Close);
    BOF3_INJECT(BossGazer_Hook);
}
