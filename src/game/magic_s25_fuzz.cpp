// BOF3X_SHADOW=magic_s25: group S25's four overlays (MAGIC107..110) through the
// spell round's shared harness (magic_harness.h), once at start-up.
// docs/magic_s25.md section 3.
//
// The clone tables are tools/magic_rows.py --unit MAGIC107..110 --clones
// (2026-09-25; capstone: one jump table, in SpellConfuse_PushFacingMatrix;
// no REFUSED line). Beyond the harness's standard set this group lists the
// draw callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), the other units' functions it calls,
// and its own functions that others of its own call. A hook
// (magic_harness::SetCallHook) logs what an argument mask cannot - the
// SVECTORs a projection reads, the angles, the DR_MOVE rectangle, the
// arguments past the fourth - moves Gfx_PacketNext on by each commit's size
// through a packet buffer of the fuzz's own, as the real commits do, and keeps
// the facing byte a jump table reads inside the table.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s25.h"
#include "game/move_script_bytes.h"

namespace magic_s25 {
namespace {

namespace mh = magic_harness;
namespace addr = bof3::addr;
using move_script::SetLong;
using move_script::SetWord;

// 0x4D2D80: 0x26 bytes  SpellSleep_Task
constexpr mh::Imm kImms4D2D80[] = {{0xF, 0x4D2DB0}, {0x17, 0x4BDC10}};
// 0x4D2DB0: 0x7F bytes  SpellSleep_Start
constexpr mh::CallSite kCalls4D2DB0[] = {{0x0, 0x4FC0E0}, {0x9, 0x435180}, {0x64, 0x587900}};
// 0x4D2E30: 0x50 bytes; +0xB note: call through .data 0x65b918, 10 code entries (a data_tables entry)  SpellSleep_Child
constexpr mh::CallSite kCalls4D2E30[] = {{0x1D, 0x4D2F40}, {0x22, 0x4D3030}, {0x27, 0x4D3350}, {0x2C, 0x4D3160}, {0x31, 0x5A7BC0}, {0x36, 0x4D36C0}, {0x3B, 0x4D3CB0}, {0x40, 0x4D3770}, {0x45, 0x4D3A40}, {0x4A, 0x5A7BC0}};
// 0x4D2E80: 0x26 bytes  SpellSleep_ChildInit
// 0x4D2EB0: 0x29 bytes  SpellSleep_ChildGrow
// 0x4D2EE0: 0x1C bytes  SpellSleep_ChildHold
// 0x4D2F00: 0x36 bytes  SpellSleep_ChildFade
constexpr mh::CallSite kCalls4D2F00[] = {{0x30, 0x4351F0}};
// 0x4D2F40: 0xEA bytes  SpellSleep_PushSwayMatrix
constexpr mh::CallSite kCalls4D2F40[] = {{0x3, 0x5A7B90}, {0x24, 0x5A7A00}, {0x43, 0x5A7A00}, {0xAA, 0x5A8200}, {0xB9, 0x5A8060}, {0xCD, 0x5A7D70}, {0xD7, 0x5A8DE0}, {0xE1, 0x5A8E00}};
// 0x4D3030: 0x126 bytes  SpellSleep_DrawStem
constexpr mh::CallSite kCalls4D3030[] = {{0x13, 0x5A77C0}, {0x29, 0x572FA0}, {0x35, 0x5A76D0}, {0x3D, 0x5A7780}, {0xAA, 0x5A84A0}, {0xB3, 0x5A9420}, {0x118, 0x572FA0}};
// 0x4D3160: 0x1ED bytes  SpellSleep_DrawFan
constexpr mh::CallSite kCalls4D3160[] = {{0x2D, 0x5A77C0}, {0x43, 0x572FA0}, {0x53, 0x5A7A00}, {0x69, 0x5A7A50}, {0xD6, 0x5A7A00}, {0xEC, 0x5A7A50}, {0x122, 0x5A7570}, {0x129, 0x5A7780}, {0x153, 0x5A84A0}, {0x159, 0x5A91C0}, {0x1B8, 0x572FA0}};
// 0x4D3350: 0x362 bytes  SpellSleep_DrawDome
constexpr mh::CallSite kCalls4D3350[] = {{0x2F, 0x5A77C0}, {0x45, 0x572FA0}, {0x63, 0x5A7A50}, {0x76, 0x5A7A00}, {0x89, 0x5A7A50}, {0x9C, 0x5A7A50}, {0xAF, 0x5A7A00}, {0xCA, 0x5A7A50}, {0xDD, 0x5A7A00}, {0xF0, 0x5A7A50}, {0x103, 0x5A7A50}, {0x116, 0x5A7A00}, {0x171, 0x5A7A50}, {0x183, 0x5A7A00}, {0x196, 0x5A7A50}, {0x1A8, 0x5A7A50}, {0x1BB, 0x5A7A00}, {0x1FE, 0x5A7A50}, {0x210, 0x5A7A00}, {0x223, 0x5A7A50}, {0x235, 0x5A7A50}, {0x248, 0x5A7A00}, {0x27B, 0x5A75B0}, {0x286, 0x5A7780}, {0x2B9, 0x5A85F0}, {0x2BF, 0x5A9240}, {0x31F, 0x572FA0}};
// 0x4D36C0: 0xAC bytes  SpellSleep_PushTurnMatrix
constexpr mh::CallSite kCalls4D36C0[] = {{0x3, 0x5A7B90}, {0x6C, 0x5A8200}, {0x7B, 0x5A8060}, {0x8F, 0x5A7D70}, {0x99, 0x5A8DE0}, {0xA3, 0x5A8E00}};
// 0x4D3770: 0x2CD bytes  SpellSleep_DrawTrail
constexpr mh::CallSite kCalls4D3770[] = {{0x4F, 0x5A7A00}, {0x6E, 0x5A7A50}, {0x8A, 0x5A7A00}, {0xAD, 0x5A7A50}, {0xC8, 0x5A7A00}, {0x106, 0x5A7A00}, {0x146, 0x5A7A50}, {0x161, 0x5A7A00}, {0x1A6, 0x5A7A50}, {0x1C2, 0x5A7A00}, {0x1E3, 0x5A77C0}, {0x1F9, 0x572FA0}, {0x205, 0x5A7610}, {0x20C, 0x5A7780}, {0x242, 0x5A85F0}, {0x248, 0x5A9350}, {0x2A0, 0x572FA0}};
// 0x4D3A40: 0x269 bytes  SpellSleep_DrawTrailLines
constexpr mh::CallSite kCalls4D3A40[] = {{0x4F, 0x5A7A00}, {0x6C, 0x5A7A50}, {0x88, 0x5A7A00}, {0xAA, 0x5A7A50}, {0xC2, 0x5A7A00}, {0xE1, 0x5A7A50}, {0x105, 0x5A7A00}, {0x130, 0x5A7A50}, {0x14C, 0x5A7A00}, {0x16C, 0x5A77C0}, {0x182, 0x572FA0}, {0x191, 0x5A76F0}, {0x198, 0x5A7780}, {0x1CB, 0x5A85F0}, {0x1D1, 0x5A9460}, {0x23C, 0x572FA0}};
// 0x4D3CB0: 0x1EB bytes  SpellSleep_DrawShadowFan
constexpr mh::CallSite kCalls4D3CB0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x41, 0x5A7A00}, {0x57, 0x5A7A50}, {0x74, 0x5A7A00}, {0x8B, 0x5A7A50}, {0xF6, 0x5A7A00}, {0x10C, 0x5A7A50}, {0x131, 0x5A75F0}, {0x138, 0x5A7780}, {0x162, 0x5A84A0}, {0x168, 0x5A9310}, {0x1AA, 0x461E50}, {0x1D2, 0x5A77C0}, {0x1DB, 0x461E50}};
// 0x4D3EA0: 0x26 bytes  SpellConfuse_Task
constexpr mh::Imm kImms4D3EA0[] = {{0xF, 0x4D3ED0}, {0x17, 0x4BDC10}};
// 0x4D3ED0: 0x60 bytes  SpellConfuse_Start
constexpr mh::CallSite kCalls4D3ED0[] = {{0xB, 0x435180}};
// 0x4D3F30: 0x3C bytes; +0xB note: call through .data 0x65b928, 6 code entries (a data_tables entry)  SpellConfuse_Child
constexpr mh::CallSite kCalls4D3F30[] = {{0x1D, 0x4D4510}, {0x22, 0x4D45F0}, {0x27, 0x5A7BC0}, {0x2C, 0x4D41B0}, {0x31, 0x4D4300}, {0x36, 0x5A7BC0}};
// 0x4D3F70: 0x102 bytes  SpellConfuse_ChildInit
constexpr mh::CallSite kCalls4D3F70[] = {{0x1F, 0x446770}, {0xF8, 0x587900}};
// 0x4D4080: 0x7E bytes  SpellConfuse_ChildFly
constexpr mh::CallSite kCalls4D4080[] = {{0x4A, 0x4FBA90}, {0x65, 0x4FBC70}};
// 0x4D4100: 0x38 bytes  SpellConfuse_ChildChime
constexpr mh::CallSite kCalls4D4100[] = {{0x1D, 0x587900}};
// 0x4D4140: 0x1D bytes  SpellConfuse_ChildGrow
// 0x4D4160: 0x1C bytes  SpellConfuse_ChildHold
// 0x4D4180: 0x28 bytes  SpellConfuse_ChildEnd
constexpr mh::CallSite kCalls4D4180[] = {{0x22, 0x4351F0}};
// 0x4D41B0: 0x144 bytes  SpellConfuse_PushFacingMatrix
constexpr mh::CallSite kCalls4D41B0[] = {{0x3, 0x5A7B90}, {0xF3, 0x5A8200}, {0x102, 0x5A8060}, {0x116, 0x5A7D70}, {0x120, 0x5A8DE0}, {0x12A, 0x5A8E00}};
constexpr mh::JumpTable kTables4D41B0[] = {{0x1E, 0x134, 4}};
// 0x4D4300: 0x20E bytes  SpellConfuse_DrawRays
constexpr mh::CallSite kCalls4D4300[] = {{0x34, 0x5A7A50}, {0x4B, 0x5A7A00}, {0x9C, 0x5A77C0}, {0xB2, 0x572FA0}, {0xBE, 0x5A7650}, {0xC9, 0x5A7780}, {0xE1, 0x5A8250}, {0xED, 0x5A9110}, {0x124, 0x5A7A50}, {0x13A, 0x5A7A00}, {0x162, 0x5A8250}, {0x16B, 0x5A9110}, {0x1D9, 0x572FA0}};
// 0x4D4510: 0xD6 bytes  SpellConfuse_PushSpinMatrix
constexpr mh::CallSite kCalls4D4510[] = {{0x3, 0x5A7B90}, {0x96, 0x5A8200}, {0xA5, 0x5A8060}, {0xB9, 0x5A7D70}, {0xC3, 0x5A8DE0}, {0xCD, 0x5A8E00}};
// 0x4D45F0: 0x3E3 bytes  SpellConfuse_DrawQuads
constexpr mh::CallSite kCalls4D45F0[] = {{0x18, 0x5A77C0}, {0x2D, 0x572FA0}, {0x39, 0x5A7610}, {0x40, 0x5A7780}, {0x7E, 0x5A7A50}, {0x98, 0x5A7A00}, {0xB9, 0x5A7A50}, {0xD3, 0x5A7A00}, {0xF4, 0x5A7A50}, {0x10E, 0x5A7A00}, {0x12F, 0x5A7A50}, {0x149, 0x5A7A00}, {0x18C, 0x5A85F0}, {0x195, 0x5A9350}, {0x219, 0x572FA0}, {0x229, 0x5A77C0}, {0x23E, 0x572FA0}, {0x24A, 0x5A7610}, {0x251, 0x5A7780}, {0x261, 0x5A7A50}, {0x277, 0x5A7A00}, {0x298, 0x5A7A50}, {0x2B2, 0x5A7A00}, {0x2D3, 0x5A7A50}, {0x2ED, 0x5A7A00}, {0x30E, 0x5A7A50}, {0x328, 0x5A7A00}, {0x36B, 0x5A85F0}, {0x374, 0x5A9350}, {0x3D4, 0x572FA0}};
// 0x4D49E0: 0x36 bytes  SpellDepress_Task
constexpr mh::Imm kImms4D49E0[] = {{0xF, 0x4D4A20}, {0x17, 0x4D4BA0}, {0x22, 0x4D4BE0}, {0x2A, 0x4D4C60}};
// 0x4D4A20: 0x175 bytes  SpellDepress_Start
constexpr mh::CallSite kCalls4D4A20[] = {{0x50, 0x4530D0}, {0x5F, 0x4351F0}, {0x6D, 0x587740}, {0xDA, 0x5A7810}, {0xE3, 0x461E50}};
// 0x4D4BA0: 0x3A bytes  SpellDepress_Open
constexpr mh::CallSite kCalls4D4BA0[] = {{0x33, 0x4D4CE0}};
// 0x4D4BE0: 0x7C bytes  SpellDepress_Hold
constexpr mh::CallSite kCalls4D4BE0[] = {{0x1C, 0x452F70}, {0x36, 0x4D4CE0}};
// 0x4D4C60: 0x7D bytes  SpellDepress_Close
constexpr mh::CallSite kCalls4D4C60[] = {{0x1F, 0x4530D0}, {0x2E, 0x4351F0}, {0x37, 0x4D4CE0}};
// 0x4D4CE0: 0x61D bytes  SpellDepress_DrawVortex
constexpr mh::CallSite kCalls4D4CE0[] = {{0x22, 0x5A7A00}, {0x42, 0x5A7A50}, {0xD6, 0x5A7810}, {0xDF, 0x461E50}, {0xF6, 0x5A77C0}, {0xFF, 0x461E50}, {0x114, 0x5A7A00}, {0x12E, 0x5A7A50}, {0x19B, 0x5A7A00}, {0x1BB, 0x5A7A50}, {0x214, 0x5A75B0}, {0x22E, 0x5A7630}, {0x59F, 0x461E50}, {0x5A8, 0x461E50}};
// 0x4D5300: 0x46 bytes  SpellRagnarok_Task
constexpr mh::Imm kImms4D5300[] = {{0xF, 0x4D5350}, {0x17, 0x4D5460}, {0x22, 0x4D54A0}, {0x2A, 0x4D54E0}, {0x32, 0x4D5560}, {0x3A, 0x43FE80}};
// 0x4D5350: 0x110 bytes  SpellRagnarok_Start
constexpr mh::CallSite kCalls4D5350[] = {{0x0, 0x4FC0E0}, {0x35, 0x435180}, {0x63, 0x435180}, {0x109, 0x587900}};
// 0x4D5460: 0x35 bytes  SpellRagnarok_WaitChildren
constexpr mh::CallSite kCalls4D5460[] = {{0x10, 0x587900}};
// 0x4D54A0: 0x3D bytes  SpellRagnarok_FadeIn
constexpr mh::CallSite kCalls4D54A0[] = {{0x0, 0x4D55C0}};
// 0x4D54E0: 0x7D bytes  SpellRagnarok_Burst
constexpr mh::CallSite kCalls4D54E0[] = {{0x1, 0x4D55C0}, {0x1C, 0x452F70}, {0x2A, 0x435180}};
// 0x4D5560: 0x52 bytes  SpellRagnarok_FadeOut
constexpr mh::CallSite kCalls4D5560[] = {{0x0, 0x4D55C0}, {0x41, 0x4530D0}};
// 0x4D55C0: 0x1B8 bytes  SpellRagnarok_DrawScreenTint
constexpr mh::CallSite kCalls4D55C0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x50, 0x5A7610}, {0x58, 0x5A7780}, {0xEB, 0x461E50}, {0xF7, 0x5A7610}, {0xFF, 0x5A7780}, {0x18D, 0x461E50}, {0x1A2, 0x5A77C0}, {0x1AB, 0x461E50}};
// 0x4D5780: 0x12 bytes; +0xB note: jmp through .data 0x65b960, 14 code entries (a data_tables entry)  SpellRagnarok_Child
// 0x4D57A0: 0x3C bytes; +0x15 note: call through .data 0x65b96c, 11 code entries (a data_tables entry)  SpellRagnarok_SpriteChild
constexpr mh::CallSite kCalls4D57A0[] = {{0x2C, 0x588F20}};
// 0x4D57E0: 0xD2 bytes  SpellRagnarok_SpriteInit
constexpr mh::CallSite kCalls4D57E0[] = {{0xBB, 0x5891F0}};
// 0x4D58C0: 0x44 bytes  SpellRagnarok_SpriteRise
constexpr mh::CallSite kCalls4D58C0[] = {{0x0, 0x5B93D2}};
// 0x4D5910: 0x45 bytes  SpellRagnarok_SpriteClimb
constexpr mh::CallSite kCalls4D5910[] = {{0x0, 0x5B93D2}};
// 0x4D5960: 0x3D bytes  SpellRagnarok_SpriteLeave
constexpr mh::CallSite kCalls4D5960[] = {{0x0, 0x5B93D2}, {0x37, 0x4351F0}};
// 0x4D59A0: 0x38 bytes; +0xB note: call through .data 0x65b980, 6 code entries (a data_tables entry)  SpellRagnarok_RingChild
constexpr mh::CallSite kCalls4D59A0[] = {{0x23, 0x4B7D40}, {0x28, 0x4D5A70}, {0x2D, 0x4D5C10}, {0x32, 0x5A7BC0}};
// 0x4D59E0: 0x6D bytes  SpellRagnarok_RingInit
// 0x4D5A50: 0x20 bytes  SpellRagnarok_RingGrow
// 0x4D5A70: 0x19E bytes  SpellRagnarok_DrawDisc
constexpr mh::CallSite kCalls4D5A70[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x42, 0x5A7A00}, {0x5B, 0x5A7A50}, {0x97, 0x5A75F0}, {0x9F, 0x5A7780}, {0xCD, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x123, 0x5A84A0}, {0x129, 0x5A9310}, {0x180, 0x461E50}};
// 0x4D5C10: 0x22B bytes  SpellRagnarok_DrawRim
constexpr mh::CallSite kCalls4D5C10[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x55, 0x5A7A00}, {0x6E, 0x5A7A50}, {0x87, 0x5A7A00}, {0xA0, 0x5A7A50}, {0xE3, 0x5A7610}, {0xEA, 0x5A7780}, {0x10A, 0x5A7A00}, {0x123, 0x5A7A50}, {0x156, 0x5A7A00}, {0x16F, 0x5A7A50}, {0x1B5, 0x5A85F0}, {0x1BE, 0x5A9350}, {0x20D, 0x461E50}};
// 0x4D5E40: 0x33 bytes; +0xB note: call through .data 0x65b990, 2 code entries (a data_tables entry)  SpellRagnarok_SparkChild
constexpr mh::CallSite kCalls4D5E40[] = {{0x23, 0x4B7D40}, {0x28, 0x4D5F20}, {0x2D, 0x5A7BC0}};
// 0x4D5E80: 0x65 bytes  SpellRagnarok_SparkInit
// 0x4D5EF0: 0x2D bytes  SpellRagnarok_SparkFade
constexpr mh::CallSite kCalls4D5EF0[] = {{0x27, 0x4351F0}};
// 0x4D5F20: 0x1EC bytes  SpellRagnarok_DrawSpark
constexpr mh::CallSite kCalls4D5F20[] = {{0xC, 0x5A75D0}, {0x14, 0x5A7780}, {0x40, 0x5A7A50}, {0x5D, 0x5A7A00}, {0x83, 0x5A7A50}, {0xA0, 0x5A7A00}, {0xC4, 0x5A7A50}, {0xE1, 0x5A7A00}, {0x105, 0x5A7A50}, {0x122, 0x5A7A00}, {0x14E, 0x5A79A0}, {0x15D, 0x5A79E0}, {0x1CF, 0x5A85F0}, {0x1D5, 0x5A9290}, {0x1DE, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"SpellSleep_Task", 0x4D2D80, 0x26, nullptr, 0, kImms4D2D80, MH_N(kImms4D2D80), nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_Task)},
    {"SpellSleep_Start", 0x4D2DB0, 0x7F, kCalls4D2DB0, MH_N(kCalls4D2DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_Start)},
    {"SpellSleep_Child", 0x4D2E30, 0x50, kCalls4D2E30, MH_N(kCalls4D2E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_Child)},
    {"SpellSleep_ChildInit", 0x4D2E80, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_ChildInit)},
    {"SpellSleep_ChildGrow", 0x4D2EB0, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_ChildGrow)},
    {"SpellSleep_ChildHold", 0x4D2EE0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_ChildHold)},
    {"SpellSleep_ChildFade", 0x4D2F00, 0x36, kCalls4D2F00, MH_N(kCalls4D2F00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_ChildFade)},
    {"SpellSleep_PushSwayMatrix", 0x4D2F40, 0xEA, kCalls4D2F40, MH_N(kCalls4D2F40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_PushSwayMatrix)},
    {"SpellSleep_DrawStem", 0x4D3030, 0x126, kCalls4D3030, MH_N(kCalls4D3030), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawStem)},
    {"SpellSleep_DrawFan", 0x4D3160, 0x1ED, kCalls4D3160, MH_N(kCalls4D3160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawFan)},
    {"SpellSleep_DrawDome", 0x4D3350, 0x362, kCalls4D3350, MH_N(kCalls4D3350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawDome)},
    {"SpellSleep_PushTurnMatrix", 0x4D36C0, 0xAC, kCalls4D36C0, MH_N(kCalls4D36C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_PushTurnMatrix)},
    {"SpellSleep_DrawTrail", 0x4D3770, 0x2CD, kCalls4D3770, MH_N(kCalls4D3770), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawTrail)},
    {"SpellSleep_DrawTrailLines", 0x4D3A40, 0x269, kCalls4D3A40, MH_N(kCalls4D3A40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawTrailLines)},
    {"SpellSleep_DrawShadowFan", 0x4D3CB0, 0x1EB, kCalls4D3CB0, MH_N(kCalls4D3CB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellSleep_DrawShadowFan)},
    {"SpellConfuse_Task", 0x4D3EA0, 0x26, nullptr, 0, kImms4D3EA0, MH_N(kImms4D3EA0), nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_Task)},
    {"SpellConfuse_Start", 0x4D3ED0, 0x60, kCalls4D3ED0, MH_N(kCalls4D3ED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_Start)},
    {"SpellConfuse_Child", 0x4D3F30, 0x3C, kCalls4D3F30, MH_N(kCalls4D3F30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_Child)},
    {"SpellConfuse_ChildInit", 0x4D3F70, 0x102, kCalls4D3F70, MH_N(kCalls4D3F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildInit)},
    {"SpellConfuse_ChildFly", 0x4D4080, 0x7E, kCalls4D4080, MH_N(kCalls4D4080), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildFly)},
    {"SpellConfuse_ChildChime", 0x4D4100, 0x38, kCalls4D4100, MH_N(kCalls4D4100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildChime)},
    {"SpellConfuse_ChildGrow", 0x4D4140, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildGrow)},
    {"SpellConfuse_ChildHold", 0x4D4160, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildHold)},
    {"SpellConfuse_ChildEnd", 0x4D4180, 0x28, kCalls4D4180, MH_N(kCalls4D4180), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_ChildEnd)},
    {"SpellConfuse_PushFacingMatrix", 0x4D41B0, 0x144, kCalls4D41B0, MH_N(kCalls4D41B0), nullptr, 0, kTables4D41B0, MH_N(kTables4D41B0), reinterpret_cast<const void*>(&::SpellConfuse_PushFacingMatrix)},
    {"SpellConfuse_DrawRays", 0x4D4300, 0x20E, kCalls4D4300, MH_N(kCalls4D4300), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_DrawRays)},
    {"SpellConfuse_PushSpinMatrix", 0x4D4510, 0xD6, kCalls4D4510, MH_N(kCalls4D4510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_PushSpinMatrix)},
    {"SpellConfuse_DrawQuads", 0x4D45F0, 0x3E3, kCalls4D45F0, MH_N(kCalls4D45F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellConfuse_DrawQuads)},
    {"SpellDepress_Task", 0x4D49E0, 0x36, nullptr, 0, kImms4D49E0, MH_N(kImms4D49E0), nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_Task)},
    {"SpellDepress_Start", 0x4D4A20, 0x175, kCalls4D4A20, MH_N(kCalls4D4A20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_Start)},
    {"SpellDepress_Open", 0x4D4BA0, 0x3A, kCalls4D4BA0, MH_N(kCalls4D4BA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_Open)},
    {"SpellDepress_Hold", 0x4D4BE0, 0x7C, kCalls4D4BE0, MH_N(kCalls4D4BE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_Hold)},
    {"SpellDepress_Close", 0x4D4C60, 0x7D, kCalls4D4C60, MH_N(kCalls4D4C60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_Close)},
    {"SpellDepress_DrawVortex", 0x4D4CE0, 0x61D, kCalls4D4CE0, MH_N(kCalls4D4CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellDepress_DrawVortex)},
    {"SpellRagnarok_Task", 0x4D5300, 0x46, nullptr, 0, kImms4D5300, MH_N(kImms4D5300), nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_Task)},
    {"SpellRagnarok_Start", 0x4D5350, 0x110, kCalls4D5350, MH_N(kCalls4D5350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_Start)},
    {"SpellRagnarok_WaitChildren", 0x4D5460, 0x35, kCalls4D5460, MH_N(kCalls4D5460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_WaitChildren)},
    {"SpellRagnarok_FadeIn", 0x4D54A0, 0x3D, kCalls4D54A0, MH_N(kCalls4D54A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_FadeIn)},
    {"SpellRagnarok_Burst", 0x4D54E0, 0x7D, kCalls4D54E0, MH_N(kCalls4D54E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_Burst)},
    {"SpellRagnarok_FadeOut", 0x4D5560, 0x52, kCalls4D5560, MH_N(kCalls4D5560), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_FadeOut)},
    {"SpellRagnarok_DrawScreenTint", 0x4D55C0, 0x1B8, kCalls4D55C0, MH_N(kCalls4D55C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_DrawScreenTint)},
    {"SpellRagnarok_Child", 0x4D5780, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_Child)},
    {"SpellRagnarok_SpriteChild", 0x4D57A0, 0x3C, kCalls4D57A0, MH_N(kCalls4D57A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SpriteChild)},
    {"SpellRagnarok_SpriteInit", 0x4D57E0, 0xD2, kCalls4D57E0, MH_N(kCalls4D57E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SpriteInit)},
    {"SpellRagnarok_SpriteRise", 0x4D58C0, 0x44, kCalls4D58C0, MH_N(kCalls4D58C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SpriteRise)},
    {"SpellRagnarok_SpriteClimb", 0x4D5910, 0x45, kCalls4D5910, MH_N(kCalls4D5910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SpriteClimb)},
    {"SpellRagnarok_SpriteLeave", 0x4D5960, 0x3D, kCalls4D5960, MH_N(kCalls4D5960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SpriteLeave)},
    {"SpellRagnarok_RingChild", 0x4D59A0, 0x38, kCalls4D59A0, MH_N(kCalls4D59A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_RingChild)},
    {"SpellRagnarok_RingInit", 0x4D59E0, 0x6D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_RingInit)},
    {"SpellRagnarok_RingGrow", 0x4D5A50, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_RingGrow)},
    {"SpellRagnarok_DrawDisc", 0x4D5A70, 0x19E, kCalls4D5A70, MH_N(kCalls4D5A70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_DrawDisc)},
    {"SpellRagnarok_DrawRim", 0x4D5C10, 0x22B, kCalls4D5C10, MH_N(kCalls4D5C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_DrawRim)},
    {"SpellRagnarok_SparkChild", 0x4D5E40, 0x33, kCalls4D5E40, MH_N(kCalls4D5E40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SparkChild)},
    {"SpellRagnarok_SparkInit", 0x4D5E80, 0x65, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SparkInit)},
    {"SpellRagnarok_SparkFade", 0x4D5EF0, 0x2D, kCalls4D5EF0, MH_N(kCalls4D5EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_SparkFade)},
    {"SpellRagnarok_DrawSpark", 0x4D5F20, 0x1EC, kCalls4D5F20, MH_N(kCalls4D5F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellRagnarok_DrawSpark)},
};
#undef MH_N

enum : unsigned {
    kSleep_Task, kSleep_Start, kSleep_Child, kSleep_ChildInit, kSleep_ChildGrow, kSleep_ChildHold, kSleep_ChildFade,
    kSleep_PushSwayMatrix, kSleep_DrawStem, kSleep_DrawFan, kSleep_DrawDome, kSleep_PushTurnMatrix, kSleep_DrawTrail,
    kSleep_DrawTrailLines, kSleep_DrawShadowFan, kConfuse_Task, kConfuse_Start, kConfuse_Child, kConfuse_ChildInit,
    kConfuse_ChildFly, kConfuse_ChildChime, kConfuse_ChildGrow, kConfuse_ChildHold, kConfuse_ChildEnd,
    kConfuse_PushFacingMatrix, kConfuse_DrawRays, kConfuse_PushSpinMatrix, kConfuse_DrawQuads, kDepress_Task,
    kDepress_Start, kDepress_Open, kDepress_Hold, kDepress_Close, kDepress_DrawVortex, kRagnarok_Task, kRagnarok_Start,
    kRagnarok_WaitChildren, kRagnarok_FadeIn, kRagnarok_Burst, kRagnarok_FadeOut, kRagnarok_DrawScreenTint,
    kRagnarok_Child, kRagnarok_SpriteChild, kRagnarok_SpriteInit, kRagnarok_SpriteRise, kRagnarok_SpriteClimb,
    kRagnarok_SpriteLeave, kRagnarok_RingChild, kRagnarok_RingInit, kRagnarok_RingGrow, kRagnarok_DrawDisc,
    kRagnarok_DrawRim, kRagnarok_SparkChild, kRagnarok_SparkInit, kRagnarok_SparkFade, kRagnarok_DrawSpark, kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one seed index per clone");

template <typename F> std::uint32_t Key(F* f) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reinterpret_cast<void*>(f)));
}
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;

#define S25_OURS(name) #name, ::bof3::addr::name, Key(&::name)
#define S25_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, battle_items: all ours)
    {S25_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},   // + tw: the hook
    {S25_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetDrawMove), 4, {kAll, 0, kAll, kAll}, mh::Answer::kGarbage, 0, 0},   // + the rect: the hook
    {S25_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetLineG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetLineF2), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetPolyF4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetPolyG4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetPolyFT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gpu_SetPolyGT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_RAW(0x5A7570), 1, {kAll}, mh::Answer::kGarbage, 0, 0},   // libgpu SetPolyF3, unnamed
    {S25_RAW(0x5A76F0), 1, {kAll}, mh::Answer::kGarbage, 0, 0},   // libgpu SetLineG4, unnamed
    {S25_OURS(Gte_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_RotTrans), 3, {0, 0, 0}, mh::Answer::kGarbage, 0, 0},        // stack pointers; the vector: the hook
    {S25_OURS(Gte_RotMatrix), 2, {0, 0}, mh::Answer::kGarbage, 0, 0},          // the angles: the hook
    {S25_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, mh::Answer::kGarbage, 0, 0},   // Camera_Matrix, the stack's
    {S25_OURS(Gte_SetRotMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_SetTransMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_RotTransPers3), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_RotTransPers4), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_StoreDepthF), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths3_0C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths3_10C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths4_0C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths4_10), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths4_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Gte_PrimDepths4_10C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(Sprite_SetAnimation), 1, {kU8}, mh::Answer::kGarbage, 0, 0},
    // other units: the effect library (group L), the engine
    {S25_RAW(0x4FC0E0), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_RAW(0x4FBA90), 4, {kAll, kAll, kAll, 0}, mh::Answer::kGarbage, 0, 0},   // the pad unread; + speed: the hook
    {S25_RAW(0x4FBC70), 4, {kAll, kAll, 0xFFFF0000u, kAll}, mh::Answer::kBool, 0, 0},
    {S25_RAW(0x446770), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    // this group's own, called by its own
    {S25_OURS(SpellSleep_PushSwayMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawStem), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawFan), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawDome), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_PushTurnMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawTrail), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawTrailLines), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellSleep_DrawShadowFan), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellConfuse_PushFacingMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellConfuse_DrawRays), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellConfuse_PushSpinMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellConfuse_DrawQuads), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellDepress_DrawVortex), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellRagnarok_DrawScreenTint), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellRagnarok_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellRagnarok_DrawRim), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S25_OURS(SpellRagnarok_DrawSpark), 0, {}, mh::Answer::kGarbage, 0, 0},
};
#undef S25_OURS
#undef S25_RAW

// The six .data phase tables (their entries swapped for handler recorders
// while the fuzz runs).
const mh::DataTable kTables[] = {
    {addr::SpellSleep_ChildPhases, 4},     {addr::SpellConfuse_ChildPhases, 6},  {addr::SpellRagnarok_ChildKinds, 3},
    {addr::SpellRagnarok_SpritePhases, 5}, {addr::SpellRagnarok_RingPhases, 4}, {addr::SpellRagnarok_SparkPhases, 2},
};

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kCasterRecord = 0x904B3C, kSideCorner = 0x904AAC,
                        kSpriteBank = 0x9039D8;

mh::Region g_regions[] = {
    {0x7E0670, 4},                       // Gfx_PacketNext
    {0, kPrimBytes},                     // g_prims (filled in at start-up)
    {kVertex, 0x20},                     // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                    // 0x903850.., Scratch_Swap at +0xC
    {0x905E60, 8},                       // Field_Kind2Z, Field_Kind2X
    {0x905B89, 1},                       // Gfx_BufferIndex
    {kSpriteBank, 4},                    // the sprite bank pointer SpellRagnarok_SpriteChild swaps
    {0x80B980, 0x20},                    // the CLUT strips SpellRagnarok_Start copies
    {0x80E980, 0x200},
    {0x80F980, 0x20},
    {0x812980, 0x200},
    {addr::SpellDepress_Outer, 0x580},   // Outer, Rings, Inner: 0x698940..0x698EBF
};

// Gfx_CommitPrim / MapView_LinkPrimAt's advance of Gfx_PacketNext, kept in the
// buffer (the real ones advance it by the size; a draw writes up to 0x8C past
// it).
void Advance(std::uint32_t size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
}

const void* At(std::uint32_t v) { return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(v)); }

void Hook(std::uint32_t address, const std::uint32_t* a, bool after) {
    if (after) {
        // SpellConfuse_PushFacingMatrix's jump table reads the facing right
        // after this call; past 3 the original's angles are stack garbage and
        // ours aborts: the fuzz stays inside the table.
        if (address == addr::Gte_PushMatrix) Sprite_Current[8] &= 3;
        return;
    }
    switch (address) {
    case addr::Gte_RotTransPers3:
        for (unsigned i = 0; i < 3; ++i) mh::LogBytes(At(a[i]), 6);
        mh::LogValue(a[4]);
        mh::LogValue(a[5]);
        break;
    case addr::Gte_RotTransPers4:
        for (unsigned i = 0; i < 4; ++i) mh::LogBytes(At(a[i]), 6);
        for (unsigned i = 4; i < 8; ++i) mh::LogValue(a[i]);
        break;
    case addr::Gte_RotTransPers:
    case addr::Gte_RotTrans:
    case addr::Gte_RotMatrix: mh::LogBytes(At(a[0]), 6); break;
    case addr::Gpu_SetDrawMove: mh::LogBytes(At(a[1]), 8); break;
    case addr::Gpu_SetDrawMode: mh::LogValue(a[4]); break;
    case 0x4FBA90: mh::LogValue(a[4] & 0xFFFF); break;
    case addr::Gfx_CommitPrim: Advance(a[1]); break;
    case addr::MapView_LinkPrimAt: Advance(a[3]); break;
    default: break;
    }
}

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2:
        // the dword at 0x903850 is SpellConfuse_DrawRays' loop counter: only
        // its low byte, and small, or the loop runs for ever
        if (v % 16 < 4) mh::Mem(kScratch)[0] = static_cast<unsigned char>((h >> 24) & 0x3F);
        else mh::Mem(kScratch + v % 16)[0] = static_cast<unsigned char>(h >> 24);
        break;
    case 3: SetLong(mh::Mem(0x905E60 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: mh::Mem(kSideCorner)[0] = static_cast<unsigned char>(v & 3); break;
    case 5: mh::Mem(addr::SpellDepress_Outer + v % 0x580)[0] = static_cast<unsigned char>(h >> 24); break;
    case 6: Gfx_BufferIndex = static_cast<unsigned char>(v & 1); break;
    default: break;
    }
}

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
// Half the time the byte one step before a threshold, or at it (one past what
// the step reaches); else as the random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kCasterRecord, mh::SpriteRecord(mh::Next()));
    if (mh::Often()) mh::Mem(kSideCorner)[0] = Byte(mh::Next() % 4);
    if (mh::Often()) Gfx_BufferIndex = Byte(mh::Next() & 1);
    if (mh::Often()) sc[8] = Byte(mh::Next() % 4);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kSleep_Task: case kConfuse_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kDepress_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kRagnarok_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kSleep_Child: sc[1] = Byte(mh::Next() % 4); break;
    case kConfuse_Child: sc[1] = Byte(mh::Next() % 6); break;
    case kRagnarok_Child: sc[1] = Byte(mh::Next() % 3); break;
    case kRagnarok_SpriteChild: sc[2] = Byte(mh::Next() % 5); break;
    case kRagnarok_RingChild: sc[2] = Byte(mh::Next() % 4); break;
    case kRagnarok_SparkChild: sc[2] = Byte(mh::Next() % 2); break;
    // the counters: at their thresholds
    case kSleep_ChildGrow: Near(sc[9], 0x1F); break;
    case kSleep_ChildHold: Near(sc[9], 0xBF); break;
    case kSleep_ChildFade: Near(sc[0xA], 1); break;
    case kConfuse_ChildChime: Near(sc[0xA], 0x1F); break;
    case kConfuse_ChildGrow: Near(sc[9], 0x78); break;
    case kConfuse_ChildHold: Near(sc[0xA], 0x5F); break;
    case kConfuse_ChildEnd: Near(sc[0xA], 0x7F); break;
    case kDepress_Open: Near(sc[9], 0x52); break;
    case kDepress_Hold:
    case kDepress_Close:
        if (k == kDepress_Hold) Near(sc[0xA], 0);
        else Near(sc[9], 4);
        if (mh::Often()) {
            SetLong(sc + 0x64, static_cast<std::int32_t>(MH_PICK(0x78, 0x77, 0x80, static_cast<std::uint32_t>(-0x78),
                                                                 static_cast<std::uint32_t>(-0x77),
                                                                 static_cast<std::uint32_t>(-0x80), 0)));
            sc[0xB] = Byte(mh::Next() & 1);
        }
        break;
    case kDepress_Start:
        if (mh::Often()) mh::Mem(mh::at::kTarget)[0] = Byte(MH_PICK(0x80, 0x40, 0x80, 0x40, 3, 4, 0xC0));
        sc[1] = 0;
        break;
    case kRagnarok_WaitChildren: if (mh::Half()) sc[0xB] = 2; break;
    case kRagnarok_FadeIn:
        if (mh::Half()) sc[0xA] = Byte(0xE + mh::Next() % 4);
        Near(sc[9], 0xF);
        break;
    case kRagnarok_Burst: if (mh::Half()) sc[0xB] = 0; break;
    case kRagnarok_FadeOut:
        if (mh::Half()) Frame_Counter &= ~3u;
        if (mh::Half()) sc[0xA] = 0;
        Near(sc[9], 1);
        break;
    case kRagnarok_SpriteRise:
        if (mh::Half()) SetLong(sc + 0x44, static_cast<std::int32_t>(MH_PICK(0x16000, 0x15FFF)));
        break;
    case kRagnarok_SpriteClimb: if (mh::Half()) SetWord(sc + 0x30, MH_PICK(0xB6, 0xB5)); break;
    case kRagnarok_SpriteLeave: if (mh::Half()) SetWord(sc + 0x30, MH_PICK(0x82, 0x81)); break;
    case kRagnarok_RingGrow: if (mh::Half()) SetLong(sc + 0x14, static_cast<std::int32_t>(MH_PICK(0xBC0, 0xBBF))); break;
    case kRagnarok_SparkInit: if (mh::Often()) sc[0xB] = Byte(mh::Next() % 6); break;
    case kRagnarok_SparkFade: Near(sc[0xA], 1); break;
    // the draws: both phase parities, a short count now and then
    case kSleep_DrawFan: case kSleep_DrawDome: sc[1] = Byte(mh::Next() % 4); break;
    case kConfuse_DrawRays:
        sc[1] = Byte(mh::Half() ? 5 : mh::Next() % 6);
        if (mh::Often()) sc[9] = Byte(mh::Next() % 24);
        break;
    case kConfuse_DrawQuads:
        sc[1] = Byte(MH_PICK(1, 5, 0, 2));
        if (mh::Half()) sc[0xA] = Byte(0x17 + mh::Next() % 3);
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    const mh::Group group = {
        "magic_s25", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::SetCallHook(&Hook);
    mh::Run(group);
}

}  // namespace magic_s25
