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
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_si::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
unsigned char* Owner() { return At(static_cast<U>(Long(At(at::kOwner)))); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using Entry = unsigned long (__cdecl*)(unsigned);
unsigned long CallEntry(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return CallEntry(table, state, word);
}

// mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 * eax]: an enemy's +0xF4
// hook by its word's low byte (3 entries in every table of the group), the
// word itself left for the entry.
unsigned long HookDispatch(const char* who, U table, unsigned word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook word %u is past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, index, (unsigned)table);
    return CallEntry(table, index, word);
}

// A kind's state 0, the part every kind of the group shares: 0x939AD8's (read
// again for each store) +0xFC = its animation bytes, +0xF4 = its hook, +0xF8 =
// its cue words, then its +0x114 |= 8 (the dword), and Sprite_Current +1 = 2.
void EnterStores(U fc, U hook, U f8) {
    SetLong(Enemy() + 0xFC, S(fc));
    SetLong(Enemy() + 0xF4, S(hook));
    SetLong(Enemy() + 0xF8, S(f8));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, S(static_cast<U>(Long(e + 0x114)) | 8u));
    Sprite_Current[1] = 2;
}
// ... and a tail jump to Sprite_ScriptTick, whose al is the answer.
unsigned char Enter(U fc, U hook, U f8) {
    EnterStores(fc, hook, f8);
    return BH_CALL(Sprite_ScriptTick)();
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void Setup(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), S(end));
    SetLong(At(at::kHookExit), S(exit));
    SetLong(At(at::kHookEvent), S(event));
}
// The set-ups whose end is BH's BossHook_EndPickWay (fights 40, 42, 44, 45 -
// four copies of one body): exit BareRet, event BareRetZero.
void SetupPickWay() { Setup(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero); }
// The set-ups with an end of their own: exit BareRet, event BareRetZero.
void SetupOwnEnd(U end) { Setup(end, bof3::addr::BareRet, bof3::addr::BareRetZero); }

// An end hook of five copies (fights 37, 49, 51, 53): with 0x904AE8 bit 1 (the
// win) the chapter's step 0x8034E5 = step and a tail jump to 0x446DE0 (the end
// phase, step 1); otherwise a tail jump to 0x446E00 (step 2).
void EndStep(unsigned step) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = static_cast<unsigned char>(step);
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

}  // namespace

// ===========================================================================
// Set-up 37 (BOSS037, area 144 row 7; its kind 44, Elder, is group BSH's)
// ===========================================================================

// original 0x43ECC0: Boss_SetupTable[37]. End Boss37_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss37_Setup(void) { SetupOwnEnd(bof3::addr::Boss37_End); }

// original 0x43ECE0: the end hook: won, the chapter's step 0x1C and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss37_End(void) { EndStep(0x1C); }

// ===========================================================================
// Kind 45 (Ammonite, area 134) and kind 51 (Sample 6, area 162); set-ups 38
// (BOSS038, area 134 row 7) and 44 (BOSS038 row 6, area 162)
// ===========================================================================

// original 0x43ED00: BossKind_Table[45]. jmp [BossAmmonite_Steps + 4 *
// Sprite_Current +1] (12: its entry, then the generic enemy states).
extern "C" unsigned long __cdecl BossAmmonite_Dispatch(unsigned word) {
    return Dispatch("BossAmmonite_Dispatch", AddressOf(BossAmmonite_Steps), 12, 1, word);
}

// original 0x43ED20: state 0. The stores (BossAmmonite_Anims,
// BossAmmonite_Hook, BossAmmonite_Cues, +0x114 |= 8, +1 = 2); then
// Sprite_ScriptTick is CALLED (its answer dropped); with bit 2 of the chapter's
// flag bits ([0x929ED0], read after the call) set: Battle_CopyEnemyData(the
// current enemy's +5 - 3 as a byte - 0x939AD8 read after EnemyData_FindByTag,
// EnemyData_FindByTag(0x96)). Nothing answered (eax is a callee's leftover;
// BattleEnemy_RunAll reads none).
extern "C" void __cdecl BossAmmonite_Enter(void) {
    EnterStores(AddressOf(BossAmmonite_Anims), bof3::addr::BossAmmonite_Hook, AddressOf(BossAmmonite_Cues));
    BH_CALL(Sprite_ScriptTick)();
    const unsigned char* const bits = At(static_cast<U>(Long(At(0x929ED0))));
    if (BH_CALL(Flags_Test)(bits, 2) == 0) return;
    const unsigned id = BH_CALL(EnemyData_FindByTag)(0x96);
    const unsigned slot = static_cast<unsigned char>(Enemy()[5] - 3);
    BH_CALL(Battle_CopyEnemyData)(slot, id);
}

// original 0x43EDB0: the +0xF4 hook: by the word's low byte through
// BossAmmonite_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossAmmonite_Hook(unsigned word) {
    return HookDispatch("BossAmmonite_Hook", AddressOf(BossAmmonite_Hooks), word);
}

// original 0x43EDC0: BossKind_Table[51]: BossSample6_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample6_Dispatch(unsigned word) {
    return Dispatch("BossSample6_Dispatch", AddressOf(BossSample6_Steps), 12, 1, word);
}

// original 0x43EDE0: state 0: kind 45's two byte tables, its own hook; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample6_Enter(void) {
    return Enter(AddressOf(BossAmmonite_Anims), bof3::addr::BossSample6_Hook, AddressOf(BossAmmonite_Cues));
}

// original 0x43EE40: the +0xF4 hook: BossSample6_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample6_Hook(unsigned word) {
    return HookDispatch("BossSample6_Hook", AddressOf(BossSample6_Hooks), word);
}

// original 0x43EE50: Boss_SetupTable[38]. End Boss38_End, exit BareRet, event
// BareRetZero; the target 0x904B44 = 0x40 (the enemies' side); Sprite_Current
// = enemy 7's object; MagicFx_CenterOnSide (Sprite_Current to the side's
// centre); then, Sprite_Current read after the call, MoveScript_F3Divisor =
// 0x20, Field_Kind2X = the centre's x + (its +0x34 - the centre's x) / 2 and
// Field_Kind2Z = the centre's z + (its +0x38 - the centre's z) / 2 (signed
// 32-bit differences, halved toward zero): the field's kind-2 point midway
// between the fight's centre and the enemies'. Sprite_Current is left at
// enemy 7's object.
extern "C" void __cdecl Boss38_Setup(void) {
    SetupOwnEnd(bof3::addr::Boss38_End);
    B(at::kTarget) = 0x40;
    Sprite_Current = At(at::kEnemy7);
    BH_CALL(MagicFx_CenterOnSide)();
    const unsigned char* const s = Sprite_Current;
    const U cx = static_cast<U>(Long(At(at::kCentreX)));
    SetWord(At(at::kF3Divisor), 0x20);
    SetLong(At(at::kKind2X), S(cx + static_cast<U>(S(static_cast<U>(Long(s + 0x34)) - cx) / 2)));
    const U cz = static_cast<U>(Long(At(at::kCentreZ)));
    SetLong(At(at::kKind2Z), S(cz + static_cast<U>(S(static_cast<U>(Long(s + 0x38)) - cz) / 2)));
}

// original 0x43EED0: the end hook: won, the movement script's variable 3 =
// 5, the chapter's step 0x20, Music_Track = 0x77, step 1; otherwise step 2.
extern "C" void __cdecl Boss38_End(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kScriptVar3) = 5;
        B(at::kChapterStep) = 0x20;
        B(at::kMusicTrack) = 0x77;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

// original 0x43EF00: Boss_SetupTable[44]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss44_Setup(void) { SetupPickWay(); }

// ===========================================================================
// Kind 47 (Sample 2, area 159) and kind 60 (HugeSlug, area 119); set-ups 40
// and 53 (BOSS040, row 7)
// ===========================================================================

// original 0x43EF20: BossKind_Table[47]: BossSample2_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample2_Dispatch(unsigned word) {
    return Dispatch("BossSample2_Dispatch", AddressOf(BossSample2_Steps), 12, 1, word);
}

// original 0x43EF40: state 0: BossSample2_Anims, BossSample2_Hook,
// BossSample2_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample2_Enter(void) {
    return Enter(AddressOf(BossSample2_Anims), bof3::addr::BossSample2_Hook, AddressOf(BossSample2_Cues));
}

// original 0x43EFA0: the +0xF4 hook: BossSample2_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample2_Hook(unsigned word) {
    return HookDispatch("BossSample2_Hook", AddressOf(BossSample2_Hooks), word);
}

// original 0x43EFB0: BossKind_Table[60]: BossHugeSlug_Steps (12) by +1.
extern "C" unsigned long __cdecl BossHugeSlug_Dispatch(unsigned word) {
    return Dispatch("BossHugeSlug_Dispatch", AddressOf(BossHugeSlug_Steps), 12, 1, word);
}

// original 0x43EFD0: state 0: kind 47's two byte tables, its own hook; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossHugeSlug_Enter(void) {
    return Enter(AddressOf(BossSample2_Anims), bof3::addr::BossHugeSlug_Hook, AddressOf(BossSample2_Cues));
}

// original 0x43F030: the +0xF4 hook: BossHugeSlug_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossHugeSlug_Hook(unsigned word) {
    return HookDispatch("BossHugeSlug_Hook", AddressOf(BossHugeSlug_Hooks), word);
}

// original 0x43F040: Boss_SetupTable[40]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss40_Setup(void) { SetupPickWay(); }

// original 0x43F060: Boss_SetupTable[53]. End Boss53_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss53_Setup(void) { SetupOwnEnd(bof3::addr::Boss53_End); }

// original 0x43F080: the end hook: won, the chapter's step 0x26 and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss53_End(void) { EndStep(0x26); }

// ===========================================================================
// Kind 49 (Sample 4, area 161); set-up 42 (BOSS042, row 7)
// ===========================================================================

// original 0x43F0A0: BossKind_Table[49]: BossSample4_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample4_Dispatch(unsigned word) {
    return Dispatch("BossSample4_Dispatch", AddressOf(BossSample4_Steps), 12, 1, word);
}

// original 0x43F0C0: state 0: BossSample4_Anims, BossSample4_Hook,
// BossSample4_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample4_Enter(void) {
    return Enter(AddressOf(BossSample4_Anims), bof3::addr::BossSample4_Hook, AddressOf(BossSample4_Cues));
}

// original 0x43F120: the +0xF4 hook: BossSample4_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample4_Hook(unsigned word) {
    return HookDispatch("BossSample4_Hook", AddressOf(BossSample4_Hooks), word);
}

// original 0x43F130: Boss_SetupTable[42]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss42_Setup(void) { SetupPickWay(); }

// ===========================================================================
// Kind 52 (Sample 7, area 163) and kind 56 (Manmo, area 196); set-ups 45 and
// 49 (BOSS049, row 7)
// ===========================================================================

// original 0x43F150: BossKind_Table[52]: BossSample7_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample7_Dispatch(unsigned word) {
    return Dispatch("BossSample7_Dispatch", AddressOf(BossSample7_Steps), 12, 1, word);
}

// original 0x43F170: state 0: BossSample7_Anims, BossSample7_Hook,
// BossSample7_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample7_Enter(void) {
    return Enter(AddressOf(BossSample7_Anims), bof3::addr::BossSample7_Hook, AddressOf(BossSample7_Cues));
}

// original 0x43F1D0: the +0xF4 hook: BossSample7_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample7_Hook(unsigned word) {
    return HookDispatch("BossSample7_Hook", AddressOf(BossSample7_Hooks), word);
}

// original 0x43F1E0: BossKind_Table[56]: BossManmo_Steps (12) by +1.
extern "C" unsigned long __cdecl BossManmo_Dispatch(unsigned word) {
    return Dispatch("BossManmo_Dispatch", AddressOf(BossManmo_Steps), 12, 1, word);
}

// original 0x43F200: state 0: kind 52's two byte tables, its own hook; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossManmo_Enter(void) {
    return Enter(AddressOf(BossSample7_Anims), bof3::addr::BossManmo_Hook, AddressOf(BossSample7_Cues));
}

// original 0x43F260: the +0xF4 hook: BossManmo_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossManmo_Hook(unsigned word) {
    return HookDispatch("BossManmo_Hook", AddressOf(BossManmo_Hooks), word);
}

// original 0x43F270: Boss_SetupTable[45]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss45_Setup(void) { SetupPickWay(); }

// original 0x43F290: Boss_SetupTable[49]. End Boss49_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss49_Setup(void) { SetupOwnEnd(bof3::addr::Boss49_End); }

// original 0x43F2B0: the end hook: won, the chapter's step 0xA and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss49_End(void) { EndStep(0xA); }

// ===========================================================================
// Kind 57 (Chimera, area 197); set-up 50 (BOSS050, row 7)
// ===========================================================================

// original 0x43F2D0: BossKind_Table[57]: BossChimera_Steps (12) by +1.
extern "C" unsigned long __cdecl BossChimera_Dispatch(unsigned word) {
    return Dispatch("BossChimera_Dispatch", AddressOf(BossChimera_Steps), 12, 1, word);
}

// original 0x43F2F0: state 0: BossChimera_Anims, BossChimera_Hook,
// BossChimera_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossChimera_Enter(void) {
    return Enter(AddressOf(BossChimera_Anims), bof3::addr::BossChimera_Hook, AddressOf(BossChimera_Cues));
}

// original 0x43F350: the +0xF4 hook: BossChimera_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossChimera_Hook(unsigned word) {
    return HookDispatch("BossChimera_Hook", AddressOf(BossChimera_Hooks), word);
}

// original 0x43F360: Boss_SetupTable[50]. End Boss50_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss50_Setup(void) { SetupOwnEnd(bof3::addr::Boss50_End); }

// original 0x43F380: the end hook: won, the chapter's step 0x10, step 1 (a
// call, not a tail jump) and after it Music_Track = 0x8C; otherwise step 2.
extern "C" void __cdecl Boss50_End(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = 0x10;
        BH_AT(Handler, at::kEndWin)();
        B(at::kMusicTrack) = 0x8C;
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

// ===========================================================================
// Kind 58 (Arwan, area 142), its effect task (the kind-3 dispatcher's slot 7)
// and set-up 51 (BOSS051, row 7)
// ===========================================================================

// original 0x43F480: BossKind_Table[58]: BossArwan_Steps (12) by +1 (its entry
// at 0, BossArwan_State4Dispatch at 4, BossArwan_State5Await at 5, the generic
// states elsewhere).
extern "C" unsigned long __cdecl BossArwan_Dispatch(unsigned word) {
    return Dispatch("BossArwan_Dispatch", AddressOf(BossArwan_Steps), 12, 1, word);
}

// original 0x43F4A0: state 0: BossArwan_Anims, BossArwan_Hook, BossArwan_Cues;
// a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossArwan_Enter(void) {
    return Enter(AddressOf(BossArwan_Anims), bof3::addr::BossArwan_Hook, AddressOf(BossArwan_Cues));
}

// original 0x43F500: state 4 (the generic table's turn start EnemyOp_TurnStart 0x4365D0): jmp
// [BossArwan_State4Steps + 4 * Sprite_Current +2] (2: BossArwan_State4Fx,
// BareRet).
extern "C" unsigned long __cdecl BossArwan_State4Dispatch(unsigned word) {
    return Dispatch("BossArwan_State4Dispatch", AddressOf(BossArwan_State4Steps), 2, 2, word);
}

// original 0x43F520: state 4 step 0. Sprite_Current +9 = the +0x8A byte of the
// area's enemy data record the current enemy's +0xF0 indexes (0x8C5652 + 0x8C
// * it); slot n = BattleTask_Create(3, 7) - the kind-3 dispatcher's slot 7,
// BossArwanFx_Dispatch - unchecked; slot n's owner +0x80 = Sprite_Current
// (read after the call), and its +2 up by one (the step then waits on
// BareRet: the task moves the enemy to state 5).
extern "C" void __cdecl BossArwan_State4Fx(void) {
    const unsigned type = Enemy()[0xF0];
    Sprite_Current[9] = At(at::kEnemyDataCount + at::kEnemyDataStride * type)[0];
    const unsigned slot = BH_CALL(BattleTask_Create)(3, 7) & 0xFF;
    if (slot >= at::kTaskCount)
        bof3::Fatal("BossArwan_State4Fx: BattleTask_Create answered slot %u, past the %u - the original stores the owner "
                    "at 0x%X (docs/boss_si.md section 6)",
                    slot, at::kTaskCount, (unsigned)(at::kTaskOwners + at::kTaskStride * slot));
    unsigned char* const s = Sprite_Current;
    SetLong(At(at::kTaskOwners + at::kTaskStride * slot), S(AddressOf(s)));
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x43F580: state 5: once 0x904AA8 has bit 2 (the task's last step,
// MagicFx_DoneAndFree, sets it): 0x4376F0, then a tail jump to 0x4376A0 (the
// turn closed). Otherwise nothing.
extern "C" void __cdecl BossArwan_State5Await(void) {
    if (!(B(at::kFlags) & 4)) return;
    BH_AT(Handler, at::kTurnChance)();
    BH_AT(Handler, at::kTurnClose)();
}

// original 0x43F5A0: the +0xF4 hook: BossArwan_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossArwan_Hook(unsigned word) {
    return HookDispatch("BossArwan_Hook", AddressOf(BossArwan_Hooks), word);
}

// original 0x43F5B0: Boss_SetupTable[51]. End Boss51_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss51_Setup(void) { SetupOwnEnd(bof3::addr::Boss51_End); }

// original 0x43F5D0: the end hook: won, the chapter's step 0xF and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss51_End(void) { EndStep(0xF); }

// original 0x43F5F0: the kind-3 dispatcher's slot 7 (BattleBossFx_Dispatch),
// run by BattleTask_RunAll with Sprite_Current the task slot: call [esp + 4 *
// Sprite_Current +1] through a stack table of four - BossArwanFx_Start,
// BossArwanFx_Count, BossArwanFx_Finish, MagicFx_DoneAndFree -, a Fatal past
// them (the original calls through its caller's frame); the entry's eax back.
extern "C" unsigned long __cdecl BossArwanFx_Dispatch(void) {
    static const U kSteps[] = {at::kFxStart, at::kFxCount, at::kFxFinish, at::kFxDone};
    const unsigned step = Sprite_Current[1];
    if (step >= 4)
        bof3::Fatal("BossArwanFx_Dispatch: step +1 is %u, past the 4 of its stack table - the original calls through its "
                    "caller's frame (docs/boss_si.md section 6)",
                    step);
    return BH_AT(unsigned long (__cdecl*)(), kSteps[step])();
}

// original 0x43F630: the task's step 0. +1 up by one; +9 = the +0x8A byte of
// the enemy data record ENEMY 0's +0xF0 indexes (0x93BA50, whatever enemy owns
// the task); +0xA = 0x2D; then on the owner (0x93B940, made Sprite_Current
// for the rest): +0x24 |= 0x80, the words +0x2E += 0x30 and +0x30 -= 0x10,
// +0x29 = 2, Sprite_SetAnimation(2); Sprite_Current put back.
extern "C" void __cdecl BossArwanFx_Start(void) {
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    Sprite_Current[9] = At(at::kEnemyDataCount + at::kEnemyDataStride * B(at::kEnemy0Type))[0];
    Sprite_Current[0xA] = 0x2D;
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    Sprite_Current[0x24] |= 0x80;
    SetWord(Sprite_Current + 0x2E, (Word(Sprite_Current + 0x2E) + 0x30) & 0xFFFF);
    SetWord(Sprite_Current + 0x30, (Word(Sprite_Current + 0x30) - 0x10) & 0xFFFF);
    Sprite_Current[0x29] = 2;
    BH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current = task;
}

// original 0x43F6C0: the task's step 1. Sprite_ScriptTickOnce on the owner
// (Sprite_Current put back after); the task's +9 counts down to 0, where
// Sound_PlayEffect(0x601) and +9 = 0xFF (on the Sprite_Current of after the
// call), and stays at 0xFF; then +0xA (the Sprite_Current of now) counts down,
// and at 0 the owner (read again for each) +1 = 5, +2 = 0,
// Battle_SetTargetFlag40(the target 0x904B44) and the task's +1 (read after
// the call) up by one.
extern "C" void __cdecl BossArwanFx_Count(void) {
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    BH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = task;
    const unsigned char count = task[9];
    if (count != 0xFF) {
        if (count == 0) {
            BH_CALL(Sound_PlayEffect)(0x601);
            Sprite_Current[9] = 0xFF;
        } else {
            task[9] = static_cast<unsigned char>(count - 1);
        }
    }
    unsigned char* const now = Sprite_Current;
    const unsigned char wait = now[0xA];
    if (wait != 0) {
        now[0xA] = static_cast<unsigned char>(wait - 1);
        return;
    }
    Owner()[1] = 5;
    Owner()[2] = 0;
    BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x43F750: the task's step 2. Sprite_ScriptTickOnce on the owner;
// when it answers al not 0: Sprite_SetAnimation(0), then on the Sprite_Current
// of after the call +0x24 &= 0x7F and +0x29 = 4, and the task's +1 up by one.
// Sprite_Current put back to the task either way.
extern "C" void __cdecl BossArwanFx_Finish(void) {
    unsigned char* const task = Sprite_Current;
    Sprite_Current = Owner();
    if (BH_CALL(Sprite_ScriptTickOnce)() != 0) {
        BH_CALL(Sprite_SetAnimation)(0);
        Sprite_Current[0x24] &= 0x7F;
        Sprite_Current[0x29] = 4;
        task[1] = static_cast<unsigned char>(task[1] + 1);
    }
    Sprite_Current = task;
}

// ===========================================================================

void BossSi_Inject() {
    if (bof3::WantsShadow("boss_si")) boss_si::SelfTest();
    BOF3_INJECT(Boss37_Setup);
    BOF3_INJECT(Boss37_End);
    BOF3_INJECT(BossAmmonite_Dispatch);
    BOF3_INJECT(BossAmmonite_Enter);
    BOF3_INJECT(BossAmmonite_Hook);
    BOF3_INJECT(BossSample6_Dispatch);
    BOF3_INJECT(BossSample6_Enter);
    BOF3_INJECT(BossSample6_Hook);
    BOF3_INJECT(Boss38_Setup);
    BOF3_INJECT(Boss38_End);
    BOF3_INJECT(Boss44_Setup);
    BOF3_INJECT(BossSample2_Dispatch);
    BOF3_INJECT(BossSample2_Enter);
    BOF3_INJECT(BossSample2_Hook);
    BOF3_INJECT(BossHugeSlug_Dispatch);
    BOF3_INJECT(BossHugeSlug_Enter);
    BOF3_INJECT(BossHugeSlug_Hook);
    BOF3_INJECT(Boss40_Setup);
    BOF3_INJECT(Boss53_Setup);
    BOF3_INJECT(Boss53_End);
    BOF3_INJECT(BossSample4_Dispatch);
    BOF3_INJECT(BossSample4_Enter);
    BOF3_INJECT(BossSample4_Hook);
    BOF3_INJECT(Boss42_Setup);
    BOF3_INJECT(BossSample7_Dispatch);
    BOF3_INJECT(BossSample7_Enter);
    BOF3_INJECT(BossSample7_Hook);
    BOF3_INJECT(BossManmo_Dispatch);
    BOF3_INJECT(BossManmo_Enter);
    BOF3_INJECT(BossManmo_Hook);
    BOF3_INJECT(Boss45_Setup);
    BOF3_INJECT(Boss49_Setup);
    BOF3_INJECT(Boss49_End);
    BOF3_INJECT(BossChimera_Dispatch);
    BOF3_INJECT(BossChimera_Enter);
    BOF3_INJECT(BossChimera_Hook);
    BOF3_INJECT(Boss50_Setup);
    BOF3_INJECT(Boss50_End);
    BOF3_INJECT(BossArwan_Dispatch);
    BOF3_INJECT(BossArwan_Enter);
    BOF3_INJECT(BossArwan_State4Dispatch);
    BOF3_INJECT(BossArwan_State4Fx);
    BOF3_INJECT(BossArwan_State5Await);
    BOF3_INJECT(BossArwan_Hook);
    BOF3_INJECT(Boss51_Setup);
    BOF3_INJECT(Boss51_End);
    BOF3_INJECT(BossArwanFx_Dispatch);
    BOF3_INJECT(BossArwanFx_Start);
    BOF3_INJECT(BossArwanFx_Count);
    BOF3_INJECT(BossArwanFx_Finish);
}
