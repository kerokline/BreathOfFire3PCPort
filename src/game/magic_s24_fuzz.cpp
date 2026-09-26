// BOF3X_SHADOW=magic_s24: group S24's three overlays (MAGIC104, 105, 106)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s24.md section 4.
//
// The clone table is tools/magic_rows.py --unit MAGIC104 / 105 / 106
// --clones (2026-09-25), the Fn_ placeholders renamed; the callees the
// standard set lacks (the GTE and GPU helpers, the group's own functions
// called directly, and the two raw addresses of magic_s24_callees.h); the
// three .data dispatch runs; the regions the overlays read and write beyond
// the standard ones; and a seed per function.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s24.h"
#include "game/magic_s24_callees.h"
#include "game/move_script_bytes.h"

namespace magic_s24 {
namespace {

namespace mh = magic_harness;

// --- the clones ------------------------------------------------------------

constexpr mh::Imm kImms4D04B0[] = {{0xF, 0x4D04F0}, {0x17, 0x4D0600}, {0x22, 0x4D0620}, {0x2A, 0x4D0700}};
constexpr mh::CallSite kCalls4D04F0[] = {{0x37, 0x4FC0E0}, {0x5D, 0x435180}, {0xB2, 0x446770}, {0xEA, 0x587900}};
constexpr mh::CallSite kCalls4D0620[] = {{0x3C, 0x435180}, {0x94, 0x446770}};
constexpr mh::CallSite kCalls4D0700[] = {{0x12, 0x4530D0}, {0x21, 0x4351F0}};
constexpr mh::CallSite kCalls4D0750[] = {{0x23, 0x4D0D30}, {0x28, 0x4D0FF0}, {0x2D, 0x4D0DF0}, {0x32, 0x4D0890}, {0x37, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D0790[] = {{0x2B, 0x587900}};
constexpr mh::CallSite kCalls4D0850[] = {{0x31, 0x4351F0}};
constexpr mh::CallSite kCalls4D0890[] = {
    {0x8E, 0x5A7A50},  {0xA4, 0x5A7A00},  {0xB7, 0x5A7A50},  {0xCD, 0x5A7A50},  {0xE0, 0x5A7A00},
    {0xFF, 0x5A7A50},  {0x115, 0x5A7A00}, {0x128, 0x5A7A50}, {0x13E, 0x5A7A50}, {0x151, 0x5A7A00},
    {0x1A3, 0x5A7A50}, {0x1B8, 0x5A7A00}, {0x1CB, 0x5A7A50}, {0x1E0, 0x5A7A50}, {0x20D, 0x5A7A50},
    {0x222, 0x5A7A00}, {0x235, 0x5A7A50}, {0x24A, 0x5A7A50}, {0x28D, 0x5A77C0}, {0x298, 0x572FA0},
    {0x2A7, 0x5A7610}, {0x2AF, 0x5A7780}, {0x2E2, 0x5A85F0}, {0x2E8, 0x5A9350}, {0x44A, 0x572FA0}};
constexpr mh::CallSite kCalls4D0D30[] = {{0x3, 0x5A7B90}, {0x76, 0x5A8200}, {0x85, 0x5A8060}, {0x99, 0x5A7D70}, {0xA3, 0x5A8DE0}, {0xAD, 0x5A8E00}};
constexpr mh::CallSite kCalls4D0DF0[] = {{0xC, 0x5A7A50},  {0x33, 0x5A7A00},  {0x4C, 0x5A7A50},  {0xA7, 0x5A7A00},  {0xC0, 0x5A7A50},
                                         {0x10C, 0x5A77C0}, {0x117, 0x572FA0}, {0x123, 0x5A75F0}, {0x12B, 0x5A7780}, {0x1AC, 0x5A84A0},
                                         {0x1B2, 0x5A9310}, {0x1BD, 0x572FA0}, {0x1E5, 0x5A77C0}, {0x1EE, 0x461E50}};
constexpr mh::CallSite kCalls4D0FF0[] = {{0xC, 0x5A7A50},  {0x38, 0x5A7A50},  {0x61, 0x5A7A00},  {0x7A, 0x5A7A50},  {0x93, 0x5A7A00},
                                         {0xAC, 0x5A7A50},  {0xD4, 0x5A77C0},  {0xDD, 0x461E50},  {0xF1, 0x5A7610},  {0xF8, 0x5A7780},
                                         {0x121, 0x5A7A00}, {0x13A, 0x5A7A50}, {0x17B, 0x5A7A00}, {0x194, 0x5A7A50}, {0x261, 0x5A85F0},
                                         {0x267, 0x5A9350}, {0x270, 0x461E50}, {0x296, 0x5A77C0}, {0x29F, 0x461E50}};
constexpr mh::CallSite kCalls4D12A0[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}, {0x3C, 0x5A77C0}, {0x45, 0x461E50}};
constexpr mh::CallSite kCalls4D1320[] = {{0x0, 0x4D0D30}, {0x5, 0x4D1370}, {0xA, 0x5A7BC0}, {0x3E, 0x4351F0}};
constexpr mh::CallSite kCalls4D1370[] = {
    {0x2B, 0x5A7A50},  {0x41, 0x5A7A00},  {0x54, 0x5A7A50},  {0x6A, 0x5A7A50},  {0x7D, 0x5A7A00},  {0xA0, 0x5A7A50},
    {0xB6, 0x5A7A00},  {0xC9, 0x5A7A50},  {0xDF, 0x5A7A50},  {0xF2, 0x5A7A00},  {0x145, 0x5A7A50}, {0x15A, 0x5A7A00},
    {0x16D, 0x5A7A50}, {0x182, 0x5A7A50}, {0x195, 0x5A7A00}, {0x1D6, 0x5A7A50}, {0x1EB, 0x5A7A00}, {0x1FE, 0x5A7A50},
    {0x213, 0x5A7A50}, {0x226, 0x5A7A00}, {0x273, 0x5A77C0}, {0x27E, 0x572FA0}, {0x28D, 0x5A7610}, {0x295, 0x5A7780},
    {0x2C8, 0x5A85F0}, {0x2CE, 0x5A9350}, {0x32F, 0x572FA0}};

constexpr mh::CallSite kCalls4D16E0[] = {{0x53, 0x4D1F30}, {0x58, 0x4D2220}};
constexpr mh::Imm kImms4D16E0[] = {{0x16, 0x4D1760}, {0x1E, 0x4F7350}};
constexpr mh::CallSite kCalls4D1760[] = {{0x1F, 0x4FC0E0}, {0x55, 0x446770}, {0xB2, 0x5A7A70}, {0xD3, 0x435180}, {0x14F, 0x587900}};
constexpr mh::CallSite kCalls4D18E0[] = {{0x42, 0x4D1AC0}, {0x47, 0x4D1B60}, {0x4C, 0x5A7BC0}};
constexpr mh::Imm kImms4D18E0[] = {{0xF, 0x4D1940}, {0x17, 0x4D19E0}, {0x22, 0x4D1A80}, {0x2A, 0x4D1AA0}};
constexpr mh::CallSite kCalls4D19E0[] = {{0x16, 0x4D21C0}};
constexpr mh::CallSite kCalls4D1AA0[] = {{0x14, 0x4351F0}};
constexpr mh::CallSite kCalls4D1AC0[] = {{0x3, 0x5A7B90}, {0x5F, 0x5A8200}, {0x6E, 0x5A8060}, {0x82, 0x5A7D70}, {0x8C, 0x5A8DE0}, {0x96, 0x5A8E00}};
constexpr mh::CallSite kCalls4D1B60[] = {{0xF3, 0x5A7A00},  {0x10E, 0x5A7A50}, {0x132, 0x5A7A00}, {0x14C, 0x5A7A50},
                                         {0x18C, 0x5A7A00}, {0x209, 0x5A7A00}, {0x22B, 0x5A7A50}, {0x25A, 0x5A7A00},
                                         {0x27B, 0x5A7A50}, {0x2D1, 0x5A77C0}, {0x2DC, 0x572FA0}, {0x2E8, 0x5A7610},
                                         {0x2F0, 0x5A7780}, {0x38E, 0x5A85F0}, {0x394, 0x5A9350}, {0x39F, 0x572FA0}};
constexpr mh::CallSite kCalls4D1F50[] = {{0x1C, 0x4FBD10}};
constexpr mh::CallSite kCalls4D1F80[] = {{0xF, 0x5A7A00},  {0x3D, 0x5A7A00},  {0x6D, 0x5A7A50}, {0xBE, 0x5720C0},
                                         {0x15A, 0x5B93D2}, {0x164, 0x5891F0}, {0x16C, 0x5B93D2}};
constexpr mh::CallSite kCalls4D2110[] = {{0x20, 0x5A7A00}, {0x43, 0x5A7A50}, {0x61, 0x589410}};
constexpr mh::CallSite kCalls4D2220[] = {{0xE4, 0x5A77C0}, {0xF5, 0x461E50}, {0x101, 0x5A7710}, {0x1FB, 0x5A7780}, {0x20B, 0x461E50}};

constexpr mh::CallSite kCalls4D2450[] = {{0x49, 0x4D25C0}};
constexpr mh::Imm kImms4D2450[] = {{0x15, 0x4D24C0}, {0x1D, 0x4F7350}};
constexpr mh::CallSite kCalls4D24C0[] = {{0x79, 0x4D2D00}, {0xA8, 0x5B93D2}, {0xE4, 0x587900}, {0xEE, 0x587900}};
constexpr mh::CallSite kCalls4D25E0[] = {{0x23, 0x4D29F0}, {0x28, 0x4D2AB0}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D2620[] = {{0x4B, 0x5A7A00}, {0x76, 0x5A7A50}, {0xBE, 0x5B93D2}, {0xD0, 0x5B93D2},
                                         {0xE2, 0x5B93D2}, {0x107, 0x5B93D2}, {0x11A, 0x5B93D2}};
constexpr mh::CallSite kCalls4D2770[] = {{0x15, 0x5A7A00}, {0x3F, 0x5A7A50}, {0xBE, 0x452F70}};
constexpr mh::CallSite kCalls4D2840[] = {{0x23, 0x5A7A00}, {0x4C, 0x5A7A50}};
constexpr mh::CallSite kCalls4D2930[] = {{0x23, 0x5A7A00}, {0x4C, 0x5A7A50}, {0xB6, 0x4D2D50}};
constexpr mh::CallSite kCalls4D29F0[] = {{0x3, 0x5A7B90}, {0x7C, 0x5A8200}, {0x8B, 0x5A8060}, {0x9F, 0x5A7D70}, {0xA9, 0x5A8DE0}, {0xB3, 0x5A8E00}};
constexpr mh::CallSite kCalls4D2AB0[] = {{0x15, 0x5A77C0},  {0x2B, 0x572FA0},  {0xEC, 0x5A75F0},  {0xF4, 0x5A7780},
                                         {0x101, 0x5A7A00}, {0x121, 0x5A7A50}, {0x141, 0x5A7A00}, {0x161, 0x5A7A50},
                                         {0x181, 0x5A7A00}, {0x1BD, 0x5A84A0}, {0x1C6, 0x5A9310}, {0x22A, 0x572FA0}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Fx104_Task", 0x4D04B0, 0x36, nullptr, 0, kImms4D04B0, MH_N(kImms4D04B0), nullptr, 0, reinterpret_cast<const void*>(&::Fx104_Task)},
    {"Fx104_Start", 0x4D04F0, 0x106, kCalls4D04F0, MH_N(kCalls4D04F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_Start)},
    {"Fx104_WaitFirstRing", 0x4D0600, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_WaitFirstRing)},
    {"Fx104_SecondRing", 0x4D0620, 0xE0, kCalls4D0620, MH_N(kCalls4D0620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_SecondRing)},
    {"Fx104_End", 0x4D0700, 0x27, kCalls4D0700, MH_N(kCalls4D0700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_End)},
    {"Fx104_Child", 0x4D0730, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_Child)},
    {"Fx104_Whirl", 0x4D0750, 0x3D, kCalls4D0750, MH_N(kCalls4D0750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_Whirl)},
    {"Fx104_WhirlDelay", 0x4D0790, 0x4F, kCalls4D0790, MH_N(kCalls4D0790), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_WhirlDelay)},
    {"Fx104_WhirlGrow", 0x4D07E0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_WhirlGrow)},
    {"Fx104_WhirlSpin", 0x4D0810, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_WhirlSpin)},
    {"Fx104_WhirlEnd", 0x4D0850, 0x37, kCalls4D0850, MH_N(kCalls4D0850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_WhirlEnd)},
    {"Fx104_DrawFunnel", 0x4D0890, 0x495, kCalls4D0890, MH_N(kCalls4D0890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_DrawFunnel)},
    {"Fx104_PushMatrix", 0x4D0D30, 0xB6, kCalls4D0D30, MH_N(kCalls4D0D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_PushMatrix)},
    {"Fx104_DrawDisc", 0x4D0DF0, 0x1FE, kCalls4D0DF0, MH_N(kCalls4D0DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_DrawDisc)},
    {"Fx104_DrawRing", 0x4D0FF0, 0x2AF, kCalls4D0FF0, MH_N(kCalls4D0FF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_DrawRing)},
    {"Fx104_Burst", 0x4D12A0, 0x4E, kCalls4D12A0, MH_N(kCalls4D12A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_Burst)},
    {"Fx104_BurstDelay", 0x4D12F0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_BurstDelay)},
    {"Fx104_BurstShrink", 0x4D1320, 0x44, kCalls4D1320, MH_N(kCalls4D1320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_BurstShrink)},
    {"Fx104_DrawSphere", 0x4D1370, 0x364, kCalls4D1370, MH_N(kCalls4D1370), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx104_DrawSphere)},
    {"Fx105_Task", 0x4D16E0, 0x7A, kCalls4D16E0, MH_N(kCalls4D16E0), kImms4D16E0, MH_N(kImms4D16E0), nullptr, 0, reinterpret_cast<const void*>(&::Fx105_Task)},
    {"Fx105_Start", 0x4D1760, 0x160, kCalls4D1760, MH_N(kCalls4D1760), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_Start)},
    {"Fx105_Child", 0x4D18C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_Child)},
    {"Fx105_Orb", 0x4D18E0, 0x55, kCalls4D18E0, MH_N(kCalls4D18E0), kImms4D18E0, MH_N(kImms4D18E0), nullptr, 0, reinterpret_cast<const void*>(&::Fx105_Orb)},
    {"Fx105_OrbStart", 0x4D1940, 0x99, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_OrbStart)},
    {"Fx105_OrbEmit", 0x4D19E0, 0x91, kCalls4D19E0, MH_N(kCalls4D19E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_OrbEmit)},
    {"Fx105_OrbHold", 0x4D1A80, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_OrbHold)},
    {"MagicFx_EndWithChildren", 0x4D1AA0, 0x1A, kCalls4D1AA0, MH_N(kCalls4D1AA0), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::MagicFx_EndWithChildren)},
    {"Fx105_PushMatrix", 0x4D1AC0, 0x9F, kCalls4D1AC0, MH_N(kCalls4D1AC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_PushMatrix)},
    {"Fx105_DrawOrb", 0x4D1B60, 0x3CE, kCalls4D1B60, MH_N(kCalls4D1B60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_DrawOrb)},
    {"Fx105_Mote", 0x4D1F30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_Mote)},
    {"Fx105_MoteRun", 0x4D1F50, 0x2C, kCalls4D1F50, MH_N(kCalls4D1F50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_MoteRun)},
    {"Fx105_MoteStart", 0x4D1F80, 0x185, kCalls4D1F80, MH_N(kCalls4D1F80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_MoteStart)},
    {"Fx105_MoteDrift", 0x4D2110, 0xA3, kCalls4D2110, MH_N(kCalls4D2110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_MoteDrift)},
    {"Fx105_MoteAlloc", 0x4D21C0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_MoteAlloc), 0xFF},
    {"Fx105_MoteDraw", 0x4D2220, 0x223, kCalls4D2220, MH_N(kCalls4D2220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx105_MoteDraw)},
    {"Fx106_Task", 0x4D2450, 0x61, kCalls4D2450, MH_N(kCalls4D2450), kImms4D2450, MH_N(kImms4D2450), nullptr, 0, reinterpret_cast<const void*>(&::Fx106_Task)},
    {"Fx106_Start", 0x4D24C0, 0xFA, kCalls4D24C0, MH_N(kCalls4D24C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_Start)},
    {"Fx106_Spark", 0x4D25C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_Spark)},
    {"Fx106_SparkRun", 0x4D25E0, 0x33, kCalls4D25E0, MH_N(kCalls4D25E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkRun)},
    {"Fx106_SparkDelay", 0x4D2620, 0x141, kCalls4D2620, MH_N(kCalls4D2620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkDelay)},
    {"Fx106_SparkConverge", 0x4D2770, 0xC7, kCalls4D2770, MH_N(kCalls4D2770), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkConverge)},
    {"Fx106_SparkSpin", 0x4D2840, 0xE3, kCalls4D2840, MH_N(kCalls4D2840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkSpin)},
    {"Fx106_SparkRise", 0x4D2930, 0xBC, kCalls4D2930, MH_N(kCalls4D2930), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkRise)},
    {"Fx106_PushMatrix", 0x4D29F0, 0xBC, kCalls4D29F0, MH_N(kCalls4D29F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_PushMatrix)},
    {"Fx106_DrawSpark", 0x4D2AB0, 0x245, kCalls4D2AB0, MH_N(kCalls4D2AB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_DrawSpark)},
    {"Fx106_SparkAlloc", 0x4D2D00, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkAlloc), 0xFF},
    {"Fx106_SparkFree", 0x4D2D50, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Fx106_SparkFree)},
};
#undef MH_N
enum : unsigned {
    kTask104, kStart104, kWaitFirst, kSecondRing, kEnd104, kChild104, kWhirl, kWhirlDelay, kWhirlGrow, kWhirlSpin,
    kWhirlEnd, kFunnel, kPush104, kDisc, kRing, kBurst, kBurstDelay, kBurstShrink, kSphere,
    kTask105, kStart105, kChild105, kOrb, kOrbStart, kOrbEmit, kOrbHold, kEndChildren, kPush105, kDrawOrb, kMote,
    kMoteRun, kMoteStart, kMoteDrift, kMoteAlloc, kMoteDraw,
    kTask106, kStart106, kSpark, kSparkRun, kSparkDelay, kConverge, kSpin, kRise, kPush106, kDrawSpark, kSparkAlloc,
    kSparkFree, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "the seed's indices");

// --- the callees the standard set lacks -------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu, kAngle = 0xFFFu;
template <typename F> constexpr std::uint32_t KeyOf(F f) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)); }
#define S24_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S24_RAW(address) #address, address, address
// A commit logs the primitive it links (the 0x40 bytes at Gfx_PacketNext):
// the draws write every primitive into the same few bytes of the buffer, so
// the regions at the end hold only the last one.
std::uint32_t LogPrimitive(const std::uint32_t*, std::uint32_t answer) {
    mh::NoteBytes(Gfx_PacketNext, 0x40);
    return answer;
}
const mh::Callee* Callees(unsigned& n) {
    // A pointer the caller built in its own frame is logged by the bytes it
    // points at (deref) or not at all (mask 0): its address differs between
    // the copy and ours. The matrices the GTE callees would fill are never
    // filled by a recorder, so nothing reads them.
    static const mh::Callee k[] = {
        {S24_OURS(Math_Sin), 1, {kAngle}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Math_Cos), 1, {kAngle}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Math_Ratan2), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kU16}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, mh::Answer::kGarbage, 0, 0, {}, &LogPrimitive},
        {S24_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, mh::Answer::kGarbage, 0, 0, {}, &LogPrimitive},
        {S24_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gpu_SetPolyG4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gpu_SetSprt), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_RotTransPers3), 4, {0, 0, 0, kAll}, mh::Answer::kGarbage, 0, 0, {8, 8, 8, 0}},
        {S24_OURS(Gte_RotTransPers4), 4, {0, 0, 0, 0}, mh::Answer::kGarbage, 0, 0, {8, 8, 8, 8}},
        {S24_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_PrimDepths4_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_RotTrans), 2, {0, 0}, mh::Answer::kGarbage, 0, 0, {6, 0}},
        {S24_OURS(Gte_RotMatrix), 2, {0, 0}, mh::Answer::kGarbage, 0, 0, {6, 0}},
        {S24_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_SetRotMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Gte_SetTransMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(AreaMap_Elevation), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
        {S24_OURS(Sprite_SetAnimation), 1, {kU8}, mh::Answer::kGarbage, 0, 0},
        // not ours: the effect library's (group L) and the engine's
        {S24_RAW(kCentreOnTargets), 0, {}, mh::Answer::kGarbage, 0, 0},
        {S24_RAW(kTurnByFacing), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
        // the group's own, called directly: logged as the task they run for
        {"Fx104_DrawFunnel", 0x4D0890, 0x4D0890, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx104_PushMatrix", 0x4D0D30, 0x4D0D30, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx104_DrawDisc", 0x4D0DF0, 0x4D0DF0, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx104_DrawRing", 0x4D0FF0, 0x4D0FF0, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx104_DrawSphere", 0x4D1370, 0x4D1370, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx105_PushMatrix", 0x4D1AC0, 0x4D1AC0, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx105_DrawOrb", 0x4D1B60, 0x4D1B60, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx105_Mote", 0x4D1F30, 0x4D1F30, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx105_MoteDraw", 0x4D2220, 0x4D2220, 0, {}, mh::Answer::kPhase, 0, 0},
        {"Fx106_Spark", 0x4D25C0, 0x4D25C0, 0, {cell::kSparkCurrent}, mh::Answer::kPhase, 0, 0},
        {"Fx106_PushMatrix", 0x4D29F0, 0x4D29F0, 0, {cell::kSparkCurrent}, mh::Answer::kPhase, 0, 0},
        {"Fx106_DrawSpark", 0x4D2AB0, 0x4D2AB0, 0, {cell::kSparkCurrent}, mh::Answer::kPhase, 0, 0},
        {"Fx106_SparkFree", 0x4D2D50, 0x4D2D50, 0, {cell::kSparkCurrent}, mh::Answer::kPhase, 0, 0},
        // the allocators: a record's index, or 0xFF for none (the mote pool's
        // caller tests it; the spark pool's does not, so its recorder never
        // answers past the pool)
        {"Fx105_MoteAlloc", 0x4D21C0, 0x4D21C0, 0, {}, mh::Answer::kByte, 0xFF, cell::kMotes - 1},
        {"Fx106_SparkAlloc", 0x4D2D00, 0x4D2D00, 0, {}, mh::Answer::kByte, 0, cell::kSparks - 1},
    };
    n = sizeof k / sizeof k[0];
    return k;
}
#undef S24_OURS
#undef S24_RAW

// The .data dispatch runs, each read in place: 0x65B868 is Fx104_ChildPhases
// (2), _WhirlPhases (4), _BurstPhases (2) and Fx105_ChildPhases (1) end to
// end; 0x65B8B4 Fx105_MotePhases (1) and _MoteRunPhases (2); 0x65B8E0
// Fx106_SparkPhases (1) and _SparkRunPhases (4).
const mh::DataTable kTables[] = {{0x65B868, 9}, {0x65B8B4, 3}, {0x65B8E0, 5}};

// --- the state ---------------------------------------------------------------

constexpr unsigned kPrimBytes = 0x100, kFrameBytes = 0x100;
alignas(16) unsigned char g_prims[kPrimBytes];     // what Gfx_PacketNext points into
alignas(16) unsigned char g_frames[kFrameBytes];   // a mote's frame record ([+0x54] + u16 +0x5A)

mh::Region g_regions[] = {
    {cell::kVertices, 0x20},
    {cell::kScratch, 0x10},
    {0x905E60, 8},                               // Field_Kind2Z, Field_Kind2X
    {0x7E0670, 4},                               // Gfx_PacketNext
    {0, kPrimBytes},                             // g_prims (filled in at start-up)
    {0, kFrameBytes},                            // g_frames
    {cell::kSpriteBank, 4},
    {cell::kClutSource, 0x20},
    {cell::kClutRow, 0x20},
    {cell::kMotePool, cell::kMotes * cell::kMoteStride},
    {cell::kSparkPool, cell::kSparks * cell::kSparkStride},
    {cell::kSparkCurrent, 4},
    {0x65B828, 0x40},                            // Fx104_ChildOffsets
    {0x65B88C, 0x28},                            // Fx105_OrbAngles, Fx105_OrbColours
    {0x65B8C0, 0x20},                            // Fx105_MoteSizes
    {0x65B8F4, 0x24},                            // Fx106_ShardTables
};

unsigned char* PrimAt(std::uint32_t v) { return g_prims + 4 * (v % 16); }
unsigned char* SparkAt(std::uint32_t v) { return mh::Mem(cell::kSparkPool + (v % cell::kSparks) * cell::kSparkStride); }
unsigned char* MoteAt(std::uint32_t v) { return mh::Mem(cell::kMotePool + (v % cell::kMotes) * cell::kMoteStride); }
unsigned char* Spark() { return mh::Pointer(cell::kSparkCurrent); }
unsigned char* AnOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }
void SetLong(unsigned char* p, std::int32_t v) { move_script::SetLong(p, v); }
void SetWord(unsigned char* p, unsigned v) { move_script::SetWord(p, v); }

// A byte at one of the values, or one either side, half the time.
void Near(unsigned char& b, unsigned v) {
    if (mh::Half()) b = static_cast<unsigned char>(v + (mh::Next() % 3) - 1);
}

void Seed(unsigned k) {
    // Everything any function dereferences, put back inside.
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(cell::kActorRecord, mh::SpriteRecord(mh::Next()));
    mh::SetPointer(cell::kSparkCurrent, SparkAt(mh::Next()));
    for (unsigned i = 0; i < cell::kMotes; ++i) mh::SetPointer(cell::kMotePool + i * cell::kMoteStride + 0x80, AnOwner(mh::Next()));
    for (unsigned i = 0; i < cell::kSparks; ++i) mh::SetPointer(cell::kSparkPool + i * cell::kSparkStride + 0x1C, AnOwner(mh::Next()));
    for (unsigned t = 0; t < 4; ++t) {
        unsigned char* const task = mh::TaskAt(t);
        mh::SetPointer(mh::at::kTasks + t * mh::at::kTaskStride + 0x54, g_frames);
        SetWord(task + 0x5A, mh::Next() % 0x40);
        g_frames[task[0x5A]] = static_cast<unsigned char>(mh::Next() % 7);
    }
    unsigned char* const sc = Sprite_Current;
    unsigned char* const spark = Spark();
    switch (k) {
    case kTask104: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kStart104:
        if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(0x40 | mh::Next() % 3);
        break;
    case kWaitFirst: case kEnd104: Near(sc[0xB], 7); break;
    case kSecondRing: Near(sc[9], 1); break;
    case kChild104: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kWhirl: sc[2] = static_cast<unsigned char>(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kWhirlDelay: Near(sc[0xA], 1); break;
    case kWhirlGrow: Near(sc[9], 8); break;
    case kWhirlSpin: Near(sc[9], 0x10); break;
    case kWhirlEnd: Near(sc[9], 0x18); break;
    case kFunnel:
        sc[2] = static_cast<unsigned char>(mh::Next() % 5);
        if (mh::Often()) sc[9] = static_cast<unsigned char>(mh::Next() % 24);
        break;
    case kPush104: if (mh::Half()) sc[5] = 0xC; break;
    case kDisc: if (mh::Half()) sc[2] = 3; break;
    case kRing: Near(sc[9], 8); break;
    case kBurst:
        // it dispatches after two calls, whose disturbance may move
        // Sprite_Current to another slot: every slot's +2 inside the table
        for (unsigned t = 0; t < 4; ++t) mh::TaskAt(t)[2] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kBurstDelay: if (mh::Half()) sc[0xA] = 0; break;
    case kBurstShrink: Near(sc[9], 1); break;
    case kTask105: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kChild105: sc[1] = 0; break;
    case kOrb:
        sc[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        break;
    case kOrbStart: if (mh::Often()) sc[4] = static_cast<unsigned char>(mh::Next() % 8); break;
    case kOrbEmit:
        if (mh::Half()) Frame_Counter = (Frame_Counter & ~3u) | ((sc[4] & 3u) ^ (mh::Next() % 2 ? 0 : 1));
        Near(sc[9], 0x20);
        break;
    case kOrbHold: Near(sc[0xA], 0x40); break;
    case kEndChildren: if (mh::Half()) sc[0xB] = 0; break;
    case kDrawOrb:
        sc[4] = static_cast<unsigned char>(mh::Next() % 7);
        if (mh::Often()) sc[9] = static_cast<unsigned char>(mh::Next() % 0x21);
        break;
    case kMote: sc[1] = 0; break;
    case kMoteRun: sc[2] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kMoteDrift:
        if (mh::Half()) SetLong(sc + 0x14, static_cast<short>(move_script::Word(sc + 0x3E)) - 8 + static_cast<int>(mh::Next() % 3) - 1);
        break;
    case kMoteAlloc: case kSparkAlloc:
        if (mh::Half()) {
            const bool motes = k == kMoteAlloc;
            const unsigned n = motes ? cell::kMotes : cell::kSparks;
            for (unsigned i = 0; i < n; ++i) (motes ? MoteAt(i) : SparkAt(i))[0] |= 1;
            if (mh::Half()) (motes ? MoteAt(mh::Next()) : SparkAt(mh::Next()))[0] &= 0xFE;
        }
        break;
    case kMoteDraw: {
        if (mh::Often()) sc[0] &= 0xBF;
        const int xs[] = {-0x41, -0x40, 0x180, 0x181};
        const int ys[] = {-0x41, -0x40, 0x130, 0x131};
        SetWord(sc + 0x2E, static_cast<unsigned>(mh::Half() ? xs[mh::Next() % 4] : static_cast<int>(mh::Next() % 0x1C0) - 0x40));
        SetWord(sc + 0x30, static_cast<unsigned>(mh::Half() ? ys[mh::Next() % 4] : static_cast<int>(mh::Next() % 0x170) - 0x40));
        if (mh::Half()) sc[0x28] = 0;
        break;
    }
    case kTask106: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kSpark: spark[1] = 0; break;
    case kSparkRun:
        spark[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) spark[0] = 0;
        break;
    case kSparkDelay: Near(spark[0xA], 1); break;
    case kConverge: {
        if (mh::Half()) spark[4] = static_cast<unsigned char>(mh::Next() % 2 ? 0 : 0x20 * (mh::Next() % 8));
        Near(spark[9], 0x10);
        const int limit = (spark[4] >> 5) * 8 + 0x20;
        if (mh::Half()) SetWord(spark + 0xC, static_cast<unsigned>(limit + 2 + static_cast<int>(mh::Next() % 3) - 1));
        break;
    }
    case kSpin:
        Near(spark[5], 0xC);
        Near(spark[6], 4);
        Near(spark[7], 4);
        Near(spark[3], 7);
        Near(spark[8], 1);
        break;
    case kRise: Near(spark[9], 1); break;
    default: break;
    }
}

// The group's own cells a callee may move: the packet pointer, the current
// spark, a scratch word or vertex, a byte of the current spark.
void Disturb(std::uint32_t h) {
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 5) {
    case 0: Gfx_PacketNext = PrimAt(h >> 12); break;
    case 1: mh::SetPointer(cell::kSparkCurrent, SparkAt(h >> 12)); break;
    case 2: mh::Mem(cell::kScratch + (h >> 12) % 0x10)[0] = b; break;
    case 3: mh::Mem(cell::kVertices + (h >> 12) % 0x20)[0] = b; break;
    default: Spark()[(h >> 12) % 0x20] = b; break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[4].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    g_regions[5].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_frames));
    unsigned n_callees = 0;
    const mh::Callee* const callees = Callees(n_callees);
    const mh::Group group = {
        "magic_s24", kClones, sizeof kClones / sizeof kClones[0], callees, n_callees, kTables,
        sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed, &Disturb, 2000, nullptr, 2,
    };
    mh::Run(group);
}

}  // namespace magic_s24
