// The raw addresses effect_2g.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_2g.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace effect_2g::at {

// --- callees nobody names, called by address (SH_AT); the effect-standard set
// lists both (scenario_harness.cpp kEffectStd) ------------------------------
constexpr std::uint32_t kBox = bof3::addr::Menu_DrawPanelBox;   // (x, y, w, h, style) five words: a menu box (kind 0x55's two)
constexpr std::uint32_t kPrimFromRect = bof3::addr::Gpu_SetTexWindow;  // (unsigned char *prim, const short *rect): a 12-byte texture-window
                                                   // primitive of the rectangle (effect_1a_callees.h kPrimFromRect)

// --- kind 0x66 (area 135's): the window's rectangle and the pieces' .data --
constexpr std::uint32_t kKind66Rect = 0x654750;    // s16 x, s16 y, u8 w, u8 h: the window (Window_DrawFrame's)
constexpr std::uint32_t kKind66Places = 0x654770;  // three records of 8: s16 x, y, w, h on the screen
constexpr unsigned kKind66PlaceCount = 3;
constexpr std::uint32_t kKind66Pieces = 0x654788;  // four records of 4: u8 u, v, w, h in the texture page
constexpr unsigned kKind66PieceCount = 4;
constexpr std::uint32_t kMessageWord = 0x7DEE48;   // u16: the message MsgBox_FrameTask types, 0xFFFF none
constexpr std::uint32_t kMsgBoxStep = 0x7DEE46;    // u8, MsgBoxState + 6

// --- kind 0x18's sub-kind 0x20 -----------------------------------------------
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags' story row (Flags_Test's bank)
constexpr unsigned kSub20Flag = 0x2A;              // the story flag the sub-kind follows

// --- kinds 0x15, 0x54 and 0x55: the counters they share ----------------------
constexpr std::uint32_t kHit = 0x903849;           // u8: 1 a hit to take, 2 taken, 0 cleared
constexpr std::uint32_t kCue = 0x90384A;           // u8: the step both kinds wait on (1, 2, 3, 5; bit 7 at the end)
constexpr std::uint32_t kScore = 0x90384B;         // u8: the count kind 0x54 raises; bit 7 the end (kind 0x55 sets it)
constexpr std::uint32_t kTimerWord = 0x8034E6;     // u16: the chapter timer (scenario_harness at::kTimer), 0xFF a mark
constexpr std::uint32_t kKind15Wait = 0x67625C;    // u8: kind 0x15's frame count (nothing else in the image reads it)
constexpr std::uint32_t kKind15Delays8 = 0x6547AC; // 8 bytes: a wait by Rand() & 7
constexpr std::uint32_t kKind15Delays4 = 0x6547B8; // 4 bytes: a wait by Rand() & 3
constexpr std::uint32_t kMember1 = 0x802E8C;       // ObjTrio record 1 (ObjTrio + 0x14C), the second member's
constexpr std::uint32_t kMember1Flags = 0x802FB0;  // its +0x124 (bit 6 held)
constexpr std::uint32_t kMember1Pose = 0x802EE4;   // its +0x58 (u16, the animation)
constexpr std::uint32_t kMember1PoseDone = 0x802ED6; // its +0x4A (1 when the animation has run)
constexpr std::uint32_t kObjectStride = 0xA4;      // Sprite_Objects' records
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kKind55TimeFormat = 0x654830; // the seconds-and-hundredths format (field_o_callees.h kFmtTime)
constexpr std::uint32_t kKind55Label = 0x66A094;   // the text Text_DrawAt draws beside the count
constexpr std::uint32_t kText = 0x904BA0;          // the text scratch Crt_sprintf prints into

// --- kind 0x5B (area 75's two) -------------------------------------------------
constexpr std::uint32_t kArea75EffectSlots = 0x93C34E; // two bytes: the effect records area 75 keeps
                                                       // (area_w1f_callees.h kOtherEffect / kPlayerEffect)
constexpr unsigned kArea75EffectSlotCount = 2;

// --- ranges ------------------------------------------------------------------
constexpr std::uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;   // .text

}  // namespace effect_2g::at
