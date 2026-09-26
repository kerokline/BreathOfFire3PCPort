// BOF3X_SHADOW=magic_s19: group S19's two overlays (MAGIC083, MAGIC086)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s19.md section 4.
//
// The clone table is tools/magic_rows.py --unit MAGIC083 / MAGIC086 --clones.
// Beyond the harness's standard set this group needs its own recorders
// (magic_harness::Custom) for the draws' callees: Math_Sin / Math_Cos and the
// GPU and GTE primitive calls take up to ten arguments, write through pointers
// into the caller's frame and the primitive, and change nothing a spell reads
// - so their recorders log and answer but never disturb - while
// Gfx_CommitPrim, MapView_LinkPrimAt and 0x4FB880 move Gfx_PacketNext, which
// the draws read again, and so their recorders move it too.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s19.h"
#include "game/move_script_bytes.h"

// Ours, called through MH_CALL by one another (magic_s19.cpp).
extern "C" unsigned char __cdecl Shield_Kind(void);
extern "C" void __cdecl ShieldAura_DrawDisc(void);
extern "C" void __cdecl ShieldAura_DrawHalo(void);
extern "C" void __cdecl ShieldSpark_Dispatch(void);
extern "C" void __cdecl ShieldSpark_DrawCrystal(void);
extern "C" unsigned char __cdecl ShieldSpark_Alloc(void);
extern "C" void __cdecl BarrierDisc_Draw(void);
extern "C" void __cdecl BarrierRing_Draw(void);
extern "C" void __cdecl BarrierLine_Draw(void);
extern "C" void __cdecl Shield_Task(void);
extern "C" void __cdecl Shield_Start(void);
extern "C" void __cdecl ShieldAura_Dispatch(void);
extern "C" void __cdecl ShieldAura_Task(void);
extern "C" void __cdecl ShieldAura_Wait(void);
extern "C" void __cdecl ShieldAura_Rise(void);
extern "C" void __cdecl ShieldAura_Fade(void);
extern "C" void __cdecl ShieldSpark_Task(void);
extern "C" void __cdecl ShieldSpark_Place(void);
extern "C" void __cdecl ShieldSpark_Rise(void);
extern "C" void __cdecl ShieldSpark_Fall(void);
extern "C" void __cdecl ShieldSpark_Spin(void);
extern "C" void __cdecl ShieldSpark_Orbit(void);
extern "C" void __cdecl Barrier_Task(void);
extern "C" void __cdecl Barrier_Start(void);
extern "C" void __cdecl Barrier_Tint(void);
extern "C" void __cdecl Barrier_WaitRings(void);
extern "C" void __cdecl Barrier_Fade(void);
extern "C" void __cdecl BarrierPart_Dispatch(void);
extern "C" void __cdecl BarrierDisc_Task(void);
extern "C" void __cdecl BarrierDisc_Grow(void);
extern "C" void __cdecl BarrierDisc_Wait(void);
extern "C" void __cdecl BarrierDisc_Shrink(void);
extern "C" void __cdecl BarrierRing_Task(void);
extern "C" void __cdecl BarrierRing_Grow(void);
extern "C" void __cdecl BarrierRing_Rise(void);
extern "C" void __cdecl BarrierRing_Lift(void);
extern "C" void __cdecl BarrierRing_Hold(void);
extern "C" void __cdecl BarrierRing_Settle(void);
extern "C" void __cdecl BarrierRing_End(void);
extern "C" void __cdecl BarrierLine_Task(void);
extern "C" void __cdecl BarrierLine_Wait(void);
extern "C" void __cdecl BarrierLine_Delay(void);
extern "C" void __cdecl BarrierLine_Fly(void);

namespace magic_s19 {
namespace {

namespace mh = magic_harness;

// tools/magic_rows.py --unit MAGIC083 --clones and --unit MAGIC086 --clones,
// 2026-09-25 (capstone: every jump internal, no jump table, no REFUSED line).
// 0x4C1470: 0x75 bytes
constexpr mh::CallSite kCalls4C1470[] = {{0x53, 0x4C1E70}};
constexpr mh::Imm kImms4C1470[] = {{0x16, 0x4C14F0}, {0x1E, 0x4E5200}};
// 0x4C14F0: 0x218 bytes
constexpr mh::CallSite kCalls4C14F0[] = {{0x25, 0x4C1710}, {0x6D, 0x4456C0}, {0x81, 0x435180}, {0x12E, 0x4456C0}, {0x142, 0x435180}, {0x20F, 0x587900}};
// 0x4C1710: 0x20 bytes
// 0x4C1730: 0x12 bytes; +0xB note: jmp through .data 0x65b47c, 43 code entries (a data_tables entry)
// 0x4C1750: 0x38 bytes; +0xB note: call through .data 0x65b480, 42 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C1750[] = {{0x23, 0x4B7D40}, {0x28, 0x4C1C20}, {0x2D, 0x4C1A20}, {0x32, 0x5A7BC0}};
// 0x4C1790: 0x86 bytes
constexpr mh::CallSite kCalls4C1790[] = {{0x53, 0x454DC0}, {0x61, 0x454CC0}};
// 0x4C1820: 0x111 bytes
constexpr mh::CallSite kCalls4C1820[] = {{0x76, 0x4C2790}};
// 0x4C1940: 0xDF bytes
constexpr mh::CallSite kCalls4C1940[] = {{0xA3, 0x454DC0}, {0xAD, 0x4FBDB0}, {0xBF, 0x4FB790}};
// 0x4C1A20: 0x1F1 bytes
constexpr mh::CallSite kCalls4C1A20[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x8E, 0x5A7A00}, {0xA0, 0x5A7A50}, {0xE4, 0x5A7A00}, {0xF6, 0x5A7A50}, {0x123, 0x5A75F0}, {0x12B, 0x5A7780}, {0x155, 0x5A84A0}, {0x15B, 0x5A9310}, {0x1B2, 0x461E50}, {0x1D9, 0x5A77C0}, {0x1E2, 0x461E50}};
// 0x4C1C20: 0x247 bytes
constexpr mh::CallSite kCalls4C1C20[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x84, 0x5A7A00}, {0x96, 0x5A7A50}, {0xA8, 0x5A7A00}, {0xBA, 0x5A7A50}, {0x10C, 0x5A7A00}, {0x11E, 0x5A7A50}, {0x130, 0x5A7A00}, {0x142, 0x5A7A50}, {0x176, 0x5A7610}, {0x17D, 0x5A7780}, {0x1B0, 0x5A85F0}, {0x1B9, 0x5A9350}, {0x208, 0x461E50}, {0x22E, 0x5A77C0}, {0x237, 0x461E50}};
// 0x4C1E70: 0x12 bytes; +0xB note: jmp through .data 0x65b494, 37 code entries (a data_tables entry)
// 0x4C1E90: 0x12 bytes; +0xB note: jmp through .data 0x65b498, 36 code entries (a data_tables entry)
// 0x4C1EB0: 0xE8 bytes
constexpr mh::CallSite kCalls4C1EB0[] = {{0x33, 0x5A7A00}, {0x60, 0x5A7A50}};
// 0x4C1FA0: 0x95 bytes
constexpr mh::CallSite kCalls4C1FA0[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4C12F0}, {0x19, 0x4B7D40}, {0x1E, 0x4C22D0}, {0x23, 0x5A7BC0}};
// 0x4C2040: 0x82 bytes
constexpr mh::CallSite kCalls4C2040[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4C12F0}, {0x19, 0x4B7D40}, {0x1E, 0x4C22D0}, {0x23, 0x5A7BC0}};
// 0x4C20D0: 0xE1 bytes
constexpr mh::CallSite kCalls4C20D0[] = {{0x0, 0x4FBD10}, {0x16, 0x5B93D2}, {0x23, 0x5B93D2}, {0x2F, 0x4C12F0}, {0x37, 0x4B7D40}, {0x3C, 0x4C22D0}, {0x41, 0x5A7BC0}, {0xB0, 0x587900}};
// 0x4C21C0: 0x104 bytes
constexpr mh::CallSite kCalls4C21C0[] = {{0x0, 0x4FBD10}, {0x5, 0x5B93D2}, {0x11, 0x4C12F0}, {0x16, 0x4B7D40}, {0x1B, 0x4C22D0}, {0x20, 0x5A7BC0}, {0x86, 0x5A7A00}, {0xB4, 0x5A7A50}, {0xFE, 0x4F6290}};
// 0x4C22D0: 0x4B6 bytes
constexpr mh::CallSite kCalls4C22D0[] = {{0x15, 0x5A77C0}, {0x2A, 0x572FA0}, {0xF5, 0x5A7A00}, {0x115, 0x5A7A50}, {0x14C, 0x5A7A00}, {0x16C, 0x5A7A50}, {0x192, 0x5A75F0}, {0x199, 0x5A7780}, {0x1C3, 0x5A87A0}, {0x275, 0x4FB880}, {0x32A, 0x5A7A00}, {0x34A, 0x5A7A50}, {0x381, 0x5A7A00}, {0x3A1, 0x5A7A50}, {0x3C7, 0x5A75F0}, {0x3CE, 0x5A7780}, {0x3F8, 0x5A87A0}, {0x4A6, 0x4FB880}};
// 0x4C2790: 0x57 bytes
// 0x4C27F0: 0x46 bytes
constexpr mh::Imm kImms4C27F0[] = {{0xF, 0x4C2840}, {0x17, 0x4C2960}, {0x22, 0x4B1ED0}, {0x2A, 0x4C29B0}, {0x32, 0x4C29C0}, {0x3A, 0x4BDC10}};
// 0x4C2840: 0x116 bytes
constexpr mh::CallSite kCalls4C2840[] = {{0x2F, 0x435180}, {0x8D, 0x435180}, {0xF2, 0x587900}};
// 0x4C2960: 0x46 bytes
constexpr mh::CallSite kCalls4C2960[] = {{0x13, 0x454DC0}, {0x21, 0x454CC0}};
// 0x4C29B0: 0xF bytes
// 0x4C29C0: 0x85 bytes
constexpr mh::CallSite kCalls4C29C0[] = {{0x68, 0x454DC0}, {0x74, 0x4FBDB0}};
// 0x4C2A50: 0x12 bytes; +0xB note: jmp through .data 0x65b4ac, 31 code entries (a data_tables entry)
// 0x4C2A70: 0x2D bytes; +0xB note: call through .data 0x65b4b8, 28 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C2A70[] = {{0x1D, 0x4B7D40}, {0x22, 0x4C2B10}, {0x27, 0x5A7BC0}};
// 0x4C2AA0: 0x29 bytes
// 0x4C2AD0: 0x14 bytes
// 0x4C2AF0: 0x1F bytes
constexpr mh::CallSite kCalls4C2AF0[] = {{0x19, 0x4351F0}};
// 0x4C2B10: 0x1C7 bytes
constexpr mh::CallSite kCalls4C2B10[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x3A, 0x5A7A00}, {0x50, 0x5A7A50}, {0x98, 0x5A7A00}, {0xAE, 0x5A7A50}, {0xDF, 0x5A75F0}, {0xE7, 0x5A7780}, {0x111, 0x5A84A0}, {0x117, 0x5A9310}, {0x188, 0x461E50}, {0x1AF, 0x5A77C0}, {0x1B8, 0x461E50}};
// 0x4C2CE0: 0x2D bytes; +0xB note: call through .data 0x65b4c4, 25 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C2CE0[] = {{0x1D, 0x4B7D40}, {0x22, 0x4C2E30}, {0x27, 0x5A7BC0}};
// 0x4C2D10: 0x1C bytes
// 0x4C2D30: 0x38 bytes
// 0x4C2D70: 0x1C bytes
// 0x4C2D90: 0x1D bytes
// 0x4C2DB0: 0x38 bytes
// 0x4C2DF0: 0x3C bytes
constexpr mh::CallSite kCalls4C2DF0[] = {{0x36, 0x4351F0}};
// 0x4C2E30: 0x456 bytes
constexpr mh::CallSite kCalls4C2E30[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x8B, 0x5A7A50}, {0xA2, 0x5A7A00}, {0x122, 0x5A7A50}, {0x138, 0x5A7A00}, {0x14E, 0x5A7A00}, {0x16A, 0x5A7A50}, {0x180, 0x5A7A00}, {0x196, 0x5A7A00}, {0x21D, 0x5A77C0}, {0x227, 0x572FA0}, {0x238, 0x5A7A50}, {0x24E, 0x5A7A00}, {0x26D, 0x5A7A50}, {0x283, 0x5A7A00}, {0x2C1, 0x5A7A50}, {0x2D7, 0x5A7A00}, {0x2FD, 0x5A7610}, {0x305, 0x5A7780}, {0x338, 0x5A85F0}, {0x33E, 0x5A9350}, {0x39B, 0x572FA0}, {0x40F, 0x461E50}, {0x43D, 0x5A77C0}, {0x446, 0x461E50}};
// 0x4C3290: 0x33 bytes; +0xB note: call through .data 0x65b4dc, 19 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C3290[] = {{0x23, 0x4B7D40}, {0x28, 0x4C3360}, {0x2D, 0x5A7BC0}};
// 0x4C32D0: 0x14 bytes
// 0x4C32F0: 0x35 bytes
// 0x4C3330: 0x2E bytes
constexpr mh::CallSite kCalls4C3330[] = {{0x28, 0x4351F0}};
// 0x4C3360: 0x128 bytes
constexpr mh::CallSite kCalls4C3360[] = {{0x13, 0x5A7A00}, {0x3E, 0x5A7A50}, {0x85, 0x5A77C0}, {0x9B, 0x572FA0}, {0xA7, 0x5A76D0}, {0xAF, 0x5A7780}, {0xD9, 0x5A84A0}, {0xE2, 0x5A9420}, {0x11B, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Shield_Task", 0x4C1470, 0x75, kCalls4C1470, MH_N(kCalls4C1470), kImms4C1470, MH_N(kImms4C1470), nullptr, 0, reinterpret_cast<const void*>(&::Shield_Task)},
    {"Shield_Start", 0x4C14F0, 0x218, kCalls4C14F0, MH_N(kCalls4C14F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Shield_Start)},
    {"Shield_Kind", 0x4C1710, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Shield_Kind)},
    {"ShieldAura_Dispatch", 0x4C1730, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_Dispatch)},
    {"ShieldAura_Task", 0x4C1750, 0x38, kCalls4C1750, MH_N(kCalls4C1750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_Task)},
    {"ShieldAura_Wait", 0x4C1790, 0x86, kCalls4C1790, MH_N(kCalls4C1790), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_Wait)},
    {"ShieldAura_Rise", 0x4C1820, 0x111, kCalls4C1820, MH_N(kCalls4C1820), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_Rise)},
    {"ShieldAura_Fade", 0x4C1940, 0xDF, kCalls4C1940, MH_N(kCalls4C1940), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_Fade)},
    {"ShieldAura_DrawDisc", 0x4C1A20, 0x1F1, kCalls4C1A20, MH_N(kCalls4C1A20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_DrawDisc)},
    {"ShieldAura_DrawHalo", 0x4C1C20, 0x247, kCalls4C1C20, MH_N(kCalls4C1C20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldAura_DrawHalo)},
    {"ShieldSpark_Dispatch", 0x4C1E70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Dispatch)},
    {"ShieldSpark_Task", 0x4C1E90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Task)},
    {"ShieldSpark_Place", 0x4C1EB0, 0xE8, kCalls4C1EB0, MH_N(kCalls4C1EB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Place)},
    {"ShieldSpark_Rise", 0x4C1FA0, 0x95, kCalls4C1FA0, MH_N(kCalls4C1FA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Rise)},
    {"ShieldSpark_Fall", 0x4C2040, 0x82, kCalls4C2040, MH_N(kCalls4C2040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Fall)},
    {"ShieldSpark_Spin", 0x4C20D0, 0xE1, kCalls4C20D0, MH_N(kCalls4C20D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Spin)},
    {"ShieldSpark_Orbit", 0x4C21C0, 0x104, kCalls4C21C0, MH_N(kCalls4C21C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Orbit)},
    {"ShieldSpark_DrawCrystal", 0x4C22D0, 0x4B6, kCalls4C22D0, MH_N(kCalls4C22D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_DrawCrystal)},
    {"ShieldSpark_Alloc", 0x4C2790, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShieldSpark_Alloc)},
    {"Barrier_Task", 0x4C27F0, 0x46, nullptr, 0, kImms4C27F0, MH_N(kImms4C27F0), nullptr, 0, reinterpret_cast<const void*>(&::Barrier_Task)},
    {"Barrier_Start", 0x4C2840, 0x116, kCalls4C2840, MH_N(kCalls4C2840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Barrier_Start)},
    {"Barrier_Tint", 0x4C2960, 0x46, kCalls4C2960, MH_N(kCalls4C2960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Barrier_Tint)},
    {"Barrier_WaitRings", 0x4C29B0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Barrier_WaitRings)},
    {"Barrier_Fade", 0x4C29C0, 0x85, kCalls4C29C0, MH_N(kCalls4C29C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Barrier_Fade)},
    {"BarrierPart_Dispatch", 0x4C2A50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierPart_Dispatch)},
    {"BarrierDisc_Task", 0x4C2A70, 0x2D, kCalls4C2A70, MH_N(kCalls4C2A70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierDisc_Task)},
    {"BarrierDisc_Grow", 0x4C2AA0, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierDisc_Grow)},
    {"BarrierDisc_Wait", 0x4C2AD0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierDisc_Wait)},
    {"BarrierDisc_Shrink", 0x4C2AF0, 0x1F, kCalls4C2AF0, MH_N(kCalls4C2AF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierDisc_Shrink)},
    {"BarrierDisc_Draw", 0x4C2B10, 0x1C7, kCalls4C2B10, MH_N(kCalls4C2B10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierDisc_Draw)},
    {"BarrierRing_Task", 0x4C2CE0, 0x2D, kCalls4C2CE0, MH_N(kCalls4C2CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Task)},
    {"BarrierRing_Grow", 0x4C2D10, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Grow)},
    {"BarrierRing_Rise", 0x4C2D30, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Rise)},
    {"BarrierRing_Lift", 0x4C2D70, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Lift)},
    {"BarrierRing_Hold", 0x4C2D90, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Hold)},
    {"BarrierRing_Settle", 0x4C2DB0, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Settle)},
    {"BarrierRing_End", 0x4C2DF0, 0x3C, kCalls4C2DF0, MH_N(kCalls4C2DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_End)},
    {"BarrierRing_Draw", 0x4C2E30, 0x456, kCalls4C2E30, MH_N(kCalls4C2E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierRing_Draw)},
    {"BarrierLine_Task", 0x4C3290, 0x33, kCalls4C3290, MH_N(kCalls4C3290), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierLine_Task)},
    {"BarrierLine_Wait", 0x4C32D0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierLine_Wait)},
    {"BarrierLine_Delay", 0x4C32F0, 0x35, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierLine_Delay)},
    {"BarrierLine_Fly", 0x4C3330, 0x2E, kCalls4C3330, MH_N(kCalls4C3330), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierLine_Fly)},
    {"BarrierLine_Draw", 0x4C3360, 0x128, kCalls4C3360, MH_N(kCalls4C3360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BarrierLine_Draw)},
};
#undef MH_N
enum : unsigned {
    kShieldTask,
    kShieldStart,
    kShieldKind,
    kShieldAuraDispatch,
    kShieldAuraTask,
    kShieldAuraWait,
    kShieldAuraRise,
    kShieldAuraFade,
    kShieldAuraDrawDisc,
    kShieldAuraDrawHalo,
    kShieldSparkDispatch,
    kShieldSparkTask,
    kShieldSparkPlace,
    kShieldSparkRise,
    kShieldSparkFall,
    kShieldSparkSpin,
    kShieldSparkOrbit,
    kShieldSparkDrawCrystal,
    kShieldSparkAlloc,
    kBarrierTask,
    kBarrierStart,
    kBarrierTint,
    kBarrierWaitRings,
    kBarrierFade,
    kBarrierPartDispatch,
    kBarrierDiscTask,
    kBarrierDiscGrow,
    kBarrierDiscWait,
    kBarrierDiscShrink,
    kBarrierDiscDraw,
    kBarrierRingTask,
    kBarrierRingGrow,
    kBarrierRingRise,
    kBarrierRingLift,
    kBarrierRingHold,
    kBarrierRingSettle,
    kBarrierRingEnd,
    kBarrierRingDraw,
    kBarrierLineTask,
    kBarrierLineWait,
    kBarrierLineDelay,
    kBarrierLineFly,
    kBarrierLineDraw,
};

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

constexpr std::uint32_t kPool = 0x68FA78, kPoolStride = 0x84;
constexpr unsigned kPoolCount = 0x18;
constexpr std::uint32_t kTints = 0x7E0700;
unsigned char* Pool(unsigned i) { return mh::Mem(kPool + (i % kPoolCount) * kPoolStride); }

// The primitives' buffer: Gfx_PacketNext is put inside it every round.
alignas(16) unsigned char g_prims[0x3000];

// --- the recorders ------------------------------------------------------------

std::uint32_t Mix(std::uint32_t h, std::uint32_t k) {
    h += k * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
// Three shorts of a vector, folded so that any one differing shows.
std::uint32_t Vec3(const short* v) {
    std::uint32_t h = 0x811C9DC5u;
    for (int i = 0; i < 3; ++i) h = (h ^ static_cast<std::uint16_t>(v[i])) * 0x01000193u;
    return h;
}
void PutFloat(float* f, std::uint32_t bits) { std::memcpy(f, &bits, sizeof bits); }
// The screen point (x, y) a projection writes, and for Gte_RotAverage3 its z.
void Project(float* out, std::uint32_t h, unsigned n) {
    for (unsigned i = 0; i < n; ++i) PutFloat(out + i, Mix(h, i + 1));
}

// Math_Sin / Math_Cos: the angle logged; three in four answers in -4096..4096
// (the table's range), the rest anything, so that a wrapping multiply shows.
template <std::uint32_t A> int __cdecl StubTrig(int angle) {
    mh::Record(A, static_cast<std::uint32_t>(angle));
    const std::uint32_t h = mh::Noise();
    return h % 4 ? static_cast<int>(h % 8193) - 4096 : static_cast<int>(h);
}
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    mh::Record(bof3::addr::Gpu_SetDrawMode, Key(prim), static_cast<std::uint32_t>(dfe) ^ (static_cast<std::uint32_t>(dtd) << 16),
             tpage, static_cast<std::uint32_t>(tw));
}
// A commit or a link moves Gfx_PacketNext on by the size - three times in
// four; the fourth models a full pool or a cell off the map, where the real
// ones leave it.
void Advance(unsigned size) {
    if (mh::Noise() % 4) Gfx_PacketNext = Gfx_PacketNext + (size & 0xFF);
}
void __cdecl StubCommit(unsigned slot, unsigned size) {
    mh::Record(bof3::addr::Gfx_CommitPrim, slot & 0xFF, size & 0xFF);
    Advance(size);
}
void __cdecl StubLinkAt(unsigned long x, unsigned long z, int dy, unsigned size) {
    mh::Record(bof3::addr::MapView_LinkPrimAt, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z),
             static_cast<std::uint32_t>(dy) & 0xFF, size & 0xFF);
    Advance(size);
}
template <std::uint32_t A> void __cdecl StubPrim(unsigned char* prim) { mh::Record(A, Key(prim)); }
unsigned char* __cdecl StubPolyG4(unsigned char* prim) {
    mh::Record(bof3::addr::Gpu_SetPolyG4, Key(prim));
    return prim;
}
void __cdecl StubSemi(unsigned char* prim, unsigned abe) { mh::Record(bof3::addr::Gpu_SetSemiTrans, Key(prim), abe); }
template <std::uint32_t A> void __cdecl StubDepths(void* prim) { mh::Record(A, Key(prim)); }

// The projections: the vectors' contents and the outputs' places logged; the
// outputs and the depth written; a depth answered (Gte_RotAverage3's is kept
// by the crystal).
long __cdecl StubRtp3(const short* v0, const short* v1, const short* v2, float* s0, float* s1, float* s2, long* p) {
    mh::Record(bof3::addr::Gte_RotTransPers3, Key(v0) ^ (Key(v1) << 1) ^ (Key(v2) << 2), Vec3(v0) ^ (Vec3(v1) << 1) ^ (Vec3(v2) << 3),
             Key(s0), Key(s1) ^ (Key(s2) << 1));
    const std::uint32_t h = mh::Noise();
    Project(s0, Mix(h, 1), 2);
    Project(s1, Mix(h, 2), 2);
    Project(s2, Mix(h, 3), 2);
    *p = static_cast<long>(Mix(h, 4));
    return static_cast<long>(Mix(h, 5));
}
long __cdecl StubRtp4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0, float* s1, float* s2,
                      float* s3, long* p) {
    mh::Record(bof3::addr::Gte_RotTransPers4, Key(v0) ^ (Key(v1) << 1) ^ (Key(v2) << 2) ^ (Key(v3) << 3),
             Vec3(v0) ^ (Vec3(v1) << 1) ^ (Vec3(v2) << 3) ^ (Vec3(v3) << 5), Key(s0) ^ (Key(s1) << 1),
             Key(s2) ^ (Key(s3) << 1));
    const std::uint32_t h = mh::Noise();
    Project(s0, Mix(h, 1), 2);
    Project(s1, Mix(h, 2), 2);
    Project(s2, Mix(h, 3), 2);
    Project(s3, Mix(h, 4), 2);
    *p = static_cast<long>(Mix(h, 5));
    return static_cast<long>(Mix(h, 6));
}
long __cdecl StubRotAverage3(const short* v0, const short* v1, const short* v2, float* s0, float* s1, float* s2, long* p) {
    mh::Record(bof3::addr::Gte_RotAverage3, Key(v0) ^ (Key(v1) << 1) ^ (Key(v2) << 2), Vec3(v0) ^ (Vec3(v1) << 1) ^ (Vec3(v2) << 3),
             Key(s0), Key(s1) ^ (Key(s2) << 1));
    const std::uint32_t h = mh::Noise();
    Project(s0, Mix(h, 1), 3);
    Project(s1, Mix(h, 2), 3);
    Project(s2, Mix(h, 3), 3);
    *p = static_cast<long>(Mix(h, 4));
    return static_cast<long>(Mix(h, 5));
}
// 0x4FB880 (the library's): the four depths logged, then - a quarter of the
// time, as when the cell is off the map or the pool full - the primitives
// dropped (Gfx_PacketNext back by count * stride); the depths it zeroes as it
// links.
constexpr std::uint32_t kLinkSorted = 0x4FB880;
void __cdecl StubLinkSorted(unsigned long x, unsigned long z, long* depths, unsigned char* prim, unsigned count,
                            unsigned stride, unsigned last) {
    mh::Record(kLinkSorted, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), Key(prim),
             (count & 0xFF) | ((stride & 0xFF) << 8) | ((last & 0xFF) << 16));
    mh::Record(kLinkSorted, static_cast<std::uint32_t>(depths[0]), static_cast<std::uint32_t>(depths[1]),
             static_cast<std::uint32_t>(depths[2]), static_cast<std::uint32_t>(depths[3]));
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) {
        Gfx_PacketNext = Gfx_PacketNext - (count & 0xFF) * (stride & 0xFF);
    } else {
        for (unsigned i = 0; i < 4; ++i)
            if (Mix(h, i) & 1) depths[i] = 0;
    }
}
unsigned char __cdecl StubSetTint(unsigned char* sprite, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    mh::Record(bof3::addr::Sprite_SetTint, Key(sprite), r | (g << 8) | (b << 16), a);
    mh::Stir();
    return static_cast<unsigned char>(mh::Noise());
}
unsigned char __cdecl StubKind() {
    mh::Record(bof3::addr::Shield_Kind);
    mh::Stir();
    return static_cast<unsigned char>(mh::Noise());
}
// ShieldSpark_Alloc: a slot's index (its live bit set, as the real one), or
// 0xFF a fifth of the time.
unsigned char __cdecl StubAlloc() {
    mh::Record(bof3::addr::ShieldSpark_Alloc);
    mh::Stir();
    const std::uint32_t h = mh::Noise();
    if (h % 5 == 0) return 0xFF;
    const unsigned i = (h >> 8) % kPoolCount;
    Pool(i)[0] = static_cast<unsigned char>(Pool(i)[0] | 1);
    return static_cast<unsigned char>(i);
}
// A callee that runs on the current slot (a crystal's phase, a draw, the pool
// slot's free): the slot, its owner and its phase bytes logged; a crystal's
// phase may free or take another slot of the pool.
template <std::uint32_t A> void __cdecl StubOnSlot() {
    const unsigned char* const cur = Sprite_Current;
    mh::Record(A, Key(cur), static_cast<std::uint32_t>(move_script::Long(mh::Mem(mh::at::kOwner))), cur[1] | (cur[2] << 8));
    if (A == bof3::addr::ShieldSpark_Dispatch) {
        const std::uint32_t h = mh::Noise();
        if (h % 3 == 0) Pool((h >> 8) % kPoolCount)[0] ^= 1;
    }
    mh::Stir();
}

#define S19_CUSTOM_OURS(name, fn) \
    {#name, ::bof3::addr::name, KeyOf(&::name), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(fn)}
constexpr std::uint32_t kU8 = 0xFF, kU16 = 0xFFFF;
const mh::Callee kCallees[] = {
    S19_CUSTOM_OURS(Math_Sin, &StubTrig<bof3::addr::Math_Sin>),
    S19_CUSTOM_OURS(Math_Cos, &StubTrig<bof3::addr::Math_Cos>),
    S19_CUSTOM_OURS(Gpu_SetDrawMode, &StubDrawMode),
    S19_CUSTOM_OURS(Gfx_CommitPrim, &StubCommit),
    S19_CUSTOM_OURS(MapView_LinkPrimAt, &StubLinkAt),
    S19_CUSTOM_OURS(Gpu_SetPolyG3, &StubPrim<bof3::addr::Gpu_SetPolyG3>),
    S19_CUSTOM_OURS(Gpu_SetPolyG4, &StubPolyG4),
    S19_CUSTOM_OURS(Gpu_SetLineG3, &StubPrim<bof3::addr::Gpu_SetLineG3>),
    S19_CUSTOM_OURS(Gpu_SetSemiTrans, &StubSemi),
    S19_CUSTOM_OURS(Gte_RotTransPers3, &StubRtp3),
    S19_CUSTOM_OURS(Gte_RotTransPers4, &StubRtp4),
    S19_CUSTOM_OURS(Gte_RotAverage3, &StubRotAverage3),
    S19_CUSTOM_OURS(Gte_PrimDepths3_10B, &StubDepths<bof3::addr::Gte_PrimDepths3_10B>),
    S19_CUSTOM_OURS(Gte_PrimDepths4_10B, &StubDepths<bof3::addr::Gte_PrimDepths4_10B>),
    S19_CUSTOM_OURS(Gte_PrimDepths3_10C, &StubDepths<bof3::addr::Gte_PrimDepths3_10C>),
    S19_CUSTOM_OURS(Sprite_SetTint, &StubSetTint),
    {"0x4FB880", kLinkSorted, kLinkSorted, 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&StubLinkSorted)},
    S19_CUSTOM_OURS(Shield_Kind, &StubKind),
    S19_CUSTOM_OURS(ShieldSpark_Alloc, &StubAlloc),
    S19_CUSTOM_OURS(ShieldSpark_Dispatch, &StubOnSlot<bof3::addr::ShieldSpark_Dispatch>),
    S19_CUSTOM_OURS(ShieldAura_DrawDisc, &StubOnSlot<bof3::addr::ShieldAura_DrawDisc>),
    S19_CUSTOM_OURS(ShieldAura_DrawHalo, &StubOnSlot<bof3::addr::ShieldAura_DrawHalo>),
    S19_CUSTOM_OURS(ShieldSpark_DrawCrystal, &StubOnSlot<bof3::addr::ShieldSpark_DrawCrystal>),
    S19_CUSTOM_OURS(BarrierDisc_Draw, &StubOnSlot<bof3::addr::BarrierDisc_Draw>),
    S19_CUSTOM_OURS(BarrierRing_Draw, &StubOnSlot<bof3::addr::BarrierRing_Draw>),
    S19_CUSTOM_OURS(BarrierLine_Draw, &StubOnSlot<bof3::addr::BarrierLine_Draw>),
    {"0x4F6290", 0x4F6290, 0x4F6290, 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&StubOnSlot<0x4F6290>)},
    {"Battle_ActorIsOut", bof3::addr::Battle_ActorIsOut, KeyOf(&::Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    // the library's (group L): a kind-1 task 0x48 by (kind & 3, side index); both read as bytes
    {"0x4FB790", 0x4FB790, 0x4FB790, 2, {kU8, kU8}, mh::Answer::kGarbage, 0, 0},
    // MAGIC082's (group S18): a disc of radius (word) at the task
    {"0x4C12F0", 0x4C12F0, 0x4C12F0, 1, {kU16}, mh::Answer::kGarbage, 0, 0},
};
#undef S19_CUSTOM_OURS

const mh::DataTable kTables[] = {
    {0x65B47C, 1},   // ShieldAura_Types
    {0x65B480, 5},   // ShieldAura_Phases
    {0x65B494, 1},   // ShieldSpark_Types
    {0x65B498, 5},   // ShieldSpark_Phases
    {0x65B4AC, 3},   // BarrierPart_Types
    {0x65B4B8, 3},   // BarrierDisc_Phases
    {0x65B4C4, 6},   // BarrierRing_Phases
    {0x65B4DC, 3},   // BarrierLine_Phases
};

const mh::Region kRegions[] = {
    {kPool, kPoolCount * kPoolStride},   // the crystals' pool
    {kTints, 0xC00},                     // MoveScript_TintRecords, all 256 (the index is a byte)
    {0x904B80, 4},                       // the action id Shield_Kind rewrites
    {0x9037A0, 0x20},                    // the four GTE input vectors
    {0x903850, 0x10},                    // the effects' scratch words
    {0x80E980, 0x40},                    // the CLUT rows Shield_Start copies from
    {0x812980, 0x40},                    // and to
    {0x7E0670, 4},                       // Gfx_PacketNext
    {0x65B404, 0x78},                    // MAGIC083's colour tables
    {0, sizeof g_prims},                 // g_prims (set in SelfTest)
};
mh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seeds -----------------------------------------------------------------

unsigned char g_colours[0x78];   // the exe's colour tables, read at start-up

void OnPool() {
    if (mh::Half()) Sprite_Current = Pool(mh::Next());
}
unsigned char Near(unsigned edge) { return static_cast<unsigned char>(mh::Often() ? edge : mh::Next()); }

void Seed(unsigned k) {
    Gfx_PacketNext = g_prims + 0x800 + (mh::Next() & 0x3FC);
    // The pool's owners where the recorders write through them (a crystal's
    // owner is an aura task).
    for (unsigned i = 0; i < kPoolCount; ++i) {
        const std::uint32_t v = mh::Next();
        mh::SetPointer(kPool + i * kPoolStride + 0x80, v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v));
    }
    if (mh::Often()) std::memcpy(mh::Mem(0x65B404), g_colours, sizeof g_colours);
    unsigned char* sc = Sprite_Current;
    switch (k) {
    case kShieldTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kShieldStart:
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Next() & 0xC3);
        break;
    case kShieldKind:
        mh::Mem(0x904B35)[0] = static_cast<unsigned char>(mh::Half() ? 4 : MH_PICK(0, 1, 2, 3, 5, 0x84));
        move_script::SetWord(mh::Mem(0x904B80), MH_PICK(0x136, 0x136, 0x135, 0x137, 0x36, 0x236, 0x53));
        break;
    case kShieldAuraDispatch:
    case kShieldSparkDispatch:
        OnPool();
        Sprite_Current[1] = static_cast<unsigned char>(mh::Often() ? 0 : mh::Next() % 6);
        break;
    case kShieldAuraTask:
    case kShieldSparkTask:
        OnPool();
        Sprite_Current[2] = static_cast<unsigned char>(mh::Next() % 5);
        if (mh::Half()) Sprite_Current[0] = 0;
        break;
    case kShieldAuraWait:
        sc[9] = Near(1);
        sc[3] = static_cast<unsigned char>(mh::Next() % 8);
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Half() ? 0x40 | (mh::Next() & 0x87) : mh::Next() & 0x87);
        if (!(mh::Mem(mh::at::kTarget)[0] & 0x40)) sc[3] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kShieldAuraRise:
        sc[9] = Near(0xF);
        if (mh::Half()) mh::Mem(kTints + 12 * sc[0xA] + 2)[0] = 8;
        break;
    case kShieldAuraFade:
        sc[9] = Near(1);
        sc[3] = static_cast<unsigned char>(mh::Next() % 8);
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Half() ? 0x40 | (mh::Next() & 0x87) : mh::Next() & 0x87);
        if (!(mh::Mem(mh::at::kTarget)[0] & 0x40)) sc[3] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Half()) mh::Mem(kTints + 12 * sc[0xA] + 2)[0] = 0;
        break;
    case kShieldAuraDrawDisc:
    case kShieldAuraDrawHalo:
    case kShieldSparkDrawCrystal:
        OnPool();
        if (mh::Often()) Sprite_Current[4] = static_cast<unsigned char>(mh::Next() % 4);
        break;
    case kShieldSparkPlace:
        OnPool();
        Sprite_Current[9] = Near(1);
        break;
    case kShieldSparkRise:
    case kShieldSparkFall:
        OnPool();
        Sprite_Current[0xA] = Near(1);
        break;
    case kShieldSparkSpin: {
        OnPool();
        unsigned char* const s = Sprite_Current;
        move_script::SetLong(s + 0xC, static_cast<std::int32_t>(MH_PICK(3, 4, 4, 5, 0x80000004u, 0xFFFFFFFFu)));
        s[0xA] = static_cast<unsigned char>(MH_PICK(1, 1, 4, 5, 6, 0));
        if (mh::Half()) s[0x10] = static_cast<unsigned char>((mh::Next() & 0xF8) | 7);
        s[3] = static_cast<unsigned char>(mh::Half() ? mh::Next() & 0x0F : mh::Next());
        break;
    }
    case kShieldSparkOrbit:
        OnPool();
        Sprite_Current[0xA] = Near(0xF);
        break;
    case kShieldSparkAlloc:
        for (unsigned i = 0; i < kPoolCount; ++i) Pool(i)[0] = static_cast<unsigned char>(Pool(i)[0] | 1);
        if (mh::Often()) Pool(mh::Next())[0] = static_cast<unsigned char>(Pool(mh::Next())[0] & 0xFE);
        if (mh::Half()) Pool(mh::Next())[0] = static_cast<unsigned char>(Pool(mh::Next())[0] & 0xFE);
        break;
    case kBarrierTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 6);
        break;
    case kBarrierTint:
        sc[0xB] = Near(1);
        break;
    case kBarrierWaitRings:
        sc[0xB] = Near(2);
        break;
    case kBarrierFade:
        if (mh::Often()) mh::Mem(kTints + 12 * sc[0xA] + 2)[0] = 1;
        break;
    case kBarrierPartDispatch:
        sc[1] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kBarrierDiscTask:
    case kBarrierLineTask:
        sc[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        break;
    case kBarrierRingTask:
        sc[2] = static_cast<unsigned char>(mh::Next() % 6);
        if (mh::Half()) sc[0] = 0;
        break;
    case kBarrierDiscGrow:
    case kBarrierRingGrow:
        sc[9] = Near(0xF);
        break;
    case kBarrierDiscWait:
        mh::Pointer(mh::at::kOwner)[0xB] = Near(2);
        break;
    case kBarrierLineWait:
        mh::Pointer(mh::at::kOwner)[0xB] = Near(1);
        break;
    case kBarrierDiscShrink:
    case kBarrierLineDelay:
        sc[9] = Near(1);
        break;
    case kBarrierRingRise:
        sc[9] = Near(0x17);
        break;
    case kBarrierRingLift:
        sc[0xA] = Near(0x15);
        break;
    case kBarrierRingHold:
        sc[0xB] = Near(1);
        break;
    case kBarrierRingSettle:
        sc[9] = Near(0x11);
        break;
    case kBarrierRingEnd:
        if (mh::Half()) sc[9] = 0;
        sc[0xA] = Near(1);
        break;
    case kBarrierRingDraw:
        if (mh::Often()) sc[2] = 0;
        break;
    case kBarrierLineFly:
        move_script::SetLong(sc + 0x14, static_cast<std::int32_t>(-16 + static_cast<int>(mh::Next() % 5) - 2));
        move_script::SetLong(sc + 0x20, static_cast<std::int32_t>(static_cast<int>(mh::Next() % 4) - 2));
        break;
    default:
        break;
    }
}

// A cell of the group's moved after a call: a crystal slot's live bit, a
// level of the current slot's tint record, the action id.
void Disturb(std::uint32_t h) {
    switch ((h >> 8) % 3) {
    case 0: Pool(h >> 12)[0] = static_cast<unsigned char>(Pool(h >> 12)[0] ^ 1); break;
    case 1: mh::Mem(kTints + 12 * Sprite_Current[0xA] + 2 + (h >> 12) % 3)[0] = static_cast<unsigned char>(h >> 16); break;
    default: mh::Mem(0x904B80)[0] = static_cast<unsigned char>(h >> 16); break;
    }
}

}  // namespace

void SelfTest() {
    std::memcpy(g_colours, mh::Mem(0x65B404), sizeof g_colours);
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[sizeof kRegions / sizeof kRegions[0] - 1].at = Key(g_prims);
    const mh::Group group = {
        "magic_s19", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed, &Disturb, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_s19
