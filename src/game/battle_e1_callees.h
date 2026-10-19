// Internal to battle_e1.cpp and battle_e1_fuzz.cpp: the cells group BE1's
// functions touch that symbols.toml has no name for, the .data tables they
// dispatch through, and the callees nobody owns yet, by raw address.
// docs/battle_e1.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x42E0E0  (): BATE's, Capcom's (catalogue part 7, no group): called as
//             the equipment screen opens; the engine standard set's recorder.
//   0x42E250  (): BATE's, Capcom's: called on the list's confirm.
//   0x42E2F0  () -> al (unread by our caller): BATE's, Capcom's: called each
//             frame of the list.
//   0x5B9450  (dst, src, n): the CRT's strncpy (Crt_strncpy), Capcom's (kThrough).
//   0x494E70  (): BattleEnemy_ClearStates, the eight enemies' +0..+4 zeroed, ours since R3G (kThrough).
//   0x452EB0, 0x452F10  () -> al: a random live member / enemy, R4A's
//             Battle_RandomLiveMember / _RandomLiveEnemy, ours; the engine set answers a flag.
//   0x444660  (): group BE4's (the round's cut) - a draw BE1's 0x42EE00 makes.
//   0x446D90  (slot byte, item word) -> al: group BE4's - an item command
//             given back to the inventory (battle_phases.md section 3).
//   0x44A910  (member byte): group BE4's - the member's name into
//             Text_Records (Str_CopyN of its character record, 8 bytes).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e1 {
namespace at {

using U = std::uint32_t;

// --- BATE, game mode 9 (the byte block 0x929F00; menu-screens.md names the
// field menu's use of the same block) ---
// kMode: now BattleE1_Mode (symbols.toml). u8: BATE's state (BattleExtra_Dispatch 0x42D710 dispatches it through BattleExtra_States 0x64ADAC)
// kModeStep: now FieldS_State (symbols.toml). u8: the state's step
// kModeSub: now FieldS_Step (symbols.toml). u8: the step's sub-step
constexpr U kModeTimer = 0x929F04;   // u8: a frame count the steps count down
// kTallyRow: now FieldS_Column (symbols.toml). s8: the tally counter being counted up (0..5; 6 done)
// kModeByte9: now BattleE1_ModeByte9 (symbols.toml). u8: cleared as the equipment screen opens
// kWaitWord: now MoveScript_WaitWordDA (symbols.toml). u16 MoveScript_WaitWordDA: a transition is running while non-zero
// kMenuShade: now FieldS_Backdrop (symbols.toml). u8: Menu_DrawBackdrop's kind (the config's window colour)
// kBoxColour: now BattleDraw_Colour (symbols.toml). u8: Menu_DrawBox's colour byte

// The tally's six counters: shown (counted up), target, the step per frame.
constexpr U kTallyShown = 0x675E88;  // u32 x 6: what the window shows
constexpr U kTallyTarget = 0x675EA0; // u32 x 6: where each count stops
constexpr U kTallyStep = 0x64ADD0;   // u8 x 6 (.data): each count's step per frame
constexpr U kTallyFmtWide = 0x64ADDC;   // .data: the sprintf format of the first column
constexpr U kTallyFmtNarrow = 0x64ADD8; // .data: the second column's
constexpr U kTallyText = 0x904BA0;   // char[]: the sprintf buffer the tally draws from
constexpr U kEquipOpenBytes = 0x675EBE;  // u8 x 3 (0x675EBE, BF, C0): set as the equipment screen opens

// What the tally adds up (written by fight 26's event hook, boss_se.md).
// kSumA: now BattleE1_SumA (symbols.toml). u32
// kSumB: now BattleE1_SumB (symbols.toml). u32
constexpr U kCount = 0x939A0C;       // u8

// Character record 7 (CharacterRecords 0x903A70 + 7 * 0xA4; char-stats.md's
// fields): the stat words the tally raises, its two equipment bytes, its
// first five bytes (copied in from .data at the screens' ends).
constexpr U kGuest = 0x903EEC;
// kGuestEquipA: now Rest3a_GuestEquip (symbols.toml). +0x12
// kGuestEquipB: now BattleE1_GuestEquipB (symbols.toml). +0x15
constexpr U kGuestHpMax = 0x903F0C;   // +0x20
// kGuestStat24: now BossSe_CarryB4 (symbols.toml). +0x24
// kGuestStat26: now BossSe_CarryB6 (symbols.toml). +0x26
// kGuestHpBase: now BattleE1_GuestHpBase (symbols.toml). +0x40
// kGuestStat44: now BattleE1_GuestStat44 (symbols.toml). +0x44
// kGuestStat46: now BattleE1_GuestStat46 (symbols.toml). +0x46
constexpr U kGuestHeadIn = 0x669CE8;  // .data: 5 bytes copied into the record as the equipment screen opens
// kGuestHeadOut: now BattleE1_GuestHeadOut (symbols.toml). .data: 5 bytes copied into it as BATE leaves

// The window records BATE's equipment screen uses (WindowRecords 0x803160,
// 0x24 each).
constexpr U kWin0 = 0x803160;
constexpr U kWin1 = 0x803184;
constexpr U kWin2 = 0x8031A8;
constexpr U kWin3 = 0x8031CC;
constexpr U kWin4 = 0x8031F0;
// kWin21: now BattleE4_Above (symbols.toml).

// --- the battle engine ---
constexpr U kStep = 0x904AA1;
// kSubStep: now BattleE4_Step2 (symbols.toml).
// kSubStep2: now BattleE4_Step3 (symbols.toml).
// kSubStep3: now BattleE4_Step4 (symbols.toml).
// kMemberAt: now BossSb_Countdown (symbols.toml). u8: the party slot a step walks (the result's, the ability notice's)
// kRoster: now BattleE1_Roster (symbols.toml). u8: that slot's roster index
constexpr U kFlags = 0x904AA8;       // u8: the round flags' low byte
constexpr U kPartyCount = 0x904AB0;  // u8
constexpr U kMenuMember = 0x904AB4;  // u8: the member the command menu is for
constexpr U kEntryFirst = 0x904AB6;  // u8: the entry order's first byte
constexpr U kCrossGrow = 0x904ABC;   // u8 per member: the command cross's size
constexpr U kEntryCount = 0x904AC3;  // s8: the entries in the order
constexpr U kBattleEnd = 0x904AE8;
constexpr U kActor = 0x904B34;
constexpr U kActing = 0x904B3C;      // unsigned char *: the acting sprite
constexpr U kActing2 = 0x904B40;     // unsigned char *: the action record (+2 the ability id)
// kLossBar: now BattleResult_TickStep (symbols.toml). u16: the loss screen's bar width
constexpr U kOldLevel = 0x904B72;    // u16
constexpr U kNewLevel = 0x904B74;    // u16
constexpr U kActionId = 0x904B80;    // u16
// kEffectBits: now BattleActions_Pending (symbols.toml). u16: a bit per actor with an effect still running
// kAutoTest: now BattleFlow_AnimGate (symbols.toml). u8
// kMessageUp: now BattleE1_MessageUp (symbols.toml). u8
constexpr U kBannerText = 0x669E08;  // .data: the banner string pointer kind 3's first step shows
constexpr U kHoldIcons = 0x64E2AC;   // .data: 7 byte pairs, the command icons' offsets
constexpr U kLevelMsgPtr = 0x93B8E4; // const char *: the level-up line (Msg_SystemPtr(7))
// kPartySetByte: now AreaW0b_PartySet (symbols.toml). u8: & 0x7F the party set PartySet_Select loads
constexpr U kAnimBuffer = 0x8C5D80;  // Sprite_AnimFromSet's buffer (0x1800 bytes)
constexpr U kRandomIds24 = 0x64AEC8;   // .data: 32 ability ids, the random pick of action 0x24
constexpr U kRandomIds25 = 0x64AEE8;   // .data: 32, of actions 0x25 and 0x8C
constexpr U kCharExpTable = 0x658F48;  // Char_ExpTable: 99 records of 8 bytes per character

// Named cells symbols.toml types as data (their names are macros there):
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kInputHeld: now Input_Held (symbols.toml). Input_Held (u16)
constexpr U kInputPressed = 0x7E1BEC;  // Input_Pressed (u16)
// kConfirmButtons: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancelButtons: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)

// The party (ObjTrio) and the character records.
// kMembers: now ObjTrio (symbols.toml).
constexpr U kMemberSize = 0x14C;
constexpr U kCharRecords = 0x903A70;
constexpr U kCharSize = 0xA4;

// The loss screen's two CLUT runs greyed on the way out (0x100 words each).
// kClutA: now Gfx_ClutStrip[0x1a00] (symbols.toml).
// kClutB: now Gfx_ClutStrip[0xf00] (symbols.toml).

// --- the .data tables the dispatchers jump through ---
// kEquipSteps: now BattleExtra_EquipSteps (symbols.toml). by 0x929F01: 3 (0x42DC10, 0x42DCD0, 0x42E040)
// kEquipOpenSteps: now BattleExtra_EquipOpenSteps (symbols.toml). by 0x929F02: 3 (BattleExtra_OpenTransition 0x42D780, 0x42DC20, 0x42DC80)
constexpr U kEquipRunSteps = 0x64ADF8;   // by 0x929F02: 2, called (0x42DCF0, 0x42DE50)
// kEquipLeaveSteps: now BattleExtra_EquipLeaveSteps (symbols.toml). by 0x929F02: 2 (0x42E050, 0x42E090)
// kHoldSteps: now BattleHold_Steps (symbols.toml). by 0x904AA2: 3 (0x42EDA0, 0x42EE00, 0x42EEA0)
// kKind3Steps: now BattleAction_Kind3Steps (symbols.toml). by 0x904AA3: 5 (0x42F5F0, 0x42F640, the three ability steps)
// kLossSteps: now BattleLoss_Steps (symbols.toml). by 0x904AA3: 5 (0x432440 .. 0x432750)

// --- the callees nobody owns yet ---
constexpr U kEquipOpenHelper = bof3::addr::BattleExtra_EquipSetupWindows;   // 0x42E0E0, R3A's (round 14)
constexpr U kEquipConfirmHelper = bof3::addr::BattleExtra_EquipCommit;   // 0x42E250, R3A's
constexpr U kEquipFrameHelper = bof3::addr::BattleExtra_EquipRefresh;   // 0x42E2F0, R3A's
constexpr U kEnemiesClear = bof3::addr::BattleEnemy_ClearStates;
constexpr U kSideTargetA = bof3::addr::Battle_RandomLiveMember;   // R4A's (round 14)
constexpr U kSideTargetB = bof3::addr::Battle_RandomLiveEnemy;    // R4A's (round 14)
constexpr U kBe4Draw444660 = bof3::addr::BattleWin_DimScreen;   // BE4's
constexpr U kBe4ReturnItem = bof3::addr::Battle_ReturnItem;   // BE4's
constexpr U kBe4MemberName = bof3::addr::Battle_MemberNameToText;   // BE4's

}  // namespace at
}  // namespace battle_e1
