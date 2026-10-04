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
//   0x5B9450  (dst, src, n): the CRT's memcpy, Capcom's (kThrough).
//   0x494E70  (): BattleEnemy_ClearStates, the eight enemies' +0..+4 zeroed, ours since R3G (kThrough).
//   0x452EB0, 0x452F10  () -> al: the default targets of a side, Capcom's
//             (no group); the engine set answers a flag.
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
constexpr U kMode = 0x929F00;        // u8: BATE's state (0x42D710 dispatches it through 0x64ADAC)
constexpr U kModeStep = 0x929F01;    // u8: the state's step
constexpr U kModeSub = 0x929F02;     // u8: the step's sub-step
constexpr U kModeTimer = 0x929F04;   // u8: a frame count the steps count down
constexpr U kTallyRow = 0x929F08;    // s8: the tally counter being counted up (0..5; 6 done)
constexpr U kModeByte9 = 0x929F09;   // u8: cleared as the equipment screen opens
constexpr U kWaitWord = 0x66C810;    // u16 MoveScript_WaitWordDA: a transition is running while non-zero
constexpr U kMenuShade = 0x903A5B;   // u8: Menu_DrawBackdrop's kind (the config's window colour)
constexpr U kBoxColour = 0x903A5A;   // u8: Menu_DrawBox's colour byte

// The tally's six counters: shown (counted up), target, the step per frame.
constexpr U kTallyShown = 0x675E88;  // u32 x 6: what the window shows
constexpr U kTallyTarget = 0x675EA0; // u32 x 6: where each count stops
constexpr U kTallyStep = 0x64ADD0;   // u8 x 6 (.data): each count's step per frame
constexpr U kTallyFmtWide = 0x64ADDC;   // .data: the sprintf format of the first column
constexpr U kTallyFmtNarrow = 0x64ADD8; // .data: the second column's
constexpr U kTallyText = 0x904BA0;   // char[]: the sprintf buffer the tally draws from
constexpr U kEquipOpenBytes = 0x675EBE;  // u8 x 3 (0x675EBE, BF, C0): set as the equipment screen opens

// What the tally adds up (written by fight 26's event hook, boss_se.md).
constexpr U kSumA = 0x939A04;        // u32
constexpr U kSumB = 0x939A08;        // u32
constexpr U kCount = 0x939A0C;       // u8

// Character record 7 (CharacterRecords 0x903A70 + 7 * 0xA4; char-stats.md's
// fields): the stat words the tally raises, its two equipment bytes, its
// first five bytes (copied in from .data at the screens' ends).
constexpr U kGuest = 0x903EEC;
constexpr U kGuestEquipA = 0x903EFE;  // +0x12
constexpr U kGuestEquipB = 0x903F01;  // +0x15
constexpr U kGuestHpMax = 0x903F0C;   // +0x20
constexpr U kGuestStat24 = 0x903F10;  // +0x24
constexpr U kGuestStat26 = 0x903F12;  // +0x26
constexpr U kGuestHpBase = 0x903F2C;  // +0x40
constexpr U kGuestStat44 = 0x903F30;  // +0x44
constexpr U kGuestStat46 = 0x903F32;  // +0x46
constexpr U kGuestHeadIn = 0x669CE8;  // .data: 5 bytes copied into the record as the equipment screen opens
constexpr U kGuestHeadOut = 0x669CE0; // .data: 5 bytes copied into it as BATE leaves

// The window records BATE's equipment screen uses (WindowRecords 0x803160,
// 0x24 each).
constexpr U kWin0 = 0x803160;
constexpr U kWin1 = 0x803184;
constexpr U kWin2 = 0x8031A8;
constexpr U kWin3 = 0x8031CC;
constexpr U kWin4 = 0x8031F0;
constexpr U kWin21 = 0x803454;

// --- the battle engine ---
constexpr U kStep = 0x904AA1;
constexpr U kSubStep = 0x904AA2;
constexpr U kSubStep2 = 0x904AA3;
constexpr U kSubStep3 = 0x904AA4;
constexpr U kMemberAt = 0x904AA5;    // u8: the party slot a step walks (the result's, the ability notice's)
constexpr U kRoster = 0x904AA7;      // u8: that slot's roster index
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
constexpr U kLossBar = 0x904B70;     // u16: the loss screen's bar width
constexpr U kOldLevel = 0x904B72;    // u16
constexpr U kNewLevel = 0x904B74;    // u16
constexpr U kActionId = 0x904B80;    // u16
constexpr U kEffectBits = 0x904B82;  // u16: a bit per actor with an effect still running
constexpr U kAutoTest = 0x904B8E;    // u8
constexpr U kMessageUp = 0x939F60;   // u8
constexpr U kBannerText = 0x669E08;  // .data: the banner string pointer kind 3's first step shows
constexpr U kHoldIcons = 0x64E2AC;   // .data: 7 byte pairs, the command icons' offsets
constexpr U kLevelMsgPtr = 0x93B8E4; // const char *: the level-up line (Msg_SystemPtr(7))
constexpr U kPartySetByte = 0x90412C;  // u8: & 0x7F the party set PartySet_Select loads
constexpr U kAnimBuffer = 0x8C5D80;  // Sprite_AnimFromSet's buffer (0x1800 bytes)
constexpr U kRandomIds24 = 0x64AEC8;   // .data: 32 ability ids, the random pick of action 0x24
constexpr U kRandomIds25 = 0x64AEE8;   // .data: 32, of actions 0x25 and 0x8C
constexpr U kCharExpTable = 0x658F48;  // Char_ExpTable: 99 records of 8 bytes per character

// Named cells symbols.toml types as data (their names are macros there):
constexpr U kPacketNext = 0x7E0670;  // Gfx_PacketNext
constexpr U kInputHeld = 0x7E1BE8;   // Input_Held (u16)
constexpr U kInputPressed = 0x7E1BEC;  // Input_Pressed (u16)
constexpr U kConfirmButtons = 0x90358E;  // Field_ConfirmButtons (u16)
constexpr U kCancelButtons = 0x903590;   // Field_CancelButtons (u16)

// The party (ObjTrio) and the character records.
constexpr U kMembers = 0x802D40;
constexpr U kMemberSize = 0x14C;
constexpr U kCharRecords = 0x903A70;
constexpr U kCharSize = 0xA4;

// The loss screen's two CLUT runs greyed on the way out (0x100 words each).
constexpr U kClutA = 0x812980;
constexpr U kClutB = 0x811380;

// --- the .data tables the dispatchers jump through ---
constexpr U kEquipSteps = 0x64ADE0;   // by 0x929F01: 3 (0x42DC10, 0x42DCD0, 0x42E040)
constexpr U kEquipOpenSteps = 0x64ADEC;  // by 0x929F02: 3 (0x42D780, 0x42DC20, 0x42DC80)
constexpr U kEquipRunSteps = 0x64ADF8;   // by 0x929F02: 2, called (0x42DCF0, 0x42DE50)
constexpr U kEquipLeaveSteps = 0x64AE00; // by 0x929F02: 2 (0x42E050, 0x42E090)
constexpr U kHoldSteps = 0x64AE48;    // by 0x904AA2: 3 (0x42EDA0, 0x42EE00, 0x42EEA0)
constexpr U kKind3Steps = 0x64AEB4;   // by 0x904AA3: 5 (0x42F5F0, 0x42F640, the three ability steps)
constexpr U kLossSteps = 0x64AFD8;    // by 0x904AA3: 5 (0x432440 .. 0x432750)

// --- the callees nobody owns yet ---
constexpr U kEquipOpenHelper = 0x42E0E0;
constexpr U kEquipConfirmHelper = 0x42E250;
constexpr U kEquipFrameHelper = 0x42E2F0;
constexpr U kMemcpy = 0x5B9450;
constexpr U kEnemiesClear = bof3::addr::BattleEnemy_ClearStates;
constexpr U kSideTargetA = 0x452EB0;
constexpr U kSideTargetB = 0x452F10;
constexpr U kBe4Draw444660 = bof3::addr::BattleWin_DimScreen;   // BE4's
constexpr U kBe4ReturnItem = bof3::addr::Battle_ReturnItem;   // BE4's
constexpr U kBe4MemberName = bof3::addr::Battle_MemberNameToText;   // BE4's

}  // namespace at
}  // namespace battle_e1
