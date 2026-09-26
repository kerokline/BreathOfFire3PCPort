// BOF3X_SHADOW=magic_s20: group S20's three overlays (MAGIC087, MAGIC088,
// MAGIC092) through the spell round's shared harness (magic_harness.h), once
// at start-up. docs/magic_s20.md section 4.
//
// The clone tables (tools/magic_rows.py --unit MAGIC087 / 088 / 092
// --clones), the callees the standard set lacks - the PSX library layer the
// draws call, the overlays' own functions each other calls, and three other
// groups' addresses called raw -, the thirteen .data dispatch tables, the
// regions beyond the standard ones (the two mote pools, the tint records,
// the scratch, the CLUT rows, a packet buffer of the fuzz's own), and a seed
// per function.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s20.h"

namespace magic_s20 {
namespace {

namespace mh = magic_harness;

// tools/magic_rows.py --unit MAGIC087 / MAGIC088 / MAGIC092 --clones,
// 2026-09-25: 22 + 16 + 13 functions; one jump table (Magic088_Variant's,
// four entries, moved into its copy; its byte table stays read in place).
// 0x4C3490 Magic087_Task: 0x75 bytes
constexpr mh::CallSite kCalls4C3490[] = {{0x53, 0x4C3CC0}};
constexpr mh::Imm kImms4C3490[] = {{0x16, 0x4C3510}, {0x1E, 0x4F7350}};
// 0x4C3510 Magic087_Start: 0x1E7 bytes
constexpr mh::CallSite kCalls4C3510[] = {{0x4A, 0x4456C0}, {0x5E, 0x435180}, {0x119, 0x4456C0}, {0x129, 0x435180}};
// 0x4C3700 Magic087_Child: 0x12 bytes; +0xB note: jmp through .data 0x65b4e8, 16 code entries (a data_tables entry)
// 0x4C3720 Magic087_ChildPhase: 0x12 bytes; +0xB note: jmp through .data 0x65b4ec, 15 code entries (a data_tables entry)
// 0x4C3740 Magic087_ChildSpawn: 0xFB bytes
constexpr mh::CallSite kCalls4C3740[] = {{0x15, 0x4C4430}, {0x78, 0x4C4430}, {0xEF, 0x587900}};
// 0x4C3840 Magic087_ChildWait: 0x2B bytes
// 0x4C3870 Magic087_ChildRing: 0xEE bytes
constexpr mh::CallSite kCalls4C3870[] = {{0x12, 0x587900}, {0x37, 0x4C4430}, {0x67, 0x4C4430}, {0xD7, 0x4FBD10}, {0xDE, 0x4C39C0}, {0xE5, 0x4C39C0}};
// 0x4C3960 Magic087_ChildGrow: 0x36 bytes
constexpr mh::CallSite kCalls4C3960[] = {{0x1F, 0x4FBD10}, {0x26, 0x4C39C0}, {0x2D, 0x4C39C0}};
// 0x4C39A0 Magic087_ChildEnd: 0x19 bytes
constexpr mh::CallSite kCalls4C39A0[] = {{0x13, 0x4351F0}};
// 0x4C39C0 Magic087_DrawColumn: 0x2FA bytes
constexpr mh::CallSite kCalls4C39C0[] = {{0x12, 0x5A77C0}, {0x28, 0x572FA0}, {0xA0, 0x5A7630}, {0xA8, 0x5A7780}, {0xCB, 0x5A7A00}, {0x149, 0x5A7A00}, {0x230, 0x5A79A0}, {0x240, 0x5A79E0}, {0x2C1, 0x572FA0}};
// 0x4C3CC0 Magic087_MoteRun: 0x12 bytes; +0xB note: jmp through .data 0x65b500, 10 code entries (a data_tables entry)
// 0x4C3CE0 Magic087_OrbitRun: 0x37 bytes; +0xB note: call through .data 0x65b510, 6 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C3CE0[] = {{0x27, 0x4C4050}, {0x2C, 0x4C4110}, {0x31, 0x5A7BC0}};
// 0x4C3D20 Magic087_OrbitStart: 0x63 bytes
// 0x4C3D90 Magic087_OrbitSpin: 0x10F bytes
constexpr mh::CallSite kCalls4C3D90[] = {{0x4E, 0x5A7A00}, {0x77, 0x5A7A50}};
// 0x4C3EA0 Magic087_OrbitWait: 0x14 bytes
// 0x4C3EC0 Magic087_OrbitFade: 0x51 bytes
// 0x4C3F20 Magic087_RiseRun: 0x33 bytes; +0xB note: call through .data 0x65b520, 2 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C3F20[] = {{0x23, 0x4C4050}, {0x28, 0x4C4110}, {0x2D, 0x5A7BC0}};
// 0x4C3F60 Magic087_RiseStart: 0x7E bytes
// 0x4C3FE0 Magic087_RiseStep: 0x69 bytes
// 0x4C4050 Magic087_PushMatrix: 0xB2 bytes
constexpr mh::CallSite kCalls4C4050[] = {{0x3, 0x5A7B90}, {0x72, 0x5A8200}, {0x81, 0x5A8060}, {0x95, 0x5A7D70}, {0x9F, 0x5A8DE0}, {0xA9, 0x5A8E00}};
// 0x4C4110 Magic087_DrawTriangle: 0x319 bytes
constexpr mh::CallSite kCalls4C4110[] = {{0x19, 0x5A77C0}, {0x2F, 0x572FA0}, {0x3B, 0x5A76D0}, {0x42, 0x5A7780}, {0x78, 0x5A7A00}, {0x92, 0x5A7A50}, {0xB3, 0x5A7A00}, {0xCD, 0x5A7A50}, {0xEE, 0x5A7A00}, {0x108, 0x5A7A50}, {0x149, 0x5A84A0}, {0x14F, 0x5A9420}, {0x1BC, 0x572FA0}, {0x1CB, 0x5A76D0}, {0x1D2, 0x5A7780}, {0x1DC, 0x5A7A00}, {0x1F6, 0x5A7A50}, {0x217, 0x5A7A00}, {0x231, 0x5A7A50}, {0x252, 0x5A7A00}, {0x26C, 0x5A7A50}, {0x2AD, 0x5A84A0}, {0x2B6, 0x5A9420}, {0x309, 0x572FA0}};
// 0x4C4430 Magic087_PoolAlloc: 0x57 bytes
// 0x4C4490 Magic088_Task: 0x4E bytes
constexpr mh::Imm kImms4C4490[] = {{0xF, 0x4C44E0}, {0x17, 0x4C4650}, {0x22, 0x4C46B0}, {0x2A, 0x4EF7C0}, {0x32, 0x4C4700}, {0x3A, 0x4C4780}, {0x42, 0x4E5200}};
// 0x4C44E0 Magic088_Start: 0x165 bytes
constexpr mh::CallSite kCalls4C44E0[] = {{0x2B, 0x4C4830}, {0x59, 0x435180}, {0xBB, 0x435180}, {0x15C, 0x587900}};
// 0x4C4650 Magic088_TintOn: 0x58 bytes
constexpr mh::CallSite kCalls4C4650[] = {{0x25, 0x454DC0}, {0x33, 0x454CC0}};
// 0x4C46B0 Magic088_Darken: 0x4D bytes
// 0x4C4700 Magic088_Lighten: 0x74 bytes
constexpr mh::CallSite kCalls4C4700[] = {{0x57, 0x454DC0}, {0x63, 0x4FBDB0}};
// 0x4C4780 Magic088_Apply: 0xAD bytes
constexpr mh::CallSite kCalls4C4780[] = {{0x23, 0x4FB6F0}, {0x33, 0x435180}, {0x64, 0x435180}};
// 0x4C4830 Magic088_Variant: 0xFF bytes
constexpr mh::JumpTable kTables4C4830[] = {{0x28, 0x4C, 4}};
// 0x4C4930 Magic088_Child: 0x12 bytes; +0xB note: jmp through .data 0x65b574, 5 code entries (a data_tables entry)
// 0x4C4950 Magic088_FanRun: 0x2D bytes; +0xB note: call through .data 0x65b57c, 3 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C4950[] = {{0x1D, 0x4B7D40}, {0x22, 0x4C4980}, {0x27, 0x5A7BC0}};
// 0x4C4980 Magic088_DrawFan: 0x1A5 bytes
constexpr mh::CallSite kCalls4C4980[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x49, 0x5A7A00}, {0x62, 0x5A7A50}, {0xAD, 0x5A7A00}, {0xC6, 0x5A7A50}, {0xFA, 0x5A75F0}, {0x101, 0x5A7780}, {0x12B, 0x5A84A0}, {0x131, 0x5A9310}, {0x166, 0x461E50}, {0x18C, 0x5A77C0}, {0x195, 0x461E50}};
// 0x4C4B30 Magic088_WaveRun: 0x38 bytes; +0xB note: call through .data 0x65b5a8, 43 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C4B30[] = {{0x23, 0x4B7D40}, {0x28, 0x4C4BE0}, {0x2D, 0x4C4C00}, {0x32, 0x5A7BC0}};
// 0x4C4B70 Magic088_WaveGrow: 0x1C bytes
// 0x4C4B90 Magic088_WaveHold: 0x2F bytes
// 0x4C4BC0 Magic088_WaveFade: 0x20 bytes
constexpr mh::CallSite kCalls4C4BC0[] = {{0x1A, 0x4351F0}};
// 0x4C4BE0 Magic088_WaveStep: 0x20 bytes
// 0x4C4C00 Magic088_DrawWave: 0x3B2 bytes
constexpr mh::CallSite kCalls4C4C00[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0xF5, 0x5A7A50}, {0x10F, 0x5A7A00}, {0x12F, 0x5A7A00}, {0x164, 0x5A7A50}, {0x17E, 0x5A7A00}, {0x19E, 0x5A7A00}, {0x22C, 0x5A7A50}, {0x24C, 0x5A7A00}, {0x26C, 0x5A7A00}, {0x2AB, 0x5A7A50}, {0x2CB, 0x5A7A00}, {0x2EB, 0x5A7A00}, {0x314, 0x5A75B0}, {0x31C, 0x5A7780}, {0x34F, 0x5A85F0}, {0x358, 0x5A9240}, {0x388, 0x572FA0}};
// 0x4C5680 Magic092_Task: 0x75 bytes
constexpr mh::CallSite kCalls4C5680[] = {{0x53, 0x4C5B10}};
constexpr mh::Imm kImms4C5680[] = {{0x16, 0x4C5700}, {0x1E, 0x4E5200}};
// 0x4C5700 Magic092_Start: 0x1A0 bytes
constexpr mh::CallSite kCalls4C5700[] = {{0x48, 0x4456C0}, {0x58, 0x435180}, {0xDF, 0x4456C0}, {0xEF, 0x435180}};
// 0x4C58A0 Magic092_Child: 0x12 bytes; +0xB note: jmp through .data 0x65b5c8, 35 code entries (a data_tables entry)
// 0x4C58C0 Magic092_ChildRun: 0x33 bytes; +0xB note: call through .data 0x65b5cc, 34 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C58C0[] = {{0x23, 0x4B7D40}, {0x28, 0x4E9420}, {0x2D, 0x5A7BC0}};
// 0x4C5900 Magic092_ChildSpawn: 0xA5 bytes
constexpr mh::CallSite kCalls4C5900[] = {{0x25, 0x4C62A0}, {0x8D, 0x587900}};
// 0x4C59B0 Magic092_ChildTint: 0xAF bytes
constexpr mh::CallSite kCalls4C59B0[] = {{0x3E, 0x454DC0}, {0x4C, 0x454CC0}, {0x7A, 0x454DC0}, {0x88, 0x454CC0}, {0x98, 0x452F70}};
// 0x4C5A60 Magic092_ChildEnd: 0xAE bytes
constexpr mh::CallSite kCalls4C5A60[] = {{0x3E, 0x454DC0}, {0x4F, 0x4FBDB0}, {0x7B, 0x454DC0}, {0x8A, 0x4FBDB0}, {0x98, 0x4530D0}, {0xA8, 0x4351F0}};
// 0x4C5B10 Magic092_MoteRun: 0x12 bytes; +0xB note: jmp through .data 0x65b5dc, 30 code entries (a data_tables entry)
// 0x4C5B30 Magic092_FlameRun: 0x2E bytes; +0xB note: call through .data 0x65b5e4, 28 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C5B30[] = {{0x23, 0x4FBD10}, {0x28, 0x4C5BA0}};
// 0x4C5B60 Magic092_FlameGrow: 0x3C bytes
// 0x4C5BA0 Magic092_DrawFlame: 0x6AA bytes
constexpr mh::CallSite kCalls4C5BA0[] = {{0x6F, 0x5A77C0}, {0x85, 0x572FA0}, {0x8E, 0x5A7A00}, {0xA7, 0x5A7A00}, {0xC0, 0x5A7A00}, {0xD8, 0x5B93D2}, {0xF9, 0x5A7A00}, {0x118, 0x5A7630}, {0x120, 0x5A7780}, {0x1E8, 0x5A79E0}, {0x1FF, 0x5A79A0}, {0x264, 0x572FA0}, {0x270, 0x5A7630}, {0x278, 0x5A7780}, {0x336, 0x5A79E0}, {0x34D, 0x5A79A0}, {0x39E, 0x572FA0}, {0x3AA, 0x5A7630}, {0x3B2, 0x5A7780}, {0x484, 0x5A79E0}, {0x49B, 0x5A79A0}, {0x513, 0x572FA0}, {0x522, 0x5A7630}, {0x52A, 0x5A7780}, {0x5FC, 0x5A79E0}, {0x613, 0x5A79A0}, {0x675, 0x572FA0}};
// 0x4C6250 Magic092_SparkRun: 0x4F bytes; +0xB note: call through .data 0x65b5f4, 24 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C6250[] = {{0x23, 0x4FBD10}, {0x49, 0x4E9850}};
// 0x4C62A0 Magic092_PoolAlloc: 0x57 bytes
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Magic087_Task", 0x4C3490, 0x75, kCalls4C3490, MH_N(kCalls4C3490), kImms4C3490, MH_N(kImms4C3490), nullptr, 0, reinterpret_cast<const void*>(&::Magic087_Task)},
    {"Magic087_Start", 0x4C3510, 0x1E7, kCalls4C3510, MH_N(kCalls4C3510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_Start)},
    {"Magic087_Child", 0x4C3700, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_Child)},
    {"Magic087_ChildPhase", 0x4C3720, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildPhase)},
    {"Magic087_ChildSpawn", 0x4C3740, 0xFB, kCalls4C3740, MH_N(kCalls4C3740), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildSpawn)},
    {"Magic087_ChildWait", 0x4C3840, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildWait)},
    {"Magic087_ChildRing", 0x4C3870, 0xEE, kCalls4C3870, MH_N(kCalls4C3870), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildRing)},
    {"Magic087_ChildGrow", 0x4C3960, 0x36, kCalls4C3960, MH_N(kCalls4C3960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildGrow)},
    {"Magic087_ChildEnd", 0x4C39A0, 0x19, kCalls4C39A0, MH_N(kCalls4C39A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_ChildEnd)},
    {"Magic087_DrawColumn", 0x4C39C0, 0x2FA, kCalls4C39C0, MH_N(kCalls4C39C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_DrawColumn)},
    {"Magic087_MoteRun", 0x4C3CC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_MoteRun)},
    {"Magic087_OrbitRun", 0x4C3CE0, 0x37, kCalls4C3CE0, MH_N(kCalls4C3CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_OrbitRun)},
    {"Magic087_OrbitStart", 0x4C3D20, 0x63, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_OrbitStart)},
    {"Magic087_OrbitSpin", 0x4C3D90, 0x10F, kCalls4C3D90, MH_N(kCalls4C3D90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_OrbitSpin)},
    {"Magic087_OrbitWait", 0x4C3EA0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_OrbitWait)},
    {"Magic087_OrbitFade", 0x4C3EC0, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_OrbitFade)},
    {"Magic087_RiseRun", 0x4C3F20, 0x33, kCalls4C3F20, MH_N(kCalls4C3F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_RiseRun)},
    {"Magic087_RiseStart", 0x4C3F60, 0x7E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_RiseStart)},
    {"Magic087_RiseStep", 0x4C3FE0, 0x69, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_RiseStep)},
    {"Magic087_PushMatrix", 0x4C4050, 0xB2, kCalls4C4050, MH_N(kCalls4C4050), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_PushMatrix)},
    {"Magic087_DrawTriangle", 0x4C4110, 0x319, kCalls4C4110, MH_N(kCalls4C4110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_DrawTriangle)},
    {"Magic087_PoolAlloc", 0x4C4430, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic087_PoolAlloc)},
    {"Magic088_Task", 0x4C4490, 0x4E, nullptr, 0, kImms4C4490, MH_N(kImms4C4490), nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Task)},
    {"Magic088_Start", 0x4C44E0, 0x165, kCalls4C44E0, MH_N(kCalls4C44E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Start)},
    {"Magic088_TintOn", 0x4C4650, 0x58, kCalls4C4650, MH_N(kCalls4C4650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_TintOn)},
    {"Magic088_Darken", 0x4C46B0, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Darken)},
    {"Magic088_Lighten", 0x4C4700, 0x74, kCalls4C4700, MH_N(kCalls4C4700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Lighten)},
    {"Magic088_Apply", 0x4C4780, 0xAD, kCalls4C4780, MH_N(kCalls4C4780), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Apply)},
    {"Magic088_Variant", 0x4C4830, 0xFF, nullptr, 0, nullptr, 0, kTables4C4830, MH_N(kTables4C4830), reinterpret_cast<const void*>(&::Magic088_Variant)},
    {"Magic088_Child", 0x4C4930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_Child)},
    {"Magic088_FanRun", 0x4C4950, 0x2D, kCalls4C4950, MH_N(kCalls4C4950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_FanRun)},
    {"Magic088_DrawFan", 0x4C4980, 0x1A5, kCalls4C4980, MH_N(kCalls4C4980), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_DrawFan)},
    {"Magic088_WaveRun", 0x4C4B30, 0x38, kCalls4C4B30, MH_N(kCalls4C4B30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_WaveRun)},
    {"Magic088_WaveGrow", 0x4C4B70, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_WaveGrow)},
    {"Magic088_WaveHold", 0x4C4B90, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_WaveHold)},
    {"Magic088_WaveFade", 0x4C4BC0, 0x20, kCalls4C4BC0, MH_N(kCalls4C4BC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_WaveFade)},
    {"Magic088_WaveStep", 0x4C4BE0, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_WaveStep)},
    {"Magic088_DrawWave", 0x4C4C00, 0x3B2, kCalls4C4C00, MH_N(kCalls4C4C00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic088_DrawWave)},
    {"Magic092_Task", 0x4C5680, 0x75, kCalls4C5680, MH_N(kCalls4C5680), kImms4C5680, MH_N(kImms4C5680), nullptr, 0, reinterpret_cast<const void*>(&::Magic092_Task)},
    {"Magic092_Start", 0x4C5700, 0x1A0, kCalls4C5700, MH_N(kCalls4C5700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_Start)},
    {"Magic092_Child", 0x4C58A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_Child)},
    {"Magic092_ChildRun", 0x4C58C0, 0x33, kCalls4C58C0, MH_N(kCalls4C58C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_ChildRun)},
    {"Magic092_ChildSpawn", 0x4C5900, 0xA5, kCalls4C5900, MH_N(kCalls4C5900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_ChildSpawn)},
    {"Magic092_ChildTint", 0x4C59B0, 0xAF, kCalls4C59B0, MH_N(kCalls4C59B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_ChildTint)},
    {"Magic092_ChildEnd", 0x4C5A60, 0xAE, kCalls4C5A60, MH_N(kCalls4C5A60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_ChildEnd)},
    {"Magic092_MoteRun", 0x4C5B10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_MoteRun)},
    {"Magic092_FlameRun", 0x4C5B30, 0x2E, kCalls4C5B30, MH_N(kCalls4C5B30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_FlameRun)},
    {"Magic092_FlameGrow", 0x4C5B60, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_FlameGrow)},
    {"Magic092_DrawFlame", 0x4C5BA0, 0x6AA, kCalls4C5BA0, MH_N(kCalls4C5BA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_DrawFlame)},
    {"Magic092_SparkRun", 0x4C6250, 0x4F, kCalls4C6250, MH_N(kCalls4C6250), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_SparkRun)},
    {"Magic092_PoolAlloc", 0x4C62A0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic092_PoolAlloc)},
};
#undef MH_N

// Keys for the callees that are ours (the pointer ours calls).
template <typename F> std::uint32_t KeyOf(F f) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reinterpret_cast<const void*>(f)));
}
#define S20_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S20_RAW(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;

// The callees the standard set lacks. The draws' arguments are all pushed as
// whole dwords (constants, pointers into the packet or the vertex scratch);
// stack addresses are masked off (0) and what they point at is logged with
// LogPointee where the callee reads it.
const mh::Callee kCallees[] = {
    {S20_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S20_OURS(Sprite_SetTint), 4, {kAll, kU8, kU8, kU8}, mh::Answer::kByte, 0, 0xFF},
    {S20_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_SetPolyGT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_SetPolyF4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_SetLineG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_RotTrans), 2, {0, 0}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_RotMatrix), 2, {0, 0}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_SetRotMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_SetTransMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_RotTransPers3), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_RotTransPers4), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_PrimDepths3_10C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Gte_PrimDepths4_0C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    // the group's own, called directly by its others
    {S20_OURS(Magic087_MoteRun), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic087_PoolAlloc), 0, {}, mh::Answer::kByte, 0, 0xFF},
    {S20_OURS(Magic087_DrawColumn), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic087_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic087_DrawTriangle), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic088_Variant), 0, {}, mh::Answer::kByte, 0, 3},
    {S20_OURS(Magic088_DrawFan), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic088_WaveStep), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic088_DrawWave), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic092_MoteRun), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic092_DrawFlame), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_OURS(Magic092_PoolAlloc), 0, {}, mh::Answer::kByte, 0, 0xFF},
    // other groups' addresses, called raw: the effect library's stat change
    // (group L) and MAGIC144's two draws
    {S20_RAW(0x4FB6F0), 2, {kU8, kU8}, mh::Answer::kFlag, 0, 0},
    {S20_RAW(0x4E9420), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S20_RAW(0x4E9850), 0, {}, mh::Answer::kGarbage, 0, 0},
};
#undef S20_OURS
#undef S20_RAW

// The .data dispatch tables, their entries as far as each overlay reaches
// (docs/magic_s20.md section 2); contiguous runs let a seed step one past.
const mh::DataTable kTables[] = {
    {0x65B4E8, 1}, {0x65B4EC, 5}, {0x65B500, 4}, {0x65B510, 4}, {0x65B520, 2},   // MAGIC087
    {0x65B574, 2}, {0x65B57C, 3}, {0x65B5A8, 4},                                // MAGIC088
    {0x65B5C8, 1}, {0x65B5CC, 4}, {0x65B5DC, 2}, {0x65B5E4, 4}, {0x65B5F4, 4},   // MAGIC092
};

enum : unsigned {
    kM087Task, kM087Start, kM087Child, kM087ChildPhase, kM087ChildSpawn, kM087ChildWait, kM087ChildRing,
    kM087ChildGrow, kM087ChildEnd, kM087DrawColumn, kM087MoteRun, kM087OrbitRun, kM087OrbitStart, kM087OrbitSpin,
    kM087OrbitWait, kM087OrbitFade, kM087RiseRun, kM087RiseStart, kM087RiseStep, kM087PushMatrix,
    kM087DrawTriangle, kM087PoolAlloc,
    kM088Task, kM088Start, kM088TintOn, kM088Darken, kM088Lighten, kM088Apply, kM088Variant, kM088Child,
    kM088FanRun, kM088DrawFan, kM088WaveRun, kM088WaveGrow, kM088WaveHold, kM088WaveFade, kM088WaveStep,
    kM088DrawWave,
    kM092Task, kM092Start, kM092Child, kM092ChildRun, kM092ChildSpawn, kM092ChildTint, kM092ChildEnd,
    kM092MoteRun, kM092FlameRun, kM092FlameGrow, kM092DrawFlame, kM092SparkRun, kM092PoolAlloc,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one seed case per clone");

constexpr std::uint32_t kPools = 0x6906D8;     // pool A (80) then pool B (48) at 0x693018
constexpr std::uint32_t kPoolB = 0x693018;
constexpr std::uint32_t kStride = 0x84;
// The pools and 256 slots past pool B's start: an alloc's stand-in answers
// any byte (the original's 0xFF included), and the untested ones write there.
constexpr std::uint32_t kPoolsSize = (kPoolB - kPools) + 0x100 * kStride;
constexpr std::uint32_t kTints = 0x7E0700;     // MoveScript_TintRecords, 12 bytes; any byte indexes them
constexpr std::uint32_t kAbility = 0x904B80;

// The fuzz's own packet buffer: Gfx_PacketNext points here every round (the
// stand-ins never move it).
alignas(16) unsigned char g_packet[0x60];

const mh::Region kRegions[] = {
    {kPools, kPoolsSize},
    {0x7E0670, 4},                      // Gfx_PacketNext
    {kTints, 0x100 * 12},
    {0x903850, 0x10},                   // the scratch
    {0x9037A0, 0x20},                   // Prim_VertexScratch
    {0x80B580, 0x20},                   // Gfx_ClutStripSource (MAGIC087's row)
    {0x80E980, 0x40},                   // 0x4000 below the rows (MAGIC088's, MAGIC092's)
    {0x812980, 0x40},                   // CLUT strip rows 26 and 27
    {0x65B588, 0x18},                   // MAGIC088's six band phases
    {kAbility, 4},
    {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_packet)), sizeof g_packet},
};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolAt(std::uint32_t base, unsigned i) { return mh::Mem(base + i * kStride); }
std::uint8_t Byte(unsigned v) { return static_cast<std::uint8_t>(v); }

// A pool whose every mote but one (or none) is in use: the alloc's answer.
void FillPool(std::uint32_t base, unsigned count) {
    const unsigned free_at = mh::Half() ? mh::Next() % count : count;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const m = PoolAt(base, i);
        m[0] = static_cast<unsigned char>(i == free_at ? m[0] & ~1u : m[0] | 1u);
        if (i > free_at && mh::Half()) m[0] = static_cast<unsigned char>(m[0] & ~1u);
    }
}

void Seed(unsigned k) {
    Gfx_PacketNext = g_packet;
    // Every mote's owner a real record: the walks put it in 0x93B940, and the
    // stand-ins may write through it.
    for (unsigned i = 0; i < 0x80; ++i)
        mh::SetPointer(kPools + i * kStride + 0x80, mh::Half() ? mh::TaskAt(mh::Next()) : mh::SpriteRecord(mh::Next()));
    unsigned char* const sc = Sc();
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    // The target's side bit (0x40: the enemies) for the four that test it
    // (the harness's disturbance leaves the enemy records alone while the
    // byte is past 10).
    if ((k == kM087Start || k == kM092Start || k == kM092ChildTint || k == kM092ChildEnd) && mh::Half())
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(0x40 | mh::Next() % 11);
    switch (k) {
    case kM087Task:
    case kM092Task: sc[1] = Byte(mh::Next() % 2); break;
    case kM088Task: sc[1] = Byte(mh::Next() % 7); break;
    case kM087Child: sc[1] = Byte(mh::Often() ? 0 : mh::Next() % 6); break;
    case kM087ChildPhase: sc[2] = Byte(mh::Next() % 5); break;
    case kM087MoteRun: sc[1] = Byte(mh::Next() % 4); break;
    case kM087OrbitRun: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kM087RiseRun: sc[2] = Byte(mh::Next() % 2); if (mh::Half()) sc[0] = 0; break;
    case kM088Child: sc[1] = Byte(mh::Next() % 2); break;
    case kM088FanRun: sc[2] = Byte(mh::Next() % 3); if (mh::Half()) sc[0] = 0; break;
    case kM088WaveRun: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kM092Child: sc[1] = Byte(mh::Often() ? 0 : mh::Next() % 12); break;
    case kM092ChildRun: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kM092MoteRun: sc[1] = Byte(mh::Next() % 2); break;
    case kM092FlameRun: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kM092SparkRun:
        sc[2] = Byte(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        if (mh::Half()) sc[0xB] = 1;
        break;
    case kM087ChildSpawn: if (mh::Half()) sc[4] = 0; break;
    case kM087ChildWait: if (mh::Half()) sc[0xB] = 2; break;
    case kM087ChildRing:
        if (mh::Half()) sc[4] = 0;
        if (mh::Often()) sc[9] = 1;
        if (mh::Half()) sc[0xA] = 1;
        break;
    case kM087ChildGrow: if (mh::Half()) sc[9] = 0xF; sc[2] = Byte(mh::Next() % 5); break;
    case kM087ChildEnd: if (mh::Half()) sc[0xB] = 0x80; break;
    case kM087DrawColumn:
        sc[4] = Byte(MH_PICK(0, 1, 0xE, 0xF, 0x10, 0x11, 0x30));
        if (mh::Half()) sc[2] = 3;
        sc[9] = Byte(mh::Next() % 20);
        break;
    case kM087OrbitStart: if (mh::Half()) sc[9] = 1; if (mh::Half()) sc[1] = 0; break;
    case kM087OrbitSpin: {
        if (mh::Half()) {
            // the radius reaching 0: +0xC equal to +0x18
            std::memcpy(sc + 0xC, sc + 0x18, 4);
        }
        if (mh::Half()) sc[4] = 0;
        sc[0xA] = Byte(MH_PICK(0x1F, 0x20, 0x21, 0x22, 0));
        break;
    }
    case kM087OrbitWait: if (mh::Half()) owner[0xB] = 0x82; break;
    case kM087OrbitFade:
    case kM087RiseStep:
    case kM088WaveHold: if (mh::Half()) sc[0xA] = 1; break;
    case kM087RiseStart: if (mh::Half()) sc[1] = 2; break;
    case kM087DrawTriangle: sc[1] = Byte(mh::Next() % 4); break;
    case kM087PoolAlloc: FillPool(kPools, 0x50); break;
    case kM092PoolAlloc: FillPool(kPoolB, 0x30); break;
    case kM088TintOn: if (mh::Half()) sc[9] = 1; break;
    case kM088Darken: if (mh::Half()) sc[9] = 1; break;
    case kM088Lighten: if (mh::Half()) mh::Mem(kTints + sc[0xA] * 12u)[2] = 0xFF; break;
    case kM088Apply: if (mh::Half()) sc[0xB] = 0; break;
    case kM088Variant: {
        mh::Mem(0x904B35)[0] = Byte(mh::Half() ? 4 : MH_PICK(3, 5, 0));
        const std::uint32_t id = MH_PICK(0x1D, 0x1E, 0x1F, 0x20, 0x58, 0x59, 0x5A, 0x5B, 0xBE, 0xBF, 0xC0, 0xC1,
                                         0x111, 0x112, 0x113, 0x10000 + 0x112);
        const std::uint32_t word = mh::Half() ? id : (mh::Next() & 0xFFFF0000u) | (id & 0xFFFF);
        std::memcpy(mh::Mem(kAbility), &word, sizeof word);
        break;
    }
    case kM088WaveGrow: if (mh::Half()) sc[9] = 0x1F; break;
    case kM088WaveFade: if (mh::Half()) sc[9] = 2; break;
    case kM092ChildSpawn: if (mh::Half()) sc[9] = 1; FillPool(kPoolB, 0x30); break;
    case kM092ChildTint: if (mh::Half()) sc[9] = 0xE; break;
    case kM092ChildEnd:
        if (mh::Half()) sc[9] = 0;
        if (mh::Half()) sc[0xB] = 0x80;
        break;
    case kM092FlameGrow: if (mh::Half()) sc[9] = 0x18; break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // What the draws hand the matrix calls on their stack: the vector and
    // the angles (six bytes each; the original leaves the pads unwritten).
    mh::LogPointee(bof3::addr::Gte_RotTrans, 0, 6);
    mh::LogPointee(bof3::addr::Gte_RotMatrix, 0, 6);
    // The vertices each projection reads, as it is called.
    for (unsigned arg = 0; arg < 3; ++arg) mh::LogPointee(bof3::addr::Gte_RotTransPers3, arg, 6);
    for (unsigned arg = 0; arg < 4; ++arg) mh::LogPointee(bof3::addr::Gte_RotTransPers4, arg, 6);
    // The three that answer something their callers read.
    mh::LogReturn(bof3::addr::Magic087_PoolAlloc, 0xFF);
    mh::LogReturn(bof3::addr::Magic088_Variant, 0xFF);
    mh::LogReturn(bof3::addr::Magic092_PoolAlloc, 0xFF);
    const mh::Group group = {
        "magic_s20", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, nullptr, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_s20
