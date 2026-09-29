// The raw addresses effect_1e.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_1e.md.
#pragma once

#include <cstdint>

namespace effect_1e::at {

// Callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called through the harness by address (SH_AT) until they merge; the
// coordinator rebinds them after (docs/effect_1e.md section 8).
constexpr std::uint32_t kBoxPrims = 0x469750;       // E1B: void(int x, int y, int w, int h, colour); reads each & 0xFFFF, the colour a byte
constexpr std::uint32_t kClearEffects = 0x52CE20;   // E1F: void(void); Effect_Objects +0..+3 of all 20 cleared, 7..19 released, record 1 +1 = 2

// Callees nobody owns (catalog part 7, "Unlabelled"; not in the round's cut),
// read to their last instruction for their arguments (docs/effect_1e.md
// section 3). Each is in the harness's effect-standard set by address.
constexpr std::uint32_t kPoseSound = 0x52B1B0;      // void(void): a sound by 0x6BC71D when it differs from 0x6BC71C
constexpr std::uint32_t kUseItemEnd = 0x52B200;     // void(void): animation 9, sound 0x204, effect record 4's +1 up, +3 up, Sprite_ScriptTick (a tail jmp)
constexpr std::uint32_t kEffect3Mode = 0x52B2A0;    // void(unsigned char mode): effect record 3's +6 = mode, +1 up (and +7 = 1) when +1 is 0, 4 or 6
constexpr std::uint32_t kHoldTest = 0x52B2E0;       // void(void): 0x903850 and Sprite_Current +0xA by Input_Pressed / Input_Held bit 0x4000
constexpr std::uint32_t kEffectsStep = 0x52B370;    // void(void): 0x52B460, then the 0x6BC70C.. counters against the tables from 0x66A4C8
constexpr std::uint32_t kLeaveOnPress = 0x52B330;   // unsigned char(unsigned buttons): Input_Pressed & buttons -> Transition_Start(2), +2 = 8, +3 = 0, al 1

// Data: the state machine's cells (docs/effect_1e.md section 2).
constexpr std::uint32_t kSpriteIndex = 0x939A1C;    // u8: the Sprite_Objects record the stages read (0xFF: none; 0x5289C0 writes it)
constexpr std::uint32_t kRecordA = 0x939A20;        // pointer: a 10-byte record (0x66A4E8 + 10 * (0x904130 - 0x2E), 0x52B250)
constexpr std::uint32_t kRecordB = 0x939A24;        // pointer: a 20-byte record (0x66A528 + 20 * (0x90412E - 0x1C), 0x52B250)
constexpr std::uint32_t kBlink = 0x939A28;          // u8: FieldPanel_DrawBlink's switch
constexpr std::uint32_t kChoice = 0x6BC709;         // u8 (0x528A90's, not ours)
constexpr std::uint32_t kCounter = 0x6BC70C;        // u32: cleared by stage 3's start
constexpr std::uint32_t kPoseWas = 0x6BC71C;        // u8: the pose 0x52B1B0 compares with
constexpr std::uint32_t kPose = 0x6BC71D;           // u8: 0, 1 or 2
constexpr std::uint32_t kBestCounts = 0x9040EC;     // u8 by Sprite_Objects +6: the best of each kind (save data)
constexpr std::uint32_t kItemA = 0x90412E;          // u8 and the next: cleared when Inventory_Remove fails
constexpr std::uint32_t kItemA2 = 0x90412F;
constexpr std::uint32_t kKindSounds = 0x66A6AD;     // u8 by 36 * kind: 2 or more picks sound 0x20C over 0x20B (image table, read-only)
constexpr std::uint32_t kPools = 0x803580;          // MessagePools: the base the offset words below are added to
constexpr std::uint32_t kPoolWord00 = 0x803600;     // u16: the stage-1 prompt's message
constexpr std::uint32_t kPoolWord20 = 0x803620;     // u16: stage 9's three
constexpr std::uint32_t kPoolWord40 = 0x803640;
constexpr std::uint32_t kPoolWord5E = 0x80365E;

// Effect_Objects cells the stages drive (record n at 0x7E11E0 + 0x80 n).
constexpr std::uint32_t kEff0State = 0x7E11E1;      // record 0 +1
constexpr std::uint32_t kEff0Hold = 0x7E11EB;       // record 0 +0xB
constexpr std::uint32_t kEff0StepX = 0x7E11EC;      // record 0 +0xC (s32)
constexpr std::uint32_t kEff0StepY = 0x7E11F0;      // record 0 +0x10 (s32)
constexpr std::uint32_t kEff0StepZ = 0x7E11F4;      // record 0 +0x14 (s32)
constexpr std::uint32_t kEff0X = 0x7E1214;          // record 0 +0x34 (s32, 16.16)
constexpr std::uint32_t kEff0Z = 0x7E1218;          // record 0 +0x38 (s32)
constexpr std::uint32_t kEff0Height = 0x7E121E;     // record 0 +0x3E (s16)
constexpr std::uint32_t kEff1State = 0x7E1261;      // record 1 +1
constexpr std::uint32_t kEff1Sub = 0x7E1262;        // record 1 +2
constexpr std::uint32_t kEff2State = 0x7E12E1;      // record 2 +1
constexpr std::uint32_t kEff2Word = 0x7E12F8;       // record 2 +0x18 (u32)
constexpr std::uint32_t kEff3State = 0x7E1361;      // record 3 +1
constexpr std::uint32_t kEff4State = 0x7E13E1;      // record 4 +1
constexpr std::uint32_t kEff4Mode = 0x7E13E6;       // record 4 +6
constexpr std::uint32_t kEff5State = 0x7E1461;      // record 5 +1
constexpr std::uint32_t kEff5Count = 0x7E146A;      // record 5 +0xA
constexpr std::uint32_t kEff5Frame = 0x7E1470;      // record 5 +0x10 (s32)
constexpr std::uint32_t kEff6State = 0x7E14E1;      // record 6 +1
constexpr std::uint32_t kEff6Choice = 0x7E14E6;     // record 6 +6
constexpr std::uint32_t kEff6Hold = 0x7E14EB;       // record 6 +0xB

// Sprite_Objects fields the stages read by kSpriteIndex (stride 0xA4, 30 records).
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr unsigned kSpriteCount = 30;

}  // namespace effect_1e::at
