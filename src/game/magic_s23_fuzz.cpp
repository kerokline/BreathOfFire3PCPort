// BOF3X_SHADOW=magic_s23: group S23's four overlays through the spell round's
// shared harness (magic_harness.h), once at start-up. docs/magic_s23.md
// section 6.
//
// The clone table is tools/magic_rows.py --unit MAGIC100 .. MAGIC103
// --clones (2026-09-25), the placeholders renamed. Beyond the harness's
// standard set this lists: the GTE / GPU library and Math_Sin / _Cos /
// _Ratan2 called for real on both sides (Answer::kThrough: the draws hand
// them stack vectors and matrices, so what they compute has to land in the
// compared state); the recorders for the map (MapView_LinkPrimAt,
// Gfx_CommitPrim, AreaMap_Elevation), Battle_ActorIsOut, the four raw callees
// of other units, and the group's own draws and matrices, which the tasks
// call; the eight .data tables of handlers; and every region the functions
// touch beyond the standard ones.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s23.h"
#include "game/magic_s23_callees.h"
#include "game/move_script_bytes.h"

namespace magic_s23 {
namespace {

namespace mh = magic_harness;
using move_script::SetLong;
using move_script::SetWord;

// 0x4CC970: 0x2E bytes
constexpr mh::Imm kImms4CC970[] = {{0xF, 0x4CC9A0}, {0x17, 0x4CCA70}, {0x22, 0x4F7350}};
// 0x4CC9A0: 0xC6 bytes
constexpr mh::CallSite kCalls4CC9A0[] = {{0x6C, 0x435180}, {0xBB, 0x587900}};
// 0x4CCA70: 0x26 bytes
constexpr mh::CallSite kCalls4CCA70[] = {{0x15, 0x452F70}};
// 0x4CCAA0: 0x12 bytes; +0xB note: jmp through .data 0x65b75c, 6 code entries (a data_tables entry)
// 0x4CCAC0: 0x42 bytes; +0xB note: call through .data 0x65b760, 5 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4CCAC0[] = {{0x23, 0x4CCEF0}, {0x28, 0x4CCFA0}, {0x2D, 0x5A7BC0}, {0x32, 0x4CD2D0}, {0x37, 0x4CD390}, {0x3C, 0x5A7BC0}};
// 0x4CCB10: 0x17C bytes
constexpr mh::CallSite kCalls4CCB10[] = {{0x4A, 0x446770}, {0x100, 0x5A7A70}, {0x127, 0x5B93D2}, {0x141, 0x5B93D2}, {0x155, 0x5B93D2}};
// 0x4CCC90: 0x9C bytes
constexpr mh::CallSite kCalls4CCC90[] = {{0x6E, 0x4FBB40}};
// 0x4CCD30: 0x104 bytes
constexpr mh::CallSite kCalls4CCD30[] = {{0x85, 0x4FBB40}, {0x99, 0x4FBC30}};
// 0x4CCE40: 0xAA bytes
constexpr mh::CallSite kCalls4CCE40[] = {{0x20, 0x446770}, {0xA4, 0x4351F0}};
// 0x4CCEF0: 0xA4 bytes
constexpr mh::CallSite kCalls4CCEF0[] = {{0x3, 0x5A7B90}, {0x64, 0x5A8200}, {0x73, 0x5A8060}, {0x87, 0x5A7D70}, {0x91, 0x5A8DE0}, {0x9B, 0x5A8E00}};
// 0x4CCFA0: 0x328 bytes
constexpr mh::CallSite kCalls4CCFA0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x39, 0x5A7A00}, {0xAE, 0x5A7A00}, {0xCE, 0x5A7A50}, {0xEE, 0x5A7A00}, {0x10E, 0x5A7A50}, {0x151, 0x5A7A00}, {0x198, 0x5A7A00}, {0x1B8, 0x5A7A50}, {0x200, 0x5A7A00}, {0x220, 0x5A7A50}, {0x246, 0x5A7610}, {0x24E, 0x5A7780}, {0x2E9, 0x5A85F0}, {0x2F2, 0x5A9350}, {0x308, 0x572FA0}};
// 0x4CD2D0: 0xBC bytes
constexpr mh::CallSite kCalls4CD2D0[] = {{0x3, 0x5A7B90}, {0x56, 0x5720C0}, {0x7C, 0x5A8200}, {0x8B, 0x5A8060}, {0x9F, 0x5A7D70}, {0xA9, 0x5A8DE0}, {0xB3, 0x5A8E00}};
// 0x4CD390: 0x312 bytes
constexpr mh::CallSite kCalls4CD390[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x3D, 0x5A7A00}, {0xA0, 0x5A7A00}, {0xC0, 0x5A7A50}, {0xE0, 0x5A7A00}, {0x100, 0x5A7A50}, {0x13E, 0x5A7A00}, {0x18D, 0x5A7A00}, {0x1AD, 0x5A7A50}, {0x1F5, 0x5A7A00}, {0x215, 0x5A7A50}, {0x23B, 0x5A7610}, {0x242, 0x5A7780}, {0x2BB, 0x5A85F0}, {0x2C4, 0x5A9350}, {0x2CD, 0x461E50}, {0x2F9, 0x5A77C0}, {0x302, 0x461E50}};
// 0x4CD6B0: 0x5E bytes
constexpr mh::CallSite kCalls4CD6B0[] = {{0x4B, 0x4B7D40}, {0x50, 0x4CD840}, {0x55, 0x5A7BC0}};
constexpr mh::Imm kImms4CD6B0[] = {{0xF, 0x4CD710}, {0x17, 0x4CD7E0}, {0x22, 0x4DA3B0}, {0x2A, 0x4CD820}, {0x32, 0x4F7350}};
// 0x4CD710: 0xCA bytes
constexpr mh::CallSite kCalls4CD710[] = {{0x36, 0x4FC0E0}, {0x6B, 0x435180}, {0xBF, 0x587900}};
// 0x4CD7E0: 0x32 bytes
constexpr mh::CallSite kCalls4CD7E0[] = {{0x21, 0x452F70}};
// 0x4CD820: 0x1D bytes
// 0x4CD840: 0x1BB bytes
constexpr mh::CallSite kCalls4CD840[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x34, 0x5A7A00}, {0x61, 0x5A7A00}, {0x7A, 0x5A7A50}, {0xA1, 0x5A75F0}, {0xA8, 0x5A7780}, {0xD6, 0x5A7A00}, {0xEF, 0x5A7A50}, {0x141, 0x5A84A0}, {0x147, 0x5A9310}, {0x17C, 0x461E50}, {0x1A2, 0x5A77C0}, {0x1AB, 0x461E50}};
// 0x4CDA00: 0x12 bytes; +0xB note: jmp through .data 0x65b770, 1 code entries (a data_tables entry)
// 0x4CDA20: 0x33 bytes; +0xB note: call through .data 0x65b79c, 5 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4CDA20[] = {{0x23, 0x4CDCF0}, {0x28, 0x4CDDC0}, {0x2D, 0x5A7BC0}};
// 0x4CDA60: 0xC9 bytes
// 0x4CDB30: 0x5E bytes
constexpr mh::CallSite kCalls4CDB30[] = {{0x1A, 0x5A7A00}};
// 0x4CDB90: 0xB8 bytes
constexpr mh::CallSite kCalls4CDB90[] = {{0x1A, 0x5A7A00}, {0x66, 0x587900}};
// 0x4CDC50: 0x3B bytes
// 0x4CDC90: 0x57 bytes
constexpr mh::CallSite kCalls4CDC90[] = {{0x51, 0x4351F0}};
// 0x4CDCF0: 0xC5 bytes
constexpr mh::CallSite kCalls4CDCF0[] = {{0x3, 0x5A7B90}, {0x85, 0x5A8200}, {0x94, 0x5A8060}, {0xA8, 0x5A7D70}, {0xB2, 0x5A8DE0}, {0xBC, 0x5A8E00}};
// 0x4CDDC0: 0x332 bytes
constexpr mh::CallSite kCalls4CDDC0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x39, 0x5A7A00}, {0x7B, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xBB, 0x5A7A00}, {0xDB, 0x5A7A50}, {0x117, 0x5A7A00}, {0x15E, 0x5A7A00}, {0x17E, 0x5A7A50}, {0x1C6, 0x5A7A00}, {0x1E6, 0x5A7A50}, {0x20C, 0x5A7610}, {0x214, 0x5A7780}, {0x2F6, 0x5A85F0}, {0x2FC, 0x5A9350}, {0x312, 0x572FA0}};
// 0x4CE100: 0x36 bytes
constexpr mh::Imm kImms4CE100[] = {{0xF, 0x4CE140}, {0x17, 0x4CE4A0}, {0x22, 0x4CE500}, {0x2A, 0x4CEA20}};
// 0x4CE140: 0x351 bytes
constexpr mh::CallSite kCalls4CE140[] = {{0xB3, 0x5720C0}, {0xED, 0x4456C0}, {0x104, 0x5720C0}, {0x14E, 0x5720C0}, {0x189, 0x5720C0}, {0x1CA, 0x5720C0}, {0x31C, 0x587900}};
constexpr mh::JumpTable kTables4CE140[] = {{0x303, 0x330, 2}};
// 0x4CE4A0: 0x57 bytes
// 0x4CE500: 0x516 bytes
constexpr mh::CallSite kCalls4CE500[] = {{0x2F, 0x5A7A00}, {0x4F, 0x5A7A00}, {0x7F, 0x5A7A00}, {0xA1, 0x5A7A00}, {0x36C, 0x5720C0}, {0x3A5, 0x4456C0}, {0x3C9, 0x5720C0}, {0x3E2, 0x5720C0}, {0x422, 0x5720C0}, {0x45A, 0x5720C0}, {0x498, 0x5720C0}};
// 0x4CEA20: 0x11C bytes
constexpr mh::CallSite kCalls4CEA20[] = {{0xFD, 0x4530D0}, {0x10C, 0x4351F0}};
// 0x4CF5F0: 0x2E bytes
constexpr mh::Imm kImms4CF5F0[] = {{0xF, 0x4CF620}, {0x17, 0x4CF740}, {0x22, 0x43FE80}};
// 0x4CF620: 0x114 bytes
constexpr mh::CallSite kCalls4CF620[] = {{0x6, 0x435180}, {0x3A, 0x435180}, {0x86, 0x435180}, {0x103, 0x587900}};
// 0x4CF740: 0x24 bytes
constexpr mh::CallSite kCalls4CF740[] = {{0x13, 0x4530D0}};
// 0x4CF770: 0x12 bytes; +0xB note: jmp through .data 0x65b7d8, 6 code entries (a data_tables entry)
// 0x4CF790: 0x33 bytes; +0xB note: call through .data 0x65b7e4, 3 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4CF790[] = {{0x23, 0x4B7D40}, {0x28, 0x4CF8C0}, {0x2D, 0x5A7BC0}};
// 0x4CF7D0: 0x48 bytes
// 0x4CF820: 0x4F bytes
constexpr mh::CallSite kCalls4CF820[] = {{0x13, 0x452F70}};
// 0x4CF870: 0x46 bytes
constexpr mh::CallSite kCalls4CF870[] = {{0x40, 0x4351F0}};
// 0x4CF8C0: 0x453 bytes
constexpr mh::CallSite kCalls4CF8C0[] = {{0xD8, 0x5A7A50}, {0xF1, 0x5A7A50}, {0x10B, 0x5A7A00}, {0x125, 0x5A7A50}, {0x13E, 0x5A7A00}, {0x15E, 0x5A7A00}, {0x178, 0x5A7A50}, {0x191, 0x5A7A00}, {0x1EE, 0x5A7A00}, {0x207, 0x5A7A50}, {0x23A, 0x5A7A00}, {0x253, 0x5A7A50}, {0x299, 0x5A77C0}, {0x2A4, 0x572FA0}, {0x2B0, 0x5A7630}, {0x2B8, 0x5A7780}, {0x2EE, 0x5A85F0}, {0x2F4, 0x5A93A0}, {0x307, 0x5A79A0}, {0x317, 0x5A79E0}, {0x3FE, 0x572FA0}};
// 0x4CFD20: 0x29 bytes; +0xB note: call through .data 0x65b808, 8 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4CFD20[] = {{0x23, 0x4CFEB0}};
// 0x4CFD50: 0x8E bytes
// 0x4CFDE0: 0x2F bytes
constexpr mh::CallSite kCalls4CFDE0[] = {{0x0, 0x4FBD10}};
// 0x4CFE10: 0x3A bytes
constexpr mh::CallSite kCalls4CFE10[] = {{0x0, 0x4FBD10}};
// 0x4CFE50: 0x58 bytes
constexpr mh::CallSite kCalls4CFE50[] = {{0x0, 0x4FBD10}, {0x52, 0x4351F0}};
// 0x4CFEB0: 0x34B bytes
constexpr mh::CallSite kCalls4CFEB0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x37, 0x5A7630}, {0x3E, 0x5A7780}, {0xCE, 0x5A7A00}, {0x10B, 0x5A7A50}, {0x146, 0x5A7A00}, {0x181, 0x5A7A50}, {0x1BC, 0x5A7A00}, {0x1F7, 0x5A7A50}, {0x22C, 0x5A7A00}, {0x260, 0x5A7A50}, {0x293, 0x5A79A0}, {0x2A3, 0x5A79E0}, {0x33F, 0x572FA0}};
// 0x4D0200: 0x33 bytes; +0xB note: call through .data 0x65b818, 4 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4D0200[] = {{0x23, 0x4B7D40}, {0x28, 0x4D02C0}, {0x2D, 0x5A7BC0}};
// 0x4D0240: 0x46 bytes
// 0x4D0290: 0x2A bytes
// 0x4D02C0: 0x1E3 bytes
constexpr mh::CallSite kCalls4D02C0[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x37, 0x5A7A50}, {0x5E, 0x5A7A50}, {0xA6, 0x5A7A00}, {0xBF, 0x5A7A50}, {0xFB, 0x5A75F0}, {0x103, 0x5A7780}, {0x131, 0x5A7A00}, {0x14A, 0x5A7A50}, {0x1B3, 0x5A84A0}, {0x1B9, 0x5A9310}, {0x1C2, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Cyclone_Task", 0x4CC970, 0x2E, nullptr, 0, kImms4CC970, MH_N(kImms4CC970), nullptr, 0, reinterpret_cast<const void*>(&::Cyclone_Task)},
    {"Cyclone_Start", 0x4CC9A0, 0xC6, kCalls4CC9A0, MH_N(kCalls4CC9A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Cyclone_Start)},
    {"Cyclone_Wait", 0x4CCA70, 0x26, kCalls4CCA70, MH_N(kCalls4CCA70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Cyclone_Wait)},
    {"FxFunnel_Dispatch", 0x4CCAA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Dispatch)},
    {"FxFunnel_Task", 0x4CCAC0, 0x42, kCalls4CCAC0, MH_N(kCalls4CCAC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Task)},
    {"FxFunnel_Wait", 0x4CCB10, 0x17C, kCalls4CCB10, MH_N(kCalls4CCB10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Wait)},
    {"FxFunnel_Approach", 0x4CCC90, 0x9C, kCalls4CCC90, MH_N(kCalls4CCC90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Approach)},
    {"FxFunnel_Orbit", 0x4CCD30, 0x104, kCalls4CCD30, MH_N(kCalls4CCD30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Orbit)},
    {"FxFunnel_Rise", 0x4CCE40, 0xAA, kCalls4CCE40, MH_N(kCalls4CCE40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Rise)},
    {"FxFunnel_PushMatrix", 0x4CCEF0, 0xA4, kCalls4CCEF0, MH_N(kCalls4CCEF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_PushMatrix)},
    {"FxFunnel_Draw", 0x4CCFA0, 0x328, kCalls4CCFA0, MH_N(kCalls4CCFA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_Draw)},
    {"FxFunnel_PushGroundMatrix", 0x4CD2D0, 0xBC, kCalls4CD2D0, MH_N(kCalls4CD2D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_PushGroundMatrix)},
    {"FxFunnel_DrawGround", 0x4CD390, 0x312, kCalls4CD390, MH_N(kCalls4CD390), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxFunnel_DrawGround)},
    {"Typhoon_Task", 0x4CD6B0, 0x5E, kCalls4CD6B0, MH_N(kCalls4CD6B0), kImms4CD6B0, MH_N(kImms4CD6B0), nullptr, 0, reinterpret_cast<const void*>(&::Typhoon_Task)},
    {"Typhoon_Start", 0x4CD710, 0xCA, kCalls4CD710, MH_N(kCalls4CD710), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Typhoon_Start)},
    {"Typhoon_Grow", 0x4CD7E0, 0x32, kCalls4CD7E0, MH_N(kCalls4CD7E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Typhoon_Grow)},
    {"Typhoon_Fade", 0x4CD820, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Typhoon_Fade)},
    {"Typhoon_DrawFan", 0x4CD840, 0x1BB, kCalls4CD840, MH_N(kCalls4CD840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Typhoon_DrawFan)},
    {"FxSpiral_Dispatch", 0x4CDA00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Dispatch)},
    {"FxSpiral_Task", 0x4CDA20, 0x33, kCalls4CDA20, MH_N(kCalls4CDA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Task)},
    {"FxSpiral_Wait", 0x4CDA60, 0xC9, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Wait)},
    {"FxSpiral_Grow", 0x4CDB30, 0x5E, kCalls4CDB30, MH_N(kCalls4CDB30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Grow)},
    {"FxSpiral_Hold", 0x4CDB90, 0xB8, kCalls4CDB90, MH_N(kCalls4CDB90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Hold)},
    {"FxSpiral_Spread", 0x4CDC50, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Spread)},
    {"FxSpiral_Fade", 0x4CDC90, 0x57, kCalls4CDC90, MH_N(kCalls4CDC90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Fade)},
    {"FxSpiral_PushMatrix", 0x4CDCF0, 0xC5, kCalls4CDCF0, MH_N(kCalls4CDCF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_PushMatrix)},
    {"FxSpiral_Draw", 0x4CDDC0, 0x332, kCalls4CDDC0, MH_N(kCalls4CDDC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxSpiral_Draw)},
    {"Quake_Task", 0x4CE100, 0x36, nullptr, 0, kImms4CE100, MH_N(kImms4CE100), nullptr, 0, reinterpret_cast<const void*>(&::Quake_Task)},
    {"Quake_Start", 0x4CE140, 0x351, kCalls4CE140, MH_N(kCalls4CE140), nullptr, 0, kTables4CE140, MH_N(kTables4CE140), reinterpret_cast<const void*>(&::Quake_Start)},
    {"Quake_Rumble", 0x4CE4A0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Quake_Rumble)},
    {"Quake_Heave", 0x4CE500, 0x516, kCalls4CE500, MH_N(kCalls4CE500), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Quake_Heave)},
    {"Quake_End", 0x4CEA20, 0x11C, kCalls4CEA20, MH_N(kCalls4CEA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Quake_End)},
    {"Simoon_Task", 0x4CF5F0, 0x2E, nullptr, 0, kImms4CF5F0, MH_N(kImms4CF5F0), nullptr, 0, reinterpret_cast<const void*>(&::Simoon_Task)},
    {"Simoon_Start", 0x4CF620, 0x114, kCalls4CF620, MH_N(kCalls4CF620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Simoon_Start)},
    {"Simoon_Wait", 0x4CF740, 0x24, kCalls4CF740, MH_N(kCalls4CF740), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Simoon_Wait)},
    {"SimoonFx_Dispatch", 0x4CF770, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonFx_Dispatch)},
    {"SimoonDome_Task", 0x4CF790, 0x33, kCalls4CF790, MH_N(kCalls4CF790), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDome_Task)},
    {"SimoonDome_Start", 0x4CF7D0, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDome_Start)},
    {"SimoonDome_Grow", 0x4CF820, 0x4F, kCalls4CF820, MH_N(kCalls4CF820), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDome_Grow)},
    {"SimoonDome_Expand", 0x4CF870, 0x46, kCalls4CF870, MH_N(kCalls4CF870), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDome_Expand)},
    {"SimoonDome_Draw", 0x4CF8C0, 0x453, kCalls4CF8C0, MH_N(kCalls4CF8C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDome_Draw)},
    {"SimoonDust_Task", 0x4CFD20, 0x29, kCalls4CFD20, MH_N(kCalls4CFD20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Task)},
    {"SimoonDust_Wait", 0x4CFD50, 0x8E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Wait)},
    {"SimoonDust_Rise", 0x4CFDE0, 0x2F, kCalls4CFDE0, MH_N(kCalls4CFDE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Rise)},
    {"SimoonDust_Arc", 0x4CFE10, 0x3A, kCalls4CFE10, MH_N(kCalls4CFE10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Arc)},
    {"SimoonDust_Fall", 0x4CFE50, 0x58, kCalls4CFE50, MH_N(kCalls4CFE50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Fall)},
    {"SimoonDust_Draw", 0x4CFEB0, 0x34B, kCalls4CFEB0, MH_N(kCalls4CFEB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonDust_Draw)},
    {"SimoonFan_Task", 0x4D0200, 0x33, kCalls4D0200, MH_N(kCalls4D0200), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonFan_Task)},
    {"SimoonFan_Start", 0x4D0240, 0x46, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonFan_Start)},
    {"SimoonFan_Grow", 0x4D0290, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonFan_Grow)},
    {"SimoonFan_Draw", 0x4D02C0, 0x1E3, kCalls4D02C0, MH_N(kCalls4D02C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SimoonFan_Draw)},
};
#undef MH_N
enum : unsigned {
    kCyclone_Task,
    kCyclone_Start,
    kCyclone_Wait,
    kFxFunnel_Dispatch,
    kFxFunnel_Task,
    kFxFunnel_Wait,
    kFxFunnel_Approach,
    kFxFunnel_Orbit,
    kFxFunnel_Rise,
    kFxFunnel_PushMatrix,
    kFxFunnel_Draw,
    kFxFunnel_PushGroundMatrix,
    kFxFunnel_DrawGround,
    kTyphoon_Task,
    kTyphoon_Start,
    kTyphoon_Grow,
    kTyphoon_Fade,
    kTyphoon_DrawFan,
    kFxSpiral_Dispatch,
    kFxSpiral_Task,
    kFxSpiral_Wait,
    kFxSpiral_Grow,
    kFxSpiral_Hold,
    kFxSpiral_Spread,
    kFxSpiral_Fade,
    kFxSpiral_PushMatrix,
    kFxSpiral_Draw,
    kQuake_Task,
    kQuake_Start,
    kQuake_Rumble,
    kQuake_Heave,
    kQuake_End,
    kSimoon_Task,
    kSimoon_Start,
    kSimoon_Wait,
    kSimoonFx_Dispatch,
    kSimoonDome_Task,
    kSimoonDome_Start,
    kSimoonDome_Grow,
    kSimoonDome_Expand,
    kSimoonDome_Draw,
    kSimoonDust_Task,
    kSimoonDust_Wait,
    kSimoonDust_Rise,
    kSimoonDust_Arc,
    kSimoonDust_Fall,
    kSimoonDust_Draw,
    kSimoonFan_Task,
    kSimoonFan_Start,
    kSimoonFan_Grow,
    kSimoonFan_Draw,
};

#define S23_OURS(name) #name, ::bof3::addr::name, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&::name))
#define S23_RAW(name, address) name, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
using mh::Answer;

// 0x4FBB40's answer is the new orbit angle; FxFunnel_Orbit compares its
// distance from the last (+0x10 by then) with 0x400 and 0xC00. Half the time
// it lands on either bound or one beside it.
std::uint32_t OrbitAnswer(const std::uint32_t*, std::uint32_t answer) {
    if (answer & 1) return answer;
    static const int kD[] = {0x400, 0x401, 0x3FF, 0xC00, 0xBFF, 0xC01, -0x400, -0x401, -0xC00, -0xBFF};
    const int last = static_cast<int>(move_script::Long(Sprite_Current + 0x10)) & 0xFFF;
    return (answer & 0xFFFFF000u) | (static_cast<std::uint32_t>(last - kD[(answer >> 1) % 10]) & 0xFFF);
}
// AreaMap_Elevation's answer (ax is read) a third of the time 0xFF..0x101,
// the heights the seed gives Quake's kept enemy heights, so the hover
// compare meets its equal case.
std::uint32_t GroundAnswer(const std::uint32_t*, std::uint32_t answer) {
    if (answer % 3) return answer;
    return (answer & 0xFFFF0000u) | (0x100u + (answer >> 2) % 3 - 1);
}

const mh::Callee kCallees[] = {
    // called for real on both sides: deterministic in the regions below
    {S23_OURS(Gte_PushMatrix), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_RotTrans), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_RotMatrix), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_MulMatrix0), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_SetRotMatrix), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_SetTransMatrix), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_RotTransPers3), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_RotTransPers4), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_PrimDepths3_10B), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_PrimDepths4_10B), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gte_PrimDepths4_14), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Math_Sin), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Math_Cos), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Math_Ratan2), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_SetDrawMode), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_SetPolyG3), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_SetPolyG4), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_SetPolyGT4), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_SetSemiTrans), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_GetTPage), 0, {}, Answer::kThrough, 0, 0},
    {S23_OURS(Gpu_GetClut), 0, {}, Answer::kThrough, 0, 0},
    // recorders
    {S23_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {S23_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, Answer::kGarbage, 0, 0},
    {S23_OURS(AreaMap_Elevation), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, &GroundAnswer},
    {S23_OURS(Battle_ActorIsOut), 1, {kU8}, Answer::kFlag, 0, 0},
    {S23_RAW("0x446770", kTurnByFacing), 1, {kAll}, Answer::kGarbage, 0, 0},
    {S23_RAW("0x4FBB40", kOrbitRecord), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, &OrbitAnswer},
    {S23_RAW("0x4FBC30", kNearRecord), 2, {kAll, kAll}, Answer::kBool, 0, 0},
    {S23_RAW("0x4FC0E0", kSideCentre), 0, {}, Answer::kGarbage, 0, 0},
    // the group's own, as the tasks call them
    {S23_OURS(FxFunnel_PushMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(FxFunnel_Draw), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(FxFunnel_PushGroundMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(FxFunnel_DrawGround), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(Typhoon_DrawFan), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(FxSpiral_PushMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(FxSpiral_Draw), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(SimoonDome_Draw), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(SimoonDust_Draw), 0, {}, Answer::kGarbage, 0, 0},
    {S23_OURS(SimoonFan_Draw), 0, {}, Answer::kGarbage, 0, 0},
};
#undef S23_OURS
#undef S23_RAW

const mh::DataTable kDataTables[] = {
    {at::kFunnelTypes, 1}, {at::kFunnelPhases, 4}, {at::kSpiralTypes, 1}, {at::kSpiralPhases, 5},
    {at::kSimoonTypes, 3}, {at::kDomePhases, 3},   {at::kDustPhases, 4},  {at::kFanPhases, 4},
};

// The primitives the draws build: Gfx_PacketNext is put inside, and moved
// inside by the disturbance (the recorders for MapView_LinkPrimAt and
// Gfx_CommitPrim do not advance it).
constexpr unsigned kPrimBytes = 0x400;
alignas(16) unsigned char g_prims[kPrimBytes];

constexpr std::uint32_t kGte = 0x7DE428;          // Gte_RampFar .. Gte_Depth: the whole GTE state
constexpr std::uint32_t kGteSize = 0x380;
constexpr std::uint32_t kGteDepth = 0x7DE450;     // Gte_MatrixDepth: kept 0..0x15 (a negative one writes below the stack)
constexpr std::uint32_t kPacketNext = 0x7E0670;   // Gfx_PacketNext

const mh::Region kRegions[] = {
    {0x903800, 4},                    // Camera_ShiftY
    {at::kR, 0x10},                   // the effect scratch 0x903850..0x90385F
    {at::kV0, 0x20},                  // Prim_VertexScratch
    {kGte, kGteSize},
    {0x905E40, 0x30},                 // Camera_Matrix, the view's cell words, MapView_Redraw
    {kPacketNext, 4},
    {0, kPrimBytes},                  // g_prims: its address is filled in by SelfTest
    {0x695990, 0x2A4},                // Quake's .bss
    {at::kSpriteObjects, 30 * 0xA4},
    {at::kSpriteExtra, 4 * 0xA4},
    {at::kEffectObjects, 20 * 0x80},
    {at::kMapCells, at::kMapCellsEnd - at::kMapCells},
    {at::kArea - 4, 0x800},           // the loaded area's first 0x800 bytes: header, cell runs, corners
    {at::kClutRow, 0x40},
    {at::kClutSource, 0x40},
};
mh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

unsigned char* Sc() { return Sprite_Current; }

// The map's cell runs (Quake_Start / Quake_End walk them; a run that does
// not land on its end runs away, as in the original): MapView_Cells all 0 but
// up to a dozen cells naming four runs at dwords 0x100, 0x130, 0x160 and
// 0x190 of the area, 1..8 long, stepped by 1..3, their codes the ones the
// walks switch or any. The area's width and height 0..16, so that Quake_Heave
// stays in the region.
void SeedArea() {
    unsigned char* const area = mh::Mem(at::kArea);
    area[0] = static_cast<unsigned char>(mh::Next() % 17);
    area[1] = static_cast<unsigned char>(mh::Next() % 17);
    const std::uint32_t base = 0x80 + mh::Next() % 0x40;
    SetWord(mh::Mem(at::kAreaCellBase), base);
    std::memset(mh::Mem(at::kMapCells), 0, at::kMapCellsEnd - at::kMapCells);
    for (unsigned b = 0; b < 4; ++b) {
        const std::uint32_t index = 0x100 + 0x30 * b;
        const unsigned n = 1 + mh::Next() % 8;
        SetWord(mh::Mem(at::kArea + (index - 1) * 4 + 2), n);
        for (unsigned pos = 0; pos + 1 < n;) {
            unsigned step = 1 + mh::Next() % 3;
            if (step > n - 1 - pos) step = n - 1 - pos;
            unsigned char* const e = mh::Mem(at::kArea + (index + pos) * 4);
            e[2] = static_cast<unsigned char>(step);
            e[3] = static_cast<unsigned char>(MH_PICK(0, 0x21, 0x22, 0x27, 0x28, 0x29, 0x2A, 0x2B, mh::Next() & 0xFF));
            pos += step;
        }
        const unsigned cells = mh::Next() % 4;
        for (unsigned c = 0; c < cells; ++c)
            SetWord(mh::Mem(at::kMapCells + 2 * (mh::Next() % 1568)), index - base);
    }
}

void Seed(unsigned k) {
    // every function: the pointers the draws and Quake follow put inside
    Gfx_PacketNext = g_prims + 4 * (mh::Next() % 16);
    mh::SetPointer(at::kActorSprite, mh::SpriteRecord(mh::Next()));
    SetLong(mh::Mem(kGteDepth), static_cast<std::int32_t>(mh::Next() % 0x16));
    SeedArea();
    mh::Mem(at::kQuakeFacing)[0] = static_cast<unsigned char>(mh::Next() % 4);
    SetWord(mh::Mem(at::kQuakeX), MH_PICK(0, 1, 2, 3, 5, 8, 12, 15, 16, static_cast<std::uint32_t>(-1),
                                          static_cast<std::uint32_t>(-4), static_cast<std::uint32_t>(-15), 0x7FFF, 0x8000));
    SetWord(mh::Mem(at::kQuakeY), MH_PICK(0, 1, 2, 3, 5, 8, 12, 15, 16, static_cast<std::uint32_t>(-1),
                                          static_cast<std::uint32_t>(-4), static_cast<std::uint32_t>(-15), 0x7FFF, 0x8000));
    if (mh::Half())
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>((mh::Next() % 11) | MH_PICK(0, 0x40, 0x80, 0xC0));
    mh::Mem(at::kEventBattle)[0] = static_cast<unsigned char>(
        MH_PICK(0x1C, 0x23, 0x2F, 0x34, 0x1B, 0x1D, 0x22, 0x24, 0x2E, 0x30, 0x33, 0x35, 0, mh::Next() & 0xFF));

    unsigned char* const sc = Sc();
    const auto pick = [](std::uint32_t v) { return static_cast<unsigned char>(v); };
    switch (k) {
    case kCyclone_Task: case kSimoon_Task: sc[1] = pick(mh::Next() % 3); break;
    case kTyphoon_Task:
        sc[1] = pick(mh::Next() % 5);
        if (mh::Half()) sc[0xA] = 0;
        if (mh::Half()) sc[0] = 0;
        break;
    case kQuake_Task: sc[1] = pick(mh::Next() % 4); break;
    case kCyclone_Wait: if (mh::Half()) sc[9] = 0; break;
    case kFxFunnel_Dispatch: case kFxSpiral_Dispatch: sc[1] = 0; break;
    case kSimoonFx_Dispatch: sc[1] = pick(mh::Next() % 3); break;
    case kFxFunnel_Task: case kSimoonDust_Task: case kSimoonFan_Task:
        sc[2] = pick(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        break;
    case kFxSpiral_Task:
        sc[2] = pick(mh::Next() % 5);
        if (mh::Half()) sc[0] = 0;
        break;
    case kSimoonDome_Task:
        sc[2] = pick(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        break;
    case kFxFunnel_Wait: case kFxSpiral_Spread: case kSimoonDust_Wait:
        sc[9] = pick(MH_PICK(1, 2, 0, mh::Next()));
        if (mh::Often()) sc[0xB] = pick(mh::Next() % 2);
        break;
    case kFxFunnel_Approach: sc[0xA] = pick(MH_PICK(0xC, 0xB, 0xD, mh::Next())); break;
    case kFxFunnel_Rise: case kTyphoon_Fade: case kFxSpiral_Fade: sc[0xA] = pick(MH_PICK(1, 2, 0, 3)); break;
    case kFxFunnel_PushGroundMatrix: case kSimoonDust_Draw: case kSimoonFan_Draw:
        sc[2] = pick(MH_PICK(0, 1, 2, 3, 4, mh::Next()));
        break;
    case kTyphoon_Grow: sc[0xA] = pick(MH_PICK(0xF, 0xE, 0x10)); break;
    case kFxSpiral_Wait:
        sc[9] = pick(MH_PICK(1, 2, 0));
        sc[4] = pick(mh::Often() ? mh::Next() % 16 : mh::Next());
        break;
    case kFxSpiral_Grow: sc[0xA] = pick(MH_PICK(0xE, 0xD, 0xF, 0x10)); break;
    case kFxSpiral_Hold:
        sc[9] = pick(MH_PICK(1, 2));
        sc[4] = pick(mh::Half() ? 0 : mh::Next());
        sc[8] = pick(mh::Often() ? mh::Next() % 4 : mh::Next());
        break;
    case kQuake_Rumble: sc[9] = pick(MH_PICK(0xF, 0xE, 0x10, mh::Next())); break;
    case kQuake_Heave:
        for (unsigned i = 0; i < 8; ++i)
            if (mh::Often()) SetWord(mh::Mem(at::kQuakeEnemyZ + 2 * i), 0xFF + mh::Next() % 3);
        sc[9] = pick(MH_PICK(0x5A, 0x59, 0x5B, mh::Next()));
        if (mh::Half()) std::memset(mh::Mem(at::kQuakeLift), 0, 0xF0);
        if (mh::Half()) mh::Mem(at::kArea)[mh::Next() & 1] = 0;
        break;
    case kSimoon_Wait: if (mh::Half()) sc[0xB] = 0; break;
    case kSimoonDome_Grow: sc[9] = pick(MH_PICK(3, 4, 5, 7, 8, mh::Next())); break;
    case kSimoonDome_Expand: sc[9] = pick(MH_PICK(0x27, 0x26, 0x28, mh::Next())); break;
    case kSimoonDome_Draw: sc[9] = pick(MH_PICK(0x10, 0x11, 0x12, 0x18, 0x19, 0x20, 0x21, 0x30, mh::Next())); break;
    case kSimoonDust_Rise: case kSimoonFan_Grow: sc[9] = pick(MH_PICK(7, 6, 8)); break;
    case kSimoonDust_Arc: sc[9] = pick(MH_PICK(0x1B, 0x1A, 0x1C, mh::Next())); break;
    case kSimoonDust_Fall:
        sc[9] = pick(MH_PICK(0x2C, 0x2B, 0x2D, mh::Next()));
        sc[0xA] = pick(MH_PICK(1, 2, 0));
        break;
    default: break;
    }
}

// The group's cells a caller reads again after a call.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = g_prims + 4 * (v % 16); break;
    case 1: SetWord(mh::Mem(at::kR + 2 * (v % 8)), h >> 16); break;
    case 2: SetWord(mh::Mem(at::kV0 + 2 * (v % 16)), h >> 16); break;
    case 3: mh::Mem(at::kQuakeHover)[0] = static_cast<unsigned char>(v & 1); break;
    case 4: SetWord(mh::Mem(at::kQuakeEnemyZ + 2 * (v % 8)), h >> 16); break;
    case 5: SetWord(mh::Mem(at::kQuakeEnemyUp + 2 * (v % 8)), h >> 16); break;
    case 6: SetWord(mh::Mem(at::kQuakeSpriteZ + 2 * (v % 30)), h >> 16); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    for (mh::Region& r : g_regions)
        if (r.at == 0) r.at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    const mh::Group group = {
        "magic_s23",
        kClones,
        sizeof kClones / sizeof kClones[0],
        kCallees,
        sizeof kCallees / sizeof kCallees[0],
        kDataTables,
        sizeof kDataTables / sizeof kDataTables[0],
        g_regions,
        sizeof g_regions / sizeof g_regions[0],
        &Seed,
        &Disturb,
        2000,
    };
    mh::Run(group);
}

}  // namespace magic_s23
