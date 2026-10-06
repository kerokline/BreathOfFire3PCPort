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

unsigned char& B(U address) { return At(address)[0]; }
U L(U address) { return static_cast<U>(Long(At(address))); }
unsigned char* PtrAt(U cell) { return At(L(cell)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
// Sprite_Current, the running task slot (0x93B8C4) and its owner (0x93B940),
// each read afresh where the original reads it.
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Slot() { return PtrAt(at::kTaskCurrent); }
unsigned char* Owner() { return PtrAt(at::kTaskOwner); }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after). The jmp leaves the caller's stack word and the
// entry's eax in place: ours hands the word on and answers the entry's eax
// (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3a.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(L(table + 4 * index)))(through);
}

// call [esp + 4 * index] through a table the original builds on its stack:
// the entry by the address the original stores (Phase: Capcom's, or the jmp
// to ours), a Fatal past `n` (the original calls through its own frame above
// the table).
void CallStack(const char* who, const U* table, unsigned n, unsigned index) {
    if (index >= n)
        bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack (docs/rest_3a.md section 7)", who,
                    index, n);
    Phase(table[index])();
}

// A party member by its index, for a write through it: past the three the
// original writes into the window records (docs/rest_3a.md section 7).
unsigned char* MemberForWrite(unsigned index, const char* who) {
    if (index > 2) bof3::Fatal("%s: the member index is %u, past ObjTrio's three - the original writes on past them", who, index);
    return At(at::kMembers + index * at::kMemberSize);
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
    if (S16(s + 0x2E) >= 0x10) s[2] = static_cast<unsigned char>(s[2] + 1);
    Phase(bof3::addr::WorldMap33_FrameShown)();
}

// original 0x4041B0 (WorldMap33_FrameStates[2], and the tail of 0x404180):
// the mode byte 0x9045FA 2 makes +2 = 3; WorldMap_DrawFrame(0x10, word
// +0x2E). The y is pushed in edx with a stale high half; every reader takes
// its low word (world_map.cpp), so ours passes the word sign-extended.
extern "C" void __cdecl WorldMap33_FrameShown(void) {
    if (B(at::kMapMode) == 2) Sc()[2] = 3;
    const short y = S16(Sc() + 0x2E);
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
    if (S16(s + 0x2E) <= -0x30) {
        s[2] = 0;
        s = Sc();
    }
    if (B(at::kMapMode) != 2) {
        s[2] = 1;
        s = Sc();
    }
    BH_CALL(WorldMap_DrawFrame)(0x10, S16(s + 0x2E));
}

// ============================================================================
// BATE (game mode 9): the root and its sub-dispatchers
// ============================================================================

// original 0x42D710 (Mode8_Step5's call): jmp [BattleExtra_States + 4 *
// (dword 0x929F00 & 0xFF)], 4 entries (the run of code pointers to
// BattleExtra_TallySteps; ours aborts past them).
extern "C" unsigned long __cdecl BattleExtra_Dispatch(unsigned long through) {
    return Dispatch("BattleExtra_Dispatch", at::kStates, L(at::kMode) & 0xFF, 4, through);
}

// original 0x42D730 (BattleExtra_States[0]): the state 0x904C9F + 2, the step
// and the sub-step 0.
extern "C" void __cdecl BattleExtra_Start(void) {
    const auto state = static_cast<unsigned char>(B(at::kModeRequest) + 2);
    B(at::kMode) = state;
    B(at::kModeStep) = 0;
    B(at::kModeSub) = 0;
}

// original 0x42D750 (BattleExtra_States[1]): Window_ResetAll, Game_Step + 1.
extern "C" void __cdecl BattleExtra_Leave(void) {
    BH_CALL(Window_ResetAll)();
    SetWord(At(at::kGameStep), Word(At(at::kGameStep)) + 1);
}

// original 0x42D760 (BattleExtra_States[3]): jmp [BattleExtra_TallySteps + 4
// * byte 0x929F01], 3 (BattleExtra_TallyOpenDispatch, BattleExtra_TallyCount,
// BattleExtra_TallyClose).
extern "C" unsigned long __cdecl BattleExtra_TallyDispatch(unsigned long through) {
    return Dispatch("BattleExtra_TallyDispatch", at::kTallySteps, B(at::kModeStep), 3, through);
}

// original 0x42D770 (BattleExtra_TallySteps[0]): jmp [BattleExtra_TallyOpenSteps
// + 4 * byte 0x929F02], 2 (BattleExtra_OpenTransition, BattleExtra_TallyOpen).
extern "C" unsigned long __cdecl BattleExtra_TallyOpenDispatch(unsigned long through) {
    return Dispatch("BattleExtra_TallyOpenDispatch", at::kTallyOpenSteps, B(at::kModeSub), 2, through);
}

// original 0x42D780 (BattleExtra_TallyOpenSteps[0], BattleExtra_EquipOpenSteps[0]):
// Transition_Start(3), the sub-step + 1 (read after the call).
extern "C" void __cdecl BattleExtra_OpenTransition(void) {
    BH_CALL(Transition_Start)(3);
    B(at::kModeSub) = static_cast<unsigned char>(B(at::kModeSub) + 1);
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
    B(0x803161) = 2;
    B(0x803162) = 2;
    B(0x803163) = 2;
    B(0x803160) = 1;
    W16(0x803164, 0x140);
    W16(0x803166, 0x3E);
    for (unsigned i = 0;;) {
        const unsigned char who = B(at::kPartyList + i);
        if (who == 0 || who == 7) {
            B(at::kWin0Cursor) = static_cast<unsigned char>(i);
            break;
        }
        i = (i + 1) & 0xFF;
        if (i >= 3) break;
    }
    SetLong(At(0x803180), static_cast<std::int32_t>(at::kChosen));
    SetLong(At(0x8031A4), static_cast<std::int32_t>(at::kChosen));
    B(0x8031AA) = 4;
    W16(0x8031AE, 0xFFEC);
    W16(0x8031D2, 0xFFEC);
    B(0x8031CE) = 4;
    B(0x803169) = 0;
    B(0x80316B) = 0;
    B(0x80316A) = 0;
    B(0x803168) = 0;
    B(0x80316D) = 0;
    W16(0x803170, 0);
    B(0x803185) = 2;
    B(0x803186) = 3;
    B(0x803187) = 2;
    B(0x803184) = 1;
    W16(0x803188, 0xFF56);
    W16(0x80318A, 0x3E);
    B(0x803190) = 7;
    B(0x80318E) = 0;
    B(0x80318F) = 0xFF;
    B(0x803191) = 1;
    B(0x8031A9) = 2;
    B(0x8031AB) = 2;
    B(0x8031A8) = 1;
    W16(0x8031AC, 0x14);
    W16(0x8031B8, 0);
    W16(0x8031BA, 0x118);
    B(0x8031CD) = 2;
    B(0x8031CF) = 3;
    B(0x8031CC) = 1;
    W16(0x8031D0, 0xA4);
    W16(0x8031DC, 0);
    W16(0x8031DE, 0x80);
    B(0x8031F1) = 2;
    B(0x8031F2) = 1;
    B(0x8031F0) = 0;
}

// original 0x42E250 (BattleExtra_EquipListInput's confirm): for each of the
// six equipment slots, a chosen byte 0x675EB8[i] that is not 0 and not what
// record 7 holds at +0x12 + i: Inventory_Remove(category, chosen, 1) - the
// category 0x64AE20[i] - and Inventory_Add(category, the held byte read again,
// 1) (each with a fourth word 0 the callees do not read), then the chosen
// byte, read again, into the record. Then Char_RecalcStats(record 7).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleExtra_EquipCommit(void) {
    for (unsigned i = 0; i < at::kChosenCount; ++i) {
        const unsigned char chosen = B(at::kChosen + i);
        if (chosen == 0) continue;
        unsigned char* const held = At(at::kGuestEquip + i);
        if (chosen == held[0]) continue;
        BH_CALL(Inventory_Remove)(B(at::kCategories + i), chosen, 1);
        const unsigned char back = held[0];
        BH_CALL(Inventory_Add)(B(at::kCategories + i), back, 1);
        held[0] = B(at::kChosen + i);
    }
    BH_CALL(Char_RecalcStats)(At(at::kGuest));
}

// original 0x42E2F0 (BattleExtra_EquipListInput, each frame): window 13's
// +0xD = Item_EquipMask(window 0's +8, +0xD)'s bit 0 clear; record 7's
// bytes +0x14, +0x12, +0x13, +0x15, +0x17 into 0x675EBA, B8, B9, BB, BC (+0x16
// is not copied; 0x675EBC takes +0x17); then 0x675EB8[window 1's +0xA] =
// window 0's +0xD. al: +0x17 (the last byte loaded; the caller does not read
// it). The cursor indexes unchecked; ours aborts past the six bytes.
extern "C" unsigned char __cdecl BattleExtra_EquipRefresh(void) {
    const unsigned char category = B(at::kWin0Category);
    const unsigned char item = B(at::kWin0Item);
    const unsigned fits = BH_CALL(Item_EquipMask)(category, item);
    const unsigned char e14 = B(at::kGuestEquip + 2);
    const unsigned char e12 = B(at::kGuestEquip + 0);
    B(at::kWin13Dim) = static_cast<unsigned char>(~fits & 1);
    const unsigned char e13 = B(at::kGuestEquip + 1);
    B(at::kChosen + 2) = e14;
    const unsigned slot = B(at::kWin1Slot);
    B(at::kChosen + 0) = e12;
    const unsigned char e15 = B(at::kGuestEquip + 3);
    B(at::kChosen + 1) = e13;
    const unsigned char e17 = B(at::kGuestEquip + 5);
    B(at::kChosen + 3) = e15;
    B(at::kChosen + 4) = e17;
    if (slot >= at::kChosenCount)
        bof3::Fatal("BattleExtra_EquipRefresh: window 1's slot cursor is %u, past the %u chosen bytes - the original writes on "
                    "(docs/rest_3a.md section 7)",
                    slot, at::kChosenCount);
    B(at::kChosen + slot) = item;
    return e17;
}

// ============================================================================
// The battle's end: BattleEnd_Steps[2] (the loss) and [3], ExitSteps[3]
// ============================================================================

// original 0x431540 (BattleEnd_Steps[2], 0x904AE8 bit 0's way): jmp
// [BattleEnd_LossSteps + 4 * byte 0x904AA2], 3 (BattleEnd_LossBanner,
// BattleEnd_LossAwaitLoad, BattleLoss_Dispatch).
extern "C" unsigned long __cdecl BattleEnd_LossDispatch(unsigned long through) {
    return Dispatch("BattleEnd_LossDispatch", at::kLossSteps, B(at::kSubStep), 3, through);
}

// original 0x431550 (BattleEnd_LossSteps[0]): unless 0x904AE5 bit 0x40 keeps
// the battle's music, Music_FadeOutStop(10) and Music_Play(0xA6, 10);
// Battle_OpenMsgWindow; BattleBanner_Set(0, 2, 0, 0, 0xFF, Msg_SystemPtr(0x11));
// 0x904AA2 + 1 (read after the calls). The win's BattleEnd_WinBegin plays 0xA5.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossBanner(void) {
    if ((B(at::kMusicFlags) & 0x40) == 0) {
        BH_CALL(Music_FadeOutStop)(10);
        BH_CALL(Music_Play)(0xA6, 10);
    }
    BH_CALL(Battle_OpenMsgWindow)();
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(0x11);
    BH_CALL(BattleBanner_Set)(0, 2, 0, 0, 0xFF, reinterpret_cast<const char*>(text));
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x4315A0 (BattleEnd_LossSteps[1]): 0x904AA2 + 1 once File_LoadDone
// answers (its whole eax tested).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossAwaitLoad(void) {
    if (BH_CALL(File_LoadDone)() != 0) B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x4315B0 (BattleEnd_Steps[3]): jmp [BattleEnd_RestoreSteps + 4 *
// byte 0x904AA2], 5 (BattleEnd_TasksBegin, BattleEnd_StartMemberTask,
// BattleEnd_AwaitRestore, BattleEnd_RestoreLoadBank, BattleEnd_RestoreAwaitBank).
extern "C" unsigned long __cdecl BattleEnd_RestoreDispatch(unsigned long through) {
    return Dispatch("BattleEnd_RestoreDispatch", at::kRestoreSteps, B(at::kSubStep), 5, through);
}

// original 0x431710 (BattleEnd_RestoreSteps[3]): once File_LoadDone answers,
// Snd_LoadBankFile(Game_AreaNumber + 3) (the word zero-extended) and
// 0x904AA2 + 1 (read after the call).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreLoadBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Snd_LoadBankFile)(Word(At(at::kAreaNumber)) + 3);
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x431740 (BattleEnd_RestoreSteps[4]): once File_LoadDone answers,
// 0x904AA1 = 4 (BattleEnd_ExitStep) and 0x904AA2 = 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreAwaitBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    B(at::kStep) = 4;
    B(at::kSubStep) = 0;
}

// original 0x4318F0 (BattleEnd_ExitSteps[3]): once MoveScript_WaitWordDA is 0,
// Draw_PassFlags = 0 and 0x904AA2 - 1 (back to BattleEnd_Finish).
extern "C" void __cdecl BattleEnd_ExitAwaitFade(void) {
    if (Word(At(at::kWaitWord)) != 0) return;
    const unsigned char step = B(at::kSubStep);
    B(at::kPassFlags) = 0;
    B(at::kSubStep) = static_cast<unsigned char>(step - 1);
}

// ============================================================================
// Battle-task kind 0, slot 2: a flash over the screen
// ============================================================================

namespace {
// The flash's draw: a draw mode (Gpu_GetTPage(0, 0, 0x3C0, 0); the fifth word
// its caller leaves is Gpu_SetDrawMode's last), committed (1, 0xC); then
// BattleWin_DrawTileRgb(0, 0, 0xB, colour, 1): for a party actor (0x904B34 <
// 3) the slot's word +0x10 times 0x421 (grey: the same five bits in r, g and
// b), else that word << 10 (red). The colour is pushed with a high half the
// callee does not read (bits 0..14 only).
void FlashTile() {
    const unsigned page = BH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0);
    BH_CALL(Gpu_SetDrawMode)(PtrAt(0x7E0670), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char actor = B(at::kActor);
    unsigned colour;
    if (actor < 3)
        colour = (Word(Slot() + 0x10) * 0x421u) & 0xFFFF;
    else
        colour = (Word(Slot() + 0x10) << 10) & 0xFFFF;
    BH_CALL(BattleWin_DrawTileRgb)(0, 0, 0xB, static_cast<int>(colour), 1);
}
}  // namespace

// original 0x432F90 (BattleFx_Dispatch's slot 2): by the running slot's +1
// through a table it builds on its stack: BattleFxFlash_Banner,
// BattleFxFlash_Rise, BattleFxFlash_Fall.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxFlash_Banner, bof3::addr::BattleFxFlash_Rise, bof3::addr::BattleFxFlash_Fall};
    CallStack("BattleFxFlash_Dispatch", kTable, 3, Slot()[1]);
}

// original 0x432FC0 (state 0): BattleBanner_Add(1, 0, 0, 0x1E, the string at
// 0x669DFC for a party actor, 0x669E00 for an enemy); window 4's +3 = 1; the
// slot's +0xC = 0x100, +0x10 = 0, +9 = 2, +1 up (the slot read again each).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Banner(void) {
    const U text = B(at::kActor) < 3 ? L(at::kBannerParty) : L(at::kBannerEnemy);
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(text)));
    unsigned char* const s = Slot();
    B(0x8031F3) = 1;
    SetLong(s + 0xC, 0x100);
    SetLong(Slot() + 0x10, 0);
    Slot()[9] = 2;
    unsigned char* const t = Slot();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x433020 (state 1): Camera_Distance down by the slot's word +0xC;
// the flash's tile; the slot's +0x10 up 8; MapView_Redraw = 2; +9 counted
// down, at 0 +1 up and +9 = 2.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Rise(void) {
    const unsigned zoom = Word(Slot() + 0xC);
    SetWord(At(at::kCameraDistance), Word(At(at::kCameraDistance)) - zoom);
    FlashTile();
    unsigned char* s = Slot();
    SetLong(s + 0x10, static_cast<std::int32_t>(L(Addr(s + 0x10)) + 8));
    s = Slot();
    B(at::kRedraw) = 2;
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Slot()[9] = 2;
}

// original 0x4330E0 (state 2): Camera_Distance up by +0xC; the tile; +0x10
// down 8; MapView_Redraw = 2; +9 counted down, at 0 a tail jmp to
// BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Fall(void) {
    const unsigned zoom = Word(Slot() + 0xC);
    SetWord(At(at::kCameraDistance), Word(At(at::kCameraDistance)) + zoom);
    FlashTile();
    unsigned char* s = Slot();
    SetLong(s + 0x10, static_cast<std::int32_t>(L(Addr(s + 0x10)) - 8));
    s = Slot();
    B(at::kRedraw) = 2;
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    BH_CALL(BattleTask_FreeCurrent)();
}

// ============================================================================
// Kind 0, slot 4: a tint over the screen
// ============================================================================

// original 0x4332B0 (BattleFx_Dispatch's slot 4): by the running slot's +1:
// BattleFx_StepReset, BattleFxTint_Brighten, BattleFxTint_Hold.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFx_StepReset, bof3::addr::BattleFxTint_Brighten, bof3::addr::BattleFxTint_Hold};
    CallStack("BattleFxTint_Dispatch", kTable, 3, Slot()[1]);
}

namespace {
// Sprite_Current's +9 into the tint's three bytes (b, g, r as stored), then
// BattleWin_DrawTileTint(0, 0, 0xB).
void TintTile() {
    const unsigned char v = Sc()[9];
    B(at::kTint + 2) = v;
    B(at::kTint + 1) = v;
    B(at::kTint + 0) = v;
    BH_CALL(BattleWin_DrawTileTint)(0, 0, 0xB);
}
}  // namespace

// original 0x433300 (state 1): the tile at +9's shade; +9 up by one, and at
// 0xFF +1 up (Sprite_Current read again).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Brighten(void) {
    TintTile();
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Sc();
    if (s[9] == 0xFF) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433350 (state 2): the tile at +9's shade, held.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Hold(void) { TintTile(); }

// ============================================================================
// Kind 0, slot 5: an animation of bank 0x18
// ============================================================================

// original 0x433380 (BattleFx_Dispatch's slot 5): by Sprite_Current's +1:
// BattleFxAnim_Start, BattleFxAnim_Run, BattleFxAnim_Linger; then, while
// Sprite_Current's +0 has bit 0, Sprite_QueueOverlay.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxAnim_Start, bof3::addr::BattleFxAnim_Run, bof3::addr::BattleFxAnim_Linger};
    CallStack("BattleFxAnim_Dispatch", kTable, 3, Sc()[1]);
    if (Sc()[0] & 1) BH_CALL(Sprite_QueueOverlay)();
}

// original 0x4333C0 (state 0): Sprite_SetAnimationBank(0x18); +0x24 = 0x80,
// +0x2A = 0; animation 0 when the action's ability (the word +2 of the action
// record 0x904B40) has bit 1 in its record's byte +0x15, else 2; +1 up. The
// ability id indexes Ability_Records unchecked (kept: a read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Start(void) {
    BH_CALL(Sprite_SetAnimationBank)(0x18);
    Sc()[0x24] = 0x80;
    Sc()[0x2A] = 0;
    const unsigned id = Word(PtrAt(at::kAction) + 2);
    const unsigned char animation = (B(at::kAbilityFlags + id * 24) & 2) ? 0 : 2;
    BH_CALL(Sprite_SetAnimation)(animation);
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433410 (state 1): once Sprite_ScriptTick answers (al) not 0, +1
// up and +9 = 0x10.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Run(void) {
    if (BH_CALL(Sprite_ScriptTick)() == 0) return;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Sc()[9] = 0x10;
}

// original 0x433430 (state 2): Sprite_ScriptTickOnce; +9 down by one; at 0 the
// round flags |= 0x20 and a tail jmp to BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Linger(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Sc()[9] != 0) return;
    B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) | 0x20);
    BH_CALL(BattleTask_FreeCurrent)();
}

// ============================================================================
// The actor watch's states 1 and 4 (BattleFx_ActorWatch's stack table)
// ============================================================================

// original 0x433550 (state 1): +0xB = 0; i = BattleFx_NextStatusIcon (al);
// unless i is 0xFF and its animation 0x64B048[i] is 0xFF: +0xB = i, the icon
// sprite's fields (+0x29 2, +0x25 5, +0x26 0xF0, +0x24 0x80, +0x27 0xFF, +0x28
// 0, word +0x2C 1, +0x2B 0, +0x48 0, +0x44 and +0x40 0x10000, +0xC 0, +0x18
// 0x666, +0xA 0x3C), BattleFx_PlaceOverOwner, Sprite_SetAnimation(the byte
// read again), +1 up by 2. The table has 16 entries; ours aborts past them.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_WatchIconStart(void) {
    Sc()[0xB] = 0;
    const unsigned char icon = BH_CALL(BattleFx_NextStatusIcon)();
    if (icon == 0xFF) return;
    if (icon >= at::kStatusIconCount)
        bof3::Fatal("BattleFx_WatchIconStart: icon %u, past the %u-entry table 0x%X (docs/rest_3a.md section 7)", icon,
                    at::kStatusIconCount, (unsigned)at::kStatusIcons);
    if (B(at::kStatusIcons + icon) == 0xFF) return;
    Sc()[0xB] = icon;
    Sc()[0x29] = 2;
    Sc()[0x25] = 5;
    Sc()[0x26] = 0xF0;
    Sc()[0x24] = 0x80;
    Sc()[0x27] = 0xFF;
    Sc()[0x28] = 0;
    SetWord(Sc() + 0x2C, 1);
    Sc()[0x2B] = 0;
    Sc()[0x48] = 0;
    SetLong(Sc() + 0x44, 0x10000);
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0xC, 0);
    SetLong(Sc() + 0x18, 0x666);
    Sc()[0xA] = 0x3C;
    BH_CALL(BattleFx_PlaceOverOwner)();
    BH_CALL(Sprite_SetAnimation)(B(at::kStatusIcons + icon));
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 2);
}

// original 0x433790 (state 4): with the round flags' bit 2, the owner's actor
// (+5): a member's status byte +0x90, an enemy's +0x92 (the index - 3,
// unchecked: kept, a read); with any of 0x58 set Sprite_Current's +1 down by
// one (back to state 3), else +1 = 0.
extern "C" void __cdecl BattleFx_WatchRecheck(void) {
    if ((B(at::kFlags) & 4) == 0) return;
    const unsigned actor = Owner()[5];
    const unsigned char status = actor <= 2 ? B(at::kMembers + 0x90 + actor * at::kMemberSize)
                                            : B(at::kEnemies + 0x92 + (actor - 3) * at::kEnemySize);
    unsigned char* const s = Sc();
    if (status & 0x58)
        s[1] = static_cast<unsigned char>(s[1] - 1);
    else
        s[1] = 0;
}

// ============================================================================
// Kind 0, slots 11 and 12: a member shrunk, its DAT loaded, grown back
// ============================================================================

// original 0x433970 (BattleFx_Dispatch's slot 11): by Sprite_Current's +1:
// BattleFxReform_Begin, _Shrink, _LoadDat, _Reload, _Grow.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxReform_Begin, bof3::addr::BattleFxReform_Shrink, bof3::addr::BattleFxReform_LoadDat,
                               bof3::addr::BattleFxReform_Reload, bof3::addr::BattleFxReform_Grow};
    CallStack("BattleFxReform_Dispatch", kTable, 5, Sc()[1]);
}

// original 0x4339B0 (slot 11's state 0): the owner's +0x48 = 2; +1 up.
extern "C" void __cdecl BattleFxReform_Begin(void) {
    Owner()[0x48] = 2;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4339D0 (state 1 of slots 11 and 12): the owner's u32 +0x40 down
// 0x2000 a frame while not 0; at 0 its +0 |= 0x40 and +1 up.
extern "C" void __cdecl BattleFxReform_Shrink(void) {
    unsigned char* const o = Owner();
    const U scale = L(Addr(o + 0x40));
    if (scale != 0) {
        SetLong(o + 0x40, static_cast<std::int32_t>(scale - 0x2000));
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] | 0x40);
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433A00 (slot 11's state 2): LoadDatFile 0x2E8 (the owner's word
// +0x2C 0, its +8 0 or 1), 0x2EA (+0x2C 0, other +8), 0x2E9 / 0x2EB (+0x2C not
// 0); +1 up.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_LoadDat(void) {
    const unsigned char* const o = Owner();
    const bool plain = Word(o + 0x2C) == 0;
    const unsigned char form = o[8];
    const bool low = form == 0 || form == 1;
    BH_CALL(LoadDatFile)(plain ? (low ? 0x2E8 : 0x2EA) : (low ? 0x2E9 : 0x2EB));
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433A50 (slot 11's state 3): once File_LoadDone answers - the
// owner made Sprite_Current (the slot kept) and its member (+5) Field_State;
// Sprite_SetAnimation(the owner's +8 + 4); Sprite_ReleaseTint(the member of
// Sprite_Current's +5), Sprite_LoadPalette(0x80D380 + 0x40 * +5, 0),
// Battle_StatusTint(that member's word +0x90) (Sprite_Current read again for
// each); Sprite_SetClutStp; Field_State's +0x134 bit 0 cleared;
// BattleParty_RecalcStats; Sprite_Current back, and the slot's +1 up. Ours
// aborts on a member index past 2 where it is written through (section 7).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Reload(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    unsigned char* const owner = Owner();
    unsigned char* const kept = Sc();
    Sprite_Current = owner;
    SetLong(At(at::kFieldState), static_cast<std::int32_t>(Addr(MemberForWrite(owner[5], "BattleFxReform_Reload"))));
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(owner[8] + 4));
    BH_CALL(Sprite_ReleaseTint)(MemberForWrite(Sc()[5], "BattleFxReform_Reload"));
    const unsigned palette = Sc()[5];
    MemberForWrite(palette, "BattleFxReform_Reload");   // the palette slot is the member's: past 2 it writes on
    BH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(At(at::kPalettes + (palette << 6))), 0);
    const unsigned member = Sc()[5];
    BH_CALL(Battle_StatusTint)(Word(At(at::kMembers + 0x90 + member * at::kMemberSize)));
    BH_CALL(Sprite_SetClutStp)();
    unsigned char* const field = PtrAt(at::kFieldState);
    SetLong(field + 0x134, static_cast<std::int32_t>(L(Addr(field + 0x134)) & 0xFFFFFFFEu));
    BH_CALL(BattleParty_RecalcStats)();
    Sprite_Current = kept;
    kept[1] = static_cast<unsigned char>(kept[1] + 1);
}

// original 0x433B20 (slot 11's state 4): the owner's +0 bit 0x40 cleared; its
// u32 +0x40 up 0x2000 a frame to 0x10000; there +0x48 = 0, the member of its
// +5 (the owner read again) loses +0x130 bit 0x2000, and a tail jmp to
// BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Grow(void) {
    unsigned char* o = Owner();
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o = Owner();
    const U scale = L(Addr(o + 0x40));
    if (scale != 0x10000) {
        SetLong(o + 0x40, static_cast<std::int32_t>(scale + 0x2000));
        return;
    }
    o[0x48] = 0;
    unsigned char* const m = MemberForWrite(Owner()[5], "BattleFxReform_Grow");
    SetLong(m + 0x130, static_cast<std::int32_t>(L(Addr(m + 0x130)) & 0xFFFFDFFFu));
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x433B80 (BattleFx_Dispatch's slot 12): by Sprite_Current's +1:
// BattleFxRestore_Begin, BattleFxReform_Shrink, BattleFxRestore_LoadDat,
// BattleFx_RestoreParty, BattleFx_RestoreFade.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxRestore_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxRestore_Begin, bof3::addr::BattleFxReform_Shrink, bof3::addr::BattleFxRestore_LoadDat,
                               bof3::addr::BattleFx_RestoreParty, bof3::addr::BattleFx_RestoreFade};
    CallStack("BattleFxRestore_Dispatch", kTable, 5, Sc()[1]);
}

// original 0x433BC0 (slot 12's state 0): the member of the owner's +5 gets
// +0x130 bit 0x2000; the owner's +0x48 = 2; +1 up.
extern "C" void __cdecl BattleFxRestore_Begin(void) {
    unsigned char* const o = Owner();
    unsigned char* const m = MemberForWrite(o[5], "BattleFxRestore_Begin");
    SetLong(m + 0x130, static_cast<std::int32_t>(L(Addr(m + 0x130)) | 0x2000));
    o[0x48] = 2;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433C00 (slot 12's state 2): with the round flags' bit 15 -
// Battle_BackupFlagged (al) not 0: for the party set 0x90412C 7, 0xD, 0xE
// and 0xF (read again for each) LoadDatFile 0x122..0x125 (the owner's +8 0 or
// 1) or 0x126..0x129; al 0: LoadDatFile(set + 0xFC) or (set + 0x10F). Without
// bit 15: LoadDatFile(the word of the party set's entry of the 20 at
// [0x64EA84] (+8 0 or 1) or [0x64EA88]); ours aborts past the 20. +1 up.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxRestore_LoadDat(void) {
    const auto low = [] {
        const unsigned char form = Owner()[8];
        return form == 0 || form == 1;
    };
    if (L(at::kFlags) & 0x8000) {
        if (BH_CALL(Battle_BackupFlagged)() != 0) {
            if (B(at::kPartySet) == 7) BH_CALL(LoadDatFile)(low() ? 0x122 : 0x126);
            if (B(at::kPartySet) == 0xD) BH_CALL(LoadDatFile)(low() ? 0x123 : 0x127);
            if (B(at::kPartySet) == 0xE) BH_CALL(LoadDatFile)(low() ? 0x124 : 0x128);
            if (B(at::kPartySet) == 0xF) BH_CALL(LoadDatFile)(low() ? 0x125 : 0x129);
        } else {
            const bool first = low();
            const unsigned set = B(at::kPartySet);
            BH_CALL(LoadDatFile)(static_cast<int>(set + (first ? 0xFCu : 0x10Fu)));
        }
    } else {
        const bool first = low();
        const unsigned set = B(at::kPartySet);
        const U table = L(first ? at::kRestoreFilesA : at::kRestoreFilesB);
        if (set >= at::kPartySetCount)
            bof3::Fatal("BattleFxRestore_LoadDat: party set %u, past the %u entries of 0x%X (docs/rest_3a.md section 7)", set,
                        at::kPartySetCount, (unsigned)table);
        BH_CALL(LoadDatFile)(static_cast<int>(Word(At(table + set * 2))));
    }
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// ============================================================================
// Kind 3: BattleBossFx_Dispatch
// ============================================================================

// original 0x4357D0 (BattleTask_RunAll's kind 3): by the running slot's +5
// through an eight-entry table it builds on its stack: BareRet,
// Magic002Ball_Task, BossGazerFx_Dispatch, BossAnglerFx_Dispatch,
// BossDLordFx_Dispatch, BossMyriaFx_Dispatch, BossWeretigrFx_Task,
// BossArwanFx_Dispatch. The entry is called with the table under it, so the
// words an entry finds above its return address are the table's (the
// dispatchers among them hand the first on): ours passes the eight.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleBossFx_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BareRet,
                               bof3::addr::Magic002Ball_Task,
                               bof3::addr::BossGazerFx_Dispatch,
                               bof3::addr::BossAnglerFx_Dispatch,
                               bof3::addr::BossDLordFx_Dispatch,
                               bof3::addr::BossMyriaFx_Dispatch,
                               bof3::addr::BossWeretigrFx_Task,
                               bof3::addr::BossArwanFx_Dispatch};
    const unsigned index = Slot()[5];
    if (index >= 8)
        bof3::Fatal("BattleBossFx_Dispatch: index %u, past the 8-entry table the original builds on its stack (docs/rest_3a.md "
                    "section 7)",
                    index);
    using Eight = void (__cdecl*)(U, U, U, U, U, U, U, U);
    BH_AT(Eight, kTable[index])(kTable[0], kTable[1], kTable[2], kTable[3], kTable[4], kTable[5], kTable[6], kTable[7]);
}

// ============================================================================
// An enemy's animation by its battle index
// ============================================================================

namespace {
unsigned char* EnemyByActor(unsigned actor, const char* who) {
    const unsigned index = (actor & 0xFF) - 3u;
    if (index >= 8)
        bof3::Fatal("%s: actor %u is no enemy (3..10) - the original runs BattleEnemy_SetAnimation on a record outside the "
                    "eight (docs/rest_3a.md section 7)",
                    who, actor & 0xFF);
    return At(at::kEnemies + index * at::kEnemySize);
}
}  // namespace

// original 0x435A20 (PSX 0x801E247C, call-anchored): enemy (actor & 0xFF) - 3
// made Sprite_Current and 0x939AD8 for BattleEnemy_SetAnimation(animation, the
// whole word handed on); both put back after.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationAs(unsigned actor, unsigned animation) {
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationAs");
    unsigned char* const sprite = Sc();
    const U current = L(at::kEnemyCurrent);
    Sprite_Current = enemy;
    SetLong(At(at::kEnemyCurrent), static_cast<std::int32_t>(Addr(enemy)));
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    SetLong(At(at::kEnemyCurrent), static_cast<std::int32_t>(current));
    Sprite_Current = sprite;
}

// original 0x435A70 (PSX 0x801E2500, call-anchored): the same with 0x939AD8
// alone (Sprite_Current untouched).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationOf(unsigned actor, unsigned animation) {
    const U current = L(at::kEnemyCurrent);
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationOf");
    SetLong(At(at::kEnemyCurrent), static_cast<std::int32_t>(Addr(enemy)));
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    SetLong(At(at::kEnemyCurrent), static_cast<std::int32_t>(current));
}

// ============================================================================
// New Game
// ============================================================================

// original 0x437820 (TitleFlow_NewGame's call): the seven default records
// (Char_DefaultRecords, 0xA4 bytes each, dword by dword) over CharacterRecords
// 0..6, each followed by one byte derived from its level (+0xA): (level << 4)
// / the pair's divisor (a signed idiv) + its addend, the low byte stored at
// +0x4E, +0x1C and +0x2E; then the whelp's record (0x64B80C) over the record
// Char_WhelpSlot names. DIV-0020's names flow through the records it copies
// (char_names.cpp checks the two instructions that read them, which the
// inject's jmp at 0x437820 leaves in place). Ours aborts on a divisor of 0 and
// on a whelp slot past the eight records (section 7).
extern "C" void __cdecl NewGame_InitCharacters(void) {
    for (unsigned n = 0; n < 7; ++n) {
        unsigned char* const record = At(at::kCharRecords + n * at::kCharSize);
        std::memcpy(record, At(at::kDefaultRecords + n * at::kCharSize), at::kCharSize);
        const std::int32_t level = record[0xA];
        const unsigned divisor = B(at::kLevelDivisors + 2 * n);
        if (divisor == 0) bof3::Fatal("NewGame_InitCharacters: record %u's divisor is 0 - the original faults", n);
        const auto derived = static_cast<unsigned char>((level << 4) / static_cast<std::int32_t>(divisor) + B(at::kLevelDivisors + 2 * n + 1));
        record[0x4E] = derived;
        record[0x1C] = derived;
        record[0x2E] = derived;
    }
    const unsigned slot = B(at::kWhelpSlot);
    if (slot >= at::kCharCount)
        bof3::Fatal("NewGame_InitCharacters: Char_WhelpSlot is %u, past the %u records - the original writes on", slot, at::kCharCount);
    std::memcpy(At(at::kCharRecords + slot * at::kCharSize), At(at::kWhelpRecord), at::kCharSize);
}

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
    BOF3_INJECT(BattleEnd_LossDispatch);
    BOF3_INJECT(BattleEnd_LossBanner);
    BOF3_INJECT(BattleEnd_LossAwaitLoad);
    BOF3_INJECT(BattleEnd_RestoreDispatch);
    BOF3_INJECT(BattleEnd_RestoreLoadBank);
    BOF3_INJECT(BattleEnd_RestoreAwaitBank);
    BOF3_INJECT(BattleEnd_ExitAwaitFade);
    BOF3_INJECT(BattleFxFlash_Dispatch);
    BOF3_INJECT(BattleFxFlash_Banner);
    BOF3_INJECT(BattleFxFlash_Rise);
    BOF3_INJECT(BattleFxFlash_Fall);
    BOF3_INJECT(BattleFxTint_Dispatch);
    BOF3_INJECT(BattleFxTint_Brighten);
    BOF3_INJECT(BattleFxTint_Hold);
    BOF3_INJECT(BattleFxAnim_Dispatch);
    BOF3_INJECT(BattleFxAnim_Start);
    BOF3_INJECT(BattleFxAnim_Run);
    BOF3_INJECT(BattleFxAnim_Linger);
    BOF3_INJECT(BattleFx_WatchIconStart);
    BOF3_INJECT(BattleFx_WatchRecheck);
    BOF3_INJECT(BattleFxReform_Dispatch);
    BOF3_INJECT(BattleFxReform_Begin);
    BOF3_INJECT(BattleFxReform_Shrink);
    BOF3_INJECT(BattleFxReform_LoadDat);
    BOF3_INJECT(BattleFxReform_Reload);
    BOF3_INJECT(BattleFxReform_Grow);
    BOF3_INJECT(BattleFxRestore_Dispatch);
    BOF3_INJECT(BattleFxRestore_Begin);
    BOF3_INJECT(BattleFxRestore_LoadDat);
    BOF3_INJECT(BattleBossFx_Dispatch);
    BOF3_INJECT(BattleEnemy_SetAnimationAs);
    BOF3_INJECT(BattleEnemy_SetAnimationOf);
    BOF3_INJECT(NewGame_InitCharacters);
}
