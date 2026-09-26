// BOF3X_SHADOW=magic_s22: group S22's four overlays (MAGIC096..099) through
// the spell round's shared harness (magic_harness.h), once at start-up, and
// three functions through a fuzz of their own. docs/magic_s22.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC096..099 --clones
// (2026-09-25; capstone, every jump internal but the jump table of
// BlizzardShard_Launch, which the harness moves into the copy), less the three
// the harness cannot compare:
//
//   - BlizzardShard_PushMatrix 0x4C91E0 and LightningBolt_PushMatrix 0x4CAE30
//     hand their vectors to the GTE by pointer - the harness logs arguments,
//     not what they point at, so a wrong vector would pass;
//   - Blizzard_CenterOnTargets 0x4C9AF0 divides by the count of the actors
//     Battle_ActorIsOut says are in, and the harness's recorder answers
//     "out" for every one of them often enough to fault both sides.
//
// Those three run below (SelfTestOwn) with recorders of this file's that read
// the vectors and answer from a mask with one actor in, reached from ours
// through magic_s22::g_hooks.
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s22.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s22 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC096..099 --clones, 2026-09-25, names given.
constexpr mh::Imm kImms4C8D40[] = {{0xF, 0x4C8D70}, {0x17, 0x4C8E30}, {0x22, 0x4F7350}};
constexpr mh::CallSite kCalls4C8D70[] = {{0x2, 0x4C9AF0}, {0x7, 0x4FBD10}, {0x46, 0x435180}, {0xB2, 0x587900}};
constexpr mh::CallSite kCalls4C8E30[] = {{0x25, 0x452F70}};
constexpr mh::CallSite kCalls4C8E90[] = {{0x4B, 0x4C91E0}, {0x50, 0x4C9290}, {0x55, 0x5A7BC0}, {0x5A, 0x4B7D40}, {0x5F, 0x4C96A0}, {0x64, 0x4C9890}, {0x69, 0x5A7BC0}};
constexpr mh::Imm kImms4C8E90[] = {{0xF, 0x4C8F10}, {0x17, 0x4C9170}, {0x22, 0x4C2D90}, {0x2A, 0x4E47F0}, {0x32, 0x4C91B0}};
constexpr mh::CallSite kCalls4C8F10[] = {{0x57, 0x446770}, {0xBE, 0x5B93D2}, {0xD8, 0x5B93D2}, {0xE9, 0x5B93D2}, {0x101, 0x5B93D2}, {0x11E, 0x5B93D2}, {0x136, 0x5B93D2}, {0x147, 0x5B93D2}, {0x15F, 0x5B93D2}, {0x17C, 0x5B93D2}, {0x189, 0x5B93D2}, {0x1A9, 0x5B93D2}, {0x1BA, 0x5B93D2}, {0x1D2, 0x5B93D2}, {0x1EF, 0x5B93D2}, {0x1F8, 0x5B93D2}};
constexpr mh::JumpTable kTables4C8F10[] = {{0xB0, 0x248, 4}};
constexpr mh::CallSite kCalls4C91B0[] = {{0x21, 0x4351F0}};
constexpr mh::CallSite kCalls4C9290[] = {{0x88, 0x5A7A00}, {0xA1, 0x5A7A50}, {0xD1, 0x5A7A00}, {0xEA, 0x5A7A50}, {0x144, 0x5A77C0}, {0x14E, 0x572FA0}, {0x15A, 0x5A7590}, {0x16C, 0x5A79A0}, {0x17E, 0x5A79E0}, {0x1D1, 0x5A84A0}, {0x1D7, 0x5A9200}, {0x20D, 0x572FA0}, {0x267, 0x5A7A00}, {0x283, 0x5A7A50}, {0x2A9, 0x5A7A00}, {0x2C2, 0x5A7A50}, {0x313, 0x5A77C0}, {0x31D, 0x572FA0}, {0x329, 0x5A75D0}, {0x33C, 0x5A79A0}, {0x34F, 0x5A79E0}, {0x3A6, 0x5A85F0}, {0x3AC, 0x5A9290}, {0x3D1, 0x572FA0}};
constexpr mh::CallSite kCalls4C96A0[] = {{0x14, 0x5A77C0}, {0x2A, 0x572FA0}, {0x78, 0x5A7A00}, {0x91, 0x5A7A50}, {0xCD, 0x5A75F0}, {0xD5, 0x5A7780}, {0x103, 0x5A7A00}, {0x11C, 0x5A7A50}, {0x159, 0x5A84A0}, {0x15F, 0x5A9310}, {0x1C3, 0x572FA0}};
constexpr mh::CallSite kCalls4C9890[] = {{0x18, 0x5A77C0}, {0x2E, 0x572FA0}, {0x7F, 0x5A7A00}, {0x98, 0x5A7A50}, {0xB1, 0x5A7A00}, {0xCA, 0x5A7A50}, {0x106, 0x5A7610}, {0x10D, 0x5A7780}, {0x12D, 0x5A7A00}, {0x146, 0x5A7A50}, {0x179, 0x5A7A00}, {0x192, 0x5A7A50}, {0x1D8, 0x5A85F0}, {0x1E1, 0x5A9350}, {0x23D, 0x572FA0}};
constexpr mh::Imm kImms4C9C60[] = {{0xF, 0x4C9C90}, {0x17, 0x4E5200}};
constexpr mh::CallSite kCalls4C9C90[] = {{0x29, 0x4456C0}, {0x39, 0x435180}, {0xBD, 0x4456C0}, {0xCD, 0x435180}, {0x180, 0x587900}};
constexpr mh::CallSite kCalls4C9E50[] = {{0x12, 0x4FBD10}, {0x28, 0x4CAE30}, {0x33, 0x4CA0B0}, {0x47, 0x4CA0B0}, {0x4C, 0x4CA6B0}, {0x60, 0x4CA0B0}, {0x68, 0x4CA6B0}, {0x85, 0x4CA8C0}, {0x94, 0x4CA8C0}, {0x99, 0x5A7BC0}};
constexpr mh::CallSite kCalls4C9EF0[] = {{0x4B, 0x454DC0}, {0x59, 0x454CC0}, {0x87, 0x454DC0}, {0x95, 0x454CC0}, {0xA5, 0x452F70}};
constexpr mh::CallSite kCalls4C9FC0[] = {{0x39, 0x4530D0}, {0x57, 0x454DC0}, {0x6D, 0x4530D0}, {0x8E, 0x454DC0}, {0x9D, 0x4FBDB0}};
constexpr mh::CallSite kCalls4CA070[] = {{0x2F, 0x4351F0}};
constexpr mh::CallSite kCalls4CA0B0[] = {{0x4C, 0x5B93D2}, {0x6B, 0x5A7A00}, {0x9D, 0x5A77C0}, {0xB3, 0x572FA0}, {0xE0, 0x5B93D2}, {0xE9, 0x5B93D2}, {0xFC, 0x5B93D2}, {0x11C, 0x5B93D2}, {0x17A, 0x5A7A00}, {0x1C4, 0x5A7610}, {0x1CB, 0x5A7780}, {0x283, 0x5A85F0}, {0x289, 0x5A9350}, {0x29F, 0x572FA0}, {0x2E2, 0x5A7610}, {0x2EC, 0x5A7780}, {0x382, 0x5A85F0}, {0x388, 0x5A9350}, {0x39E, 0x572FA0}, {0x3E1, 0x5A7610}, {0x3EB, 0x5A7780}, {0x4A3, 0x5A85F0}, {0x4A9, 0x5A9350}, {0x4BF, 0x572FA0}, {0x4FF, 0x5A7610}, {0x509, 0x5A7780}, {0x59F, 0x5A85F0}, {0x5A5, 0x5A9350}, {0x5BB, 0x572FA0}};
constexpr mh::CallSite kCalls4CA6B0[] = {{0x32, 0x5B93D2}, {0x51, 0x5A7A00}, {0xC1, 0x5A77C0}, {0xCC, 0x572FA0}, {0xD8, 0x5A76B0}, {0xE0, 0x5A7780}, {0xF8, 0x5A8250}, {0x104, 0x5A9110}, {0x10C, 0x5B93D2}, {0x115, 0x5B93D2}, {0x128, 0x5B93D2}, {0x164, 0x5A7A00}, {0x19D, 0x5A8250}, {0x1A6, 0x5A9110}, {0x1C1, 0x5B93D2}, {0x1E5, 0x572FA0}};
constexpr mh::CallSite kCalls4CA8C0[] = {{0x4, 0x5B93D2}, {0x33, 0x5A77C0}, {0x49, 0x572FA0}, {0x5A, 0x5A75F0}, {0x61, 0x5A7780}, {0x90, 0x5A7A00}, {0xBA, 0x5A7A50}, {0xEA, 0x5A7A00}, {0x114, 0x5A7A50}, {0x18E, 0x572FA0}};
constexpr mh::Imm kImms4CAA70[] = {{0xF, 0x4CAAA0}, {0x17, 0x4E5200}};
constexpr mh::CallSite kCalls4CAAA0[] = {{0x29, 0x4456C0}, {0x39, 0x435180}, {0xBD, 0x4456C0}, {0xCD, 0x435180}, {0x180, 0x587900}};
constexpr mh::CallSite kCalls4CAC60[] = {{0x12, 0x4FBD10}, {0x2C, 0x4CAE30}, {0x37, 0x4CAEE0}, {0x4B, 0x4CAEE0}, {0x59, 0x4CB4F0}, {0x70, 0x4CAEE0}, {0x90, 0x4CB6E0}, {0x9F, 0x4CB6E0}, {0xA4, 0x5A7BC0}};
constexpr mh::CallSite kCalls4CAD10[] = {{0x19, 0x5B93D2}};
constexpr mh::CallSite kCalls4CAD60[] = {{0x4B, 0x454DC0}, {0x59, 0x454CC0}, {0x87, 0x454DC0}, {0x95, 0x454CC0}, {0xA5, 0x452F70}};
constexpr mh::CallSite kCalls4CAEE0[] = {{0x4C, 0x5B93D2}, {0x6B, 0x5A7A00}, {0x9D, 0x5A77C0}, {0xB3, 0x572FA0}, {0xE0, 0x5B93D2}, {0xE9, 0x5B93D2}, {0xFC, 0x5B93D2}, {0x11C, 0x5B93D2}, {0x180, 0x5A7A00}, {0x1CE, 0x5A7610}, {0x1D5, 0x5A7780}, {0x28D, 0x5A85F0}, {0x293, 0x5A9350}, {0x2A9, 0x572FA0}, {0x2EE, 0x5A7610}, {0x2F8, 0x5A7780}, {0x38E, 0x5A85F0}, {0x394, 0x5A9350}, {0x3AA, 0x572FA0}, {0x3F2, 0x5A7610}, {0x3FC, 0x5A7780}, {0x4B4, 0x5A85F0}, {0x4BA, 0x5A9350}, {0x4D0, 0x572FA0}, {0x511, 0x5A7610}, {0x51B, 0x5A7780}, {0x5B1, 0x5A85F0}, {0x5B7, 0x5A9350}, {0x5CD, 0x572FA0}};
constexpr mh::CallSite kCalls4CB4F0[] = {{0x17, 0x5A77C0}, {0x2D, 0x572FA0}, {0x5F, 0x5B93D2}, {0x7C, 0x5A7A00}, {0xB6, 0x5A76B0}, {0xBE, 0x5A7780}, {0xD6, 0x5A8250}, {0xDF, 0x5A9110}, {0xE7, 0x5B93D2}, {0xF0, 0x5B93D2}, {0x105, 0x5B93D2}, {0x13F, 0x5A7A00}, {0x178, 0x5A8250}, {0x181, 0x5A9110}, {0x19B, 0x5B93D2}, {0x1C9, 0x572FA0}};
constexpr mh::CallSite kCalls4CB6E0[] = {{0x4, 0x5B93D2}, {0x33, 0x5A77C0}, {0x49, 0x572FA0}, {0x5A, 0x5A75F0}, {0x61, 0x5A7780}, {0x90, 0x5A7A00}, {0xBA, 0x5A7A50}, {0xEA, 0x5A7A00}, {0x114, 0x5A7A50}, {0x18E, 0x572FA0}};
constexpr mh::Imm kImms4CB890[] = {{0xF, 0x4CB8D0}, {0x17, 0x4CBA00}, {0x22, 0x4CBA80}, {0x2A, 0x4B1ED0}, {0x32, 0x4CBAA0}};
constexpr mh::CallSite kCalls4CB8D0[] = {{0x38, 0x454DC0}, {0x46, 0x454CC0}, {0x58, 0x435180}, {0x88, 0x435180}, {0xCF, 0x435180}, {0x10A, 0x587900}};
constexpr mh::CallSite kCalls4CBA00[] = {{0x6A, 0x452F70}};
constexpr mh::CallSite kCalls4CBAA0[] = {{0x12, 0x454DC0}, {0x1E, 0x4FBDB0}, {0x29, 0x4530D0}, {0x38, 0x4351F0}};
constexpr mh::CallSite kCalls4CBB00[] = {{0x12, 0x4FBD10}, {0x24, 0x4CAE30}, {0x2F, 0x4CBE90}, {0x37, 0x5A7BC0}};
constexpr mh::CallSite kCalls4CBB40[] = {{0x0, 0x5B93D2}};
constexpr mh::CallSite kCalls4CBBE0[] = {{0x2E, 0x4351F0}};
constexpr mh::CallSite kCalls4CBC20[] = {{0x12, 0x4FBD10}, {0x22, 0x4CAE30}, {0x2D, 0x4CBE90}, {0x35, 0x4CC7D0}, {0x3A, 0x5A7BC0}};
constexpr mh::CallSite kCalls4CBC60[] = {{0x43, 0x5A7A50}, {0x69, 0x5A7A00}};
constexpr mh::CallSite kCalls4CBD40[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}};
constexpr mh::CallSite kCalls4CBDE0[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}, {0xA1, 0x4351F0}};
constexpr mh::CallSite kCalls4CBE90[] = {{0x48, 0x5A7A00}, {0x77, 0x5A77C0}, {0x8D, 0x572FA0}, {0xBA, 0x5B93D2}, {0xC3, 0x5B93D2}, {0xDD, 0x5B93D2}, {0x13B, 0x5A7A00}, {0x17C, 0x5A7610}, {0x183, 0x5A7780}, {0x21C, 0x5A85F0}, {0x222, 0x5A9350}, {0x24F, 0x572FA0}, {0x277, 0x5A7610}, {0x27E, 0x5A7780}, {0x2E5, 0x5A85F0}, {0x2EB, 0x5A9350}, {0x301, 0x572FA0}, {0x32C, 0x5A7610}, {0x336, 0x5A7780}, {0x3CD, 0x5A85F0}, {0x3D3, 0x5A9350}, {0x3E9, 0x572FA0}, {0x411, 0x5A7610}, {0x41B, 0x5A7780}, {0x486, 0x5A85F0}, {0x48C, 0x5A9350}, {0x4A2, 0x572FA0}};
constexpr mh::CallSite kCalls4CC360[] = {{0x12, 0x4FBD10}, {0x24, 0x4CAE30}, {0x29, 0x4CC5D0}, {0x2E, 0x4CC7D0}, {0x33, 0x5A7BC0}};
constexpr mh::CallSite kCalls4CC3A0[] = {{0x0, 0x5B93D2}, {0x55, 0x5A7A50}, {0x7B, 0x5A7A00}};
constexpr mh::CallSite kCalls4CC480[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}};
constexpr mh::CallSite kCalls4CC520[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}, {0xA2, 0x4351F0}};
constexpr mh::CallSite kCalls4CC5D0[] = {{0x31, 0x5B93D2}, {0x4D, 0x5A7A00}, {0xBB, 0x5A77C0}, {0xC6, 0x572FA0}, {0xD2, 0x5A76B0}, {0xDA, 0x5A7780}, {0xF2, 0x5A8250}, {0xFE, 0x5A9110}, {0x106, 0x5B93D2}, {0x10F, 0x5B93D2}, {0x123, 0x5B93D2}, {0x15A, 0x5A7A00}, {0x190, 0x5A8250}, {0x199, 0x5A9110}, {0x1B3, 0x5B93D2}, {0x1D6, 0x572FA0}};
constexpr mh::CallSite kCalls4CC7D0[] = {{0x4, 0x5B93D2}, {0x37, 0x5A77C0}, {0x4D, 0x572FA0}, {0x5E, 0x5A75F0}, {0x65, 0x5A7780}, {0x94, 0x5A7A00}, {0xBB, 0x5A7A50}, {0xE8, 0x5A7A00}, {0x10F, 0x5A7A50}, {0x186, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Blizzard_Task", 0x4C8D40, 0x2E, nullptr, 0, kImms4C8D40, MH_N(kImms4C8D40), nullptr, 0, reinterpret_cast<const void*>(&::Blizzard_Task)},
    {"Blizzard_Start", 0x4C8D70, 0xB9, kCalls4C8D70, MH_N(kCalls4C8D70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Blizzard_Start)},
    {"Blizzard_Wait", 0x4C8E30, 0x36, kCalls4C8E30, MH_N(kCalls4C8E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Blizzard_Wait)},
    {"BlizzardShard_Task", 0x4C8E70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_Task)},
    {"BlizzardShard_Run", 0x4C8E90, 0x72, kCalls4C8E90, MH_N(kCalls4C8E90), kImms4C8E90, MH_N(kImms4C8E90), nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_Run)},
    {"BlizzardShard_Launch", 0x4C8F10, 0x258, kCalls4C8F10, MH_N(kCalls4C8F10), nullptr, 0, kTables4C8F10, MH_N(kTables4C8F10), reinterpret_cast<const void*>(&::BlizzardShard_Launch)},
    {"BlizzardShard_Grow", 0x4C9170, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_Grow)},
    {"BlizzardShard_End", 0x4C91B0, 0x27, kCalls4C91B0, MH_N(kCalls4C91B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_End)},
    {"BlizzardShard_DrawCrystal", 0x4C9290, 0x406, kCalls4C9290, MH_N(kCalls4C9290), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_DrawCrystal)},
    {"BlizzardShard_DrawFan", 0x4C96A0, 0x1E4, kCalls4C96A0, MH_N(kCalls4C96A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_DrawFan)},
    {"BlizzardShard_DrawRing", 0x4C9890, 0x25E, kCalls4C9890, MH_N(kCalls4C9890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BlizzardShard_DrawRing)},
    {"Jolt_Task", 0x4C9C60, 0x26, nullptr, 0, kImms4C9C60, MH_N(kImms4C9C60), nullptr, 0, reinterpret_cast<const void*>(&::Jolt_Task)},
    {"Jolt_Start", 0x4C9C90, 0x19A, kCalls4C9C90, MH_N(kCalls4C9C90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Jolt_Start)},
    {"JoltBolt_Task", 0x4C9E30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_Task)},
    {"JoltBolt_Run", 0x4C9E50, 0x9F, kCalls4C9E50, MH_N(kCalls4C9E50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_Run)},
    {"JoltBolt_Rise", 0x4C9EF0, 0xC1, kCalls4C9EF0, MH_N(kCalls4C9EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_Rise)},
    {"JoltBolt_Fade", 0x4C9FC0, 0xAE, kCalls4C9FC0, MH_N(kCalls4C9FC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_Fade)},
    {"JoltBolt_End", 0x4CA070, 0x35, kCalls4CA070, MH_N(kCalls4CA070), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_End)},
    {"JoltBolt_DrawBand", 0x4CA0B0, 0x5F4, kCalls4CA0B0, MH_N(kCalls4CA0B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_DrawBand)},
    {"JoltBolt_DrawArcs", 0x4CA6B0, 0x205, kCalls4CA6B0, MH_N(kCalls4CA6B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_DrawArcs)},
    {"JoltBolt_DrawFlash", 0x4CA8C0, 0x1A7, kCalls4CA8C0, MH_N(kCalls4CA8C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::JoltBolt_DrawFlash)},
    {"Lightning_Task", 0x4CAA70, 0x26, nullptr, 0, kImms4CAA70, MH_N(kImms4CAA70), nullptr, 0, reinterpret_cast<const void*>(&::Lightning_Task)},
    {"Lightning_Start", 0x4CAAA0, 0x19A, kCalls4CAAA0, MH_N(kCalls4CAAA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Lightning_Start)},
    {"LightningBolt_Task", 0x4CAC40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_Task)},
    {"LightningBolt_Run", 0x4CAC60, 0xAA, kCalls4CAC60, MH_N(kCalls4CAC60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_Run)},
    {"LightningBolt_Wait", 0x4CAD10, 0x45, kCalls4CAD10, MH_N(kCalls4CAD10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_Wait)},
    {"LightningBolt_Rise", 0x4CAD60, 0xC1, kCalls4CAD60, MH_N(kCalls4CAD60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_Rise)},
    {"LightningBolt_DrawBand", 0x4CAEE0, 0x606, kCalls4CAEE0, MH_N(kCalls4CAEE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_DrawBand)},
    {"LightningBolt_DrawArcs", 0x4CB4F0, 0x1E9, kCalls4CB4F0, MH_N(kCalls4CB4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_DrawArcs)},
    {"LightningBolt_DrawFlash", 0x4CB6E0, 0x1A7, kCalls4CB6E0, MH_N(kCalls4CB6E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LightningBolt_DrawFlash)},
    {"Myollnir_Task", 0x4CB890, 0x3E, nullptr, 0, kImms4CB890, MH_N(kImms4CB890), nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_Task)},
    {"Myollnir_Start", 0x4CB8D0, 0x130, kCalls4CB8D0, MH_N(kCalls4CB8D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_Start)},
    {"Myollnir_Darken", 0x4CBA00, 0x7B, kCalls4CBA00, MH_N(kCalls4CBA00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_Darken)},
    {"Myollnir_WaitChildren", 0x4CBA80, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_WaitChildren)},
    {"Myollnir_End", 0x4CBAA0, 0x3E, kCalls4CBAA0, MH_N(kCalls4CBAA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_End)},
    {"MyollnirChild_Task", 0x4CBAE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirChild_Task)},
    {"MyollnirBolt_Run", 0x4CBB00, 0x3D, kCalls4CBB00, MH_N(kCalls4CBB00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirBolt_Run)},
    {"MyollnirBolt_Start", 0x4CBB40, 0x63, kCalls4CBB40, MH_N(kCalls4CBB40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirBolt_Start)},
    {"MyollnirBolt_Hold", 0x4CBBB0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirBolt_Hold)},
    {"MyollnirBolt_End", 0x4CBBE0, 0x34, kCalls4CBBE0, MH_N(kCalls4CBBE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirBolt_End)},
    {"MyollnirOrb_Run", 0x4CBC20, 0x40, kCalls4CBC20, MH_N(kCalls4CBC20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirOrb_Run)},
    {"MyollnirOrb_Start", 0x4CBC60, 0xA8, kCalls4CBC60, MH_N(kCalls4CBC60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirOrb_Start)},
    {"Myollnir_GrowHold", 0x4CBD10, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_GrowHold)},
    {"MyollnirOrb_Circle", 0x4CBD40, 0x9D, kCalls4CBD40, MH_N(kCalls4CBD40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirOrb_Circle)},
    {"MyollnirOrb_End", 0x4CBDE0, 0xA7, kCalls4CBDE0, MH_N(kCalls4CBDE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirOrb_End)},
    {"Myollnir_DrawBand", 0x4CBE90, 0x4CB, kCalls4CBE90, MH_N(kCalls4CBE90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_DrawBand)},
    {"MyollnirRing_Run", 0x4CC360, 0x39, kCalls4CC360, MH_N(kCalls4CC360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_Run)},
    {"MyollnirRing_Start", 0x4CC3A0, 0xBA, kCalls4CC3A0, MH_N(kCalls4CC3A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_Start)},
    {"MyollnirRing_Grow", 0x4CC460, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_Grow)},
    {"MyollnirRing_Circle", 0x4CC480, 0x9D, kCalls4CC480, MH_N(kCalls4CC480), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_Circle)},
    {"MyollnirRing_End", 0x4CC520, 0xA8, kCalls4CC520, MH_N(kCalls4CC520), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_End)},
    {"MyollnirRing_DrawArcs", 0x4CC5D0, 0x1F4, kCalls4CC5D0, MH_N(kCalls4CC5D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MyollnirRing_DrawArcs)},
    {"Myollnir_DrawFlash", 0x4CC7D0, 0x19F, kCalls4CC7D0, MH_N(kCalls4CC7D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Myollnir_DrawFlash)},
};
#undef MH_N

enum : unsigned {
    kBlizzard_Task, kBlizzard_Start, kBlizzard_Wait, kBlizzardShard_Task, kBlizzardShard_Run, kBlizzardShard_Launch,
    kBlizzardShard_Grow, kBlizzardShard_End, kBlizzardShard_DrawCrystal, kBlizzardShard_DrawFan, kBlizzardShard_DrawRing,
    kJolt_Task, kJolt_Start, kJoltBolt_Task, kJoltBolt_Run, kJoltBolt_Rise, kJoltBolt_Fade, kJoltBolt_End,
    kJoltBolt_DrawBand, kJoltBolt_DrawArcs, kJoltBolt_DrawFlash, kLightning_Task, kLightning_Start, kLightningBolt_Task,
    kLightningBolt_Run, kLightningBolt_Wait, kLightningBolt_Rise, kLightningBolt_DrawBand, kLightningBolt_DrawArcs,
    kLightningBolt_DrawFlash, kMyollnir_Task, kMyollnir_Start, kMyollnir_Darken, kMyollnir_WaitChildren, kMyollnir_End,
    kMyollnirChild_Task, kMyollnirBolt_Run, kMyollnirBolt_Start, kMyollnirBolt_Hold, kMyollnirBolt_End, kMyollnirOrb_Run,
    kMyollnirOrb_Start, kMyollnir_GrowHold, kMyollnirOrb_Circle, kMyollnirOrb_End, kMyollnir_DrawBand, kMyollnirRing_Run,
    kMyollnirRing_Start, kMyollnirRing_Grow, kMyollnirRing_Circle, kMyollnirRing_End, kMyollnirRing_DrawArcs,
    kMyollnir_DrawFlash, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees the standard set lacks ---------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define S22_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S22_THEIRS(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

// A pointer argument into ours' or the copy's own stack frame (the depth and
// flag outputs) differs between the two by construction: masked off. The
// harness logs four arguments: what Gpu_SetDrawMode's fifth, Sprite_SetTint's
// fifth and the projections' outputs after the fourth are is not compared
// (docs/magic_s22.md section 5).
const mh::Callee kCallees[] = {
    {S22_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S22_OURS(Sprite_SetTint), 4, {kAll, kU8, kU8, kU8}, kG, 0, 0},
    {S22_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S22_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, kG, 0, 0},
    {S22_OURS(Gte_RotTransPers3), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gte_RotTransPers4), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S22_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gte_PrimDepths3_10), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S22_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: SetPolyFT3, and the dx / dz turn by direction.
    {S22_THEIRS(0x5A7590), 1, {kAll}, kG, 0, 0},
    {S22_THEIRS(0x446770), 1, {kAll}, kG, 0, 0},
    // This group's own, called by address.
    {S22_THEIRS(0x4C9AF0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4C91E0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4C9290), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4C96A0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4C9890), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CAE30), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CA0B0), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S22_THEIRS(0x4CA6B0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CA8C0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CAEE0), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S22_THEIRS(0x4CB4F0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CB6E0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CBE90), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S22_THEIRS(0x4CC5D0), 0, {}, kG, 0, 0},
    {S22_THEIRS(0x4CC7D0), 0, {}, kG, 0, 0},
};

// The .data handler tables the dispatchers read in place (JoltBolt_TaskTable
// and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65B6F4, 1}, {0x65B6F8, 1}, {0x65B6FC, 4}, {0x65B70C, 1}, {0x65B710, 4},
    {0x65B720, 3}, {0x65B72C, 4}, {0x65B73C, 4}, {0x65B74C, 4},
};

// The packets the draws fill: Gfx_PacketNext is aimed at one of four places
// in this buffer (and moved between them by the disturbance), so a copy that
// keeps the pointer where the original reads it again shows.
alignas(16) unsigned char g_packets[0x200];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 3) * 0x40; }

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0;
const mh::Region kRegions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                               // Gfx_PacketNext
    {Key(g_packets), sizeof g_packets},
    {Key(MoveScript_TintRecords), 0xC00},
    {Key(Gfx_ClutStrip + 0x1A00), 0x200},        // row 26, Blizzard_Start's
    {Key(Gfx_ClutStripSource + 0x1A00), 0x200},
    {Key(BlizzardShard_Offsets), 0xA0},          // MAGIC096's .data up to its handler table
};

unsigned char* Sc() { return Sprite_Current; }

// The group's cells an effect reads again after a call: Gfx_PacketNext, a
// scratch word (the loop bounds +8 and +0xC kept small), a vertex word, the
// task's fields the harness leaves alone, the owner's height, the target's
// all-enemies bit.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 7) {
    case 0: mh::SetPointer(0x7E0670, PacketAt(v)); break;
    case 1: {
        const unsigned k = (v % 8) * 2;
        SetWord(mh::Mem(kScratch + k), k == 8 || k == 0xC ? word % 24 : k == 0xE ? 0 : word);
        break;
    }
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: {
        static const unsigned kFields[] = {3, 0x10, 0x18, 0x1C, 0x2E, 0x2F, 0x31, 0x3E, 0x3F};
        Sc()[kFields[v % 9]] = static_cast<unsigned char>(word);
        break;
    }
    case 4: {
        static const unsigned kFields[] = {0x35, 0x39, 0x3E, 0x3F};
        mh::Pointer(mh::at::kOwner)[kFields[v % 4]] = static_cast<unsigned char>(word);
        break;
    }
    case 5: mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Mem(mh::at::kTarget)[0] ^ 0x40); break;
    default: break;
    }
}

// --- the seed ------------------------------------------------------------------

void SetFloat(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

unsigned g_base = 0;   // DEBUG
void Seed(unsigned k) {
    k += g_base;
    unsigned char* const sc = Sc();
    mh::SetPointer(0x7E0670, PacketAt(mh::Next()));
    // The fcomp against 0.0 in Myollnir_DrawBand reads the third point's y
    // the (quiet) projection left: at, either side of, and not a number.
    for (unsigned q = 0; q < 4; ++q) {
        static const float kY[] = {0.0f, -0.0f, 1.0f, -1.0f, 1e-30f, -1e-30f};
        const std::uint32_t pick = mh::Next() % 8;
        if (pick < 6) SetFloat(PacketAt(q) + 0x2C, kY[pick]);
        else if (pick == 6) SetLong(PacketAt(q) + 0x2C, 0x7FC00000);
    }
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Mem(mh::at::kTarget)[0] | 0x40);
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kBlizzard_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kJolt_Task: case kLightning_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kMyollnir_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 5); break;
    case kBlizzardShard_Task: case kJoltBolt_Task: case kLightningBolt_Task: sc[1] = 0; break;
    case kMyollnirChild_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kBlizzardShard_Run: sc[2] = static_cast<unsigned char>(mh::Next() % 5); if (mh::Half()) sc[0] = 0; break;
    case kJoltBolt_Run: case kLightningBolt_Run: case kMyollnirBolt_Run: case kMyollnirOrb_Run: case kMyollnirRing_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        break;
    // the count-downs: one step from their ends
    case kBlizzard_Wait: case kBlizzardShard_Launch: case kJoltBolt_Fade: case kLightningBolt_Wait:
    case kMyollnir_Darken: case kMyollnirBolt_Hold: case kMyollnirOrb_Circle: case kMyollnirRing_Circle:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(1 + mh::Next() % 2);
        if (k == kBlizzardShard_Launch) {
            sc[8] = static_cast<unsigned char>(mh::Next() % 6);
            if (mh::Often()) sc[4] = static_cast<unsigned char>(mh::Next() % 16);
            mh::SetRandHint(mh::Half() ? 0x10 : 0x00);
        }
        break;
    case kBlizzardShard_Grow: {
        if (mh::Often()) sc[4] = static_cast<unsigned char>(mh::Next() % 16);
        const unsigned char size = BlizzardShard_Sizes[sc[4] & 0xF];
        if (mh::Often() && sc[4] < 16) sc[9] = static_cast<unsigned char>(size - 2 - (mh::Next() % 2));
        break;
    }
    case kBlizzardShard_End: case kMyollnirBolt_End:
        if (mh::Often()) sc[0xA] = static_cast<unsigned char>(1 + mh::Next() % 2);
        break;
    case kJoltBolt_End: case kMyollnirRing_End:
        if (mh::Often()) sc[0xA] = static_cast<unsigned char>(2 + (mh::Next() % 2) * 2);
        break;
    case kMyollnirOrb_End:
        if (mh::Often()) sc[0xA] = static_cast<unsigned char>(1 + mh::Next() % 2);
        break;
    case kJoltBolt_Rise: case kLightningBolt_Rise:
        if (mh::Often()) sc[0xA] = static_cast<unsigned char>(0xC + (mh::Next() % 2) * 4);
        break;
    case kMyollnir_WaitChildren: if (mh::Half()) sc[0xB] = 0; break;
    case kMyollnir_End: if (mh::Often()) sc[0xB] = static_cast<unsigned char>(8 + mh::Next() % 3); break;
    case kMyollnir_GrowHold: case kMyollnirRing_Grow:
        if (mh::Often()) sc[0xA] = static_cast<unsigned char>(0xE + mh::Next() % 4);
        break;
    case kMyollnirOrb_Start: case kMyollnirRing_Start: sc[3] = static_cast<unsigned char>(mh::Next() % 8); break;
    // the loops bounded by +0xA: short enough for the log
    case kJoltBolt_DrawArcs: case kLightningBolt_DrawArcs: case kMyollnirRing_DrawArcs:
        sc[0xA] = static_cast<unsigned char>(mh::Next() % 40);
        break;
    default: break;
    }
}

}  // namespace

// --- the three the harness cannot compare -----------------------------------------

namespace {

// Their recorders: every call and what it reads, into one log per pass. The
// GTE stand-ins write deterministic results where the original's callee
// writes (the translation, the matrix), so what reaches the next call is
// compared too; each may move the task's fields (and the scratch sums) that
// the caller reads after it.
struct OwnLog {
    std::uint32_t v[512];
    unsigned n;
    void Put(std::uint32_t x) {
        if (n < 512) v[n] = x;
        ++n;
    }
};
OwnLog g_own;
std::uint32_t g_own_seed;
std::uint32_t g_out_mask;
alignas(16) unsigned char g_tasks[2][0x84];

std::uint32_t OwnHash() {
    std::uint32_t h = (g_own_seed + g_own.n * 0x9E3779B1u) * 0x85EBCA6Bu;
    h ^= h >> 13;
    return h * 0xC2B2AE35u;
}
// After a call: a third of the time Sprite_Current moves to the other task,
// or one of the fields the pushes and the centre read is rewritten.
void OwnDisturb() {
    const std::uint32_t h = OwnHash();
    switch (h % 6) {
    case 0: Sprite_Current = g_tasks[(h >> 8) & 1]; break;
    case 1: {
        static const unsigned kFields[] = {0x18, 0x19, 0x1C, 0x1D, 0x34, 0x36, 0x38, 0x3A, 0x3E, 0x3F};
        Sprite_Current[kFields[(h >> 8) % 10]] = static_cast<unsigned char>(h >> 16);
        break;
    }
    case 2: SetLong(mh::Mem(kScratch + ((h >> 8) % 3) * 4), static_cast<std::int32_t>(h)); break;
    case 3: SetLong(mh::Mem(kScratch + 0xC), static_cast<std::int32_t>(1 + (h >> 8) % 20)); break;
    default: break;
    }
}
void Put3(const short* v) {
    for (unsigned i = 0; i < 3; ++i) g_own.Put(static_cast<std::uint16_t>(v[i]));
}
void __cdecl OwnPush() {
    g_own.Put(0x100);
    OwnDisturb();
}
void __cdecl OwnRotTrans(const short* v, long* t, long* flag) {
    g_own.Put(0x200);
    Put3(v);
    t[0] = v[0] * 3 + 1;
    t[1] = v[1] * 5 - 2;
    t[2] = v[2] * 7 + 3;
    *flag = 0;
    OwnDisturb();
}
short* __cdecl OwnRotMatrix(const short* a, short* m) {
    g_own.Put(0x300);
    Put3(a);
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(a[i % 3] * static_cast<int>(i + 1) + static_cast<int>(i));
    OwnDisturb();
    return m;
}
short* __cdecl OwnMulMatrix0(const short* a, const short* b, short* out) {
    g_own.Put(0x400);
    g_own.Put(a == Camera_Matrix);
    g_own.Put(b == out);
    for (unsigned i = 0; i < 9; ++i) g_own.Put(static_cast<std::uint16_t>(b[i]));
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    OwnDisturb();
    return out;
}
void __cdecl OwnSetRot(const unsigned long* m) {
    g_own.Put(0x500);
    const auto* s = reinterpret_cast<const short*>(m);
    for (unsigned i = 0; i < 9; ++i) g_own.Put(static_cast<std::uint16_t>(s[i]));
    OwnDisturb();
}
void __cdecl OwnSetTrans(const unsigned long* m) {
    g_own.Put(0x600);
    for (unsigned i = 0; i < 3; ++i) g_own.Put(static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(m) + 0x14 + 4 * i)));
    OwnDisturb();
}
unsigned g_out_calls;
unsigned char __cdecl OwnIsOut(unsigned actor) {
    g_own.Put(0x700 | (actor & 0xFF));
    const unsigned bit = (g_out_mask >> (g_out_calls++ & 31)) & 1;
    OwnDisturb();
    return static_cast<unsigned char>(bit);
}
const Hooks kOwnHooks = {OwnPush, OwnRotTrans, OwnRotMatrix, OwnMulMatrix0, OwnSetRot, OwnSetTrans, OwnIsOut};

template <typename F> const void* V(F f) { return reinterpret_cast<const void*>(f); }

struct OwnState {
    unsigned char scratch[0x10];
    unsigned char tasks[2][0x84];
    std::uint32_t current;
    OwnLog log;
};
void OwnCapture(OwnState& s) {
    std::memcpy(s.scratch, mh::Mem(kScratch), sizeof s.scratch);
    std::memcpy(s.tasks, g_tasks, sizeof s.tasks);
    s.current = Key(Sprite_Current);
    s.log = g_own;
}
bool OwnSame(const OwnState& a, const OwnState& b) {
    return a.log.n == b.log.n && std::memcmp(a.log.v, b.log.v, sizeof a.log.v) == 0 &&
           std::memcmp(a.scratch, b.scratch, sizeof a.scratch) == 0 && std::memcmp(a.tasks, b.tasks, sizeof a.tasks) == 0 &&
           a.current == b.current;
}

void SelfTestOwn() {
    struct Own {
        const char* name;
        std::uint32_t base, size;
        bof3::CloneCall calls[6];
        int n;
        void (__cdecl* ours)();
        void* copy;
    };
    static Own own[] = {
        {"BlizzardShard_PushMatrix", 0x4C91E0, 0xA7,
         {{0x3, V(OwnPush), 0x5A7B90}, {0x67, V(OwnRotTrans), 0x5A8200}, {0x76, V(OwnRotMatrix), 0x5A8060},
          {0x8A, V(OwnMulMatrix0), 0x5A7D70}, {0x94, V(OwnSetRot), 0x5A8DE0}, {0x9E, V(OwnSetTrans), 0x5A8E00}},
         6, &::BlizzardShard_PushMatrix, nullptr},
        {"LightningBolt_PushMatrix", 0x4CAE30, 0xA1,
         {{0x3, V(OwnPush), 0x5A7B90}, {0x61, V(OwnRotTrans), 0x5A8200}, {0x70, V(OwnRotMatrix), 0x5A8060},
          {0x84, V(OwnMulMatrix0), 0x5A7D70}, {0x8E, V(OwnSetRot), 0x5A8DE0}, {0x98, V(OwnSetTrans), 0x5A8E00}},
         6, &::LightningBolt_PushMatrix, nullptr},
        {"Blizzard_CenterOnTargets", 0x4C9AF0, 0x164, {{0x30, V(OwnIsOut), 0x4456C0}, {0xA5, V(OwnIsOut), 0x4456C0}}, 2,
         &::Blizzard_CenterOnTargets, nullptr},
    };
    for (Own& o : own) o.copy = bof3::CloneOriginal(o.name, o.base, o.size, o.calls, o.n);

    // Everything the three read or write, kept and put back.
    static unsigned char kept_scratch[0x10], kept_enemies[8 * 0x128], kept_party[3 * 0x14C];
    unsigned char* const enemies = mh::Mem(mh::at::kEnemies);
    unsigned char* const party = mh::Mem(mh::at::kParty);
    std::memcpy(kept_scratch, mh::Mem(kScratch), sizeof kept_scratch);
    std::memcpy(kept_enemies, enemies, sizeof kept_enemies);
    std::memcpy(kept_party, party, sizeof kept_party);
    const unsigned char kept_target = mh::Mem(mh::at::kTarget)[0];
    unsigned char* const kept_current = Sprite_Current;

    constexpr unsigned kRounds = 2000;
    unsigned bad = 0, calls = 0, divided[2] = {0, 0};
    static OwnState input, theirs, ours;
    for (unsigned round = 0; round < kRounds * 3; ++round) {
        const unsigned k = round % 3;
        for (auto& t : g_tasks)
            for (auto& b : t) b = static_cast<unsigned char>(mh::Next());
        for (unsigned i = 0; i < 0x10; ++i) mh::Mem(kScratch)[i] = static_cast<unsigned char>(mh::Next());
        for (unsigned i = 0; i < 8 + 3; ++i) {
            unsigned char* const r = i < 8 ? enemies + i * 0x128 : party + (i - 8) * 0x14C;
            SetLong(r + 0x34, static_cast<std::int32_t>(mh::Next()));
            SetLong(r + 0x38, static_cast<std::int32_t>(mh::Next()));
            SetWord(r + 0x3E, mh::Next());
        }
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Next());
        // The actors out: random, and never every one of those the side has.
        const bool all_enemies = (mh::Mem(mh::at::kTarget)[0] & 0x40) != 0;
        g_out_mask = mh::Next();
        g_out_mask &= ~(1u << (mh::Next() % (all_enemies ? 8 : 3)));
        Sprite_Current = g_tasks[mh::Next() & 1];
        g_own_seed = mh::Next();
        g_own.n = 0;
        OwnCapture(input);
        const unsigned char target = mh::Mem(mh::at::kTarget)[0];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(mh::Mem(kScratch), input.scratch, sizeof input.scratch);
            std::memcpy(g_tasks, input.tasks, sizeof g_tasks);
            Sprite_Current = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(input.current));
            mh::Mem(mh::at::kTarget)[0] = target;
            g_own.n = 0;
            g_out_calls = 0;
            if (pass) {
                g_hooks = kOwnHooks;
                own[k].ours();
                g_hooks = kHooks;
            } else {
                reinterpret_cast<void (__cdecl*)()>(own[k].copy)();
            }
            OwnCapture(pass ? ours : theirs);
        }
        calls += theirs.log.n;
        if (k == 2) ++divided[all_enemies];
        if (!OwnSame(theirs, ours) && ++bad <= 12)
            bof3::Log("shadow      magic_s22 self-test MISMATCH: round %u, %s, log %u / %u", round, own[k].name, theirs.log.n,
                      ours.log.n);
    }
    std::memcpy(mh::Mem(kScratch), kept_scratch, sizeof kept_scratch);
    std::memcpy(enemies, kept_enemies, sizeof kept_enemies);
    std::memcpy(party, kept_party, sizeof kept_party);
    mh::Mem(mh::at::kTarget)[0] = kept_target;
    Sprite_Current = kept_current;
    bof3::Log("shadow      magic_s22 self-test (own recorders): %u rounds over 3 functions (%u each), %u calls to the "
              "stand-ins, %u MISMATCHES; the centre over the enemies %u times, the party %u",
              kRounds * 3, kRounds, calls, bad, divided[1], divided[0]);
    if (bad) bof3::Fatal("magic_s22 differs from the original in %u self-test rounds (own recorders)", bad);
}

}  // namespace

void SelfTest() {
    SelfTestOwn();
    unsigned from = 0, to = sizeof kClones / sizeof kClones[0];   // DEBUG
    if (const char* e = std::getenv("S22_FROM")) from = static_cast<unsigned>(std::atoi(e));
    if (const char* e = std::getenv("S22_TO")) to = static_cast<unsigned>(std::atoi(e));
    g_base = from;
    const mh::Group group = {
        "magic_s22", kClones + from, to - from, kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_s22
