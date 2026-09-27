// BOF3X_SHADOW=magic_s12: group S12's two overlays (MAGIC060, MAGIC062)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s12.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC060 / MAGIC062 --clones
// (2026-09-26; capstone, every jump internal but Identify_DrawElements' jump
// table, which the harness moves into the copy; no REFUSED line), names
// given. Beyond the standard set this group lists the text calls, the draw
// callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), Sprite_SetTint, the engine's 0x453300
// and the functions of its own that its functions call directly. What the
// harness lacks is built here, not in the harness:
//
//   - Text_DrawAt and Text_CharCount log the text they are given (its bytes
//     to the NUL, and the pointer unless it is in the caller's stack: the
//     panel prints numbers into a stack buffer);
//   - Crt_sprintf runs for real on both sides (kThrough), so the number's
//     text is what Text_DrawAt logs;
//   - Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's bytes and
//     move Gfx_PacketNext on through a packet buffer of the fuzz's own (every
//     primitive of a draw is built at the same pointer, so the state compare
//     alone would see only the last);
//   - MagicFx_LinkByDepth logs its depths (deref) and the four primitives it
//     links;
//   - Celerity_ApplyStat's stat and the cell it moves are chosen together in
//     the seed (the stat below 0x10, so the cell stays in the record).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s12.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s12 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC060 / MAGIC062 --clones, 2026-09-26, names
// given; Identify_WasSeen answers al, Identify_DrawItem the whole of eax
// (Identify_DrawEnemy passes its upper half on), CeleritySpark_Draw runs calm
// (it indexes its stack by +9 read again, as group S18's BuffSpike_Draw).
// 0x4B0D50: 0x3E bytes  Identify_Task
constexpr mh::Imm kImms4B0D50[] = {{0xF, 0x4B0D90}, {0x17, 0x4B0E00}, {0x22, 0x4B0E50}, {0x2A, 0x4B0E90}, {0x32, 0x4B0ED0}};
// 0x4B0D90: 0x70 bytes  Identify_Start
constexpr mh::CallSite kCalls4B0D90[] = {{0x4, 0x435180}, {0x2F, 0x435180}, {0x59, 0x4B0EF0}};
// 0x4B0E00: 0x46 bytes  Identify_WaitOpen
constexpr mh::CallSite kCalls4B0E00[] = {{0xF, 0x435180}};
// 0x4B0E50: 0x32 bytes  Identify_ShowTimed
constexpr mh::CallSite kCalls4B0E50[] = {{0x9, 0x4B1090}, {0x10, 0x4B11F0}};
// 0x4B0E90: 0x31 bytes  Identify_ShowUntilInput
constexpr mh::CallSite kCalls4B0E90[] = {{0x9, 0x4B1090}, {0x10, 0x4B11F0}};
// 0x4B0ED0: 0x18 bytes  Identify_End
constexpr mh::CallSite kCalls4B0ED0[] = {{0x12, 0x4351F0}};
// 0x4B0EF0: 0xF3 bytes  Identify_Roll
constexpr mh::CallSite kCalls4B0EF0[] = {{0x8D, 0x5B93D2}, {0x9E, 0x4B1040}, {0xD5, 0x4B0FF0}};
// 0x4B0FF0: 0x4D bytes  Identify_MarkSeen
// 0x4B1040: 0x4D bytes  Identify_WasSeen
// 0x4B1090: 0x15E bytes  Identify_DrawMember
constexpr mh::CallSite kCalls4B1090[] = {{0x32, 0x57D800}, {0x4B, 0x516B30}, {0x6F, 0x57D800}, {0x90, 0x516B30}, {0xA2, 0x516B30}, {0xBA, 0x516B30}, {0xCF, 0x516B30}, {0xE4, 0x516B30}, {0xF9, 0x516B30}, {0x106, 0x57D800}, {0x12A, 0x516B30}, {0x13F, 0x516B30}, {0x154, 0x516B30}};
// 0x4B11F0: 0x22B bytes  Identify_DrawEnemy
constexpr mh::CallSite kCalls4B11F0[] = {{0x15, 0x57D800}, {0x52, 0x57D800}, {0x6B, 0x516B30}, {0x87, 0x516B30}, {0x90, 0x4B1520}, {0xA9, 0x5B9380}, {0xBB, 0x516B30}, {0xD3, 0x516B30}, {0xEC, 0x5B9380}, {0xFE, 0x516B30}, {0x113, 0x516B30}, {0x120, 0x57D800}, {0x139, 0x516B30}, {0x155, 0x516B30}, {0x167, 0x516B30}, {0x17C, 0x516B30}, {0x194, 0x516B30}, {0x1A9, 0x516B30}, {0x1BE, 0x516B30}, {0x1CB, 0x57D800}, {0x1EF, 0x516B30}, {0x206, 0x4B1420}, {0x21D, 0x4B1420}};
// 0x4B1420: 0xFD bytes  Identify_DrawItem
constexpr mh::CallSite kCalls4B1420[] = {{0x3E, 0x57D800}, {0x65, 0x516B30}, {0x97, 0x57D800}, {0xBE, 0x516B30}, {0xF3, 0x516B30}};
// 0x4B1520: 0x198 bytes  Identify_DrawElements
constexpr mh::CallSite kCalls4B1520[] = {{0x27, 0x5A7760}, {0x12E, 0x5A79E0}, {0x15B, 0x461E50}};
constexpr mh::JumpTable kTables4B1520[] = {{0x70, 0x180, 6}};
// 0x4B16C0: 0x12 bytes; +0xB note: jmp through .data 0x65aad0, 3 code entries (a data_tables entry)  IdentifyChild_Task
// 0x4B16E0: 0x29 bytes; +0xB note: call through .data 0x65aadc, 4 code entries (a data_tables entry)  IdentifyDim_Run
constexpr mh::CallSite kCalls4B16E0[] = {{0x23, 0x4B1770}};
// 0x4B1710: 0x2B bytes  IdentifyDim_FadeIn
// 0x4B1740: 0x27 bytes  MagicFx_CountDownRelease
constexpr mh::CallSite kCalls4B1740[] = {{0x21, 0x4351F0}};
// 0x4B1770: 0x9F bytes  IdentifyDim_Draw
constexpr mh::CallSite kCalls4B1770[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7740}, {0x2C, 0x5A7780}, {0x75, 0x461E50}, {0x89, 0x5A77C0}, {0x95, 0x461E50}};
// 0x4B1810: 0x29 bytes; +0xB note: call through .data 0x65aaec, 4 code entries (a data_tables entry)  IdentifyDisc_Run
constexpr mh::CallSite kCalls4B1810[] = {{0x23, 0x4B18E0}};
// 0x4B1840: 0x1E bytes  IdentifyDisc_WaitDim
// 0x4B1860: 0x2B bytes  IdentifyDisc_Grow
// 0x4B1890: 0x14 bytes  IdentifyFx_WaitClose
// 0x4B18B0: 0x28 bytes  MagicFx_CountDown2Release
constexpr mh::CallSite kCalls4B18B0[] = {{0x22, 0x4351F0}};
// 0x4B18E0: 0x260 bytes  IdentifyDisc_Draw
constexpr mh::CallSite kCalls4B18E0[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A7610}, {0x2D, 0x5A7780}, {0x5B, 0x5A7A00}, {0x80, 0x5A7A50}, {0xB5, 0x5A7A00}, {0xDB, 0x5A7A50}, {0x10F, 0x5A7A00}, {0x135, 0x5A7A50}, {0x16D, 0x5A7A00}, {0x192, 0x5A7A50}, {0x239, 0x461E50}, {0x24C, 0x5A77C0}, {0x255, 0x461E50}};
// 0x4B1B40: 0x12 bytes; +0xB note: jmp through .data 0x65aafc, 3 code entries (a data_tables entry)  IdentifyTint_Task
// 0x4B1B60: 0x3D bytes  IdentifyTint_Start
constexpr mh::CallSite kCalls4B1B60[] = {{0x6, 0x454DC0}, {0x1A, 0x454CC0}};
// 0x4B1BA0: 0x63 bytes  IdentifyTint_Brighten
// 0x4B1C10: 0x9B bytes  IdentifyTint_Dim
constexpr mh::CallSite kCalls4B1C10[] = {{0x79, 0x454DC0}, {0x85, 0x4FBDB0}, {0x95, 0x4351F0}};
// 0x4B1CB0: 0x46 bytes  Celerity_Task
constexpr mh::Imm kImms4B1CB0[] = {{0xF, 0x4B1D00}, {0x17, 0x4B1E70}, {0x22, 0x4B1ED0}, {0x2A, 0x4C03B0}, {0x32, 0x4B1F40}, {0x3A, 0x4B2040}};
// 0x4B1D00: 0x165 bytes  Celerity_Start
constexpr mh::CallSite kCalls4B1D00[] = {{0x5A, 0x435180}, {0xBD, 0x435180}, {0x15C, 0x587900}};
// 0x4B1F40: 0xF2 bytes  Celerity_Apply
constexpr mh::CallSite kCalls4B1F40[] = {{0x6E, 0x454DC0}, {0x7A, 0x4FBDB0}, {0x8B, 0x4B2060}, {0x94, 0x435180}};
// 0x4B2040: 0x20 bytes  Celerity_End
constexpr mh::CallSite kCalls4B2040[] = {{0x1A, 0x4351F0}};
// 0x4B2060: 0xB3 bytes  Celerity_ApplyStat
constexpr mh::CallSite kCalls4B2060[] = {{0xA8, 0x453300}};
// 0x4B2120: 0x12 bytes; +0xB note: jmp through .data 0x65ab78, 2 code entries (a data_tables entry)  CelerityChild_Task
// 0x4B2140: 0x32 bytes; +0xB note: call through .data 0x65ab80, 3 code entries (a data_tables entry)  CelerityRing_Run
constexpr mh::CallSite kCalls4B2140[] = {{0x1D, 0x4B7D40}, {0x22, 0x4B2180}, {0x27, 0x4B2380}, {0x2C, 0x5A7BC0}};
// 0x4B2180: 0x1F8 bytes  CelerityRing_DrawDisc
constexpr mh::CallSite kCalls4B2180[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x37, 0x5B93D2}, {0x56, 0x5B93D2}, {0x75, 0x5B93D2}, {0x95, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xEB, 0x5A7A00}, {0xFD, 0x5A7A50}, {0x12A, 0x5A75F0}, {0x132, 0x5A7780}, {0x15C, 0x5A84A0}, {0x162, 0x5A9310}, {0x1B9, 0x461E50}, {0x1E0, 0x5A77C0}, {0x1E9, 0x461E50}};
// 0x4B2380: 0x1EB bytes  CelerityRing_DrawBand
constexpr mh::CallSite kCalls4B2380[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x28, 0x5A7A00}, {0x3A, 0x5A7A50}, {0x4C, 0x5A7A00}, {0x5E, 0x5A7A50}, {0xB0, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xD4, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x11A, 0x5A7610}, {0x121, 0x5A7780}, {0x154, 0x5A85F0}, {0x15D, 0x5A9350}, {0x1AC, 0x461E50}, {0x1D2, 0x5A77C0}, {0x1DB, 0x461E50}};
// 0x4B2570: 0x12 bytes; +0xB note: jmp through .data 0x65ab8c, 6 code entries (a data_tables entry)  CeleritySpark_Run
// 0x4B2590: 0x95 bytes  CeleritySpark_Rise
constexpr mh::CallSite kCalls4B2590[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4B2D70}, {0x19, 0x4B7D40}, {0x1E, 0x4B28B0}, {0x23, 0x5A7BC0}};
// 0x4B2630: 0x82 bytes  CeleritySpark_Fall
constexpr mh::CallSite kCalls4B2630[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4B2D70}, {0x19, 0x4B7D40}, {0x1E, 0x4B28B0}, {0x23, 0x5A7BC0}};
// 0x4B26C0: 0xD4 bytes  CeleritySpark_Spin
constexpr mh::CallSite kCalls4B26C0[] = {{0x0, 0x4FBD10}, {0x16, 0x5B93D2}, {0x23, 0x5B93D2}, {0x2F, 0x4B2D70}, {0x37, 0x4B7D40}, {0x3C, 0x4B28B0}, {0x41, 0x5A7BC0}, {0xA5, 0x587900}};
// 0x4B27A0: 0x104 bytes  CeleritySpark_Orbit
constexpr mh::CallSite kCalls4B27A0[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4B2D70}, {0x16, 0x4B7D40}, {0x1B, 0x4B28B0}, {0x20, 0x5A7BC0}, {0x86, 0x5A7A00}, {0xB4, 0x5A7A50}, {0xFE, 0x4351F0}};
// 0x4B28B0: 0x4B9 bytes  CeleritySpark_Draw
constexpr mh::CallSite kCalls4B28B0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0xF6, 0x5A7A00}, {0x116, 0x5A7A50}, {0x14D, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x193, 0x5A75F0}, {0x19A, 0x5A7780}, {0x1C4, 0x5A87A0}, {0x277, 0x4FB880}, {0x32C, 0x5A7A00}, {0x34C, 0x5A7A50}, {0x383, 0x5A7A00}, {0x3A3, 0x5A7A50}, {0x3C9, 0x5A75F0}, {0x3D0, 0x5A7780}, {0x3FA, 0x5A87A0}, {0x4A9, 0x4FB880}};
// 0x4B2D70: 0x1C5 bytes  CeleritySpark_DrawDisc
constexpr mh::CallSite kCalls4B2D70[] = {{0x68, 0x5A77C0}, {0x7E, 0x572FA0}, {0x8A, 0x5A75F0}, {0x91, 0x5A7780}, {0xC0, 0x5A7A00}, {0xEA, 0x5A7A50}, {0x11A, 0x5A7A00}, {0x144, 0x5A7A50}, {0x1AD, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Identify_Task", 0x4B0D50, 0x3E, nullptr, 0, kImms4B0D50, MH_N(kImms4B0D50), nullptr, 0, reinterpret_cast<const void*>(&::Identify_Task)},
    {"Identify_Start", 0x4B0D90, 0x70, kCalls4B0D90, MH_N(kCalls4B0D90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_Start)},
    {"Identify_WaitOpen", 0x4B0E00, 0x46, kCalls4B0E00, MH_N(kCalls4B0E00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_WaitOpen)},
    {"Identify_ShowTimed", 0x4B0E50, 0x32, kCalls4B0E50, MH_N(kCalls4B0E50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_ShowTimed)},
    {"Identify_ShowUntilInput", 0x4B0E90, 0x31, kCalls4B0E90, MH_N(kCalls4B0E90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_ShowUntilInput)},
    {"Identify_End", 0x4B0ED0, 0x18, kCalls4B0ED0, MH_N(kCalls4B0ED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_End)},
    {"Identify_Roll", 0x4B0EF0, 0xF3, kCalls4B0EF0, MH_N(kCalls4B0EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_Roll)},
    {"Identify_MarkSeen", 0x4B0FF0, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_MarkSeen)},
    {"Identify_WasSeen", 0x4B1040, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_WasSeen), 0xFF},
    {"Identify_DrawMember", 0x4B1090, 0x15E, kCalls4B1090, MH_N(kCalls4B1090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_DrawMember)},
    {"Identify_DrawEnemy", 0x4B11F0, 0x22B, kCalls4B11F0, MH_N(kCalls4B11F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_DrawEnemy)},
    {"Identify_DrawItem", 0x4B1420, 0xFD, kCalls4B1420, MH_N(kCalls4B1420), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Identify_DrawItem), 0xFFFFFFFFu},
    {"Identify_DrawElements", 0x4B1520, 0x198, kCalls4B1520, MH_N(kCalls4B1520), nullptr, 0, kTables4B1520, MH_N(kTables4B1520), reinterpret_cast<const void*>(&::Identify_DrawElements)},
    {"IdentifyChild_Task", 0x4B16C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyChild_Task)},
    {"IdentifyDim_Run", 0x4B16E0, 0x29, kCalls4B16E0, MH_N(kCalls4B16E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDim_Run)},
    {"IdentifyDim_FadeIn", 0x4B1710, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDim_FadeIn)},
    {"MagicFx_CountDownRelease", 0x4B1740, 0x27, kCalls4B1740, MH_N(kCalls4B1740), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_CountDownRelease)},
    {"IdentifyDim_Draw", 0x4B1770, 0x9F, kCalls4B1770, MH_N(kCalls4B1770), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDim_Draw)},
    {"IdentifyDisc_Run", 0x4B1810, 0x29, kCalls4B1810, MH_N(kCalls4B1810), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDisc_Run)},
    {"IdentifyDisc_WaitDim", 0x4B1840, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDisc_WaitDim)},
    {"IdentifyDisc_Grow", 0x4B1860, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDisc_Grow)},
    {"IdentifyFx_WaitClose", 0x4B1890, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyFx_WaitClose)},
    {"MagicFx_CountDown2Release", 0x4B18B0, 0x28, kCalls4B18B0, MH_N(kCalls4B18B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_CountDown2Release)},
    {"IdentifyDisc_Draw", 0x4B18E0, 0x260, kCalls4B18E0, MH_N(kCalls4B18E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyDisc_Draw)},
    {"IdentifyTint_Task", 0x4B1B40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyTint_Task)},
    {"IdentifyTint_Start", 0x4B1B60, 0x3D, kCalls4B1B60, MH_N(kCalls4B1B60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyTint_Start)},
    {"IdentifyTint_Brighten", 0x4B1BA0, 0x63, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyTint_Brighten)},
    {"IdentifyTint_Dim", 0x4B1C10, 0x9B, kCalls4B1C10, MH_N(kCalls4B1C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IdentifyTint_Dim)},
    {"Celerity_Task", 0x4B1CB0, 0x46, nullptr, 0, kImms4B1CB0, MH_N(kImms4B1CB0), nullptr, 0, reinterpret_cast<const void*>(&::Celerity_Task)},
    {"Celerity_Start", 0x4B1D00, 0x165, kCalls4B1D00, MH_N(kCalls4B1D00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Celerity_Start)},
    {"Celerity_Apply", 0x4B1F40, 0xF2, kCalls4B1F40, MH_N(kCalls4B1F40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Celerity_Apply)},
    {"Celerity_End", 0x4B2040, 0x20, kCalls4B2040, MH_N(kCalls4B2040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Celerity_End)},
    {"Celerity_ApplyStat", 0x4B2060, 0xB3, kCalls4B2060, MH_N(kCalls4B2060), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Celerity_ApplyStat)},
    {"CelerityChild_Task", 0x4B2120, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CelerityChild_Task)},
    {"CelerityRing_Run", 0x4B2140, 0x32, kCalls4B2140, MH_N(kCalls4B2140), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CelerityRing_Run)},
    {"CelerityRing_DrawDisc", 0x4B2180, 0x1F8, kCalls4B2180, MH_N(kCalls4B2180), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CelerityRing_DrawDisc)},
    {"CelerityRing_DrawBand", 0x4B2380, 0x1EB, kCalls4B2380, MH_N(kCalls4B2380), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CelerityRing_DrawBand)},
    {"CeleritySpark_Run", 0x4B2570, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Run)},
    {"CeleritySpark_Rise", 0x4B2590, 0x95, kCalls4B2590, MH_N(kCalls4B2590), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Rise)},
    {"CeleritySpark_Fall", 0x4B2630, 0x82, kCalls4B2630, MH_N(kCalls4B2630), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Fall)},
    {"CeleritySpark_Spin", 0x4B26C0, 0xD4, kCalls4B26C0, MH_N(kCalls4B26C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Spin)},
    {"CeleritySpark_Orbit", 0x4B27A0, 0x104, kCalls4B27A0, MH_N(kCalls4B27A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Orbit)},
    {"CeleritySpark_Draw", 0x4B28B0, 0x4B9, kCalls4B28B0, MH_N(kCalls4B28B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_Draw), 0, true},
    {"CeleritySpark_DrawDisc", 0x4B2D70, 0x1C5, kCalls4B2D70, MH_N(kCalls4B2D70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CeleritySpark_DrawDisc)},
};
#undef MH_N

enum : unsigned {
    kIdentify_Task, kIdentify_Start, kIdentify_WaitOpen, kIdentify_ShowTimed, kIdentify_ShowUntilInput, kIdentify_End,
    kIdentify_Roll, kIdentify_MarkSeen, kIdentify_WasSeen, kIdentify_DrawMember, kIdentify_DrawEnemy, kIdentify_DrawItem,
    kIdentify_DrawElements, kIdentifyChild_Task, kIdentifyDim_Run, kIdentifyDim_FadeIn, kMagicFx_CountDownRelease,
    kIdentifyDim_Draw, kIdentifyDisc_Run, kIdentifyDisc_WaitDim, kIdentifyDisc_Grow, kIdentifyFx_WaitClose,
    kMagicFx_CountDown2Release, kIdentifyDisc_Draw, kIdentifyTint_Task, kIdentifyTint_Start, kIdentifyTint_Brighten,
    kIdentifyTint_Dim,
    kCelerity_Task, kCelerity_Start, kCelerity_Apply, kCelerity_End, kCelerity_ApplyStat, kCelerityChild_Task,
    kCelerityRing_Run, kCelerityRing_DrawDisc, kCelerityRing_DrawBand, kCeleritySpark_Run, kCeleritySpark_Rise,
    kCeleritySpark_Fall, kCeleritySpark_Spin, kCeleritySpark_Orbit, kCeleritySpark_Draw, kCeleritySpark_DrawDisc,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kScratch = 0x903850, kVertex = 0x9037A0, kTints = 0x7E0700, kSeenBits = 0x9040A8,
                        kInput = 0x7E1BEC, kAbility = 0x904B80, kAbilityStep = 0x65C4DB;

// Celerity_ApplyStat's stat, chosen by the seed with the cell it moves.
unsigned g_stat = 0;

// --- the callees' effects (after the recorder's log and disturbance) ------------

// A text a callee reads: its bytes to the NUL (at most 32), and where it is
// unless that is the caller's stack (the number buffer differs by frame).
void NoteText(std::uint32_t at) {
    const auto* s = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(at));
    unsigned n = 0;
    while (n < 32 && s[n] != 0) ++n;
    mh::NoteBytes(s, n < 32 ? n + 1 : n);
    unsigned char here = 0;
    const std::uint32_t frame = Key(&here);
    const bool on_stack = at - frame < 0x10000u || frame - at < 0x10000u;
    if (!on_stack) mh::Note(at);
}
std::uint32_t DrawAtEffect(const std::uint32_t* a, std::uint32_t answer) {
    NoteText(a[4]);
    return answer;
}
std::uint32_t CharCountEffect(const std::uint32_t* a, std::uint32_t answer) {
    NoteText(a[0]);
    return answer;
}
// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer.
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x200 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[3]);
    return answer;
}
// MagicFx_LinkByDepth: the primitives it links (count x size bytes at prims).
std::uint32_t LinkDepthsEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[3])), (a[4] & 0xFF) * (a[5] & 0xFF));
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S12_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S12_THEIRS(name) #name, KeyOf(name), KeyOf(name)
#define S12_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // the text: x and y are stored as words; the text by its bytes
    {S12_OURS(Text_DrawAt), 5, {kU16, kU16, kAll, kAll, 0}, kG, 0, 0, {}, &DrawAtEffect},
    {S12_OURS(Text_CharCount), 1, {0}, mh::Answer::kByte, 0, 0x14, {}, &CharCountEffect},
    {S12_THEIRS(Crt_sprintf), 3, {kAll, kAll, kAll}, mh::Answer::kThrough, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items, magic_lib: all ours)
    {S12_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S12_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S12_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S12_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S12_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gpu_SetTile), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gpu_SetCode6C), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S12_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S12_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S12_OURS(Gte_RotAverage3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S12_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S12_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // the effect library's: the depths by their 16 bytes, the primitives
    {S12_OURS(MagicFx_LinkByDepth), 7, {kAll, kAll, 0, kAll, kU8, kU8, kU8}, kG, 0, 0, {0, 0, 16}, &LinkDepthsEffect},
    {S12_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    // Capcom's, unnamed: a stat change for the target
    {S12_RAW(0x453300), 1, {kU8}, kG, 0, 0},
    // this group's own, called directly
    {S12_RAW(0x4B0EF0), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B0FF0), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B1040), 0, {}, mh::Answer::kFlag, 0, 0},
    {S12_RAW(0x4B1090), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B11F0), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B1420), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S12_RAW(0x4B1520), 2, {kU16, kU16}, kG, 0, 0},
    {S12_RAW(0x4B1770), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B18E0), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B2060), 1, {kU8}, kG, 0, 0},
    {S12_RAW(0x4B2180), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B2380), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B28B0), 0, {}, kG, 0, 0},
    {S12_RAW(0x4B2D70), 1, {kU16}, kG, 0, 0},
};
#undef S12_OURS
#undef S12_THEIRS
#undef S12_RAW

// The seven .data handler tables the dispatchers read in place
// (IdentifyChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65AAD0, 3}, {0x65AADC, 4}, {0x65AAEC, 4}, {0x65AAFC, 3}, {0x65AB78, 2}, {0x65AB80, 3}, {0x65AB8C, 6},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},        // Gfx_PacketNext
    {0, kPrimBytes},      // g_prims (filled in at start-up)
    {kScratch, 0x10},     // DamageScratch 0x903850..0x90385F
    {kVertex, 0x20},      // Prim_VertexScratch, four SVECTORs
    {kTints, 0xC00},      // MoveScript_TintRecords (the record index is a byte)
    {0x80E980, 0x40},     // Celerity_Start's CLUT strip, from
    {0x812980, 0x40},     // and to
    {0x904B50, 0x40},     // the result record pointer 0x904B60, the ability word 0x904B80
    {0x90465C, 4},        // Celerity_End's byte
    {kSeenBits, 0x20},    // the kinds identified
    {kInput, 2},          // Input_Pressed
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(mh::at::kOwner); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: mh::Mem(kScratch + v % 16)[0] = Byte(h >> 24); break;
    case 2: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 3: mh::Mem(kTints + v % 0xC00)[0] = Byte(h >> 24); break;
    case 4: SetWord(mh::Mem(kInput), (h >> 16) & 1 ? 0 : h >> 20); break;
    case 5: SetLong(mh::Mem(kSeenBits + 4 * (v % 8)), static_cast<std::int32_t>(h)); break;
    case 6: {
        // the target enemy's "identifiable" byte +0x8F, 0 or not: the panel and
        // the roll read it after calls (an enemy target only, as the harness's
        // own record disturbance)
        const unsigned t = mh::Mem(mh::at::kTarget)[0];
        if (t >= 3 && t <= 10)
            mh::Mem(mh::at::kEnemies + (t - 3) * mh::at::kEnemyStride + 0x8F)[0] = (h >> 16) & 1 ? 0 : Byte(h >> 24);
        break;
    }
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
unsigned char* PartyRec(unsigned i) { return mh::Mem(mh::at::kParty + i * mh::at::kPartyStride); }
unsigned char* EnemyRec(int i) { return mh::Mem(mh::at::kEnemies + static_cast<std::uint32_t>(i) * mh::at::kEnemyStride); }
unsigned char& Target() { return mh::Mem(mh::at::kTarget)[0]; }
// A target on the enemy side (3..10) or the party's (0..2).
void EnemyTarget() { Target() = Byte(3 + mh::Next() % 8); }
void PartyTarget() { Target() = Byte(mh::Next() % 3); }

// The roll: the level gap at each threshold's two sides, Rand's first answer
// either side of the chance.
void SeedRoll() {
    if (mh::Often()) EnemyTarget();
    unsigned char& actor = mh::Mem(mh::at::kActor)[0];
    actor = Byte(mh::Next() % 3);
    static const int kGaps[] = {-17, -16, -15, -11, -10, -6, -5, -1, 0, 4, 5, 9, 10, 14, 15, 16, 40};
    const int gap = kGaps[mh::Next() % (sizeof kGaps / sizeof kGaps[0])];
    const int level = 40 + static_cast<int>(mh::Next() % 150);
    if (mh::Often()) {
        SetWord(EnemyRec(static_cast<int>(Target()) - 3) + 0x98, static_cast<unsigned>(level));
        PartyRec(actor)[0x8A] = Byte(static_cast<unsigned>(level + gap));
    }
    int chance = 0x10;
    if (gap < 15) chance = 0xE;
    if (gap < 10) chance = 0xC;
    if (gap < 5) chance = 0xA;
    if (gap < 0) chance = 8;
    if (gap < -5) chance = 6;
    if (gap < -10) chance = 4;
    if (gap < -15) chance = 2;
    mh::SetRandFirst(static_cast<int>(((mh::Next() & 0xF) << 4) | static_cast<unsigned>(chance - (mh::Half() ? 1 : 0))) & 0xFF);
    if (mh::Half()) EnemyRec(static_cast<int>(Target()) - 3)[0x8F] = 0;
}

// Celerity_ApplyStat: the stat below 0x10, the ability word inside 256 records,
// the cell so that it plus the step lands either side of 100 or -100.
void SeedApplyStat() {
    g_stat = mh::Next() % 0x10;
    if (mh::Half()) g_stat = MH_PICK(1, 2, 0, 3);
    SetWord(mh::Mem(kAbility), mh::Next() % 0x100);
    const unsigned char t = Target();
    unsigned char* const rec = t < 3 ? PartyRec(t) + 0x124 : EnemyRec(static_cast<int>(t) - 3) + 0x104;
    const int step = static_cast<signed char>(mh::Mem(kAbilityStep + 0x18u * Word(mh::Mem(kAbility)))[0]);
    static const int kLands[] = {99, 100, 101, -99, -100, -101, 0};
    if (mh::Often()) rec[0x14 + g_stat] = Byte(static_cast<unsigned>(kLands[mh::Next() % 7] - step));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    if (mh::Often()) sc[4] = Byte(mh::Next() % 4);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kIdentify_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kCelerity_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kIdentifyChild_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kCelerityChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kIdentifyDim_Run: case kIdentifyDisc_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kIdentifyTint_Task: case kCelerityRing_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kCeleritySpark_Run: sc[2] = Byte(mh::Next() % 6); break;
    // MAGIC060's task
    case kIdentify_WaitOpen: sc[0xB] = Byte(MH_PICK(1, 2, 2, 3)); break;
    case kIdentify_ShowTimed: case kIdentify_ShowUntilInput:
        if (mh::Half()) PartyTarget(); else EnemyTarget();
        Near(sc[9], 1);
        if (mh::Half()) SetWord(mh::Mem(kInput), 0);
        break;
    case kIdentify_End: sc[0xB] = Byte(MH_PICK(0x7F, 0x80, 0x80, 0x81)); break;
    case kIdentify_Roll: SeedRoll(); break;
    case kIdentify_MarkSeen: case kIdentify_WasSeen: if (mh::Often()) EnemyTarget(); break;
    // Identify_DrawMember zeroes and restores a byte of the member's record:
    // a member of the five the regions hold, or the zero would outlive the
    // round. Identify_DrawEnemy indexes the enemies by the target byte - 3 as
    // a byte: its caller sends it 3 and above only.
    case kIdentify_DrawMember: Target() = Byte(mh::Often() ? mh::Next() % 3 : mh::Next() % 5); break;
    case kIdentify_DrawEnemy:
        EnemyTarget();
        if (mh::Half()) EnemyRec(static_cast<int>(Target()) - 3)[0x8F] = 0;
        break;
    case kIdentify_DrawItem: if (mh::Half()) sc[4] = 0; break;
    case kIdentify_DrawElements: {
        if (mh::Often()) EnemyTarget();
        unsigned char* const rec = EnemyRec(static_cast<int>(Target()) - 3);
        for (unsigned off = 0xBF; off <= 0xC4; ++off)
            if (mh::Often()) rec[off] = Byte(mh::Next() % 5);
        break;
    }
    // MAGIC060's children
    case kIdentifyDim_FadeIn: Near(sc[9], 0xF); break;
    case kMagicFx_CountDownRelease: Near(sc[9], 1); break;
    case kIdentifyDisc_WaitDim: Owner()[0xB] = Byte(MH_PICK(0, 1, 1, 2)); break;
    case kIdentifyDisc_Grow: Near(sc[9], 0x1D); break;
    case kIdentifyFx_WaitClose: Owner()[0xB] = Byte(MH_PICK(0x82, 0x83, 0x83, 0x84)); break;
    case kMagicFx_CountDown2Release: sc[9] = Byte(MH_PICK(1, 2, 2, 3, 4)); break;
    case kIdentifyTint_Brighten:
        mh::Mem(kTints + sc[0xB] * 12u + 2)[0] = Byte(MH_PICK(6, 7, 7, 8, 9));
        break;
    case kIdentifyTint_Dim:
        mh::Mem(kTints + sc[0xB] * 12u + 2)[0] = Byte(MH_PICK(0, 1, 1, 2));
        if (mh::Half()) Owner()[0xB] = Byte(Owner()[0xB] & 0x7F);
        break;
    // MAGIC062
    case kCelerity_Apply: Near(sc[9], 1); break;
    case kCelerity_End: if (mh::Half()) sc[0xB] = 0; break;
    case kCelerity_ApplyStat: SeedApplyStat(); break;
    case kCeleritySpark_Rise: case kCeleritySpark_Fall: Near(sc[0xA], 1); break;
    case kCeleritySpark_Spin: {
        const std::uint32_t spin = MH_PICK(3, 4, 4, 5, 0xFFFFFFFFu, 0x80000000u);
        if (mh::Often()) SetLong(sc + 0xC, static_cast<std::int32_t>(spin));
        sc[0xA] = Byte(MH_PICK(1, 1, 4, 5, 6, 0x80));
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 2);
        if (mh::Half()) sc[0x10] = Byte(MH_PICK(7, 0xF, 0));
        break;
    }
    case kCeleritySpark_Orbit: Near(sc[0xA], 0xF); break;
    default: break;
    }
}

// The functions that take arguments: Identify_DrawItem (an item word with
// the category in its high byte, as Identify_DrawEnemy passes it, or any
// category), Identify_DrawElements (its caller's point, or any),
// Celerity_ApplyStat (the seed's stat), CeleritySpark_DrawDisc (a radius).
void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kIdentify_DrawItem:
        if (mh::Often()) a[1] = MH_PICK(0, 1, 2, 3, 4, 0x80);
        if (mh::Half()) a[0] = (a[0] & 0xFFFF00FFu) | ((a[1] & 0xFF) << 8);
        break;
    case kIdentify_DrawElements:
        if (mh::Half()) {
            a[0] = 0x66;
            a[1] = 0x54;
        }
        break;
    case kCelerity_ApplyStat: a[0] = (a[0] & 0xFFFFFF00u) | g_stat; break;
    case kCeleritySpark_DrawDisc: if (mh::Often()) a[0] = (a[0] & 0xFFFF0000u) | MH_PICK(0x18, 0x1B, 0x20, 0x40, 0x43); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s12", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s12
