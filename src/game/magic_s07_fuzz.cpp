// BOF3X_SHADOW=magic_s07: group S07's four overlays (MAGIC021, 038, 039, 040)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s07.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC021 / 038 / 039 / 040
// --clones (2026-09-26; capstone, every jump internal, no jump table, no
// REFUSED line), names given. Beyond the standard set this group lists the
// draw callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), the sprite calls, Sprite_SetTint,
// Battle_ActorIsOut, the other units' functions called by address and its own
// functions called directly. What the harness lacks is built here, not in
// the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log the whole packet
//     buffer (every primitive of a draw is built in it; group C1's) and move
//     Gfx_PacketNext on as the real ones do;
//   - the sprite calls that act on Sprite_Current (the two script ticks, the
//     animation, the screen update) log which sprite, the screen update the
//     frame-offset table 0x9039D8 too (WarShoutMote_Run swaps it round its
//     step);
//   - the kFlag callees whose answer a caller branches on (the script ticks,
//     Battle_ActorIsOut) answer from their own stream, not from the hash
//     that chose their disturbance - the round's kFlag blind spot
//     (docs/takeover-queue-round9.md section 9; group E's workaround);
//   - WarShoutMote_Run's five steps are listed before their .data table
//     registers them, so each call logs the frame-offset table as well.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s07.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s07 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

#define S07_P(name) reinterpret_cast<const void*>(&::name)

// tools/magic_rows.py --unit MAGIC021 / 038 / 039 / 040 --clones, 2026-09-26,
// names given.
// 0x4A3B20: 0x7D bytes  Bonebreak_Task
constexpr mh::CallSite kCalls4A3B20[] = {{0x5B, 0x4A3E10}};
constexpr mh::Imm kImms4A3B20[] = {{0x16, 0x4A3BA0}, {0x1E, 0x4A13B0}, {0x26, 0x43FE80}};
// 0x4A3BA0: 0xDA bytes  Bonebreak_Start
constexpr mh::CallSite kCalls4A3BA0[] = {{0x32, 0x4FB830}, {0x3B, 0x435180}};
// 0x4A3C80: 0x12 bytes; +0xB note: jmp through .data 0x65a6e8 (a data_tables entry)  BonebreakChild_Task
// 0x4A3CA0: 0x56 bytes  BonebreakChild_Run
constexpr mh::CallSite kCalls4A3CA0[] = {{0x4D, 0x588F20}};
constexpr mh::Imm kImms4A3CA0[] = {{0xF, 0x4ED5C0}, {0x17, 0x4A3D00}, {0x22, 0x4A3DA0}, {0x2A, 0x4A3DC0}, {0x32, 0x4A3DE0}, {0x3A, 0x4AEE90}};
// 0x4A3D00: 0x93 bytes  BonebreakChild_Burst
constexpr mh::CallSite kCalls4A3D00[] = {{0x2, 0x589410}, {0x26, 0x587900}, {0x40, 0x4A43E0}};
// 0x4A3DA0: 0x20 bytes  BonebreakChild_WaitScript
constexpr mh::CallSite kCalls4A3DA0[] = {{0x0, 0x589410}, {0xF, 0x4530D0}};
// 0x4A3DC0: 0x1E bytes  BonebreakChild_Shade
constexpr mh::CallSite kCalls4A3DC0[] = {{0xB, 0x4A29C0}};
// 0x4A3DE0: 0x22 bytes  BonebreakChild_WaitMotes
// 0x4A3E10: 0x12 bytes; +0xB note: jmp through .data 0x65a6ec (a data_tables entry)  BonebreakMote_Task
// 0x4A3E30: 0x33 bytes; +0xB note: call through .data 0x65a6f0 (a data_tables entry)  BonebreakMote_Run
constexpr mh::CallSite kCalls4A3E30[] = {{0x23, 0x4FBD10}, {0x28, 0x4A3F90}, {0x2D, 0x4A4160}};
// 0x4A3E70: 0xEA bytes  BonebreakMote_Launch
constexpr mh::CallSite kCalls4A3E70[] = {{0x1E, 0x5B93D2}, {0x6B, 0x5B93D2}, {0x89, 0x5A7A00}, {0xB2, 0x5A7A50}};
// 0x4A3F60: 0x28 bytes  BonebreakMote_Fade
constexpr mh::CallSite kCalls4A3F60[] = {{0x22, 0x4F6290}};
// 0x4A3F90: 0x1D0 bytes  BonebreakMote_DrawFan
constexpr mh::CallSite kCalls4A3F90[] = {{0x14, 0x5A77C0}, {0x2A, 0x572FA0}, {0x8E, 0x5A75F0}, {0x96, 0x5A7780}, {0xC7, 0x5A7A00}, {0xF5, 0x5A7A50}, {0x12B, 0x5A7A00}, {0x159, 0x5A7A50}, {0x1B7, 0x572FA0}};
// 0x4A4160: 0x277 bytes  BonebreakMote_DrawRing
constexpr mh::CallSite kCalls4A4160[] = {{0x18, 0x5A77C0}, {0x2E, 0x572FA0}, {0x99, 0x5A7610}, {0xA0, 0x5A7780}, {0xAD, 0x5A7A00}, {0xDB, 0x5A7A50}, {0x109, 0x5A7A00}, {0x137, 0x5A7A50}, {0x16D, 0x5A7A00}, {0x19B, 0x5A7A50}, {0x1C9, 0x5A7A00}, {0x1F7, 0x5A7A50}, {0x25E, 0x572FA0}};
// 0x4A43E0: 0x57 bytes  BonebreakMote_Alloc
// 0x4A4440: 0x2E bytes  WarShout_Task
constexpr mh::Imm kImms4A4440[] = {{0xF, 0x4A4470}, {0x17, 0x4A4570}, {0x22, 0x4E5200}};
// 0x4A4470: 0xF2 bytes  WarShout_Start
constexpr mh::CallSite kCalls4A4470[] = {{0x2, 0x4FC0E0}, {0x3E, 0x435180}, {0xEB, 0x587900}};
// 0x4A4570: 0x172 bytes  WarShout_Rally
constexpr mh::CallSite kCalls4A4570[] = {{0x1A, 0x587900}, {0x3C, 0x4456C0}, {0x4C, 0x435180}, {0xDD, 0x4456C0}, {0xED, 0x435180}};
// 0x4A46F0: 0x12 bytes; +0xB note: jmp through .data 0x65a6fc (a data_tables entry)  WarShoutChild_Task
// 0x4A4710: 0x3D bytes; +0x15 note: call through .data 0x65a704 (a data_tables entry)  WarShoutMote_Run
constexpr mh::CallSite kCalls4A4710[] = {{0x2D, 0x588F20}};
// 0x4A4750: 0x15F bytes  WarShoutMote_Appear
constexpr mh::CallSite kCalls4A4750[] = {{0x3E, 0x5A7A00}, {0x64, 0x5A7A50}, {0x97, 0x5A7A00}, {0x14D, 0x5891F0}};
// 0x4A48B0: 0x120 bytes  WarShoutMote_Rise
constexpr mh::CallSite kCalls4A48B0[] = {{0x0, 0x5893A0}, {0x4E, 0x5A7A00}, {0x73, 0x5A7A50}, {0xBC, 0x5A7A00}};
// 0x4A49D0: 0xFA bytes  WarShoutMote_Circle
constexpr mh::CallSite kCalls4A49D0[] = {{0x0, 0x5893A0}, {0x25, 0x5A7A00}, {0x4A, 0x5A7A50}, {0x93, 0x5A7A00}};
// 0x4A4AD0: 0xEC bytes  WarShoutMote_Fade
constexpr mh::CallSite kCalls4A4AD0[] = {{0x24, 0x5893A0}, {0x49, 0x5A7A00}, {0x6E, 0x5A7A50}, {0xB7, 0x5A7A00}};
// 0x4A4BC0: 0x12 bytes; +0xB note: jmp through .data 0x65a728 (a data_tables entry)  WarShoutBuff_Run
// 0x4A4BE0: 0x27 bytes  WarShoutBuff_Start
constexpr mh::CallSite kCalls4A4BE0[] = {{0xD, 0x4FB790}};
// 0x4A4C10: 0x2E bytes  Focus_Task
constexpr mh::Imm kImms4A4C10[] = {{0xF, 0x4A4C40}, {0x17, 0x49DA50}, {0x22, 0x4F7350}};
// 0x4A4C40: 0x146 bytes  Focus_Start
constexpr mh::CallSite kCalls4A4C40[] = {{0x36, 0x4A4D90}, {0x5B, 0x435180}, {0xAA, 0x435180}, {0x13D, 0x587900}};
// 0x4A4D90: 0x10 bytes  Focus_Kind
// 0x4A4DA0: 0x12 bytes; +0xB note: jmp through .data 0x65a750 (a data_tables entry)  FocusChild_Task
// 0x4A4DC0: 0x6B bytes  FocusAura_Run
constexpr mh::CallSite kCalls4A4DC0[] = {{0x53, 0x4B7D40}, {0x58, 0x4A53A0}, {0x5D, 0x4A57C0}, {0x62, 0x5A7BC0}};
constexpr mh::Imm kImms4A4DC0[] = {{0xF, 0x4A4E30}, {0x17, 0x4A4E80}, {0x22, 0x4A4EB0}, {0x2A, 0x4A4F10}, {0x32, 0x4A4F90}, {0x3A, 0x4A5010}};
// 0x4A4E30: 0x4F bytes  FocusAura_Start
// 0x4A4E80: 0x26 bytes  FocusAura_Grow
// 0x4A4EB0: 0x5B bytes  FocusAura_Swell
constexpr mh::CallSite kCalls4A4EB0[] = {{0x2D, 0x454DC0}, {0x41, 0x454CC0}};
// 0x4A4F10: 0x74 bytes  FocusAura_Brighten
// 0x4A4F90: 0x75 bytes  FocusAura_Dim
// 0x4A5010: 0x50 bytes  FocusAura_End
constexpr mh::CallSite kCalls4A5010[] = {{0x2E, 0x454DC0}, {0x3A, 0x4FBDB0}, {0x4A, 0x4351F0}};
// 0x4A5060: 0x49 bytes  FocusMote_Run
constexpr mh::CallSite kCalls4A5060[] = {{0x3B, 0x4FBD10}, {0x40, 0x4A51C0}};
constexpr mh::Imm kImms4A5060[] = {{0xF, 0x4A50B0}, {0x17, 0x4A5160}, {0x22, 0x4A5180}};
// 0x4A50B0: 0xAA bytes  FocusMote_Start
constexpr mh::CallSite kCalls4A50B0[] = {{0x2F, 0x5A7A50}, {0x58, 0x5A7A00}};
// 0x4A5160: 0x1D bytes  FocusMote_Grow
// 0x4A5180: 0x35 bytes  FocusMote_Rise
constexpr mh::CallSite kCalls4A5180[] = {{0x2F, 0x4351F0}};
// 0x4A51C0: 0x1E0 bytes  FocusMote_Draw
constexpr mh::CallSite kCalls4A51C0[] = {{0x89, 0x5A77C0}, {0x9F, 0x572FA0}, {0xAB, 0x5A7610}, {0xB2, 0x5A7780}, {0x1D4, 0x572FA0}};
// 0x4A53A0: 0x416 bytes  FocusAura_DrawRing
constexpr mh::CallSite kCalls4A53A0[] = {{0x14, 0x5B93D2}, {0x37, 0x5A7A00}, {0x71, 0x5A7A00}, {0x14E, 0x5A7A50}, {0x167, 0x5A7A00}, {0x18C, 0x5A7A50}, {0x1A5, 0x5A7A00}, {0x1DB, 0x5B93D2}, {0x204, 0x5A7A00}, {0x235, 0x5A7A00}, {0x289, 0x5A7A50}, {0x2A2, 0x5A7A00}, {0x2E3, 0x5A7A50}, {0x2FC, 0x5A7A00}, {0x33C, 0x5A77C0}, {0x347, 0x572FA0}, {0x353, 0x5A7610}, {0x35E, 0x5A7780}, {0x3DD, 0x5A85F0}, {0x3E3, 0x5A9350}, {0x3EE, 0x572FA0}};
// 0x4A57C0: 0x247 bytes  FocusAura_DrawDisc
constexpr mh::CallSite kCalls4A57C0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0xDC, 0x5A7A00}, {0xF5, 0x5A7A50}, {0x131, 0x5A75F0}, {0x138, 0x5A7780}, {0x166, 0x5A7A00}, {0x17F, 0x5A7A50}, {0x1BC, 0x5A84A0}, {0x1C2, 0x5A9310}, {0x208, 0x461E50}, {0x22E, 0x5A77C0}, {0x237, 0x461E50}};
// 0x4A5A10: 0x2E bytes  Enlighten_Task
constexpr mh::Imm kImms4A5A10[] = {{0xF, 0x4A5A40}, {0x17, 0x4A5B50}, {0x22, 0x4E5200}};
// 0x4A5A40: 0x106 bytes  Enlighten_Start
constexpr mh::CallSite kCalls4A5A40[] = {{0x5A, 0x435180}, {0x91, 0x435180}, {0xFD, 0x587900}};
// 0x4A5B50: 0xB2 bytes  Enlighten_Apply
constexpr mh::CallSite kCalls4A5B50[] = {{0x1A, 0x4FB6F0}, {0x2A, 0x435180}, {0x5F, 0x435180}};
// 0x4A5C10: 0x12 bytes; +0xB note: jmp through .data 0x65a758 (a data_tables entry)  EnlightenChild_Task
// 0x4A5C30: 0x80 bytes  EnlightenRays_Run
constexpr mh::CallSite kCalls4A5C30[] = {{0x43, 0x4A6120}, {0x5B, 0x4A5FC0}, {0x74, 0x4A5FC0}};
constexpr mh::Imm kImms4A5C30[] = {{0xF, 0x4A5CB0}, {0x17, 0x4A5D20}, {0x22, 0x4C4B70}, {0x2A, 0x4A5D50}};
// 0x4A5CB0: 0x66 bytes  EnlightenRays_Start
constexpr mh::CallSite kCalls4A5CB0[] = {{0x44, 0x4A5E90}};
// 0x4A5D20: 0x29 bytes  EnlightenRays_Grow
// 0x4A5D50: 0x34 bytes  EnlightenRays_Fade
constexpr mh::CallSite kCalls4A5D50[] = {{0x2E, 0x4351F0}};
// 0x4A5D90: 0x3C bytes  EnlightenRing_Run
constexpr mh::CallSite kCalls4A5D90[] = {{0x33, 0x4A62B0}};
constexpr mh::Imm kImms4A5D90[] = {{0xF, 0x4A5DD0}, {0x17, 0x4A5E40}};
// 0x4A5DD0: 0x6F bytes  EnlightenRing_Start
constexpr mh::CallSite kCalls4A5DD0[] = {{0x44, 0x4A5E90}};
// 0x4A5E40: 0x46 bytes  EnlightenRing_Spread
constexpr mh::CallSite kCalls4A5E40[] = {{0x40, 0x4351F0}};
// 0x4A5E90: 0x12E bytes  Enlighten_AnchorToActor
constexpr mh::CallSite kCalls4A5E90[] = {{0x1, 0x4FBD10}};
// 0x4A5FC0: 0x159 bytes  EnlightenRays_DrawLines
constexpr mh::CallSite kCalls4A5FC0[] = {{0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x65, 0x5A76B0}, {0x6D, 0x5A7780}, {0xB2, 0x5A7A50}, {0xD9, 0x5A7A00}, {0x114, 0x461E50}, {0x145, 0x5A77C0}, {0x14E, 0x461E50}};
// 0x4A6120: 0x181 bytes  EnlightenRays_DrawDisc
constexpr mh::CallSite kCalls4A6120[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0x56, 0x5A75F0}, {0x5D, 0x5A7780}, {0x87, 0x5A7A00}, {0xAE, 0x5A7A50}, {0xDB, 0x5A7A00}, {0x102, 0x5A7A50}, {0x14A, 0x461E50}, {0x16B, 0x5A77C0}, {0x174, 0x461E50}};
// 0x4A62B0: 0x185 bytes  EnlightenRing_Draw
constexpr mh::CallSite kCalls4A62B0[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x5E, 0x5A7650}, {0x66, 0x5A7780}, {0x7A, 0x5A7A00}, {0xA8, 0x5A7A50}, {0xDE, 0x5A7A00}, {0x10C, 0x5A7A50}, {0x150, 0x461E50}, {0x170, 0x5A77C0}, {0x179, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Bonebreak_Task", 0x4A3B20, 0x7D, kCalls4A3B20, MH_N(kCalls4A3B20), kImms4A3B20, MH_N(kImms4A3B20), nullptr, 0, S07_P(Bonebreak_Task)},
    {"Bonebreak_Start", 0x4A3BA0, 0xDA, kCalls4A3BA0, MH_N(kCalls4A3BA0), nullptr, 0, nullptr, 0, S07_P(Bonebreak_Start)},
    {"BonebreakChild_Task", 0x4A3C80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(BonebreakChild_Task)},
    {"BonebreakChild_Run", 0x4A3CA0, 0x56, kCalls4A3CA0, MH_N(kCalls4A3CA0), kImms4A3CA0, MH_N(kImms4A3CA0), nullptr, 0, S07_P(BonebreakChild_Run)},
    {"BonebreakChild_Burst", 0x4A3D00, 0x93, kCalls4A3D00, MH_N(kCalls4A3D00), nullptr, 0, nullptr, 0, S07_P(BonebreakChild_Burst)},
    {"BonebreakChild_WaitScript", 0x4A3DA0, 0x20, kCalls4A3DA0, MH_N(kCalls4A3DA0), nullptr, 0, nullptr, 0, S07_P(BonebreakChild_WaitScript)},
    {"BonebreakChild_Shade", 0x4A3DC0, 0x1E, kCalls4A3DC0, MH_N(kCalls4A3DC0), nullptr, 0, nullptr, 0, S07_P(BonebreakChild_Shade)},
    {"BonebreakChild_WaitMotes", 0x4A3DE0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(BonebreakChild_WaitMotes)},
    {"BonebreakMote_Task", 0x4A3E10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(BonebreakMote_Task)},
    {"BonebreakMote_Run", 0x4A3E30, 0x33, kCalls4A3E30, MH_N(kCalls4A3E30), nullptr, 0, nullptr, 0, S07_P(BonebreakMote_Run)},
    {"BonebreakMote_Launch", 0x4A3E70, 0xEA, kCalls4A3E70, MH_N(kCalls4A3E70), nullptr, 0, nullptr, 0, S07_P(BonebreakMote_Launch)},
    {"BonebreakMote_Fade", 0x4A3F60, 0x28, kCalls4A3F60, MH_N(kCalls4A3F60), nullptr, 0, nullptr, 0, S07_P(BonebreakMote_Fade)},
    {"BonebreakMote_DrawFan", 0x4A3F90, 0x1D0, kCalls4A3F90, MH_N(kCalls4A3F90), nullptr, 0, nullptr, 0, S07_P(BonebreakMote_DrawFan)},
    {"BonebreakMote_DrawRing", 0x4A4160, 0x277, kCalls4A4160, MH_N(kCalls4A4160), nullptr, 0, nullptr, 0, S07_P(BonebreakMote_DrawRing)},
    {"BonebreakMote_Alloc", 0x4A43E0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(BonebreakMote_Alloc), 0xFF},
    {"WarShout_Task", 0x4A4440, 0x2E, nullptr, 0, kImms4A4440, MH_N(kImms4A4440), nullptr, 0, S07_P(WarShout_Task)},
    {"WarShout_Start", 0x4A4470, 0xF2, kCalls4A4470, MH_N(kCalls4A4470), nullptr, 0, nullptr, 0, S07_P(WarShout_Start)},
    {"WarShout_Rally", 0x4A4570, 0x172, kCalls4A4570, MH_N(kCalls4A4570), nullptr, 0, nullptr, 0, S07_P(WarShout_Rally)},
    {"WarShoutChild_Task", 0x4A46F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(WarShoutChild_Task)},
    {"WarShoutMote_Run", 0x4A4710, 0x3D, kCalls4A4710, MH_N(kCalls4A4710), nullptr, 0, nullptr, 0, S07_P(WarShoutMote_Run)},
    {"WarShoutMote_Appear", 0x4A4750, 0x15F, kCalls4A4750, MH_N(kCalls4A4750), nullptr, 0, nullptr, 0, S07_P(WarShoutMote_Appear)},
    {"WarShoutMote_Rise", 0x4A48B0, 0x120, kCalls4A48B0, MH_N(kCalls4A48B0), nullptr, 0, nullptr, 0, S07_P(WarShoutMote_Rise)},
    {"WarShoutMote_Circle", 0x4A49D0, 0xFA, kCalls4A49D0, MH_N(kCalls4A49D0), nullptr, 0, nullptr, 0, S07_P(WarShoutMote_Circle)},
    {"WarShoutMote_Fade", 0x4A4AD0, 0xEC, kCalls4A4AD0, MH_N(kCalls4A4AD0), nullptr, 0, nullptr, 0, S07_P(WarShoutMote_Fade)},
    {"WarShoutBuff_Run", 0x4A4BC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(WarShoutBuff_Run)},
    {"WarShoutBuff_Start", 0x4A4BE0, 0x27, kCalls4A4BE0, MH_N(kCalls4A4BE0), nullptr, 0, nullptr, 0, S07_P(WarShoutBuff_Start)},
    {"Focus_Task", 0x4A4C10, 0x2E, nullptr, 0, kImms4A4C10, MH_N(kImms4A4C10), nullptr, 0, S07_P(Focus_Task)},
    {"Focus_Start", 0x4A4C40, 0x146, kCalls4A4C40, MH_N(kCalls4A4C40), nullptr, 0, nullptr, 0, S07_P(Focus_Start)},
    {"Focus_Kind", 0x4A4D90, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(Focus_Kind), 0xFF},
    {"FocusChild_Task", 0x4A4DA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusChild_Task)},
    {"FocusAura_Run", 0x4A4DC0, 0x6B, kCalls4A4DC0, MH_N(kCalls4A4DC0), kImms4A4DC0, MH_N(kImms4A4DC0), nullptr, 0, S07_P(FocusAura_Run)},
    {"FocusAura_Start", 0x4A4E30, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusAura_Start)},
    {"FocusAura_Grow", 0x4A4E80, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusAura_Grow)},
    {"FocusAura_Swell", 0x4A4EB0, 0x5B, kCalls4A4EB0, MH_N(kCalls4A4EB0), nullptr, 0, nullptr, 0, S07_P(FocusAura_Swell)},
    {"FocusAura_Brighten", 0x4A4F10, 0x74, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusAura_Brighten)},
    {"FocusAura_Dim", 0x4A4F90, 0x75, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusAura_Dim)},
    {"FocusAura_End", 0x4A5010, 0x50, kCalls4A5010, MH_N(kCalls4A5010), nullptr, 0, nullptr, 0, S07_P(FocusAura_End)},
    {"FocusMote_Run", 0x4A5060, 0x49, kCalls4A5060, MH_N(kCalls4A5060), kImms4A5060, MH_N(kImms4A5060), nullptr, 0, S07_P(FocusMote_Run)},
    {"FocusMote_Start", 0x4A50B0, 0xAA, kCalls4A50B0, MH_N(kCalls4A50B0), nullptr, 0, nullptr, 0, S07_P(FocusMote_Start)},
    {"FocusMote_Grow", 0x4A5160, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(FocusMote_Grow)},
    {"FocusMote_Rise", 0x4A5180, 0x35, kCalls4A5180, MH_N(kCalls4A5180), nullptr, 0, nullptr, 0, S07_P(FocusMote_Rise)},
    {"FocusMote_Draw", 0x4A51C0, 0x1E0, kCalls4A51C0, MH_N(kCalls4A51C0), nullptr, 0, nullptr, 0, S07_P(FocusMote_Draw)},
    {"FocusAura_DrawRing", 0x4A53A0, 0x416, kCalls4A53A0, MH_N(kCalls4A53A0), nullptr, 0, nullptr, 0, S07_P(FocusAura_DrawRing)},
    {"FocusAura_DrawDisc", 0x4A57C0, 0x247, kCalls4A57C0, MH_N(kCalls4A57C0), nullptr, 0, nullptr, 0, S07_P(FocusAura_DrawDisc)},
    {"Enlighten_Task", 0x4A5A10, 0x2E, nullptr, 0, kImms4A5A10, MH_N(kImms4A5A10), nullptr, 0, S07_P(Enlighten_Task)},
    {"Enlighten_Start", 0x4A5A40, 0x106, kCalls4A5A40, MH_N(kCalls4A5A40), nullptr, 0, nullptr, 0, S07_P(Enlighten_Start)},
    {"Enlighten_Apply", 0x4A5B50, 0xB2, kCalls4A5B50, MH_N(kCalls4A5B50), nullptr, 0, nullptr, 0, S07_P(Enlighten_Apply)},
    {"EnlightenChild_Task", 0x4A5C10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(EnlightenChild_Task)},
    {"EnlightenRays_Run", 0x4A5C30, 0x80, kCalls4A5C30, MH_N(kCalls4A5C30), kImms4A5C30, MH_N(kImms4A5C30), nullptr, 0, S07_P(EnlightenRays_Run)},
    {"EnlightenRays_Start", 0x4A5CB0, 0x66, kCalls4A5CB0, MH_N(kCalls4A5CB0), nullptr, 0, nullptr, 0, S07_P(EnlightenRays_Start)},
    {"EnlightenRays_Grow", 0x4A5D20, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, S07_P(EnlightenRays_Grow)},
    {"EnlightenRays_Fade", 0x4A5D50, 0x34, kCalls4A5D50, MH_N(kCalls4A5D50), nullptr, 0, nullptr, 0, S07_P(EnlightenRays_Fade)},
    {"EnlightenRing_Run", 0x4A5D90, 0x3C, kCalls4A5D90, MH_N(kCalls4A5D90), kImms4A5D90, MH_N(kImms4A5D90), nullptr, 0, S07_P(EnlightenRing_Run)},
    {"EnlightenRing_Start", 0x4A5DD0, 0x6F, kCalls4A5DD0, MH_N(kCalls4A5DD0), nullptr, 0, nullptr, 0, S07_P(EnlightenRing_Start)},
    {"EnlightenRing_Spread", 0x4A5E40, 0x46, kCalls4A5E40, MH_N(kCalls4A5E40), nullptr, 0, nullptr, 0, S07_P(EnlightenRing_Spread)},
    {"Enlighten_AnchorToActor", 0x4A5E90, 0x12E, kCalls4A5E90, MH_N(kCalls4A5E90), nullptr, 0, nullptr, 0, S07_P(Enlighten_AnchorToActor)},
    {"EnlightenRays_DrawLines", 0x4A5FC0, 0x159, kCalls4A5FC0, MH_N(kCalls4A5FC0), nullptr, 0, nullptr, 0, S07_P(EnlightenRays_DrawLines)},
    {"EnlightenRays_DrawDisc", 0x4A6120, 0x181, kCalls4A6120, MH_N(kCalls4A6120), nullptr, 0, nullptr, 0, S07_P(EnlightenRays_DrawDisc)},
    {"EnlightenRing_Draw", 0x4A62B0, 0x185, kCalls4A62B0, MH_N(kCalls4A62B0), nullptr, 0, nullptr, 0, S07_P(EnlightenRing_Draw)},
};
#undef MH_N

enum : unsigned {
    kBonebreak_Task, kBonebreak_Start, kBonebreakChild_Task, kBonebreakChild_Run, kBonebreakChild_Burst,
    kBonebreakChild_WaitScript, kBonebreakChild_Shade, kBonebreakChild_WaitMotes, kBonebreakMote_Task,
    kBonebreakMote_Run, kBonebreakMote_Launch, kBonebreakMote_Fade, kBonebreakMote_DrawFan, kBonebreakMote_DrawRing,
    kBonebreakMote_Alloc,
    kWarShout_Task, kWarShout_Start, kWarShout_Rally, kWarShoutChild_Task, kWarShoutMote_Run, kWarShoutMote_Appear,
    kWarShoutMote_Rise, kWarShoutMote_Circle, kWarShoutMote_Fade, kWarShoutBuff_Run, kWarShoutBuff_Start,
    kFocus_Task, kFocus_Start, kFocus_Kind, kFocusChild_Task, kFocusAura_Run, kFocusAura_Start, kFocusAura_Grow,
    kFocusAura_Swell, kFocusAura_Brighten, kFocusAura_Dim, kFocusAura_End, kFocusMote_Run, kFocusMote_Start,
    kFocusMote_Grow, kFocusMote_Rise, kFocusMote_Draw, kFocusAura_DrawRing, kFocusAura_DrawDisc,
    kEnlighten_Task, kEnlighten_Start, kEnlighten_Apply, kEnlightenChild_Task, kEnlightenRays_Run,
    kEnlightenRays_Start, kEnlightenRays_Grow, kEnlightenRays_Fade, kEnlightenRing_Run, kEnlightenRing_Start,
    kEnlightenRing_Spread, kEnlighten_AnchorToActor, kEnlightenRays_DrawLines, kEnlightenRays_DrawDisc,
    kEnlightenRing_Draw, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into every round. The commits log it
// whole and move the pointer on by the size, as the real ones do; so every
// primitive of a draw is compared, and a copy that reads the pointer once
// where the original reads it again lands elsewhere.
constexpr unsigned kPacketBytes = 0x800;
alignas(16) unsigned char g_packets[kPacketBytes];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 7) * 0x40; }

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0, kFrameSet = 0x9039D8, kAbilityId = 0x904B80,
                        kFormIndex = 0x904B89, kMotePool = 0x679B40, kPoolBytes = 64 * 0x84;

// --- the callees' effects (after the recorder's log and disturbance) ------------

void Advance(std::uint32_t size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_packets || p + 0x100 > g_packets + kPacketBytes) p = g_packets + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[3]);
    return answer;
}
// The sprite calls that act on Sprite_Current: which sprite; the screen
// update the frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// A flag from the recorders' own stream (0 a third of the time, garbage
// above), not from the hash that chose the disturbance: a caller's read after
// a "no" then meets a moved cell too.
std::uint32_t FreshFlag() {
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
}
std::uint32_t TickEffect(const std::uint32_t*, std::uint32_t) {
    mh::Note(Key(Sprite_Current));
    return FreshFlag();
}
std::uint32_t FlagEffect(const std::uint32_t*, std::uint32_t) { return FreshFlag(); }

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kPh = mh::Answer::kPhase;
#define S07_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S07_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S07_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S07_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &TickEffect},
    {S07_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &FlagEffect},
    // the sprite calls
    {S07_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &TickEffect},
    {S07_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S07_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    // the draw library
    {S07_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S07_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S07_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S07_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S07_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    // The projections: the vertices logged by what they hold (six bytes of
    // each SVECTOR), the outputs in the packet buffer by address, the depth
    // and flag pointers into the caller's frame masked off.
    {S07_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S07_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S07_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S07_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Other units' called by address: MAGIC008's screen shade, MAGIC219's
    // record free (a tail jump).
    {S07_RAW(0x4A29C0), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4F6290), 0, {}, kPh, 0, 0},
    // This group's own, called directly.
    {S07_RAW(0x4A3E10), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A3F90), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A4160), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A51C0), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A53A0), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A57C0), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A5E90), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A6120), 0, {}, kPh, 0, 0},
    {S07_RAW(0x4A62B0), 0, {}, kPh, 0, 0},
    // EnlightenRays_DrawLines(first, length): the byte and the low word read
    {S07_RAW(0x4A5FC0), 2, {kU8, kU16}, kG, 0, 0},
    // the mote pool's alloc: an index 0..0x3F (its callers do not test 0xFF)
    {S07_RAW(0x4A43E0), 0, {}, mh::Answer::kByte, 0, 0x3F},
    // Focus_Kind: a byte in al
    {S07_RAW(0x4A4D90), 0, {}, mh::Answer::kByte, 0, 0xFF},
    // WarShoutMote_Run's steps, called with the frame-offset table swapped:
    // listed before their .data table registers them as plain handlers.
    {S07_RAW(0x4A4750), 0, {kFrameSet}, kPh, 0, 0},
    {S07_RAW(0x4A48B0), 0, {kFrameSet}, kPh, 0, 0},
    {S07_RAW(0x4A49D0), 0, {kFrameSet}, kPh, 0, 0},
    {S07_RAW(0x4A4AD0), 0, {kFrameSet}, kPh, 0, 0},
    {S07_RAW(0x4AF490), 0, {kFrameSet}, kPh, 0, 0},
};
#undef S07_OURS
#undef S07_RAW

// The eight .data handler tables the dispatchers read in place (symbols.toml),
// each cell once.
const mh::DataTable kTables[] = {
    {0x65A6E8, 1}, {0x65A6EC, 1}, {0x65A6F0, 3}, {0x65A6FC, 2},
    {0x65A704, 5}, {0x65A728, 2}, {0x65A750, 2}, {0x65A758, 2},
};

mh::Region g_regions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                               // Gfx_PacketNext
    {0, kPacketBytes},                           // g_packets (filled in at start-up)
    {0x7E0700, 0xC00},                           // MoveScript_TintRecords
    {0x812980, 0x40},                            // Gfx_ClutStrip 0x1A00..0x1A1F
    {0x80E980, 0x40},                            // Gfx_ClutStripSource, the same
    {0x80F980, 0x20},                            // Gfx_ClutStrip 0x200..0x20F
    {0x80B980, 0x20},                            // Gfx_ClutStripSource, the same
    {kFrameSet, 4},                              // the frame-offset table pointer
    {kAbilityId, 0x10},                          // the ability word 0x904B80, the form index 0x904B89
    {kMotePool, kPoolBytes},                     // BonebreakMote_Pool
    // the overlays' .data the functions read, less the handler cells
    {0x65A718, 0x10},                            // WarShoutMote_Animations
    {0x65A730, 0x20},                            // FocusAura_DiscShades .. FocusMote_Delays
    {0x65A760, 0xCC},                            // Enlighten_AnchorOffsets, _AnchorOffsetsB, _AnchorIndexB
};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kMotePool + (i & 63) * 0x84u); }
unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

// A real slot or record for a pool record's owner (the walk makes it the
// owner cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells a function reads again after a call: Gfx_PacketNext, a
// scratch word, a vertex word, a pool record's live bit, the owner's count
// +0xB, the ability word, a tint byte, the frame-offset table pointer.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PacketAt(v); break;
    case 1: SetWord(mh::Mem(kScratch + (v % 8) * 2), word); break;
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: PoolRecord(v)[0] ^= 1; break;
    case 4: mh::Pointer(mh::at::kOwner)[0xB] = Byte(v % 3); break;
    case 5: SetWord(mh::Mem(kAbilityId), (v & 1) ? 0xA3 : word); break;
    case 6: mh::Mem(0x7E0700 + v % 0xC00)[0] = Byte(h >> 24); break;
    case 7: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    default: break;
    }
}

// --- the seed --------------------------------------------------------------------

// A byte `below` under `at` .. `above` over it.
unsigned char Near(unsigned at, unsigned below, unsigned above) {
    return Byte(at - below + mh::Next() % (below + above + 1));
}

void FillPool() {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 65;
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);   // full
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(kMotePool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PacketAt(mh::Next());
    FillPool();
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Mem(mh::at::kTarget)[0] | 0x40);
    if (mh::Half()) SetWord(mh::Mem(kAbilityId), mh::Half() ? 0xA3 : Near(0xA3, 1, 1));
    unsigned char* const tint = MoveScript_TintRecords + sc[0xB] * 12u;
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kBonebreak_Task: case kWarShout_Task: case kFocus_Task: case kEnlighten_Task:
        sc[1] = Byte(mh::Next() % 3);
        break;
    case kBonebreakChild_Task: case kBonebreakMote_Task: sc[1] = 0; break;
    case kWarShoutChild_Task: case kFocusChild_Task: case kEnlightenChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kBonebreakChild_Run: case kFocusAura_Run: sc[2] = Byte(mh::Next() % 6); if (mh::Half()) sc[0] = 0; break;
    case kBonebreakMote_Run: case kFocusMote_Run: sc[2] = Byte(mh::Next() % 3); if (mh::Half()) sc[0] = 0; break;
    case kWarShoutMote_Run: sc[2] = Byte(mh::Next() % 5); if (mh::Half()) sc[0] = 0; break;
    case kWarShoutBuff_Run: case kEnlightenRing_Run: sc[2] = Byte(mh::Next() % 2); if (mh::Half()) sc[0] = 0; break;
    case kEnlightenRays_Run: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    // the count-downs and count-ups: at, below and past their ends
    case kBonebreakChild_Burst: case kBonebreakMote_Launch: case kWarShoutMote_Appear: case kFocusMote_Start:
        if (mh::Often()) sc[9] = Near(1, 0, 1);
        break;
    case kBonebreakChild_Shade: sc[9] = Near(0xC, 1, 1); break;
    case kBonebreakChild_WaitMotes: case kWarShout_Rally: case kEnlighten_Apply:
        if (mh::Often()) sc[0xB] = Near(0, 0, 1);
        break;
    case kBonebreakMote_Fade: case kFocusAura_End: sc[0xA] = Near(2, 1, 1); break;
    case kWarShoutMote_Rise: sc[0x5D] = Near(0xBC, 1, 1); break;
    case kWarShoutMote_Circle:
        if (mh::Often()) SetLong(sc + 0xC, static_cast<std::int32_t>(0xFE + mh::Next() % 3));
        break;
    case kFocusAura_Grow: sc[9] = Near(0xE, 1, 1); break;
    case kFocusAura_Swell: sc[0xA] = Near(0x16, 1, 1); break;
    case kFocusAura_Brighten: tint[2] = Near(0xE, 1, 1); break;
    case kFocusAura_Dim: tint[2] = Near(2, 1, 1); break;
    case kFocusMote_Grow: sc[9] = Near(0x70, 1, 1); break;
    case kFocusMote_Rise: case kEnlightenRays_Fade: sc[0xA] = Near(1, 0, 1); break;
    case kEnlightenRays_Grow: sc[0xA] = Near(0xF, 1, 1); break;
    case kEnlightenRing_Spread: sc[0xB] = Near(1, 0, 1); break;
    // the shades: both sides of +9 0x10
    case kFocusAura_DrawRing: case kFocusAura_DrawDisc:
        if (mh::Often()) sc[9] = Near(0x10, 2, 1);
        if (mh::Often()) sc[4] = Byte(mh::Next() % 2);
        break;
    case kFocusMote_Draw: if (mh::Often()) sc[4] = Byte(mh::Next() % 2); break;
    // the anchor: a party target most of the time, both tables, the
    // directions 0..3 and past
    case kEnlighten_AnchorToActor: {
        if (mh::Often()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Next() % 3);
        const unsigned t = mh::Mem(mh::at::kTarget)[0];
        if (t < 3) {
            unsigned char* const rec = mh::PartyOf(Byte(t));
            rec[0x134] = Byte(mh::Half() ? rec[0x134] | 2 : rec[0x134] & ~2u);
            if (mh::Often()) rec[0x89] = Byte(mh::Next() % 11);
        }
        if (mh::Often()) mh::Mem(kFormIndex)[0] = Byte(mh::Next() % 25);
        if (mh::Often()) sc[8] = Byte(mh::Next() % 5);
        break;
    }
    default: break;
    }
}

// EnlightenRays_DrawLines's two words: the first step a byte below 0xE0 (from
// 0xE0 on the original's loop never ends: docs/magic_s07.md section 7),
// garbage above it; the length any word.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kEnlightenRays_DrawLines) return;
    a[0] = (a[0] & ~0xFFu) | (mh::Next() % 0xE0);
}

}  // namespace

void SelfTest() {
    g_regions[3].at = Key(g_packets);
    mh::Group group = {
        "magic_s07", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s07
