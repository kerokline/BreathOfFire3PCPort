// BOF3X_SHADOW=magic_s14: group S14's three overlays (MAGIC064, MAGIC065's
// last function, MAGIC066) through the spell round's shared harness
// (magic_harness.h), once at start-up. docs/magic_s14.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC064 / 065 / 066 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// GTE and libgpu entry points, Math_Sin / Math_Cos, Gfx_CommitPrim,
// MapView_LinkPrimAt), the sprite calls, the field-view calls of
// Weretiger_ResetMapView, the file calls, Str_CopyN, Battle_ActorIsOut, and
// the functions of its own that its functions call directly. Everything the
// harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the GTE callees of the ring's matrix push log what their pointers point
//     at (`deref`) and write a result where the real ones write;
//   - the sprite calls that act on Sprite_Current (the animation, the script
//     tick, the palette, the CLUT STP bits, the screen update) log which
//     sprite: Weretiger's steps swap Sprite_Current for the acting member's
//     record round them;
//   - the pool's current record (0x684788) is kept pointing into the pool,
//     and the group's disturbance re-points it, so the streak functions'
//     re-reads after each call are compared.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s14.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s14 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC064 / 065 / 066 --clones, 2026-09-26, names
// given.
// 0x4B3F00: 0x2E bytes  Weretiger_Task
constexpr mh::Imm kImms4B3F00[] = {{0xF, 0x492750}, {0x17, 0x4B3F30}, {0x22, 0x4B4510}};
// 0x4B3F30: 0x76 bytes  Weretiger_Run
constexpr mh::Imm kImms4B3F30[] = {{0xF, 0x4B3FB0}, {0x17, 0x4B4010}, {0x22, 0x4B4060}, {0x2A, 0x4B4140}, {0x32, 0x437CC0}, {0x3A, 0x4B41C0}, {0x42, 0x4B4250}, {0x4A, 0x437CC0}, {0x52, 0x4B4290}, {0x5A, 0x4B4350}, {0x62, 0x4B4470}, {0x6A, 0x4B44B0}};
// 0x4B3FB0: 0x59 bytes  Weretiger_Pose
constexpr mh::CallSite kCalls4B3FB0[] = {{0x2D, 0x589330}, {0x3D, 0x587740}};
// 0x4B4010: 0x46 bytes  Weretiger_WaitPose
constexpr mh::CallSite kCalls4B4010[] = {{0x27, 0x589410}};
// 0x4B4060: 0xD6 bytes  Weretiger_FocusActor
constexpr mh::CallSite kCalls4B4060[] = {{0x75, 0x5720C0}, {0x82, 0x4B5350}, {0x8B, 0x435180}};
// 0x4B4140: 0x7F bytes  Weretiger_Burst
constexpr mh::CallSite kCalls4B4140[] = {{0xE, 0x571FF0}, {0x34, 0x435180}, {0x6D, 0x587740}};
// 0x4B41C0: 0x8E bytes  Weretiger_Copy
constexpr mh::CallSite kCalls4B41C0[] = {{0x7, 0x435180}};
// 0x4B4250: 0x40 bytes  Weretiger_Release
constexpr mh::CallSite kCalls4B4250[] = {{0x19, 0x454DC0}};
// 0x4B4290: 0xB7 bytes  Weretiger_LoadForm
constexpr mh::CallSite kCalls4B4290[] = {{0x33, 0x4B5240}, {0x44, 0x4B5240}, {0x51, 0x4B5240}, {0x5E, 0x4B5240}, {0xA6, 0x454590}};
// 0x4B4350: 0x11C bytes  Weretiger_FadeIn
constexpr mh::CallSite kCalls4B4350[] = {{0x33, 0x4B5240}, {0x44, 0x4B5240}, {0x51, 0x4B5240}, {0x5E, 0x4B5240}, {0x7A, 0x454810}, {0x90, 0x589330}, {0xB0, 0x454DC0}, {0xCC, 0x5366A0}, {0xED, 0x446BB0}, {0xF2, 0x4551A0}, {0x104, 0x587740}};
// 0x4B4470: 0x3B bytes  Weretiger_WaitScript
constexpr mh::CallSite kCalls4B4470[] = {{0x27, 0x589410}};
// 0x4B44B0: 0x53 bytes  Weretiger_Return
constexpr mh::CallSite kCalls4B44B0[] = {{0x26, 0x4B5350}};
// 0x4B4510: 0x76 bytes  Weretiger_End
constexpr mh::CallSite kCalls4B4510[] = {{0xD, 0x571FF0}, {0x68, 0x4530D0}, {0x70, 0x4351F0}};
// 0x4B4590: 0x36 bytes  WeretigerChild_Task
constexpr mh::Imm kImms4B4590[] = {{0xF, 0x4B45D0}, {0x17, 0x4B4A00}, {0x22, 0x4B5120}, {0x2A, 0x4B51C0}};
// 0x4B45D0: 0x3E bytes  WeretigerCopy_Run
constexpr mh::CallSite kCalls4B45D0[] = {{0x35, 0x588F20}};
constexpr mh::Imm kImms4B45D0[] = {{0xF, 0x4B5190}, {0x17, 0x4B4610}, {0x22, 0x4AEE90}};
// 0x4B4610: 0x26 bytes  WeretigerCopy_Step
constexpr mh::Imm kImms4B4610[] = {{0xF, 0x4B4640}, {0x17, 0x4B49D0}};
// 0x4B4640: 0x38E bytes  WeretigerCopy_Spawn
constexpr mh::CallSite kCalls4B4640[] = {{0x50, 0x461E10}, {0x58, 0x435180}, {0x10F, 0x5366A0}, {0x130, 0x446BB0}, {0x135, 0x4551A0}, {0x143, 0x435180}, {0x1F0, 0x435180}, {0x2A6, 0x5366A0}, {0x2C7, 0x446BB0}, {0x2CF, 0x4551A0}, {0x2DD, 0x435180}};
// 0x4B49D0: 0x29 bytes  WeretigerCopy_Tick
constexpr mh::CallSite kCalls4B49D0[] = {{0x0, 0x589410}};
// 0x4B4A00: 0x82 bytes  WeretigerBurst_Run
constexpr mh::CallSite kCalls4B4A00[] = {{0x4B, 0x5A77C0}, {0x54, 0x461E50}, {0x71, 0x4B4BD0}};
constexpr mh::Imm kImms4B4A00[] = {{0x14, 0x4B4A90}, {0x1C, 0x4B4AC0}, {0x24, 0x4B4AF0}, {0x2C, 0x4B4B30}, {0x34, 0x4B4B50}};
// 0x4B4A90: 0x21 bytes  WeretigerBurst_Start
constexpr mh::CallSite kCalls4B4A90[] = {{0x13, 0x4B4B90}};
// 0x4B4AC0: 0x22 bytes  WeretigerBurst_Emit
constexpr mh::CallSite kCalls4B4AC0[] = {{0x0, 0x4B4B90}};
// 0x4B4AF0: 0x37 bytes  WeretigerBurst_Grow
constexpr mh::CallSite kCalls4B4AF0[] = {{0x0, 0x4B4B90}};
// 0x4B4B30: 0x18 bytes  WeretigerBurst_WaitOwner
constexpr mh::CallSite kCalls4B4B30[] = {{0x13, 0x4B4B90}};
// 0x4B4B50: 0x34 bytes  WeretigerBurst_End
constexpr mh::CallSite kCalls4B4B50[] = {{0x2E, 0x4351F0}};
// 0x4B4B90: 0x3D bytes  WeretigerStreak_Alloc6
// 0x4B4BD0: 0x26 bytes  WeretigerStreak_Run
constexpr mh::Imm kImms4B4BD0[] = {{0xF, 0x4B4C00}, {0x17, 0x4B4CE0}};
// 0x4B4C00: 0xD1 bytes  WeretigerStreak_Start
constexpr mh::CallSite kCalls4B4C00[] = {{0x0, 0x5B93D2}};
// 0x4B4CE0: 0x3B5 bytes  WeretigerStreak_Draw
constexpr mh::CallSite kCalls4B4CE0[] = {{0x5A, 0x5A7A00}, {0x7F, 0x5A7A50}, {0xA4, 0x5A7A00}, {0xC9, 0x5A7A50}, {0xE9, 0x4B50A0}, {0xF9, 0x5A7A00}, {0x11F, 0x5A7A50}, {0x145, 0x5A7A00}, {0x16B, 0x5A7A50}, {0x18B, 0x4B50A0}, {0x19D, 0x5A7A00}, {0x1C5, 0x5A7A50}, {0x1ED, 0x5A7A00}, {0x215, 0x5A7A50}, {0x235, 0x4B50A0}, {0x247, 0x5A7A00}, {0x26F, 0x5A7A50}, {0x297, 0x5A7A00}, {0x2BF, 0x5A7A50}, {0x2DF, 0x4B50A0}, {0x2F1, 0x5A7A00}, {0x319, 0x5A7A50}, {0x341, 0x5A7A00}, {0x369, 0x5A7A50}, {0x389, 0x4B50A0}};
// 0x4B50A0: 0x7A bytes  WeretigerStreak_DrawLine
constexpr mh::CallSite kCalls4B50A0[] = {{0x8, 0x5A76B0}, {0x67, 0x5A7780}, {0x70, 0x461E50}};
// 0x4B5120: 0x3E bytes  WeretigerImage_Run
constexpr mh::CallSite kCalls4B5120[] = {{0x35, 0x588F20}};
constexpr mh::Imm kImms4B5120[] = {{0xF, 0x4B5160}, {0x17, 0x4B5190}, {0x22, 0x4B51A0}};
// 0x4B5160: 0x2C bytes  WeretigerImage_Play
constexpr mh::CallSite kCalls4B5160[] = {{0x9, 0x589330}, {0x11, 0x589410}, {0x1A, 0x589410}};
// 0x4B5190: 0x9 bytes  WeretigerFx_Next
// 0x4B51A0: 0x13 bytes  WeretigerImage_End
constexpr mh::CallSite kCalls4B51A0[] = {{0x6, 0x454DC0}, {0xE, 0x4351F0}};
// 0x4B51C0: 0x12 bytes; +0xB note: jmp through .data 0x65ac00, 4 code entries (a data_tables entry)  WeretigerDim_Run
// 0x4B51E0: 0x31 bytes  WeretigerDim_Down
constexpr mh::CallSite kCalls4B51E0[] = {{0x2A, 0x573050}};
// 0x4B5220: 0x14 bytes  WeretigerDim_WaitOwner
// 0x4B5240: 0x10F bytes  Weretiger_DrawSprite
constexpr mh::CallSite kCalls4B5240[] = {{0x19, 0x5A79A0}, {0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x54, 0x5A7710}, {0xEF, 0x5A7780}, {0x105, 0x572FA0}};
// 0x4B5350: 0x158 bytes  Weretiger_ResetMapView
constexpr mh::CallSite kCalls4B5350[] = {{0x99, 0x56EB50}, {0xBA, 0x56EAB0}, {0x100, 0x56F910}, {0x14E, 0x56FAD0}, {0x153, 0x571720}};
// 0x4B58F0: 0xA8 bytes  Item_CopyName
constexpr mh::CallSite kCalls4B58F0[] = {{0x30, 0x5171A0}, {0x4E, 0x5171A0}, {0x75, 0x5171A0}, {0x9F, 0x5171A0}};
// 0x4B59A0: 0x2E bytes  Tsunami_Task
constexpr mh::Imm kImms4B59A0[] = {{0xF, 0x4B59D0}, {0x17, 0x4B5AB0}, {0x22, 0x4BDC10}};
// 0x4B59D0: 0xD9 bytes  Tsunami_Start
constexpr mh::CallSite kCalls4B59D0[] = {{0x37, 0x435180}, {0xB5, 0x587740}};
// 0x4B5AB0: 0x5E bytes  Tsunami_Rings
constexpr mh::CallSite kCalls4B5AB0[] = {{0x1D, 0x435180}, {0x4D, 0x452F70}};
// 0x4B5B10: 0x12 bytes; +0xB note: jmp through .data 0x65ac2c, 2 code entries (a data_tables entry)  TsunamiChild_Task
// 0x4B5B30: 0x2E bytes; +0xB note: call through .data 0x65ac34, 5 code entries (a data_tables entry)  TsunamiWave_Run
constexpr mh::CallSite kCalls4B5B30[] = {{0x1E, 0x4B7D40}, {0x23, 0x4B5CB0}, {0x28, 0x5A7BC0}};
// 0x4B5B60: 0x6E bytes  TsunamiWave_Start
// 0x4B5BD0: 0x2E bytes  TsunamiWave_Advance
// 0x4B5C00: 0x30 bytes  TsunamiWave_Grow
// 0x4B5C30: 0x3E bytes  TsunamiWave_Shrink
// 0x4B5C70: 0x36 bytes  TsunamiWave_End
constexpr mh::CallSite kCalls4B5C70[] = {{0x30, 0x4351F0}};
// 0x4B5CB0: 0x4AB bytes  TsunamiWave_Draw
constexpr mh::CallSite kCalls4B5CB0[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x5C, 0x5A7A00}, {0x88, 0x5A75D0}, {0x90, 0x5A7780}, {0x101, 0x5A7A00}, {0x186, 0x5A79A0}, {0x195, 0x5A79E0}, {0x224, 0x5A85F0}, {0x22D, 0x5A9290}, {0x236, 0x461E50}, {0x286, 0x5A7A00}, {0x2B5, 0x5A7610}, {0x2BD, 0x5A7780}, {0x32E, 0x5A7A00}, {0x45F, 0x5A85F0}, {0x465, 0x5A9350}, {0x46E, 0x461E50}, {0x492, 0x5A77C0}, {0x49B, 0x461E50}};
// 0x4B6160: 0x12 bytes; +0xB note: jmp through .data 0x65ac48, 3 code entries (a data_tables entry)  TsunamiRings_Run
// 0x4B6180: 0x19 bytes  TsunamiRings_Start
// 0x4B61A0: 0x21 bytes  TsunamiRings_Grow
constexpr mh::CallSite kCalls4B61A0[] = {{0x0, 0x4B6200}};
// 0x4B61D0: 0x23 bytes  TsunamiRings_End
constexpr mh::CallSite kCalls4B61D0[] = {{0x0, 0x4B6200}, {0x1D, 0x4351F0}};
// 0x4B6200: 0x1B7 bytes  TsunamiRings_Draw
constexpr mh::CallSite kCalls4B6200[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x3D, 0x4456C0}, {0x8F, 0x4B6620}, {0x94, 0x4B63C0}, {0x99, 0x5A7BC0}, {0xDA, 0x4456C0}, {0x12C, 0x4B6620}, {0x131, 0x4B63C0}, {0x136, 0x5A7BC0}, {0x1A1, 0x5A77C0}, {0x1AA, 0x461E50}};
// 0x4B63C0: 0x252 bytes  TsunamiRing_DrawQuads
constexpr mh::CallSite kCalls4B63C0[] = {{0x34, 0x5A75D0}, {0x3C, 0x5A7780}, {0x68, 0x5A7A50}, {0x85, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xCC, 0x5A7A00}, {0xF6, 0x5A7A50}, {0x113, 0x5A7A00}, {0x13D, 0x5A7A50}, {0x15A, 0x5A7A00}, {0x18D, 0x5A79A0}, {0x19D, 0x5A79E0}, {0x226, 0x5A85F0}, {0x22C, 0x5A9290}, {0x235, 0x461E50}};
// 0x4B6620: 0xA1 bytes  TsunamiRing_PushMatrix
constexpr mh::CallSite kCalls4B6620[] = {{0x3, 0x5A7B90}, {0x61, 0x5A8200}, {0x70, 0x5A8060}, {0x84, 0x5A7D70}, {0x8E, 0x5A8DE0}, {0x98, 0x5A8E00}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S14_P(name) reinterpret_cast<const void*>(&::name)
const mh::Clone kClones[] = {
    {"Weretiger_Task", 0x4B3F00, 0x2E, nullptr, 0, kImms4B3F00, MH_N(kImms4B3F00), nullptr, 0, S14_P(Weretiger_Task)},
    {"Weretiger_Run", 0x4B3F30, 0x76, nullptr, 0, kImms4B3F30, MH_N(kImms4B3F30), nullptr, 0, S14_P(Weretiger_Run)},
    {"Weretiger_Pose", 0x4B3FB0, 0x59, kCalls4B3FB0, MH_N(kCalls4B3FB0), nullptr, 0, nullptr, 0, S14_P(Weretiger_Pose)},
    {"Weretiger_WaitPose", 0x4B4010, 0x46, kCalls4B4010, MH_N(kCalls4B4010), nullptr, 0, nullptr, 0, S14_P(Weretiger_WaitPose)},
    {"Weretiger_FocusActor", 0x4B4060, 0xD6, kCalls4B4060, MH_N(kCalls4B4060), nullptr, 0, nullptr, 0, S14_P(Weretiger_FocusActor)},
    {"Weretiger_Burst", 0x4B4140, 0x7F, kCalls4B4140, MH_N(kCalls4B4140), nullptr, 0, nullptr, 0, S14_P(Weretiger_Burst)},
    {"Weretiger_Copy", 0x4B41C0, 0x8E, kCalls4B41C0, MH_N(kCalls4B41C0), nullptr, 0, nullptr, 0, S14_P(Weretiger_Copy)},
    {"Weretiger_Release", 0x4B4250, 0x40, kCalls4B4250, MH_N(kCalls4B4250), nullptr, 0, nullptr, 0, S14_P(Weretiger_Release)},
    {"Weretiger_LoadForm", 0x4B4290, 0xB7, kCalls4B4290, MH_N(kCalls4B4290), nullptr, 0, nullptr, 0, S14_P(Weretiger_LoadForm)},
    {"Weretiger_FadeIn", 0x4B4350, 0x11C, kCalls4B4350, MH_N(kCalls4B4350), nullptr, 0, nullptr, 0, S14_P(Weretiger_FadeIn)},
    {"Weretiger_WaitScript", 0x4B4470, 0x3B, kCalls4B4470, MH_N(kCalls4B4470), nullptr, 0, nullptr, 0, S14_P(Weretiger_WaitScript)},
    {"Weretiger_Return", 0x4B44B0, 0x53, kCalls4B44B0, MH_N(kCalls4B44B0), nullptr, 0, nullptr, 0, S14_P(Weretiger_Return)},
    {"Weretiger_End", 0x4B4510, 0x76, kCalls4B4510, MH_N(kCalls4B4510), nullptr, 0, nullptr, 0, S14_P(Weretiger_End)},
    {"WeretigerChild_Task", 0x4B4590, 0x36, nullptr, 0, kImms4B4590, MH_N(kImms4B4590), nullptr, 0, S14_P(WeretigerChild_Task)},
    {"WeretigerCopy_Run", 0x4B45D0, 0x3E, kCalls4B45D0, MH_N(kCalls4B45D0), kImms4B45D0, MH_N(kImms4B45D0), nullptr, 0, S14_P(WeretigerCopy_Run)},
    {"WeretigerCopy_Step", 0x4B4610, 0x26, nullptr, 0, kImms4B4610, MH_N(kImms4B4610), nullptr, 0, S14_P(WeretigerCopy_Step)},
    {"WeretigerCopy_Spawn", 0x4B4640, 0x38E, kCalls4B4640, MH_N(kCalls4B4640), nullptr, 0, nullptr, 0, S14_P(WeretigerCopy_Spawn)},
    {"WeretigerCopy_Tick", 0x4B49D0, 0x29, kCalls4B49D0, MH_N(kCalls4B49D0), nullptr, 0, nullptr, 0, S14_P(WeretigerCopy_Tick)},
    {"WeretigerBurst_Run", 0x4B4A00, 0x82, kCalls4B4A00, MH_N(kCalls4B4A00), kImms4B4A00, MH_N(kImms4B4A00), nullptr, 0, S14_P(WeretigerBurst_Run)},
    {"WeretigerBurst_Start", 0x4B4A90, 0x21, kCalls4B4A90, MH_N(kCalls4B4A90), nullptr, 0, nullptr, 0, S14_P(WeretigerBurst_Start)},
    {"WeretigerBurst_Emit", 0x4B4AC0, 0x22, kCalls4B4AC0, MH_N(kCalls4B4AC0), nullptr, 0, nullptr, 0, S14_P(WeretigerBurst_Emit)},
    {"WeretigerBurst_Grow", 0x4B4AF0, 0x37, kCalls4B4AF0, MH_N(kCalls4B4AF0), nullptr, 0, nullptr, 0, S14_P(WeretigerBurst_Grow)},
    {"WeretigerBurst_WaitOwner", 0x4B4B30, 0x18, kCalls4B4B30, MH_N(kCalls4B4B30), nullptr, 0, nullptr, 0, S14_P(WeretigerBurst_WaitOwner)},
    {"WeretigerBurst_End", 0x4B4B50, 0x34, kCalls4B4B50, MH_N(kCalls4B4B50), nullptr, 0, nullptr, 0, S14_P(WeretigerBurst_End)},
    {"WeretigerStreak_Alloc6", 0x4B4B90, 0x3D, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(WeretigerStreak_Alloc6)},
    {"WeretigerStreak_Run", 0x4B4BD0, 0x26, nullptr, 0, kImms4B4BD0, MH_N(kImms4B4BD0), nullptr, 0, S14_P(WeretigerStreak_Run)},
    {"WeretigerStreak_Start", 0x4B4C00, 0xD1, kCalls4B4C00, MH_N(kCalls4B4C00), nullptr, 0, nullptr, 0, S14_P(WeretigerStreak_Start)},
    {"WeretigerStreak_Draw", 0x4B4CE0, 0x3B5, kCalls4B4CE0, MH_N(kCalls4B4CE0), nullptr, 0, nullptr, 0, S14_P(WeretigerStreak_Draw)},
    {"WeretigerStreak_DrawLine", 0x4B50A0, 0x7A, kCalls4B50A0, MH_N(kCalls4B50A0), nullptr, 0, nullptr, 0, S14_P(WeretigerStreak_DrawLine)},
    {"WeretigerImage_Run", 0x4B5120, 0x3E, kCalls4B5120, MH_N(kCalls4B5120), kImms4B5120, MH_N(kImms4B5120), nullptr, 0, S14_P(WeretigerImage_Run)},
    {"WeretigerImage_Play", 0x4B5160, 0x2C, kCalls4B5160, MH_N(kCalls4B5160), nullptr, 0, nullptr, 0, S14_P(WeretigerImage_Play)},
    {"WeretigerFx_Next", 0x4B5190, 0x9, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(WeretigerFx_Next)},
    {"WeretigerImage_End", 0x4B51A0, 0x13, kCalls4B51A0, MH_N(kCalls4B51A0), nullptr, 0, nullptr, 0, S14_P(WeretigerImage_End)},
    {"WeretigerDim_Run", 0x4B51C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(WeretigerDim_Run)},
    {"WeretigerDim_Down", 0x4B51E0, 0x31, kCalls4B51E0, MH_N(kCalls4B51E0), nullptr, 0, nullptr, 0, S14_P(WeretigerDim_Down)},
    {"WeretigerDim_WaitOwner", 0x4B5220, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(WeretigerDim_WaitOwner)},
    {"Weretiger_DrawSprite", 0x4B5240, 0x10F, kCalls4B5240, MH_N(kCalls4B5240), nullptr, 0, nullptr, 0, S14_P(Weretiger_DrawSprite)},
    {"Weretiger_ResetMapView", 0x4B5350, 0x158, kCalls4B5350, MH_N(kCalls4B5350), nullptr, 0, nullptr, 0, S14_P(Weretiger_ResetMapView)},
    {"Item_CopyName", 0x4B58F0, 0xA8, kCalls4B58F0, MH_N(kCalls4B58F0), nullptr, 0, nullptr, 0, S14_P(Item_CopyName), 0xFFFFFFFFu},
    {"Tsunami_Task", 0x4B59A0, 0x2E, nullptr, 0, kImms4B59A0, MH_N(kImms4B59A0), nullptr, 0, S14_P(Tsunami_Task)},
    {"Tsunami_Start", 0x4B59D0, 0xD9, kCalls4B59D0, MH_N(kCalls4B59D0), nullptr, 0, nullptr, 0, S14_P(Tsunami_Start)},
    {"Tsunami_Rings", 0x4B5AB0, 0x5E, kCalls4B5AB0, MH_N(kCalls4B5AB0), nullptr, 0, nullptr, 0, S14_P(Tsunami_Rings)},
    {"TsunamiChild_Task", 0x4B5B10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiChild_Task)},
    {"TsunamiWave_Run", 0x4B5B30, 0x2E, kCalls4B5B30, MH_N(kCalls4B5B30), nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Run)},
    {"TsunamiWave_Start", 0x4B5B60, 0x6E, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Start)},
    {"TsunamiWave_Advance", 0x4B5BD0, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Advance)},
    {"TsunamiWave_Grow", 0x4B5C00, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Grow)},
    {"TsunamiWave_Shrink", 0x4B5C30, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Shrink)},
    {"TsunamiWave_End", 0x4B5C70, 0x36, kCalls4B5C70, MH_N(kCalls4B5C70), nullptr, 0, nullptr, 0, S14_P(TsunamiWave_End)},
    {"TsunamiWave_Draw", 0x4B5CB0, 0x4AB, kCalls4B5CB0, MH_N(kCalls4B5CB0), nullptr, 0, nullptr, 0, S14_P(TsunamiWave_Draw)},
    {"TsunamiRings_Run", 0x4B6160, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiRings_Run)},
    {"TsunamiRings_Start", 0x4B6180, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, S14_P(TsunamiRings_Start)},
    {"TsunamiRings_Grow", 0x4B61A0, 0x21, kCalls4B61A0, MH_N(kCalls4B61A0), nullptr, 0, nullptr, 0, S14_P(TsunamiRings_Grow)},
    {"TsunamiRings_End", 0x4B61D0, 0x23, kCalls4B61D0, MH_N(kCalls4B61D0), nullptr, 0, nullptr, 0, S14_P(TsunamiRings_End)},
    {"TsunamiRings_Draw", 0x4B6200, 0x1B7, kCalls4B6200, MH_N(kCalls4B6200), nullptr, 0, nullptr, 0, S14_P(TsunamiRings_Draw)},
    {"TsunamiRing_DrawQuads", 0x4B63C0, 0x252, kCalls4B63C0, MH_N(kCalls4B63C0), nullptr, 0, nullptr, 0, S14_P(TsunamiRing_DrawQuads)},
    {"TsunamiRing_PushMatrix", 0x4B6620, 0xA1, kCalls4B6620, MH_N(kCalls4B6620), nullptr, 0, nullptr, 0, S14_P(TsunamiRing_PushMatrix)},
};
#undef MH_N
#undef S14_P

enum : unsigned {
    kWeretiger_Task, kWeretiger_Run, kWeretiger_Pose, kWeretiger_WaitPose, kWeretiger_FocusActor, kWeretiger_Burst, kWeretiger_Copy, kWeretiger_Release, kWeretiger_LoadForm, kWeretiger_FadeIn, kWeretiger_WaitScript, kWeretiger_Return, kWeretiger_End, kWeretigerChild_Task, kWeretigerCopy_Run, kWeretigerCopy_Step, kWeretigerCopy_Spawn, kWeretigerCopy_Tick, kWeretigerBurst_Run, kWeretigerBurst_Start, kWeretigerBurst_Emit, kWeretigerBurst_Grow, kWeretigerBurst_WaitOwner, kWeretigerBurst_End, kWeretigerStreak_Alloc6, kWeretigerStreak_Run, kWeretigerStreak_Start, kWeretigerStreak_Draw, kWeretigerStreak_DrawLine, kWeretigerImage_Run, kWeretigerImage_Play, kWeretigerFx_Next, kWeretigerImage_End, kWeretigerDim_Run, kWeretigerDim_Down, kWeretigerDim_WaitOwner, kWeretiger_DrawSprite, kWeretiger_ResetMapView, kItem_CopyName, kTsunami_Task, kTsunami_Start, kTsunami_Rings, kTsunamiChild_Task, kTsunamiWave_Run, kTsunamiWave_Start, kTsunamiWave_Advance, kTsunamiWave_Grow, kTsunamiWave_Shrink, kTsunamiWave_End, kTsunamiWave_Draw, kTsunamiRings_Run, kTsunamiRings_Start, kTsunamiRings_Grow, kTsunamiRings_End, kTsunamiRings_Draw, kTsunamiRing_DrawQuads, kTsunamiRing_PushMatrix, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kPool = 0x683288, kPoolCurrent = 0x684788,
                        kRingPhases = 0x68478C, kFrameCounter = 0x937F94;
constexpr unsigned kPoolCount = 0xC0, kPoolStride = 0x1C;
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kPool + (i % kPoolCount) * kPoolStride); }

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
// The sprite calls that act on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
// The GTE stand-ins of the matrix push: a result from the inputs, written
// where the real callee writes (group S22's, S31's).
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

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kP = mh::Answer::kPhase;
#define S14_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S14_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S14_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S14_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSprite},
    // the sprite and palette calls
    {S14_OURS(Sprite_EnsureAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S14_OURS(Sprite_LoadPalette), 2, {kAll, kAll}, kG, 0, 0, {}, &NoteSprite},
    {S14_OURS(Battle_StatusTint), 1, {kU16}, kG, 0, 0, {}, &NoteSprite},
    {S14_OURS(Sprite_SetClutStp), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S14_OURS(Gfx_ClearRect), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    // the files, the actors, the item names
    {S14_OURS(LoadDatFile), 1, {kAll}, kG, 0, 0},
    {S14_OURS(File_LoadDone), 0, {}, mh::Answer::kBool, 0, 0},
    {S14_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S14_OURS(Str_CopyN), 3, {kAll, kAll, kAll}, kG, 0, 0},
    // the field view
    {S14_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    {S14_OURS(MapView_PlaceRuns), 0, {}, kG, 0, 0},
    {S14_OURS(MapView_ShiftRowsNext), 0, {}, kG, 0, 0},
    {S14_OURS(MapView_ShiftRowsPrev), 0, {}, kG, 0, 0},
    {S14_OURS(MapView_CellToMap), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S14_OURS(AreaMap_BakePatches), 0, {}, kG, 0, 0},
    {S14_OURS(AreaMap_SetupEntries), 0, {}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items: all ours)
    {S14_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S14_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S14_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S14_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S14_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S14_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S14_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gpu_SetSprt), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S14_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S14_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S14_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S14_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S14_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S14_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S14_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S14_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // this group's own, called directly (the streak run logs the pool's
    // current record it was called for)
    {S14_RAW(0x4B4B90), 0, {}, kP, 0, 0},
    {S14_RAW(0x4B4BD0), 0, {kPoolCurrent}, kP, 0, 0},
    {S14_RAW(0x4B50A0), 4, {kU16, kU16, kU16, kU16}, kG, 0, 0},
    {S14_RAW(0x4B5240), 3, {kU8, kU8, kU8}, kG, 0, 0},
    {S14_RAW(0x4B5350), 0, {}, kP, 0, 0},
    {S14_RAW(0x4B5CB0), 0, {}, kP, 0, 0},
    {S14_RAW(0x4B6200), 0, {}, kP, 0, 0},
    {S14_RAW(0x4B63C0), 0, {}, kP, 0, 0},
    {S14_RAW(0x4B6620), 0, {}, kP, 0, 0},
};
#undef S14_OURS
#undef S14_RAW

// The four .data handler tables the dispatchers read in place
// (WeretigerDim_Steps and the rest, symbols.toml).
const mh::DataTable kTables[] = {{0x65AC00, 4}, {0x65AC2C, 2}, {0x65AC34, 5}, {0x65AC48, 3}};

mh::Region g_regions[] = {
    {0x7E0670, 0x1C},                     // Gfx_PacketNext .. MapView_Origin
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850..
    {0x905E60, 0x10},                     // Field_Kind2Z / X, MapView_Redraw
    {0x905EB0, 0x100},                    // the head of the entries Weretiger_ResetMapView clears
    {0x929E00, 0x130},                    // their tail; MapView_Focus*, _Elevation, _Column, _Row
    {kPool, 0x1508},                      // the streak pool, its current record, the ring phases
    {0x80B980, 0x20},                     // Gfx_ClutStripSource row 2's first 16 words
    {0x80F980, 0x20},                     // Gfx_ClutStrip the same
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
    {0x80D4C0, 0x40},                     // Gfx_ClutStripSource words 0xFA0..0xFBF
    {0x8114C0, 0x40},                     // Gfx_ClutStrip the same
    {0x8CB59C, 4},                        // AreaMap_Word1E
    {0x92BEE0, 4},                        // MapView_ScrollX, MapView_ElevationOffset
    {0x904EFC, 4},                        // MoveScript_FAWord
    {0x905BA0, 4},                        // DrawTable_Count
    {0x937FA0, 0x1880},                   // MapView_CellItems
    {0x7E09E0, 0x800},                    // DrawItemPool_Free
    {0x9039D4, 4},                        // DrawItemPool_Top
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(mh::at::kOwner); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 9) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: mh::Mem(kScratch + v % 16)[0] = Byte(h >> 24); break;
    case 3: SetLong(mh::Mem(0x905E60 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: mh::SetPointer(kPoolCurrent, PoolRecord(v)); break;
    case 5: PoolRecord(v)[(h >> 24) % kPoolStride] = Byte(h >> 16); break;
    case 6: mh::Mem(kRingPhases + v % 4)[0] = Byte(h >> 24); break;
    case 7: {
        static const unsigned kFields[] = {0x10, 0x14, 0x34, 0x38, 0x3E};
        SetWord(Sc() + kFields[v % 5], h >> 16);
        break;
    }
    case 8: MapView_ElevationOffset = static_cast<unsigned short>(static_cast<int>((h >> 16) % 0xC00) - 0x600); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void NearLong(unsigned char* at, std::uint32_t before) {
    if (mh::Half()) SetLong(at, static_cast<std::int32_t>(before + (mh::Half() ? 1u : 0u)));
}
// The acting actor's party record, as the originals index it.
unsigned char* ActorRecord() { return mh::PartyOf(mh::Mem(mh::at::kActor)[0]); }
// Every streak record's +0 cleared with one chance in `one_in` (0: all).
void FreePool(unsigned one_in) {
    for (unsigned i = 0; i < kPoolCount; ++i)
        if (one_in == 0 || mh::Next() % one_in == 0) PoolRecord(i)[0] = 0;
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kPoolCurrent, PoolRecord(mh::Next()));
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kWeretiger_Task: case kTsunami_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kWeretiger_Run: sc[2] = Byte(mh::Next() % 12); break;
    case kWeretigerChild_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kTsunamiChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kWeretigerCopy_Run: case kWeretigerImage_Run: case kTsunamiRings_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kWeretigerCopy_Step: sc[3] = Byte(mh::Next() % 2); break;
    case kWeretigerDim_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kWeretigerStreak_Run: mh::Pointer(kPoolCurrent)[1] = Byte(mh::Next() % 2); break;
    case kWeretigerBurst_Run:
        sc[2] = Byte(mh::Next() % 5);
        FreePool(MH_PICK(2, 8, 64));
        break;
    case kTsunamiWave_Run:
        sc[2] = Byte(mh::Next() % 5);
        if (mh::Half()) Owner()[0xB] = 0xFF;
        break;
    // the counters: at their thresholds
    case kWeretiger_FocusActor: case kWeretiger_Release: Near(sc[9], 0); break;
    case kWeretiger_Burst: case kWeretiger_End:
        if (mh::Half()) sc[0xA] = 0;
        Near(sc[9], 0);
        break;
    case kWeretigerBurst_Emit: case kTsunami_Rings: case kTsunamiWave_Advance: Near(sc[9], 1); break;
    case kWeretigerBurst_Grow:
        if (mh::Often()) mh::Mem(kFrameCounter)[0] &= 0xFE;
        Near(sc[0xB], 0x17);
        break;
    case kWeretigerBurst_WaitOwner: case kWeretigerDim_WaitOwner:
        if (mh::Half()) Owner()[2] = 0xA;
        break;
    case kWeretigerBurst_End:
        FreePool(0);
        if (mh::Half()) PoolRecord(mh::Next())[0] = Byte(1 + mh::Next() % 0xFF);
        break;
    case kWeretigerStreak_Alloc6: FreePool(MH_PICK(1, 2, 8, 32, 64)); break;
    case kWeretigerDim_Down: Near(sc[9], 0xFA); break;
    case kWeretiger_Return: if (mh::Half()) sc[0xB] &= 0xFE; break;
    case kWeretiger_LoadForm: case kWeretiger_FadeIn: {
        unsigned char* const owner = Owner();
        if (mh::Half()) SetWord(owner + 0x2C, 0);
        if (mh::Often()) owner[8] = Byte(mh::Next() % 4);
        if (k == kWeretiger_FadeIn && mh::Half()) sc[9] = Byte(mh::Half() ? 0x80 : 0x7E);
        break;
    }
    case kWeretigerStreak_Start: if (mh::Often()) ActorRecord()[8] = Byte(mh::Next() % 4); break;
    case kWeretigerStreak_Draw: {
        // the second radius's whole part either side of 0x1C0 after its growth
        unsigned char* const r = mh::Pointer(kPoolCurrent);
        if (mh::Often()) {
            const std::uint32_t grow = mh::Next() % 0x30000;
            SetLong(r + 0x18, static_cast<std::int32_t>(grow));
            SetLong(r + 0x10, static_cast<std::int32_t>(0x1C00000u - grow + MH_PICK(0xFFFF0000u, 0, 0xFFFF, 0x10000)));
        }
        break;
    }
    case kWeretiger_ResetMapView:
        SetWord(mh::Mem(0x8CB59E), MH_PICK(0, 0, 0x1FF, 0x200, 0x201, 0xFE00, 0xFDFF, 0xFE01, 0x600, 0xFA00));
        if (mh::Often()) MapView_Elevation = static_cast<long>(MH_PICK(0, 0xFFFFFF01u, 0xFFFFFF00u, 0xFFFFFEFFu, 0xFF, 0x100, 0x101, 0x300));
        break;
    case kTsunamiWave_Grow: NearLong(sc + 0x10, 0x7E); break;
    case kTsunamiWave_Shrink: NearLong(sc + 0x10, 0x22); break;
    case kTsunamiWave_End: NearLong(sc + 0x10, 1); break;
    case kTsunamiWave_Draw: if (mh::Half()) sc[2] = 4; break;
    case kTsunamiRings_Grow: Near(sc[9], 0x5F); break;
    case kTsunamiRings_End: Near(sc[9], 0x7F); break;
    case kTsunamiRings_Draw:
        if (mh::Often()) sc[9] = Byte(mh::Next() % 16);
        if (mh::Half()) sc[2] = 1;
        break;
    case kTsunamiRing_DrawQuads:
        if (mh::Often()) sc[9] = Byte(mh::Next() % 16);
        for (unsigned j = 0; j < 4; ++j)
            if (mh::Often()) mh::Mem(kRingPhases + j)[0] = Byte(mh::Next() % 0x14);
        break;
    default: break;
    }
}

// The functions that take arguments: Weretiger_DrawSprite's image mostly
// 0..3; Item_CopyName's category mostly 0..4.
void Args(unsigned k, std::uint32_t* a) {
    if (k == kWeretiger_DrawSprite && mh::Often()) a[0] = (a[0] & ~0xFFu) | (mh::Next() % 4);
    if (k == kItem_CopyName && mh::Often()) a[1] = (a[1] & ~0xFFu) | (mh::Next() % 5);
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s14", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s14
