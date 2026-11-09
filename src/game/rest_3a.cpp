// Round fourteen group R3A (wave three; docs/takeover-queue-round14.md,
// analysis/round14_cut.tsv's R3A rows): 45 functions of the band
// 0x404180..0x4378AA - the cut's 44 and one start it lacks (0x4041B0) - each
// read to its last instruction with capstone (2026-10-04). docs/rest_3a.md has
// every function one row each.
//
//   - area 33's world-map frame states 1..3 (WorldMap33_FrameStates), which
//     our WorldMap_FrameStep carries inline: the table still holds them;
//   - BATE's root (game mode 9's 0x42D710 and its two sub-dispatchers by
//     0x929F00..02, its start, its way out, the transition step) and the
//     equipment screen's three helpers (its windows, the commit, the frame's
//     refresh of the six chosen bytes);
//   - the battle's end: the loss way's dispatcher and two steps
//     (BattleEnd_Steps[2]), the third way's dispatcher and two steps
//     (BattleEnd_Steps[3]), BattleEnd_ExitSteps[3];
//   - battle-task kind 0's slots 2 (a flash), 4 (a tint), 5 (an animation),
//     11 and 12 (a member shrunk, its DAT loaded, grown back) with their
//     states, and the actor watch's states 1 and 4; kind 3's dispatcher
//     (BattleBossFx_Dispatch);
//   - the two engine helpers that run BattleEnemy_SetAnimation on an enemy
//     by its battle index, and New Game's character records.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps or calls
// through whatever follows (round9 doc section 6); the writes through an
// unchecked index abort past what they index (docs/rest_3a.md section 7); the
// reads through one are kept. Every call goes through the boss harness
// (BH_CALL / BH_AT / Phase), so the start-up fuzz can stand recorders in.
#include "game/rest_3a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3a::at;
using U = std::uint32_t;
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
U LRest3a(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
short S16Rest3a(const unsigned char* p) { return static_cast<short>(Word(p)); }
// Sprite_Current, the running task slot (0x93B8C4) and its owner (0x93B940),
// each read afresh where the original reads it.
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Slot() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after). The jmp leaves the caller's stack word and the
// entry's eax in place: ours hands the word on and answers the entry's eax
// (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long DispatchRest3a(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3a.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LRest3a(table + 4 * index)))(through);
}

}  // namespace

// ============================================================================
// Area 33's world-map frame (WorldMap33_FrameStates 1..3)
// ============================================================================

// original 0x404180 (WorldMap33_FrameStates[1]; the case block
// WorldMap_FrameStep carries inline): word +0x2E up 0x10; at 0x10 or more
// (signed, Sprite_Current read again) +2 up; then a tail jmp to state 2.
extern "C" void __cdecl WorldMap33_FrameSlideIn(void) {
    unsigned char* s = Sc();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x10);
    s = Sc();
    if (S16Rest3a(s + 0x2E) >= 0x10) s[2] = static_cast<unsigned char>(s[2] + 1);
    Phase(bof3::addr::WorldMap33_FrameShown)();
}

// original 0x4041B0 (WorldMap33_FrameStates[2], and the tail of 0x404180):
// the mode byte 0x9045FA 2 makes +2 = 3; WorldMap_DrawFrame(0x10, word
// +0x2E). The y is pushed in edx with a stale high half; every reader takes
// its low word (world_map.cpp), so ours passes the word sign-extended.
extern "C" void __cdecl WorldMap33_FrameShown(void) {
    if (AreaW0b_MapMode == 2) Sc()[2] = 3;
    const short y = S16Rest3a(Sc() + 0x2E);
    BH_CALL(WorldMap_DrawFrame)(0x10, y);
}

// original 0x4041E0 (WorldMap33_FrameStates[3]): word +0x2E down 0x10; at
// -0x30 or less +2 = 0; unless the mode byte is 2, +2 = 1 (over the 0); then
// WorldMap_DrawFrame(0x10, word +0x2E) - pushed in eax, the high half the
// object pointer's.
extern "C" void __cdecl WorldMap33_FrameSlideOut(void) {
    unsigned char* s = Sc();
    SetWord(s + 0x2E, Word(s + 0x2E) - 0x10);
    s = Sc();
    if (S16Rest3a(s + 0x2E) <= -0x30) {
        s[2] = 0;
        s = Sc();
    }
    if (AreaW0b_MapMode != 2) {
        s[2] = 1;
        s = Sc();
    }
    BH_CALL(WorldMap_DrawFrame)(0x10, S16Rest3a(s + 0x2E));
}

// ============================================================================
// BATE (game mode 9): the root and its sub-dispatchers
// ============================================================================

// original 0x42D710 (Mode8_Step5's call): jmp [BattleExtra_States + 4 *
// (dword 0x929F00 & 0xFF)], 4 entries (the run of code pointers to
// BattleExtra_TallySteps; ours aborts past them).
extern "C" unsigned long __cdecl BattleExtra_Dispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_Dispatch", ::bof3::Img(BattleExtra_States_addr), LRest3a(::bof3::Img(at::kMode.v)) & 0xFF, 4, through);
}

// original 0x42D730 (BattleExtra_States[0]): the state 0x904C9F + 2, the step
// and the sub-step 0.
extern "C" void __cdecl BattleExtra_Start(void) {
    const auto state = static_cast<unsigned char>(ScenaSc6_Byte904C9F + 2);
    BattleE1_Mode = state;
    FieldS_State = 0;
    FieldS_Step = 0;
}

// original 0x42D750 (BattleExtra_States[1]): Window_ResetAll, Game_Step + 1.
extern "C" void __cdecl BattleExtra_Leave(void) {
    BH_CALL(Window_ResetAll)();
    Game_Step = Game_Step + 1;
}

// original 0x42D760 (BattleExtra_States[3]): jmp [BattleExtra_TallySteps + 4
// * byte 0x929F01], 3 (BattleExtra_TallyOpenDispatch, BattleExtra_TallyCount,
// BattleExtra_TallyClose).
extern "C" unsigned long __cdecl BattleExtra_TallyDispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_TallyDispatch", ::bof3::Img(BattleExtra_TallySteps_addr), FieldS_State, 3, through);
}

// original 0x42D770 (BattleExtra_TallySteps[0]): jmp [BattleExtra_TallyOpenSteps
// + 4 * byte 0x929F02], 2 (BattleExtra_OpenTransition, BattleExtra_TallyOpen).
extern "C" unsigned long __cdecl BattleExtra_TallyOpenDispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_TallyOpenDispatch", ::bof3::Img(BattleExtra_TallyOpenSteps_addr), FieldS_Step, 2, through);
}

// original 0x42D780 (BattleExtra_TallyOpenSteps[0], BattleExtra_EquipOpenSteps[0]):
// Transition_Start(3), the sub-step + 1 (read after the call).
extern "C" void __cdecl BattleExtra_OpenTransition(void) {
    BH_CALL(Transition_Start)(3);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// ============================================================================
// BATE's equipment screen: the three helpers BE1's steps call
// ============================================================================

namespace {
void W16(U address, unsigned v) { SetWord(At(address), v); }
}  // namespace

// original 0x42E0E0 (BattleExtra_EquipOpen's call): window records 0..4 set
// up as the screen's - their in-use, kind and state bytes, positions,
// cursors, counts, the list pointer 0x675EB8 in records 0 and 1 - and record
// 0's +0xC the first of the party list's three entries (0x904062) that is 0
// or 7 (left as it was when none is). Each store as the original orders it.
extern "C" void __cdecl BattleExtra_EquipSetupWindows(void) {
    B(::bof3::Img(0x803161)) = 2;
    B(::bof3::Img(0x803162)) = 2;
    B(::bof3::Img(0x803163)) = 2;
    B(::bof3::Img(0x803160)) = 1;
    W16(::bof3::Img(0x803164), 0x140);
    W16(::bof3::Img(0x803166), 0x3E);
    for (unsigned i = 0;;) {
        const unsigned char who = B(::bof3::Img(at::kPartyList.v) + i);
        if (who == 0 || who == 7) {
            Window0_Cursor = static_cast<unsigned char>(i);
            break;
        }
        i = (i + 1) & 0xFF;
        if (i >= 3) break;
    }
    SetLong(At(::bof3::Img(0x803180)), static_cast<std::int32_t>(::bof3::Img(at::kChosen.v)));
    SetLong(At(::bof3::Img(0x8031A4)), static_cast<std::int32_t>(::bof3::Img(at::kChosen.v)));
    B(::bof3::Img(0x8031AA)) = 4;
    W16(::bof3::Img(0x8031AE), 0xFFEC);
    W16(::bof3::Img(0x8031D2), 0xFFEC);
    B(::bof3::Img(0x8031CE)) = 4;
    B(::bof3::Img(0x803169)) = 0;
    B(::bof3::Img(0x80316B)) = 0;
    Window0_ParamA = 0;
    Window0_Category = 0;
    Window0_Item = 0;
    Window0_Arrows = 0;
    B(::bof3::Img(0x803185)) = 2;
    WindowRecords[1].kind = 3;
    WindowRecords[1].state = 2;
    WindowRecords[1].in_use = 1;
    W16(::bof3::Img(0x803188), 0xFF56);
    W16(::bof3::Img(0x80318A), 0x3E);
    B(::bof3::Img(0x803190)) = 7;
    Window1_ParamA = 0;
    B(::bof3::Img(0x80318F)) = 0xFF;
    Window1_Master = 1;
    B(::bof3::Img(0x8031A9)) = 2;
    WindowRecords[2].state = 2;
    WindowRecords[2].in_use = 1;
    W16(::bof3::Img(0x8031AC), 0x14);
    W16(::bof3::Img(0x8031B8), 0);
    W16(::bof3::Img(0x8031BA), 0x118);
    B(::bof3::Img(0x8031CD)) = 2;
    B(::bof3::Img(0x8031CF)) = 3;
    B(::bof3::Img(0x8031CC)) = 1;
    W16(::bof3::Img(0x8031D0), 0xA4);
    W16(::bof3::Img(0x8031DC), 0);
    W16(::bof3::Img(0x8031DE), 0x80);
    B(::bof3::Img(0x8031F1)) = 2;
    B(::bof3::Img(0x8031F2)) = 1;
    B(::bof3::Img(0x8031F0)) = 0;
}

// original 0x42E250 (BattleExtra_EquipListInput's confirm): for each of the
// six equipment slots, a chosen byte 0x675EB8[i] that is not 0 and not what
// record 7 holds at +0x12 + i: Inventory_Remove(category, chosen, 1) - the
// category 0x64AE20[i] - and Inventory_Add(category, the held byte read again,
// 1) (each with a fourth word 0 the callees do not read), then the chosen
// byte, read again, into the record. Then Char_RecalcStats(record 7).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleExtra_EquipCommit(void) {
    for (unsigned i = 0; i < at::kChosenCount; ++i) {
        const unsigned char chosen = B(::bof3::Img(at::kChosen.v) + i);
        if (chosen == 0) continue;
        unsigned char* const held = At(::bof3::Img(Rest3a_GuestEquip_addr) + i);
        if (chosen == held[0]) continue;
        BH_CALL(Inventory_Remove)(B(::bof3::Img(at::kCategories.v) + i), chosen, 1);
        const unsigned char back = held[0];
        BH_CALL(Inventory_Add)(B(::bof3::Img(at::kCategories.v) + i), back, 1);
        held[0] = B(::bof3::Img(at::kChosen.v) + i);
    }
    BH_CALL(Char_RecalcStats)(At(::bof3::Img(CharacterRecords_addr) + 0x47c));
}

// original 0x42E2F0 (BattleExtra_EquipListInput, each frame): window 13's
// +0xD = Item_EquipMask(window 0's +8, +0xD)'s bit 0 clear; record 7's
// bytes +0x14, +0x12, +0x13, +0x15, +0x17 into 0x675EBA, B8, B9, BB, BC (+0x16
// is not copied; 0x675EBC takes +0x17); then 0x675EB8[window 1's +0xA] =
// window 0's +0xD. al: +0x17 (the last byte loaded; the caller does not read
// it). The cursor indexes unchecked; ours aborts past the six bytes.
extern "C" unsigned char __cdecl BattleExtra_EquipRefresh(void) {
    const unsigned char category = Window0_Category;
    const unsigned char item = Window0_Item;
    const unsigned fits = BH_CALL(Item_EquipMask)(category, item);
    const unsigned char e14 = B(::bof3::Img(at::kGuestEquip.v) + 2);
    const unsigned char e12 = CharacterRecords[7].weapon;
    B(::bof3::Img(at::kWin13Dim.v)) = static_cast<unsigned char>(~fits & 1);
    const unsigned char e13 = B(::bof3::Img(at::kGuestEquip.v) + 1);
    B(::bof3::Img(at::kChosen.v) + 2) = e14;
    const unsigned slot = Window1_ParamA;
    B(::bof3::Img(at::kChosen.v) + 0) = e12;
    const unsigned char e15 = B(::bof3::Img(at::kGuestEquip.v) + 3);
    B(::bof3::Img(at::kChosen.v) + 1) = e13;
    const unsigned char e17 = B(::bof3::Img(at::kGuestEquip.v) + 5);
    B(::bof3::Img(at::kChosen.v) + 3) = e15;
    B(::bof3::Img(at::kChosen.v) + 4) = e17;
    if (slot >= at::kChosenCount)
        bof3::Fatal("BattleExtra_EquipRefresh: window 1's slot cursor is %u, past the %u chosen bytes - the original writes on "
                    "(docs/rest_3a.md section 7)",
                    slot, at::kChosenCount);
    B(::bof3::Img(at::kChosen.v) + slot) = item;
    return e17;
}

// ============================================================================
// The battle's end: BattleEnd_Steps[2] (the loss) and [3], ExitSteps[3]
// ============================================================================

// ============================================================================
// Battle-task kind 0, slot 2: a flash over the screen
// ============================================================================

// ============================================================================
// Kind 0, slot 4: a tint over the screen
// ============================================================================

// ============================================================================
// Kind 0, slot 5: an animation of bank 0x18
// ============================================================================

// ============================================================================
// The actor watch's states 1 and 4 (BattleFx_ActorWatch's stack table)
// ============================================================================

// ============================================================================
// Kind 0, slots 11 and 12: a member shrunk, its DAT loaded, grown back
// ============================================================================

// ============================================================================
// Kind 3: BattleBossFx_Dispatch
// ============================================================================

// ============================================================================
// An enemy's animation by its battle index
// ============================================================================

// ============================================================================
// New Game
// ============================================================================

// ============================================================================

void Rest3A_Inject() {
    if (bof3::WantsShadow("rest_3a")) rest_3a::SelfTest();
    BOF3_INJECT(WorldMap33_FrameSlideIn);
    BOF3_INJECT(WorldMap33_FrameShown);
    BOF3_INJECT(WorldMap33_FrameSlideOut);
    BOF3_INJECT(BattleExtra_Dispatch);
    BOF3_INJECT(BattleExtra_Start);
    BOF3_INJECT(BattleExtra_Leave);
    BOF3_INJECT(BattleExtra_TallyDispatch);
    BOF3_INJECT(BattleExtra_TallyOpenDispatch);
    BOF3_INJECT(BattleExtra_OpenTransition);
    BOF3_INJECT(BattleExtra_EquipSetupWindows);
    BOF3_INJECT(BattleExtra_EquipCommit);
    BOF3_INJECT(BattleExtra_EquipRefresh);
}
