// BOF3X_SHADOW=magic_s02: group S02's two overlays (MAGIC003; MAGIC004, whose
// code ten files run) through the spell round's shared harness
// (magic_harness.h), once at start-up. docs/magic_s02.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC003 / --unit
// MAGIC004/MAGIC005/... --clones (2026-09-26; capstone, every jump internal
// but ElemStrike_Kind's jump table - 12 entries, bounded by its byte table,
// whose largest entry is 0xB - which the harness moves into the copy; no
// REFUSED line), names given. Beyond the standard set this group lists the
// draw callees, the sprite calls, the engine's 0x446770, the functions of
// its own its functions call directly, and two other units' (by address).
// Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim logs each primitive's bytes (every primitive
//     of a draw is built at the same pointer, so the state compare alone
//     would see only the last) and moves Gfx_PacketNext on through a packet
//     buffer of the fuzz's own, as the real one does;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - the sprite calls that act on Sprite_Current log which sprite, and the
//     two that queue or update it log the frame-offset table 0x9039D8 the
//     hit sprites and the effect swap round them;
//   - the kFlag blind spot (docs/takeover-queue-round9.md section 9): the
//     three flag callees whose answer the callers act on -
//     Sprite_ScriptTickOnce, BattleActor_FxSize, Battle_ActorIsOut - answer
//     from their own stream after a second, independent disturbance, so a
//     read after a "no" meets moved cells too; Battle_ActorIsOut also moves
//     the state byte +1 of the target's record, which ElemStrike_End reads
//     after it through a pointer taken before;
//   - SuperComboHit_Alloc's stand-in answers 0..31 only: its "none free"
//     (0xFF) makes SuperCombo_SpawnHits write past the pool, outside every
//     region (docs/magic_s02.md section 8); the allocator itself is fuzzed
//     with a full pool a quarter of the time.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s02.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s02 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC003 / MAGIC004/... --clones, 2026-09-26,
// names given.
// 0x49A7B0: 0x62 bytes; +0xF note: call through .data 0x65a524, 11 code entries (a data_tables entry)  SuperCombo_Task
constexpr mh::CallSite kCalls49A7B0[] = {{0x43, 0x49B080}};
// 0x49A820: 0x55 bytes  SuperCombo_Start
// 0x49A880: 0x37 bytes  SuperCombo_Prompt1
constexpr mh::CallSite kCalls49A880[] = {{0x2, 0x49B900}, {0x9, 0x49B420}};
// 0x49A8C0: 0x37 bytes  SuperCombo_Prompt2
constexpr mh::CallSite kCalls49A8C0[] = {{0x2, 0x49B900}, {0x9, 0x49B420}};
// 0x49A900: 0x5A bytes  SuperCombo_PickButton
constexpr mh::CallSite kCalls49A900[] = {{0x19, 0x5B93D2}};
// 0x49A960: 0x89 bytes  SuperCombo_ReadButton
constexpr mh::CallSite kCalls49A960[] = {{0x2, 0x49B900}, {0x10, 0x49B5F0}};
// 0x49A9F0: 0x26 bytes  SuperCombo_Pause
// 0x49AA20: 0x3C bytes  SuperCombo_ShowCount
constexpr mh::CallSite kCalls49AA20[] = {{0x2, 0x49B900}, {0x10, 0x49B700}, {0x17, 0x49B420}};
// 0x49AA60: 0x89 bytes  SuperCombo_Strike
constexpr mh::CallSite kCalls49AA60[] = {{0x59, 0x4FB830}, {0x5E, 0x49B1F0}, {0x63, 0x49B2B0}, {0x80, 0x587900}};
// 0x49AAF0: 0x29 bytes  SuperCombo_WaitChildren
constexpr mh::CallSite kCalls49AAF0[] = {{0x18, 0x4FB830}};
// 0x49AB20: 0x33 bytes  SuperCombo_End
constexpr mh::CallSite kCalls49AB20[] = {{0x5, 0x587900}, {0x1F, 0x4530D0}, {0x2E, 0x4351F0}};
// 0x49AB60: 0x26 bytes  SuperComboChild_Task
constexpr mh::Imm kImms49AB60[] = {{0xF, 0x49AB90}, {0x17, 0x49AE30}};
// 0x49AB90: 0x54 bytes  SuperComboDash_Run
constexpr mh::CallSite kCalls49AB90[] = {{0x4B, 0x588F00}};
constexpr mh::Imm kImms49AB90[] = {{0xF, 0x49ABF0}, {0x17, 0x49AC80}, {0x22, 0x49AD30}, {0x2A, 0x49ADC0}, {0x32, 0x49AE10}};
// 0x49ABF0: 0x8D bytes  SuperComboDash_Start
constexpr mh::CallSite kCalls49ABF0[] = {{0x6E, 0x446770}, {0x76, 0x4FC1F0}};
// 0x49AC80: 0xA2 bytes  SuperComboDash_Leap
constexpr mh::CallSite kCalls49AC80[] = {{0x1D, 0x587900}, {0x26, 0x4FC030}, {0x2B, 0x49B390}, {0x38, 0x452F70}, {0x8A, 0x589410}};
// 0x49AD30: 0x85 bytes  SuperComboDash_Hit
constexpr mh::CallSite kCalls49AD30[] = {{0x1D, 0x587900}, {0x26, 0x4FC030}, {0x2B, 0x49B390}, {0x38, 0x452F70}, {0x4A, 0x589410}, {0x74, 0x446770}};
// 0x49ADC0: 0x42 bytes  SuperComboDash_Return
constexpr mh::CallSite kCalls49ADC0[] = {{0x20, 0x589410}};
// 0x49AE10: 0x18 bytes  SuperComboDash_End
constexpr mh::CallSite kCalls49AE10[] = {{0x12, 0x4351F0}};
// 0x49AE30: 0x4C bytes  SuperComboImage_Run
constexpr mh::CallSite kCalls49AE30[] = {{0x43, 0x588F00}};
constexpr mh::Imm kImms49AE30[] = {{0xF, 0x49AE80}, {0x17, 0x49AF80}, {0x22, 0x49AFE0}, {0x2A, 0x49B020}};
// 0x49AE80: 0xF6 bytes  SuperComboImage_Start
constexpr mh::CallSite kCalls49AE80[] = {{0x72, 0x446770}, {0xE5, 0x454CC0}};
// 0x49AF80: 0x58 bytes  SuperComboImage_Leap
constexpr mh::CallSite kCalls49AF80[] = {{0x40, 0x589410}};
// 0x49AFE0: 0x36 bytes  SuperComboImage_Turn
constexpr mh::CallSite kCalls49AFE0[] = {{0x20, 0x446770}, {0x28, 0x589410}};
// 0x49B020: 0x55 bytes  SuperComboImage_Return
constexpr mh::CallSite kCalls49B020[] = {{0x20, 0x589410}, {0x3F, 0x454DC0}, {0x4F, 0x4351F0}};
// 0x49B080: 0x12 bytes; +0xB note: jmp through .data 0x65a54c, 1 code entries (a data_tables entry)  SuperComboHit_Task
// 0x49B0A0: 0x50 bytes  SuperComboHit_Run
constexpr mh::CallSite kCalls49B0A0[] = {{0x3D, 0x5890E0}};
constexpr mh::Imm kImms49B0A0[] = {{0x19, 0x49B0F0}, {0x24, 0x49B1D0}};
// 0x49B0F0: 0xDA bytes  SuperComboHit_Start
constexpr mh::CallSite kCalls49B0F0[] = {{0x4E, 0x4FBD10}, {0xC7, 0x5891F0}};
// 0x49B1D0: 0x1C bytes  SuperComboHit_Play
constexpr mh::CallSite kCalls49B1D0[] = {{0x0, 0x589410}, {0x5, 0x589410}, {0x16, 0x4F6290}};
// 0x49B1F0: 0xB1 bytes  SuperCombo_SpawnDash
constexpr mh::CallSite kCalls49B1F0[] = {{0x7, 0x435180}};
// 0x49B2B0: 0xDB bytes  SuperCombo_SpawnImages
constexpr mh::CallSite kCalls49B2B0[] = {{0x11, 0x435180}};
// 0x49B390: 0x90 bytes  SuperCombo_SpawnHits
constexpr mh::CallSite kCalls49B390[] = {{0x18, 0x49BA90}};
// 0x49B420: 0x1C1 bytes  SuperCombo_DrawText
constexpr mh::CallSite kCalls49B420[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x92, 0x5A75D0}, {0xEE, 0x5A79A0}, {0xFE, 0x5A79E0}, {0x180, 0x461E50}};
// 0x49B5F0: 0x10A bytes  SuperCombo_DrawButton
constexpr mh::CallSite kCalls49B5F0[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x36, 0x5A75D0}, {0x92, 0x5A79A0}, {0xB5, 0x5A79E0}, {0xFB, 0x461E50}};
// 0x49B700: 0x1F8 bytes  SuperCombo_DrawCount
constexpr mh::CallSite kCalls49B700[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x66, 0x5A75D0}, {0xCA, 0x5A79A0}, {0xDA, 0x5A79E0}, {0x125, 0x461E50}, {0x13F, 0x5A75D0}, {0x191, 0x5A79A0}, {0x1A1, 0x5A79E0}, {0x1E8, 0x461E50}};
// 0x49B900: 0x187 bytes  SuperCombo_DrawBox
constexpr mh::CallSite kCalls49B900[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x26, 0x5A7740}, {0x2E, 0x5A7780}, {0xC3, 0x461E50}, {0xCF, 0x5A7740}, {0xD7, 0x5A7780}, {0x15B, 0x461E50}, {0x172, 0x5A77C0}, {0x17B, 0x461E50}};
// 0x49BA90: 0x57 bytes  SuperComboHit_Alloc
// 0x49BAF0: 0x3E bytes  ElemStrike_Task
constexpr mh::Imm kImms49BAF0[] = {{0xF, 0x49BB30}, {0x17, 0x49BCD0}, {0x22, 0x49BDE0}, {0x2A, 0x49BE30}, {0x32, 0x49BF30}};
// 0x49BB30: 0x192 bytes  ElemStrike_Start
constexpr mh::CallSite kCalls49BB30[] = {{0x66, 0x4FB830}, {0x6B, 0x49BF90}, {0x74, 0x435180}, {0x117, 0x435180}};
// 0x49BCD0: 0x106 bytes  ElemStrike_Tint
constexpr mh::CallSite kCalls49BCD0[] = {{0x80, 0x454DC0}, {0x9F, 0x454CC0}, {0xB1, 0x4FB830}, {0xCB, 0x452F70}, {0xD5, 0x587900}, {0xF5, 0x587900}};
// 0x49BDE0: 0x48 bytes  ElemStrike_Hit
constexpr mh::CallSite kCalls49BDE0[] = {{0x1F, 0x587900}, {0x2E, 0x4530D0}};
// 0x49BE30: 0xF6 bytes  ElemStrike_Fade
constexpr mh::CallSite kCalls49BE30[] = {{0xB, 0x4A29C0}, {0xDA, 0x454DC0}, {0xE5, 0x4FBDB0}};
// 0x49BF30: 0x5E bytes  ElemStrike_End
constexpr mh::CallSite kCalls49BF30[] = {{0x3E, 0x4456C0}, {0x57, 0x4351F0}};
// 0x49BF90: 0x1D5 bytes  ElemStrike_Kind
constexpr mh::JumpTable kTables49BF90[] = {{0x23, 0x10C, 12}};
// 0x49C170: 0x12 bytes; +0xB note: jmp through .data 0x65a5cc, 8 code entries (a data_tables entry)  ElemStrikeChild_Task
// 0x49C190: 0x46 bytes  ElemStrikeCopy_Run
constexpr mh::CallSite kCalls49C190[] = {{0x3D, 0x588F20}};
constexpr mh::Imm kImms49C190[] = {{0xF, 0x4ED5C0}, {0x17, 0x49C1E0}, {0x22, 0x4EE560}, {0x2A, 0x4AEE90}};
// 0x49C1E0: 0x42 bytes  ElemStrikeCopy_Play
constexpr mh::CallSite kCalls49C1E0[] = {{0x0, 0x589410}, {0x31, 0x4FC030}};
// 0x49C230: 0x46 bytes; +0x34 note: call through .data 0x65a5d4, 6 code entries (a data_tables entry)  ElemStrikeFx_Run
constexpr mh::CallSite kCalls49C230[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}};
// 0x49C280: 0xF8 bytes  ElemStrikeFx_Start
constexpr mh::CallSite kCalls49C280[] = {{0xD4, 0x5891F0}};
// 0x49C380: 0x46 bytes  ElemStrikeFx_Play
constexpr mh::CallSite kCalls49C380[] = {{0x23, 0x587900}, {0x2B, 0x589410}, {0x3C, 0x4351F0}, {0x41, 0x588F20}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"SuperCombo_Task", 0x49A7B0, 0x62, kCalls49A7B0, MH_N(kCalls49A7B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Task)},
    {"SuperCombo_Start", 0x49A820, 0x55, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Start)},
    {"SuperCombo_Prompt1", 0x49A880, 0x37, kCalls49A880, MH_N(kCalls49A880), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Prompt1)},
    {"SuperCombo_Prompt2", 0x49A8C0, 0x37, kCalls49A8C0, MH_N(kCalls49A8C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Prompt2)},
    {"SuperCombo_PickButton", 0x49A900, 0x5A, kCalls49A900, MH_N(kCalls49A900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_PickButton)},
    {"SuperCombo_ReadButton", 0x49A960, 0x89, kCalls49A960, MH_N(kCalls49A960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_ReadButton)},
    {"SuperCombo_Pause", 0x49A9F0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Pause)},
    {"SuperCombo_ShowCount", 0x49AA20, 0x3C, kCalls49AA20, MH_N(kCalls49AA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_ShowCount)},
    {"SuperCombo_Strike", 0x49AA60, 0x89, kCalls49AA60, MH_N(kCalls49AA60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_Strike)},
    {"SuperCombo_WaitChildren", 0x49AAF0, 0x29, kCalls49AAF0, MH_N(kCalls49AAF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_WaitChildren)},
    {"SuperCombo_End", 0x49AB20, 0x33, kCalls49AB20, MH_N(kCalls49AB20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_End)},
    {"SuperComboChild_Task", 0x49AB60, 0x26, nullptr, 0, kImms49AB60, MH_N(kImms49AB60), nullptr, 0, reinterpret_cast<const void*>(&::SuperComboChild_Task)},
    {"SuperComboDash_Run", 0x49AB90, 0x54, kCalls49AB90, MH_N(kCalls49AB90), kImms49AB90, MH_N(kImms49AB90), nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_Run)},
    {"SuperComboDash_Start", 0x49ABF0, 0x8D, kCalls49ABF0, MH_N(kCalls49ABF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_Start)},
    {"SuperComboDash_Leap", 0x49AC80, 0xA2, kCalls49AC80, MH_N(kCalls49AC80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_Leap)},
    {"SuperComboDash_Hit", 0x49AD30, 0x85, kCalls49AD30, MH_N(kCalls49AD30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_Hit)},
    {"SuperComboDash_Return", 0x49ADC0, 0x42, kCalls49ADC0, MH_N(kCalls49ADC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_Return)},
    {"SuperComboDash_End", 0x49AE10, 0x18, kCalls49AE10, MH_N(kCalls49AE10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboDash_End)},
    {"SuperComboImage_Run", 0x49AE30, 0x4C, kCalls49AE30, MH_N(kCalls49AE30), kImms49AE30, MH_N(kImms49AE30), nullptr, 0, reinterpret_cast<const void*>(&::SuperComboImage_Run)},
    {"SuperComboImage_Start", 0x49AE80, 0xF6, kCalls49AE80, MH_N(kCalls49AE80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboImage_Start)},
    {"SuperComboImage_Leap", 0x49AF80, 0x58, kCalls49AF80, MH_N(kCalls49AF80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboImage_Leap)},
    {"SuperComboImage_Turn", 0x49AFE0, 0x36, kCalls49AFE0, MH_N(kCalls49AFE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboImage_Turn)},
    {"SuperComboImage_Return", 0x49B020, 0x55, kCalls49B020, MH_N(kCalls49B020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboImage_Return)},
    {"SuperComboHit_Task", 0x49B080, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboHit_Task)},
    {"SuperComboHit_Run", 0x49B0A0, 0x50, kCalls49B0A0, MH_N(kCalls49B0A0), kImms49B0A0, MH_N(kImms49B0A0), nullptr, 0, reinterpret_cast<const void*>(&::SuperComboHit_Run)},
    {"SuperComboHit_Start", 0x49B0F0, 0xDA, kCalls49B0F0, MH_N(kCalls49B0F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboHit_Start)},
    {"SuperComboHit_Play", 0x49B1D0, 0x1C, kCalls49B1D0, MH_N(kCalls49B1D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboHit_Play)},
    {"SuperCombo_SpawnDash", 0x49B1F0, 0xB1, kCalls49B1F0, MH_N(kCalls49B1F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_SpawnDash)},
    {"SuperCombo_SpawnImages", 0x49B2B0, 0xDB, kCalls49B2B0, MH_N(kCalls49B2B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_SpawnImages)},
    {"SuperCombo_SpawnHits", 0x49B390, 0x90, kCalls49B390, MH_N(kCalls49B390), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_SpawnHits)},
    {"SuperCombo_DrawText", 0x49B420, 0x1C1, kCalls49B420, MH_N(kCalls49B420), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_DrawText)},
    {"SuperCombo_DrawButton", 0x49B5F0, 0x10A, kCalls49B5F0, MH_N(kCalls49B5F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_DrawButton)},
    {"SuperCombo_DrawCount", 0x49B700, 0x1F8, kCalls49B700, MH_N(kCalls49B700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_DrawCount)},
    {"SuperCombo_DrawBox", 0x49B900, 0x187, kCalls49B900, MH_N(kCalls49B900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperCombo_DrawBox)},
    {"SuperComboHit_Alloc", 0x49BA90, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SuperComboHit_Alloc), 0xFF},
    {"ElemStrike_Task", 0x49BAF0, 0x3E, nullptr, 0, kImms49BAF0, MH_N(kImms49BAF0), nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_Task)},
    {"ElemStrike_Start", 0x49BB30, 0x192, kCalls49BB30, MH_N(kCalls49BB30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_Start)},
    {"ElemStrike_Tint", 0x49BCD0, 0x106, kCalls49BCD0, MH_N(kCalls49BCD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_Tint)},
    {"ElemStrike_Hit", 0x49BDE0, 0x48, kCalls49BDE0, MH_N(kCalls49BDE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_Hit)},
    {"ElemStrike_Fade", 0x49BE30, 0xF6, kCalls49BE30, MH_N(kCalls49BE30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_Fade)},
    {"ElemStrike_End", 0x49BF30, 0x5E, kCalls49BF30, MH_N(kCalls49BF30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrike_End)},
    {"ElemStrike_Kind", 0x49BF90, 0x1D5, nullptr, 0, nullptr, 0, kTables49BF90, MH_N(kTables49BF90), reinterpret_cast<const void*>(&::ElemStrike_Kind)},
    {"ElemStrikeChild_Task", 0x49C170, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeChild_Task)},
    {"ElemStrikeCopy_Run", 0x49C190, 0x46, kCalls49C190, MH_N(kCalls49C190), kImms49C190, MH_N(kImms49C190), nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeCopy_Run)},
    {"ElemStrikeCopy_Play", 0x49C1E0, 0x42, kCalls49C1E0, MH_N(kCalls49C1E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeCopy_Play)},
    {"ElemStrikeFx_Run", 0x49C230, 0x46, kCalls49C230, MH_N(kCalls49C230), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeFx_Run)},
    {"ElemStrikeFx_Start", 0x49C280, 0xF8, kCalls49C280, MH_N(kCalls49C280), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeFx_Start)},
    {"ElemStrikeFx_Play", 0x49C380, 0x46, kCalls49C380, MH_N(kCalls49C380), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ElemStrikeFx_Play)},
};
#undef MH_N

enum : unsigned {
    kSuperCombo_Task, kSuperCombo_Start, kSuperCombo_Prompt1, kSuperCombo_Prompt2, kSuperCombo_PickButton,
    kSuperCombo_ReadButton, kSuperCombo_Pause, kSuperCombo_ShowCount, kSuperCombo_Strike, kSuperCombo_WaitChildren,
    kSuperCombo_End, kSuperComboChild_Task, kSuperComboDash_Run, kSuperComboDash_Start, kSuperComboDash_Leap,
    kSuperComboDash_Hit, kSuperComboDash_Return, kSuperComboDash_End, kSuperComboImage_Run, kSuperComboImage_Start,
    kSuperComboImage_Leap, kSuperComboImage_Turn, kSuperComboImage_Return, kSuperComboHit_Task, kSuperComboHit_Run,
    kSuperComboHit_Start, kSuperComboHit_Play, kSuperCombo_SpawnDash, kSuperCombo_SpawnImages, kSuperCombo_SpawnHits,
    kSuperCombo_DrawText, kSuperCombo_DrawButton, kSuperCombo_DrawCount, kSuperCombo_DrawBox, kSuperComboHit_Alloc,
    kElemStrike_Task, kElemStrike_Start, kElemStrike_Tint, kElemStrike_Hit, kElemStrike_Fade, kElemStrike_End,
    kElemStrike_Kind, kElemStrikeChild_Task, kElemStrikeCopy_Run, kElemStrikeCopy_Play, kElemStrikeFx_Run,
    kElemStrikeFx_Start, kElemStrikeFx_Play, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kPool = 0x6769C0, kPoolStride = 0x84, kActorRecord = 0x904B3C, kAbility = 0x904B80,
                        kFrameSet = 0x9039D8, kInput = 0x7E1BEC, kTints = 0x7E0700;
unsigned char* PoolEntry(unsigned i) { return mh::Mem(kPool + (i % 32) * kPoolStride); }

// The prompt texts: SuperCombo_DrawText reads the pointer table 0x66A0D8 in
// place. The game's four strings hold no 0xFF (the half-width space) and none
// is empty, so half the time the seed aims the table at strings of the
// fuzz's own: up to 12 bytes, 0xFF, glyphs and anything, or none.
constexpr std::uint32_t kTexts = 0x66A0D8;
std::uint32_t g_real_texts[4];
alignas(4) unsigned char g_texts[4][16];
void AimTexts() {
    const bool own = mh::Half();
    for (unsigned t = 0; t < 4; ++t) {
        if (!own) {
            SetLong(mh::Mem(kTexts + 4 * t), static_cast<std::int32_t>(g_real_texts[t]));
            continue;
        }
        const unsigned n = mh::Next() % 13;
        for (unsigned i = 0; i < n; ++i) {
            const std::uint32_t r = mh::Next();
            g_texts[t][i] = r % 3 == 0 ? 0xFF : static_cast<unsigned char>(r % 3 == 1 ? 0x40 + (r >> 8) % 0x40 : 1 + (r >> 8) % 0xFF);
        }
        g_texts[t][n] = 0;
        mh::SetPointer(kTexts + 4 * t, g_texts[t]);
    }
}

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
// The target's record as the originals index it (a party member below 3).
unsigned char* TargetRecordOf(unsigned t) {
    return t < 3 ? mh::PartyOf(static_cast<unsigned char>(t)) : mh::EnemyOf(static_cast<unsigned char>(t));
}

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim: the primitive at Gfx_PacketNext into the log (the real one
// links it), then Gfx_PacketNext on by its size, kept in the buffer (a draw
// writes up to 0x48 past it).
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    const unsigned size = a[1] & 0xFF;
    mh::NoteBytes(Gfx_PacketNext, size);
    unsigned char* p = Gfx_PacketNext + size;
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
    return answer;
}
// The sprite calls that act on Sprite_Current: which sprite; the two that
// queue or update it, the frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// 0x446770 turns the task's +0xC / +0x10 by its +8: the inputs logged, a new
// pair written where the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}
// A flag whose "no" the caller acts on: a second disturbance, independent of
// the answer, which comes from the stream (0 a third of the time).
std::uint32_t FlagAnswer(std::uint32_t answer) {
    mh::Stir();
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? answer & 0xFFFFFF00u : answer | 0x10;
}
std::uint32_t TickEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return FlagAnswer(answer);
}
std::uint32_t SizeEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(0x5122u);
    return FlagAnswer(answer);
}
// Battle_ActorIsOut: half the time the target's state byte +1 moved (6 two
// times in three), then the answer from the stream.
std::uint32_t OutEffect(const std::uint32_t* a, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    const unsigned t = a[0] & 0xFF;
    if (t <= 10 && h % 2 == 0) TargetRecordOf(t)[1] = (h >> 8) % 3 == 0 ? Byte(h >> 16) : 6;
    return FlagAnswer(answer);
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S02_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S02_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S02_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &TickEffect},
    {S02_OURS(BattleActor_FxSize), 0, {}, mh::Answer::kFlag, 0, 0, {}, &SizeEffect},
    {S02_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S02_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &OutEffect},
    // the sprite calls
    {S02_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S02_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S02_OURS(Sprite_UpdateScreenSlot), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S02_OURS(Sprite_QueueOverlay), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    // the draw library (psx_gpu, draw_emit: all ours)
    {S02_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S02_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S02_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S02_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S02_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S02_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S02_OURS(Gpu_SetTile), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S02_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // other units', called directly: MAGIC008's (S06), MAGIC219's (S37)
    {S02_RAW(0x4A29C0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x4F6290), 0, {}, mh::Answer::kPhase, 0, 0},
    // this group's own, called directly
    {S02_RAW(0x49B080), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49B1F0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49B2B0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49B390), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49BF90), 0, {}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49B420), 1, {kU8}, kG, 0, 0},
    {S02_RAW(0x49B5F0), 1, {kU8}, kG, 0, 0},
    {S02_RAW(0x49B700), 1, {kU8}, kG, 0, 0},
    {S02_RAW(0x49B900), 1, {kU8}, kG, 0, 0},
    {S02_RAW(0x49BA90), 0, {}, mh::Answer::kByte, 0, 0x1F},
    // the phases run under a swapped frame-offset table (SuperComboHit_Run's
    // two, ElemStrikeFx_Steps' three): kPhase logs the table 0x9039D8 too
    {S02_RAW(0x49B0F0), 0, {kFrameSet}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49B1D0), 0, {kFrameSet}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x4EF840), 0, {kFrameSet}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49C280), 0, {kFrameSet}, mh::Answer::kPhase, 0, 0},
    {S02_RAW(0x49C380), 0, {kFrameSet}, mh::Answer::kPhase, 0, 0},
};
#undef S02_OURS
#undef S02_RAW

// The four .data handler tables the dispatchers read in place
// (SuperCombo_Phases and the rest, symbols.toml).
const mh::DataTable kTables[] = {{0x65A524, 10}, {0x65A54C, 1}, {0x65A5CC, 2}, {0x65A5D4, 3}};

mh::Region g_regions[] = {
    {0x7E0670, 4},        // Gfx_PacketNext
    {0, kPrimBytes},      // g_prims (filled in at start-up)
    {kPool, 32 * kPoolStride},   // SuperComboHit_Pool
    {kAbility, 0x18},     // the ability word, the hit count 0x904B96
    {kInput, 4},          // Input_Pressed
    {kFrameSet, 4},       // the frame-offset table pointer
    {kTints, 0xC00},      // MoveScript_TintRecords
    {kTexts, 16},         // the prompt texts' pointers (the seed aims them)
    {0, sizeof g_texts},  // g_texts (filled in at start-up)
    {0x80E980, 0x200},    // Gfx_ClutStripSource row 26
    {0x812980, 0x200},    // Gfx_ClutStrip row 26
};

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kInput), h >> 16); break;
    case 2: SetWord(mh::Mem(kAbility), h >> 16); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4: PoolEntry(v)[0] = Byte(h >> 24); break;
    case 5: PoolEntry(v)[4 + (h >> 28) % 8] = Byte(h >> 20); break;
    case 6: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 7: mh::Mem(kTints + v % 0xC00)[0] = Byte(h >> 24); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
// Half the time the dword at `to` made `from` + `step` (or one off it).
void Arrive(unsigned char* to, const unsigned char* from, const unsigned char* step) {
    if (!mh::Half()) return;
    const std::uint32_t off = MH_PICK(0, 0, 0, 1, 0xFFFFFFFFu);
    SetLong(to, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(from)) + static_cast<std::uint32_t>(Long(step)) + off));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    // every pool entry's owner a task or a record (the disturbance writes
    // through the owner while SuperCombo_Task runs an entry)
    for (unsigned i = 0; i < 32; ++i) mh::SetPointer(kPool + i * kPoolStride + 0x80, mh::TaskAt(mh::Next()));
    AimTexts();
    if (mh::Half()) sc[0] = 0;
    if (mh::Half()) SetWord(mh::Mem(kAbility), MH_PICK(0x3, 0x4, 0x5, 0x84, 0x85, 0x86, 0x88, 0x89, 0x9C, 0x9D, 0x9E));
    switch (k) {
    // the dispatchers: inside their tables
    case kSuperCombo_Task: sc[1] = Byte(mh::Next() % 10); break;
    case kSuperComboChild_Task: case kElemStrikeChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kSuperComboHit_Task: sc[1] = 0; break;
    case kElemStrike_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kSuperComboDash_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kSuperComboImage_Run: case kElemStrikeCopy_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kSuperComboHit_Run: sc[2] = Byte(mh::Next() % 2); break;
    case kElemStrikeFx_Run: sc[2] = Byte(mh::Next() % 3); break;
    // the counters: at their thresholds
    case kSuperCombo_Prompt1: case kSuperCombo_Prompt2: case kSuperCombo_Pause: case kSuperCombo_ShowCount:
    case kSuperComboDash_Start: case kSuperComboImage_Start: case kSuperComboHit_Start: case kElemStrikeCopy_Play:
        Near(sc[9], 1);
        if (k == kSuperCombo_ShowCount && mh::Half()) sc[0xA] = Byte(mh::Next() % 10);
        break;
    case kSuperCombo_PickButton:
        Near(sc[9], 1);
        // the count at the time table's end (0x10; entries 0x10..0x13 are 3,
        // the late time, 0x14 the next table's)
        if (mh::Half()) sc[0xA] = Byte(MH_PICK(0xF, 0x10, 0x13, 0x14));
        break;
    case kSuperCombo_ReadButton: {
        Near(sc[9], 1);
        Near(sc[0xA], 0x1F);
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 4);
        const std::uint16_t wanted = mh::Mem(0x65A518)[sc[0xB] % 4];
        const unsigned mode = mh::Next() % 4;
        if (mode == 0) SetWord(mh::Mem(kInput), MH_PICK(0, 0, 0x10, 0x20, 0x40, 0x80, 0xF0, 0x0F, 0x110, 0x8000));
        else if (mode == 1) SetWord(mh::Mem(kInput), mh::Next());
        else SetWord(mh::Mem(kInput), mh::Often() ? wanted : wanted | 0x100u);
        break;
    }
    case kSuperCombo_WaitChildren: case kElemStrike_Hit: if (mh::Half()) sc[0xB] = 0; break;
    case kElemStrike_Tint: sc[0xB] = mh::Often() ? Byte(MH_PICK(0, 1, 2)) : Byte(mh::Next()); break;
    case kSuperComboDash_Leap: case kSuperComboDash_Hit:
        sc[9] = mh::Often() ? Byte(MH_PICK(0, 0, 1, 2, 0xFF, 0xFF)) : Byte(mh::Next());
        if (k == kSuperComboDash_Leap) Arrive(sc + 0x44, sc + 0x3C, sc + 0x14);
        break;
    case kSuperComboImage_Leap: Arrive(sc + 0x44, sc + 0x3C, sc + 0x14); break;
    case kSuperComboDash_Return: case kSuperComboImage_Return:
        Arrive(sc + 0x18, sc + 0x34, sc + 0xC);
        Arrive(sc + 0x1C, sc + 0x38, sc + 0x10);
        break;
    case kSuperComboDash_End: mh::Pointer(mh::at::kOwner)[0xB] = mh::Often() ? Byte(MH_PICK(0, 1, 1, 2)) : Byte(mh::Next()); break;
    case kSuperCombo_SpawnHits: mh::Pointer(mh::at::kOwner)[0xA] = mh::Often() ? Byte(MH_PICK(0, 1, 2, 5, 0x1F, 0x20)) : Byte(mh::Next() % 40); break;
    case kSuperComboHit_Alloc: {
        // a full pool a quarter of the time, else one or a few free entries
        const unsigned mode = mh::Next() % 4;
        if (mode == 0) {
            for (unsigned i = 0; i < 32; ++i) PoolEntry(i)[0] |= 1;
        } else if (mode == 1) {
            for (unsigned i = 0; i < 32; ++i) PoolEntry(i)[0] |= 1;
            PoolEntry(mh::Next())[0] &= 0xFE;
        }
        break;
    }
    case kElemStrike_Fade: {
        Near(sc[9], 0xB);
        const unsigned mode = mh::Next() % 3;
        for (unsigned c = 0x5D; c < 0x60; ++c) {
            if (mode == 0) sc[c] = mh::Often() ? Byte(MH_PICK(0, 0, 1, 2, 2, 3)) : Byte(mh::Next());
            else if (mode == 1) sc[c] = Byte(MH_PICK(0, 2));
        }
        break;
    }
    case kElemStrike_End:
        if (mh::Half()) TargetRecordOf(mh::Mem(mh::at::kTarget)[0])[1] = 6;
        break;
    case kElemStrike_Kind:
        if (mh::Often()) SetWord(mh::Mem(kAbility), mh::Half() ? MH_PICK(4, 5, 7, 0x1D, 0x31, 0x85, 0x86, 0x87, 0x88, 0x9B, 0x9C, 0x9D, 6, 3)
                                                             : (mh::Half() ? mh::Next() % 0xA0 : mh::Next()));
        break;
    case kElemStrikeFx_Start: if (mh::Half()) sc[3] = 0; break;
    case kElemStrikeFx_Play: sc[9] = mh::Often() ? Byte(MH_PICK(0, 0, 1, 1, 2)) : Byte(mh::Next()); break;
    default: break;
    }
}

// The draws' argument: the text 0..3 (the pointer table's entries), the box
// 0..4, any button and count (half of them below 10), with garbage above the
// byte the callee reads.
void Args(unsigned k, std::uint32_t* a) {
    const std::uint32_t high = mh::Half() ? (mh::Next() & 0xFFFFFF00u) : 0;
    switch (k) {
    case kSuperCombo_DrawText: a[0] = high | (mh::Next() % 4); break;
    case kSuperCombo_DrawBox: a[0] = high | (mh::Next() % 5); break;
    case kSuperCombo_DrawButton: a[0] = high | (mh::Half() ? mh::Next() % 4 : mh::Next() & 0xFF); break;
    case kSuperCombo_DrawCount: a[0] = high | (mh::Half() ? mh::Next() % 10 : mh::Next() & 0xFF); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    g_regions[8].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_texts));
    for (unsigned t = 0; t < 4; ++t) g_real_texts[t] = static_cast<std::uint32_t>(Long(mh::Mem(kTexts + 4 * t)));
    mh::Group group = {
        "magic_s02", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    // ElemStrikeFx_Run reads its phase +2 after a call and indexes a table of
    // three with it; no other dispatcher reads its phase after a call.
    group.phase_span = 3;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s02
