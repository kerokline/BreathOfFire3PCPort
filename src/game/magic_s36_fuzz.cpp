// BOF3X_SHADOW=magic_s36: group S36's three overlays (MAGIC172, MAGIC173,
// MAGIC218) through the spell round's shared harness (magic_harness.h), once
// at start-up. docs/magic_s36.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC172 / MAGIC173 / MAGIC218
// --clones (2026-09-27; capstone, every jump internal, no jump table, no
// REFUSED line), names given. Beyond the standard set this group lists the
// draw callees (the libgpu and GTE entry points, Math_Sin / Math_Cos /
// Math_Ratan2, Gfx_CommitPrim, MapView_LinkPrimAt), the sprite calls,
// Battle_ActorIsOut, the engine's 0x446770 and the functions of its own that
// its functions call directly. Everything the harness lacks is built here, not
// in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - Math_Ratan2 answers, while a flight step is fuzzed, a heading that turns
//     by one step either side of 0x600 or 0xA00 from the old one half the
//     time, so the "turned past the target" test meets both of its bounds;
//   - Math_Sin answers 0 or +-0x400 in half of a spark's rounds, so the
//     spark's radius walk creeps across its floor and runs past its 32-step
//     wave (a garbage sine ends it in a step or two);
//   - Battle_SetTargetFlags, while the dome grows, moves the struck byte of
//     the enemy it flags (the dome stores it before the call);
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair and
//     writes a new pair; AuraBreath_InReach (called by the dome) answers a
//     whole eax of 0 or 1 and logs the reach word and the point it reads;
//   - the sprite calls log which sprite, and Sprite_UpdateScreen the
//     frame-offset table 0x9039D8 the burst swaps round its steps;
//   - a settle keeps the first four task slots' +4 below 48: MagicBallOrb_Fly
//     reads the task slot +4 names (a byte past 47 reads past the image, a
//     fault on both sides), and the trails index IntimidateTrail_Points by it.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s36.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s36 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC172 / MAGIC173 / MAGIC218 --clones,
// 2026-09-27, names given.
// 0x4F1E40: 0x26 bytes  MagicBall_Task
constexpr mh::Imm kImms4F1E40[] = {{0xF, 0x4F1E70}, {0x17, 0x4F7350}};
// 0x4F1E70: 0xF3 bytes  MagicBall_Start
constexpr mh::CallSite kCalls4F1E70[] = {{0x5D, 0x435180}, {0x90, 0x435180}, {0xE7, 0x587900}};
// 0x4F1F70: 0x12 bytes; +0xB note: jmp through .data 0x65c180, 2 code entries (a data_tables entry)  MagicBallChild_Task
// 0x4F1F90: 0x28E bytes; +0xB note: call through .data 0x65c188, 5 code entries (a data_tables entry)  MagicBallCore_Run
constexpr mh::CallSite kCalls4F1F90[] = {{0x3E, 0x4FBD10}, {0x45, 0x5B93D2}, {0x50, 0x4F2F20}, {0x5A, 0x5B93D2}, {0x6A, 0x4F3140}, {0x74, 0x5B93D2}, {0x84, 0x4F2F20}, {0x8E, 0x5B93D2}, {0x9E, 0x4F3140}, {0xA8, 0x5B93D2}, {0xB8, 0x4F2F20}, {0xC0, 0x4F2680}, {0xC5, 0x4F2860}, {0xCA, 0x4FBD10}, {0xD1, 0x5B93D2}, {0xDC, 0x4F2F20}, {0xE6, 0x5B93D2}, {0xF6, 0x4F3140}, {0x100, 0x5B93D2}, {0x110, 0x4F2F20}, {0x11A, 0x5B93D2}, {0x12A, 0x4F3140}, {0x134, 0x5B93D2}, {0x144, 0x4F2F20}, {0x14E, 0x5B93D2}, {0x15E, 0x4F2F20}, {0x168, 0x5B93D2}, {0x178, 0x4F2F20}, {0x182, 0x5B93D2}, {0x192, 0x4F2F20}, {0x19B, 0x4FBD10}, {0x1A0, 0x4F2AA0}, {0x1A5, 0x4F2CE0}, {0x1AA, 0x4FBD10}, {0x1B1, 0x5B93D2}, {0x1BC, 0x4F2F20}, {0x1C6, 0x5B93D2}, {0x1D6, 0x4F3140}, {0x1E0, 0x5B93D2}, {0x1F0, 0x4F2F20}, {0x1FA, 0x5B93D2}, {0x20A, 0x4F3140}, {0x214, 0x5B93D2}, {0x224, 0x4F2F20}, {0x22E, 0x5B93D2}, {0x23E, 0x4F2F20}, {0x248, 0x5B93D2}, {0x258, 0x4F2F20}, {0x262, 0x5B93D2}, {0x272, 0x4F2F20}, {0x27A, 0x4F2680}, {0x27F, 0x4F2860}, {0x284, 0x4F2AA0}, {0x289, 0x4F2CE0}};
// 0x4F2220: 0xA7 bytes  MagicBallCore_Start
constexpr mh::CallSite kCalls4F2220[] = {{0x6E, 0x5A7A70}};
// 0x4F22D0: 0x161 bytes  MagicBallCore_Fly
constexpr mh::CallSite kCalls4F22D0[] = {{0x56, 0x4FBA90}, {0x95, 0x5A7A70}, {0xA9, 0x4FBC30}, {0xBD, 0x452F70}, {0xC7, 0x587900}, {0x138, 0x452F70}, {0x142, 0x587900}};
// 0x4F2440: 0x3E bytes  MagicBallCore_Swell
// 0x4F2480: 0x48 bytes  MagicBallCore_Shrink
constexpr mh::CallSite kCalls4F2480[] = {{0x2D, 0x587900}};
// 0x4F24D0: 0x2B bytes; +0xB note: call through .data 0x65c19c, 3 code entries (a data_tables entry)  MagicBallOrb_Run
constexpr mh::CallSite kCalls4F24D0[] = {{0x20, 0x4FBD10}, {0x25, 0x4F3360}};
// 0x4F2500: 0xAC bytes  MagicBallOrb_Start
constexpr mh::CallSite kCalls4F2500[] = {{0x86, 0x5A7A70}};
// 0x4F25B0: 0xCB bytes  MagicBallOrb_Fly
constexpr mh::CallSite kCalls4F25B0[] = {{0x56, 0x4FBA90}, {0x95, 0x5A7A70}};
// 0x4F2680: 0x1D8 bytes  MagicBall_DrawDisc
constexpr mh::CallSite kCalls4F2680[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0xAD, 0x5A75F0}, {0xB5, 0x5A7780}, {0xD0, 0x5A7A00}, {0xF7, 0x5A7A50}, {0x126, 0x5A7A00}, {0x14D, 0x5A7A50}, {0x1BE, 0x461E50}};
// 0x4F2860: 0x23B bytes  MagicBall_DrawRing
constexpr mh::CallSite kCalls4F2860[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x86, 0x5A7610}, {0x8E, 0x5A7780}, {0x9B, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xE9, 0x5A7A00}, {0x110, 0x5A7A50}, {0x13F, 0x5A7A00}, {0x166, 0x5A7A50}, {0x18D, 0x5A7A00}, {0x1B4, 0x5A7A50}, {0x223, 0x461E50}};
// 0x4F2AA0: 0x23D bytes  MagicBall_DrawRingOut
constexpr mh::CallSite kCalls4F2AA0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x88, 0x5A7610}, {0x90, 0x5A7780}, {0x9D, 0x5A7A00}, {0xC4, 0x5A7A50}, {0xEB, 0x5A7A00}, {0x112, 0x5A7A50}, {0x141, 0x5A7A00}, {0x168, 0x5A7A50}, {0x18F, 0x5A7A00}, {0x1B6, 0x5A7A50}, {0x225, 0x461E50}};
// 0x4F2CE0: 0x23E bytes  MagicBall_DrawRingIn
constexpr mh::CallSite kCalls4F2CE0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x89, 0x5A7610}, {0x91, 0x5A7780}, {0x9E, 0x5A7A00}, {0xC5, 0x5A7A50}, {0xEC, 0x5A7A00}, {0x113, 0x5A7A50}, {0x142, 0x5A7A00}, {0x169, 0x5A7A50}, {0x190, 0x5A7A00}, {0x1B7, 0x5A7A50}, {0x226, 0x461E50}};
// 0x4F2F20: 0x21D bytes  MagicBall_DrawSpark
constexpr mh::CallSite kCalls4F2F20[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x6B, 0x5A76B0}, {0x73, 0x5A7780}, {0x80, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xDA, 0x5A7A00}, {0xE1, 0x5B93D2}, {0xFC, 0x5B93D2}, {0x11B, 0x5A7A00}, {0x142, 0x5A7A50}, {0x1F8, 0x461E50}};
// 0x4F3140: 0x21D bytes  MagicBall_DrawSparkShort
constexpr mh::CallSite kCalls4F3140[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x6B, 0x5A76B0}, {0x73, 0x5A7780}, {0x80, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xDA, 0x5A7A00}, {0xE1, 0x5B93D2}, {0xFC, 0x5B93D2}, {0x11B, 0x5A7A00}, {0x142, 0x5A7A50}, {0x1F8, 0x461E50}};
// 0x4F3360: 0x1D6 bytes  MagicBallOrb_DrawDisc
constexpr mh::CallSite kCalls4F3360[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0xC6, 0x5A75F0}, {0xCD, 0x5A7780}, {0xE8, 0x5A7A00}, {0x10F, 0x5A7A50}, {0x142, 0x5A7A00}, {0x169, 0x5A7A50}, {0x1BC, 0x461E50}};
// 0x4F3540: 0x26 bytes  Intimidate_Task
constexpr mh::Imm kImms4F3540[] = {{0xF, 0x4F3570}, {0x17, 0x4F7350}};
// 0x4F3570: 0xCC bytes  Intimidate_Start
constexpr mh::CallSite kCalls4F3570[] = {{0x61, 0x435180}, {0xC5, 0x587900}};
// 0x4F3640: 0x12 bytes; +0xB note: jmp through .data 0x65c1bc, 2 code entries (a data_tables entry)  IntimidateChild_Task
// 0x4F3660: 0x41 bytes; +0xB note: call through .data 0x65c1c4, 4 code entries (a data_tables entry)  IntimidateTrail_Run
constexpr mh::CallSite kCalls4F3660[] = {{0x2B, 0x4F3AF0}, {0x3B, 0x4F3F40}};
// 0x4F36B0: 0x1FC bytes  IntimidateTrail_Start
constexpr mh::CallSite kCalls4F36B0[] = {{0xFC, 0x446770}, {0x16B, 0x5A7A70}, {0x184, 0x4FBD10}};
// 0x4F38B0: 0x1CE bytes  IntimidateTrail_Fly
constexpr mh::CallSite kCalls4F38B0[] = {{0x59, 0x4FBA90}, {0x98, 0x5A7A70}, {0xAC, 0x4FBC30}, {0x131, 0x4FBD10}, {0x136, 0x4F4640}, {0x155, 0x452F70}, {0x16C, 0x435180}};
// 0x4F3A80: 0x3D bytes  IntimidateTrail_Grow
constexpr mh::CallSite kCalls4F3A80[] = {{0x1B, 0x4FBD10}, {0x20, 0x4F4640}};
// 0x4F3AC0: 0x2C bytes  IntimidateTrail_Fade
constexpr mh::CallSite kCalls4F3AC0[] = {{0x0, 0x4F46D0}, {0x26, 0x4351F0}};
// 0x4F3AF0: 0x445 bytes  IntimidateTrail_DrawThin
constexpr mh::CallSite kCalls4F3AF0[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0xEE, 0x5A7A70}, {0x197, 0x5A7610}, {0x19E, 0x5A7780}, {0x1BC, 0x5A7A00}, {0x1EA, 0x5A7A50}, {0x229, 0x5A7A00}, {0x257, 0x5A7A50}, {0x2B7, 0x5A7A70}, {0x2D5, 0x5A7A00}, {0x303, 0x5A7A50}, {0x342, 0x5A7A00}, {0x370, 0x5A7A50}, {0x406, 0x461E50}, {0x42F, 0x5A77C0}, {0x438, 0x461E50}};
// 0x4F3F40: 0x6F2 bytes  IntimidateTrail_DrawWide
constexpr mh::CallSite kCalls4F3F40[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0xE7, 0x5A7A70}, {0x18F, 0x5A7610}, {0x196, 0x5A7780}, {0x1B4, 0x5A7A00}, {0x1E2, 0x5A7A50}, {0x266, 0x5A7A70}, {0x284, 0x5A7A00}, {0x2B2, 0x5A7A50}, {0x368, 0x461E50}, {0x434, 0x5A7A70}, {0x4DD, 0x5A7610}, {0x4E4, 0x5A7780}, {0x502, 0x5A7A00}, {0x530, 0x5A7A50}, {0x5B4, 0x5A7A70}, {0x5D2, 0x5A7A00}, {0x600, 0x5A7A50}, {0x6B4, 0x461E50}, {0x6DC, 0x5A77C0}, {0x6E5, 0x461E50}};
// 0x4F4640: 0x8E bytes  IntimidateTrail_Push
// 0x4F46D0: 0x8B bytes  IntimidateTrail_PushGap
// 0x4F4760: 0x4C bytes; +0x15 note: call through .data 0x65c1d4, 4 code entries (a data_tables entry)  IntimidateBurst_Run
constexpr mh::CallSite kCalls4F4760[] = {{0x27, 0x4FBD10}, {0x2C, 0x4F48D0}, {0x3C, 0x588F20}};
// 0x4F47B0: 0x95 bytes  IntimidateBurst_Start
constexpr mh::CallSite kCalls4F47B0[] = {{0x7A, 0x5891F0}};
// 0x4F4850: 0x22 bytes  IntimidateBurst_Rise
constexpr mh::CallSite kCalls4F4850[] = {{0x0, 0x589410}};
// 0x4F4880: 0x17 bytes  IntimidateBurst_Play
constexpr mh::CallSite kCalls4F4880[] = {{0x0, 0x589410}, {0x5, 0x589410}};
// 0x4F48A0: 0x28 bytes  IntimidateBurst_Fade
constexpr mh::CallSite kCalls4F48A0[] = {{0x22, 0x4351F0}};
// 0x4F48D0: 0x184 bytes  IntimidateBurst_DrawDisc
constexpr mh::CallSite kCalls4F48D0[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0x27, 0x5B93D2}, {0x58, 0x5A75F0}, {0x5F, 0x5A7780}, {0x8E, 0x5A7A00}, {0xB8, 0x5A7A50}, {0xE8, 0x5A7A00}, {0x112, 0x5A7A50}, {0x16B, 0x461E50}};
// 0x4F52F0: 0x26 bytes  AuraBreath_Task
constexpr mh::Imm kImms4F52F0[] = {{0xF, 0x4F5320}, {0x17, 0x4F7350}};
// 0x4F5320: 0xD4 bytes  AuraBreath_Start
constexpr mh::CallSite kCalls4F5320[] = {{0x66, 0x435180}, {0xCD, 0x587900}};
// 0x4F5400: 0x12 bytes; +0xB note: jmp through .data 0x65c20c, 1 code entries (a data_tables entry)  AuraBreathDome_Task
// 0x4F5420: 0x33 bytes; +0xB note: call through .data 0x65c210, 3 code entries (a data_tables entry)  AuraBreathDome_Run
constexpr mh::CallSite kCalls4F5420[] = {{0x23, 0x4B7D40}, {0x28, 0x4F55E0}, {0x2D, 0x5A7BC0}};
// 0x4F5460: 0x6D bytes  AuraBreathDome_Wait
// 0x4F54D0: 0xB8 bytes  AuraBreathDome_Grow
constexpr mh::CallSite kCalls4F54D0[] = {{0x3F, 0x4456C0}, {0x4C, 0x4F5970}, {0x6F, 0x452F70}};
// 0x4F5590: 0x50 bytes  AuraBreathDome_Fade
constexpr mh::CallSite kCalls4F5590[] = {{0x4A, 0x4351F0}};
// 0x4F55E0: 0x390 bytes  AuraBreathDome_Draw
constexpr mh::CallSite kCalls4F55E0[] = {{0x58, 0x5A7A00}, {0x9F, 0x5A7A00}, {0xB9, 0x5A7A50}, {0xE5, 0x5A7A00}, {0xFF, 0x5A7A50}, {0x11C, 0x5A7A00}, {0x18A, 0x5A7A00}, {0x1AA, 0x5A7A50}, {0x1E4, 0x5A7A00}, {0x204, 0x5A7A50}, {0x24E, 0x5A77C0}, {0x259, 0x572FA0}, {0x265, 0x5A75D0}, {0x26D, 0x5A7780}, {0x283, 0x5A79A0}, {0x293, 0x5A79E0}, {0x322, 0x5A85F0}, {0x32B, 0x5A9290}, {0x350, 0x572FA0}};
// 0x4F5970: 0x60 bytes  AuraBreath_InReach
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"MagicBall_Task", 0x4F1E40, 0x26, nullptr, 0, kImms4F1E40, MH_N(kImms4F1E40), nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_Task)},
    {"MagicBall_Start", 0x4F1E70, 0xF3, kCalls4F1E70, MH_N(kCalls4F1E70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_Start)},
    {"MagicBallChild_Task", 0x4F1F70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallChild_Task)},
    {"MagicBallCore_Run", 0x4F1F90, 0x28E, kCalls4F1F90, MH_N(kCalls4F1F90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallCore_Run)},
    {"MagicBallCore_Start", 0x4F2220, 0xA7, kCalls4F2220, MH_N(kCalls4F2220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallCore_Start)},
    {"MagicBallCore_Fly", 0x4F22D0, 0x161, kCalls4F22D0, MH_N(kCalls4F22D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallCore_Fly)},
    {"MagicBallCore_Swell", 0x4F2440, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallCore_Swell)},
    {"MagicBallCore_Shrink", 0x4F2480, 0x48, kCalls4F2480, MH_N(kCalls4F2480), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallCore_Shrink)},
    {"MagicBallOrb_Run", 0x4F24D0, 0x2B, kCalls4F24D0, MH_N(kCalls4F24D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallOrb_Run)},
    {"MagicBallOrb_Start", 0x4F2500, 0xAC, kCalls4F2500, MH_N(kCalls4F2500), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallOrb_Start)},
    {"MagicBallOrb_Fly", 0x4F25B0, 0xCB, kCalls4F25B0, MH_N(kCalls4F25B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallOrb_Fly)},
    {"MagicBall_DrawDisc", 0x4F2680, 0x1D8, kCalls4F2680, MH_N(kCalls4F2680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawDisc)},
    {"MagicBall_DrawRing", 0x4F2860, 0x23B, kCalls4F2860, MH_N(kCalls4F2860), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawRing)},
    {"MagicBall_DrawRingOut", 0x4F2AA0, 0x23D, kCalls4F2AA0, MH_N(kCalls4F2AA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawRingOut)},
    {"MagicBall_DrawRingIn", 0x4F2CE0, 0x23E, kCalls4F2CE0, MH_N(kCalls4F2CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawRingIn)},
    {"MagicBall_DrawSpark", 0x4F2F20, 0x21D, kCalls4F2F20, MH_N(kCalls4F2F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawSpark)},
    {"MagicBall_DrawSparkShort", 0x4F3140, 0x21D, kCalls4F3140, MH_N(kCalls4F3140), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBall_DrawSparkShort)},
    {"MagicBallOrb_DrawDisc", 0x4F3360, 0x1D6, kCalls4F3360, MH_N(kCalls4F3360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicBallOrb_DrawDisc)},
    {"Intimidate_Task", 0x4F3540, 0x26, nullptr, 0, kImms4F3540, MH_N(kImms4F3540), nullptr, 0, reinterpret_cast<const void*>(&::Intimidate_Task)},
    {"Intimidate_Start", 0x4F3570, 0xCC, kCalls4F3570, MH_N(kCalls4F3570), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Intimidate_Start)},
    {"IntimidateChild_Task", 0x4F3640, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateChild_Task)},
    {"IntimidateTrail_Run", 0x4F3660, 0x41, kCalls4F3660, MH_N(kCalls4F3660), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Run)},
    {"IntimidateTrail_Start", 0x4F36B0, 0x1FC, kCalls4F36B0, MH_N(kCalls4F36B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Start)},
    {"IntimidateTrail_Fly", 0x4F38B0, 0x1CE, kCalls4F38B0, MH_N(kCalls4F38B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Fly)},
    {"IntimidateTrail_Grow", 0x4F3A80, 0x3D, kCalls4F3A80, MH_N(kCalls4F3A80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Grow)},
    {"IntimidateTrail_Fade", 0x4F3AC0, 0x2C, kCalls4F3AC0, MH_N(kCalls4F3AC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Fade)},
    {"IntimidateTrail_DrawThin", 0x4F3AF0, 0x445, kCalls4F3AF0, MH_N(kCalls4F3AF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_DrawThin)},
    {"IntimidateTrail_DrawWide", 0x4F3F40, 0x6F2, kCalls4F3F40, MH_N(kCalls4F3F40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_DrawWide)},
    {"IntimidateTrail_Push", 0x4F4640, 0x8E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_Push)},
    {"IntimidateTrail_PushGap", 0x4F46D0, 0x8B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateTrail_PushGap)},
    {"IntimidateBurst_Run", 0x4F4760, 0x4C, kCalls4F4760, MH_N(kCalls4F4760), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_Run)},
    {"IntimidateBurst_Start", 0x4F47B0, 0x95, kCalls4F47B0, MH_N(kCalls4F47B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_Start)},
    {"IntimidateBurst_Rise", 0x4F4850, 0x22, kCalls4F4850, MH_N(kCalls4F4850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_Rise)},
    {"IntimidateBurst_Play", 0x4F4880, 0x17, kCalls4F4880, MH_N(kCalls4F4880), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_Play)},
    {"IntimidateBurst_Fade", 0x4F48A0, 0x28, kCalls4F48A0, MH_N(kCalls4F48A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_Fade)},
    {"IntimidateBurst_DrawDisc", 0x4F48D0, 0x184, kCalls4F48D0, MH_N(kCalls4F48D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IntimidateBurst_DrawDisc)},
    {"AuraBreath_Task", 0x4F52F0, 0x26, nullptr, 0, kImms4F52F0, MH_N(kImms4F52F0), nullptr, 0, reinterpret_cast<const void*>(&::AuraBreath_Task)},
    {"AuraBreath_Start", 0x4F5320, 0xD4, kCalls4F5320, MH_N(kCalls4F5320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreath_Start)},
    {"AuraBreathDome_Task", 0x4F5400, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Task)},
    {"AuraBreathDome_Run", 0x4F5420, 0x33, kCalls4F5420, MH_N(kCalls4F5420), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Run)},
    {"AuraBreathDome_Wait", 0x4F5460, 0x6D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Wait)},
    {"AuraBreathDome_Grow", 0x4F54D0, 0xB8, kCalls4F54D0, MH_N(kCalls4F54D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Grow)},
    {"AuraBreathDome_Fade", 0x4F5590, 0x50, kCalls4F5590, MH_N(kCalls4F5590), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Fade)},
    {"AuraBreathDome_Draw", 0x4F55E0, 0x390, kCalls4F55E0, MH_N(kCalls4F55E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreathDome_Draw)},
    {"AuraBreath_InReach", 0x4F5970, 0x60, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AuraBreath_InReach), 0xFFFFFFFFu},
};
#undef MH_N

enum : unsigned {
    kMagicBall_Task, kMagicBall_Start, kMagicBallChild_Task, kMagicBallCore_Run, kMagicBallCore_Start,
    kMagicBallCore_Fly, kMagicBallCore_Swell, kMagicBallCore_Shrink, kMagicBallOrb_Run, kMagicBallOrb_Start,
    kMagicBallOrb_Fly, kMagicBall_DrawDisc, kMagicBall_DrawRing, kMagicBall_DrawRingOut, kMagicBall_DrawRingIn,
    kMagicBall_DrawSpark, kMagicBall_DrawSparkShort, kMagicBallOrb_DrawDisc,
    kIntimidate_Task, kIntimidate_Start, kIntimidateChild_Task, kIntimidateTrail_Run, kIntimidateTrail_Start,
    kIntimidateTrail_Fly, kIntimidateTrail_Grow, kIntimidateTrail_Fade, kIntimidateTrail_DrawThin,
    kIntimidateTrail_DrawWide, kIntimidateTrail_Push, kIntimidateTrail_PushGap, kIntimidateBurst_Run,
    kIntimidateBurst_Start, kIntimidateBurst_Rise, kIntimidateBurst_Play, kIntimidateBurst_Fade,
    kIntimidateBurst_DrawDisc,
    kAuraBreath_Task, kAuraBreath_Start, kAuraBreathDome_Task, kAuraBreathDome_Run, kAuraBreathDome_Wait,
    kAuraBreathDome_Grow, kAuraBreathDome_Fade, kAuraBreathDome_Draw, kAuraBreath_InReach, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kFrameSet = 0x9039D8, kTrail = 0x6B2858,
                        kStruck = 0x6B4A58;
// IntimidateTrail_Points as far as a trail +4 below 48 and a point index up
// to 256 reach: 4 x ((47 << 5) + 256) + 4 bytes.
constexpr std::uint32_t kTrailBytes = 0x1B84;

// Set by the seed for the functions each matters to.
bool g_turn = false;    // Math_Ratan2 answers near a turn bound (the flight steps)
bool g_calm_sin = false;   // Math_Sin answers 0 or +-0x400 (half of a spark's rounds)
bool g_grow = false;       // Battle_SetTargetFlags moves the struck byte (AuraBreathDome_Grow)
unsigned g_reach_enemy = 3;   // AuraBreath_InReach's record, chosen by the seed

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x48 past it).
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
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
// The sprite calls act on Sprite_Current: which sprite; the screen update the
// frame-offset table too.
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
// AuraBreath_InReach: what it reads besides its argument - the reach word
// 0x903850 and Sprite_Current's point.
std::uint32_t ReachEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Word(mh::Mem(kScratch)), static_cast<std::uint32_t>(Long(Sprite_Current + 0x34)),
             static_cast<std::uint32_t>(Long(Sprite_Current + 0x38)));
    return answer;
}
// Math_Ratan2 for the flight steps: half the time a heading that makes
// (+0x10 & 0xFFF) - (answer & 0xFFF) one of the turn test's bounds or a step
// past it, either sign (Sprite_Current read now, after the disturbance - the
// callers store into it).
std::uint32_t RatanEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!g_turn || (mh::Noise() & 1) == 0) return answer;
    static const std::int32_t kTurns[] = {0x5FF, 0x600, 0x601, 0x9FF, 0xA00, 0xA01};
    const std::uint32_t n = mh::Noise();
    std::int32_t d = kTurns[n % 6];
    if (n & 0x100) d = -d;
    const std::uint32_t old = static_cast<std::uint32_t>(Long(Sprite_Current + 0x10)) & 0xFFF;
    return (answer & 0xFFFFF000u) | ((old - static_cast<std::uint32_t>(d)) & 0xFFF);
}
// In a calm spark round: 0 seven times in eight, else +-0x400 (a radius step
// of a quarter of the Rand draw), so the radius creeps across the loop's floor.
std::uint32_t SinEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!g_calm_sin) return answer;
    const std::uint32_t n = mh::Noise();
    if ((n & 7) != 0) return 0;
    return n & 8 ? 0x400u : 0xFFFFFC00u;
}
// Battle_SetTargetFlags while the dome grows: a new byte in the struck entry
// of the enemy flagged, half the time (the dome's store comes before the call
// in the original, so what it holds after is the callee's).
std::uint32_t FlagsEffect(const std::uint32_t* a, std::uint32_t answer) {
    if (g_grow && (mh::Noise() & 1)) mh::Mem(0x6B4A58 + ((a[0] - 3) & 7))[0] = static_cast<unsigned char>(mh::Noise());
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S36_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S36_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S36_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S36_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    // the sprite and battle calls
    {S36_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S36_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items, move_cmds: all ours)
    {S36_OURS(Math_Sin), 1, {kAll}, kG, 0, 0, {}, &SinEffect},
    // listed over the standard one for its effect
    {S36_OURS(Battle_SetTargetFlags), 2, {kU8, 0xFFFFu}, kG, 0, 0, {}, &FlagsEffect},
    {S36_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S36_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0, {}, &RatanEffect},
    {S36_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S36_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S36_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S36_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S36_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S36_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S36_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S36_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S36_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S36_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S36_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S36_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S36_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly
    {S36_RAW(0x4F2680), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F2860), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F2AA0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F2CE0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F2F20), 2, {kAll, kAll}, kG, 0, 0},
    {S36_RAW(0x4F3140), 2, {kAll, kAll}, kG, 0, 0},
    {S36_RAW(0x4F3360), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F3AF0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F3F40), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F4640), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F46D0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F48D0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F55E0), 0, {}, kG, 0, 0},
    {S36_RAW(0x4F5970), 1, {kAll}, mh::Answer::kBool, 0, 0, {}, &ReachEffect},
};
#undef S36_OURS
#undef S36_RAW

// The eight .data handler tables the dispatchers read in place
// (MagicBallChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65C180, 2}, {0x65C188, 5}, {0x65C19C, 3}, {0x65C1BC, 2},
    {0x65C1C4, 4}, {0x65C1D4, 4}, {0x65C20C, 1}, {0x65C210, 3},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},              // Gfx_PacketNext
    {0, kPrimBytes},            // g_prims (filled in at start-up)
    {kVertex, 0x20},            // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},           // 0x903850.., Scratch_Swap at +0xC
    {kFrameSet, 4},             // the frame-offset table pointer the burst swaps
    {0x80E980, 0x200},          // Gfx_ClutStripSource row 26
    {0x812980, 0x200},          // Gfx_ClutStrip row 26
    {kTrail, kTrailBytes},      // IntimidateTrail_Points (and past it, as far as the indices reach)
    {kStruck, 8},               // AuraBreath_Struck
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Point(unsigned trail, unsigned k) { return mh::Mem(kTrail + ((trail << 5) + k) * 4); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4: SetWord(mh::Mem(kTrail + 2 * (v % (kTrailBytes / 2))), h >> 16); break;
    case 5: mh::Mem(kStruck + v % 8)[0] = Byte(h >> 24); break;
    case 6: {
        static const unsigned kFields[] = {0x2E, 0x30, 0x10, 0x14, 0x5D};
        const unsigned f = kFields[v % 5];
        if (f == 0x5D) Sc()[f] = Byte(h >> 24);
        else SetWord(Sc() + f, h >> 16);
        break;
    }
    default: break;
    }
}
// After every disturbance: the task slots Sprite_Current can be keep +4
// below 48 (see the head of this file).
void Settle() {
    for (unsigned k = 0; k < 4; ++k) mh::TaskAt(k)[4] = Byte(mh::TaskAt(k)[4] % 48);
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    Settle();
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    g_turn = k == kMagicBallCore_Fly || k == kMagicBallOrb_Fly || k == kIntimidateTrail_Fly;
    g_calm_sin = (k == kMagicBall_DrawSpark || k == kMagicBall_DrawSparkShort) && mh::Half();
    g_grow = k == kAuraBreathDome_Grow;
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kMagicBall_Task: case kMagicBallChild_Task: case kIntimidate_Task: case kIntimidateChild_Task:
    case kAuraBreath_Task:
        sc[1] = Byte(mh::Next() % 2);
        break;
    case kAuraBreathDome_Task: sc[1] = 0; break;
    case kMagicBallCore_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kMagicBallOrb_Run: case kAuraBreathDome_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kIntimidateTrail_Run: case kIntimidateBurst_Run:
        sc[2] = Byte(mh::Next() % 4);
        if (mh::Half()) sc[4] = 0;
        break;
    // the counters: at their thresholds
    case kMagicBallCore_Swell:
        Near(sc[9], 0);
        Near(sc[0xA], 0xF);
        break;
    case kMagicBallCore_Shrink: Near(sc[0xA], 1); break;
    case kMagicBallOrb_Start: case kIntimidateTrail_Fade: case kAuraBreathDome_Wait: Near(sc[9], 0); break;
    case kMagicBallCore_Fly: Near(sc[9], 0xF); break;
    case kMagicBallOrb_Fly:
        Near(sc[9], 0xF);
        if (mh::Often()) mh::Mem(mh::at::kTasks + sc[4] * mh::at::kTaskStride + 2)[0] = Byte(1 + mh::Next() % 2);
        break;
    case kIntimidateTrail_Start:
        if (mh::Often()) {
            const unsigned a = mh::Mem(mh::at::kActor)[0];
            if (a >= 3) mh::EnemyOf(Byte(a))[0x8C] = Byte(mh::Half() ? 0x61 : mh::Half() ? 0x6C : mh::Next());
        }
        break;
    case kIntimidateTrail_Fly:
        Near(sc[0xA], 7);
        if (mh::Often()) sc[2] = 1;
        if (mh::Half()) sc[4] = 0;
        break;
    case kIntimidateTrail_Grow: sc[0xA] = Byte(6 + mh::Next() % 4); break;
    case kIntimidateTrail_Push: case kIntimidateTrail_PushGap:
        if (mh::Often()) {
            sc[0xA] = Byte(mh::Next() % 34);
            sc[0xB] = Byte(mh::Next() % 34);
        }
        break;
    case kIntimidateTrail_DrawThin: case kIntimidateTrail_DrawWide:
        if (mh::Often()) {
            sc[0xB] = Byte(mh::Next() % 12);
            sc[0xA] = Byte(sc[0xB] + mh::Next() % 20);
        }
        // a gap or two at the head, the scan's case
        if (mh::Half()) {
            SetWord(Point(sc[4], sc[0xB]), 0xFFFF);
            if (mh::Half()) SetWord(Point(sc[4], sc[0xB] + 1u), 0xFFFF);
        }
        break;
    case kIntimidateBurst_Rise: Near(sc[9], 0xB); break;
    case kIntimidateBurst_Fade: Near(sc[9], 3); break;
    case kAuraBreathDome_Grow:
        Near(sc[9], 0x5D);
        Near(sc[0xA], 0x1F);
        for (unsigned i = 0; i < 8; ++i)
            if (mh::Often()) mh::Mem(kStruck + i)[0] = 0;
        if (mh::Half()) mh::Mem(mh::at::kActor)[0] = Byte(3 + mh::Next() % 8);
        break;
    case kAuraBreath_InReach: {
        g_reach_enemy = 3 + mh::Next() % 8;
        if (!mh::Often()) break;
        unsigned char* const record = mh::EnemyOf(Byte(g_reach_enemy));
        const int reach = static_cast<int>(mh::Next() % 0x400);
        SetWord(mh::Mem(kScratch), static_cast<unsigned>(reach));
        const int d = reach - 1 + static_cast<int>(mh::Next() % 3);
        SetLong(record + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(sc + 0x34)) +
                                                         (static_cast<std::uint32_t>(d) << 9)));
        SetLong(record + 0x38, Long(sc + 0x38));
        break;
    }
    case kAuraBreathDome_Fade:
        Near(sc[0x5D], 0);
        Near(sc[0xA], 0x1F);
        break;
    case kMagicBallOrb_DrawDisc:
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 10);
        break;
    case kMagicBall_DrawSpark: case kMagicBall_DrawSparkShort:
        if (mh::Half()) sc[2] = Byte(2 + mh::Next() % 2);
        break;
    default: break;
    }
}

// The functions that take arguments: the sparks an angle and a jitter mask
// (their callers' 3, 7 or 0xF most of the time); AuraBreath_InReach the
// enemy record the seed chose (and put one step either side of the reach:
// memory written here, after the input is captured, would be lost).
void Args(unsigned k, std::uint32_t* a) {
    if (k == kMagicBall_DrawSpark || k == kMagicBall_DrawSparkShort) {
        a[0] &= 0xFFFF;
        if (mh::Often()) a[1] = MH_PICK(3, 7, 0xF);
        return;
    }
    if (k == kAuraBreath_InReach) a[0] = Key(mh::EnemyOf(Byte(g_reach_enemy)));
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s36", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s36
