// The raw addresses effect_1e.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_1e.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1e::at {

// Callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called through the harness by address (SH_AT) until they merge; the
// coordinator rebinds them after (docs/effect_1e.md section 8).
constexpr std::uint32_t kBoxPrims = bof3::addr::Panel_DrawWindow;       // E1B: void(int x, int y, int w, int h, colour); reads each & 0xFFFF, the colour a byte
constexpr std::uint32_t kClearEffects = bof3::addr::Effect_ResetFirstSeven;   // E1F: void(void); Effect_Objects +0..+3 of all 20 cleared, 7..19 released, record 1 +1 = 2

// Callees nobody owns (catalog part 7, "Unlabelled"; not in the round's cut),
// read to their last instruction for their arguments (docs/effect_1e.md
// section 3). Each is in the harness's effect-standard set by address.
constexpr std::uint32_t kPoseSound = bof3::addr::LeaderPanel_PoseSound;      // void(void): a sound by 0x6BC71D when it differs from 0x6BC71C
constexpr std::uint32_t kUseItemEnd = bof3::addr::LeaderPanel_UseItemEnd;     // void(void): animation 9, sound 0x204, effect record 4's +1 up, +3 up, Sprite_ScriptTick (a tail jmp)
constexpr std::uint32_t kEffect3Mode = bof3::addr::LeaderPanel_Effect3Mode;    // void(unsigned char mode): effect record 3's +6 = mode, +1 up (and +7 = 1) when +1 is 0, 4 or 6
constexpr std::uint32_t kHoldTest = bof3::addr::LeaderPanel_HoldTest;       // void(void): 0x903850 and Sprite_Current +0xA by Input_Pressed / Input_Held bit 0x4000
constexpr std::uint32_t kEffectsStep = bof3::addr::LeaderPanel_EffectsStep;    // void(void): 0x52B460, then the 0x6BC70C.. counters against the tables from 0x66A4C8
constexpr std::uint32_t kLeaveOnPress = bof3::addr::LeaderPanel_LeaveOnPress;   // unsigned char(unsigned buttons): Input_Pressed & buttons -> Transition_Start(2), +2 = 8, +3 = 0, al 1

// Data: the state machine's cells (docs/effect_1e.md section 2).
constexpr std::uint32_t kSpriteIndex = 0x939A1C;    // u8: the Sprite_Objects record the stages read (0xFF: none; 0x5289C0 writes it)
// kRecordA: now Effect1a_PanelScript (symbols.toml). pointer: a 10-byte record (0x66A4E8 + 10 * (0x904130 - 0x2E), 0x52B250)
// kRecordB: now Effect1a_PanelSet (symbols.toml). pointer: a 20-byte record (0x66A528 + 20 * (0x90412E - 0x1C), 0x52B250)
// kBlink: now Effect1e_Blink (symbols.toml). u8: FieldPanel_DrawBlink's switch
constexpr std::uint32_t kChoice = 0x6BC709;         // u8 (0x528A90's, not ours)
constexpr std::uint32_t kCounter = 0x6BC70C;        // u32: cleared by stage 3's start
// kPoseWas: now Effect1e_PoseWas (symbols.toml). u8: the pose 0x52B1B0 compares with
// kPose: now Effect1e_Pose (symbols.toml). u8: 0, 1 or 2
constexpr std::uint32_t kBestCounts = 0x9040EC;     // u8 by Sprite_Objects +6: the best of each kind (save data)
// kItemA: now CharStats_ExtraItem (symbols.toml). u8 and the next: cleared when Inventory_Remove fails
// kItemA2: now CharStats_ExtraCount (symbols.toml).
constexpr std::uint32_t kKindSounds = 0x66A6AD;     // u8 by 36 * kind: 2 or more picks sound 0x20C over 0x20B (image table, read-only)
constexpr std::uint32_t kPools = 0x803580;          // MessagePools: the base the offset words below are added to
constexpr std::uint32_t kPoolWord00 = 0x803600;     // u16: the stage-1 prompt's message
constexpr std::uint32_t kPoolWord20 = 0x803620;     // u16: stage 9's three
constexpr std::uint32_t kPoolWord40 = 0x803640;
constexpr std::uint32_t kPoolWord5E = 0x80365E;

// Effect_Objects cells the stages drive (record n at 0x7E11E0 + 0x80 n).
// kEff0State: now Effect_Objects[1] (symbols.toml). record 0 +1
// kEff0Hold: now Effect_Objects[0xb] (symbols.toml). record 0 +0xB
constexpr std::uint32_t kEff0StepX = 0x7E11EC;      // record 0 +0xC (s32)
constexpr std::uint32_t kEff0StepY = 0x7E11F0;      // record 0 +0x10 (s32)
// kEff0StepZ: now rv::As<records::EffectObject>(Effect_Objects)->step_y (records.h). record 0 +0x14 (s32)
constexpr std::uint32_t kEff0X = 0x7E1214;          // record 0 +0x34 (s32, 16.16)
// kEff0Z: now rv::As<records::EffectObject>(Effect_Objects)->z (records.h). record 0 +0x38 (s32)
// kEff0Height: now Effect_Objects[0x3e] (symbols.toml). record 0 +0x3E (s16)
// kEff1State: now Effect_Objects[0x81] (symbols.toml). record 1 +1
// kEff1Sub: now Effect_Objects[0x82] (symbols.toml). record 1 +2
// kEff2State: now Effect_Objects[0x101] (symbols.toml). record 2 +1
// kEff2Word: now rv::As<records::EffectObject>(Effect_Objects + 0x100)->work_18 (records.h). record 2 +0x18 (u32)
// kEff3State: now Effect_Objects[0x181] (symbols.toml). record 3 +1
// kEff4State: now Effect_Objects[0x201] (symbols.toml). record 4 +1
// kEff4Mode: now Effect_Objects[0x206] (symbols.toml). record 4 +6
// kEff5State: now Effect_Objects[0x281] (symbols.toml). record 5 +1
// kEff5Count: now Effect_Objects[0x28a] (symbols.toml). record 5 +0xA
constexpr std::uint32_t kEff5Frame = 0x7E1470;      // record 5 +0x10 (s32)
// kEff6State: now Effect_Objects[0x301] (symbols.toml). record 6 +1
// kEff6Choice: now Effect_Objects[0x306] (symbols.toml). record 6 +6
// kEff6Hold: now Effect_Objects[0x30b] (symbols.toml). record 6 +0xB

// Sprite_Objects fields the stages read by kSpriteIndex (stride 0xA4, 30 records).
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr unsigned kSpriteCount = 30;

}  // namespace effect_1e::at
