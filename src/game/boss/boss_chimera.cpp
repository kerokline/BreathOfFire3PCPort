// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_chimera.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_si.h"
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

// The set-ups with an end of their own: exit BareRet, event BareRetZero.
void SetupOwnEnd(U end) { SetupBossSi(end, ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

}  // namespace

// original 0x43F2D0: BossKind_Table[57]: BossChimera_Steps (12) by +1.
extern "C" unsigned long __cdecl BossChimera_Dispatch(unsigned word) {
    return DispatchBossSi("BossChimera_Dispatch", AddressOf(BossChimera_Steps), 12, 1, word);
}

// original 0x43F2F0: state 0: BossChimera_Anims, BossChimera_Hook,
// BossChimera_Cues; a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossChimera_Enter(void) {
    return EnterBossSi(AddressOf(BossChimera_Anims), ::bof3::Code(bof3::addr::BossChimera_Hook), AddressOf(BossChimera_Cues));
}

// original 0x43F350: the +0xF4 hook: BossChimera_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossChimera_Hook(unsigned word) {
    return HookDispatchBossSi("BossChimera_Hook", AddressOf(BossChimera_Hooks), word);
}

// original 0x43F360: Boss_SetupTable[50]. End Boss50_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss50_Setup(void) { SetupOwnEnd(::bof3::Code(bof3::addr::Boss50_End)); }

// original 0x43F380: the end hook: won, the chapter's step 0x10, step 1 (a
// call, not a tail jump) and after it Music_Track = 0x8C; otherwise step 2.
extern "C" void __cdecl Boss50_End(void) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = 0x10;
        BH_AT(Handler, at::kEndWin.v)();
        Music_Track = 0x8C;
        return;
    }
    BH_AT(Handler, at::kEndOther.v)();
}

void BossChimera_Inject() {
    BOF3_INJECT(BossChimera_Dispatch);
    BOF3_INJECT(BossChimera_Enter);
    BOF3_INJECT(BossChimera_Hook);
    BOF3_INJECT(Boss50_Setup);
    BOF3_INJECT(Boss50_End);
}
