// BOF3X_SHADOW=magic_s15: group S15's three overlays (MAGIC067, 068, 069)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s15.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC067 / 068 / 069 --clones
// (2026-09-26; capstone, every jump internal but the jump table of
// ChillRay_PushMatrix, which the harness moves into the copy; no REFUSED
// line), names given. Beyond the standard set this group lists the draw
// callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim), the sprite and tint calls, Battle_ActorIsOut, the engine's
// 0x446770, and the functions of its own its functions call directly.
// Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim logs each primitive's bytes (every primitive
//     of a draw is built in the same buffer, so the state compare alone would
//     see only the last) and moves Gfx_PacketNext on through a packet buffer
//     of the fuzz's own, as the real one does;
//   - the GTE callees of the ray's matrix push log what their pointers point
//     at (`deref`) and write a result where the real ones write;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - Sprite_SetAnimation logs which sprite it acts on;
//   - Battle_ActorIsOut keeps one enemy in, and that enemy's divisor word
//     +0xB0 not 0, while Foretell_Read is fuzzed (its idiv by the count of
//     enemies counted, and by each one's word, faults in the original and
//     aborts ours; the members' side is seeded);
//   - Gte_PushMatrix keeps the facing byte inside ChillRay_PushMatrix's
//     four-entry jump table while that function is fuzzed (past it the
//     original turns by an uninitialised stack word and ours aborts).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s15.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s15 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC067 / 068 / 069 --clones, 2026-09-26, names
// given.
constexpr mh::Imm kImms4B66D0[] = {{0xF, 0x4B6710}, {0x17, 0x4E75B0}, {0x22, 0x4B6890}, {0x2A, 0x4B68C0}, {0x32, 0x4E5200}};
constexpr mh::CallSite kCalls4B6710[] = {{0x21, 0x435180}, {0x62, 0x435180}, {0x10C, 0x435180}};
constexpr mh::CallSite kCalls4B6890[] = {{0x13, 0x4530D0}};
constexpr mh::CallSite kCalls4B68C0[] = {{0x3C, 0x4456C0}, {0x4C, 0x435180}, {0xDD, 0x4456C0}, {0xED, 0x435180}};
constexpr mh::CallSite kCalls4B6A60[] = {{0x1D, 0x4B6BC0}, {0x22, 0x4B6CA0}, {0x27, 0x5A7BC0}};
constexpr mh::CallSite kCalls4B6A90[] = {{0x0, 0x4FC0E0}, {0x25, 0x446770}, {0x55, 0x587740}};
constexpr mh::CallSite kCalls4B6BC0[] = {{0x3, 0x5A7B90}, {0x8B, 0x5A8200}, {0x9A, 0x5A8060}, {0xAE, 0x5A7D70}, {0xB8, 0x5A8DE0}, {0xC2, 0x5A8E00}};
constexpr mh::JumpTable kTables4B6BC0[] = {{0x26, 0xCC, 4}};
constexpr mh::CallSite kCalls4B6CA0[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x6B, 0x5A7A00}, {0x88, 0x5A7A50}, {0xA8, 0x5A7A00}, {0xC5, 0x5A7A00}, {0xE2, 0x5A7A50}, {0x102, 0x5A7A00}, {0x137, 0x5A7630}, {0x13F, 0x5A7780}, {0x1BD, 0x5A7A00}, {0x1DA, 0x5A7A50}, {0x1FA, 0x5A7A00}, {0x217, 0x5A7A00}, {0x234, 0x5A7A50}, {0x254, 0x5A7A00}, {0x476, 0x5A79A0}, {0x486, 0x5A79E0}, {0x517, 0x5A85F0}, {0x520, 0x5A93A0}, {0x529, 0x461E50}, {0x551, 0x5A77C0}, {0x55A, 0x461E50}};
constexpr mh::CallSite kCalls4B7210[] = {{0x1D, 0x4B7240}};
constexpr mh::CallSite kCalls4B7240[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A7610}, {0x2D, 0x5A7780}, {0x10B, 0x461E50}, {0x11D, 0x5A77C0}, {0x126, 0x461E50}};
constexpr mh::Imm kImms4B73A0[] = {{0xF, 0x4B73D0}, {0x17, 0x43EC10}, {0x22, 0x4AEE90}};
constexpr mh::CallSite kCalls4B73D0[] = {{0x16, 0x5891F0}};
constexpr mh::Imm kImms4B7400[] = {{0xF, 0x4B7440}, {0x17, 0x4B74C0}, {0x22, 0x4B7880}, {0x2A, 0x4B78B0}, {0x32, 0x4B78D0}};
constexpr mh::CallSite kCalls4B7440[] = {{0x48, 0x435180}};
constexpr mh::CallSite kCalls4B74C0[] = {{0xF8, 0x4456C0}, {0x28C, 0x497740}, {0x296, 0x44A880}, {0x2AC, 0x497740}, {0x2B6, 0x44A880}, {0x2CC, 0x497740}, {0x2D6, 0x44A880}, {0x2EC, 0x497740}, {0x2F6, 0x44A880}, {0x306, 0x497740}, {0x310, 0x44A880}, {0x325, 0x4456C0}};
constexpr mh::CallSite kCalls4B78B0[] = {{0x0, 0x4B7900}};
constexpr mh::CallSite kCalls4B78D0[] = {{0x10, 0x4351F0}, {0x27, 0x4B7900}};
constexpr mh::CallSite kCalls4B7900[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x5B, 0x5A7740}, {0xA2, 0x461E50}, {0xCA, 0x5A7740}, {0xFE, 0x461E50}};
constexpr mh::CallSite kCalls4B7A50[] = {{0x4A, 0x4FBD10}, {0x54, 0x587900}};
constexpr mh::CallSite kCalls4B7AE0[] = {{0x0, 0x4B7D40}, {0x5, 0x4B7BD0}, {0xA, 0x5A7BC0}};
constexpr mh::CallSite kCalls4B7B20[] = {{0x3B, 0x587900}, {0x51, 0x587900}};
constexpr mh::CallSite kCalls4B7B90[] = {{0x0, 0x4B7D40}, {0x5, 0x4B7BD0}, {0xA, 0x5A7BC0}, {0x30, 0x4351F0}};
constexpr mh::CallSite kCalls4B7BD0[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A75D0}, {0xFD, 0x5A79A0}, {0x10D, 0x5A79E0}, {0x15F, 0x461E50}};
constexpr mh::Imm kImms4B7DE0[] = {{0xF, 0x4B7E10}, {0x17, 0x4F7350}};
constexpr mh::CallSite kCalls4B7E10[] = {{0x40, 0x435180}, {0x94, 0x435180}, {0x11F, 0x587900}};
constexpr mh::CallSite kCalls4B7F60[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x43, 0x4B8910}, {0x48, 0x4B8690}, {0x5D, 0x4B8A90}, {0x70, 0x5A77C0}, {0x79, 0x461E50}};
constexpr mh::CallSite kCalls4B8060[] = {{0x31, 0x587900}, {0x4A, 0x4351F0}};
constexpr mh::CallSite kCalls4B80B0[] = {{0x18, 0x4B8BD0}};
constexpr mh::CallSite kCalls4B80E0[] = {{0x4C, 0x587900}};
constexpr mh::CallSite kCalls4B8150[] = {{0x21, 0x4351F0}};
constexpr mh::CallSite kCalls4B8180[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x43, 0x4B8910}, {0x48, 0x4B8690}, {0x5B, 0x5A77C0}, {0x64, 0x461E50}};
constexpr mh::CallSite kCalls4B8260[] = {{0x2E, 0x4351F0}};
constexpr mh::CallSite kCalls4B82C0[] = {{0x3A, 0x454DC0}, {0x48, 0x454CC0}};
constexpr mh::CallSite kCalls4B8440[] = {{0x6C, 0x587900}, {0x7D, 0x454D60}, {0x8B, 0x4FBDB0}};
constexpr mh::CallSite kCalls4B84F0[] = {{0x0, 0x4B8530}};
constexpr mh::CallSite kCalls4B8530[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A75D0}, {0xF0, 0x5A79A0}, {0x100, 0x5A79E0}, {0x14D, 0x461E50}};
constexpr mh::CallSite kCalls4B8690[] = {{0x9B, 0x5A7A00}, {0xBE, 0x5A7A50}, {0xF3, 0x5A75F0}, {0xFB, 0x5A7780}, {0x13C, 0x5A7A00}, {0x16A, 0x5A7A50}, {0x1A8, 0x5A7A00}, {0x1D6, 0x5A7A50}, {0x240, 0x461E50}};
constexpr mh::CallSite kCalls4B8910[] = {{0x71, 0x5A7650}, {0x79, 0x5A7780}, {0x8D, 0x5A7A00}, {0xBB, 0x5A7A50}, {0xF1, 0x5A7A00}, {0x11F, 0x5A7A50}, {0x163, 0x461E50}};
constexpr mh::CallSite kCalls4B8A90[] = {{0x2D, 0x5A7650}, {0x35, 0x5A7780}, {0xA6, 0x461E50}, {0xB2, 0x5A7650}, {0xBA, 0x5A7780}, {0x12B, 0x461E50}};
constexpr mh::CallSite kCalls4B8BD0[] = {{0x1B, 0x4456C0}, {0x6D, 0x435180}, {0xDD, 0x4456C0}, {0x133, 0x435180}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Chill_Task", 0x4B66D0, 0x3E, nullptr, 0, kImms4B66D0, MH_N(kImms4B66D0), nullptr, 0, reinterpret_cast<const void*>(&::Chill_Task)},
    {"Chill_Start", 0x4B6710, 0x171, kCalls4B6710, MH_N(kCalls4B6710), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Chill_Start)},
    {"Chill_WaitChildren", 0x4B6890, 0x2E, kCalls4B6890, MH_N(kCalls4B6890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Chill_WaitChildren)},
    {"Chill_SpawnMarks", 0x4B68C0, 0x172, kCalls4B68C0, MH_N(kCalls4B68C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Chill_SpawnMarks)},
    {"ChillChild_Task", 0x4B6A40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillChild_Task)},
    {"ChillRay_Run", 0x4B6A60, 0x2D, kCalls4B6A60, MH_N(kCalls4B6A60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillRay_Run)},
    {"ChillRay_Start", 0x4B6A90, 0x8F, kCalls4B6A90, MH_N(kCalls4B6A90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillRay_Start)},
    {"ChillRay_Grow", 0x4B6B20, 0x43, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillRay_Grow)},
    {"ChillRay_Shrink", 0x4B6B70, 0x43, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillRay_Shrink)},
    {"ChillRay_PushMatrix", 0x4B6BC0, 0xDC, kCalls4B6BC0, MH_N(kCalls4B6BC0), nullptr, 0, kTables4B6BC0, MH_N(kTables4B6BC0), reinterpret_cast<const void*>(&::ChillRay_PushMatrix)},
    {"ChillRay_Draw", 0x4B6CA0, 0x567, kCalls4B6CA0, MH_N(kCalls4B6CA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillRay_Draw)},
    {"ChillFlash_Run", 0x4B7210, 0x23, kCalls4B7210, MH_N(kCalls4B7210), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillFlash_Run)},
    {"ChillFlash_Draw", 0x4B7240, 0x131, kCalls4B7240, MH_N(kCalls4B7240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillFlash_Draw)},
    {"ChillMark_Run", 0x4B7380, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillMark_Run)},
    {"ChillEnemy_Task", 0x4B73A0, 0x2E, nullptr, 0, kImms4B73A0, MH_N(kImms4B73A0), nullptr, 0, reinterpret_cast<const void*>(&::ChillEnemy_Task)},
    {"ChillEnemy_Start", 0x4B73D0, 0x27, kCalls4B73D0, MH_N(kCalls4B73D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChillEnemy_Start)},
    {"Foretell_Task", 0x4B7400, 0x3E, nullptr, 0, kImms4B7400, MH_N(kImms4B7400), nullptr, 0, reinterpret_cast<const void*>(&::Foretell_Task)},
    {"Foretell_Start", 0x4B7440, 0x7F, kCalls4B7440, MH_N(kCalls4B7440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_Start)},
    {"Foretell_Read", 0x4B74C0, 0x3BC, kCalls4B74C0, MH_N(kCalls4B74C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_Read)},
    {"Foretell_Pause", 0x4B7880, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_Pause)},
    {"Foretell_Show", 0x4B78B0, 0x19, kCalls4B78B0, MH_N(kCalls4B78B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_Show)},
    {"Foretell_End", 0x4B78D0, 0x2D, kCalls4B78D0, MH_N(kCalls4B78D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_End)},
    {"Foretell_DrawTiles", 0x4B7900, 0x10A, kCalls4B7900, MH_N(kCalls4B7900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Foretell_DrawTiles)},
    {"ForetellChild_Task", 0x4B7A10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellChild_Task)},
    {"ForetellOrb_Run", 0x4B7A30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_Run)},
    {"ForetellOrb_Start", 0x4B7A50, 0x82, kCalls4B7A50, MH_N(kCalls4B7A50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_Start)},
    {"ForetellOrb_Spin", 0x4B7AE0, 0x35, kCalls4B7AE0, MH_N(kCalls4B7AE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_Spin)},
    {"ForetellOrb_Count", 0x4B7B20, 0x6C, kCalls4B7B20, MH_N(kCalls4B7B20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_Count)},
    {"ForetellOrb_End", 0x4B7B90, 0x36, kCalls4B7B90, MH_N(kCalls4B7B90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_End)},
    {"ForetellOrb_Draw", 0x4B7BD0, 0x16A, kCalls4B7BD0, MH_N(kCalls4B7BD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ForetellOrb_Draw)},
    {"Influence_Task", 0x4B7DE0, 0x26, nullptr, 0, kImms4B7DE0, MH_N(kImms4B7DE0), nullptr, 0, reinterpret_cast<const void*>(&::Influence_Task)},
    {"Influence_Start", 0x4B7E10, 0x128, kCalls4B7E10, MH_N(kCalls4B7E10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Influence_Start)},
    {"InfluenceChild_Task", 0x4B7F40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceChild_Task)},
    {"InfluenceRight_Run", 0x4B7F60, 0x82, kCalls4B7F60, MH_N(kCalls4B7F60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Run)},
    {"InfluenceRight_Launch", 0x4B7FF0, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Launch)},
    {"InfluenceRight_Slide", 0x4B8060, 0x4F, kCalls4B8060, MH_N(kCalls4B8060), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Slide)},
    {"InfluenceRight_Pick", 0x4B80B0, 0x30, kCalls4B80B0, MH_N(kCalls4B80B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Pick)},
    {"InfluenceRight_Blink", 0x4B80E0, 0x67, kCalls4B80E0, MH_N(kCalls4B80E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Blink)},
    {"InfluenceRight_Shrink", 0x4B8150, 0x27, kCalls4B8150, MH_N(kCalls4B8150), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceRight_Shrink)},
    {"InfluenceLeft_Run", 0x4B8180, 0x6D, kCalls4B8180, MH_N(kCalls4B8180), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceLeft_Run)},
    {"InfluenceLeft_Launch", 0x4B81F0, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceLeft_Launch)},
    {"InfluenceLeft_Slide", 0x4B8260, 0x34, kCalls4B8260, MH_N(kCalls4B8260), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceLeft_Slide)},
    {"InfluenceMark_Run", 0x4B82A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Run)},
    {"InfluenceMark_Start", 0x4B82C0, 0x10D, kCalls4B82C0, MH_N(kCalls4B82C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Start)},
    {"InfluenceMark_Brighten", 0x4B83D0, 0x63, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Brighten)},
    {"InfluenceMark_Fade", 0x4B8440, 0xAF, kCalls4B8440, MH_N(kCalls4B8440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Fade)},
    {"InfluenceMark_Show", 0x4B84F0, 0x34, kCalls4B84F0, MH_N(kCalls4B84F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Show)},
    {"InfluenceMark_Draw", 0x4B8530, 0x158, kCalls4B8530, MH_N(kCalls4B8530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::InfluenceMark_Draw)},
    {"Influence_DrawTriangles", 0x4B8690, 0x275, kCalls4B8690, MH_N(kCalls4B8690), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Influence_DrawTriangles)},
    {"Influence_DrawRing", 0x4B8910, 0x179, kCalls4B8910, MH_N(kCalls4B8910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Influence_DrawRing)},
    {"Influence_DrawCross", 0x4B8A90, 0x136, kCalls4B8A90, MH_N(kCalls4B8A90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Influence_DrawCross)},
    {"Influence_SpawnMarks", 0x4B8BD0, 0x197, kCalls4B8BD0, MH_N(kCalls4B8BD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Influence_SpawnMarks)},
};
#undef MH_N

enum : unsigned {
    kChill_Task, kChill_Start, kChill_WaitChildren, kChill_SpawnMarks, kChillChild_Task, kChillRay_Run,
    kChillRay_Start, kChillRay_Grow, kChillRay_Shrink, kChillRay_PushMatrix, kChillRay_Draw, kChillFlash_Run,
    kChillFlash_Draw, kChillMark_Run, kChillEnemy_Task, kChillEnemy_Start,
    kForetell_Task, kForetell_Start, kForetell_Read, kForetell_Pause, kForetell_Show, kForetell_End,
    kForetell_DrawTiles, kForetellChild_Task, kForetellOrb_Run, kForetellOrb_Start, kForetellOrb_Spin,
    kForetellOrb_Count, kForetellOrb_End, kForetellOrb_Draw,
    kInfluence_Task, kInfluence_Start, kInfluenceChild_Task, kInfluenceRight_Run, kInfluenceRight_Launch,
    kInfluenceRight_Slide, kInfluenceRight_Pick, kInfluenceRight_Blink, kInfluenceRight_Shrink, kInfluenceLeft_Run,
    kInfluenceLeft_Launch, kInfluenceLeft_Slide, kInfluenceMark_Run, kInfluenceMark_Start, kInfluenceMark_Brighten,
    kInfluenceMark_Fade, kInfluenceMark_Show, kInfluenceMark_Draw, kInfluence_DrawTriangles, kInfluence_DrawRing,
    kInfluence_DrawCross, kInfluence_SpawnMarks, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kEventBattle = 0x904AAA, kKind2 = 0x905E60,
                        kTints = 0x7E0700, kInput = 0x7E1BEC, kEnemyLift = 0x8C564F;

// Set by the seed for the one function each matters to.
bool g_keep_facing = false;   // the facing stays inside 0..3 (ChillRay_PushMatrix)
bool g_keep_enemy = false;    // one enemy stays in, its divisor not 0 (Foretell_Read)
unsigned g_kept = 3;          // that enemy
bool g_keep_index = false;    // the task's actor index +0xB stays 0..10 (InfluenceMark_Start)

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim: the primitive at Gfx_PacketNext into the log (the real one
// links it), then Gfx_PacketNext on by its size, kept in the buffer (a draw
// writes up to 0x58 past it).
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    const std::uint32_t size = a[1];
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
    return answer;
}
// Sprite_SetAnimation acts on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
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
std::uint32_t KeepFacing(const std::uint32_t*, std::uint32_t answer) {
    if (g_keep_facing) Sprite_Current[8] &= 3;
    return answer;
}
// Battle_ActorIsOut: the recorder's flag; for Foretell_Read one enemy kept in,
// and an enemy that is in has its divisor word +0xB0 not 0 (read right after
// this answer, before the next disturbance).
std::uint32_t IsOutEffect(const std::uint32_t* a, std::uint32_t answer) {
    if (!g_keep_enemy) return answer;
    const unsigned actor = a[0] & 0xFF;
    if (actor == g_kept) answer &= 0xFFFFFF00u;
    if ((answer & 0xFF) == 0 && actor >= 3 && actor <= 10) {
        unsigned char* const e = mh::EnemyOf(static_cast<unsigned char>(actor));
        if (Word(e + 0xB0) == 0) SetWord(e + 0xB0, 1);
    }
    return answer;
}
// The GTE stand-ins of the matrix push: a result from the inputs, written
// where the real callee writes (group S22's).
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* t = reinterpret_cast<long*>(static_cast<std::uintptr_t>(a[1]));
    t[0] = v[0] * 3 + 1;
    t[1] = v[1] * 5 - 2;
    t[2] = v[2] * 7 + 3;
    return answer;
}
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* r = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* m = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(r[i % 3] * static_cast<int>(i + 1) + static_cast<int>(i));
    return answer;
}
std::uint32_t MulMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* b = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[1]));
    auto* out = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[2]));
    mh::Note(a[1] == a[2]);
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S15_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S15_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // the battle and sprite calls
    {S15_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &IsOutEffect},
    {S15_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S15_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S15_OURS(Tint_Release), 1, {kU8}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, field_misc, battle_items:
    // all ours)
    {S15_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S15_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S15_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S15_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetTile), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {S15_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0, {}, &KeepFacing},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S15_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S15_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S15_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S15_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S15_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S15_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S15_OURS(Gte_PrimDepths4_14), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S15_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly
    {S15_RAW(0x4B6BC0), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B6CA0), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B7240), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B7900), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B7BD0), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B8530), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B8690), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B8910), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B8A90), 0, {}, kG, 0, 0},
    {S15_RAW(0x4B8BD0), 0, {}, kG, 0, 0},
};
#undef S15_OURS
#undef S15_RAW

// The ten .data handler tables the dispatchers read in place
// (ChillChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65AC78, 4}, {0x65AC88, 4}, {0x65AC98, 4}, {0x65ACA8, 2}, {0x65ACE0, 1},
    {0x65ACE4, 4}, {0x65ACF4, 3}, {0x65AD00, 5}, {0x65AD14, 2}, {0x65AD1C, 5},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850.., Scratch_Swap at +0xC
    {kKind2, 8},                          // Field_Kind2Z, Field_Kind2X
    {kTints, 0xC00},                      // MoveScript_TintRecords
    {kInput, 2},                          // Input_Pressed
    {0x80B980, 0x200},                    // Gfx_ClutStripSource row 2
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x80F980, 0x200},                    // Gfx_ClutStrip row 2
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
    {0x65ACB0, 0x30},                     // Foretell_Bias, Foretell_TileColours
    {0x65AD30, 4},                        // Influence_TrianglePoints
    {kEnemyLift, 8 * 0x8C},               // the lift bytes of enemy types 0..7 (zero at start-up)
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// The group's cells a recorder may move (the harness's case 14). The loop
// bound the ray reads back (word 0x903850) stays small, or a loop runs past
// the log.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: {
        const unsigned k = v % 16;
        if (k < 2) SetWord(mh::Mem(kScratch), (h >> 16) % 0x30);
        else mh::Mem(kScratch + k)[0] = Byte(h >> 24);
        break;
    }
    case 3: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: {
        static const unsigned kFields[] = {0xC, 0xD, 0x2E, 0x2F, 0x30, 0x31, 0x34, 0x38};
        Sc()[kFields[v % 8]] = Byte(h >> 24);
        break;
    }
    case 5: mh::Mem(kTints + v % 0xC00)[0] = Byte(h >> 24); break;
    default: break;
    }
}

// After every disturbance: InfluenceMark_Start reads its actor index +0xB
// again after two calls and indexes the records by it; the game only writes
// 0..10 there (Influence_SpawnMarks), and an index past the enemies reads past
// the image on both sides.
void Settle() {
    if (g_keep_index && Sc()[0xB] > 10) Sc()[0xB] = Byte(Sc()[0xB] % 11);
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void NearLong(unsigned char* at, std::uint32_t before) {
    if (mh::Half()) SetLong(at, static_cast<std::int32_t>(before + (mh::Half() ? 1u : 0u)));
}
unsigned char* Enemy(unsigned i) { return mh::EnemyOf(static_cast<unsigned char>(i + 3)); }
unsigned char* Member(unsigned i) { return mh::PartyOf(static_cast<unsigned char>(i)); }

// Foretell_Read's records: every divisor word not 0, the ratio's word most
// often at most the divisor (the bands 0..70 %), the means close (the level
// bands), the six flag bytes around their bounds; at least one member counted.
void SeedForetell() {
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const r = Member(i);
        const unsigned den = 1 + mh::Next() % 0x3FF;
        SetWord(r + 0xA0, den);
        if (mh::Often()) SetWord(r + 0x98, mh::Next() % (den + 1));
        if (mh::Often()) r[0x8A] = Byte(mh::Next() % 100);
        if (mh::Half()) r[0] = Byte(mh::Half() ? 0 : 1 + mh::Next() % 0xFF);
        if (mh::Half()) r[0x91] = Byte(r[0x91] & ~0x40u);
    }
    {
        unsigned char* const r = Member(mh::Next() % 3);
        r[0] = Byte(1 + mh::Next() % 0xFF);
        r[0x91] = Byte(r[0x91] & ~0x40u);
    }
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const e = Enemy(i);
        const unsigned den = 1 + mh::Next() % 0x3FF;
        SetWord(e + 0xB0, den);
        if (mh::Often()) SetWord(e + 0xA4, mh::Next() % (den + 1));
        if (mh::Often()) SetWord(e + 0x98, mh::Next() % 100);
        for (unsigned k = 0xBF; k <= 0xC4; ++k)
            if (mh::Half()) e[k] = Byte(mh::Next() % 5);
    }
    g_kept = 3 + mh::Next() % 8;
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    g_keep_facing = k == kChillRay_PushMatrix;
    g_keep_enemy = k == kForetell_Read;
    g_keep_index = k == kInfluenceMark_Start;
    if (mh::Often()) sc[8] = Byte(mh::Next() % 5);
    if (mh::Half()) sc[0] = 0;
    // the enemy types InfluenceMark_Start indexes the lift bytes by
    for (unsigned i = 0; i < 8; ++i) Enemy(i)[0xF0] = Byte(mh::Next() % 8);
    // the target's side bit (Chill_SpawnMarks)
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Mem(mh::at::kTarget)[0] | 0x40);
    switch (k) {
    // the dispatchers: inside their tables
    case kChill_Task: case kForetell_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kChillChild_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kForetellChild_Task: sc[1] = 0; break;
    case kInfluence_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kInfluenceChild_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kChillRay_Run: case kChillFlash_Run: case kForetellOrb_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kChillMark_Run: case kInfluenceLeft_Run: sc[2] = Byte(mh::Next() % 2); break;
    case kChillEnemy_Task: sc[2] = Byte(mh::Next() % 3); break;
    case kInfluenceRight_Run: case kInfluenceMark_Run: sc[2] = Byte(mh::Next() % 5); break;
    // Chill's third child: an event battle, the acting enemy's +0x100 0x29
    case kChill_Start:
        if (mh::Often()) mh::Mem(kEventBattle)[0] = Byte(1 + mh::Next() % 0xFF);
        if (mh::Often()) {
            const unsigned a = mh::Mem(mh::at::kActor)[0];
            mh::Mem(mh::at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(a) - 3) * 0x128) + 0x100)[0] = 0x29;
        }
        break;
    // the counters: at their thresholds
    case kChill_WaitChildren: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0 : 1); break;
    case kChill_SpawnMarks: case kForetellOrb_Spin: case kForetellOrb_End: case kInfluenceRight_Launch:
    case kInfluenceLeft_Launch:
        Near(sc[9], 1);
        break;
    case kChillRay_Grow: Near(sc[9], 0xBE); break;
    case kChillRay_Shrink: Near(sc[9], 0x62); break;
    case kChillRay_PushMatrix: sc[8] = Byte(mh::Next() % 4); break;
    case kChillRay_Draw:
        if (mh::Half()) sc[2] = 3;
        sc[9] = Byte(mh::Often() ? mh::Next() % 0x30 : mh::Next());
        break;
    case kForetell_Read:
        if (mh::Often()) sc[0xB] = 0;
        SeedForetell();
        break;
    case kForetell_Pause: Near(sc[0xA], 1); break;
    case kForetell_Show: if (mh::Half()) SetWord(mh::Mem(kInput), 0); break;
    case kForetell_End:
        if (mh::Half()) mh::Mem(mh::at::kMessageUp)[0] = 0;
        if (mh::Half()) sc[9] = Byte(mh::Next() % 3);
        break;
    case kForetell_DrawTiles:
        if (mh::Often()) sc[0xB] = Byte(mh::Half() ? 0 : mh::Next() & 0x3F);
        break;
    case kForetellOrb_Count:
        Near(sc[9], 1);
        Near(sc[0xA], 1);
        break;
    case kInfluenceRight_Slide: case kInfluenceLeft_Slide:
        NearLong(sc + 0xC, 1);
        if (mh::Half()) sc[0xB] = 0;
        break;
    case kInfluenceRight_Pick: Near(sc[9], 0xF); break;
    case kInfluenceRight_Blink:
        if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0 : 1);
        if (mh::Half()) sc[9] = Byte(sc[0xB] != 0 ? 0xC : 2);
        if (mh::Half()) mh::Pointer(mh::at::kOwner)[0xB] = 1;
        break;
    case kInfluenceRight_Shrink: Near(sc[0xA], 9); break;
    case kInfluenceMark_Start:
        sc[0xB] = Byte(mh::Next() % 11);
        for (unsigned i = 0; i < 3; ++i) Member(i)[8] = Byte(mh::Next() % 3);
        break;
    case kInfluenceMark_Brighten: case kInfluenceMark_Fade: {
        unsigned char* const t = mh::Mem(kTints + sc[0xA] * 12u + 2);
        if (mh::Half()) t[0] = Byte(k == kInfluenceMark_Brighten ? 0xF : 1);
        if (mh::Half()) sc[4] = 0;
        break;
    }
    case kInfluenceMark_Show:
        if (mh::Half()) sc[0xA] = Byte(mh::Half() ? 0x18 : 0x14);
        Near(sc[9], 0x13);
        break;
    case kInfluence_DrawTriangles: case kInfluence_DrawRing:
        if (mh::Half()) sc[2] = Byte(mh::Next() % 4);
        break;
    case kInfluence_SpawnMarks:
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const e = Enemy(i);
            if (mh::Half()) SetWord(e + 0xBA, mh::Next() % 3);
            if (mh::Half()) e[0x92] = Byte(e[0x92] & ~0x20u);
            if (mh::Half()) SetLong(e + 0x114, Long(e + 0x114) & ~0x4000);
        }
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const r = Member(i);
            if (mh::Half()) SetWord(r + 0xAA, mh::Next() % 3);
            if (mh::Half()) r[0x90] = Byte(r[0x90] & ~0x20u);
            if (mh::Half()) SetLong(r + 0x134, Long(r + 0x134) & ~0x4001);
        }
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s15", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s15
