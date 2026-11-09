// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sb.cpp, boss_sc.cpp, boss_sd.cpp, boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_nina.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_nina_callees.h"
#include "game/boss_h.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sb.h"
#include "game/boss_sb_callees.h"
#include "game/boss_sc.h"
#include "game/boss_sc_callees.h"
#include "game/boss_sd.h"
#include "game/boss_sd_callees.h"
#include "game/boss_se.h"
#include "game/boss_se_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_nina::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

using boss_harness::Handler;

void Setup(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

using move_script::SetWord;

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

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

// BossActor_Find(tag) for a hook that writes through the answer: the original
// uses it untested and faults near address 0 when no field actor carries the
// tag. Unreachable after the same tag's BossActor_ClearBit40 (ours aborts
// there first); ours aborts here too rather than write through null.
unsigned char* Actor(const char* who, unsigned tag) {
    unsigned char* const a = BH_CALL(BossActor_Find)(tag);
    if (a == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through a null pointer (docs/boss_sc.md "
                    "section 7)",
                    who, tag);
    return a;
}

// Fights 13 and 16's line: the enemy's action byte, the window pass byte 2,
// then Msg_OpenScript(line).
void Line(unsigned char action, unsigned short line) {
    B(::bof3::Img(at::kAction.v)) = action;
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(line);
}

// The banner with a character's name (fight 16's event hook and Nina's hit
// hook share it): Battle_OpenMsgWindow, Str_CopyN(Text_Records, the live
// record 0x66972D names, 8), BattleBanner_Add(2, 0, 0, 0x2D,
// Msg_SystemPtr(0x25)).
void NameBanner() {
    BH_CALL(Battle_OpenMsgWindow)();
    const unsigned n = Character_IndexById[1];
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records))),
                       reinterpret_cast<const char*>(At(::bof3::Img(CharacterRecords_addr) + n * at::kCharStride)), 8);
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(0x25);
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
}

// The end hooks' common shape: on the win (0x904AE8 bit 1) the move-script
// counter byte 0x903848 = scene and the end phase's step 1, else step 2.
void EndByWin(unsigned char scene) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kMoveCounter.v)) = scene;
        BH_AT(Handler, boss_sc::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sc::at::kEndOther.v)();
}

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
void SetCell(unsigned char* p, U value) { SetLong(p, static_cast<std::int32_t>(value)); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's code -
// reads that word).
using Forward = void (__cdecl*)(unsigned);

Forward EntryBossSd(U table, unsigned index) { return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index))))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
void DispatchBossSd(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sd.md section 6)",
                    who, at, state, entries, (unsigned)table);
    EntryBossSd(table, state)(passed);   // as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: jmp [table + 4 * (code & 0xFF)], three entries, a
// Fatal past them; the entry gets the hook's word.
void HookTable(const char* who, U table, unsigned code) {
    const unsigned index = code & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook code %u, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sd.md section 6)",
                    who, index, (unsigned)table);
    EntryBossSd(table, index)(code);
}

// The entrance's three stores into 0x939AD8's object (read again for each):
// +0xFC, +0xF4 (the hook), +0xF8.
void PointEnemy(U fc, U hook, U f8) {
    SetCell(Enemy() + 0xFC, fc);
    SetCell(Enemy() + 0xF4, hook);
    SetCell(Enemy() + 0xF8, f8);
}

// The three set-ups' stores.
void StoreHooksBossSd(U end, U exit, U event) {
    BattleHook_End = end;
    BattleHook_Exit = exit;
    BattleHook_Event = event;
}

// Kinds 21..23's entrance: with chapter flag `flag` set, 0x939AD8's +0xA4 and
// +0xA6 (the object read again for each) from the words set-ups 18..20 kept;
// without, the flag set. Then the three stores.
void RestoreOrMark(unsigned flag, U fc, U hook, U f8) {
    if (BH_CALL(Flags_Test)(At(Chapter_FlagRow), flag) & 0xFF) {
        rv::Store<unsigned>(rv::As<records::BattleEnemy>(Enemy())->hp, BossSd_KeptA4);
        rv::Store<unsigned>(rv::As<records::BattleEnemy>(Enemy())->ap, BossSd_KeptA6);
    } else {
        BH_CALL(Flags_Set)(At(Chapter_FlagRow), flag);
    }
    PointEnemy(fc, hook, f8);
}

// The end phase's steps (nobody's; the harness's standard set).
void EndStep(U step) { BH_WORD(Handler, step)(); }

// Set-ups 18..20's end hooks: the win hands the chapter the step `win_step`
// (by 0x904AAD bit 1: `step_if_1` or `step_if_not`) and takes step 1; enemy 2
// fallen (0x904AAD bit 0) takes step 2; otherwise flag 0x23 and 0x24 of the
// chapter's bits: 0x23 and 0x24 step 2, 0x23 alone the chapter step 0x3A, not
// 0x23 the step 0x1C, each with Boss_SetByLeaderId (before the step's store,
// or after it for set-up 20: `pick_first`) and step 3. Then the four words
// kept: enemy 1's HP, enemy 2's, enemy 0's +0xA4 and +0xA6.
void EndAreaFight(unsigned char step_if_1, unsigned char step_if_not, bool pick_first) {
    const unsigned char fell = BossSe_Script;     // read once, before any call
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = fell & 2 ? step_if_1 : step_if_not;
        EndStep(boss_sd::at::kEndWin.v);
    } else if (fell & 1) {
        EndStep(boss_sd::at::kEndOther.v);
    } else {
        const bool f23 = BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x23) & 0xFF;
        if (f23 && (BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x24) & 0xFF)) {
            EndStep(boss_sd::at::kEndOther.v);
        } else {
            const unsigned char step = f23 ? 0x3A : 0x1C;
            if (pick_first) {
                BH_CALL(Boss_SetByLeaderId)();
                MoveScript_PartyRecords[0x65] = step;
            } else {
                MoveScript_PartyRecords[0x65] = step;
                BH_CALL(Boss_SetByLeaderId)();
            }
            EndStep(boss_sd::at::kEndThird.v);
        }
    }
    const unsigned hp1 = Battle_Enemies[1].hp;
    const unsigned hp2 = Battle_Enemies[2].hp;
    const unsigned a4 = Battle_Enemies[0].hp;
    BossSd_KeptHp1 = hp1;
    const unsigned a6 = Battle_Enemies[0].ap;
    BossSd_KeptHp2 = hp2;
    BossSd_KeptA4 = a4;
    BossSd_KeptA6 = a6;
}

// Set-ups 18..20's event hooks (the three are one body): phase 0 - enemy 0's
// +0x93 bit 0x40 sets the win bit 1 of 0x904AE8; enemy 1's sets 0x904AAD and
// 0x904AE8 bit 1; enemy 2's sets both bit 0; then with 0x904AE8 not 0,
// BossMap_UpdateFromEnemies. Phase 5: BossMap_UpdateFromEnemies. al 0.
unsigned char EventAreaFight(unsigned code) {
    const unsigned char phase = static_cast<unsigned char>(code);
    if (phase == 5) {
        BH_CALL(BossMap_UpdateFromEnemies)();
        return 0;
    }
    if (phase != 0) return 0;
    if (static_cast<U>(Long(At(::bof3::Img(Battle_Enemies_addr) + 0x92))) & 0x4000) Battle_EndFlags |= 2;
    if (BossH_Enemy1Status & 0x4000) {
        BossSe_Script |= 2;
        Battle_EndFlags |= 2;
    }
    if (BossH_Enemy2Status & 0x4000) {
        BossSe_Script |= 1;
        Battle_EndFlags |= 1;
    }
    if (Battle_EndFlags != 0) BH_CALL(BossMap_UpdateFromEnemies)();
    return 0;
}

// Set-ups 18..20's exit hooks: unless enemy 0's +0x93 has 0x40, the actor
// tagged `tag` takes enemy 0's place (BossActor_CopyFrom(tag, enemy 0, 1)) and
// loses bit 0x40; then actors 3 and 4 take enemies 1 and 2's poses (what 0)
// and lose bit 0x40.
void ExitAreaFight(unsigned tag) {
    if (!(static_cast<U>(Long(At(::bof3::Img(Battle_Enemies_addr) + 0x92))) & 0x4000)) {
        BH_CALL(BossActor_CopyFrom)(tag, At(::bof3::Img(Battle_Enemies_addr)), 1);
        BH_CALL(BossActor_ClearBit40)(tag);
    }
    BH_CALL(BossActor_CopyFrom)(3, At(::bof3::Img(Battle_Enemies_addr) + 0x128), 0);
    BH_CALL(BossActor_ClearBit40)(3);
    BH_CALL(BossActor_CopyFrom)(4, At(::bof3::Img(Battle_Enemies_addr) + 0x250), 0);
    BH_CALL(BossActor_ClearBit40)(4);
}

// Set-ups 17 and 21's end hooks: the win sets the field's move-script counter
// 0 (`counter`) and takes the end phase's step 1, else step 2.
void EndCounter(unsigned char counter) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kCounter0.v)) = counter;
        EndStep(boss_sd::at::kEndWin.v);
        return;
    }
    EndStep(boss_sd::at::kEndOther.v);
}

// The death of kinds 24 and 26's +2 tables: Sprite_Current's +0 &= 0xBF, +1
// = 3, +2 = 0, +3 = 0 (Sprite_Current read again for each).
void DeathStates() {
    Sprite_Current[0] &= 0xBF;
    Sprite_Current[1] = 3;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// The two ground words: enemy 0's +0x3E and the leader's, AreaMap_Elevation
// of their (+0x34, +0x38).
void GroundWords() {
    const long e = BH_CALL(AreaMap_Elevation)(Battle_Enemies[0].x, Battle_Enemies[0].z);
    Battle_Enemies[0].height_whole = static_cast<unsigned>(e) & 0xFFFF;
    const long l = BH_CALL(AreaMap_Elevation)(ObjTrio[0].x, ObjTrio[0].z);
    ObjTrio[0].height_whole = static_cast<unsigned>(l) & 0xFFFF;
}

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSe(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

}  // namespace

// original 0x439E00: BossKind_Table[17]: by +1 through BossNina_Steps (12).
extern "C" unsigned long __cdecl BossNina_Dispatch(unsigned long through) {
    return DispatchBossSc("BossNina_Dispatch", ::bof3::Img(0x64CD48), 12, 1, through);
}

// original 0x439E20: state 0: the tables (BossNina_Anims, BossNina_Hook,
// BossNina_F8), +8 = 1, +1 = 2.
extern "C" void __cdecl BossNina_Enter(void) {
    StoreKindTables(::bof3::Img(0x64CCA0), ::bof3::Code(bof3::addr::BossNina_Hook), ::bof3::Img(0x64CCC8));
    rv::Current<records::BattleEnemy>()->layout = 1;
    rv::Current<records::BattleEnemy>()->state = 2;
}

// original 0x439E70: BossNina_Steps 9: by +2 through BossNina_WalkSteps (3:
// BossNina_WalkStart, BossNina_WalkStep, 0x4373C0).
extern "C" unsigned long __cdecl BossNina_WalkDispatch(unsigned long through) {
    return DispatchBossSc("BossNina_WalkDispatch", ::bof3::Img(0x64CD78), 3, 2, through);
}

// original 0x439E90: Sprite_SetAnimationBank(0xF9); +0x24 &= 0xFE;
// Sprite_SetAnimation(2); +2 up by one - Sprite_Current read after each call.
extern "C" void __cdecl BossNina_WalkStart(void) {
    BH_CALL(Sprite_SetAnimationBank)(0xF9);
    Sprite_Current[0x24] &= 0xFE;
    BH_CALL(Sprite_SetAnimation)(2);
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x439EC0: with +0 bit 7, +2 up by one (eax the sprite); else the
// dword +0x34 up by 0x8000 and Sprite_ScriptTick twice, the second's answer
// the answer.
extern "C" unsigned __cdecl BossNina_WalkStep(void) {
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    if (cur_enemy->flags & 0x80) {
        cur_enemy->step += 1;
        return Low32(s);
    }
    cur_enemy->x = static_cast<std::int32_t>(static_cast<U>(cur_enemy->x) + 0x8000);
    BH_CALL(Sprite_ScriptTick)();
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x439EE0: the +0xF4 hook: through BossNina_Hooks (3:
// BossNina_HookAct, BossNina_HookHit, BareRet).
extern "C" unsigned long __cdecl BossNina_Hook(unsigned long word) { return HookDispatchBossSc("BossNina_Hook", ::bof3::Img(0x64CD84), word); }

// original 0x439EF0: hook entry 0: with the fight's bit 0, action 3; else
// while the wait count 0x675F00 is not 0, action 0 and the count down; at 0,
// action 1, the target 4, 0x904AE2 down by one and the fight's bit 0 set.
extern "C" void __cdecl BossNina_HookAct(unsigned) {
    if (BossSe_Script & 1) {
        B(::bof3::Img(at::kAction.v)) = 3;
        return;
    }
    const unsigned char wait = BossSc_WaitTurns;
    if (wait) {
        B(::bof3::Img(at::kAction.v)) = 0;
        BossSc_WaitTurns = static_cast<unsigned char>(wait - 1);
        return;
    }
    const unsigned char count = BossSe_RoundSlot;
    B(::bof3::Img(at::kAction.v)) = 1;
    B(::bof3::Img(boss_sc::at::kTarget.v)) = 4;
    BossSe_RoundSlot = static_cast<unsigned char>(count - 1);
    BossSe_Script = static_cast<unsigned char>(BossSe_Script | 1);
}

// original 0x439F50: hook entry 1: the name banner (NameBanner above); the
// wait count 0; Field_MemberSprite(1, 1); Sprite_Current +0x29 = 4;
// 0x939AD8's +0xFC = BossNina_Anims2; Sprite_SetAnimation(+8 + 4); the
// fight's bit 6 set.
extern "C" void __cdecl BossNina_HookHit(unsigned) {
    NameBanner();
    BossSc_WaitTurns = 0;
    BH_CALL(Field_MemberSprite)(1, 1);
    rv::Current<records::BattleEnemy>()->ot_slot = 4;
    rv::As<records::BattleEnemy>(Enemy())->anim_table = ::bof3::Img(0x64CCAC);
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->layout + 4));
    BossSe_Script = static_cast<unsigned char>(BossSe_Script | 0x40);
}

// original 0x439FE0: Boss_SetupTable[13]: End Boss13_End, Exit Boss07_Exit,
// Event BareRetZero.
extern "C" void __cdecl Boss13_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss13_End), ::bof3::Code(bof3::addr::Boss07_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x43A000: set-up 13's end hook: the movement script's variable 3
// = 0x32; the win (0x904AE8 bit 1, read before) - 0x446DE0; otherwise
// 0x446E20, then 0x92BF18 = 3.
extern "C" void __cdecl Boss13_End(void) {
    const unsigned char end = Battle_EndFlags;
    B(::bof3::Img(boss_sb::at::kScriptVar3.v)) = 0x32;
    if (end & 2) {
        BH_AT(Handler, boss_sb::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sb::at::kEndThird.v)();
    BossH_LeaderPick = 3;
}

// original 0x43A030: Boss_SetupTable[16]: End = Boss16_End, Exit = Boss16_Exit,
// Event = Boss16_Event; the wait count 0x675F00 = (Rand() & 2) + 8.
extern "C" void __cdecl Boss16_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::Boss16_End), ::bof3::Code(bof3::addr::Boss16_Exit), ::bof3::Code(bof3::addr::Boss16_Event));
    BossSc_WaitTurns = static_cast<unsigned char>((BH_CALL(Rand)() & 2) + 8);
}

// original 0x43A060: the event hook, al 0 on every path, by the phase code's
// low byte - 2 (BattleRoundEnd_NextRound): unless the fight's bit 6 or a
// wait count left, the name banner, the wait count 0, Sprite_Current and
// 0x939AD8 = enemy 2, Field_MemberSprite(1, 1), 0x939AD8's +0xFC =
// BossNina_Anims2, +0x29 = 4, Sprite_SetAnimation(+8 + 4), bit 6 set. 1: with
// the acting actor 0, the target's bit 6 and not the fight's bit 0, action 0
// and line 0x27. 0: with enemy 0's or 1's dword +0x92 bit 0x2000, the fight's
// bit 3 set.
extern "C" unsigned char __cdecl Boss16_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0:
        if ((L(At(::bof3::Img(Battle_Enemies_addr) + at::kEnemyStatus)) & 0x2000) || (L(At(::bof3::Img(Battle_Enemies_addr) + 0x128 + at::kEnemyStatus)) & 0x2000))
            BossSe_Script |= 8;
        return 0;
    case 1:
        if (B(::bof3::Img(boss_sc::at::kActor.v)) != 0 || !(B(::bof3::Img(boss_sc::at::kTarget.v)) & 0x40) || (BossSe_Script & 1)) return 0;
        Line(0, 0x27);
        return 0;
    case 2:
        if (BossSe_Script & 0x40) return 0;
        if (BossSc_WaitTurns != 0) return 0;
        NameBanner();
        BossSc_WaitTurns = 0;
        Sprite_Current = At(::bof3::Img(Battle_Enemies_addr) + 0x250);
        BattleE3_CurrentEnemy = &Battle_Enemies[2];
        BH_CALL(Field_MemberSprite)(1, 1);
        rv::As<records::BattleEnemy>(Enemy())->anim_table = ::bof3::Img(0x64CCAC);
        Sprite_Current[0x29] = 4;
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 4));
        BossSe_Script = static_cast<unsigned char>(BossSe_Script | 0x40);
        return 0;
    default:
        return 0;
    }
}

// original 0x43A190: the end hook. Won: 0x904AEC = enemy 1's word +0x96 +
// enemy 0's, 0x903848 = 0x14, 0x446DE0; else, with the fight's bit 6,
// Sprite_Current = enemy 2, Sprite_SetAnimationBank(0xF9), +0x24 &= 0xFE,
// +0x2A = 1, Sprite_SetAnimation(1); 0x903848 = 0x50, 0x446E20. Both ways
// then the word 0x9039A2 &= 0xFF7F and 0x904131 = 0x31.
//
// DIV-0045: the sum times the EXP multiplier (src/game/cheats.cpp; 1 unless
// BOF3X_EXP is set, so the fuzz below compares the original's sum). This is
// the second Balio and Sunder fight's whole EXP: their hit hooks put HP 0
// back to 1, so Battle_EnemyDefeated - where the multiplier otherwise
// applies - never adds theirs, and this store replaces whatever it did add.
extern "C" void __cdecl Boss16_End(void) {
    if (Battle_EndFlags & 2) {
        const U sum = (static_cast<std::uint32_t>(Battle_Enemies[1].exp) + Battle_Enemies[0].exp) * Cheats_ExpMultiplier();
        B(::bof3::Img(at::kMoveCounter.v)) = 0x14;
        Battle_ExpTotal = static_cast<std::int32_t>(sum);
        BH_AT(Handler, boss_sc::at::kEndWin.v)();
    } else {
        if (BossSe_Script & 0x40) {
            Sprite_Current = At(::bof3::Img(Battle_Enemies_addr) + 0x250);
            BH_CALL(Sprite_SetAnimationBank)(0xF9);
            Sprite_Current[0x24] &= 0xFE;
            Sprite_Current[0x2A] = 1;
            BH_CALL(Sprite_SetAnimation)(1);
        }
        B(::bof3::Img(at::kMoveCounter.v)) = 0x50;
        BH_AT(Handler, boss_sc::at::kEndThird.v)();
    }
    Field_ScriptFlags = Field_ScriptFlags & 0xFF7F;
    Music_Track = 0x31;
}

// original 0x43A230: the exit hook. Unless won: (without the fight's bit 0)
// actor 2 - BossActor_ClearBit40, Sprite_Current = BossActor_Find,
// Sprite_SetAnimationBank(0xF9), +0x24 &= 0xFE, then by the fight's bit 6
// (read after that call) Sprite_SetAnimation(1) or Sprite_SetAnimationAt(5,
// 4), +0x2A = 1; actor 1 - bank 0xEF, animation 0, enemy 1's +0x58 / +0x5A;
// actor 0 - bank 0xEE, animation 0, enemy 0's. Both ways then
// Scenario_CallA(1), member 1's +0 |= 0x40, and 0x92BF18 = 6, or 7 when
// 0x903848 (read after the call) is not 0x14.
extern "C" void __cdecl Boss16_Exit(void) {
    if (!(Battle_EndFlags & 2)) {
        if (!(BossSe_Script & 1)) {
            BH_CALL(BossActor_ClearBit40)(2);
            Sprite_Current = Actor("Boss16_Exit", 2);
            BH_CALL(Sprite_SetAnimationBank)(0xF9);
            Sprite_Current[0x24] &= 0xFE;
            if (BossSe_Script & 0x40)
                BH_CALL(Sprite_SetAnimation)(1);
            else
                BH_CALL(Sprite_SetAnimationAt)(5, 4);
            Sprite_Current[0x2A] = 1;
        }
        BH_CALL(BossActor_ClearBit40)(1);
        Sprite_Current = Actor("Boss16_Exit", 1);
        BH_CALL(Sprite_SetAnimationBank)(0xEF);
        BH_CALL(Sprite_SetAnimation)(0);
        SetWord(Sprite_Current + 0x58, Battle_Enemies[1].script_pos);
        SetWord(Sprite_Current + 0x5A, Battle_Enemies[1].frame_word);
        BH_CALL(BossActor_ClearBit40)(0);
        Sprite_Current = Actor("Boss16_Exit", 0);
        BH_CALL(Sprite_SetAnimationBank)(0xEE);
        BH_CALL(Sprite_SetAnimation)(0);
        SetWord(Sprite_Current + 0x58, Battle_Enemies[0].script_pos);
        SetWord(Sprite_Current + 0x5A, Battle_Enemies[0].frame_word);
    }
    BH_CALL(Scenario_CallA)(1);
    ObjTrio[1].flags |= 0x40;
    BossH_LeaderPick = static_cast<unsigned char>(B(::bof3::Img(at::kMoveCounter.v)) != 0x14 ? 7 : 6);
}

// original 0x43A360: BossKind_Table[15]: by +1 through BossRocky_Steps (12).
extern "C" unsigned long __cdecl BossRocky_Dispatch(unsigned long through) {
    return DispatchBossSc("BossRocky_Dispatch", ::bof3::Img(0x64CDAC), 12, 1, through);
}

// original 0x43A380: state 0: the tables (BossRocky_Anims, BossRocky_Hook,
// BossRocky_F8), +1 = 2, a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossRocky_Enter(void) {
    StoreKindTables(::bof3::Img(0x64CD90), ::bof3::Code(bof3::addr::BossRocky_Hook), ::bof3::Img(0x64CD9C));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A3C0: the +0xF4 hook: through BossRocky_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossRocky_Hook(unsigned long word) { return HookDispatchBossSc("BossRocky_Hook", ::bof3::Img(0x64CDDC), word); }

// original 0x43A3D0: BossKind_Table[53]: by +1 through BossSample8_Steps (12).
extern "C" unsigned long __cdecl BossSample8_Dispatch(unsigned long through) {
    return DispatchBossSc("BossSample8_Dispatch", ::bof3::Img(0x64CDE8), 12, 1, through);
}

// original 0x43A3F0: state 0: the tables (BossRocky_Anims - Rocky's own -,
// BossSample8_Hook, BossSample8_F8), 0x939AD8's +0x114 |= 8, +1 = 2, a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample8_Enter(void) {
    StoreKindTables(::bof3::Img(0x64CD90), ::bof3::Code(bof3::addr::BossSample8_Hook), ::bof3::Img(0x64CDA4));
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = static_cast<std::int32_t>(static_cast<U>(enemy_rec->flags_114) | 8);
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A450: the +0xF4 hook: through BossSample8_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample8_Hook(unsigned long word) {
    return HookDispatchBossSc("BossSample8_Hook", ::bof3::Img(0x64CE18), word);
}

// original 0x43A460: Boss_SetupTable[14]: End = Boss14_End, Exit =
// BossHook_ExitClearActor0, Event = BareRetZero.
extern "C" void __cdecl Boss14_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::Boss14_End), ::bof3::Code(bof3::addr::BossHook_ExitClearActor0), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43A480: won: 0x903848 = 0xC, 0x446DE0; else 0x446E00.
extern "C" void __cdecl Boss14_End(void) { EndByWin(0xC); }

// original 0x43A4A0: the exit hook (0x904B68) of set-ups 14, 28, 29:
// BossActor_Clear(0).
extern "C" void __cdecl BossHook_ExitClearActor0(void) { BH_CALL(BossActor_Clear)(0); }

// original 0x43A4B0: Boss_SetupTable[46]: End = BossHook_EndPickWay, Exit =
// BareRet, Event = BareRetZero.
extern "C" void __cdecl Boss46_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::BossHook_EndPickWay), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43A4D0: BossKind_Table[16]: by +1 through BossPooch_Steps (12).
extern "C" unsigned long __cdecl BossPooch_Dispatch(unsigned long through) {
    return DispatchBossSc("BossPooch_Dispatch", ::bof3::Img(0x64CE38), 12, 1, through);
}

// original 0x43A4F0: state 0: the tables (BossPooch_Anims, BossPooch_Hook,
// BossPooch_F8), +1 = 2, a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossPooch_Enter(void) {
    StoreKindTables(::bof3::Img(0x64CE24), ::bof3::Code(bof3::addr::BossPooch_Hook), ::bof3::Img(0x64CE30));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A530: the +0xF4 hook: through BossPooch_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossPooch_Hook(unsigned long word) { return HookDispatchBossSc("BossPooch_Hook", ::bof3::Img(0x64CE68), word); }

// original 0x43A540: Boss_SetupTable[15]: End = Boss15_End, Exit =
// Boss15_Exit, Event = BareRetZero.
extern "C" void __cdecl Boss15_Setup(void) { StoreHooks(::bof3::Code(bof3::addr::Boss15_End), ::bof3::Code(bof3::addr::Boss15_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x43A560: won: 0x903848 = 0x4B, 0x446DE0; else 0x446E00.
extern "C" void __cdecl Boss15_End(void) { EndByWin(0x4B); }

// original 0x43A580: the exit hook: BossActor_Clear(1).
extern "C" void __cdecl Boss15_Exit(void) { BH_CALL(BossActor_Clear)(1); }

// original 0x43A590: BossKind_Table[18]: jmp [BossMutant_Steps + 4 * +1].
extern "C" void __cdecl BossMutant_Dispatch(unsigned passed) {
    DispatchBossSd("BossMutant_Dispatch", AddressOf(BossMutant_Steps), 12, 1, passed);
}

// original 0x43A5B0: BossMutant_Steps 0, the entrance: 0x939AD8's +0xFC =
// 0x64CE74, +0xF4 = BossMutant_Hook, +0xF8 = 0x64CE98; Sprite_Current +1 = 2;
// jmp Sprite_ScriptTick (its al the answer).
extern "C" unsigned char __cdecl BossMutant_Enter(void) {
    PointEnemy(::bof3::Img(0x64CE74), ::bof3::Code(bof3::addr::BossMutant_Hook), ::bof3::Img(0x64CE98));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A5F0: BossMutant_Steps 6: jmp [BossMutant_ActSubs + 4 * +2].
extern "C" void __cdecl BossMutant_ActDispatch(unsigned passed) {
    DispatchBossSd("BossMutant_ActDispatch", AddressOf(BossMutant_ActSubs), 6, 2, passed);
}

// original 0x43A610: the +0xF4 hook: jmp [BossMutant_Hooks + 4 * (code & 0xFF)].
extern "C" void __cdecl BossMutant_Hook(unsigned code) { HookTable("BossMutant_Hook", AddressOf(BossMutant_Hooks), code); }

// original 0x43A620: Boss_SetupTable[17]: BattleHook_End = Boss17_End,
// BattleHook_Exit = BossHook_ExitActor0Bit40, BattleHook_Event = BareRetZero.
extern "C" void __cdecl Boss17_Setup(void) { StoreHooksBossSd(::bof3::Code(bof3::addr::Boss17_End), ::bof3::Code(at::kExitActor0Bit40.v), ::bof3::Code(at::kBareRetZero.v)); }

// original 0x43A640: the end hook: the win (0x904AE8 bit 1) sets the field's
// move-script counter 0 to 0x14 and jumps to the end phase's step 1; else step 2.
extern "C" void __cdecl Boss17_End(void) { EndCounter(0x14); }

// original 0x43A660: BossKind_Table[21]: jmp [BossClaw_Steps + 4 * +1].
extern "C" void __cdecl BossClaw_Dispatch(unsigned passed) {
    DispatchBossSd("BossClaw_Dispatch", AddressOf(BossClaw_Steps), 12, 1, passed);
}

// original 0x43A680: BossClaw_Steps 0, the entrance: with chapter flag 0x35,
// 0x939AD8's +0xA4 / +0xA6 = the words 0x939A1A / 0x939A14; without, flag
// 0x35 set. Then +0xFC = 0x64CF04, +0xF4 = BossClaw_Hook, +0xF8 = 0x64CF58,
// Sprite_Current +1 = 1, Sprite_EnsureAnimation(2), jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossClaw_Enter(void) {
    RestoreOrMark(0x35, ::bof3::Img(0x64CF04), ::bof3::Code(bof3::addr::BossClaw_Hook), ::bof3::Img(0x64CF58));
    rv::Current<records::BattleEnemy>()->state = 1;
    BH_CALL(Sprite_EnsureAnimation)(2);
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A720: state 1 of kinds 21, 22 and 27 (their +1 tables' entry
// 1): Sprite_ScriptTickOnce, and when it answers al not 0, Sprite_Current's
// +1 (read after the call) = 2.
extern "C" void __cdecl BossOp_EnterTick(void) {
    if (BH_CALL(Sprite_ScriptTickOnce)() & 0xFF) rv::Current<records::BattleEnemy>()->state = 2;
}

// original 0x43A740: the +0xF4 hook through BossClaw_Hooks.
extern "C" void __cdecl BossClaw_Hook(unsigned code) { HookTable("BossClaw_Hook", AddressOf(BossClaw_Hooks), code); }

// original 0x43A750: entry 0 of kinds 21, 22 and 23's +0xF4 hook tables (the
// hook called with 0, 0x435BF5): when the leader's target byte +0x124 is 4,
// the battle's target 0x904B44 = 5. The hook's word is not read.
extern "C" void __cdecl BossHook_RetargetMember0(unsigned) {
    if (rv::Bytes(ObjTrio)[0x124] == 4) B(::bof3::Img(boss_h::at::kTarget.v)) = 5;
}

// original 0x43A770: BossKind_Table[22]: jmp [BossCawer_Steps + 4 * +1].
extern "C" void __cdecl BossCawer_Dispatch(unsigned passed) {
    DispatchBossSd("BossCawer_Dispatch", AddressOf(BossCawer_Steps), 12, 1, passed);
}

// original 0x43A790: BossCawer_Steps 0: BossClaw_Enter's body with flag 0x36,
// +0xFC = 0x64CF10, +0xF4 = BossCawer_Hook, +0xF8 = 0x64CF60.
extern "C" unsigned char __cdecl BossCawer_Enter(void) {
    RestoreOrMark(0x36, ::bof3::Img(0x64CF10), ::bof3::Code(bof3::addr::BossCawer_Hook), ::bof3::Img(0x64CF60));
    rv::Current<records::BattleEnemy>()->state = 1;
    BH_CALL(Sprite_EnsureAnimation)(2);
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A830: the +0xF4 hook through BossCawer_Hooks.
extern "C" void __cdecl BossCawer_Hook(unsigned code) { HookTable("BossCawer_Hook", AddressOf(BossCawer_Hooks), code); }

// original 0x43A840: BossKind_Table[23]: jmp [BossPatrio_Steps + 4 * +1].
extern "C" void __cdecl BossPatrio_Dispatch(unsigned passed) {
    DispatchBossSd("BossPatrio_Dispatch", AddressOf(BossPatrio_Steps), 12, 1, passed);
}

// original 0x43A860: BossPatrio_Steps 0: flag 0x37, +0xFC = 0x64CF1C, +0xF4 =
// BossPatrio_Hook, +0xF8 = 0x64CF68; Sprite_Current +1 = 2 (no animation),
// jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossPatrio_Enter(void) {
    RestoreOrMark(0x37, ::bof3::Img(0x64CF1C), ::bof3::Code(bof3::addr::BossPatrio_Hook), ::bof3::Img(0x64CF68));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A8F0: the +0xF4 hook through BossPatrio_Hooks.
extern "C" void __cdecl BossPatrio_Hook(unsigned code) { HookTable("BossPatrio_Hook", AddressOf(BossPatrio_Hooks), code); }

// original 0x43A900: BossKind_Table[26]: first 0x939AD8's +0xFC by the
// sprite's +5 and a chapter flag - +5 == 4: flag 0x34 ? 0x64CF34 : 0x64CF28;
// else flag 0x33 ? 0x64CF4C : 0x64CF40 - then (Sprite_Current read again)
// jmp [BossDodai_Steps + 4 * +1].
extern "C" void __cdecl BossDodai_Dispatch(unsigned passed) {
    if (rv::Current<records::BattleEnemy>()->actor == 4) {
        const bool f = BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x34) & 0xFF;
        SetCell(Enemy() + 0xFC, ::bof3::Img(f ? 0x64CF34 : 0x64CF28));
    } else {
        const bool f = BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x33) & 0xFF;
        SetCell(Enemy() + 0xFC, ::bof3::Img(f ? 0x64CF4C : 0x64CF40));
    }
    DispatchBossSd("BossDodai_Dispatch", AddressOf(BossDodai_Steps), 12, 1, passed);
}

// original 0x43A990: BossDodai_Steps 0, the entrance: with chapter flag 0x1E
// or (asked only without it) 0x23, 0x939AD8's +0xA4 = the word 0x939A18 when
// Sprite_Current's +5 is 4, else 0x939A16. Then by Sprite_Current's +5 == 5:
// its +8 |= 2 and +0xFC = 0x64CF40, else +0xFC = 0x64CF28; +0xF4 =
// BossDodai_Hook, +0xF8 = 0x64CF70; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDodai_Enter(void) {
    bool back = BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x1E) & 0xFF;
    if (!back) back = BH_CALL(Flags_Test)(At(Chapter_FlagRow), 0x23) & 0xFF;
    if (back) {
        const unsigned hp = rv::Current<records::BattleEnemy>()->actor == 4 ? BossSd_KeptHp1 : BossSd_KeptHp2;
        rv::As<records::BattleEnemy>(Enemy())->hp = hp;
    }
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    if (cur_enemy->actor == 5) {
        cur_enemy->layout |= 2;
        SetCell(Enemy() + 0xFC, ::bof3::Img(0x64CF40));
    } else {
        SetCell(Enemy() + 0xFC, ::bof3::Img(0x64CF28));
    }
    SetCell(Enemy() + 0xF4, ::bof3::Code(bof3::addr::BossDodai_Hook));
    SetCell(Enemy() + 0xF8, ::bof3::Img(0x64CF70));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43AA50: BossDodai_Steps 6: jmp [BossDodai_ActSubs + 4 * +2].
extern "C" void __cdecl BossDodai_ActDispatch(unsigned passed) {
    DispatchBossSd("BossDodai_ActDispatch", AddressOf(BossDodai_ActSubs), 6, 2, passed);
}

// original 0x43AA70: BossDodai_ActSubs 4, the death: Sprite_SetAnimation(4),
// Battle_EnemyDefeated, BossMap_SetCorners(Sprite_Current's +5 == 4 ? 1 : 0,
// 2), the ground words of enemy 0 and the leader, then the states 3, 0, 0 with
// +0 &= 0xBF.
extern "C" void __cdecl BossDodai_Death(void) {
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(Battle_EnemyDefeated)();
    BH_CALL(BossMap_SetCorners)(rv::Current<records::BattleEnemy>()->actor == 4 ? 1 : 0, 2);
    GroundWords();
    DeathStates();
}

// original 0x43AB00: BossDodai_Steps 11 (where EnemyOp_Steps has
// EnemyOp_HitPose): jmp [BossDodai_HitPoseSubs + 4 * +2].
extern "C" void __cdecl BossDodai_HitPoseDispatch(unsigned passed) {
    DispatchBossSd("BossDodai_HitPoseDispatch", AddressOf(BossDodai_HitPoseSubs), 3, 2, passed);
}

// original 0x43AB20: BossDodai_HitPoseSubs 0: Sprite_Current's dword +0x38 +=
// (Frame_Counter & 1) << 9 - a shake of 0x200 every other frame.
extern "C" void __cdecl BossDodai_HitShake(void) {
    const U add = (Frame_Counter & 1) << 9;
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    cur_enemy->z = static_cast<std::int32_t>(static_cast<U>(cur_enemy->z) + add);
}

// original 0x43AB40: the +0xF4 hook through BossDodai_Hooks.
extern "C" void __cdecl BossDodai_Hook(unsigned code) { HookTable("BossDodai_Hook", AddressOf(BossDodai_Hooks), code); }

// original 0x43AB50: entry 0 of kinds 24 and 26's hook tables (the hook called
// with 0, the action pick): the action kind 0x904B35 = 0. The word is not read.
extern "C" void __cdecl BossHook_ActKindNone(unsigned) { B(::bof3::Img(boss_sd::at::kActKind.v)) = 0; }

// original 0x43AB60: BossDodai_Hooks 1 (the hook called with 1, the hit):
// when 0x939AD8's +0x108 is above 0 (signed word), Sound_PlayById(0x600) or,
// by Rand's bit 0 clear, 0x601.
extern "C" void __cdecl BossDodai_HitSound(unsigned) {
    if (static_cast<std::int16_t>(static_cast<std::uint16_t>(rv::As<records::BattleEnemy>(Enemy())->hp_change)) <= 0) return;
    BH_CALL(Sound_PlayById)(BH_CALL(Rand)() & 1 ? 0x600 : 0x601);
}

// original 0x43ABA0: Boss_SetupTable[18]: End = Boss18_End, Exit = Boss18_Exit,
// Event = Boss18_Event.
extern "C" void __cdecl Boss18_Setup(void) { StoreHooksBossSd(::bof3::Code(bof3::addr::Boss18_End), ::bof3::Code(bof3::addr::Boss18_Exit), ::bof3::Code(bof3::addr::Boss18_Event)); }

// original 0x43ABC0: the event hook (EventAreaFight above).
extern "C" unsigned char __cdecl Boss18_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AC50: the end hook: the win's chapter step 0x1A (0x904AAD bit
// 1) or 0x1B; the rest as EndAreaFight above, Boss_SetByLeaderId before the
// step.
extern "C" void __cdecl Boss18_End(void) { EndAreaFight(0x1A, 0x1B, true); }

// original 0x43AD10: the exit hook: the actor tagged 0 (ExitAreaFight above).
extern "C" void __cdecl Boss18_Exit(void) { ExitAreaFight(0); }

// original 0x43AD60: Boss_SetupTable[19]: End = Boss19_End, Exit =
// Boss19_Exit, Event = Boss19_Event.
extern "C" void __cdecl Boss19_Setup(void) { StoreHooksBossSd(::bof3::Code(bof3::addr::Boss19_End), ::bof3::Code(bof3::addr::Boss19_Exit), ::bof3::Code(bof3::addr::Boss19_Event)); }

// original 0x43AD80: Boss18_Event's body.
extern "C" unsigned char __cdecl Boss19_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AE10: the win's chapter step 0x1A (0x904AAD bit 1) or 0x23.
extern "C" void __cdecl Boss19_End(void) { EndAreaFight(0x1A, 0x23, true); }

// original 0x43AED0: the actor tagged 1.
extern "C" void __cdecl Boss19_Exit(void) { ExitAreaFight(1); }

// original 0x43AF20: Boss_SetupTable[20]: End = Boss20_End, Exit =
// Boss20_Exit, Event = Boss20_Event.
extern "C" void __cdecl Boss20_Setup(void) { StoreHooksBossSd(::bof3::Code(bof3::addr::Boss20_End), ::bof3::Code(bof3::addr::Boss20_Exit), ::bof3::Code(bof3::addr::Boss20_Event)); }

// original 0x43AF40: Boss18_Event's body.
extern "C" unsigned char __cdecl Boss20_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AFD0: the win's chapter step 0x1A (0x904AAD bit 1) or 0x29;
// the chapter step stored before Boss_SetByLeaderId.
extern "C" void __cdecl Boss20_End(void) { EndAreaFight(0x1A, 0x29, false); }

// original 0x43B080: the actor tagged 2.
extern "C" void __cdecl Boss20_Exit(void) { ExitAreaFight(2); }

// original 0x43B0D0: the map's corner cells of a 2 x 2 block set to one dword:
// the block's corner (x, y) the bytes BossMap_CornerCells[cell] (2 pairs), the
// dword BossMap_CornerValues[value] (3); cell (x + i, y + j) is AreaMap_Corners
// [width * (y + j) + x + i], width the byte AreaMap_Header +0 (read for each).
// Called by kind 26's 0x43AA70 (value 2) and BossMap_UpdateFromEnemies (0 or 1).
extern "C" void __cdecl BossMap_SetCorners(unsigned cell, unsigned value) {
    if (cell > 1 || value > 2)
        bof3::Fatal("BossMap_SetCorners(%u, %u): past BossMap_CornerCells (2) or BossMap_CornerValues (3) - the "
                    "original reads whatever follows (docs/boss_h.md section 6)",
                    cell, value);
    const std::int32_t v = Long(At(AddressOf(BossMap_CornerValues) + 4 * value));
    const U x = At(AddressOf(BossMap_CornerCells) + 2 * cell)[0];
    const U y = At(AddressOf(BossMap_CornerCells) + 2 * cell + 1)[0];
    for (U j = 0; j < 2; ++j)
        for (U i = 0; i < 2; ++i) {
            const U width = AreaMap_Header[0];
            SetLong(At(::bof3::Img(AreaMap_Corners_addr) + 4 * (width * (y + j) + x + i)), v);
        }
}

// original 0x43B130: by the leader's character id 0x802DC9 - 0, 1, 5, 6 - the
// byte 0x92BF18 = 0x2C, 0x2D, 0x2E, 0x2F (a jump table of 7 inside the
// function); any other id leaves it. Called by set-ups 18..20's hooks before
// the battle ends.
extern "C" void __cdecl Boss_SetByLeaderId(void) {
    switch (ObjTrio[0].char_id) {
    case 0: BossH_LeaderPick = 0x2C; break;
    case 1: BossH_LeaderPick = 0x2D; break;
    case 5: BossH_LeaderPick = 0x2E; break;
    case 6: BossH_LeaderPick = 0x2F; break;
    default: break;
    }
}

// original 0x43B180: called by set-ups 18..20's event hooks (phase 5). For
// enemy 1 (0x93BA88) and then enemy 2 (0x93BBB0), unless its +0x93 has 0x40:
// with 0x20 - flag 0x34 (0x33 for enemy 2) of the chapter's bits set,
// Sprite_Current = the enemy, Sprite_EnsureAnimation(3), BossMap_SetCorners(1
// (0 for enemy 2), 1); without - the flag cleared, Sprite_EnsureAnimation(2),
// BossMap_SetCorners(.., 0). Then enemy 0's ground word +0x3E and the
// leader's = AreaMap_Elevation of their (+0x34, +0x38); and with the leader's
// +0x134 bit 1, 0x939B1C = its +0x3C.
extern "C" void __cdecl BossMap_UpdateFromEnemies(void) {
    const struct { U status, object; unsigned flag, cell; } kEnemies[] = {
        {::bof3::Img(BossH_Enemy1Status_addr), ::bof3::Img(Battle_Enemies_addr) + 0x128, 0x34, 1},
        {::bof3::Img(BossH_Enemy2Status_addr), ::bof3::Img(Battle_Enemies_addr) + 0x250, 0x33, 0},
    };
    for (const auto& e : kEnemies) {
        const unsigned status = At(e.status)[1];   // the dword's second byte: +0x93
        if (status & 0x40) continue;
        const bool up = (status & 0x20) != 0;
        unsigned char* const bits = At(Chapter_FlagRow);
        if (up) BH_CALL(Flags_Set)(bits, e.flag);
        else BH_CALL(Flags_Clear)(bits, e.flag);
        Sprite_Current = At(e.object);
        BH_CALL(Sprite_EnsureAnimation)(up ? 3 : 2);
        BH_CALL(BossMap_SetCorners)(e.cell, up ? 1 : 0);
    }
    const long ground = BH_CALL(AreaMap_Elevation)(Battle_Enemies[0].x, Battle_Enemies[0].z);
    Battle_Enemies[0].height_whole = static_cast<unsigned>(ground) & 0xFFFF;
    const long leader = BH_CALL(AreaMap_Elevation)(ObjTrio[0].x, ObjTrio[0].z);
    ObjTrio[0].height_whole = static_cast<unsigned>(leader) & 0xFFFF;
    if (rv::Bytes(ObjTrio)[0x134] & 2) BossH_CameraY = ObjTrio[0].height;
}

// original 0x43B280: BossKind_Table[24]: jmp [BossEmitai_Steps + 4 * +1].
extern "C" void __cdecl BossEmitai_Dispatch(unsigned passed) {
    DispatchBossSd("BossEmitai_Dispatch", AddressOf(BossEmitai_Steps), 12, 1, passed);
}

// original 0x43B2A0: BossEmitai_Steps 0: +0xFC = 0x64D0A0, +0xF4 =
// BossEmitai_Hook, +0xF8 = 0x64D0B8; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossEmitai_Enter(void) {
    PointEnemy(::bof3::Img(0x64D0A0), ::bof3::Code(bof3::addr::BossEmitai_Hook), ::bof3::Img(0x64D0B8));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B2E0: BossEmitai_Steps 6: jmp [BossEmitai_ActSubs + 4 * +2].
extern "C" void __cdecl BossEmitai_ActDispatch(unsigned passed) {
    DispatchBossSd("BossEmitai_ActDispatch", AddressOf(BossEmitai_ActSubs), 6, 2, passed);
}

// original 0x43B300: BossEmitai_ActSubs 4, the death: Sprite_EnsureAnimation(4),
// Sprite_Current (read after) +0x2A = 0, Sprite_ScriptTickOnce (its answer
// not read), Battle_EnemyDefeated, 0x939AD8's +0x110 |= 0x1000, then the
// states 3, 0, 0 with +0 &= 0xBF.
extern "C" void __cdecl BossEmitai_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(4);
    rv::Current<records::BattleEnemy>()->flip = 0;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_110 = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_110)) | 0x1000);
    DeathStates();
}

// original 0x43B360: the +0xF4 hook through BossEmitai_Hooks.
extern "C" void __cdecl BossEmitai_Hook(unsigned code) { HookTable("BossEmitai_Hook", AddressOf(BossEmitai_Hooks), code); }

// original 0x43B370: BossKind_Table[25]: jmp [BossGolem_Steps + 4 * +1].
extern "C" void __cdecl BossGolem_Dispatch(unsigned passed) {
    DispatchBossSd("BossGolem_Dispatch", AddressOf(BossGolem_Steps), 12, 1, passed);
}

// original 0x43B390: BossGolem_Steps 0: +0xFC = 0x64D0AC, +0xF4 =
// BossGolem_Hook, +0xF8 = 0x64D0C0; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGolem_Enter(void) {
    PointEnemy(::bof3::Img(0x64D0AC), ::bof3::Code(bof3::addr::BossGolem_Hook), ::bof3::Img(0x64D0C0));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B3D0: the +0xF4 hook through BossGolem_Hooks (three BareRet).
extern "C" void __cdecl BossGolem_Hook(unsigned code) { HookTable("BossGolem_Hook", AddressOf(BossGolem_Hooks), code); }

// original 0x43B3E0: Boss_SetupTable[21]: End = Boss21_End, Exit =
// Boss21_Exit, Event = Boss21_Event.
extern "C" void __cdecl Boss21_Setup(void) { StoreHooksBossSd(::bof3::Code(bof3::addr::Boss21_End), ::bof3::Code(bof3::addr::Boss21_Exit), ::bof3::Code(bof3::addr::Boss21_Event)); }

// original 0x43B400: the event hook: phase 3 - every actor 0..10 that
// Battle_ActorIsOut answers al 0 for gets bit 0x10: members 0..2 in their
// +0x90 (ObjTrio), enemies 3..10 in their object's +0x92. al 0 always. The
// original writes each actor into its own argument's low byte and pushes that
// dword (the caller's word above it); ours passes the same word.
extern "C" unsigned char __cdecl Boss21_Event(unsigned code) {
    if ((code & 0xFF) != 3) return 0;
    for (unsigned i = 0; i <= 2; ++i)
        if ((BH_CALL(Battle_ActorIsOut)((code & 0xFFFFFF00u) | i) & 0xFF) == 0)
            At(::bof3::Img(ObjTrio_addr) + 0x90 + i * boss_sd::at::kPartyStride)[0] |= 0x10;
    for (unsigned i = 3; i <= 10; ++i)
        if ((BH_CALL(Battle_ActorIsOut)((code & 0xFFFFFF00u) | i) & 0xFF) == 0)
            At(::bof3::Img(Battle_Enemies_addr) + 0x92 + (i - 3) * at::kEnemyStride)[0] |= 0x10;
    return 0;
}

// original 0x43B480: the exit hook: BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0), its +0x2A = 0 (the answer untested: null faults, ours
// aborts), Sprite_SetAnimation(4); BossActor_Clear(1), BossActor_Clear(2).
extern "C" void __cdecl Boss21_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    auto* const field_actor = rv::As<records::SpriteObject>(actor);
    Sprite_Current = actor;
    if (actor == nullptr)
        bof3::Fatal("Boss21_Exit: no field actor tagged 0 - the original writes +0x2A through the null (docs/boss_sd.md "
                    "section 6)");
    field_actor->flip = 0;
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(BossActor_Clear)(1);
    BH_CALL(BossActor_Clear)(2);
}

// original 0x43B4B0: BossKind_Table[27]: jmp [BossGarr_Steps + 4 * +1].
extern "C" void __cdecl BossGarr_Dispatch(unsigned passed) {
    DispatchBossSd("BossGarr_Dispatch", AddressOf(BossGarr_Steps), 12, 1, passed);
}

// original 0x43B4D0: BossGarr_Steps 0: +0xFC = 0x64D158, +0xF4 =
// BossGarr_Hook, +0xF8 = 0x64D164; Sprite_Current +1 = 1;
// Sprite_SetAnimationBank(0x153); Sprite_Current (read after) +0x2A = 0;
// Sprite_SetAnimation(0). Its eax is the last call's (no caller reads it).
extern "C" void __cdecl BossGarr_Enter(void) {
    PointEnemy(::bof3::Img(0x64D158), ::bof3::Code(bof3::addr::BossGarr_Hook), ::bof3::Img(0x64D164));
    rv::Current<records::BattleEnemy>()->state = 1;
    BH_CALL(Sprite_SetAnimationBank)(0x153);
    rv::Current<records::BattleEnemy>()->flip = 0;
    BH_CALL(Sprite_SetAnimation)(0);
}

// original 0x43B530: BossGarr_Steps 6: jmp [BossGarr_ActSubs + 4 * +2].
extern "C" void __cdecl BossGarr_ActDispatch(unsigned passed) {
    DispatchBossSd("BossGarr_ActDispatch", AddressOf(BossGarr_ActSubs), 6, 2, passed);
}

// original 0x43B550: entry 4 (the death) of the +2 tables of kinds 2, 3, 6, 7,
// 18 and 27: Sprite_ScriptTickOnce (its answer not read), Battle_EnemyDefeated,
// then 0x939AD8's +0x110 |= 0x1000, and Sprite_Current (read after the calls)
// +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
extern "C" void __cdecl BossOp_Death(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_110 = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_110)) | 0x1000);
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    cur_enemy->flags &= 0xBF;
    cur_enemy->state = 3;
    cur_enemy->step = 0;
    cur_enemy->state3 = 0;
}

// original 0x43B5A0: the +0xF4 hook through BossGarr_Hooks (three BareRet).
extern "C" void __cdecl BossGarr_Hook(unsigned code) { HookTable("BossGarr_Hook", AddressOf(BossGarr_Hooks), code); }

// original 0x43B5B0: Boss_SetupTable[22]. The hooks: end Boss22_End, exit
// Boss22_Exit, event BareRetZero.
extern "C" void __cdecl Boss22_Setup(void) {
    StoreHooksBossSe(::bof3::Code(bof3::addr::Boss22_End), ::bof3::Code(bof3::addr::Boss22_Exit), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43B5D0: the end hook. 0x904AE8 and 0x904AE5 read; movement-script
// variable 3 (0x903848) = 0xC, 0x904AE8 |= 8, 0x904AE5 &= 0xBF (the battle's
// music not kept), Music_Track = 0x6A, the byte 0x92BF18 = 3; then a tail jump
// to 0x446E20 (the end phase, step 3) - whatever the battle's end.
extern "C" void __cdecl Boss22_End(void) {
    const unsigned char end = Battle_EndFlags;
    const unsigned char music = EventBattle_Flags;
    B(::bof3::Img(boss_se::at::kScriptVar3.v)) = 0xC;
    Battle_EndFlags = static_cast<unsigned char>(end | 8);
    EventBattle_Flags = static_cast<unsigned char>(music & 0xBF);
    Music_Track = 0x6A;
    BossH_LeaderPick = 3;
    BH_AT(Handler, boss_se::at::kEndThird.v)();
}

// original 0x43B610: the exit hook. BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0); Sprite_SetAnimationBank(0xAA); the Sprite_Current of after
// the call +0x2A = 0 and +0x48 = 0; Sprite_SetAnimation(6).
extern "C" void __cdecl Boss22_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    Sprite_Current = BH_CALL(BossActor_Find)(0);
    BH_CALL(Sprite_SetAnimationBank)(0xAA);
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    BH_CALL(Sprite_SetAnimation)(6);
}

void BossNina_Inject() {
    BOF3_INJECT(BossNina_Dispatch);
    BOF3_INJECT(BossNina_Enter);
    BOF3_INJECT(BossNina_WalkDispatch);
    BOF3_INJECT(BossNina_WalkStart);
    BOF3_INJECT(BossNina_WalkStep);
    BOF3_INJECT(BossNina_Hook);
    BOF3_INJECT(BossNina_HookAct);
    BOF3_INJECT(BossNina_HookHit);
    BOF3_INJECT(Boss13_Setup);
    BOF3_INJECT(Boss13_End);
    BOF3_INJECT(Boss16_Setup);
    BOF3_INJECT(Boss16_Event);
    BOF3_INJECT(Boss16_End);
    BOF3_INJECT(Boss16_Exit);
    BOF3_INJECT(BossRocky_Dispatch);
    BOF3_INJECT(BossRocky_Enter);
    BOF3_INJECT(BossRocky_Hook);
    BOF3_INJECT(BossSample8_Dispatch);
    BOF3_INJECT(BossSample8_Enter);
    BOF3_INJECT(BossSample8_Hook);
    BOF3_INJECT(Boss14_Setup);
    BOF3_INJECT(Boss14_End);
    BOF3_INJECT(BossHook_ExitClearActor0);
    BOF3_INJECT(Boss46_Setup);
    BOF3_INJECT(BossPooch_Dispatch);
    BOF3_INJECT(BossPooch_Enter);
    BOF3_INJECT(BossPooch_Hook);
    BOF3_INJECT(Boss15_Setup);
    BOF3_INJECT(Boss15_End);
    BOF3_INJECT(Boss15_Exit);
    BOF3_INJECT(BossMutant_Dispatch);
    BOF3_INJECT(BossMutant_Enter);
    BOF3_INJECT(BossMutant_ActDispatch);
    BOF3_INJECT(BossMutant_Hook);
    BOF3_INJECT(Boss17_Setup);
    BOF3_INJECT(Boss17_End);
    BOF3_INJECT(BossClaw_Dispatch);
    BOF3_INJECT(BossClaw_Enter);
    BOF3_INJECT(BossOp_EnterTick);
    BOF3_INJECT(BossClaw_Hook);
    BOF3_INJECT(BossHook_RetargetMember0);
    BOF3_INJECT(BossCawer_Dispatch);
    BOF3_INJECT(BossCawer_Enter);
    BOF3_INJECT(BossCawer_Hook);
    BOF3_INJECT(BossPatrio_Dispatch);
    BOF3_INJECT(BossPatrio_Enter);
    BOF3_INJECT(BossPatrio_Hook);
    BOF3_INJECT(BossDodai_Dispatch);
    BOF3_INJECT(BossDodai_Enter);
    BOF3_INJECT(BossDodai_ActDispatch);
    BOF3_INJECT(BossDodai_Death);
    BOF3_INJECT(BossDodai_HitPoseDispatch);
    BOF3_INJECT(BossDodai_HitShake);
    BOF3_INJECT(BossDodai_Hook);
    BOF3_INJECT(BossHook_ActKindNone);
    BOF3_INJECT(BossDodai_HitSound);
    BOF3_INJECT(Boss18_Setup);
    BOF3_INJECT(Boss18_Event);
    BOF3_INJECT(Boss18_End);
    BOF3_INJECT(Boss18_Exit);
    BOF3_INJECT(Boss19_Setup);
    BOF3_INJECT(Boss19_Event);
    BOF3_INJECT(Boss19_End);
    BOF3_INJECT(Boss19_Exit);
    BOF3_INJECT(Boss20_Setup);
    BOF3_INJECT(Boss20_Event);
    BOF3_INJECT(Boss20_End);
    BOF3_INJECT(Boss20_Exit);
    BOF3_INJECT(BossMap_SetCorners);
    BOF3_INJECT(Boss_SetByLeaderId);
    BOF3_INJECT(BossMap_UpdateFromEnemies);
    BOF3_INJECT(BossEmitai_Dispatch);
    BOF3_INJECT(BossEmitai_Enter);
    BOF3_INJECT(BossEmitai_ActDispatch);
    BOF3_INJECT(BossEmitai_Death);
    BOF3_INJECT(BossEmitai_Hook);
    BOF3_INJECT(BossGolem_Dispatch);
    BOF3_INJECT(BossGolem_Enter);
    BOF3_INJECT(BossGolem_Hook);
    BOF3_INJECT(Boss21_Setup);
    BOF3_INJECT(Boss21_Event);
    BOF3_INJECT(Boss21_Exit);
    BOF3_INJECT(BossGarr_Dispatch);
    BOF3_INJECT(BossGarr_Enter);
    BOF3_INJECT(BossGarr_ActDispatch);
    BOF3_INJECT(BossOp_Death);
    BOF3_INJECT(BossGarr_Hook);
    BOF3_INJECT(Boss22_Setup);
    BOF3_INJECT(Boss22_End);
    BOF3_INJECT(Boss22_Exit);
}
