// Moved here by tools/move_funcs.py from boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_angler.h"

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

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

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

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the action pick, the hit and
// BattleEnemy_RunAll pass), the entry gets the word whole, a Fatal past them.
U HookDispatchBossSh(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAtBossSh(table, index)(word);
}

// A kind's entrance, the one shape all seven kinds share (0x51 bytes each):
// 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook) and +0xF8 (its
// sound words), 0x939AD8 read for each store; then 0x939AD8 read once more and
// its dword +0x114 |= 8; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSh(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

}  // namespace

// original 0x43E7C0: BossKind_Table[43]: jmp [BossAngler_States + 4 * +1] (12:
// the entrance, the generic states, BossAngler_AdvanceDispatch at 4 and
// BossAngler_RetreatDispatch at 5).
extern "C" unsigned long __cdecl BossAngler_Dispatch(unsigned long through) {
    return DispatchBossSh("BossAngler_Dispatch", AddressOf(BossAngler_States), 12, 1, through);
}

// original 0x43E7E0: state 0 - BossAngler_Anims / BossAngler_Sounds and the
// hook BossAngler_Hook.
extern "C" unsigned char __cdecl BossAngler_Enter(void) {
    return EnterBossSh(AddressOf(BossAngler_Anims), ::bof3::Code(bof3::addr::BossAngler_Hook), AddressOf(BossAngler_Sounds));
}

// original 0x43E840: state 4 (kinds 43 and 50): jmp [BossAngler_AdvanceSteps +
// 4 * +2] (2).
extern "C" unsigned long __cdecl BossAngler_AdvanceDispatch(unsigned long through) {
    return DispatchBossSh("BossAngler_AdvanceDispatch", AddressOf(BossAngler_AdvanceSteps), 2, 2, through);
}

// original 0x43E860: state 4 step 0. Sprite_Current +9 = the byte +0x8A of the
// area's enemy data record 0x939AD8's +0xF0 names (0x8C5652 + 0x8C * index,
// unchecked); BattleEnemy_SetAnimation(2); BattleEnemy_ScriptTick (its answer
// not read); then (Sprite_Current read again for each) the dword +0x18 = +0x34
// + 0x30000, the dword +0xC = 0x4000, +2 up.
extern "C" void __cdecl BossAngler_AdvanceStart(void) {
    const unsigned record = rv::As<records::BattleEnemy>(Enemy())->data_id;
    Sprite_Current[9] = At(::bof3::Img(at::kEnemyDataCount9.v) + 0x8C * record)[0];
    BH_CALL(BattleEnemy_SetAnimation)(2);
    BH_CALL(BattleEnemy_ScriptTick)();
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    rv::Store<std::int32_t>(cur_enemy->work_18, S(static_cast<U>(cur_enemy->x) + 0x30000u));
    rv::Store<std::int32_t>(rv::Current<records::BattleEnemy>()->step_x, 0x4000);
    rv::Current<records::BattleEnemy>()->step = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->step + 1);
}

// original 0x43E8C0: state 4 step 1. +9 down; at 0, 0x437450(the first word of
// 0x939AD8's +0xF8 sound words). The dword +0x34 += the dword +0xC; when +0x34
// then equals +0x18 (Sprite_Current read again): Battle_SetTargetFlag40(the
// target), 0x4376F0, then +0x18 -= 0x30000, +1 up, +2 = 0. Then a tail jump to
// BattleEnemy_ScriptTick, whose al is the answer.
extern "C" unsigned char __cdecl BossAngler_Advance(void) {
    unsigned char* s = Sprite_Current;
    records::BattleEnemy* cur_enemy = rv::As<records::BattleEnemy>(s);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Sprite_Current;
    cur_enemy = rv::As<records::BattleEnemy>(s);
    if (s[9] == 0) {
        const unsigned sound = Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table))));
        BH_AT(void (__cdecl*)(unsigned), at::kEnemySound.v)(sound);
        s = Sprite_Current;
        cur_enemy = rv::As<records::BattleEnemy>(s);
    }
    cur_enemy->x = S(static_cast<U>(cur_enemy->x) + static_cast<U>(cur_enemy->step_x));
    s = Sprite_Current;
    cur_enemy = rv::As<records::BattleEnemy>(s);
    if (cur_enemy->x == cur_enemy->work_18) {
        BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(at::kTarget.v)));
        BH_AT(Handler, at::kEnemyActChance.v)();
        unsigned char* const t = Sprite_Current;
        rv::Store<std::int32_t>(rv::As<records::BattleEnemy>(t)->work_18, S(static_cast<U>(rv::As<records::BattleEnemy>(t)->work_18) + 0xFFFD0000u));
        rv::Current<records::BattleEnemy>()->state = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state + 1);
        rv::Current<records::BattleEnemy>()->step = 0;
    }
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x43E950: state 5 (kinds 43 and 50): jmp [BossAngler_RetreatSteps +
// 4 * +2] (2).
extern "C" unsigned long __cdecl BossAngler_RetreatDispatch(unsigned long through) {
    return DispatchBossSh("BossAngler_RetreatDispatch", AddressOf(BossAngler_RetreatSteps), 2, 2, through);
}

// original 0x43E970: state 5 step 0. The dword +0x34 -= the dword +0xC; when it
// then equals +0x18 (Sprite_Current read again, and that pointer's +2 up),
// +2 up. Then a tail jump to BattleEnemy_ScriptTick (al the answer).
extern "C" unsigned char __cdecl BossAngler_Retreat(void) {
    unsigned char* s = Sprite_Current;
    records::BattleEnemy* cur_enemy = rv::As<records::BattleEnemy>(s);
    cur_enemy->x = S(static_cast<U>(cur_enemy->x) - static_cast<U>(cur_enemy->step_x));
    s = Sprite_Current;
    cur_enemy = rv::As<records::BattleEnemy>(s);
    if (cur_enemy->x == cur_enemy->work_18) cur_enemy->step = static_cast<unsigned char>(cur_enemy->step + 1);
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x43E9A0: state 5 step 1. BattleEnemy_ScriptTick (its answer not
// read); the round flags' bit 2 set (the done flag BattleFx_ScriptUntilDone
// waits for); a tail jump to 0x4376A0 (the action's end).
extern "C" void __cdecl BossAngler_RetreatEnd(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    B(::bof3::Img(at::kFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kFlags.v)) | 4);
    BH_AT(Handler, at::kEnemyActEnd.v)();
}

// original 0x43E9C0: the +0xF4 hook through BossAngler_Hooks (BareRet, BareRet,
// BossAngler_HookSpawnFx).
extern "C" unsigned long __cdecl BossAngler_Hook(unsigned long word) {
    return HookDispatchBossSh("BossAngler_Hook", AddressOf(BossAngler_Hooks), word);
}

// original 0x43E9D0: hook entry 2 of kinds 43 and 50 (BattleEnemy_RunAll's
// call). With Sprite_Current +1 == 7 and +2 == 1: slot = BattleTask_Create(3, 3)
// (the kind-3 dispatcher's slot 3, BossAnglerFx_Dispatch); the 0x80 bytes of
// the acting actor's enemy object (0x93B960 + 0x128 * (0x904B34 - 3), a signed
// index, unchecked: an actor 0..2 reads below the enemies) copied into the
// slot a dword at a time, forward (rep movsd); then the slot's +1 = 0, +2 = 0,
// +6 = 3, +5 = 3, +9 = 0, +0x29 = 3. The word is not read. BattleTask_Create's
// 0xFF (none free) names a slot past the 48 and past the image, where the
// original copies: ours aborts.
extern "C" void __cdecl BossAngler_HookSpawnFx(unsigned) {
    if (Sprite_Current[1] != 7 || Sprite_Current[2] != 1) return;
    const unsigned slot = BH_CALL(BattleTask_Create)(3, 3) & 0xFF;
    if (slot >= at::kTaskCount)
        bof3::Fatal("BossAngler_HookSpawnFx: BattleTask_Create(3, 3) answered 0x%X (none free) - the original copies 0x80 "
                    "bytes to 0x93A000 + 0x84 * 0x%X, past the slots and the image (docs/boss_sh.md section 6)",
                    slot, slot);
    const std::int32_t actor = static_cast<std::int32_t>(B(::bof3::Img(at::kActor.v))) - 3;
    unsigned char* const to = At(::bof3::Img(BattleTask_Slots_addr) + at::kTaskStride * slot);
    const unsigned char* const from = At(static_cast<U>(static_cast<std::int32_t>(::bof3::Img(Battle_Enemies_addr)) + actor * static_cast<std::int32_t>(at::kEnemyStride)));
    for (unsigned i = 0; i < 0x80; i += 4) {
        std::uint32_t d;
        std::memcpy(&d, from + i, 4);
        std::memcpy(to + i, &d, 4);
    }
    to[1] = 0;
    to[2] = 0;
    to[6] = 3;
    to[5] = 3;
    to[9] = 0;
    to[0x29] = 3;
}

// original 0x43EA70: BossKind_Table[50]: jmp [BossSample5_States + 4 * +1]
// (12: its entrance, the Angler's states 4 and 5).
extern "C" unsigned long __cdecl BossSample5_Dispatch(unsigned long through) {
    return DispatchBossSh("BossSample5_Dispatch", AddressOf(BossSample5_States), 12, 1, through);
}

// original 0x43EA90: state 0 - the Angler's byte tables and its own hook
// BossSample5_Hook.
extern "C" unsigned char __cdecl BossSample5_Enter(void) {
    return EnterBossSh(AddressOf(BossAngler_Anims), ::bof3::Code(bof3::addr::BossSample5_Hook), AddressOf(BossAngler_Sounds));
}

// original 0x43EAF0: the +0xF4 hook through BossSample5_Hooks (BareRet,
// BareRet, BossAngler_HookSpawnFx).
extern "C" unsigned long __cdecl BossSample5_Hook(unsigned long word) {
    return HookDispatchBossSh("BossSample5_Hook", AddressOf(BossSample5_Hooks), word);
}

void BossAngler_Inject() {
    BOF3_INJECT(BossAngler_Dispatch);
    BOF3_INJECT(BossAngler_Enter);
    BOF3_INJECT(BossAngler_AdvanceDispatch);
    BOF3_INJECT(BossAngler_AdvanceStart);
    BOF3_INJECT(BossAngler_Advance);
    BOF3_INJECT(BossAngler_RetreatDispatch);
    BOF3_INJECT(BossAngler_Retreat);
    BOF3_INJECT(BossAngler_RetreatEnd);
    BOF3_INJECT(BossAngler_Hook);
    BOF3_INJECT(BossAngler_HookSpawnFx);
    BOF3_INJECT(BossSample5_Dispatch);
    BOF3_INJECT(BossSample5_Enter);
    BOF3_INJECT(BossSample5_Hook);
}
