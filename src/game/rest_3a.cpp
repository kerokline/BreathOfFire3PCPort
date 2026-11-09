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
unsigned char* PtrAtRest3a(U cell) { return move_script::At(LRest3a(cell)); }
U Addr(const void* p) { return Low32(p); }
// Sprite_Current, the running task slot (0x93B8C4) and its owner (0x93B940),
// each read afresh where the original reads it.
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Slot() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }
unsigned char* OwnerRest3a() { return PtrAtRest3a(::bof3::Img(at::kTaskOwner.v)); }

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
    return move_script::At(::bof3::Img(ObjTrio_addr) + index * at::kMemberSize);
}

}  // namespace

// ============================================================================
// Area 33's world-map frame (WorldMap33_FrameStates 1..3)
// ============================================================================

// ============================================================================
// BATE (game mode 9): the root and its sub-dispatchers
// ============================================================================

// ============================================================================
// BATE's equipment screen: the three helpers BE1's steps call
// ============================================================================

// ============================================================================
// The battle's end: BattleEnd_Steps[2] (the loss) and [3], ExitSteps[3]
// ============================================================================

// original 0x431540 (BattleEnd_Steps[2], 0x904AE8 bit 0's way): jmp
// [BattleEnd_LossSteps + 4 * byte 0x904AA2], 3 (BattleEnd_LossBanner,
// BattleEnd_LossAwaitLoad, BattleLoss_Dispatch).
extern "C" unsigned long __cdecl BattleEnd_LossDispatch(unsigned long through) {
    return DispatchRest3a("BattleEnd_LossDispatch", ::bof3::Img(BattleEnd_LossSteps_addr), BattleE4_Step2, 3, through);
}

// original 0x431550 (BattleEnd_LossSteps[0]): unless 0x904AE5 bit 0x40 keeps
// the battle's music, Music_FadeOutStop(10) and Music_Play(0xA6, 10);
// Battle_OpenMsgWindow; BattleBanner_Set(0, 2, 0, 0, 0xFF, Msg_SystemPtr(0x11));
// 0x904AA2 + 1 (read after the calls). The win's BattleEnd_WinBegin plays 0xA5.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossBanner(void) {
    if ((EventBattle_Flags & 0x40) == 0) {
        BH_CALL(Music_FadeOutStop)(10);
        BH_CALL(Music_Play)(0xA6, 10);
    }
    BH_CALL(Battle_OpenMsgWindow)();
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(0x11);
    BH_CALL(BattleBanner_Set)(0, 2, 0, 0, 0xFF, reinterpret_cast<const char*>(text));
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x4315A0 (BattleEnd_LossSteps[1]): 0x904AA2 + 1 once File_LoadDone
// answers (its whole eax tested).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossAwaitLoad(void) {
    if (BH_CALL(File_LoadDone)() != 0) BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x4315B0 (BattleEnd_Steps[3]): jmp [BattleEnd_RestoreSteps + 4 *
// byte 0x904AA2], 5 (BattleEnd_TasksBegin, BattleEnd_StartMemberTask,
// BattleEnd_AwaitRestore, BattleEnd_RestoreLoadBank, BattleEnd_RestoreAwaitBank).
extern "C" unsigned long __cdecl BattleEnd_RestoreDispatch(unsigned long through) {
    return DispatchRest3a("BattleEnd_RestoreDispatch", ::bof3::Img(BattleEnd_RestoreSteps_addr), BattleE4_Step2, 5, through);
}

// original 0x431710 (BattleEnd_RestoreSteps[3]): once File_LoadDone answers,
// Snd_LoadBankFile(Game_AreaNumber + 3) (the word zero-extended) and
// 0x904AA2 + 1 (read after the call).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreLoadBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Snd_LoadBankFile)(Game_AreaNumber + 3);
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x431740 (BattleEnd_RestoreSteps[4]): once File_LoadDone answers,
// 0x904AA1 = 4 (BattleEnd_ExitStep) and 0x904AA2 = 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreAwaitBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BattleActions_Step = 4;
    BattleE4_Step2 = 0;
}

// original 0x4318F0 (BattleEnd_ExitSteps[3]): once Transition_Busy is 0,
// Draw_PassFlags = 0 and 0x904AA2 - 1 (back to BattleEnd_Finish).
extern "C" void __cdecl BattleEnd_ExitAwaitFade(void) {
    if (Transition_Busy != 0) return;
    const unsigned char step = BattleE4_Step2;
    Draw_PassFlags = 0;
    BattleE4_Step2 = static_cast<unsigned char>(step - 1);
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
    BH_CALL(Gpu_SetDrawMode)(PtrAtRest3a(::bof3::Img(0x7E0670)), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char actor = B(::bof3::Img(at::kActor.v));
    unsigned colour;
    if (actor < 3)
        colour = (move_script::Word(Slot() + 0x10) * 0x421u) & 0xFFFF;
    else
        colour = (move_script::Word(Slot() + 0x10) << 10) & 0xFFFF;
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
    const U text = B(::bof3::Img(at::kActor.v)) < 3 ? Rest3a_BannerParty : Rest3a_BannerEnemy;
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(text)));
    unsigned char* const s = Slot();
    B(::bof3::Img(0x8031F3)) = 1;
    move_script::SetLong(s + 0xC, 0x100);
    move_script::SetLong(Slot() + 0x10, 0);
    Slot()[9] = 2;
    unsigned char* const t = Slot();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x433020 (state 1): Camera_Distance down by the slot's word +0xC;
// the flash's tile; the slot's +0x10 up 8; MapView_Redraw = 2; +9 counted
// down, at 0 +1 up and +9 = 2.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Rise(void) {
    const unsigned zoom = move_script::Word(Slot() + 0xC);
    Camera_Distance = static_cast<std::uint16_t>(Camera_Distance) - zoom;
    FlashTile();
    unsigned char* s = Slot();
    move_script::SetLong(s + 0x10, static_cast<std::int32_t>(LRest3a(Addr(s + 0x10)) + 8));
    s = Slot();
    MapView_Redraw = 2;
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
    const unsigned zoom = move_script::Word(Slot() + 0xC);
    Camera_Distance = static_cast<std::uint16_t>(Camera_Distance) + zoom;
    FlashTile();
    unsigned char* s = Slot();
    move_script::SetLong(s + 0x10, static_cast<std::int32_t>(LRest3a(Addr(s + 0x10)) - 8));
    s = Slot();
    MapView_Redraw = 2;
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
    at::kTint.b_02 = v;
    at::kTint.b_01 = v;
    at::kTint.b_00 = v;
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
    const unsigned id = rv::As<records::ActionRecord>(PtrAtRest3a(::bof3::Img(at::kAction.v)))->action_id;
    const unsigned char animation = (B(::bof3::Img(at::kAbilityFlags.v) + id * 24) & 2) ? 0 : 2;
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
    B(::bof3::Img(at::kFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kFlags.v)) | 0x20);
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
                    at::kStatusIconCount, (unsigned)at::kStatusIcons.v);
    if (B(::bof3::Img(at::kStatusIcons.v) + icon) == 0xFF) return;
    Sc()[0xB] = icon;
    Sc()[0x29] = 2;
    Sc()[0x25] = 5;
    Sc()[0x26] = 0xF0;
    Sc()[0x24] = 0x80;
    Sc()[0x27] = 0xFF;
    Sc()[0x28] = 0;
    rv::Store<std::uint32_t>(rv::As<records::BattleTask>(Sc())->column, 1);
    Sc()[0x2B] = 0;
    Sc()[0x48] = 0;
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->scale_y, 0x10000);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->scale_x, 0x10000);
    move_script::SetLong(Sc() + 0xC, 0);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->work_18, 0x666);
    Sc()[0xA] = 0x3C;
    BH_CALL(BattleFx_PlaceOverOwner)();
    BH_CALL(Sprite_SetAnimation)(B(::bof3::Img(at::kStatusIcons.v) + icon));
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 2);
}

// original 0x433790 (state 4): with the round flags' bit 2, the owner's actor
// (+5): a member's status byte +0x90, an enemy's +0x92 (the index - 3,
// unchecked: kept, a read); with any of 0x58 set Sprite_Current's +1 down by
// one (back to state 3), else +1 = 0.
extern "C" void __cdecl BattleFx_WatchRecheck(void) {
    if ((B(::bof3::Img(at::kFlags.v)) & 4) == 0) return;
    const unsigned actor = OwnerRest3a()[5];
    const unsigned char status = actor <= 2 ? B(::bof3::Img(at::kMembers.v) + 0x90 + actor * at::kMemberSize)
                                            : B(::bof3::Img(at::kEnemies.v) + 0x92 + (actor - 3) * at::kEnemySize);
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
    OwnerRest3a()[0x48] = 2;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4339D0 (state 1 of slots 11 and 12): the owner's u32 +0x40 down
// 0x2000 a frame while not 0; at 0 its +0 |= 0x40 and +1 up.
extern "C" void __cdecl BattleFxReform_Shrink(void) {
    unsigned char* const o = OwnerRest3a();
    const U scale = static_cast<U>(rv::As<records::SpriteHead>(o)->scale_x);
    if (scale != 0) {
        rv::As<records::SpriteHead>(o)->scale_x = static_cast<std::int32_t>(scale - 0x2000);
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
    const unsigned char* const o = OwnerRest3a();
    const bool plain = rv::As<records::SpriteHead>(o)->column == 0;
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
    unsigned char* const owner = OwnerRest3a();
    unsigned char* const kept = Sc();
    Sprite_Current = owner;
    move_script::SetLong(move_script::At(::bof3::Img(Field_State_addr)), static_cast<std::int32_t>(Addr(MemberForWrite(owner[5], "BattleFxReform_Reload"))));
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(owner[8] + 4));
    BH_CALL(Sprite_ReleaseTint)(MemberForWrite(Sc()[5], "BattleFxReform_Reload"));
    const unsigned palette = Sc()[5];
    MemberForWrite(palette, "BattleFxReform_Reload");   // the palette slot is the member's: past 2 it writes on
    BH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (palette << 6))), 0);
    const unsigned member = Sc()[5];
    BH_CALL(Battle_StatusTint)(move_script::Word(move_script::At(::bof3::Img(ObjTrio_addr) + 0x90 + member * at::kMemberSize)));
    BH_CALL(Sprite_SetClutStp)();
    unsigned char* const field = Field_State;
    rv::As<records::PartyMember>(field)->flags_134 = static_cast<std::int32_t>(rv::As<records::PartyMember>(field)->flags_134 & 0xFFFFFFFEu);
    BH_CALL(BattleParty_RecalcStats)();
    Sprite_Current = kept;
    kept[1] = static_cast<unsigned char>(kept[1] + 1);
}

// original 0x433B20 (slot 11's state 4): the owner's +0 bit 0x40 cleared; its
// u32 +0x40 up 0x2000 a frame to 0x10000; there +0x48 = 0, the member of its
// +5 (the owner read again) loses +0x130 bit 0x2000, and a tail jmp to
// BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Grow(void) {
    unsigned char* o = OwnerRest3a();
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o = OwnerRest3a();
    const U scale = static_cast<U>(rv::As<records::SpriteHead>(o)->scale_x);
    if (scale != 0x10000) {
        rv::As<records::SpriteHead>(o)->scale_x = static_cast<std::int32_t>(scale + 0x2000);
        return;
    }
    o[0x48] = 0;
    unsigned char* const m = MemberForWrite(OwnerRest3a()[5], "BattleFxReform_Grow");
    rv::As<records::PartyMember>(m)->flags_130 = static_cast<std::int32_t>(rv::As<records::PartyMember>(m)->flags_130 & 0xFFFFDFFFu);
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
    unsigned char* const o = OwnerRest3a();
    unsigned char* const m = MemberForWrite(o[5], "BattleFxRestore_Begin");
    rv::As<records::PartyMember>(m)->flags_130 = static_cast<std::int32_t>(rv::As<records::PartyMember>(m)->flags_130 | 0x2000);
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
        const unsigned char form = OwnerRest3a()[8];
        return form == 0 || form == 1;
    };
    if (LRest3a(::bof3::Img(at::kFlags.v)) & 0x8000) {
        if (BH_CALL(Battle_BackupFlagged)() != 0) {
            if (AreaW0b_PartySet == 7) BH_CALL(LoadDatFile)(low() ? 0x122 : 0x126);
            if (AreaW0b_PartySet == 0xD) BH_CALL(LoadDatFile)(low() ? 0x123 : 0x127);
            if (AreaW0b_PartySet == 0xE) BH_CALL(LoadDatFile)(low() ? 0x124 : 0x128);
            if (AreaW0b_PartySet == 0xF) BH_CALL(LoadDatFile)(low() ? 0x125 : 0x129);
        } else {
            const bool first = low();
            const unsigned set = AreaW0b_PartySet;
            BH_CALL(LoadDatFile)(static_cast<int>(set + (first ? 0xFCu : 0x10Fu)));
        }
    } else {
        const bool first = low();
        const unsigned set = AreaW0b_PartySet;
        const U table = LRest3a(first ? ::bof3::Img(Rest3a_RestoreFilesA_addr) : ::bof3::Img(Rest3a_RestoreFilesB_addr));
        if (set >= at::kPartySetCount)
            bof3::Fatal("BattleFxRestore_LoadDat: party set %u, past the %u entries of 0x%X (docs/rest_3a.md section 7)", set,
                        at::kPartySetCount, (unsigned)table);
        BH_CALL(LoadDatFile)(static_cast<int>(move_script::Word(move_script::At(table + set * 2))));
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
    BH_WORD(Eight, kTable[index])(::bof3::CodePlace(kTable[0]), ::bof3::CodePlace(kTable[1]), ::bof3::CodePlace(kTable[2]),
                                  ::bof3::CodePlace(kTable[3]), ::bof3::CodePlace(kTable[4]), ::bof3::CodePlace(kTable[5]),
                                  ::bof3::CodePlace(kTable[6]), ::bof3::CodePlace(kTable[7]));
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
    return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * at::kEnemySize);
}
}  // namespace

// original 0x435A20 (PSX 0x801E247C, call-anchored): enemy (actor & 0xFF) - 3
// made Sprite_Current and 0x939AD8 for BattleEnemy_SetAnimation(animation, the
// whole word handed on); both put back after.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationAs(unsigned actor, unsigned animation) {
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationAs");
    unsigned char* const sprite = Sc();
    records::BattleEnemy* const current = BattleE3_CurrentEnemy;
    Sprite_Current = enemy;
    BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(enemy);
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    BattleE3_CurrentEnemy = current;
    Sprite_Current = sprite;
}

// original 0x435A70 (PSX 0x801E2500, call-anchored): the same with 0x939AD8
// alone (Sprite_Current untouched).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_SetAnimationOf(unsigned actor, unsigned animation) {
    records::BattleEnemy* const current = BattleE3_CurrentEnemy;
    unsigned char* const enemy = EnemyByActor(actor, "BattleEnemy_SetAnimationOf");
    BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(enemy);
    BH_CALL(BattleEnemy_SetAnimation)(animation);
    BattleE3_CurrentEnemy = current;
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
        unsigned char* const record = move_script::At(::bof3::Img(CharacterRecords_addr) + n * at::kCharSize);
        std::memcpy(record, move_script::At(::bof3::Img(Char_DefaultRecords_addr) + n * at::kCharSize), at::kCharSize);
        const std::int32_t level = record[0xA];
        const unsigned divisor = B(::bof3::Img(at::kLevelDivisors.v) + 2 * n);
        if (divisor == 0) bof3::Fatal("NewGame_InitCharacters: record %u's divisor is 0 - the original faults", n);
        const auto derived = static_cast<unsigned char>((level << 4) / static_cast<std::int32_t>(divisor) + B(::bof3::Img(at::kLevelDivisors.v) + 2 * n + 1));
        record[0x4E] = derived;
        record[0x1C] = derived;
        record[0x2E] = derived;
    }
    const unsigned slot = Char_WhelpSlot;
    if (slot >= at::kCharCount)
        bof3::Fatal("NewGame_InitCharacters: Char_WhelpSlot is %u, past the %u records - the original writes on", slot, at::kCharCount);
    std::memcpy(move_script::At(::bof3::Img(CharacterRecords_addr) + slot * at::kCharSize), move_script::At(::bof3::Img(at::kWhelpRecord.v)), at::kCharSize);
}

// ============================================================================

void Rest3A_Inject() {
    if (bof3::WantsShadow("rest_3a")) rest_3a::SelfTest();
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
