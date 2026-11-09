// Moved here by tools/move_funcs.py from boss_sf.cpp (docs/source-layout-round.md).
#include "game/boss/boss_myria.h"

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

// call [table + 4 * Sprite_Current[at]]: the same, as a call (the caller goes
// on after it); the entries read no word.
void Step(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
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

// The byte of ability record `id` (24 bytes each) at `at`.
unsigned char AbilityByte(U base, unsigned id) { return At(base + id * at::kAbilityStride)[0]; }

// An enemy state's sound: 0x437450 with the first word of the current
// enemy's +0xF8 table.
void EnemySound() { BH_AT(void (__cdecl*)(unsigned), at::kEnemySound.v)(Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table))))); }

}  // namespace

// original 0x440080: BossKind_Table[62]. jmp [BossMyria_States + 4 *
// Sprite_Current +1] (12).
extern "C" unsigned __cdecl BossMyria_Dispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_Dispatch", AddressOf(BossMyria_States), 12, 1, passed);
}

// original 0x4400A0: state 0. The word 0x904B7E = 0; 0x939AD8's +0xFC =
// BossMyria_Anims, +0xF4 = BossMyria_Hook, +0xF8 = BossMyria_Sounds, +0x114 |=
// 8, +0x29 = 6 (0x939AD8 read for each); BattleEnemy_SetAnimation(0);
// BossMyria_SpawnFx(0, 6), (1, 6), (2, 6); 0x455290(Sprite_Current, 0x64DD04);
// Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick, whose al is the
// answer.
extern "C" unsigned char __cdecl BossMyria_Enter(void) {
    unsigned char* const e = Enemy();
    BossSa_EndCount = 0;
    SetCell(e + 0xFC, AddressOf(BossMyria_Anims));
    SetCell(Enemy() + 0xF4, ::bof3::Code(bof3::addr::BossMyria_Hook));
    SetCell(Enemy() + 0xF8, AddressOf(BossMyria_Sounds));
    unsigned char* const f = Enemy();
    auto* const enemy_f = rv::As<records::BattleEnemy>(f);
    SetCell(f + 0x114, static_cast<U>(static_cast<std::int32_t>(enemy_f->flags_114)) | 8);
    rv::As<records::BattleEnemy>(Enemy())->ot_slot = 6;
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(BossMyria_SpawnFx)(0, 6);
    BH_CALL(BossMyria_SpawnFx)(1, 6);
    BH_CALL(BossMyria_SpawnFx)(2, 6);
    BH_AT(void (__cdecl*)(unsigned char*, U), at::kSlotStart.v)(Sprite_Current, ::bof3::Img(at::kMyriaSlotScript.v));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x440140: state 2 (the generic table's idle). jmp
// [BossMyria_IdleSteps + 4 * Sprite_Current +2] (2).
extern "C" unsigned __cdecl BossMyria_IdleDispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_IdleDispatch", AddressOf(BossMyria_IdleSteps), 2, 2, passed);
}

// original 0x440160: idle step 0 - +0xB = 1, the word 0x904B7E = 0,
// BattleEnemy_SetAnimation(0), BattleEnemy_ScriptTick (its al not read), +2
// up by one.
extern "C" void __cdecl BossMyria_IdleStart(void) {
    Sprite_Current[0xB] = 1;
    BossSa_EndCount = 0;
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(BattleEnemy_ScriptTick)();
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x440190: idle step 1 - +0xB = 0, BattleEnemy_ScriptTick (its al
// not read), +1 up by one (state 3, the generic wait), +2 = 0.
extern "C" void __cdecl BossMyria_IdleEnd(void) {
    Sprite_Current[0xB] = 0;
    BH_CALL(BattleEnemy_ScriptTick)();
    rv::Current<records::BattleEnemy>()->state += 1;
    rv::Current<records::BattleEnemy>()->step = 0;
}

// original 0x4401C0: state 4. jmp [BossMyria_State4Steps + 4 * +2] (2).
extern "C" unsigned __cdecl BossMyria_State4Dispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_State4Dispatch", AddressOf(BossMyria_State4Steps), 2, 2, passed);
}

// original 0x4401E0: state 4 step 0 - BossMyria_SpawnFxAndWait(4, 2, 2); +9 =
// the enemy data record's +0x8A; +2 up by one.
extern "C" void __cdecl BossMyria_State4Start(void) {
    BH_CALL(BossMyria_SpawnFxAndWait)(4, 2, 2);
    Sprite_Current[9] = EnemyDataByte(0x8A);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x440220: state 4 step 1 - +9 down by one; at 0 the enemy's sound;
// +0xB = 0. The step stays (what moves Myria on from here is not in it).
extern "C" void __cdecl BossMyria_State4Wait(void) {
    Sprite_Current[9] -= 1;
    if (Sprite_Current[9] == 0) EnemySound();
    Sprite_Current[0xB] = 0;
}

// original 0x440260: state 5. jmp [BossMyria_State5Steps + 4 * +2] (2).
extern "C" unsigned __cdecl BossMyria_State5Dispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_State5Dispatch", AddressOf(BossMyria_State5Steps), 2, 2, passed);
}

// original 0x440280: state 5 step 0 - Battle_SetTargetFlag40(the target
// 0x904B44); +2 up by one. (Step 1 is BossGazer_State5Close.)
extern "C" void __cdecl BossMyria_State5Start(void) {
    BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(at::kTarget.v)));
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x4402A0: state 6 (an action taken or received). jmp
// [BossMyria_ActSubs + 4 * +2] (6).
extern "C" unsigned __cdecl BossMyria_ActDispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_ActDispatch", AddressOf(BossMyria_ActSubs), 6, 2, passed);
}

// original 0x4402C0: action step 0 - unless the acting kind 0x904B35 is 4 (an
// ability) whose record byte (0x65C4D8 + 24 * 0x904B80) has bit 2 clear,
// BossMyria_SpawnFxAndWait(5, 4, 4); then +2 = 1.
extern "C" void __cdecl BossMyria_ActPick(void) {
    if (B(::bof3::Img(at::kActKind.v)) != 4 || (AbilityByte(::bof3::Img(at::kAbilityFlags4.v), Word(At(::bof3::Img(at::kAbility.v)))) & 4) != 0) {
        BH_CALL(BossMyria_SpawnFxAndWait)(5, 4, 4);
    }
    rv::Current<records::BattleEnemy>()->step = 1;
}

// original 0x440310: action step 4 (the death) - Sprite_ScriptTickOnce,
// Battle_EnemyDefeated, 0x454A80(Sprite_Current); 0x939AD8's +0x110 |=
// 0x1000; Sprite_Current (read for each) +0 &= 0xBF, +1 = 2, +2 = 0, +3 = 0.
extern "C" void __cdecl BossMyria_Death(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    BH_AT(void (__cdecl*)(unsigned char*), at::kSlotsReleaseFor.v)(Sprite_Current);
    unsigned char* const e = Enemy();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    SetCell(e + 0x110, static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_110)) | 0x1000);
    rv::Current<records::BattleEnemy>()->flags &= 0xBF;
    rv::Current<records::BattleEnemy>()->state = 2;
    rv::Current<records::BattleEnemy>()->step = 0;
    rv::Current<records::BattleEnemy>()->state3 = 0;
}

// original 0x440370: state 7. jmp [BossMyria_State7Steps + 4 * +2] (3).
extern "C" unsigned __cdecl BossMyria_State7Dispatch(unsigned passed) {
    return DispatchBossSf("BossMyria_State7Dispatch", AddressOf(BossMyria_State7Steps), 3, 2, passed);
}

// original 0x440390: state 7 step 0 - nothing (eax 0) until File_LoadDone
// answers (its whole eax tested). Then by 0x939AD8's +0x105: 4 and an event
// battle (0x904AAA) - Battle_LoadSoundByKey(its kind +0x100 + 0x20, the set
// 0x904B8D): answered, the word 0x904B7E = 0, else +9 = 0x1E; 4 outside one,
// nothing; not 4, +9 = 0. Then by the ability 0x904B80: its record byte
// (0x65C4DD + 24 * id) bit 3 - +1 up by one; else +9 = the enemy data record's
// +0x8B and BossMyria_SpawnFxAndWait with BattleEnemy_SetAnimation by the id:
// 0x3A (5, 4, 7) and 0xA, 0x81 (6, 4, 6) and 0xA, 0x82 (3, 4, 5) and 6, any
// other (3, 4, 3) and 0xA; +2 up by one. Both end in a tail jump to
// BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl BossMyria_State7Start(void) {
    if (BH_CALL(File_LoadDone)() == 0) return 0;
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->command_kind == 4) {
        if (EventBattle_Id != 0) {
            const unsigned key = static_cast<unsigned char>(enemy_rec->run_state + 0x20);
            if (BH_CALL(Battle_LoadSoundByKey)(key, BattleE3_SoundSet) != 0)
                BossSa_EndCount = 0;
            else
                Sprite_Current[9] = 0x1E;
        }
    } else {
        Sprite_Current[9] = 0;
    }
    const unsigned id = Word(At(::bof3::Img(at::kAbility.v)));
    if (AbilityByte(::bof3::Img(at::kAbilityFlags8.v), id) & 8) {
        rv::Current<records::BattleEnemy>()->state += 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    Sprite_Current[9] = EnemyDataByte(0x8B);
    switch (id) {
    case 0x3A:
        BH_CALL(BossMyria_SpawnFxAndWait)(5, 4, 7);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    case 0x81:
        BH_CALL(BossMyria_SpawnFxAndWait)(6, 4, 6);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    case 0x82:
        BH_CALL(BossMyria_SpawnFxAndWait)(3, 4, 5);
        BH_CALL(BattleEnemy_SetAnimation)(6);
        break;
    default:
        BH_CALL(BossMyria_SpawnFxAndWait)(3, 4, 3);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    }
    rv::Current<records::BattleEnemy>()->step += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x4404A0: state 7 step 1 - +0xB = 0; +9 (Sprite_Current read
// again), when not 0, down by one; at 0: with the word 0x904B7E at 3 or 5,
// Sound_PlayEffect(0x602) (Sprite_Current read after it), and +2 up by one.
// Both end in a tail jump to BattleEnemy_ScriptTickOnce, whose al is the
// answer.
extern "C" unsigned char __cdecl BossMyria_State7Count(void) {
    Sprite_Current[0xB] = 0;
    unsigned char* s = Sprite_Current;
    if (s[9] != 0) {
        s[9] -= 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    const unsigned wait = BossSa_EndCount;
    if (wait == 3 || wait == 5) {
        BH_CALL(Sound_PlayEffect)(0x602);
        s = Sprite_Current;
    }
    s[2] += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x4404F0 (PSX 0x801E4C84): state 7 step 2 - when BattleEnemy_ScriptTickOnce answers,
// +1 up by one (state 8) and +2 = 0.
extern "C" void __cdecl BossMyria_State7End(void) {
    if (BH_CALL(BattleEnemy_ScriptTickOnce)() == 0) return;
    rv::Current<records::BattleEnemy>()->state += 1;
    rv::Current<records::BattleEnemy>()->step = 0;
}

// original 0x440510: state 8 - call [BossMyria_State8Steps + 4 * +2] (5);
// then with Sprite_Current (read again) +1 still 8 and +2 at most 2, a tail
// jump to BossMyria_State8Check.
extern "C" void __cdecl BossMyria_State8Dispatch(void) {
    Step("BossMyria_State8Dispatch", AddressOf(BossMyria_State8Steps), 5, 2);
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    if (cur_enemy->state == 8 && cur_enemy->step <= 2) BH_CALL(BossMyria_State8Check)();
}

// original 0x440540 (PSX 0x801E4D28): state 8 step 0 - BattleEnemy_ScriptTickOnce (its al not
// read), +2 up by one.
extern "C" void __cdecl BossMyria_State8Tick(void) {
    BH_CALL(BattleEnemy_ScriptTickOnce)();
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x440550: state 8 step 1 - BattleEnemy_ScriptTickOnce; with
// 0x939AD8's +0x105 at 4, its word +0xA6 down by the byte 0x904B88 (the cost
// shown) and 0x939AD8 read again; 0x904AA9 |= 8; that object's +0x105 = 0; +2
// up by one.
extern "C" void __cdecl BossMyria_State8Cost(void) {
    BH_CALL(BattleEnemy_ScriptTickOnce)();
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->command_kind == 4) {
        enemy_rec->ap = enemy_rec->ap - BattleE3_Cost;
        enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    }
    B(::bof3::Img(at::kRoundFlagsHi.v)) |= 8;
    enemy_rec->command_kind = 0;
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x440590: state 8 step 2 - unless the ability's record byte
// (0x65C4DD + 24 * 0x904B80) has bit 3, a tail jump to
// BattleEnemy_ScriptTickOnce. The original's eax is that answer, or 3 * the id
// (its index arithmetic); al is both.
extern "C" unsigned char __cdecl BossMyria_State8TickUnless(void) {
    const unsigned id = Word(At(::bof3::Img(at::kAbility.v)));
    if ((AbilityByte(::bof3::Img(at::kAbilityFlags8.v), id) & 8) == 0) return BH_CALL(BattleEnemy_ScriptTickOnce)();
    return static_cast<unsigned char>(3 * id);
}

// original 0x4405B0: state 8 step 3 - +0xB = 0.
extern "C" void __cdecl BossMyria_State8Clear(void) { Sprite_Current[0xB] = 0; }

// original 0x4405C0: state 8 step 4 - 0x4376A0 (the action's end), then +1 =
// 2 and +2 = 0 (Sprite_Current read for each).
extern "C" void __cdecl BossMyria_State8Close(void) {
    BH_AT(Handler, at::kEnemyActEnd.v)();
    rv::Current<records::BattleEnemy>()->state = 2;
    rv::Current<records::BattleEnemy>()->step = 0;
}

// original 0x4405E0: state 8's check - nothing without 0x904AA8 bit 2; with
// it, the ability 0x904B80 at 0x81: +0xB = 1, the word 0x904B7E = 8, +2 = 3;
// any other: a tail jump to 0x4376A0 (the action's end).
extern "C" void __cdecl BossMyria_State8Check(void) {
    if ((B(::bof3::Img(at::kRoundFlags.v)) & 4) == 0) return;
    if (Word(At(::bof3::Img(at::kAbility.v))) != 0x81) {
        BH_AT(Handler, at::kEnemyActEnd.v)();
        return;
    }
    Sprite_Current[0xB] = 1;
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    BossSa_EndCount = 8;
    cur_enemy->step = 3;
}

// original 0x440620: the +0xF4 hook: by the word's low byte through
// BossMyria_Hooks (3, all BareRet).
extern "C" unsigned __cdecl BossMyria_Hook(unsigned word) { return HookDispatchBossSf("BossMyria_Hook", AddressOf(BossMyria_Hooks), word); }

// original 0x440630 (state, part, wait): BossMyria_SpawnFx(state, part); then
// Sprite_Current +0xB = 1 and the word 0x904B7E = the wait's low byte.
extern "C" void __cdecl BossMyria_SpawnFxAndWait(unsigned state, unsigned part, unsigned wait) {
    BH_CALL(BossMyria_SpawnFx)(state, part);
    Sprite_Current[0xB] = 1;
    BossSa_EndCount = wait & 0xFF;
}

// original 0x440660 (state, part): BattleTask_Create(3, 5) - an effect task of
// BattleBossFx_Dispatch's slot 5 (F5, group BSJ's) - its first 0x80 bytes
// copied from enemy 0's object, then its +1 = the state's low byte, +2 = 0,
// +6 = 3, +5 = 5, +9 = 0, the owner +0x80 = enemy 0, +0x29 = the part's low
// byte.
extern "C" void __cdecl BossMyria_SpawnFx(unsigned state, unsigned part) {
    unsigned char* const task = TaskSlot("BossMyria_SpawnFx", BH_CALL(BattleTask_Create)(3, 5) & 0xFF);
    for (U i = 0; i < 0x80; i += 4) SetLong(task + i, Long(At(::bof3::Img(Battle_Enemies_addr) + i)));
    task[1] = static_cast<unsigned char>(state);
    task[2] = 0;
    task[6] = 3;
    task[5] = 5;
    task[9] = 0;
    SetCell(task + 0x80, ::bof3::Img(Battle_Enemies_addr));
    task[0x29] = static_cast<unsigned char>(part);
}

void BossMyria_Inject() {
    BOF3_INJECT(BossMyria_Dispatch);
    BOF3_INJECT(BossMyria_Enter);
    BOF3_INJECT(BossMyria_IdleDispatch);
    BOF3_INJECT(BossMyria_IdleStart);
    BOF3_INJECT(BossMyria_IdleEnd);
    BOF3_INJECT(BossMyria_State4Dispatch);
    BOF3_INJECT(BossMyria_State4Start);
    BOF3_INJECT(BossMyria_State4Wait);
    BOF3_INJECT(BossMyria_State5Dispatch);
    BOF3_INJECT(BossMyria_State5Start);
    BOF3_INJECT(BossMyria_ActDispatch);
    BOF3_INJECT(BossMyria_ActPick);
    BOF3_INJECT(BossMyria_Death);
    BOF3_INJECT(BossMyria_State7Dispatch);
    BOF3_INJECT(BossMyria_State7Start);
    BOF3_INJECT(BossMyria_State7Count);
    BOF3_INJECT(BossMyria_State7End);
    BOF3_INJECT(BossMyria_State8Dispatch);
    BOF3_INJECT(BossMyria_State8Tick);
    BOF3_INJECT(BossMyria_State8Cost);
    BOF3_INJECT(BossMyria_State8TickUnless);
    BOF3_INJECT(BossMyria_State8Clear);
    BOF3_INJECT(BossMyria_State8Close);
    BOF3_INJECT(BossMyria_State8Check);
    BOF3_INJECT(BossMyria_Hook);
    BOF3_INJECT(BossMyria_SpawnFxAndWait);
    BOF3_INJECT(BossMyria_SpawnFx);
}
