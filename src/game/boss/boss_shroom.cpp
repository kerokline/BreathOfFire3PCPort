// Moved here by tools/move_funcs.py from boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_shroom.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sj::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using HookFn = void (__cdecl*)(unsigned);

using ForwardBossSj = U (__cdecl*)(U);

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after). The jmp
// leaves the caller's stack word in place and answers what the entry
// answers, so ours hands the word on and answers the entry's eax
// (docs/boss_sc.md section 1.2).
U DispatchBossSj(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<ForwardBossSj>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))(through);
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries, the entry gets the caller's word whole, a Fatal past
// them.
void DispatchHook(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, word, (unsigned)table);
    reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entry state (kinds 59 and 61 alike): 0x939AD8's +0xFC (its
// animation bytes), +0xF4 (its hook) and +0xF8 (its sound words), then its
// +0x114 |= 8 - 0x939AD8 read for each; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSj(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

}  // namespace

// original 0x43FBB0: BossKind_Table[61]. jmp [BossShroom_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossShroom_Dispatch(unsigned long through) {
    return DispatchBossSj("BossShroom_Dispatch", AddressOf(BossShroom_States), 12, 1, through);
}

// original 0x43FBD0: state 0 (BossShroom_Anims, _Hook, _Sounds; +0x114 |= 8;
// +1 = 2; tail Sprite_ScriptTick).
extern "C" unsigned char __cdecl BossShroom_Enter(void) {
    return EnterBossSj(AddressOf(BossShroom_Anims), ::bof3::Code(bof3::addr::BossShroom_Hook), AddressOf(BossShroom_Sounds));
}

// original 0x43FC30: the +0xF4 hook: BossShroom_Hooks (3, BareRet each).
extern "C" void __cdecl BossShroom_Hook(unsigned word) { DispatchHook("BossShroom_Hook", AddressOf(BossShroom_Hooks), word); }

// original 0x43FC40: Boss_SetupTable[54]. The hooks: end Boss54_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss54_Setup(void) {
    StoreHooks(::bof3::Code(bof3::addr::Boss54_End), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43FC60: the end hook. Won: movement-script variable 3 (0x903848)
// = 0x32, a tail jump to 0x446DE0; else to 0x446E00.
extern "C" void __cdecl Boss54_End(void) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kScriptVar3.v)) = 0x32;
        BH_AT(Handler, at::kEndWin.v)();
    } else {
        BH_AT(Handler, at::kEndOther.v)();
    }
}

void BossShroom_Inject() {
    BOF3_INJECT(BossShroom_Dispatch);
    BOF3_INJECT(BossShroom_Enter);
    BOF3_INJECT(BossShroom_Hook);
    BOF3_INJECT(Boss54_Setup);
    BOF3_INJECT(Boss54_End);
}
