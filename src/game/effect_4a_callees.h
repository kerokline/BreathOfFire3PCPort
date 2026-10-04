// The raw addresses effect_4a.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_4a.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_4a::at {

// --- callees of later groups of round thirteen, called by address (SH_AT)
// until they merge (docs/effect_4a.md section 8) ------------------------------
constexpr std::uint32_t kFade = bof3::addr::Effect_DrawScreenTint;          // E4D's (void): a full-screen tile coloured by Sprite_Current +0x5D..+0x5F
constexpr std::uint32_t kKind87Setup = bof3::addr::EffectKind87_Setup;   // E4B's (void): kind 0x87's records in EffectKind30_Shards set up
constexpr std::uint32_t kKind87Step = bof3::addr::EffectKind87_StepPanes;    // E4B's (void) -> al: kind 0x87's records moved and drawn, 0 when done
constexpr std::uint32_t kKind87Reset = bof3::addr::EffectKind87_FadePanes;   // E4B's (void): six of those records reset

// --- the chapters' counters (scenario_harness at::kCounter) ----------------
constexpr std::uint32_t kCounter = 0x903848;       // u8: kind 0x85 ends at 0x35; kinds 0x85 and 0x86 raise it
constexpr std::uint32_t kCounterB = 0x903849;      // u8: the push count (3, 4, 5), 0xFF the end, 0xFE the stop
constexpr std::uint32_t kCounterC = 0x90384A;      // u8: kind 0x84's presses; 0x80 the end
constexpr std::uint32_t kCounterD = 0x90384B;      // u8: cleared with the two above
constexpr std::uint32_t kObjectCount = 0x903850;   // u16: the object EventOp_0x places (its count word)

// --- the chapter's cells (scenario_harness at::kStep) ---------------------
constexpr std::uint32_t kStep = 0x8034E5;          // u8: the chapter's step (MoveScript_Var7 + 1)
constexpr std::uint32_t kStepWord = 0x8034E6;      // u16: the chapter's timer word (area_w0a_callees.h kTimer)

// --- the objects the kinds move ---------------------------------------------
constexpr std::uint32_t kObject0X = 0x7DEEB4;      // Sprite_Objects + 0x34: record 0's x
constexpr std::uint32_t kObject1 = 0x7DEF24;       // Sprite_Objects + 0xA4: record 1
constexpr std::uint32_t kObject1X = 0x7DEF58;      // record 1's +0x34
constexpr std::uint32_t kLeaderX = 0x802D74;       // ObjTrio + 0x34: the leader's x
constexpr std::uint32_t kObjTrio2 = 0x802FD8;      // ObjTrio + 2 * 0x14C: the third member's field object
constexpr std::uint32_t kObjTrio2X = 0x80300C;     // its +0x34
constexpr std::int32_t kTrio2Line = 0x340000;      // the x at which kinds 0x82 / 0x84 end with 0xFF (step 0x14)
constexpr unsigned kSpriteCount = 30;              // Sprite_Objects' records (symbols.toml)
constexpr unsigned kSpriteStride = 0xA4;

// --- kind 0x84: the push counts and waits -----------------------------------
constexpr std::uint32_t kPushCounts = 0x654C68;    // 16 bytes by Rand & 0xF: the next push count (EffectKind84_PushCounts)
constexpr std::uint32_t kWaits = 0x654C78;         // 16 bytes by Rand & 0xF: the wait (EffectKind84_Waits)
constexpr unsigned kHeldA = 0x3000, kHeldB = 0x6000, kHeldC = 0x2000;   // Input_Held words that count

// --- kind 0x86: two objects placed --------------------------------------------
constexpr std::uint32_t kPlaceOp = 0x654D88;       // the 17-byte EventOp_0x operand both objects are placed by

// --- kind 0x87 ---------------------------------------------------------------
constexpr std::uint32_t kKind87Flag = 0x676280;    // u8: cleared by kind 0x87's start

}  // namespace effect_4a::at
