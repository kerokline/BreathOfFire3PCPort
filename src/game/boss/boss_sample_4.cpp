// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_sample_4.h"

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

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

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

// original 0x43F0A0: BossKind_Table[49]: BossSample4_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample4_Dispatch(unsigned word) {
    return DispatchBossSi("BossSample4_Dispatch", AddressOf(BossSample4_Steps), 12, 1, word);
}

// original 0x43F0C0: state 0: BossSample4_Anims, BossSample4_Hook,
// BossSample4_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample4_Enter(void) {
    return EnterBossSi(AddressOf(BossSample4_Anims), ::bof3::Code(bof3::addr::BossSample4_Hook), AddressOf(BossSample4_Cues));
}

// original 0x43F120: the +0xF4 hook: BossSample4_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample4_Hook(unsigned word) {
    return HookDispatchBossSi("BossSample4_Hook", AddressOf(BossSample4_Hooks), word);
}

// original 0x43F130: Boss_SetupTable[42]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss42_Setup(void) { SetupPickWay(); }

// original 0x43F150: BossKind_Table[52]: BossSample7_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample7_Dispatch(unsigned word) {
    return DispatchBossSi("BossSample7_Dispatch", AddressOf(BossSample7_Steps), 12, 1, word);
}

// original 0x43F170: state 0: BossSample7_Anims, BossSample7_Hook,
// BossSample7_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample7_Enter(void) {
    return EnterBossSi(AddressOf(BossSample7_Anims), ::bof3::Code(bof3::addr::BossSample7_Hook), AddressOf(BossSample7_Cues));
}

// original 0x43F1D0: the +0xF4 hook: BossSample7_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample7_Hook(unsigned word) {
    return HookDispatchBossSi("BossSample7_Hook", AddressOf(BossSample7_Hooks), word);
}

// original 0x43F1E0: BossKind_Table[56]: BossManmo_Steps (12) by +1.
extern "C" unsigned long __cdecl BossManmo_Dispatch(unsigned word) {
    return DispatchBossSi("BossManmo_Dispatch", AddressOf(BossManmo_Steps), 12, 1, word);
}

// original 0x43F200: state 0: kind 52's two byte tables, its own hook; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossManmo_Enter(void) {
    return EnterBossSi(AddressOf(BossSample7_Anims), ::bof3::Code(bof3::addr::BossManmo_Hook), AddressOf(BossSample7_Cues));
}

// original 0x43F260: the +0xF4 hook: BossManmo_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossManmo_Hook(unsigned word) {
    return HookDispatchBossSi("BossManmo_Hook", AddressOf(BossManmo_Hooks), word);
}

// original 0x43F270: Boss_SetupTable[45]. End BossHook_EndPickWay, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss45_Setup(void) { SetupPickWay(); }

// original 0x43F290: Boss_SetupTable[49]. End Boss49_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss49_Setup(void) { SetupOwnEnd(::bof3::Code(bof3::addr::Boss49_End)); }

// original 0x43F2B0: the end hook: won, the chapter's step 0xA and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss49_End(void) { EndStepBossSi(0xA); }

void BossSample4_Inject() {
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
}
