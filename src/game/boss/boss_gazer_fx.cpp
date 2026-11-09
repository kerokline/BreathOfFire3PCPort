// Moved here by tools/move_funcs.py from boss_sf.cpp (docs/source-layout-round.md).
#include "game/boss/boss_gazer_fx.h"

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

using move_script::SetWord;

using move_script::Word;

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
unsigned char* Owner() { return At(BattleE2_Owner); }

U Key(const void* p) { return Low32(p); }

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

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/boss_sf.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    if (n >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/boss_sf.md section 6)",
                    who, n, n);
    return At(::bof3::Img(BattleTask_Slots_addr) + n * at::kTaskStride);
}

// The Gazer effect's move: +0x3E (a word) up by the word +0x14, then the
// dword +0x14 up by +0x20 - Sprite_Current read for each - then the ground
// at (+0x34, +0x38) (AreaMap_Elevation's low word, signed).
std::int32_t Move() {
    unsigned char* s = Sprite_Current;
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    s = Sprite_Current;
    SetCell(s + 0x14, static_cast<U>(Long(s + 0x14)) + static_cast<U>(Long(s + 0x20)));
    s = Sprite_Current;
    return static_cast<std::int16_t>(BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
}

std::int32_t Height() { return static_cast<std::int16_t>(Word(Sprite_Current + 0x3E)); }

}  // namespace

// original 0x43CA20: jmp [BossGazerFx_States + 4 * Sprite_Current +1] (2).
extern "C" unsigned __cdecl BossGazerFx_Dispatch(unsigned passed) {
    return DispatchBossSf("BossGazerFx_Dispatch", AddressOf(BossGazerFx_States), 2, 1, passed);
}

// original 0x43CA40: state 0 - call [BossGazerFx_BounceSteps + 4 * +2] (4);
// then with Sprite_Current (read again) bit 0, a tail jump to
// Sprite_UpdateScreen.
extern "C" void __cdecl BossGazerFx_BounceDispatch(void) {
    Step("BossGazerFx_BounceDispatch", AddressOf(BossGazerFx_BounceSteps), 4, 2);
    if (rv::Current<records::BattleTask>()->flags & 1) BH_CALL(Sprite_UpdateScreen)();
}

// original 0x43CA70: bounce step 0 - +0x24, +0x2A, +0x48 = 0, +0x29 = 2;
// Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(4); from the owner
// 0x93B940 (read for each): the word +0x36 + 7, the dword +0x38, the word +0x3E
// + 0x1000; the dword +0x14 = 0, +0x20 = -0x20, +9 = 6 (the bounces). Then
// BattleTask_Create(3, 2) - a second task of the same kind at state 1 (its
// +1 = 1) owned by this one (+0x80 = Sprite_Current, read after the call) -
// and +2 up by one.
extern "C" void __cdecl BossGazerFx_BounceStart(void) {
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x29] = 2;
    BH_CALL(Sprite_SetAnimationBank)(0x15C);
    BH_CALL(Sprite_SetAnimation)(4);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->x_whole, Word(Owner() + 0x36) + 7);
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->z, Long(Owner() + 0x38));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, Word(Owner() + 0x3E) + 0x1000);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0xFFFFFFE0u);
    Sprite_Current[9] = 6;
    unsigned char* const task = TaskSlot("BossGazerFx_BounceStart", BH_CALL(BattleTask_Create)(3, 2) & 0xFF);
    auto* const cur_task = rv::As<records::BattleTask>(task);
    unsigned char* const s = Sprite_Current;
    cur_task->state = 1;
    SetCell(task + 0x80, Key(s));
    s[2] += 1;
}

// original 0x43CB50: bounce step 1 - the move (+0x3E by +0x14, +0x14 by +0x20);
// while the signed +0x3E is at or above the ground + 0x200, nothing more
// (control F79 refused > for >=). Else, once it is below, the
// ground asked again: +0x3E = it + 0x200 (a word), +0x14 = 0, +0x20 = 0x40,
// Sound_PlayById(0x600); then (Sprite_Current read again) +9, when not 0, down
// by one and +2 up by one (step 2); at 0, +2 up by two (step 3).
extern "C" void __cdecl BossGazerFx_Bounce(void) {
    const std::int32_t limit = Move() + 0x200;
    unsigned char* s = Sprite_Current;
    records::BattleTask* cur_task = rv::As<records::BattleTask>(s);
    if (Height() >= limit) return;
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(cur_task->x, cur_task->z));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, ground + 0x200);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0x40);
    BH_CALL(Sound_PlayById)(0x600);
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    if (s[9] != 0) {
        s[9] -= 1;
        Sprite_Current[2] += 1;
        return;
    }
    s[2] += 2;
}

// original 0x43CC00: bounce step 2 - the move; while the signed +0x3E is at or
// below the ground + 0x400, nothing more. Else the ground asked again: +0x3E =
// it + 0x400, +0x14 = 0, +0x20 = -0x40, +2 down by one (step 1 again).
extern "C" void __cdecl BossGazerFx_BounceBack(void) {
    const std::int32_t limit = Move() + 0x400;
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    if (Height() <= limit) return;
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(cur_task->x, cur_task->z));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, ground + 0x400);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0xFFFFFFC0u);
    Sprite_Current[2] -= 1;
}

// original 0x43CC90: bounce step 3 - the move; while the signed +0x3E is at or
// below the ground + 0x1000, nothing more. Else the owner (0x93B940, read for
// each) +1 = 5 and +2 = 0 - the Gazer to its state 5 - and a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BossGazerFx_Leave(void) {
    const std::int32_t limit = Move() + 0x1000;
    if (Height() <= limit) return;
    Owner()[1] = 5;
    Owner()[2] = 0;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x43CD00: state 1 - call [BossGazerFx_MarkSteps + 4 * +2] (3); then
// with Sprite_Current (read again) bit 0, a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl BossGazerFx_MarkDispatch(void) {
    Step("BossGazerFx_MarkDispatch", AddressOf(BossGazerFx_MarkSteps), 3, 2);
    if (rv::Current<records::BattleTask>()->flags & 1) BH_CALL(Sprite_UpdateScreen)();
}

// original 0x43CD30: mark step 0 - +0x24, +0x2A, +0x48 = 0, +0x29 = 5;
// Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(6); the owner's dwords
// +0x34 and +0x38 (0x93B940 and Sprite_Current read for each); +0x3E = the
// ground there (a word); +2 up by one.
extern "C" void __cdecl BossGazerFx_MarkStart(void) {
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x29] = 5;
    BH_CALL(Sprite_SetAnimationBank)(0x15C);
    BH_CALL(Sprite_SetAnimation)(6);
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->x, Long(Owner() + 0x34));
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->z, Long(Owner() + 0x38));
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(cur_task->x, cur_task->z));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, ground);
    Sprite_Current[2] += 1;
}

// original 0x43CDC0: mark step 1 - once the owner's +0 bit 0 is clear (the
// bouncing task freed), +2 up by one (step 2: BattleFx_FreeTask); a tail jump
// to Sprite_ScriptTick, whose al is the answer.
extern "C" unsigned char __cdecl BossGazerFx_MarkWait(void) {
    if ((Owner()[0] & 1) == 0) Sprite_Current[2] += 1;
    return BH_CALL(Sprite_ScriptTick)();
}

void BossGazerFx_Inject() {
    BOF3_INJECT(BossGazerFx_Dispatch);
    BOF3_INJECT(BossGazerFx_BounceDispatch);
    BOF3_INJECT(BossGazerFx_BounceStart);
    BOF3_INJECT(BossGazerFx_Bounce);
    BOF3_INJECT(BossGazerFx_BounceBack);
    BOF3_INJECT(BossGazerFx_Leave);
    BOF3_INJECT(BossGazerFx_MarkDispatch);
    BOF3_INJECT(BossGazerFx_MarkStart);
    BOF3_INJECT(BossGazerFx_MarkWait);
}
