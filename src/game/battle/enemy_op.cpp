// Moved here by tools/move_funcs.py from battle_e2.cpp, battle_e3.cpp, battle_flow.cpp, battle_fx_tasks.cpp, enemy_ai_ops.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/enemy_op.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle/enemy_op_callees.h"
#include "game/battle_e2.h"
#include "game/battle_e2_callees.h"
#include "game/battle_e3.h"
#include "game/battle_e3_callees.h"
#include "game/battle_flow.h"
#include "game/battle_flow_callees.h"
#include "game/battle_fx_tasks.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/enemy_ai_ops.h"
#include "game/enemy_ai_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a.h"
#include "game/rest_3a_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = enemy_op::at;

}  // namespace

using namespace battle_flow;

using namespace battle_fx_tasks;

using namespace enemy_ai_ops;

namespace {

using U = std::uint32_t;

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U Key(const void* p) { return Low32(p); }

unsigned char* PtrIn(const unsigned char* p) { return move_script::At(UL(p)); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// rv::Current<T>() is typed by who runs: a battle task slot, or the enemy an
// enemy op runs on (docs/battle_e2.md 1.1, 1.3).
// An enemy object and a party member by an actor's index, as the originals
// address them (32-bit wrap: an index below its base reads below the array).
unsigned char* EnemyObj(U index) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * battle_e2::at::kEnemyStride); }

unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e2::at::kPartyStride); }

unsigned char* Task(U slot) { return move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e2::at::kTaskStride); }

// An enemy by its index 0..7 (an argument's low byte): past 7 the original
// reads and writes past the eight objects (and past .data above 25).
unsigned char* EnemyArg(const char* who, U word) {
    const unsigned index = word & 0xFF;
    if (index > 7)
        bof3::Fatal("%s: enemy %u, past the eight enemy objects - the original indexes past them (docs/battle_e2.md "
                    "section 7)",
                    who, index);
    return EnemyObj(index);
}

// A task slot BattleTask_Create answered: 0xFF (none free) is not tested by
// the original, which then writes past the 48 slots (known-defects D163's
// form). Ours aborts.
unsigned char* NewTask(const char* who, unsigned slot) {
    slot &= 0xFF;
    if (slot >= 48)
        bof3::Fatal("%s: BattleTask_Create answered %u - the original writes past the 48 task slots (docs/battle_e2.md "
                    "section 7)",
                    who, slot);
    return Task(slot);
}

// jmp [table + 4 * Sprite_Current[at]] over a .data table of `n`: the entry as
// read (the fuzz swaps the cells for recorders), the caller's word handed on
// and the entry's eax answered (the jmp leaves both in place); a Fatal past
// the table (the original jumps through the next table's cells).
using Entry = U (__cdecl*)(U);

U Dispatch(const char* who, U table, unsigned n, unsigned at_, U word) {
    const unsigned state = Sprite_Current[at_];
    if (state >= n)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e2.md section 7)",
                    who, at_, state, n, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(UL(move_script::At(table + 4 * state))))(word);
}

using boss_harness::Handler;

using move_script::SetLong;

using move_script::SetWord;

unsigned char* PtrAt(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }

unsigned char* S() { return Sprite_Current; }

U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

// The ability record `id` (24 bytes at 0x65C4D8): a flag byte, or the word +4.
unsigned AbilityId() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbility.v))); }

unsigned char AbilityByte(U base) { return move_script::At(base + AbilityId() * battle_e3::at::kAbilityStride)[0]; }

unsigned AbilityWord4() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbilityWord4.v) + AbilityId() * battle_e3::at::kAbilityStride)); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it reads that word), its eax passed back.
using Forward = U (__cdecl*)(unsigned);

U EntryBattleE3(U table, unsigned index) { return ULong(move_script::At(table + 4 * index)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as read
// (the fuzz swaps the cells for recorders); a Fatal past them (the original
// jumps through the dword after, another table's entry or data).
unsigned CheckedState(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e3.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return state;
}

U DispatchBattleE3(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(EntryBattleE3(table, CheckedState(who, table, entries, at))))(passed);
}

// The two moves the objects make: +0x34 / +0x38 by the velocity +0xC / +0x10,
// forward or back.
void MoveBy(unsigned char* s, bool back) {
    const U x = ULong(s + 0x34), vx = ULong(s + 0xC);
    SetULong(s + 0x34, back ? x - vx : x + vx);
    const U y = ULong(s + 0x38), vy = ULong(s + 0x10);
    SetULong(s + 0x38, back ? y - vy : y + vy);
}

}  // namespace

namespace battle_flow {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_flow

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleFlow(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_flow::Long(battle_flow::At(address)))));
}

unsigned char* EnemyObject(unsigned index) { return battle_flow::At(::bof3::Img(Battle_Enemies_addr) + index * battle_flow::at::kEnemySize); }

// The event hook 0x904B6C and the enemies' +0xF4 hooks.
using HookFn = void (__cdecl*)(int);

}  // namespace

namespace battle_fx_tasks {

using move_script::At;

}  // namespace battle_fx_tasks

namespace enemy_ai_ops {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace enemy_ai_ops

namespace {

// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAtEnemyAiOps(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(enemy_ai_ops::Long(enemy_ai_ops::At(address)))));
}

unsigned char* PtrInEnemyAiOps(const unsigned char* p) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(enemy_ai_ops::Long(p))));
}

void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    enemy_ai_ops::SetLong(enemy_ai_ops::At(address), static_cast<std::int32_t>(Low32(p)));
}

std::uint32_t ULongEnemyAiOps(const unsigned char* p) { return static_cast<std::uint32_t>(enemy_ai_ops::Long(p)); }

void SetULongEnemyAiOps(unsigned char* p, std::uint32_t v) { enemy_ai_ops::SetLong(p, static_cast<std::int32_t>(v)); }

// The op tables: entry `index` of the code pointers at `table`. As the
// originals have it, the index is not checked - it is one of the object's
// step bytes, and a step past a table's end runs the next table's entries.
enemy_ai_ops::Handler EntryEnemyAiOps(std::uint32_t table, unsigned index) {
    return reinterpret_cast<const ::bof3::CodeSlot<enemy_ai_ops::Handler>*>(static_cast<std::uintptr_t>(table))[index];   // 32-bit code words
}

// The 24-byte ability record's flag bytes, by the word 0x904B80 read afresh.
unsigned char AbilityByteEnemyAiOps(std::uint32_t flags) { return enemy_ai_ops::At(flags + enemy_ai_ops::Word(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kMagicId.v))) * 24u)[0]; }

unsigned char* EnemyByActor(unsigned actor, const char* who) {
    const unsigned index = (actor & 0xFF) - 3u;
    if (index >= 8)
        bof3::Fatal("%s: actor %u is no enemy (3..10) - the original runs BattleEnemy_SetAnimation on a record outside the "
                    "eight (docs/rest_3a.md section 7)",
                    who, actor & 0xFF);
    return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * rest_3a::at::kEnemySize);
}

}  // namespace

// original 0x435830 (no PSX twin paired): once a frame from the battle's
// frame. Each of the eight enemy objects whose byte +0 is not zero becomes
// Sprite_Current and 0x939AD8, and runs the entry of the state table 0x64B084
// that its byte +0x100 picks. While the byte 0x904AAA is set it instead calls
// its +0xF4 with 2 (when its byte +1 is not zero) and then the entry of
// 0x64B088 - the same table one entry on - picked by +0x100 of the object
// 0x939AD8 holds after that call.
//
// As the original has it: 0x904AAA is read afresh for each object; the
// state is not checked (0x64B088 with state 0 is 0x64B084's second dword, a
// null). The event path re-reads 0x939AD8 after the hook; the other reads
// the loop's own object.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_RunAll(void) {
    for (unsigned i = 0; i < battle_flow::at::kEnemyCount; ++i) {
        unsigned char* const e = EnemyObject(i);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
        if (enemy_rec->flags == 0) continue;
        const unsigned char event = EventBattle_Id;
        Sprite_Current = e;
        BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(e);
        if (event != 0) {
            if (enemy_rec->state != 0) reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(enemy_rec->event_hook))))(2);
            const unsigned state = rv::As<records::BattleEnemy>(Enemy())->run_state;
            reinterpret_cast<const ::bof3::CodeSlot<battle_flow::Handler>*>(static_cast<std::uintptr_t>(::bof3::Img(BattleEnemy_States_addr) + 4))[state]();
        } else {
            const unsigned state = enemy_rec->run_state;
            reinterpret_cast<const ::bof3::CodeSlot<battle_flow::Handler>*>(static_cast<std::uintptr_t>(::bof3::Img(BattleEnemy_States_addr)))[state]();
        }
    }
}

// original 0x4358A0 (no PSX twin paired): once a frame from the battle's
// frame, after BattleEnemy_RunAll. Each enemy object whose byte +0 is not
// zero becomes Sprite_Current and 0x939AD8 and gets Sprite_UpdateScreen.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_UpdateScreenAll(void) {
    for (unsigned i = 0; i < battle_flow::at::kEnemyCount; ++i) {
        unsigned char* const e = EnemyObject(i);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
        if (enemy_rec->flags == 0) continue;
        Sprite_Current = e;
        BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(e);
        battle_flow::g.update_screen();
    }
}

// original 0x4358D0 (PSX 0x801E2298): the enemy's animation `animation` -
// the byte of 0x939AD8's table +0xFC at `animation`, or at `animation` + 1
// when Sprite_Current's byte +8 is 2 or 3 - through Sprite_EnsureAnimation
// (its low seven bits), and the sprite's flip byte +0x2A from its bit 7: set
// for modes 0 and 2, inverted for 1 and 3. A mode above 3 does nothing.
//
// As the original has it: the mode is read first; 0x939AD8, its table
// pointer and the byte are read again after the call; the flip goes to the
// Sprite_Current of after the call. The index is the argument's low byte, not
// wrapped when 1 is added.
extern "C" void __cdecl BattleEnemy_SetAnimation(unsigned animation) {
    const unsigned mode = Sprite_Current[8];
    if (mode > 3) return;
    const unsigned index = (animation & 0xFF) + (mode >= 2 ? 1u : 0u);
    const auto table = [] { return PtrAtBattleFlow(Low32(Enemy() + 0xFC)); };
    battle_flow::g.ensure_animation(static_cast<unsigned char>(table()[index] & 0x7F));
    const bool high = (table()[index] & 0x80) != 0;
    const bool flip = (mode == 0 || mode == 2) ? high : !high;
    Sprite_Current[0x2A] = flip ? 1 : 0;
}

// original 0x435A20 (PSX 0x801E247C, call-anchored): enemy (actor & 0xFF) - 3
// made Sprite_Current and 0x939AD8 for BattleEnemy_SetAnimation(animation, the
// whole word handed on); both put back after.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationAs(unsigned actor, unsigned animation) {
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationAs");
    unsigned char* const sprite = Sc();
    records::BattleEnemy* const current = BattleE3_CurrentEnemy;
    Sprite_Current = enemy;
    BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(enemy);
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    BattleE3_CurrentEnemy = current;
    Sprite_Current = sprite;
}

// original 0x435A70 (PSX 0x801E2500, call-anchored): the same with 0x939AD8
// alone (Sprite_Current untouched).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationOf(unsigned actor, unsigned animation) {
    records::BattleEnemy* const current = BattleE3_CurrentEnemy;
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationOf");
    BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(enemy);
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    BattleE3_CurrentEnemy = current;
}

// original 0x435AB0 (PSX 0x801E2564, call; Battle_BeginAction's call for an
// enemy with actor - 3): the enemy's action kind 0x904B35 - two bits of
// 0x65563C[the enemy's +0x8E] chosen by Rand() & 3; 0 when the round flags
// have 0x4000 and the enemy's u16 +0xBA is at most 1. Then 0 becomes 1; 1
// becomes 2 with the flag word +0x90's 0x80 (and u32 +0x110 |= 2), else 1; 2
// becomes 3 with +0x90's 0x40, else 1; 3 becomes 4 with +0x90's 0x20 (the
// ability u16 +0x106 = one of the eight bytes +0x9C by Rand() & 7), else 1.
// An enemy with status +0x92's 0x20 or u32 +0x114's 0x4000 acts at random:
// kind 1 and, when 0x452DD0(actor) allows, BattleEnemy_PickAnyTarget; any
// other, when 0x452DD0 allows, 0x904B44 = BattleEnemy_PickTarget. In an
// event battle the enemy becomes 0x939AD8 and Sprite_Current and its +0xF4
// hook is called with 0. A target of 0xFF clears the kind and the enemy's
// +0x105; else +0x105 = the kind.
//
// As the original has it: the enemy index is not checked (ours aborts past
// 7); the target is read after the hook.
extern "C" void __cdecl BattleEnemy_PickAction(unsigned enemy) {
    static const char* const kWho = "BattleEnemy_PickAction";
    unsigned char* const e = EnemyArg(kWho, enemy);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    const unsigned r = static_cast<unsigned>(BH_CALL(Rand)());
    unsigned char kind = static_cast<unsigned char>((move_script::At(::bof3::Img(battle_e2::at::kActKinds.v) + enemy_rec->odds_index)[0] >> ((r & 3) * 2)) & 3);
    B(::bof3::Img(battle_e2::at::kActKind.v)) = kind;
    if ((UL(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v))) & 0x4000) != 0 && enemy_rec->intel <= 1) {
        kind = 0;
        B(::bof3::Img(battle_e2::at::kActKind.v)) = 0;
    }
    switch (kind) {
    case 1:
        if ((e[0x90] & 0x80) != 0) {
            B(::bof3::Img(battle_e2::at::kActKind.v)) = 2;
            enemy_rec->flags_110 = static_cast<U>(enemy_rec->flags_110) | 2;
        } else {
            B(::bof3::Img(battle_e2::at::kActKind.v)) = 1;
        }
        break;
    case 2:
        B(::bof3::Img(battle_e2::at::kActKind.v)) = static_cast<unsigned char>(((e[0x90] & 0x40) | 0x20) >> 5);
        break;
    case 3:
        if ((e[0x90] & 0x20) != 0) {
            B(::bof3::Img(battle_e2::at::kActKind.v)) = 4;
            const unsigned r2 = static_cast<unsigned>(BH_CALL(Rand)()) & 7;
            enemy_rec->ability = e[0x9C + r2];
        } else {
            B(::bof3::Img(battle_e2::at::kActKind.v)) = 1;
        }
        break;
    default:
        B(::bof3::Img(battle_e2::at::kActKind.v)) = 1;
        break;
    }
    const auto may_act = BH_AT(unsigned char (__cdecl*)(unsigned), battle_e2::at::kActorMayAct.v);
    const unsigned actor = (enemy + 3) & 0xFF;
    if ((e[0x92] & 0x20) != 0 || (static_cast<U>(enemy_rec->flags_114) & 0x4000) != 0) {
        B(::bof3::Img(battle_e2::at::kActKind.v)) = 1;
        if (may_act(actor) != 0) BH_CALL(BattleEnemy_PickAnyTarget)(enemy);
    } else if (may_act(actor) != 0) {
        B(::bof3::Img(battle_e2::at::kTarget.v)) = BH_CALL(BattleEnemy_PickTarget)(enemy);
    }
    if (EventBattle_Id != 0) {
        BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(e);
        Sprite_Current = e;
        reinterpret_cast<boss_harness::Hook>(static_cast<std::uintptr_t>(static_cast<U>(enemy_rec->event_hook)))(0);
    }
    if (B(::bof3::Img(battle_e2::at::kTarget.v)) == 0xFF) {
        enemy_rec->command_kind = 0;
        B(::bof3::Img(battle_e2::at::kActKind.v)) = 0;
    } else {
        enemy_rec->command_kind = B(::bof3::Img(battle_e2::at::kActKind.v));
    }
}

// original 0x435C40 (PSX 0x801E2A0C, call-anchored): the target 0x904B44 of an
// enemy that acts at random: three times in eight (Rand() & 7 below 3), or
// with one enemy left, a random member; else another enemy
// (Battle_RandomEnemy(enemy + 3)). Answers it.
extern "C" unsigned char __cdecl BattleEnemy_PickAnyTarget(unsigned enemy) {
    const unsigned r = static_cast<unsigned>(BH_CALL(Rand)()) & 7;
    unsigned char t;
    if (r < 3 || Battle_EnemiesLeft == 1) t = BH_CALL(Battle_RandomMember)();
    else t = BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
    B(::bof3::Img(battle_e2::at::kTarget.v)) = t;
    return t;
}

// original 0x435C80 (no PSX twin paired): a random enemy actor: of 3..10,
// those Battle_ActorIsOut rules in and that are not `exclude` (its low
// byte), the one at Rand() % their count.
//
// As the original has it: the count is not tested (0 divides by zero; ours
// aborts); Rand's answer is taken signed (the CRT's is 0..0x7FFF).
extern "C" unsigned char __cdecl Battle_RandomEnemy(unsigned exclude) {
    unsigned char list[8] = {};
    unsigned char count = 0;
    for (unsigned char a = 3; a <= 10; ++a)
        if (BH_CALL(Battle_ActorIsOut)(a) == 0 && (exclude & 0xFF) != a) list[count++] = a;
    const int r = BH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomEnemy: no enemy left to pick - the original divides by 0 (docs/battle_e2.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomEnemy: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// original 0x435CF0 (its PSX twin by shape JP 0x801E2A0C, not the pairing's
// 0x801E28CC): the target of enemy `enemy` (its low byte) by the action kind
// 0x904B35. Kind 1: with the
// enemy's flag word +0x90 bit 0 another enemy (BattleEnemy_OtherEnemy, a
// member when that answers 0xFF), bit 4 the weakest member, else a random
// member. Kind 4: by the ability's (+0x106, NameTable_Abilities) flag byte:
// with 0x10 a side code (0xC0 for 0x80 without 0x40, else 0x80 / 0x40 by
// 0x20 - swapped when the enemy's +0x90 bit 0 is clear); without 0x40 itself
// (enemy + 3); with 0x20 (and +0x90 bit 0) BattleEnemy_OtherOrMember, (bit 0
// clear) the +0x90 bit 4 test below; without 0x20, bit 0 set: +0x90 bit 4 the
// weakest member else a random one; bit 0 clear: the weakest enemy. Other
// kinds: the enemy's actor byte +5.
extern "C" unsigned char __cdecl BattleEnemy_PickTarget(unsigned enemy) {
    static const char* const kWho = "BattleEnemy_PickTarget";
    const unsigned kind = B(::bof3::Img(battle_e2::at::kActKind.v));
    if (kind == 1) {
        const unsigned flags = rv::As<records::BattleEnemy>(EnemyArg(kWho, enemy))->flags_90;
        if ((flags & 1) != 0) {
            const unsigned char t = BH_CALL(BattleEnemy_OtherEnemy)(enemy);
            if (t != 0xFF) return t;
            return BH_CALL(Battle_RandomMember)();
        }
        if ((flags & 0x10) != 0) return BH_CALL(Battle_MemberLowestHp)();
        return BH_CALL(Battle_RandomMember)();
    }
    if (kind != 4) return rv::As<records::BattleEnemy>(EnemyArg(kWho, enemy))->actor;
    auto* const enemy_rec = rv::As<records::BattleEnemy>(EnemyArg(kWho, enemy));
    const unsigned id = enemy_rec->ability;
    const unsigned flags = enemy_rec->flags_90;
    const unsigned char a = move_script::At(::bof3::Img(battle_e2::at::kAbilities.v) + id * 24)[0];
    const bool one = (flags & 1) != 0;
    if ((a & 0x10) != 0) {
        if ((a & 0x80) != 0 && (a & 0x40) == 0) return 0xC0;
        if (one) return (a & 0x20) != 0 ? 0x40 : 0x80;
        return (a & 0x20) != 0 ? 0x80 : 0x40;
    }
    if ((a & 0x40) == 0) return static_cast<unsigned char>((enemy & 0xFF) + 3);
    if (one) {
        if ((a & 0x20) != 0) return BH_CALL(BattleEnemy_OtherOrMember)(enemy);
    } else if ((a & 0x20) == 0) {
        return BH_CALL(Battle_EnemyLowestHp)();
    }
    if ((flags & 0x10) == 0) return BH_CALL(Battle_RandomMember)();
    return BH_CALL(Battle_MemberLowestHp)();
}

// original 0x435E10 (no PSX twin paired): the enemy actor 3..10, not out,
// with the lowest HP (+0xA4) below 0xFFFF, the first of equals; 0 for none.
extern "C" unsigned char __cdecl Battle_EnemyLowestHp(void) {
    unsigned char best = 0;
    unsigned lowest = 0xFFFF;
    for (unsigned char a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned hp = rv::As<records::BattleEnemy>(EnemyObj(a - 3u))->hp;
        if (lowest > hp) {
            lowest = hp;
            best = a;
        }
    }
    return best;
}

// original 0x435E70 (no PSX twin paired): with one enemy left (0x904AB3) a
// random member (a tail jmp to Battle_RandomMember), else another enemy
// (Battle_RandomEnemy(enemy + 3)).
extern "C" unsigned char __cdecl BattleEnemy_OtherOrMember(unsigned enemy) {
    if (Battle_EnemiesLeft == 1) return BH_CALL(Battle_RandomMember)();
    return BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
}

// original 0x435E90 (no PSX twin paired): the member 0..2, not out, with the
// lowest HP (+0x98) below 10000, the first of equals; 0 for none.
extern "C" unsigned char __cdecl Battle_MemberLowestHp(void) {
    unsigned char best = 0;
    unsigned lowest = 0x2710;
    for (unsigned char m = 0; m <= 2; ++m) {
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        const unsigned hp = rv::As<records::PartyMember>(Member(m))->hp;
        if (lowest > hp) {
            lowest = hp;
            best = m;
        }
    }
    return best;
}

// original 0x435EF0 (PSX 0x801E2A0C's callee; no twin paired): a random party
// member by the formation's weights: with 3 members (0x904AB0's low byte),
// those not out weigh the bytes 0x64B18C[formation x 3 + member], with 2
// 0x64B184[formation x 2 + member] (the formation 0x904060's low byte, read
// afresh); Rand() % the sum (a byte) picks the member whose running sum first
// passes it; none: the count (3 or 2). Another count: 0.
//
// As the original has it: the sum is a byte and is not tested (0 divides by
// zero; ours aborts); each Battle_ActorIsOut is asked again on the second
// pass.
extern "C" unsigned char __cdecl Battle_RandomMember(void) {
    const unsigned n = UL(move_script::At(::bof3::Img(battle_e2::at::kPartyCount.v))) & 0xFF;
    if (n != 2 && n != 3) return 0;
    const U weights = n == 3 ? ::bof3::Img(battle_e2::at::kFormationWeights3.v) : ::bof3::Img(battle_e2::at::kFormationWeights2.v);
    unsigned char sum = 0;
    for (unsigned char m = 0; m < n; ++m)
        if (BH_CALL(Battle_ActorIsOut)(m) == 0) sum = static_cast<unsigned char>(sum + move_script::At(weights + (UL(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd0)) & 0xFF) * n + m)[0]);
    const int r = BH_CALL(Rand)();
    if (sum == 0) bof3::Fatal("Battle_RandomMember: the weights sum to 0 - the original divides by 0 (docs/battle_e2.md section 7)");
    const auto pick = static_cast<unsigned char>(r % static_cast<int>(sum));
    unsigned char run = 0;
    for (unsigned char m = 0; m < n; ++m) {
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        run = static_cast<unsigned char>(run + move_script::At(weights + (UL(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd0)) & 0xFF) * n + m)[0]);
        if (run > pick) return m;
    }
    return static_cast<unsigned char>(n);
}

// original 0x436070 (no PSX twin paired): with one enemy left 0xFF, else
// another enemy (Battle_RandomEnemy(enemy + 3)).
extern "C" unsigned char __cdecl BattleEnemy_OtherEnemy(unsigned enemy) {
    if (Battle_EnemiesLeft == 1) return 0xFF;
    return BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
}

// original 0x436090 (PSX 0x801E3070, the same): an enemy state's animation
// tick. 1 without a tick while 0x939AD8's status byte +0x92 has 0x04 or
// 0x40, or while 0x904B8E is set and its +0x114 lacks 0x10; else a tail jump
// to Sprite_ScriptTick, whose answer it is.
extern "C" unsigned char __cdecl BattleEnemy_ScriptTick(void) {
    const unsigned char* const e = Enemy();
    if ((e[0x92] & 0x44) != 0) return 1;
    if (BattleFlow_AnimGate != 0 && (e[0x114] & 0x10) == 0) return 1;
    return battle_flow::g.script_tick();
}

// original 0x4360C0 (PSX 0x801E30D8, the same): BattleEnemy_ScriptTick with
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleEnemy_ScriptTickOnce(void) {
    const unsigned char* const e = Enemy();
    if ((e[0x92] & 0x44) != 0) return 1;
    if (BattleFlow_AnimGate != 0 && (e[0x114] & 0x10) == 0) return 1;
    return battle_flow::g.script_tick_once();
}

// original 0x4360F0 (PSX 0x801E3140): state 0 of the enemy state table
// 0x64B084 - a tail jump through EnemyOp_Steps 0x64B1A0 by Sprite_Current +1.
extern "C" void __cdecl EnemyOp_StepDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_Steps_addr), rv::As<records::BattleEnemy>(Sc())->state)(); }

// original 0x436110 (PSX 0x801E3184): step 0. The enemy's animation table
// +0xFC = 0x64B078 (the bytes 0..7); the dwords 0x904B64 / 0x904B68 =
// 0x437720 / 0x437750; animation 0; Sprite_Current +0 |= 0x40;
// Battle_StatusTint(the enemy's status word +0x92); step +1 on.
//
// As the original has it: 0x939AD8 is read afresh for the status word, and
// Sprite_Current after each call. The status goes out as a word with the
// upper half of whatever edx held; Battle_StatusTint reads bit 7 alone.
extern "C" void __cdecl EnemyOp_Begin(void) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = ::bof3::Img(enemy_ai_ops::at::kAnimTable.v);
    BattleHook_End = ::bof3::Code(enemy_ai_ops::at::kHookAValue.v);
    BattleHook_Exit = ::bof3::Code(enemy_ai_ops::at::kHookBValue.v);
    enemy_ai_ops::g.set_animation(0);
    rv::As<records::BattleEnemy>(Sc())->flags = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->flags | 0x40);
    enemy_ai_ops::g.status_tint(rv::As<records::BattleEnemy>(Enemy())->status);
    rv::As<records::BattleEnemy>(Sc())->state = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->state + 1);
}

// original 0x436170 (PSX 0x801E3224): step 1, the entrance - through
// EnemyOp_EnterSubs 0x64B1D4 by +2 (0x436190, EnemyOp_SlideDispatch 0x436270).
extern "C" void __cdecl EnemyOp_EnterDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_EnterSubs_addr), rv::As<records::BattleEnemy>(Sc())->step)(); }

// original 0x436190 (PSX 0x801E3268): the entrance's first part, the
// scale-in - through EnemyOp_ScaleInSubs 0x64B1DC by +3 (0x4361B0, 0x436210).
extern "C" void __cdecl EnemyOp_ScaleInDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_ScaleInSubs_addr), rv::As<records::BattleEnemy>(Sc())->state3)(); }

// original 0x4361B0 (PSX 0x801E32AC): the scale-in's start. Sprite_Current
// +0 &= ~0x40, then animation 0; +0x48 = 2, the dwords +0x44 / +0x40 / +0xC
// = 0 and +0x18 = 0x666; +3 = 1.
extern "C" void __cdecl EnemyOp_ScaleInStart(void) {
    rv::As<records::BattleEnemy>(Sc())->flags = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->flags & 0xBF);
    enemy_ai_ops::g.set_animation(0);
    Sc()[0x48] = 2;
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_x, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_18, 0x666);
    rv::As<records::BattleEnemy>(Sc())->state3 = 1;
}

// original 0x436210 (PSX 0x801E3310): the scale-in, a frame. While the s32
// +0x40 is below 0x10000 (1.0): +0x40 = it + the speed +0xC, +0x44 += +0xC,
// the speed += +0x18. Otherwise +0x48 = 0 and the steps +1..+3 = 3, 0, 0.
// Either way a tail jump to BattleEnemy_ScriptTick.
extern "C" void __cdecl EnemyOp_ScaleInStep(void) {
    unsigned char* const s = Sc();
    const std::uint32_t scale = static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(s)->scale_x);
    if (static_cast<std::int32_t>(scale) < 0x10000) {
        rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(s)->scale_x, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(s)->step_x) + scale);
        rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x));
        rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_18));
    } else {
        s[0x48] = 0;
        rv::As<records::BattleEnemy>(Sc())->state = 3;
        rv::As<records::BattleEnemy>(Sc())->step = 0;
        rv::As<records::BattleEnemy>(Sc())->state3 = 0;
    }
    enemy_ai_ops::g.script_tick();
}

// original 0x436270 (PSX 0x801E33B0, gap44; hidden in 0x4360C0's catalogue
// extent): EnemyOp_EnterSubs 1 - through EnemyOp_EnterSubs2 0x64B1E4 by +3
// (2 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_SlideDispatch(unsigned long word) {
    return Dispatch("EnemyOp_SlideDispatch", AddressOf(EnemyOp_EnterSubs2), 2, 3, word);
}

// original 0x436290 (PSX 0x801E33F4, gap44): EnemyOp_EnterSubs2 0: +0 bit
// 0x40 cleared, animation 0; the destination +0x18 / +0x1C = the place +0x34
// / +0x38; the velocity (0x80000, 0) turned by the facing (0x446770, BE4's)
// taken off +0x34, and its second half off +0x34 again; then the velocity
// (0x8000, 0) turned; +2 on.
//
// As the original has it: the second component is subtracted from +0x34, not
// +0x38 (described in docs/battle_e2.md section 7, not changed); +2 steps,
// not +3.
extern "C" void __cdecl EnemyOp_SlideStart(void) {
    rv::Current<records::BattleEnemy>()->flags = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->flags & 0xBF);
    BH_CALL(BattleEnemy_SetAnimation)(0);
    const auto turn = BH_AT(void (__cdecl*)(unsigned char*), battle_e2::at::kTurnVelocity.v);
    rv::Store<U>(rv::Current<records::BattleEnemy>()->work_18, static_cast<U>(rv::Current<records::BattleEnemy>()->x));
    rv::Store<U>(rv::Current<records::BattleEnemy>()->work_1c, static_cast<U>(rv::Current<records::BattleEnemy>()->z));
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_x, 0x80000);
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_z, 0);
    turn(Sprite_Current);
    rv::Current<records::BattleEnemy>()->x = static_cast<U>(rv::Current<records::BattleEnemy>()->x) - static_cast<U>(rv::Current<records::BattleEnemy>()->step_x);
    rv::Current<records::BattleEnemy>()->x = static_cast<U>(rv::Current<records::BattleEnemy>()->x) - static_cast<U>(rv::Current<records::BattleEnemy>()->step_z);
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_x, 0x8000);
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_z, 0);
    turn(Sprite_Current);
    ++rv::Current<records::BattleEnemy>()->step;
}

// original 0x436330 (PSX 0x801E34A4, gap44): EnemyOp_EnterSubs2 1:
// BattleEnemy_ScriptTick (its answer unused); the place +0x34 / +0x38 moved
// by the velocity +0xC / +0x10; u16 +0x3E = AreaMap_Elevation there; at the
// destination +0x18 / +0x1C the steps +1..+3 = 3, 0, 0.
extern "C" void __cdecl EnemyOp_SlideStep(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    rv::Current<records::BattleEnemy>()->x = static_cast<U>(rv::Current<records::BattleEnemy>()->x) + static_cast<U>(rv::Current<records::BattleEnemy>()->step_x);
    rv::Current<records::BattleEnemy>()->z = static_cast<U>(rv::Current<records::BattleEnemy>()->z) + static_cast<U>(rv::Current<records::BattleEnemy>()->step_z);
    const long ground = BH_CALL(AreaMap_Elevation)(rv::Current<records::BattleEnemy>()->x, rv::Current<records::BattleEnemy>()->z);
    rv::Store<unsigned>(rv::Current<records::BattleEnemy>()->height_whole, static_cast<unsigned>(ground));
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    if (static_cast<U>(cur_enemy->x) != static_cast<U>(cur_enemy->work_18) || static_cast<U>(cur_enemy->z) != static_cast<U>(cur_enemy->work_1c)) return;
    cur_enemy->state = 3;
    rv::Current<records::BattleEnemy>()->step = 0;
    rv::Current<records::BattleEnemy>()->state3 = 0;
}

// original 0x4363B0 (PSX 0x801E3550): step 2. Animation 8 when bit 1 of the
// enemy's +0x110 is set, else 0; BattleEnemy_ScriptTick (its answer
// unused); step +1 on.
extern "C" void __cdecl EnemyOp_Idle(void) {
    enemy_ai_ops::g.set_animation((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
    enemy_ai_ops::g.script_tick();
    rv::As<records::BattleEnemy>(Sc())->state = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->state + 1);
}

// original 0x4363E0 (PSX 0x801E35B0): step 3, the idle loop, every frame
// until something else moves the step. First the highlight sub-op of
// EnemyOp_WaitSubs 0x64B1EC by +2 (called). Then, while the enemy's status
// +0x92 has 0x20, a bob: the byte +9 of the battle-task slot numbered by
// the actor byte +5 steps up while bit 11 of +0x110 is set and down while
// it is clear, and the bit flips when the byte reaches 12 or -12; the
// sprite's +0x34 (modes 1 and 3) or +0x38 (the others) = the slot's same
// dword + (the s8 byte << 9). Last, unless bit 12 of +0x110 is set, a tail
// jump to BattleEnemy_ScriptTick.
//
// As the original has it: 0x939AD8 and Sprite_Current are read once after
// the call; the actor byte and the slot's byte are re-read at each use (a
// slot past the 48 is not refused - the actor byte is 3..10 for an enemy).
extern "C" void __cdecl EnemyOp_Wait(void) {
    EntryEnemyAiOps(::bof3::Img(EnemyOp_WaitSubs_addr), rv::As<records::BattleEnemy>(Sc())->step)();
    unsigned char* const e = Enemy();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    if ((e[0x92] & 0x20) != 0) {
        unsigned char* const s = Sc();
        auto* const enemy_s = rv::As<records::BattleEnemy>(s);
        const auto slot = [s] { return enemy_ai_ops::At(::bof3::Img(BattleTask_Slots_addr) + s[5] * enemy_ai_ops::at::kTaskSize); };
        bool flip;
        if ((enemy_rec->flags_110 & 0x800) != 0) {
            slot()[9] = static_cast<unsigned char>(slot()[9] + 1);
            flip = static_cast<signed char>(slot()[9]) >= 0xC;
        } else {
            slot()[9] = static_cast<unsigned char>(slot()[9] - 1);
            flip = static_cast<signed char>(slot()[9]) <= -0xC;
        }
        if (flip) enemy_rec->flags_110 = enemy_rec->flags_110 ^ 0x800u;
        const unsigned mode = enemy_s->layout;
        const unsigned field = (mode == 1 || mode == 3) ? 0x34u : 0x38u;
        unsigned char* const t = slot();
        const std::int32_t bob = static_cast<signed char>(t[9]);
        SetULongEnemyAiOps(s + field, static_cast<std::uint32_t>(bob << 9) + ULongEnemyAiOps(t + field));
    }
    if ((rv::As<records::BattleEnemy>(Enemy())->flags_110 & 0x1000) == 0) enemy_ai_ops::g.script_tick();
}

// original 0x436510 (PSX 0x801E37B4): EnemyOp_Wait's sub-op 0. While a
// target is being picked (0x904AAF) and the command's target byte (the byte
// at the pointer 0x939FA0) is this actor (+5) or has 0x40 (a whole side), a
// tint record: Sprite_SetTint(Sprite_Current, 0, 0, 0, 0), its index to +7;
// +2 = 1.
extern "C" void __cdecl EnemyOp_HighlightOn(void) {
    if (BattleE4_Picking == 0) return;
    const unsigned char target = PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kCommandSource.v))[0];
    unsigned char* const s = Sc();
    auto* const enemy_s = rv::As<records::BattleEnemy>(s);
    if (target != enemy_s->actor && (target & 0x40) == 0) return;
    const unsigned char tint = enemy_ai_ops::g.set_tint(s, 0, 0, 0, 0);
    Sc()[7] = tint;
    rv::As<records::BattleEnemy>(Sc())->step = 1;
}

// original 0x436560 (PSX 0x801E3838): sub-op 1, the highlight's pulse. The
// tint record +7's r g b = the menu cursor's pulse 0x904AC8. When no target
// is being picked any more, or the target byte is neither this actor nor a
// side, Tint_Release(+7) and +2 = 0.
//
// As the original has it: Sprite_Current is read once before the call; +7
// is read again for each store; an index of 0xFF (Sprite_SetTint found no
// free record) writes past the 32 records.
extern "C" void __cdecl EnemyOp_HighlightPulse(void) {
    unsigned char* const s = Sc();
    auto* const enemy_s = rv::As<records::BattleEnemy>(s);
    const unsigned char pulse = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kPulse.v))[0];
    enemy_ai_ops::At(::bof3::Img(MoveScript_TintRecords_addr) + s[7] * enemy_ai_ops::at::kTintSize + 4)[0] = pulse;
    enemy_ai_ops::At(::bof3::Img(MoveScript_TintRecords_addr) + s[7] * enemy_ai_ops::at::kTintSize + 3)[0] = pulse;
    enemy_ai_ops::At(::bof3::Img(MoveScript_TintRecords_addr) + s[7] * enemy_ai_ops::at::kTintSize + 2)[0] = pulse;
    if (BattleE4_Picking != 0) {
        const unsigned char target = PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kCommandSource.v))[0];
        if (target == enemy_s->actor) return;
        if ((target & 0x40) != 0) return;
    }
    enemy_ai_ops::g.tint_release(s[7]);
    rv::As<records::BattleEnemy>(Sc())->step = 0;
}

// original 0x4365D0 (PSX 0x801E38F8, gap44): EnemyOp_Steps 4 (and every
// kind's Steps 4 in the boss tables): animation 2; +9 = the byte +0x8A of the
// area's enemy data record +0xF0 of 0x939AD8 (stride 0x8C); +1 on, +2 = 0.
extern "C" void __cdecl EnemyOp_TurnStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(2);
    const unsigned record = rv::As<records::BattleEnemy>(Enemy())->data_id;
    Sprite_Current[9] = move_script::At(::bof3::Img(battle_e2::at::kEnemyDataCount.v) + record * 0x8C)[0];
    ++rv::Current<records::BattleEnemy>()->state;
    rv::Current<records::BattleEnemy>()->step = 0;
}

// original 0x436620 (PSX 0x801E3978, gap44): EnemyOp_Steps 5 (and every
// kind's Steps 5) - through EnemyOp_Step5Subs 0x64B1F4 by +2 (2 entries; ours
// aborts past them).
extern "C" unsigned long __cdecl EnemyOp_CueDispatch(unsigned long word) {
    return Dispatch("EnemyOp_CueDispatch", AddressOf(EnemyOp_Step5Subs), 2, 2, word);
}

// original 0x436640 (PSX 0x801E39BC, the sibling's Battle_PlayCreatureCue,
// verified by reading: its script tick, the +9 count down, then the cue):
// EnemyOp_Step5Subs 0: BattleEnemy_ScriptTick (answer unused); +9 down by
// one; at 0 the cue - in an event battle 0x437450 (BE3's) with the u16 at
// 0x939AD8's +0xF8 table, else Sound_PlayEffect(0x600 + 2 x its +0xF0) - and
// +2 on.
extern "C" void __cdecl EnemyOp_PlayCreatureCue(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    if (Sprite_Current[9] != 0) return;
    const unsigned char fight = EventBattle_Id;
    const unsigned char* const e = Enemy();
    auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(e);
    if (fight != 0) {
        BH_AT(void (__cdecl*)(unsigned), battle_e2::at::kPlayCue.v)(move_script::Word(PtrIn(e + 0xF8)));
    } else {
        BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x600 + 2 * enemy_rec->data_id));
    }
    ++rv::Current<records::BattleEnemy>()->step;
}

// original 0x4366B0 (PSX 0x801E3A7C, gap44): EnemyOp_Step5Subs 1: once
// BattleEnemy_ScriptTick answers its end, Battle_SetTargetFlag40(0x904B44),
// 0x4376F0, the round flags |= 4 and a tail jmp to 0x4376A0 (both BE3's).
// Answers the tick's answer, or 0x4376A0's.
extern "C" unsigned long __cdecl EnemyOp_CueEnd(void) {
    const auto done = BH_CALL(BattleEnemy_ScriptTick)();
    if (done == 0) return done;
    BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(battle_e2::at::kTarget.v)));
    BH_AT(void (__cdecl*)(), battle_e2::at::kEnemyTaskChance.v)();
    B(::bof3::Img(battle_e2::at::kRoundFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(battle_e2::at::kRoundFlags.v)) | 4);
    return BH_AT(unsigned long (__cdecl*)(), battle_e2::at::kEnemyOpEnd.v)();
}

// original 0x4366E0 (PSX 0x801E3AE0): step 6 - through EnemyOp_ActSubs
// 0x64B1FC by +2 (0x436700, 0x436720, 0x436700, EnemyOp_Act3Dispatch 0x436BC0,
// 0x436D90, EnemyOp_Act5Dispatch 0x436F00).
extern "C" void __cdecl EnemyOp_ActDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_ActSubs_addr), rv::As<records::BattleEnemy>(Sc())->step)(); }

// original 0x436700 (PSX 0x801E3B24): EnemyOp_ActSubs' entries 0 and 2 -
// Sprite_Current +4 = 0, +2 = 1.
extern "C" void __cdecl EnemyOp_ActBegin(void) {
    rv::As<records::BattleEnemy>(Sc())->state4 = 0;
    rv::As<records::BattleEnemy>(Sc())->step = 1;
}

// original 0x436720 (PSX 0x801E3B48): the hit taken - through
// EnemyOp_HitSubs 0x64B214 by +3 (0x436740, 0x436A00, 0x436A20).
extern "C" void __cdecl EnemyOp_HitDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_HitSubs_addr), rv::As<records::BattleEnemy>(Sc())->state3)(); }

// original 0x436740 (PSX Battle_ResolveAction_Enemy 0x801E3B8C, the
// sibling's hypothesis name): the action aimed at this enemy lands.
//  - The status words 0x904B98 / 0x904B9A = 0; the result's sprite 0x904B5C
//    = Sprite_Current, its target 0x904B54 = the actor +5, the record
//    0x904B60 = the enemy +0x104, whose HP delta +4, AP delta +6 and flags
//    +8 are zeroed; the enemy's 32 bytes +0xB0.. copied to 0x939F80.
//  - The effect, by the action kind 0x904B35: 1 (an attack) - the HP delta
//    = Battle_ApplyDamage(0x904B34, 0x904B44); 4 and 5 (an ability, an item)
//    - Effect_ApplyResult; 0, 2, 3 and above 5 - nothing.
//  - In an event battle, the enemy's +0xF4 hook with 1.
//  - Flags bit 0 with an HP delta of 0: +2 = 3 (a miss) and nothing else.
//  - Unless bit 9 of +0x110: animation 4 (the hit) for an attack, for an
//    item whose class (0x591810(0x904B81, 0x904B80)) lacks 4, and for an
//    ability whose record's byte +8 lacks 4.
//  - Flags bit 0: the damage pop-up (the HP delta, the actor); bit 1: the AP
//    pop-up (0x453EB0).
//  - The status +0x92 |= 0x904B98, which is zeroed; EnemyAI_TurnCheck.
//  - An HP delta above 0 with the HP +0xA4 at 0: +9 = 0, with round flag
//    0x80 a tint (0x10, 0x10, 0x10), and the status = 0x4000 (dead) - not
//    ORed. Above 0 with HP left in an event battle: the cue 0x437450(the
//    word +4 at the enemy's +0xF8).
//  - Either delta below 0 (a heal): Sound_PlayEffect(0x206).
//  - Unless round flag 0x2000: the hit sound and the hit pop-up for an
//    attack, and for an ability whose byte +0xD has 8.
//  - Round flag 0x80 cleared; BattleEnemy_ScriptTick; +3 on.
//
// As the original has it: 0x939AD8 is re-read after every call, and so is
// Sprite_Current; 0x904B35 is read afresh at every test; the result record
// is written through 0x904B60 read afresh. The arguments go out as the
// original's bytes and words (its pushes carry other registers' upper
// bits, which every callee masks off).
extern "C" void __cdecl EnemyOp_ReceiveAction(void) {
    unsigned char* s = Sc();
    auto* const enemy_s = rv::As<records::BattleEnemy>(s);
    unsigned char* e = Enemy();
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(e);
    enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kStatusAdd2.v)), 0);
    enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kStatusAdd.v)), 0);
    const unsigned char actor = enemy_s->actor;
    BattleE3_HitSprite = s;
    enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kResultTarget.v))[0] = actor;
    SetPtrAt(::bof3::Img(enemy_ai_ops::at::kResult.v), e + 0x104);
    rv::Store<unsigned>(rv::As<records::ActionRecord>(PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kResult.v)))->hp_change, 0);
    rv::Store<unsigned>(rv::As<records::ActionRecord>(PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kResult.v)))->ap_change, 0);
    PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kResult.v))[8] = 0;
    e = Enemy();
    enemy_rec = rv::As<records::BattleEnemy>(e);
    for (unsigned i = 0; i < 32; i += 4) enemy_ai_ops::SetLong(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kStatBlock.v) + i), enemy_ai_ops::Long(e + 0xB0 + i));
    const unsigned char kind = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0];
    if (kind == 1) {
        const unsigned target = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kTarget.v))[0];
        const unsigned attacker = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActor.v))[0];
        const short delta = enemy_ai_ops::g.apply_damage(attacker, target);
        rv::Store<unsigned>(rv::As<records::ActionRecord>(PtrAtEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kResult.v)))->hp_change, static_cast<std::uint16_t>(delta));
        e = Enemy();
        enemy_rec = rv::As<records::BattleEnemy>(e);
    } else if (kind > 3 && kind <= 5) {
        enemy_ai_ops::g.effect_apply();
        e = Enemy();
        enemy_rec = rv::As<records::BattleEnemy>(e);
    }
    if (EventBattle_Id != 0) {
        reinterpret_cast<HookFn>(PtrInEnemyAiOps(e + 0xF4))(1);
        e = Enemy();
        enemy_rec = rv::As<records::BattleEnemy>(e);
    }
    if ((enemy_rec->shown & 1) != 0 && static_cast<std::uint16_t>(enemy_rec->hp_change) == 0) {
        rv::As<records::BattleEnemy>(Sc())->step = 3;
        return;
    }
    if ((enemy_rec->flags_110 & 0x200) == 0) {
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 1) {
            enemy_ai_ops::g.set_animation(4);
            e = Enemy();
            enemy_rec = rv::As<records::BattleEnemy>(e);
        }
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 5) {
            const unsigned id = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kMagicId.v))[0];
            const unsigned category = enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kMagicId.v) + 1)[0];
            if ((enemy_ai_ops::g.item_class(category, id) & 4) == 0) enemy_ai_ops::g.set_animation(4);
            e = Enemy();
            enemy_rec = rv::As<records::BattleEnemy>(e);
        }
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 4 && (AbilityByteEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kAbilityFlags8.v)) & 4) == 0) {
            enemy_ai_ops::g.set_animation(4);
            e = Enemy();
            enemy_rec = rv::As<records::BattleEnemy>(e);
        }
    }
    if ((enemy_rec->shown & 1) != 0) {
        enemy_ai_ops::g.damage_popup(static_cast<std::uint16_t>(enemy_rec->hp_change), rv::As<records::BattleEnemy>(Sc())->actor);
        e = Enemy();
        enemy_rec = rv::As<records::BattleEnemy>(e);
    }
    if ((enemy_rec->shown & 2) != 0) {
        enemy_ai_ops::g.ap_popup(static_cast<std::uint16_t>(enemy_rec->ap_change), rv::As<records::BattleEnemy>(Sc())->actor);
        e = Enemy();
        enemy_rec = rv::As<records::BattleEnemy>(e);
    }
    enemy_rec->status = enemy_rec->status | enemy_ai_ops::Word(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kStatusAdd.v)));
    enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kStatusAdd.v)), 0);
    enemy_ai_ops::g.turn_check();
    e = Enemy();
    enemy_rec = rv::As<records::BattleEnemy>(e);
    if (static_cast<short>(enemy_rec->hp_change) > 0) {
        if (enemy_rec->hp == 0) {
            Sc()[9] = 0;
            if ((enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] & 0x80) != 0) enemy_ai_ops::g.set_tint(Sc(), 0x10, 0x10, 0x10, 0);
            rv::As<records::BattleEnemy>(Enemy())->status = 0x4000;
            e = Enemy();
            enemy_rec = rv::As<records::BattleEnemy>(e);
        } else if (EventBattle_Id != 0) {
            enemy_ai_ops::g.play_cue(enemy_ai_ops::Word(PtrInEnemyAiOps(e + 0xF8) + 4));
            e = Enemy();
            enemy_rec = rv::As<records::BattleEnemy>(e);
        }
    }
    if (static_cast<short>(enemy_rec->hp_change) < 0 || static_cast<short>(enemy_rec->ap_change) < 0) enemy_ai_ops::g.play_effect(0x206);
    if ((ULongEnemyAiOps(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))) & 0x2000) == 0) {
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 1) {
            enemy_ai_ops::g.hit_sound();
            enemy_ai_ops::g.hit_popup();
        }
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 4 && (AbilityByteEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kAbilityFlagsD.v)) & 8) != 0) {
            enemy_ai_ops::g.hit_sound();
            enemy_ai_ops::g.hit_popup();
        }
    }
    enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v)), enemy_ai_ops::Word(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))) & 0xFF7F);
    enemy_ai_ops::g.script_tick();
    rv::As<records::BattleEnemy>(Sc())->state3 = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->state3 + 1);
}

// original 0x436A00 (PSX 0x801E406C): +3 on once BattleEnemy_ScriptTickOnce
// answers non-zero (the animation is done).
extern "C" void __cdecl EnemyOp_WaitAnimOnce(void) {
    if (enemy_ai_ops::g.script_tick_once() != 0) rv::As<records::BattleEnemy>(Sc())->state3 = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Sc())->state3 + 1);
}

// original 0x436A20 (PSX 0x801E40B4): the hit's end. Sprite_ScriptTickOnce
// (the sprite's own, not BattleEnemy_ScriptTickOnce), then:
//  - a dead enemy (+0x93 bit 6): Sprite_ReleaseTint(Sprite_Current), round
//    flag 0x40 cleared, the steps +1..+3 = 6, 4, 0 (the death) - and nothing
//    else;
//  - round flag 0x40 already set: it becomes 0x1000;
//  - otherwise, unless the target 0x904B44 has 0x40 or 0x80 (a side), round flag 0x40
//    is set on BattleEnemy_Chance70 for an attack, and for an ability whose
//    record's +0xD has 8 and whose +8 lacks 0x10 (Chance70 before the
//    second test).
//  - Then Battle_ClearActorBit(+5), the enemy's +0x110 bits 4..6 and 9
//    cleared, the steps = 2, 0, 0 (back to the idle loop).
//
// As the original has it: the 0x40 -> 0x1000 store is of the round flags'
// word, from the dword read with the flag's test; 0x904B80 is re-read after
// Chance70; 0x939AD8 is re-read for each of the two +0x110 stores.
extern "C" void __cdecl EnemyOp_HitEnd(void) {
    enemy_ai_ops::g.sprite_tick_once();
    if ((Enemy()[0x93] & 0x40) != 0) {
        enemy_ai_ops::g.release_tint(Sc());
        auto* const enemy_s = rv::As<records::BattleEnemy>(Sc());
        enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v)), enemy_ai_ops::Word(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))) & 0xFFBF);
        enemy_s->state = 6;
        rv::As<records::BattleEnemy>(Sc())->step = 4;
        rv::As<records::BattleEnemy>(Sc())->state3 = 0;
        return;
    }
    if ((enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] & 0x40) != 0) {
        enemy_ai_ops::SetWord(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v)), (enemy_ai_ops::Word(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))) & 0xFFBF) | 0x1000);
    } else if ((enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kTarget.v))[0] & 0xC0) == 0) {
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 1 && enemy_ai_ops::g.chance70() != 0)
            enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] = static_cast<unsigned char>(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] | 0x40);
        if (enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kActionKind.v))[0] == 4 && (AbilityByteEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kAbilityFlagsD.v)) & 8) != 0 && enemy_ai_ops::g.chance70() != 0 &&
            (AbilityByteEnemyAiOps(::bof3::Img(enemy_ai_ops::at::kAbilityFlags8.v)) & 0x10) == 0)
            enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] = static_cast<unsigned char>(enemy_ai_ops::At(::bof3::Img(enemy_ai_ops::at::kRoundFlags.v))[0] | 0x40);
    }
    enemy_ai_ops::g.clear_actor_bit(rv::As<records::BattleEnemy>(Sc())->actor);
    rv::As<records::BattleEnemy>(Enemy())->flags_110 = rv::As<records::BattleEnemy>(Enemy())->flags_110 & 0xFFFFFF8Fu;
    rv::As<records::BattleEnemy>(Enemy())->flags_110 = rv::As<records::BattleEnemy>(Enemy())->flags_110 & ~0x200u;
    rv::As<records::BattleEnemy>(Sc())->state = 2;
    rv::As<records::BattleEnemy>(Sc())->step = 0;
    rv::As<records::BattleEnemy>(Sc())->state3 = 0;
}

// original 0x436B50 (PSX 0x801E42C0, the same gates - the sibling's STEAL.md
// "false lead"): 0 when 0x939AD8's status word +0x92 has any of 0x4064, when
// 0x904B34 is 3 or more, when 0x904B35 is 4 and the word 0x904B80 is 0xA1,
// when 0x904B8E is set and +0x114 lacks 0x10, or when +0x90 lacks bit 1.
// Otherwise 1 when bit 15 of the dword +0x110 is set, else 1 for
// Rand() % 100 below 70 (signed, as idiv). What its callers (0x436AA1,
// 0x436AD1) do with it is unread.
extern "C" unsigned char __cdecl BattleEnemy_Chance70(void) {
    const unsigned char* const e = Enemy();
    auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(e);
    if ((enemy_rec->status & 0x4064) != 0) return 0;
    if (battle_flow::At(::bof3::Img(battle_flow::at::kTurnGate.v))[0] >= 3) return 0;
    if (battle_flow::At(::bof3::Img(battle_flow::at::kFormation.v))[0] == 4 && battle_flow::Word(battle_flow::At(::bof3::Img(battle_flow::at::kMagicId.v))) == 0xA1) return 0;
    if (BattleFlow_AnimGate != 0 && (e[0x114] & 0x10) == 0) return 0;
    if ((e[0x90] & 2) == 0) return 0;
    if ((static_cast<std::int32_t>(enemy_rec->flags_110) & 0x8000) != 0) return 1;
    return battle_flow::g.crt_rand() % 100 < 70 ? 1 : 0;
}

// original 0x436BC0 (PSX 0x801E43E8, table-anchored; hidden in 0x436B50's
// catalogue extent): EnemyOp_ActSubs 3 (and the kinds' ActSubs 3) - through
// EnemyOp_Act3Subs 0x64B220 by +3 (5 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_Act3Dispatch(unsigned long word) {
    return Dispatch("EnemyOp_Act3Dispatch", AddressOf(EnemyOp_Act3Subs), 5, 3, word);
}

// original 0x436BE0 (PSX 0x801E442C, table-anchored): EnemyOp_Act3Subs 0:
// 0x939AD8's u16 +0x108 = 0; the velocity (-0x2000, 0) turned by the facing
// (0x446770, BE4's); +0xA = 4; BattleEnemy_ScriptTick (answer unused);
// Sound_PlayEffect(0x205); +3 on.
extern "C" void __cdecl EnemyOp_KnockStart(void) {
    rv::As<records::BattleEnemy>(Enemy())->hp_change = 0;
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_x, 0xFFFFE000u);
    rv::Store<U>(rv::Current<records::BattleEnemy>()->step_z, 0);
    BH_AT(void (__cdecl*)(unsigned char*), battle_e2::at::kTurnVelocity.v)(Sprite_Current);
    Sprite_Current[0xA] = 4;
    BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sound_PlayEffect)(0x205);
    ++rv::Current<records::BattleEnemy>()->state3;
}

// original 0x436C40 (PSX 0x801E449C, table-anchored): EnemyOp_Act3Subs 1:
// +0xA down by one; at 0 it is 4 again and +3 on; the place +0x34 / +0x38
// moved by the velocity; a tail jmp to BattleEnemy_ScriptTick (its answer
// answered).
extern "C" unsigned char __cdecl EnemyOp_KnockBack(void) {
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    if (Sc()[0xA] == 0) {
        Sc()[0xA] = 4;
        ++rv::Current<records::BattleEnemy>()->state3;
    }
    rv::Current<records::BattleEnemy>()->x = static_cast<U>(rv::Current<records::BattleEnemy>()->x) + static_cast<U>(rv::Current<records::BattleEnemy>()->step_x);
    rv::Current<records::BattleEnemy>()->z = static_cast<U>(rv::Current<records::BattleEnemy>()->z) + static_cast<U>(rv::Current<records::BattleEnemy>()->step_z);
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x436C90 (PSX 0x801E4534, table-anchored): EnemyOp_Act3Subs 2:
// +0xA down by one; at 0 the hit's number - Battle_SetDamagePopup(0x939AD8's
// u16 +0x108, +5) with its +0x10C bit 4, else a kind-0 task of effect 1
// (BattleTask_Create(0, 1)) owned by Sprite_Current, +7 = 3, +0x27 = 0 - and
// +3 on; the place moved back by the velocity; BattleEnemy_ScriptTick
// (answer unused).
//
// As the original has it: BattleTask_Create's 0xFF is not tested (ours
// aborts).
extern "C" void __cdecl EnemyOp_KnockReturn(void) {
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    unsigned char* s = Sprite_Current;
    records::BattleEnemy* cur_enemy = rv::As<records::BattleEnemy>(s);
    if (s[0xA] == 0) {
        auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
        if ((enemy_rec->shown & 0x10) != 0) {
            BH_CALL(Battle_SetDamagePopup)(static_cast<std::uint16_t>(enemy_rec->hp_change), cur_enemy->actor);
            s = Sprite_Current;
            cur_enemy = rv::As<records::BattleEnemy>(s);
        } else {
            const unsigned slot = BH_CALL(BattleTask_Create)(0, 1);
            unsigned char* const t = NewTask("EnemyOp_KnockReturn", slot);
            auto* const task_rec = rv::As<records::BattleTask>(t);
            s = Sprite_Current;
            cur_enemy = rv::As<records::BattleEnemy>(s);
            task_rec->owner = Key(s);
            t[7] = 3;
            task_rec->clut = 0;
        }
        cur_enemy->state3 = static_cast<unsigned char>(cur_enemy->state3 + 1);
        s = Sprite_Current;
        cur_enemy = rv::As<records::BattleEnemy>(s);
    }
    cur_enemy->x = static_cast<U>(cur_enemy->x) - static_cast<U>(cur_enemy->step_x);
    s = Sprite_Current;
    cur_enemy = rv::As<records::BattleEnemy>(s);
    cur_enemy->z = static_cast<U>(cur_enemy->z) - static_cast<U>(cur_enemy->step_z);
    BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x436D50 (PSX 0x801E4644, table-anchored): EnemyOp_Act3Subs 3:
// once BattleEnemy_ScriptTickOnce answers its end, +3 on and animation 8
// (0x939AD8's +0x110 bit 1) or 0.
extern "C" void __cdecl EnemyOp_KnockPose(void) {
    if (BH_CALL(BattleEnemy_ScriptTickOnce)() == 0) return;
    ++rv::Current<records::BattleEnemy>()->state3;
    BH_CALL(BattleEnemy_SetAnimation)((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
}

// original 0x436D90 (PSX 0x801E46B8): the death - through EnemyOp_DeathSubs
// 0x64B234 by +3 (0x436DB0, 0x436E00, 0x436E40, 0x436EC0).
extern "C" void __cdecl EnemyOp_DeathDispatch(void) { EntryEnemyAiOps(::bof3::Img(EnemyOp_DeathSubs_addr), rv::As<records::BattleEnemy>(Sc())->state3)(); }

// original 0x436DB0 (PSX 0x801E46FC): the swell's start. Sprite_Current
// +0x48 = 2, the scales +0x44 / +0x40 = 0x10000, the speed +0xC = 0, +0x18 =
// 0x666; +3 = 1.
extern "C" void __cdecl EnemyOp_DeathSwellStart(void) {
    Sc()[0x48] = 2;
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y, 0x10000);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_x, 0x10000);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_18, 0x666);
    rv::As<records::BattleEnemy>(Sc())->state3 = 1;
}

// original 0x436E00 (PSX 0x801E4738): the swell, a frame - EnemyOp_ScaleInStep
// to 0x14000 (1.25) and without the tick; at the end +3 = 2.
extern "C" void __cdecl EnemyOp_DeathSwell(void) {
    unsigned char* const s = Sc();
    auto* const enemy_s = rv::As<records::BattleEnemy>(s);
    const std::uint32_t scale = static_cast<std::uint32_t>(enemy_s->scale_x);
    if (static_cast<std::int32_t>(scale) < 0x14000) {
        rv::Store<std::uint32_t>(enemy_s->scale_x, static_cast<std::uint32_t>(enemy_s->step_x) + scale);
        rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x));
        rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_x) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_18));
    } else {
        enemy_s->state3 = 2;
    }
}

// original 0x436E40 (PSX 0x801E4794): the flash. Sound_PlayEffect(0x204); in
// an event battle the cue 0x437450(the word +6 at the enemy's +0xF8);
// Sprite_ReleaseTint(Sprite_Current); Sprite_SetTint(Sprite_Current, 0xFF,
// 0xFF, 0xFF, 0); the speed +0x10 = 0 and its step +0x1C = -0x2000; +3 = 3.
extern "C" void __cdecl EnemyOp_DeathFlash(void) {
    enemy_ai_ops::g.play_effect(0x204);
    if (EventBattle_Id != 0) enemy_ai_ops::g.play_cue(enemy_ai_ops::Word(PtrInEnemyAiOps(Enemy() + 0xF8) + 6));
    enemy_ai_ops::g.release_tint(Sc());
    enemy_ai_ops::g.set_tint(Sc(), 0xFF, 0xFF, 0xFF, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_z, 0);
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_1c, 0xFFFFE000u);
    rv::As<records::BattleEnemy>(Sc())->state3 = 3;
}

// original 0x436EC0 (PSX 0x801E482C): the squash, a frame. The scale +0x44
// += the speed +0x10, the speed += +0x1C; once the s32 scale is below 0, a
// tail jump to Battle_EnemyDefeated.
extern "C" void __cdecl EnemyOp_DeathSquash(void) {
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->scale_y) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_z));
    rv::Store<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_z, static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->step_z) + static_cast<std::uint32_t>(rv::As<records::BattleEnemy>(Sc())->work_1c));
    if (rv::As<records::BattleEnemy>(Sc())->scale_y < 0) enemy_ai_ops::g.defeated();
}

// original 0x436F00 (PSX 0x801E4880, table-anchored; hidden in 0x436B50's
// extent): EnemyOp_ActSubs 5 (and the kinds' ActSubs 5) - through
// EnemyOp_Act5Subs 0x64B244 by +3 (3 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_Act5Dispatch(unsigned long word) {
    return Dispatch("EnemyOp_Act5Dispatch", AddressOf(EnemyOp_Act5Subs), 3, 3, word);
}

// original 0x436F20 (PSX 0x801E48C4, table-anchored): EnemyOp_Act5Subs 0,
// the HP change: 0x939AD8's u16 HP +0xA4 against the s16 +0x108 - at or
// below it, HP 0 and the status +0x92 = 0x4000; else (a negative change
// with Sound_PlayEffect(0x206)) HP minus it, held at the maximum +0xB0; then
// with +0x10C bit 0 Battle_SetDamagePopup(+0x108, Sprite_Current +5);
// BattleEnemy_ScriptTick (answer unused); +3 on.
extern "C" void __cdecl EnemyOp_ApplyHpChange(void) {
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    const auto change = static_cast<std::int16_t>(static_cast<std::uint16_t>(enemy_rec->hp_change));
    const auto hp = static_cast<std::int32_t>(enemy_rec->hp);
    if (hp <= change) {
        enemy_rec->hp = 0;
        rv::As<records::BattleEnemy>(Enemy())->status = 0x4000;
        enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    } else {
        if (change < 0) {
            BH_CALL(Sound_PlayEffect)(0x206);
            enemy_rec = rv::As<records::BattleEnemy>(Enemy());
        }
        enemy_rec->hp = enemy_rec->hp - static_cast<std::uint16_t>(enemy_rec->hp_change);
        enemy_rec = rv::As<records::BattleEnemy>(Enemy());
        if (enemy_rec->hp > enemy_rec->max_hp) {
            enemy_rec->hp = enemy_rec->max_hp;
            enemy_rec = rv::As<records::BattleEnemy>(Enemy());
        }
    }
    if ((enemy_rec->shown & 1) != 0) BH_CALL(Battle_SetDamagePopup)(static_cast<std::uint16_t>(enemy_rec->hp_change), rv::Current<records::BattleEnemy>()->actor);
    BH_CALL(BattleEnemy_ScriptTick)();
    ++rv::Current<records::BattleEnemy>()->state3;
}

// original 0x436FD0 (PSX 0x801E49A8, table-anchored): EnemyOp_Act5Subs 2:
// with 0x939AD8's +0x93 bit 6 the steps +1..+3 = 6, 4, 0; else
// Battle_ClearActorBit(+5) and the steps 2, 0, 0.
extern "C" void __cdecl EnemyOp_Act5End(void) {
    const unsigned char* const e = Enemy();
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    if ((e[0x93] & 0x40) != 0) {
        cur_enemy->state = 6;
        rv::Current<records::BattleEnemy>()->step = 4;
        rv::Current<records::BattleEnemy>()->state3 = 0;
        return;
    }
    BH_CALL(Battle_ClearActorBit)(cur_enemy->actor);
    rv::Current<records::BattleEnemy>()->state = 2;
    rv::Current<records::BattleEnemy>()->step = 0;
    rv::Current<records::BattleEnemy>()->state3 = 0;
}

// original 0x437030 (PSX 0x801E4A40): EnemyOp_Steps[7] - jmp [EnemyOp_CastSubs + 4 * +2] (3:
// EnemyOp_CastStart, EnemyOp_CastCue, BossMyria_State7End).
extern "C" unsigned __cdecl EnemyOp_CastDispatch(unsigned passed) {
    return DispatchBattleE3("EnemyOp_CastDispatch", AddressOf(EnemyOp_CastSubs), 3, 2, passed);
}

// original 0x437050: the cast's step 0 - nothing (eax 0) until File_LoadDone
// answers (its whole eax tested). Then by 0x939AD8's +0x105: 4 and an event
// battle (0x904AAA) - Battle_LoadSoundByKey(its kind +0x100 + 0x20, the set
// 0x904B8D): +9 = 0 when it answers, 0x1E when not; 4 outside one, +9 kept;
// not 4, +9 = 0. Then by the ability 0x904B80: its record byte +5 bit 3 - +1 up
// by one (straight to state 8); else +9 = the enemy data record's +0x8B by
// 0x939AD8's +0xF0 (read again), BattleEnemy_SetAnimation(6), +2 up by one.
// Both end in a tail jump to BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl EnemyOp_CastStart(void) {
    if (BH_CALL(File_LoadDone)() == 0) return 0;
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->command_kind == 4) {
        if (EventBattle_Id != 0) {
            const unsigned key = static_cast<unsigned char>(enemy_rec->run_state + 0x20);
            const bool loaded = BH_CALL(Battle_LoadSoundByKey)(key, BattleE3_SoundSet) != 0;
            S()[9] = loaded ? 0 : 0x1E;
        }
    } else {
        S()[9] = 0;
    }
    if (AbilityByte(::bof3::Img(battle_e3::at::kAbilityFlags5.v)) & 8) {
        rv::As<records::BattleEnemy>(S())->states[0] += 1;
    } else {
        const unsigned record = rv::As<records::BattleEnemy>(Enemy())->data_id;
        S()[9] = move_script::At(::bof3::Img(battle_e3::at::kEnemyData8B.v) + record * battle_e3::at::kEnemyDataStride)[0];
        BH_CALL(BattleEnemy_SetAnimation)(6);
        rv::As<records::BattleEnemy>(S())->states[1] += 1;
    }
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x437120 (PSX 0x801E4BCC): the cast's step 1 - +9, when not 0, down by one; at 0 the
// cue: in an event battle Sound_PlayEffectUnlessNone(the word +2 of 0x939AD8's
// +0xF8 table), else Sound_PlayEffect(0x601 + 2 * its +0xF0), then +2 up by one
// (Sprite_Current read again). Both end in a tail jump to
// BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl EnemyOp_CastCue(void) {
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[9] -= 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    const bool fight = EventBattle_Id != 0;
    unsigned char* const e = Enemy();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    if (fight)
        BH_CALL(Sound_PlayEffectUnlessNone)(move_script::Word(PtrAt(Key(e + 0xF8)) + 2));
    else
        BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x601 + 2 * enemy_rec->data_id));
    rv::As<records::BattleEnemy>(S())->step += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x437180 (PSX 0x801E4CDC): EnemyOp_Steps[8] - call [EnemyOp_CastDoneSubs + 4 * +2]
// (3: BossMyria_State8Tick, EnemyOp_CastDoneCost, EnemyOp_CastDoneTickUnless),
// then a tail jump to EnemyOp_CastDoneCheck.
extern "C" void __cdecl EnemyOp_CastDoneDispatch(void) {
    const unsigned state = CheckedState("EnemyOp_CastDoneDispatch", AddressOf(EnemyOp_CastDoneSubs), 3, 2);
    reinterpret_cast<boss_harness::Handler>(static_cast<std::uintptr_t>(EntryBattleE3(AddressOf(EnemyOp_CastDoneSubs), state)))();
    BH_CALL(EnemyOp_CastDoneCheck)();
}

// original 0x4371A0: the cast's end, step 1 - with 0x939AD8's +0x105 at 4, its
// word +0xA6 down by the byte 0x904B88 (0x939AD8 read again); 0x904AA8 |=
// 0x800; that object's +0x105 = 0; unless the ability's word +4 has bit 11,
// BattleEnemy_ScriptTickOnce; +2 up by one. (eax on return is Sprite_Current:
// nobody reads it - EnemyOp_CastDoneDispatch drops it.)
extern "C" void __cdecl EnemyOp_CastDoneCost(void) {
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->command_kind == 4) {
        enemy_rec->ap = enemy_rec->ap - BattleE3_Cost;
        enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    }
    move_script::SetWord(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v)), move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) | 0x800);
    enemy_rec->command_kind = 0;
    if ((AbilityWord4() & 0x800) == 0) BH_CALL(BattleEnemy_ScriptTickOnce)();
    rv::As<records::BattleEnemy>(S())->step += 1;
}

// original 0x437200: the cast's end, step 2 - unless the ability's byte +5 has
// bit 3, BattleEnemy_ScriptTickOnce; then +9 up by one (each frame).
extern "C" void __cdecl EnemyOp_CastDoneTickUnless(void) {
    if ((AbilityByte(::bof3::Img(battle_e3::at::kAbilityFlags5.v)) & 8) == 0) BH_CALL(BattleEnemy_ScriptTickOnce)();
    S()[9] += 1;
}

// original 0x437230 (in no start list; after 0x437200's padding, reached only
// by 0x437180's tail jump): with 0x904AA8 bit 2, a tail jump to
// EnemyOp_EndAction.
extern "C" void __cdecl EnemyOp_CastDoneCheck(void) {
    if (B(::bof3::Img(battle_e3::at::kRoundFlags.v)) & 4) BH_CALL(EnemyOp_EndAction)();
}

// original 0x437240 (PSX 0x801E4EA4): EnemyOp_Steps[9] - jmp [EnemyOp_LeaveSubs + 4 * +2] (3:
// EnemyOp_LeaveStart, EnemyOp_LeaveStep, EnemyOp_LeaveEnd).
extern "C" unsigned __cdecl EnemyOp_LeaveDispatch(unsigned passed) {
    return DispatchBattleE3("EnemyOp_LeaveDispatch", AddressOf(EnemyOp_LeaveSubs), 3, 2, passed);
}

// original 0x437260 (PSX 0x801E4EE8): the leave's step 0 - the velocity +0x18 = -0x1000, +0x1C =
// 0, turned by the pose (0x4467C0, BE4's); the pose's bit 1 flipped; +0xC /
// +0x10 = 0; +9 = 0x10, +0xA = 8; BattleEnemy_SetAnimation(0); the tint
// Sprite_SetTint(Sprite_Current, -4, -4, -4, 1); +0 |= 0x20; +0x5C = 2, the
// colour +0x5D..+0x5F = 0; Sound_PlayEffect(0x102); BattleEnemy_ScriptTick; +2
// up by one. Sprite_Current read for every store, as the original.
extern "C" void __cdecl EnemyOp_LeaveStart(void) {
    SetULong(S() + 0x18, 0xFFFFF000u);
    SetULong(S() + 0x1C, 0);
    BH_AT(void (__cdecl*)(unsigned char*), battle_e3::at::kTurnVelocity18.v)(S());
    rv::As<records::BattleEnemy>(S())->layout ^= 2;
    SetULong(S() + 0xC, 0);
    SetULong(S() + 0x10, 0);
    S()[9] = 0x10;
    S()[0xA] = 8;
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(Sprite_SetTint)(S(), 0xFC, 0xFC, 0xFC, 1);
    rv::As<records::BattleEnemy>(S())->flags |= 0x20;
    S()[0x5C] = 2;
    S()[0x5D] = 0;
    S()[0x5E] = 0;
    S()[0x5F] = 0;
    BH_CALL(Sound_PlayEffect)(0x102);
    BH_CALL(BattleEnemy_ScriptTick)();
    rv::As<records::BattleEnemy>(S())->step += 1;
}

// original 0x437320 (PSX 0x801E5000): the leave's step 1 - +9, when not 0, down by one (a wait);
// at 0, +0xA down by one; at 0, +2 up by one; else the colour +0x5D..+0x5F each
// down by 0x10, the velocity +0xC / +0x10 up by +0x18 / +0x1C, the position
// +0x34 / +0x38 up by it, and a tail jump to BattleEnemy_ScriptTick. (eax on
// return is dropped by BattleEnemy_RunAll's dispatchers.)
extern "C" void __cdecl EnemyOp_LeaveStep(void) {
    unsigned char* const s = S();
    auto* const enemy_s = rv::As<records::BattleEnemy>(s);
    if (s[9] != 0) {
        s[9] -= 1;
        return;
    }
    s[0xA] -= 1;
    if (s[0xA] == 0) {
        enemy_s->step += 1;
        return;
    }
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    SetULong(s + 0xC, ULong(s + 0xC) + ULong(s + 0x18));
    SetULong(s + 0x10, ULong(s + 0x10) + ULong(s + 0x1C));
    MoveBy(s, false);
    BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x4373C0 (PSX 0x801E511C): the leave's step 2 (also BossNina_WalkSteps[2]) -
// Battle_RemoveFromTurnOrder(+5); the enemies left 0x904AB3 down by one, and at
// 0 0x904AE8 |= 2 (the win); Sprite_ReleaseTint(Sprite_Current);
// Battle_ClearActorBit(+5); window record 4's +3 (0x8031F3) = 2;
// BattleBanner_ClearAll; a tail jump to Effect_Release. No Battle_EnemyDefeated:
// the enemy leaves the count without being defeated.
extern "C" void __cdecl EnemyOp_LeaveEnd(void) {
    BH_CALL(Battle_RemoveFromTurnOrder)(rv::As<records::BattleEnemy>(S())->actor);
    const auto left = static_cast<unsigned char>(Battle_EnemiesLeft - 1);
    Battle_EnemiesLeft = left;
    if (left == 0) Battle_EndFlags |= 2;
    BH_CALL(Sprite_ReleaseTint)(S());
    BH_CALL(Battle_ClearActorBit)(rv::As<records::BattleEnemy>(S())->actor);
    WindowRecords[4].state = 2;
    BH_CALL(BattleBanner_ClearAll)();
    BH_CALL(Effect_Release)();
}

// original 0x437420 (PSX 0x801E51C8): step 11. Animation 4; then with bit 4
// of the enemy's +0x110 a tail jump to BattleEnemy_ScriptTick, else with
// bit 5 to BattleEnemy_ScriptTickOnce, else nothing.
//
// As the original has it: 0x939AD8 is read after the call.
extern "C" void __cdecl EnemyOp_HitPose(void) {
    enemy_ai_ops::g.set_animation(4);
    const std::uint32_t flags = rv::As<records::BattleEnemy>(Enemy())->flags_110;
    if ((flags & 0x10) != 0) enemy_ai_ops::g.script_tick();
    else if ((flags & 0x20) != 0) enemy_ai_ops::g.script_tick_once();
}

// original 0x437450 (sound; PSX 0x801E5230): Sound_PlayEffect(sound) unless its low word is
// 0xFFFF (the whole dword handed on; Sound_PlayEffect reads the word). An
// enemy state's or an actor's cue (twelve callers, eleven of them ours).
extern "C" void __cdecl Sound_PlayEffectUnlessNone(unsigned sound) {
    if ((sound & 0xFFFF) == 0xFFFF) return;
    BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(sound));
}

// original 0x437470 (PSX Battle_EnemyDefeated 0x801E542C, the same order):
// Sprite_Current's byte +0 |= 0x40; the EXP total 0x904AEC += 0x939AD8's u16
// +0x96, the zenny total 0x904AF0 += its u16 +0x94, which is zeroed; its
// status +0x93 |= 0x40 (0x4000 of the word +0x92, dead);
// Battle_RemoveFromTurnOrder(Sprite_Current +5); Battle_RollDrops; the
// enemies left 0x904AB3 less one, and 0x904AE8 |= 2 at none; with round flag
// 0x4000 set and 0x904B8A equal to the actor, that flag cleared; the enemy's
// flag bit +0x8C set (0x494ED0); Sprite_ReleaseTint(Sprite_Current); bits
// 0x70 of +0x110 cleared; 0x446FD0(actor) clears the actor's bit of 0x904B82;
// Sprite_Current's state bytes +1..+3 to 2, 0, 0.
//
// As the original has it: Sprite_Current and 0x939AD8 are read afresh after
// every call; the actor byte goes to the callees zero-extended (they read a
// byte, or a word for 0x494ED0's flag number).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_EnemyDefeated(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
    unsigned char* e = Enemy();
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(e);
    // DIV-0045: the yields times the launcher's multipliers, 1 unless set
    // (src/game/cheats.cpp); the enemy's record keeps its own numbers.
    Battle_ExpTotal = static_cast<std::int32_t>(Battle_ExpTotal + enemy_rec->exp * Cheats_ExpMultiplier());
    BattleFlow_ZennyTotal = static_cast<std::int32_t>(BattleFlow_ZennyTotal + enemy_rec->zenny * Cheats_ZennyMultiplier());
    enemy_rec->zenny = 0;
    e = Enemy();
    enemy_rec = rv::As<records::BattleEnemy>(e);
    e[0x93] = static_cast<unsigned char>(e[0x93] | 0x40);
    battle_flow::g.remove_from_turn_order(Sprite_Current[5]);
    battle_flow::g.roll_drops();
    const auto left = static_cast<unsigned char>(Battle_EnemiesLeft - 1);
    Battle_EnemiesLeft = left;
    if (left == 0) Battle_EndFlags = static_cast<unsigned char>(Battle_EndFlags | 2);
    if ((battle_flow::Long(battle_flow::At(::bof3::Img(battle_flow::at::kRoundFlags.v))) & 0x4000) != 0 && BattleFlow_ActorAt == Sprite_Current[5])
        battle_flow::SetWord(battle_flow::At(::bof3::Img(battle_flow::at::kRoundFlags.v)), battle_flow::Word(battle_flow::At(::bof3::Img(battle_flow::at::kRoundFlags.v))) & 0xBFFF);
    battle_flow::g.set_flag_bit(rv::As<records::BattleEnemy>(Enemy())->enemy_kind);
    battle_flow::g.release_tint(Sprite_Current);
    e = Enemy();
    enemy_rec = rv::As<records::BattleEnemy>(e);
    enemy_rec->flags_110 = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(enemy_rec->flags_110)) & 0xFFFFFF8Fu);
    battle_flow::g.clear_turn_bit(Sprite_Current[5]);
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x437580 (PSX Battle_RollDrops 0x801E525C): for each of 0x939AD8's
// two drops - an item u16 at +0xA8 / +0xAC and its class byte at +0xAA /
// +0xAE, both non-zero - a drop when (Rand() & 0xFF) is at most
// {0, 0, 1, 3, 7, 0x1F, 0x7F, 0xFF}[class] (a table on the stack). A drop is
// looked for in the list 0x904AF4 (u16 items, counts at 0x904B14, 0x904AE7
// entries): each entry equal to it counts one more and zeroes the enemy's
// item word. Then - found or not - the enemy's word as it now stands is
// appended with count 1 (the entry count grows while below 15) and the item
// word zeroed.
//
// Not the PSX's: there a found drop is counted and the loop left; the port
// runs on and appends the zeroed word as an item 0 with count 1, and the
// entries after the match are compared with 0 - defect, docs/battle_flow.md
// section 5. Kept, as every latent defect is.
//
// As the original has it: 0x939AD8 is read afresh after Rand, after a match
// and after the append; the class is read after Rand; the entry count is
// read before the search, again after each match, and again for the count
// byte's index and for the increment. A class above 7 would read the
// original's saved registers and caller's stack; ours aborts (rule 4).
extern "C" void __cdecl Battle_RollDrops(void) {
    static constexpr unsigned char kChance[8] = {0, 0, 1, 3, 7, 0x1F, 0x7F, 0xFF};
    unsigned char* e = Enemy();
    for (unsigned slot = 0xA8; slot < 0xB0; slot += 4) {
        if (battle_flow::Word(e + slot) == 0) continue;
        if (e[slot + 2] == 0) continue;
        const unsigned roll = static_cast<unsigned>(battle_flow::g.crt_rand()) & 0xFF;
        e = Enemy();
        const unsigned cls = e[slot + 2];
        if (cls >= 8) bof3::Fatal("Battle_RollDrops: drop class %u, past the eight-entry table", cls);
        if (kChance[cls] < roll) continue;
        unsigned count = Battle_DropCount;
        if (count != 0) {
            unsigned i = 0;
            do {
                if (battle_flow::Word(battle_flow::At(::bof3::Img(battle_flow::at::kDropItems.v) + i * 2)) == battle_flow::Word(e + slot)) {
                    battle_flow::At(::bof3::Img(battle_flow::at::kDropCounts.v))[i] = static_cast<unsigned char>(battle_flow::At(::bof3::Img(battle_flow::at::kDropCounts.v))[i] + 1);
                    battle_flow::SetWord(e + slot, 0);
                    e = Enemy();
                    count = Battle_DropCount;
                }
                i = (i + 1) & 0xFF;
            } while (i < count);
        }
        battle_flow::SetWord(battle_flow::At(::bof3::Img(battle_flow::at::kDropItems.v) + count * 2), battle_flow::Word(e + slot));
        battle_flow::At(::bof3::Img(battle_flow::at::kDropCounts.v))[Battle_DropCount] = 1;
        const unsigned char entries = Battle_DropCount;
        if (entries < 0xF) Battle_DropCount = static_cast<unsigned char>(entries + 1);
        battle_flow::SetWord(e + slot, 0);
        e = Enemy();
    }
}

// original 0x4376A0: the end of an enemy's action - Sprite_Current +1 = 2, +2 =
// 0; Battle_ClearActorBit(+5); 0x939AD8's +0x110 &= ~0x200; its +0x105 = 0
// unless 0x904AA8 bit 6.
extern "C" void __cdecl EnemyOp_EndAction(void) {
    rv::As<records::BattleEnemy>(S())->state = 2;
    rv::As<records::BattleEnemy>(S())->step = 0;
    BH_CALL(Battle_ClearActorBit)(rv::As<records::BattleEnemy>(S())->actor);
    unsigned char* const e = Enemy();
    SetULong(e + 0x110, ULong(e + 0x110) & ~0x200u);
    if ((B(::bof3::Img(battle_e3::at::kRoundFlags.v)) & 0x40) == 0) rv::As<records::BattleEnemy>(Enemy())->command_kind = 0;
}

// original 0x4376F0: with 0x939AD8's +0x90 bit 3 and Rand's bit 0, 0x904AA8 |=
// 0x80 and BattleTask_Create(0, 2) (its slot not used) - the enemy side of
// BattleObj_SwingEnd's bit 7 and task.
extern "C" void __cdecl EnemyOp_RollBit80Task(void) {
    if ((Enemy()[0x90] & 8) == 0) return;
    if ((BH_CALL(Rand)() & 1) == 0) return;
    B(::bof3::Img(battle_e3::at::kRoundFlags.v)) |= 0x80;
    BH_CALL(BattleTask_Create)(0, 2);
}

// original 0x437720 (no PSX twin paired): stored at 0x904B64 by the enemy
// state 0x436110 (group CF's) and called by 0x431320 (group CC's). With bit 0
// of 0x904AE8 and the previous area (0x802290) 0xBD: MoveScript_Var7 = 1,
// the byte after it 0x19, ScriptFlags_Set40, then 0x446E20 (tail-jumped to,
// unread). The name is from what it tests; what area 0xBD is was not looked
// up.
//
// As the original has it: the flag byte is read once, before the area word.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleHook_Area189Script(void) {
    if ((Battle_EndFlags & 1) == 0) return;
    if (AreaW2c_LastArea != 0xBD) return;
    MoveScript_Var7 = 1;
    battle_fx_tasks::At(::bof3::Img(MoveScript_Var7_addr) + 1)[0] = 0x19;
    battle_fx_tasks::g.script_flags_set40();
    battle_fx_tasks::g.after_area_script();
}

// original 0x437750 (no PSX twin paired): stored at 0x904B68 by the same
// state and called by 0x4317B0 (group CC's). With the same two conditions:
// Transition_Start(4), then byte 0x904AA2 + 1 (read after the call).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleHook_Area189Transition(void) {
    if ((Battle_EndFlags & 1) == 0) return;
    if (AreaW2c_LastArea != 0xBD) return;
    battle_fx_tasks::g.transition_start(4);
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

void EnemyOp_Inject() {
    BOF3_INJECT(BattleEnemy_RunAll);
    BOF3_INJECT(BattleEnemy_UpdateScreenAll);
    BOF3_INJECT(BattleEnemy_SetAnimation);
    BOF3_INJECT(BattleEnemy_SetAnimationAs);
    BOF3_INJECT(BattleEnemy_SetAnimationOf);
    BOF3_INJECT(BattleEnemy_PickAction);
    BOF3_INJECT(BattleEnemy_PickAnyTarget);
    BOF3_INJECT(Battle_RandomEnemy);
    BOF3_INJECT(BattleEnemy_PickTarget);
    BOF3_INJECT(Battle_EnemyLowestHp);
    BOF3_INJECT(BattleEnemy_OtherOrMember);
    BOF3_INJECT(Battle_MemberLowestHp);
    BOF3_INJECT(Battle_RandomMember);
    BOF3_INJECT(BattleEnemy_OtherEnemy);
    BOF3_INJECT(BattleEnemy_ScriptTick);
    BOF3_INJECT(BattleEnemy_ScriptTickOnce);
    BOF3_INJECT(EnemyOp_StepDispatch);
    BOF3_INJECT(EnemyOp_Begin);
    BOF3_INJECT(EnemyOp_EnterDispatch);
    BOF3_INJECT(EnemyOp_ScaleInDispatch);
    BOF3_INJECT(EnemyOp_ScaleInStart);
    BOF3_INJECT(EnemyOp_ScaleInStep);
    BOF3_INJECT(EnemyOp_SlideDispatch);
    BOF3_INJECT(EnemyOp_SlideStart);
    BOF3_INJECT(EnemyOp_SlideStep);
    BOF3_INJECT(EnemyOp_Idle);
    BOF3_INJECT(EnemyOp_Wait);
    BOF3_INJECT(EnemyOp_HighlightOn);
    BOF3_INJECT(EnemyOp_HighlightPulse);
    BOF3_INJECT(EnemyOp_TurnStart);
    BOF3_INJECT(EnemyOp_CueDispatch);
    BOF3_INJECT(EnemyOp_PlayCreatureCue);
    BOF3_INJECT(EnemyOp_CueEnd);
    BOF3_INJECT(EnemyOp_ActDispatch);
    BOF3_INJECT(EnemyOp_ActBegin);
    BOF3_INJECT(EnemyOp_HitDispatch);
    BOF3_INJECT(EnemyOp_ReceiveAction);
    BOF3_INJECT(EnemyOp_WaitAnimOnce);
    BOF3_INJECT(EnemyOp_HitEnd);
    BOF3_INJECT(BattleEnemy_Chance70);
    BOF3_INJECT(EnemyOp_Act3Dispatch);
    BOF3_INJECT(EnemyOp_KnockStart);
    BOF3_INJECT(EnemyOp_KnockBack);
    BOF3_INJECT(EnemyOp_KnockReturn);
    BOF3_INJECT(EnemyOp_KnockPose);
    BOF3_INJECT(EnemyOp_DeathDispatch);
    BOF3_INJECT(EnemyOp_DeathSwellStart);
    BOF3_INJECT(EnemyOp_DeathSwell);
    BOF3_INJECT(EnemyOp_DeathFlash);
    BOF3_INJECT(EnemyOp_DeathSquash);
    BOF3_INJECT(EnemyOp_Act5Dispatch);
    BOF3_INJECT(EnemyOp_ApplyHpChange);
    BOF3_INJECT(EnemyOp_Act5End);
    BOF3_INJECT(EnemyOp_CastDispatch);
    BOF3_INJECT(EnemyOp_CastStart);
    BOF3_INJECT(EnemyOp_CastCue);
    BOF3_INJECT(EnemyOp_CastDoneDispatch);
    BOF3_INJECT(EnemyOp_CastDoneCost);
    BOF3_INJECT(EnemyOp_CastDoneTickUnless);
    BOF3_INJECT(EnemyOp_CastDoneCheck);
    BOF3_INJECT(EnemyOp_LeaveDispatch);
    BOF3_INJECT(EnemyOp_LeaveStart);
    BOF3_INJECT(EnemyOp_LeaveStep);
    BOF3_INJECT(EnemyOp_LeaveEnd);
    BOF3_INJECT(EnemyOp_HitPose);
    BOF3_INJECT(Sound_PlayEffectUnlessNone);
    BOF3_INJECT(Battle_EnemyDefeated);
    BOF3_INJECT(Battle_RollDrops);
    BOF3_INJECT(EnemyOp_EndAction);
    BOF3_INJECT(EnemyOp_RollBit80Task);
    BOF3_INJECT(BattleHook_Area189Script);
    BOF3_INJECT(BattleHook_Area189Transition);
}
