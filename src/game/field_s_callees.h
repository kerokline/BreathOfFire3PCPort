// The raw addresses field_s.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/field_s.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace field_s::at {

// Callees nobody owns (or another group of this wave owns), called through the
// harness by address (SH_AT).
constexpr std::uint32_t kDrawTag = bof3::addr::Menu_DrawTile16;          // (x, y, kind, dim): a 16 x 8 SPRT on page 0x2F at (x, y), u = kind << 4,
                                                      // v 0xD8, shade 0x80 or 0x10 by dim, 16-bit x and y; round twelve group FO's
constexpr std::uint32_t kSwapBytes = bof3::addr::FieldMenu_SwapBytes;  // (a, b): swaps the bytes a and b point at; R2E's
constexpr std::uint32_t kAbilityListCount = bof3::addr::AbilityList_CountSet; // (member, which, current) -> al: one of a record's four
                                                      // ability lists (or the current name's); nobody's
constexpr std::uint32_t kEquipPreview = bof3::addr::FieldEquip_PreviewItem;  // (): the equip screen's preview bytes 0x6BDFA8.. and 0x803341
                                                      // from the member and the chosen item; R2E's
constexpr std::uint32_t kEquipApply = bof3::addr::FieldEquip_ApplyPreview;  // (): the previewed items swapped into the member's six slots
                                                      // through the inventory, then Char_RecalcStats; R2E's

// DIV-0011's site inside PartyForm_DrawReserve (menu_frame.cpp kReserveListSite):
// E8 rel32, re-aimed at Menu_DrawFrame; the original's callee the empty 0x4DF820.
constexpr std::uint32_t kFrameSite = 0x581313;

// The menu block (docs/menu-screens.md section 1; scenario_harness at::kMenuBlock).
// kMode: now BattleE1_Mode (symbols.toml). u8: the field menus' mode (ShopMode_States' index)
// kState: now FieldS_State (symbols.toml). u8: the mode's state
// kStep: now FieldS_Step (symbols.toml). u8: the state's step
constexpr std::uint32_t kTimer = 0x929F04;            // u8: the frame counter
// kMenu05: now MenuLists_Cursor (symbols.toml). u8: cleared by ShopResist_Open
// kCursor: now BattleActorCopies_ItemActor (symbols.toml). s8: ShopResist's member cursor
constexpr std::uint32_t kColumn = 0x929F08;           // u8: PartyForm's column (0 the party, 1 the reserve)
// kRow: now BattleE1_ModeByte9 (symbols.toml). s8: PartyForm's row
// kPickColumn: now ShopStates_Clear0A (symbols.toml). u8: the column picked first (0x7F none)
// kAnswer: now FieldS_Answer (symbols.toml). u8: Menu_YesNo's answer; ShopResist's message index
// kPickRow: now FieldS_PickRow (symbols.toml). s8: the row picked first; ShopResist's resistance bit
constexpr std::uint32_t kMessageEnd = 0x929F0F;       // u8: ShopResist's message end; 1 after PartyForm_End

// Other cells.
// kStyle: now Config_WindowColour (symbols.toml). u8: the window style (Menu_DrawBox's colour)
// kBackdrop: now Config_Background (symbols.toml). u8: the Config backdrop kind
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags + 0xA0)
constexpr std::uint32_t kPartyList = 0x904062;        // 3 bytes: the party's member ids
constexpr std::uint32_t kPartyList2 = 0x904065;       // 3 bytes: the second list
// kLoneParty: now AreaW3c_CampFlag (symbols.toml). u8: non-zero: the party reloads as one member
constexpr std::uint32_t kSharedList = 0x904574;       // 128 bytes: the shared ability list (AbilityList_Add's `shared`)
// kRecords: now CharacterRecords (symbols.toml). CharacterRecords, 8 of 0xA4
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kObjStride = 0x14C;           // ObjTrio's records (Field_Members is +0x148 of each)
constexpr std::uint32_t kAbilityRecords = 0x65C4C8;   // Ability_Records, 24 bytes each (+0x12 the byte sorted on, +0x16 the help)
constexpr std::uint32_t kPlaceTable = 0x663FD8;       // Rest_PlaceParty's (x, y, z) dwords, 16 bytes a slot, three a kind
constexpr std::uint32_t kArea85Place = 0x6640C8;      // PartyForm_Reload's (x, y, z) for area 0x85, 16 bytes a member
constexpr std::uint32_t kFacingSteps = 0x6640F8;      // PartyForm_Reload's (dx, dz) s16 pairs by the leader's facing
constexpr std::uint32_t kPickCells = 0x664078;        // PartyForm_Draw's cursor cells: (x, y) words and a flag, 6 bytes
constexpr std::uint32_t kReserve = 0x6BC894;          // up to 8 bytes: PartyForm's reserve (records not in the party)
// kReserveCount: now FieldS_ReserveCount (symbols.toml). u8: its count (inside the flag pairs' reach, docs/field_s.md)
constexpr std::uint32_t kReserveFlags = 0x6BC88C;     // byte pairs (party, reserve), set by PartyForm_Setup
// kPartyFormResult: now FieldS_PartyFormResult (symbols.toml). u8: PartyForm_Swap's answer (0xFF: swapped)
// kPickIndex: now FieldS_PickIndex (symbols.toml). u8: SharedList's member cursor
constexpr std::uint32_t kJoined = 0x6BC8BC;           // up to 8 bytes: the joined records' indexes
// kSharedChoice: now FieldS_SharedChoice (symbols.toml). u8: SharedList's first choice (0 or 1)
// kJoinedCount: now FieldS_JoinedCount (symbols.toml). u8: the joined count
constexpr std::uint32_t kEquipBytes = 0x939880;       // FieldMenu's per-member bytes: top at +slot + 6 member
constexpr std::uint32_t kEquipCursor = 0x9398A0;      // and the cursor at +slot + 6 member
constexpr std::uint32_t kArea = 0x904EFC;             // Game_AreaNumber (u16)
// kZenny: now Party_Zenny (symbols.toml). Party_Zenny (u32)
// kFieldMembers: now Field_Members (symbols.toml). Field_Members: ObjTrio's +0x148, a record index
constexpr std::uint32_t kAreaDescriptors = 0x667590;  // Area_Descriptors, 200 pointers
constexpr std::uint32_t kLeaderName = 0x802DC0;       // Text_CurrentName: the leader's +0x80, a record's copy
// kWait: now Transition_Busy (symbols.toml). Transition_Busy: non-zero while a transition runs
// kChapter: now Cond_ByteFA (symbols.toml). Cond_ByteFA
// kFlagRow: now Chapter_FlagRow (symbols.toml). the chapter's flag row pointer
constexpr std::uint32_t kPressed = 0x7E1BEC;          // Input_Pressed (a dword read where the original reads one)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons
constexpr std::uint32_t kKeyItemInk = 0xF;            // SharedList's key item (KeyItem_Has(0xF))
constexpr std::uint32_t kInkItem = 0x58;              // and its consumable (category 0, item 0x58)

// Text and format addresses (read in place; no text is copied here).
constexpr std::uint32_t kFmtMember = 0x5E10C0;        // Area08_MessageFormat, the member number's format
constexpr std::uint32_t kFmtLevel = 0x64E324;         // the level's format
constexpr std::uint32_t kFmtFigureA = 0x6639B0;       // the first of a figure pair's two formats (HP, AP now)
constexpr std::uint32_t kFmtFigureB = 0x6639A8;       // the second (their maxima)
constexpr std::uint32_t kFmtCost = 0x6641DC;          // ShopResist's cost format
constexpr std::uint32_t kFmtCount = 0x6639C8;         // SharedList_DrawList's count format
// kFmtItems: now Boss26Fx_CountFormat (symbols.toml). Boss26Fx_CountFormat, the item count's
constexpr std::uint32_t kPrintBuf = 0x904BA0;         // the text scratch sprintf writes
constexpr std::uint32_t kStatusA = 0x66A0E8;          // PartyForm_DrawReserve's status text for record +0x10 bit 7
constexpr std::uint32_t kStatusB = 0x66A0F0;          // and for bit 5
constexpr std::uint32_t kListTitle = 0x664288;        // SharedList_DrawList's title
constexpr std::uint32_t kItemLabel = 0x66A118;        // SharedList_DrawItemCount's label
constexpr std::uint32_t kPiecesA = 0x6641E4;          // piece lists (Menu_DrawPieces)
constexpr std::uint32_t kPiecesB = 0x6641F0;
constexpr std::uint32_t kPiecesC = 0x6641FC;
constexpr std::uint32_t kPiecesD = 0x664214;
constexpr std::uint32_t kPiecesE = 0x664220;
constexpr std::uint32_t kPiecesF = 0x66422C;
constexpr std::uint32_t kPiecesG = 0x664238;
constexpr std::uint32_t kPiecesH = 0x664244;

// The .data tables the group's dispatchers read in place.
// kPartyFormOpenSteps: now PartyForm_OpenSteps (symbols.toml). 3: PartyForm_OpenStep's, by 0x929F02
// kPartyFormLeaveSteps: now PartyForm_LeaveSteps (symbols.toml). 4: PartyForm_LeaveStep's
// kSharedListMoveSteps: now SharedList_MoveSteps (symbols.toml). 5: SharedList_MoveStep's
// kSharedListSortSteps: now SharedList_SortSteps (symbols.toml). 3: SharedList_SortStep's
// kSharedListSorts: now SharedList_Sorts (symbols.toml). 3: SharedList_Sort's, by its argument

}  // namespace field_s::at
