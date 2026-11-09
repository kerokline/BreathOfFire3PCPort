// Moved here by tools/move_funcs.py from boss_sh.cpp (docs/source-layout-round.md).
#include "game/boss/boss_gaist.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sh.h"
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

using move_script::At;

using move_script::Long;

using move_script::SetWord;

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

// original 0x43E540: BossKind_Table[41]: jmp [BossGaist_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossGaist_Dispatch(unsigned long through) {
    return DispatchBossSh("BossGaist_Dispatch", AddressOf(BossGaist_States), 12, 1, through);
}

// original 0x43E560: state 0 - BossGaist_Anims / BossGaist_Sounds and the hook
// BossGaist_Hook.
extern "C" unsigned char __cdecl BossGaist_Enter(void) {
    return EnterBossSh(AddressOf(BossGaist_Anims), ::bof3::Code(bof3::addr::BossGaist_Hook), AddressOf(BossGaist_Sounds));
}

// original 0x43E5C0: the +0xF4 hook through BossGaist_Hooks (BareRet, BareRet,
// BossGaist_HookClearBit1).
extern "C" unsigned long __cdecl BossGaist_Hook(unsigned long word) {
    return HookDispatchBossSh("BossGaist_Hook", AddressOf(BossGaist_Hooks), word);
}

// original 0x43E5D0: hook entry 2 (BattleEnemy_RunAll's call): with the round
// flags' bit 1 set and the word Transition_Busy 0, the bit cleared (a
// word and) and Draw_PassFlags = 0. The word is not read.
extern "C" void __cdecl BossGaist_HookClearBit1(unsigned) {
    if ((B(::bof3::Img(at::kFlags.v)) & 2) == 0) return;
    if (Transition_Busy != 0) return;
    SetWord(At(::bof3::Img(at::kFlags.v)), Word(At(::bof3::Img(at::kFlags.v))) & 0xFFFD);
    Draw_PassFlags = 0;
}

// original 0x43E600: BossKind_Table[42]: jmp [BossTorch_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossTorch_Dispatch(unsigned long through) {
    return DispatchBossSh("BossTorch_Dispatch", AddressOf(BossTorch_States), 12, 1, through);
}

// original 0x43E620: state 0 - +0xFC 0x675F0C (twelve bytes of .data nothing
// in the exe writes), +0xF8 0x64D7A0 (0xFFFF words), the hook BossTorch_Hook.
extern "C" unsigned char __cdecl BossTorch_Enter(void) {
    return EnterBossSh(::bof3::Img(at::kTorchAnims.v), ::bof3::Code(bof3::addr::BossTorch_Hook), ::bof3::Img(BossTorch_Sounds_addr));
}

// original 0x43E680: the +0xF4 hook through BossTorch_Hooks
// (BossTorch_HookTarget3, BareRet, BareRet).
extern "C" unsigned long __cdecl BossTorch_Hook(unsigned long word) {
    return HookDispatchBossSh("BossTorch_Hook", AddressOf(BossTorch_Hooks), word);
}

// original 0x43E690: hook entry 0 (the action pick's call): the target
// 0x904B44 = 3 (enemy 0). The word is not read.
extern "C" void __cdecl BossTorch_HookTarget3(unsigned) { B(::bof3::Img(at::kTarget.v)) = 3; }

// original 0x43E6A0: BossKind_Table[54]: jmp [BossSample9_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossSample9_Dispatch(unsigned long through) {
    return DispatchBossSh("BossSample9_Dispatch", AddressOf(BossSample9_States), 12, 1, through);
}

// original 0x43E6C0: state 0 - Gaist's byte tables and its own hook
// BossSample9_Hook.
extern "C" unsigned char __cdecl BossSample9_Enter(void) {
    return EnterBossSh(AddressOf(BossGaist_Anims), ::bof3::Code(bof3::addr::BossSample9_Hook), AddressOf(BossGaist_Sounds));
}

// original 0x43E720: the +0xF4 hook through BossSample9_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample9_Hook(unsigned long word) {
    return HookDispatchBossSh("BossSample9_Hook", AddressOf(BossSample9_Hooks), word);
}

void BossGaist_Inject() {
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
}
