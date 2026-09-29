// The raw addresses field_e1.cpp calls or reads that symbols.toml does not
// name, or that another group of round twelve's wave two owns - each a
// load-bearing constant (CLAUDE.md rule 3). docs/field_e1.md sections 2 and 8.
#pragma once

#include <cstdint>

namespace field_e1::at {

// --- callees nobody owns (engine rows, no group of round twelve), by address ---
constexpr std::uint32_t kDrawMode = 0x52CF60;     // (index, slot): a draw-mode primitive from the 16-byte records
                                                  // 0x660394, committed
constexpr std::uint32_t kDrawSprite = 0x52CFE0;   // (sprite, slot, x, y) -> unsigned char *: a sprite primitive
                                                  // from the 16-byte records 0x660438 at (x, y) (words), committed;
                                                  // eax the primitive
constexpr std::uint32_t kKindPoints = 0x52CE60;   // (kind, count) -> u16: a kind's points for a count, from the
                                                  // 36-byte records 0x66A6AC (count at or above +3: the word +6)
constexpr std::uint32_t kKindTotal = 0x52CED0;    // () -> u16: kKindPoints summed over the 32 bytes 0x9040EC
constexpr std::uint32_t kDrawQuad = 0x468950;     // (x, y, height, which): a textured quad, committed

// --- callees round twelve's FE2 owns (docs/scenario_harness.md 7.6), raw until it merges ---
constexpr std::uint32_t kObjectTrigger = 0x56D6B0;   // Field_ObjectTrigger(object): FE2's
constexpr std::uint32_t kJumpPose = 0x535FC0;        // (): the jump's first pose (0x3C or 0x3D by +8)
constexpr std::uint32_t kJumpSetUp = 0x535FE0;       // (): Field_JumpSetUp, the frames, the pose, a step
constexpr std::uint32_t kJumpOut1 = 0x536050;        // () -> al: FE2's
constexpr std::uint32_t kJumpOut2 = 0x5360C0;        // () -> al
constexpr std::uint32_t kJumpOut3 = 0x536130;        // () -> al
constexpr std::uint32_t kJumpOut4 = 0x536170;        // () -> al
constexpr std::uint32_t kJumpAirA = 0x5364D0;        // (): by Field_InputHeld bit 12
constexpr std::uint32_t kJumpAirB = 0x536550;        // (): by bit 14
constexpr std::uint32_t kJumpAirC = 0x5365D0;        // (): by the button map word 0x903580
constexpr std::uint32_t kJumpIn0 = 0x536290;         // ()
constexpr std::uint32_t kJumpIn1 = 0x5362D0;         // () -> al
constexpr std::uint32_t kJumpIn2 = 0x5363C0;         // () -> al
constexpr std::uint32_t kJumpIn3 = 0x536440;         // () (a tail jump)

// --- data ---
// The .data dispatch tables this group's dispatchers read in place (entries to
// the next table's start; the index is a whole byte, unchecked).
constexpr std::uint32_t kJumpSteps = 0x66099C;        // 4: by Sprite_Current +2 (Field_JumpState)
constexpr std::uint32_t kJumpOutSteps = 0x6609AC;     // 5: by +3 (Field_JumpOut)
constexpr std::uint32_t kJumpInSteps = 0x6609C0;      // 4: by +3 (Field_JumpIn)
constexpr std::uint32_t kContentSteps = 0x660A1C;     // 2: by +2 (Field_ContentState)
constexpr std::uint32_t kFormActions = 0x660A44;      // 19: by the party set 0x90412C & 0x7F (Field_FormActionState)
constexpr unsigned kFormActionCount = 19;             // Field_EncounterAreas 0x660A90 follows: data, not code
// Constant tables read in place.
constexpr std::uint32_t kKindNames = 0x656FF8;        // 22-byte records by kind: the 16-byte name first
constexpr std::uint32_t kKindName16 = 0x669CD8;       // the 16-byte name of kind 0x16
constexpr std::uint32_t kCountFormat = 0x64E324;      // the sprintf format of a count
constexpr std::uint32_t kTotalFormat = 0x64ADD8;      // the sprintf format of the total
constexpr std::uint32_t kZennyFormat = 0x5E10C0;      // Area08_MessageFormat: the zenny amount's format
constexpr std::uint32_t kDirPairs = 0x66971C;         // 2 signed bytes a direction: the cell step (1 reads as 2)
constexpr std::uint32_t kPlaceOffsets = 0x660B40;     // 4 x (x, z) dwords by direction >> 1
constexpr std::uint32_t kGatewayExits = 0x660AB8;     // 10 x 8 bytes: area, the exit's area, x, z (words)
constexpr std::uint32_t kGatewayExitsEnd = 0x660B08;
constexpr std::uint32_t kGateway189 = 0x660B08;       // 2 x 3 bytes: area, x, z - area 0xBD's, by Cond_ByteFF == 0
constexpr std::uint32_t kPendingStates = 0x660B70;    // 5 x 4 bytes: a member's state bytes +1..+4 by kind
constexpr std::uint32_t kFormationAnims = 0x660B3C;   // BattleFormation_Anims: a byte by the formation 0x904AAC
// Cells.
constexpr std::uint32_t kStyle = 0x903A5A;            // u8: the window style Menu_DrawBox is given
constexpr std::uint32_t kKindCounts = 0x9040EC;       // 32 bytes: a count by kind (kKindTotal's)
constexpr std::uint32_t kRank = 0x9045F4;             // u8: the total's rank (0..12), written by FieldPanel_DrawTotal
constexpr std::uint32_t kBlink = 0x939A28;            // u8: FieldPanel_DrawBlink draws while it is not 0
constexpr std::uint32_t kItemIds = 0x904154;          // Inventory_IdLists[0]: 128 ids
constexpr std::uint32_t kItemCounts = 0x904354;       // Inventory_CountLists[0]: 128 counts
constexpr std::uint32_t kObjectFlags = 0x9040CC;      // the flag bank of the field objects' +5 (event-ops.md)
constexpr std::uint32_t kContentCount = 0x904140;     // s32: one more per field object's content taken
constexpr std::uint32_t kPartySet = 0x90412C;         // u8, & 0x7F the loaded party set
constexpr std::uint32_t kMessageWord = 0x7DEE48;      // u16: the open message's id (Msg_OpenSystem writes it)
constexpr std::uint32_t kButtonMap = 0x903580;        // u16: a button map word of the save (input-script.md section 4)
constexpr std::uint32_t kCellX = 0x903850;            // u16: the cell searched (x), and the save of the members' x / z
constexpr std::uint32_t kCellZ = 0x903852;            // u16: its z
constexpr std::uint32_t kExitArea = 0x937F82;         // u16: the area an exit leads to
constexpr std::uint32_t kExitKind = 0x905B88;         // u8: the exit's kind (4 here)
constexpr std::uint32_t kExitX = 0x903860;            // s32: the exit's x (16.16)
constexpr std::uint32_t kExitZ = 0x90384C;            // s32: its z
constexpr std::uint32_t kSlotPositions = 0x7E06E0;    // 4 x (x, z) dwords: a party slot's placed position
constexpr std::uint32_t kLeaderList = 0x904062;       // u8: the leader's id (the party list's first)
constexpr std::uint32_t kSecondList = 0x904065;       // 3 bytes: the second party list
constexpr std::uint32_t kBattleBits = 0x904AE5;       // u8: bit 1 the camera left alone, bit 7 the drop-in
constexpr std::uint32_t kFormation = 0x904AAC;        // u8: the event battle's formation
constexpr std::uint32_t kDropEntry = 0x92BF18;        // u8: Party_DropIn's entry
constexpr std::uint32_t kPendingFlag = 0x904EF0;      // u8: a pending jump runs (Field_PendingJump)
constexpr std::uint32_t kPendingCount = 0x904EF2;     // u8: the member the pending jump is at
constexpr std::uint32_t kFlags2Lo = 0x905BA4;         // Field_ScriptFlags2's low byte, tested as a byte
constexpr std::uint32_t kFlagsLo = 0x9039A2;          // Field_ScriptFlags' low byte, tested as a byte
constexpr std::uint32_t kPools = 0x803580;            // MessagePools: the script pool, u16 offsets by id

// ObjTrio (the party's records, stride 0x14C) and Sprite_Objects (0xA4).
constexpr std::uint32_t kObjStride = 0x14C;
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr std::uint32_t kLeaderX = 0x802D74;          // ObjTrio +0x34: the leader's x (16.16); +2 the whole part
constexpr std::uint32_t kLeaderZ = 0x802D78;          // +0x38: z
constexpr std::uint32_t kLeaderHeight = 0x802D7E;     // +0x3E: the height (s16)

}  // namespace field_e1::at
