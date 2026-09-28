// Group BSH of the boss round: fights 34, 35, 36, 41, 43 and 47 and kinds 41,
// 42, 43, 44, 48, 50 and 54 plus the kind-3 dispatcher's slot 3 - 46 functions
// of 0x43DEF0..0x43ECC0 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// group column BSH), each read to its last instruction with capstone
// (2026-09-28) and taken through the boss harness (boss_harness.h). Round
// eleven, wave two; docs/boss_sh.md has them one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 48   Sample 3 (area 160)       set-ups 34 and 41   BOSS034 (kinds 40 Mikba - group BSG's - and 48)
//   kind 41   Gaist, kind 42 Torch (area 120), kind 54 Sample 9 (area 165)
//                                       set-ups 35 and 47   BOSS035 (= BOSS047 by section md5)
//   kind 43   Angler (area 75), kind 50 Sample 5 (area 162)
//                                       set-ups 36 and 43   BOSS036
//   kind 44   Elder (area 144; its set-up 38 / 44 is group BSI's)
//   slot 3    the kind-3 dispatcher's (BattleBossFx_Dispatch) slot 3: the
//             effect task the Angler's hook starts (BattleTask_Create(3, 3))
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables where the original
// jumps through whatever follows; the Angler's hook aborts where the original
// copies into the slot BattleTask_Create's "none free" 0xFF names (past the
// 48 slots and the image); set-up 34's exit hook aborts where the original
// writes through BossActor_Find's null (the owner's rule for an unchecked
// index or pointer, round9 doc section 6; nothing reaches any of them). Every
// call goes through the harness (BH_CALL / BH_AT / Phase), so the start-up
// fuzz can stand recorders in for the callees; a hook or table a function
// stores is the same literal address.
#include "game/boss_sh.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sh::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
unsigned char* PointerAt(U cell) { return At(static_cast<U>(Long(At(cell)))); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's word -
// reads that word), its eax the dispatcher's answer.
using Entry = U (__cdecl*)(U);
Entry EntryAt(U table, unsigned index) {
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U Dispatch(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryAt(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the action pick, the hit and
// BattleEnemy_RunAll pass), the entry gets the word whole, a Fatal past them.
U HookDispatch(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAt(table, index)(word);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), S(end));
    SetLong(At(at::kHookExit), S(exit));
    SetLong(At(at::kHookEvent), S(event));
}

// A kind's entrance, the one shape all seven kinds share (0x51 bytes each):
// 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook) and +0xF8 (its
// sound words), 0x939AD8 read for each store; then 0x939AD8 read once more and
// its dword +0x114 |= 8; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char Enter(U fc, U hook, U f8) {
    SetLong(Enemy() + 0xFC, S(fc));
    SetLong(Enemy() + 0xF4, S(hook));
    SetLong(Enemy() + 0xF8, S(f8));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, S(static_cast<U>(Long(e + 0x114)) | 8));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// AbilityList_Add as the originals call it, by its address (the jmp to ours
// in the game): the whole eax of its answer matters, because the second
// call's member word is that eax with its low byte replaced (`mov al,
// [0x675F08]` after the first call; AbilityList_Add's list lookup 0x591EC0
// masks the member to its byte, so the upper bytes are carried, never used).
using AbilityAdd = U (__cdecl*)(unsigned, unsigned, unsigned, unsigned);

// Set-up 34's two AbilityList_Add calls (its event hook's code 1 and its end
// hook): ability 0x40 to member 4's list (the third word 0: the member's own
// list, AbilityList_Add's reading), then to the member set-up 34 picked, the
// fourth word 1 - the member word the picked byte over the first answer's
// upper bytes.
void GiveAbility40() {
    const U first = BH_AT(AbilityAdd, bof3::addr::AbilityList_Add)(0x40, 4, 0, 0);
    BH_AT(AbilityAdd, bof3::addr::AbilityList_Add)(0x40, (first & 0xFFFFFF00u) | B(at::kPicked), 0, 1);
}

// The end hooks of set-ups 35 and 36: won (0x904AE8 bit 1) - the chapter step
// `step`, then a tail jump to 0x446DE0 (the end phase, step 1); else to
// 0x446E00 (step 2).
void EndWithStep(unsigned char step) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = step;
        BH_AT(Handler, at::kEndWin)();
    } else {
        BH_AT(Handler, at::kEndOther)();
    }
}

}  // namespace

// ===========================================================================
// Kind 48 (Sample 3, area 160) and set-ups 34 and 41 (BOSS034)
// ===========================================================================

// original 0x43DEF0: BossKind_Table[48]: jmp [BossSample3_States + 4 * +1] (12:
// its entrance, then the generic enemy states).
extern "C" unsigned long __cdecl BossSample3_Dispatch(unsigned long through) {
    return Dispatch("BossSample3_Dispatch", AddressOf(BossSample3_States), 12, 1, through);
}

// original 0x43DF10: state 0 - kind 40's (Mikba's) byte tables 0x64D6CC /
// 0x64D6D8 and its own hook BossSample3_Hook.
extern "C" unsigned char __cdecl BossSample3_Enter(void) {
    return Enter(at::kMikbaAnims, bof3::addr::BossSample3_Hook, at::kMikbaSounds);
}

// original 0x43DF70: the +0xF4 hook through BossSample3_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample3_Hook(unsigned long word) {
    return HookDispatch("BossSample3_Hook", AddressOf(BossSample3_Hooks), word);
}

// original 0x43DF80: Boss_SetupTable[34]. The byte 0x669730 read once; each
// party member 0..2 whose +0x148 equals it leaves its index in 0x675F08 (the
// last one wins; none leaves the byte as it was). The hooks: end Boss34_End,
// exit Boss34_Exit, event Boss34_Event.
extern "C" void __cdecl Boss34_Setup(void) {
    const unsigned char id = B(at::kPickId);
    for (unsigned m = 0; m <= 2; ++m)
        if (At(at::kPartyTag + m * at::kPartyStride)[0] == id) B(at::kPicked) = static_cast<unsigned char>(m);
    StoreHooks(bof3::addr::Boss34_End, bof3::addr::Boss34_Exit, bof3::addr::Boss34_Event);
}

// original 0x43DFD0: the event hook, by the code's low byte; al 0 on every
// path.
//   5 (an action phase's): unless the round flags' bit 15, 0x904AAD bit 0, or
//     the picked member's +0x91 bit 0x20 (the member indexed by 0x675F08's
//     byte, unchecked - only set-up 34 writes it, 0..2): 0x904AAD |= 1 and
//     0x446700(the dword at 0x675F08) - the member put at the front of the
//     round's remaining order.
//   1 (an action phase's): with 0x904AAD bit 0 set and bit 1 clear and the
//     acting actor the picked member: the acting kind 0x904B35 = 4, the action
//     record (0x904B40) +1 = 4 and word +2 = 0x40 (the pointer read again),
//     the target = the picked member, 0x904AAD |= 2 (read after those
//     stores), the word 0x904B80 = 0x40 (stored before 0x904AAD); then
//     ability 0x40 twice (GiveAbility40).
//   any other: nothing.
extern "C" unsigned char __cdecl Boss34_Event(unsigned code) {
    switch (code & 0xFF) {
    case 5: {
        if (Long(At(at::kFlags)) & 0x8000) return 0;
        if (B(at::kScript) & 1) return 0;
        const U picked = static_cast<U>(Long(At(at::kPicked)));
        if (At(at::kPartyFlags91 + (picked & 0xFF) * at::kPartyStride)[0] & 0x20) return 0;
        B(at::kScript) = static_cast<unsigned char>(B(at::kScript) | 1);
        BH_AT(void (__cdecl*)(unsigned), at::kOrderFront)(picked);
        return 0;
    }
    case 1: {
        const unsigned char script = B(at::kScript);
        if (script & 2) return 0;
        if ((script & 1) == 0) return 0;
        if (B(at::kActor) != B(at::kPicked)) return 0;
        unsigned char* const action = PointerAt(at::kAction);
        B(at::kActKind) = 4;
        action[1] = 4;
        SetWord(PointerAt(at::kAction) + 2, 0x40);
        B(at::kTarget) = B(at::kPicked);
        const auto marked = static_cast<unsigned char>(B(at::kScript) | 2);
        SetWord(At(at::kWord80), 0x40);
        B(at::kScript) = marked;
        GiveAbility40();
        return 0;
    }
    default: return 0;
    }
}

// original 0x43E0C0: the end hook. Won (0x904AE8 bit 1): for each party member
// 0..2 with +0 bit 0 - Battle_RemoveFromTurnOrder(its +5), then (its +0x90
// dword read after the call) Sprite_Current = the member and
// Sprite_PoseFromSet(its +8 + 0x1C when +0x90 bit 14, else + 4 (a byte),
// 0x8C5D80, 0x1800); then, unless 0x904AAD bit 1, ability 0x40 twice
// (GiveAbility40); 0x904AE8 |= 8; 0x446DE0 (the end phase, step 1) called and
// the chapter step 0x8034E5 incremented after it. Otherwise a tail jump to
// 0x446E00 (step 2).
extern "C" void __cdecl Boss34_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(at::kParty + m * at::kPartyStride);
        if ((p[0] & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(p[5]);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const auto pose = static_cast<unsigned char>(p[8] + (flags & 0x4000 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(at::kPoseSet), 0x1800);
    }
    if ((B(at::kScript) & 2) == 0) GiveAbility40();
    B(at::kBattleEnd) = static_cast<unsigned char>(B(at::kBattleEnd) | 8);
    BH_AT(Handler, at::kEndWin)();
    B(at::kChapterStep) = static_cast<unsigned char>(B(at::kChapterStep) + 1);
}

// original 0x43E210: the exit hook. BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0); Sprite_SetAnimationBank(0x1C1); Sprite_SetAnimation(8);
// then (Sprite_Current read again for each) +0x2A = 1, the words +0x58 / +0x5A
// = enemy 0's +0x58 / +0x5A - the actor tagged 0 put where enemy 0 stood. The
// answer is untested: with no actor tagged 0 the original writes through the
// null; ours aborts there.
extern "C" void __cdecl Boss34_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    Sprite_Current = BH_CALL(BossActor_Find)(0);
    BH_CALL(Sprite_SetAnimationBank)(0x1C1);
    BH_CALL(Sprite_SetAnimation)(8);
    if (Sprite_Current == nullptr)
        bof3::Fatal("Boss34_Exit: no field actor tagged 0 - the original writes +0x2A through the null (docs/boss_sh.md "
                    "section 6)");
    Sprite_Current[0x2A] = 1;
    SetWord(Sprite_Current + 0x58, Word(At(at::kEnemy0X)));
    SetWord(Sprite_Current + 0x5A, Word(At(at::kEnemy0Y)));
}

// original 0x43E270: Boss_SetupTable[41]. The hooks: end BossHook_EndPickWay
// (BH's), exit BareRet, event BareRetZero.
extern "C" void __cdecl Boss41_Setup(void) {
    StoreHooks(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// ===========================================================================
// Kinds 41 (Gaist), 42 (Torch), 54 (Sample 9) and set-ups 35 and 47
// ===========================================================================

// original 0x43E540: BossKind_Table[41]: jmp [BossGaist_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossGaist_Dispatch(unsigned long through) {
    return Dispatch("BossGaist_Dispatch", AddressOf(BossGaist_States), 12, 1, through);
}

// original 0x43E560: state 0 - BossGaist_Anims / BossGaist_Sounds and the hook
// BossGaist_Hook.
extern "C" unsigned char __cdecl BossGaist_Enter(void) {
    return Enter(AddressOf(BossGaist_Anims), bof3::addr::BossGaist_Hook, AddressOf(BossGaist_Sounds));
}

// original 0x43E5C0: the +0xF4 hook through BossGaist_Hooks (BareRet, BareRet,
// BossGaist_HookClearBit1).
extern "C" unsigned long __cdecl BossGaist_Hook(unsigned long word) {
    return HookDispatch("BossGaist_Hook", AddressOf(BossGaist_Hooks), word);
}

// original 0x43E5D0: hook entry 2 (BattleEnemy_RunAll's call): with the round
// flags' bit 1 set and the word MoveScript_WaitWordDA 0, the bit cleared (a
// word and) and Draw_PassFlags = 0. The word is not read.
extern "C" void __cdecl BossGaist_HookClearBit1(unsigned) {
    if ((B(at::kFlags) & 2) == 0) return;
    if (MoveScript_WaitWordDA != 0) return;
    SetWord(At(at::kFlags), Word(At(at::kFlags)) & 0xFFFD);
    Draw_PassFlags = 0;
}

// original 0x43E600: BossKind_Table[42]: jmp [BossTorch_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossTorch_Dispatch(unsigned long through) {
    return Dispatch("BossTorch_Dispatch", AddressOf(BossTorch_States), 12, 1, through);
}

// original 0x43E620: state 0 - +0xFC 0x675F0C (twelve bytes of .data nothing
// in the exe writes), +0xF8 0x64D7A0 (0xFFFF words), the hook BossTorch_Hook.
extern "C" unsigned char __cdecl BossTorch_Enter(void) {
    return Enter(at::kTorchAnims, bof3::addr::BossTorch_Hook, at::kTorchSounds);
}

// original 0x43E680: the +0xF4 hook through BossTorch_Hooks
// (BossTorch_HookTarget3, BareRet, BareRet).
extern "C" unsigned long __cdecl BossTorch_Hook(unsigned long word) {
    return HookDispatch("BossTorch_Hook", AddressOf(BossTorch_Hooks), word);
}

// original 0x43E690: hook entry 0 (the action pick's call): the target
// 0x904B44 = 3 (enemy 0). The word is not read.
extern "C" void __cdecl BossTorch_HookTarget3(unsigned) { B(at::kTarget) = 3; }

// original 0x43E6A0: BossKind_Table[54]: jmp [BossSample9_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossSample9_Dispatch(unsigned long through) {
    return Dispatch("BossSample9_Dispatch", AddressOf(BossSample9_States), 12, 1, through);
}

// original 0x43E6C0: state 0 - Gaist's byte tables and its own hook
// BossSample9_Hook.
extern "C" unsigned char __cdecl BossSample9_Enter(void) {
    return Enter(AddressOf(BossGaist_Anims), bof3::addr::BossSample9_Hook, AddressOf(BossGaist_Sounds));
}

// original 0x43E720: the +0xF4 hook through BossSample9_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample9_Hook(unsigned long word) {
    return HookDispatch("BossSample9_Hook", AddressOf(BossSample9_Hooks), word);
}

// original 0x43E730: Boss_SetupTable[35]. The hooks: end Boss35_End, exit
// BossHook_ExitTransition4 (0x43E790, set-up 26's too - group BSE's), event
// Boss35_Event.
extern "C" void __cdecl Boss35_Setup(void) {
    StoreHooks(bof3::addr::Boss35_End, at::kExitTransition4, bof3::addr::Boss35_Event);
}

// original 0x43E750: the event hook: code 0 (the low byte) and
// Battle_ActorIsOut(3) (enemy 0) answering al not 0 - 0x904AE8 |= 2 (the win).
// al 0.
extern "C" unsigned char __cdecl Boss35_Event(unsigned code) {
    if ((code & 0xFF) == 0 && (BH_CALL(Battle_ActorIsOut)(3) & 0xFF) != 0)
        B(at::kBattleEnd) = static_cast<unsigned char>(B(at::kBattleEnd) | 2);
    return 0;
}

// original 0x43E770: the end hook: won - chapter step 0x15, step 1; else step 2.
extern "C" void __cdecl Boss35_End(void) { EndWithStep(0x15); }

// original 0x43E7A0: Boss_SetupTable[47]. As set-up 41: BossHook_EndPickWay,
// BareRet, BareRetZero.
extern "C" void __cdecl Boss47_Setup(void) {
    StoreHooks(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// ===========================================================================
// Kinds 43 (Angler) and 50 (Sample 5), set-ups 36 and 43, and the Angler's
// effect task (the kind-3 dispatcher's slot 3)
// ===========================================================================

// original 0x43E7C0: BossKind_Table[43]: jmp [BossAngler_States + 4 * +1] (12:
// the entrance, the generic states, BossAngler_AdvanceDispatch at 4 and
// BossAngler_RetreatDispatch at 5).
extern "C" unsigned long __cdecl BossAngler_Dispatch(unsigned long through) {
    return Dispatch("BossAngler_Dispatch", AddressOf(BossAngler_States), 12, 1, through);
}

// original 0x43E7E0: state 0 - BossAngler_Anims / BossAngler_Sounds and the
// hook BossAngler_Hook.
extern "C" unsigned char __cdecl BossAngler_Enter(void) {
    return Enter(AddressOf(BossAngler_Anims), bof3::addr::BossAngler_Hook, AddressOf(BossAngler_Sounds));
}

// original 0x43E840: state 4 (kinds 43 and 50): jmp [BossAngler_AdvanceSteps +
// 4 * +2] (2).
extern "C" unsigned long __cdecl BossAngler_AdvanceDispatch(unsigned long through) {
    return Dispatch("BossAngler_AdvanceDispatch", AddressOf(BossAngler_AdvanceSteps), 2, 2, through);
}

// original 0x43E860: state 4 step 0. Sprite_Current +9 = the byte +0x8A of the
// area's enemy data record 0x939AD8's +0xF0 names (0x8C5652 + 0x8C * index,
// unchecked); BattleEnemy_SetAnimation(2); BattleEnemy_ScriptTick (its answer
// not read); then (Sprite_Current read again for each) the dword +0x18 = +0x34
// + 0x30000, the dword +0xC = 0x4000, +2 up.
extern "C" void __cdecl BossAngler_AdvanceStart(void) {
    const unsigned record = Enemy()[0xF0];
    Sprite_Current[9] = At(at::kEnemyDataCount9 + 0x8C * record)[0];
    BH_CALL(BattleEnemy_SetAnimation)(2);
    BH_CALL(BattleEnemy_ScriptTick)();
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x18, S(static_cast<U>(Long(s + 0x34)) + 0x30000u));
    SetLong(Sprite_Current + 0xC, 0x4000);
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43E8C0: state 4 step 1. +9 down; at 0, 0x437450(the first word of
// 0x939AD8's +0xF8 sound words). The dword +0x34 += the dword +0xC; when +0x34
// then equals +0x18 (Sprite_Current read again): Battle_SetTargetFlag40(the
// target), 0x4376F0, then +0x18 -= 0x30000, +1 up, +2 = 0. Then a tail jump to
// BattleEnemy_ScriptTick, whose al is the answer.
extern "C" unsigned char __cdecl BossAngler_Advance(void) {
    unsigned char* s = Sprite_Current;
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Sprite_Current;
    if (s[9] == 0) {
        const unsigned sound = Word(At(static_cast<U>(Long(Enemy() + 0xF8))));
        BH_AT(void (__cdecl*)(unsigned), at::kEnemySound)(sound);
        s = Sprite_Current;
    }
    SetLong(s + 0x34, S(static_cast<U>(Long(s + 0x34)) + static_cast<U>(Long(s + 0xC))));
    s = Sprite_Current;
    if (Long(s + 0x34) == Long(s + 0x18)) {
        BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
        BH_AT(Handler, at::kEnemyActChance)();
        unsigned char* const t = Sprite_Current;
        SetLong(t + 0x18, S(static_cast<U>(Long(t + 0x18)) + 0xFFFD0000u));
        Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
        Sprite_Current[2] = 0;
    }
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x43E950: state 5 (kinds 43 and 50): jmp [BossAngler_RetreatSteps +
// 4 * +2] (2).
extern "C" unsigned long __cdecl BossAngler_RetreatDispatch(unsigned long through) {
    return Dispatch("BossAngler_RetreatDispatch", AddressOf(BossAngler_RetreatSteps), 2, 2, through);
}

// original 0x43E970: state 5 step 0. The dword +0x34 -= the dword +0xC; when it
// then equals +0x18 (Sprite_Current read again, and that pointer's +2 up),
// +2 up. Then a tail jump to BattleEnemy_ScriptTick (al the answer).
extern "C" unsigned char __cdecl BossAngler_Retreat(void) {
    unsigned char* s = Sprite_Current;
    SetLong(s + 0x34, S(static_cast<U>(Long(s + 0x34)) - static_cast<U>(Long(s + 0xC))));
    s = Sprite_Current;
    if (Long(s + 0x34) == Long(s + 0x18)) s[2] = static_cast<unsigned char>(s[2] + 1);
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x43E9A0: state 5 step 1. BattleEnemy_ScriptTick (its answer not
// read); the round flags' bit 2 set (the done flag BattleFx_ScriptUntilDone
// waits for); a tail jump to 0x4376A0 (the action's end).
extern "C" void __cdecl BossAngler_RetreatEnd(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) | 4);
    BH_AT(Handler, at::kEnemyActEnd)();
}

// original 0x43E9C0: the +0xF4 hook through BossAngler_Hooks (BareRet, BareRet,
// BossAngler_HookSpawnFx).
extern "C" unsigned long __cdecl BossAngler_Hook(unsigned long word) {
    return HookDispatch("BossAngler_Hook", AddressOf(BossAngler_Hooks), word);
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
    const std::int32_t actor = static_cast<std::int32_t>(B(at::kActor)) - 3;
    unsigned char* const to = At(at::kTasks + at::kTaskStride * slot);
    const unsigned char* const from = At(static_cast<U>(static_cast<std::int32_t>(at::kEnemies) + actor * static_cast<std::int32_t>(at::kEnemyStride)));
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
    return Dispatch("BossSample5_Dispatch", AddressOf(BossSample5_States), 12, 1, through);
}

// original 0x43EA90: state 0 - the Angler's byte tables and its own hook
// BossSample5_Hook.
extern "C" unsigned char __cdecl BossSample5_Enter(void) {
    return Enter(AddressOf(BossAngler_Anims), bof3::addr::BossSample5_Hook, AddressOf(BossAngler_Sounds));
}

// original 0x43EAF0: the +0xF4 hook through BossSample5_Hooks (BareRet,
// BareRet, BossAngler_HookSpawnFx).
extern "C" unsigned long __cdecl BossSample5_Hook(unsigned long word) {
    return HookDispatch("BossSample5_Hook", AddressOf(BossSample5_Hooks), word);
}

// original 0x43EB00: Boss_SetupTable[36]. The hooks: end Boss36_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss36_Setup(void) {
    StoreHooks(bof3::addr::Boss36_End, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// original 0x43EB20: the end hook: won - chapter step 8, step 1; else step 2.
extern "C" void __cdecl Boss36_End(void) { EndWithStep(8); }

// original 0x43EB40: Boss_SetupTable[43]. As set-up 41: BossHook_EndPickWay,
// BareRet, BareRetZero.
extern "C" void __cdecl Boss43_Setup(void) {
    StoreHooks(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// original 0x43EB80: the kind-3 dispatcher's slot 3 (BattleBossFx_Dispatch by
// the slot's +5): jmp [BossAnglerFx_States + 4 * +1] - one entry,
// BossAnglerFx_Run.
extern "C" unsigned long __cdecl BossAnglerFx_Dispatch(unsigned long through) {
    return Dispatch("BossAnglerFx_Dispatch", AddressOf(BossAnglerFx_States), 1, 1, through);
}

// original 0x43EBA0: by Sprite_Current +2 through a three-entry table built on
// the stack - BossAnglerFx_Start, BattleFx_ScriptUntilDone, BattleFx_FreeTask;
// `call [esp + 4 * +2]`, unchecked (past 2 the original calls whatever the
// stack holds above the table: ours aborts). The entry's eax the answer.
extern "C" unsigned long __cdecl BossAnglerFx_Run(void) {
    const unsigned step = Sprite_Current[2];
    using Step = U (__cdecl*)();
    switch (step) {
    case 0: return BH_AT(Step, bof3::addr::BossAnglerFx_Start)();
    case 1: return BH_AT(Step, bof3::addr::BattleFx_ScriptUntilDone)();
    case 2: return BH_CALL(BattleFx_FreeTask)();
    default:
        bof3::Fatal("BossAnglerFx_Run: +2 is %u, past its 3 steps - the original calls through the stack above its table "
                    "(docs/boss_sh.md section 6)",
                    step);
    }
}

// original 0x43EBD0: step 0. Sprite_Current +0xB = 0 and +9 = 0 (read again);
// Sprite_SetAnimation(1); Sprite_ScriptTick and Sprite_QueueOverlay (answers
// not read); +2 up (Sprite_Current read again).
extern "C" void __cdecl BossAnglerFx_Start(void) {
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    BH_CALL(Sprite_SetAnimation)(1);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43EC10: step 1 of the Angler's task and of three spell tasks'
// (magic_s09 / s15 / s31 call it by its address): Sprite_ScriptTick and
// Sprite_QueueOverlay (answers not read); with the round flags' bit 2 (the
// done flag an enemy's action end sets), +2 up.
extern "C" void __cdecl BattleFx_ScriptUntilDone(void) {
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (B(at::kFlags) & 4) Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// ===========================================================================
// Kind 44 (Elder, area 144)
// ===========================================================================

// original 0x43EC30: BossKind_Table[44]: jmp [BossElder_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossElder_Dispatch(unsigned long through) {
    return Dispatch("BossElder_Dispatch", AddressOf(BossElder_States), 12, 1, through);
}

// original 0x43EC50: state 0 - BossElder_Anims / BossElder_Sounds and the hook
// BossElder_Hook.
extern "C" unsigned char __cdecl BossElder_Enter(void) {
    return Enter(AddressOf(BossElder_Anims), bof3::addr::BossElder_Hook, AddressOf(BossElder_Sounds));
}

// original 0x43ECB0: the +0xF4 hook through BossElder_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossElder_Hook(unsigned long word) {
    return HookDispatch("BossElder_Hook", AddressOf(BossElder_Hooks), word);
}

void BossSh_Inject() {
    if (bof3::WantsShadow("boss_sh")) boss_sh::SelfTest();
    BOF3_INJECT(BossSample3_Dispatch);
    BOF3_INJECT(BossSample3_Enter);
    BOF3_INJECT(BossSample3_Hook);
    BOF3_INJECT(Boss34_Setup);
    BOF3_INJECT(Boss34_Event);
    BOF3_INJECT(Boss34_End);
    BOF3_INJECT(Boss34_Exit);
    BOF3_INJECT(Boss41_Setup);
    BOF3_INJECT(BossGaist_Dispatch);
    BOF3_INJECT(BossGaist_Enter);
    BOF3_INJECT(BossGaist_Hook);
    BOF3_INJECT(BossGaist_HookClearBit1);
    BOF3_INJECT(BossTorch_Dispatch);
    BOF3_INJECT(BossTorch_Enter);
    BOF3_INJECT(BossTorch_Hook);
    BOF3_INJECT(BossTorch_HookTarget3);
    BOF3_INJECT(BossSample9_Dispatch);
    BOF3_INJECT(BossSample9_Enter);
    BOF3_INJECT(BossSample9_Hook);
    BOF3_INJECT(Boss35_Setup);
    BOF3_INJECT(Boss35_Event);
    BOF3_INJECT(Boss35_End);
    BOF3_INJECT(Boss47_Setup);
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
    BOF3_INJECT(Boss36_Setup);
    BOF3_INJECT(Boss36_End);
    BOF3_INJECT(Boss43_Setup);
    BOF3_INJECT(BossAnglerFx_Dispatch);
    BOF3_INJECT(BossAnglerFx_Run);
    BOF3_INJECT(BossAnglerFx_Start);
    BOF3_INJECT(BattleFx_ScriptUntilDone);
    BOF3_INJECT(BossElder_Dispatch);
    BOF3_INJECT(BossElder_Enter);
    BOF3_INJECT(BossElder_Hook);
}
