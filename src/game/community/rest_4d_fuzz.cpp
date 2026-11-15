// BOF3X_SHADOW=rest_4d: group R4D's 60 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_4d.md section 4. BOF3X_R4D_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R4D --clones --harness scenario
// (2026-10-05), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them. Shapes: the dispatchers and the states
// kState (void, no arguments), the draw helpers kCall with their arguments set
// by Args; CommuDraw_RandBelow and CommuName_CountSlots answer al,
// CommuName_NthSlot a whole eax. The ten .data state tables are DataTables
// (their entries recorders while the fuzz runs); each dispatcher's index byte
// is seeded below its own table's count. Every callee the standard set types
// otherwise is re-listed here (registered before the standard rows: the
// group's listing stands) with what the callee reads, cited.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/community/commu_name.h"
#include "game/menu/name_grid.h"
#include "game/move_script_bytes.h"
#include "game/rest_4d.h"
#include "game/rest_4d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_4d {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::SetLong;
using move_script::SetWord;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char& B(U a) { return *sh::Mem(a); }
void SetW(U a, U v) { SetWord(sh::Mem(a), v); }

// --- the clone table (band_rows.py --group R4D --clones --harness scenario, 2026-10-05) ---
constexpr sh::CallSite kCalls45C400[] = {{0x1B, 0x57CF60}, {0x22, 0x45C700}, {0x59, 0x5B9380}, {0x7A, 0x516F60}, {0x89, 0x5B9380},
                                         {0x9A, 0x516F60}, {0xAB, 0x516F60}, {0xE7, 0x5B9380}, {0x108, 0x516F60}, {0x120, 0x5B9380},
                                         {0x134, 0x516F60}, {0x14F, 0x5B9380}, {0x163, 0x516F60}, {0x17A, 0x517090}, {0x1C0, 0x5B9380},
                                         {0x1E5, 0x516F60}, {0x1FD, 0x5B9380}, {0x20E, 0x516F60}, {0x229, 0x5B9380}, {0x23A, 0x516F60},
                                         {0x252, 0x517090}, {0x26D, 0x5B9380}, {0x292, 0x516F60}, {0x2A1, 0x5B9380}, {0x2B2, 0x516F60},
                                         {0x2C3, 0x516F60}, {0x2DE, 0x517090}};
constexpr sh::CallSite kCalls45C700[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x2C, 0x45B400}, {0x41, 0x45B400}, {0x56, 0x45B400},
                                         {0x6F, 0x45B400}, {0x7C, 0x45B400}, {0x91, 0x45B400}, {0xA3, 0x45B400}, {0xB9, 0x45B400}};
constexpr sh::CallSite kCalls45C7D0[] = {{0x61, 0x45B2C0}};
constexpr sh::CallSite kCalls45C850[] = {{0x25, 0x45B2C0}, {0x4F, 0x45B2C0}};
constexpr sh::CallSite kCalls45C8E0[] = {{0x2, 0x495040}};
constexpr sh::CallSite kCalls45C900[] = {{0x1D, 0x587B40}, {0x26, 0x587AE0}};
constexpr sh::CallSite kCalls45C960[] = {{0x7, 0x5B93D2}, {0x72, 0x45CE80}, {0x12F, 0x5B93D2}, {0x187, 0x5B93D2}, {0x1B0, 0x5B93D2},
                                         {0x20B, 0x45CE80}, {0x23C, 0x5B93D2}, {0x25C, 0x5B93D2}, {0x2AE, 0x45CE80}};
constexpr sh::CallSite kCalls45CC50[] = {{0x3, 0x45CEA0}, {0x15, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x33, 0x461E50}, {0x3F, 0x5A7610},
                                         {0x47, 0x5A7780}, {0xDA, 0x461E50}, {0xE6, 0x5A7740}, {0x130, 0x461E50}};
constexpr sh::CallSite kCalls45CDC0[] = {{0x0, 0x45CEA0}, {0x11, 0x495040}};
constexpr sh::CallSite kCalls45CDF0[] = {{0xC, 0x587B40}, {0x1E, 0x587AE0}, {0x2C, 0x495040}, {0x41, 0x45CEA0}};
constexpr sh::CallSite kCalls45CE40[] = {{0xF, 0x4976D0}};
constexpr sh::CallSite kCalls45CE80[] = {{0x1, 0x5B93D2}, {0x11, 0x5B93D2}};
constexpr sh::CallSite kCalls45CEA0[] = {{0x166, 0x516B30}, {0x18E, 0x516B30}};
constexpr sh::CallSite kCalls45D050[] = {{0xE, 0x4976D0}};
constexpr sh::CallSite kCalls45D090[] = {{0xE, 0x587740}};
constexpr sh::CallSite kCalls45D0D0[] = {{0x2C, 0x586160}, {0x3D, 0x45E6D0}, {0x60, 0x45E870}, {0x7F, 0x45E700}};
constexpr sh::CallSite kCalls45D170[] = {{0x1E, 0x587740}, {0x4C, 0x587740}, {0x63, 0x461EB0}, {0x77, 0x587740}, {0x95, 0x45E6B0},
                                         {0xB1, 0x45E6B0}, {0xCD, 0x45E820}, {0xDB, 0x45E6D0}, {0xE8, 0x45E870}, {0xF1, 0x45E700},
                                         {0x10A, 0x45ECC0}};
constexpr sh::CallSite kCalls45D290[] = {{0x26, 0x587740}, {0x4D, 0x587740}, {0x57, 0x587740}, {0x8B, 0x587740}, {0xA7, 0x461EB0},
                                         {0xC8, 0x587740}, {0xDB, 0x45E820}, {0x102, 0x5905D0}, {0x10E, 0x45E700}, {0x127, 0x45ECC0},
                                         {0x138, 0x45E6D0}, {0x145, 0x45E870}};
constexpr sh::CallSite kCalls45D3E0[] = {{0x2C, 0x586160}, {0x46, 0x45E700}, {0x56, 0x45E6D0}, {0x63, 0x45E870}, {0x79, 0x4976D0}};
constexpr sh::CallSite kCalls45D480[] = {{0x31, 0x45E6D0}, {0x3E, 0x45E870}};
constexpr sh::CallSite kCalls45D4E0[] = {{0x2, 0x45ED70}, {0x24, 0x4976D0}, {0x4A, 0x45E6D0}, {0x57, 0x45E870}};
constexpr sh::CallSite kCalls45D550[] = {{0x17, 0x587740}, {0x43, 0x45E6D0}, {0x50, 0x45E870}};
constexpr sh::CallSite kCalls45D5B0[] = {{0x14, 0x45E6D0}, {0x37, 0x45E870}, {0x48, 0x45F650}};
constexpr sh::CallSite kCalls45D610[] = {{0xA, 0x45E6D0}, {0x17, 0x45E870}, {0x3A, 0x437CC0}, {0x58, 0x45F1A0}, {0x86, 0x437CC0},
                                         {0xA6, 0x45E6D0}, {0xCF, 0x45E6D0}};
constexpr sh::CallSite kCalls45D730[] = {{0x8, 0x45E6D0}, {0x15, 0x45E870}, {0x33, 0x57CEF0}, {0x45, 0x437CC0}, {0x52, 0x45F1A0},
                                         {0x6F, 0x437CC0}, {0x77, 0x43C9F0}};
constexpr sh::CallSite kCalls45D7C0[] = {{0x1F, 0x45E6D0}, {0x35, 0x45E6D0}, {0x58, 0x45E870}, {0x7E, 0x437CC0}, {0x9C, 0x45F1A0},
                                         {0xCA, 0x437CC0}, {0xFB, 0x45E6D0}, {0x124, 0x4976D0}, {0x145, 0x4976D0}};
constexpr sh::CallSite kCalls45D930[] = {{0x8, 0x45E6D0}, {0x15, 0x45E870}};
constexpr sh::CallSite kCalls45D990[] = {{0x14, 0x45E6D0}, {0x37, 0x45E870}, {0x48, 0x45F650}};
constexpr sh::CallSite kCalls45D9E0[] = {{0x2C, 0x586160}, {0x3D, 0x45E6D0}, {0x60, 0x45E870}, {0x7F, 0x45E700}, {0x95, 0x4976D0}};
constexpr sh::CallSite kCalls45DAB0[] = {{0x31, 0x586160}, {0xAB, 0x45EE10}};
constexpr sh::CallSite kCalls45DB90[] = {{0x20, 0x587740}, {0x4E, 0x587740}, {0x58, 0x587740}, {0x73, 0x45F000}, {0x8C, 0x461EB0},
                                         {0xA0, 0x587740}, {0x125, 0x45EF90}, {0x161, 0x45F050}, {0x169, 0x45E820}};
constexpr sh::CallSite kCalls45DD10[] = {{0x26, 0x587740}, {0x4D, 0x587740}, {0x57, 0x587740}, {0x8B, 0x587740}, {0xA7, 0x461EB0},
                                         {0xC8, 0x587740}, {0xDB, 0x45E820}, {0x102, 0x5905D0}, {0x10A, 0x45EF90}, {0x146, 0x45F050}};
constexpr sh::CallSite kCalls45DE60[] = {{0x39, 0x586160}, {0xA2, 0x45EE10}, {0xDA, 0x45EE10}, {0x13C, 0x45EE10}, {0x177, 0x4976D0}};
constexpr sh::CallSite kCalls45E000[] = {{0x31, 0x45F020}, {0x3E, 0x45EE10}};
constexpr sh::CallSite kCalls45E060[] = {{0x2, 0x45ED70}, {0x24, 0x4976D0}, {0x4A, 0x45F020}, {0x57, 0x45EE10}};
constexpr sh::CallSite kCalls45E0D0[] = {{0x17, 0x587740}, {0x43, 0x45F020}, {0x50, 0x45EE10}};
constexpr sh::CallSite kCalls45E130[] = {{0x14, 0x45F020}, {0x35, 0x45EE10}, {0x46, 0x45F5A0}};
constexpr sh::CallSite kCalls45E190[] = {{0x18, 0x45F020}, {0x25, 0x45EE10}, {0x48, 0x437CC0}, {0x66, 0x45F1A0}, {0x94, 0x437CC0},
                                         {0xB3, 0x45F020}, {0xDE, 0x45F020}};
constexpr sh::CallSite kCalls45E2C0[] = {{0x8, 0x45F020}, {0x15, 0x45EE10}, {0x33, 0x57CEF0}, {0x45, 0x437CC0}, {0x52, 0x45F1A0},
                                         {0x6F, 0x437CC0}, {0x77, 0x43C9F0}};
constexpr sh::CallSite kCalls45E350[] = {{0x1F, 0x45F020}, {0x35, 0x45F020}, {0x58, 0x45EE10}, {0x7E, 0x437CC0}, {0x9C, 0x45F1A0},
                                         {0xCA, 0x437CC0}, {0xFB, 0x45F020}, {0x125, 0x4976D0}, {0x146, 0x4976D0}};
constexpr sh::CallSite kCalls45E4C0[] = {{0x8, 0x45F020}, {0x15, 0x45EE10}};
constexpr sh::CallSite kCalls45E520[] = {{0x14, 0x45F020}, {0x37, 0x45EE10}, {0x48, 0x45F5A0}};
constexpr sh::CallSite kCalls45E570[] = {{0x31, 0x586160}, {0xAB, 0x45EE10}, {0xD8, 0x4976D0}};
constexpr sh::CallSite kCalls45E670[] = {{0x17, 0x4976D0}};
constexpr sh::CallSite kCalls45E700[] = {{0x20, 0x57CF60}, {0x27, 0x45E770}, {0x2F, 0x45E6B0}, {0x51, 0x45EC00}};
constexpr sh::CallSite kCalls45E770[] = {{0x10, 0x45EC00}, {0x29, 0x45EC00}, {0x36, 0x45EC00}, {0x52, 0x45EC00},
                                         {0x67, 0x45EC00}, {0x74, 0x45EC00}, {0x8A, 0x45EC00}, {0x97, 0x45EC00}};
constexpr sh::CallSite kCalls45E820[] = {{0x11, 0x586160}, {0x46, 0x516B30}};

#define R4D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R4D_ROW(name, base, size, calls) #name, base, size, calls, R4D_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define R4D_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kS = sh::Shape::kState, kC = sh::Shape::kCall;
const sh::Clone kAll[] = {
    {R4D_ROW(CommuBoard_DrawRows, 0x45C400, 0x2F6, kCalls45C400), 0, false, kC},
    {R4D_ROW(CommuBoard_DrawFrame, 0x45C700, 0xC6, kCalls45C700), 0, false, kC},
    {R4D_ROW(CommuBoard_DrawRowCells, 0x45C7D0, 0x74, kCalls45C7D0), 0, false, kC},
    {R4D_ROW(CommuBoard_DrawCells, 0x45C850, 0x61, kCalls45C850), 0, false, kC},
    {R4D_LEAF(CommuDraw_Dispatch, 0x45C8C0, 0xE), 0, false, kS},
    {R4D_LEAF(CommuDraw_OpenStep, 0x45C8D0, 0xE), 0, false, kS},
    {R4D_ROW(CommuDraw_FadeOut, 0x45C8E0, 0x17, kCalls45C8E0), 0, false, kS},
    {R4D_ROW(CommuDraw_MusicIn, 0x45C900, 0x42, kCalls45C900), 0, false, kS},
    {R4D_LEAF(CommuDraw_ShowStep, 0x45C950, 0xE), 0, false, kS},
    {R4D_ROW(CommuDraw_Pick, 0x45C960, 0x2E5, kCalls45C960), 0, false, kS},
    {R4D_ROW(CommuDraw_Reveal, 0x45CC50, 0x16B, kCalls45CC50), 0, false, kS},
    {R4D_ROW(CommuDraw_WaitKey, 0x45CDC0, 0x26, kCalls45CDC0), 0, false, kS},
    {R4D_ROW(CommuDraw_MusicBack, 0x45CDF0, 0x46, kCalls45CDF0), 0, false, kS},
    {R4D_ROW(CommuDraw_Close, 0x45CE40, 0x32, kCalls45CE40), 0, false, kS},
    {R4D_ROW(CommuDraw_RandBelow, 0x45CE80, 0x20, kCalls45CE80), 0xFF, false, kC},
    {R4D_ROW(CommuDraw_DrawTitle, 0x45CEA0, 0x19A, kCalls45CEA0), 0, false, kS},
    {R4D_LEAF(CommuName_Dispatch, 0x45D040, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_Begin, 0x45D050, 0x2A, kCalls45D050), 0, false, kS},
    {R4D_LEAF(CommuName_SlotStep, 0x45D080, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_PanelReset, 0x45D090, 0x3A, kCalls45D090), 0, false, kS},
    {R4D_ROW(CommuName_SlotPanelIn, 0x45D0D0, 0x97, kCalls45D0D0), 0, false, kS},
    {R4D_ROW(CommuName_SlotChoose, 0x45D170, 0x114, kCalls45D170), 0, false, kS},
    {R4D_ROW(CommuName_SlotConfirm, 0x45D290, 0x14E, kCalls45D290), 0, false, kS},
    {R4D_ROW(CommuName_SlotPanelOut, 0x45D3E0, 0x95, kCalls45D3E0), 0, false, kS},
    {R4D_ROW(CommuName_SlotHow, 0x45D480, 0x47, kCalls45D480), 0, false, kS},
    {R4D_LEAF(CommuName_SlotRandomStep, 0x45D4D0, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_SlotRandomPick, 0x45D4E0, 0x62, kCalls45D4E0), 0, false, kS},
    {R4D_ROW(CommuName_SlotRandomAsk, 0x45D550, 0x59, kCalls45D550), 0, false, kS},
    {R4D_ROW(CommuName_SlotRandomOut, 0x45D5B0, 0x4E, kCalls45D5B0), 0, false, kS},
    {R4D_LEAF(CommuName_SlotEntryStep, 0x45D600, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_SlotEntryIn, 0x45D610, 0x119, kCalls45D610), 0, false, kS},
    {R4D_ROW(CommuName_SlotEntry, 0x45D730, 0x82, kCalls45D730), 0, false, kS},
    {R4D_ROW(CommuName_SlotEntryOut, 0x45D7C0, 0x163, kCalls45D7C0), 0, false, kS},
    {R4D_ROW(CommuName_SlotEntryAsk, 0x45D930, 0x51, kCalls45D930), 0, false, kS},
    {R4D_ROW(CommuName_SlotEntryDone, 0x45D990, 0x4E, kCalls45D990), 0, false, kS},
    {R4D_ROW(CommuName_SlotClose, 0x45D9E0, 0xB3, kCalls45D9E0), 0, false, kS},
    {R4D_LEAF(CommuName_MemberStep, 0x45DAA0, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_MemberPanelIn, 0x45DAB0, 0xDB, kCalls45DAB0), 0, false, kS},
    {R4D_ROW(CommuName_MemberChoose, 0x45DB90, 0x172, kCalls45DB90), 0, false, kS},
    {R4D_ROW(CommuName_MemberConfirm, 0x45DD10, 0x14F, kCalls45DD10), 0, false, kS},
    {R4D_ROW(CommuName_MemberPanelOut, 0x45DE60, 0x196, kCalls45DE60), 0, false, kS},
    {R4D_ROW(CommuName_MemberHow, 0x45E000, 0x47, kCalls45E000), 0, false, kS},
    {R4D_LEAF(CommuName_MemberRandomStep, 0x45E050, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_MemberRandomPick, 0x45E060, 0x62, kCalls45E060), 0, false, kS},
    {R4D_ROW(CommuName_MemberRandomAsk, 0x45E0D0, 0x59, kCalls45E0D0), 0, false, kS},
    {R4D_ROW(CommuName_MemberRandomOut, 0x45E130, 0x4C, kCalls45E130), 0, false, kS},
    {R4D_LEAF(CommuName_MemberEntryStep, 0x45E180, 0xE), 0, false, kS},
    {R4D_ROW(CommuName_MemberEntryIn, 0x45E190, 0x12B, kCalls45E190), 0, false, kS},
    {R4D_ROW(CommuName_MemberEntry, 0x45E2C0, 0x82, kCalls45E2C0), 0, false, kS},
    {R4D_ROW(CommuName_MemberEntryOut, 0x45E350, 0x164, kCalls45E350), 0, false, kS},
    {R4D_ROW(CommuName_MemberEntryAsk, 0x45E4C0, 0x51, kCalls45E4C0), 0, false, kS},
    {R4D_ROW(CommuName_MemberEntryDone, 0x45E520, 0x4E, kCalls45E520), 0, false, kS},
    {R4D_ROW(CommuName_MemberClose, 0x45E570, 0xF8, kCalls45E570), 0, false, kS},
    {R4D_ROW(CommuName_End, 0x45E670, 0x27, kCalls45E670), 0, false, kS},
    {R4D_LEAF(Commu_LeaveWhenClosed, 0x45E6A0, 0x10), 0, false, kS},
    {R4D_LEAF(CommuName_CountSlots, 0x45E6B0, 0x1A), 0xFF, false, kC},
    {R4D_LEAF(CommuName_NthSlot, 0x45E6D0, 0x29), 0xFFFFFFFFu, false, kC},
    {R4D_ROW(CommuName_DrawSlotBar, 0x45E700, 0x63, kCalls45E700), 0, false, kC},
    {R4D_ROW(CommuName_DrawSlotFrame, 0x45E770, 0xA4, kCalls45E770), 0, false, kC},
    {R4D_ROW(CommuName_DrawHeader, 0x45E820, 0x4F, kCalls45E820), 0, false, kS},
};
#undef R4D_ROW
#undef R4D_LEAF
#undef R4D_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 60, "the cut's 60 rows for R4D");

// A dispatcher's index byte and its table's count (the states its game writes,
// docs/rest_4d.md section 3).
struct Dispatch { U base, by; unsigned count; };
const Dispatch kDispatch[] = {
    {0x45C8C0, Rest4b_BoardState_addr, 3}, {0x45C8D0, Rest4b_BoardStep_addr, 2},  {0x45C950, Rest4b_BoardStep_addr, 5},  {0x45D040, Rest4b_BoardState_addr, 5},
    {0x45D080, Rest4b_BoardStep_addr, 9},  {0x45D4D0, Rest4c_Step_addr, 3}, {0x45D600, Rest4c_Step_addr, 5}, {0x45DAA0, Rest4b_BoardStep_addr, 9},
    {0x45E050, Rest4c_Step_addr, 3}, {0x45E180, Rest4c_Step_addr, 5},
};
const Dispatch* DispatchOf(U base) {
    for (const Dispatch& d : kDispatch)
        if (d.base == base) return &d;
    return nullptr;
}

const sh::DataTable kTables[] = {
    {CommuDraw_States_addr, 3},  {CommuDraw_OpenSteps_addr, 2},     {CommuDraw_ShowSteps_addr, 5},  {CommuName_States_addr, 5},
    {CommuName_SlotSteps_addr, 9},   {CommuName_SlotRandomSteps_addr, 3},   {CommuName_SlotEntrySteps_addr, 5}, {CommuName_MemberSteps_addr, 9},
    {CommuName_MemberRandomSteps_addr, 3}, {CommuName_MemberEntrySteps_addr, 5},
};

unsigned g_clone;   // the round's clone's index in kAll

// --- the moves (from a hash: Disturb's) ------------------------------------------
//
// What the functions read again after a call: the three state bytes, the
// cursor, the yes / no byte, the slide, the header word, the entry's answer
// and column, the draw's counts and picks (the first name byte half the
// time), Input_Pressed, the kept track, a slot's name bytes, a record's flags
// and name bytes. The draw's cursor stays 0..7 and its counts below 10 while
// CommuDraw_Pick runs (it indexes and writes by them; the game never moves
// them there). Not moved: the entry's three bytes 0x675FCA..CC, read again
// after calls but handed only to BareRet, which reads none (docs/rest_4d.md
// section 4).
constexpr unsigned kMoves = 15;
void Move(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    const bool pick = kAll[g_clone].base == 0x45C960;
    switch (sh::DisturbCase(h, kMoves)) {
    case 0: B(Rest4b_BoardStep_addr) = static_cast<unsigned char>(v); break;
    case 1: B(Rest4c_Step_addr) = static_cast<unsigned char>(v); break;
    case 2: B(Rest4b_BoardState_addr) = static_cast<unsigned char>(v); break;
    case 3: B(Rest4c_Count_addr) = static_cast<unsigned char>(pick ? v % 8 : v); break;
    case 4: B(Rest4c_Cursor_addr) = static_cast<unsigned char>(v & 0x10 ? v : v & 3); break;
    case 5: B(Rest4d_Count_addr) = static_cast<unsigned char>(v & 0x10 ? v : v % 6); break;
    case 6: SetW(Rest4d_Header_addr, v % 4 == 0 ? 0xFFFFu : v % 4 == 1 ? 0xF1u : v % 4 == 2 ? 0xF0u : (h >> 16) & 0x1FF); break;
    case 7: B(Rest4d_EntryDone_addr) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 8: B(::bof3::Img(at::kPicked.v) + (h >> 16) % 4) = static_cast<unsigned char>(v % 10); break;
    case 9: B(::bof3::Img(at::kName.v) + ((h >> 16) & 1 ? 0 : (h >> 17) % 0x28)) = static_cast<unsigned char>(v % 4); break;
    case 10: SetW(Input_Pressed_addr, h >> 16); break;
    case 11: B(Rest4d_EntryColumn_addr) = static_cast<unsigned char>(v); break;
    case 12: B(Rest4d_KeptTrack_addr) = static_cast<unsigned char>(v); break;
    case 13: B(::bof3::Img(at::kSlotNames.v) + ((h >> 16) & 1 ? (h >> 17) % 20 : (h >> 17) % 300)) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 14: B(CharacterRecords_addr + at::kRecordStride * ((h >> 16) % 7) + (v & 1 ? 0xB : (h >> 20) % 5)) ^= static_cast<unsigned char>(v | 1); break;
    default: break;
    }
}
void Disturb(U h) { Move(h); }


// Crt_sprintf as the standard set's FxSprintf (up to seven letters and a NUL at
// the destination), but logging the numbers each of the group's formats takes:
// three for 0x653088, one for 0x5E10C0, none for the blanks - the calls push
// two, three or five words, so the standard row's third word is the caller's
// stack in two of them.
U Sprintf(const U* a, U) {
    if (a[1] == ::bof3::Img(at::kBoardFmt3.v)) sh::Note(a[2], a[3], a[4]);
    else if (a[1] == ::bof3::Img(at::kNumberFmt.v)) sh::Note(a[2]);
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return 0;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}
// R4E's 0x45ED70: clears and writes the 0x20 bytes at 0x675F98 and answers the
// name's length + 1 (the caller copies that many with rep movs): 0..0x24.
U RandomNameAnswer(const U*, U) {
    sh::FillBytes(sh::Mem(::bof3::Img(at::kName.v)), 0x20);
    return sh::Noise() % 0x25;
}
// CommuName_NthSlot's stand-in: a slot 0..3 three times in four, so that two
// calls in one state often answer the same slot (CommuName_SlotEntryIn reads
// the slot's name byte again after its second call: control N41), and one
// call in four a name byte of the slot it answers moved (the group's own case
// 13 is too rare to reach that re-read).
U NthSlotAnswer(const U*, U answer) {
    const U n = sh::Noise();
    const U slot = n % 4 == 0 ? answer & 0xFF : (n >> 8) % 4;
    if ((n >> 4) % 4 == 0 && slot < 60) B(::bof3::Img(at::kSlotNames.v) + 5 * slot + (n >> 12) % 5) = static_cast<unsigned char>(n >> 16);
    return (answer & 0xFFFFFF00u) | slot;
}
// CommuDraw_RandBelow's stand-in: 0, the limit less two or less one, so that a
// category's draws all equal one another and wrap at the count (D13).
U RandBelowAnswer(const U* a, U answer) {
    const U n = sh::Noise();
    const U limit = a[0] & 0xFF;
    const U pick = n % 4 == 0 ? 0 : n % 4 == 1 ? limit - 2 : limit - 1;
    return n % 5 == 0 ? answer : (answer & 0xFFFFFF00u) | (pick & 0xFF);
}
// CommuName_CountSlots' and R4E's 0x45F000's stand-ins: the cursor plus 0, 1
// or 2 two times in three (the choose steps' bounds compare the cursor with the
// count: N12, N64, N67).
U NearCursor(const U*, U answer) {
    const U n = sh::Noise();
    return n % 3 == 0 ? answer : (answer & 0xFFFFFF00u) | ((B(Rest4c_Count_addr) + (n >> 8) % 3) & 0xFF);
}
// The header's stand-in: one call in four the header word moved after it
// (CommuName_SlotConfirm reads it again: N19).
U HeaderMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 == 0) SetW(Rest4d_Header_addr, (n >> 8) % 3 == 0 ? 0xF1u : (n >> 8) % 3 == 1 ? 0xFFFFu : 0xF0u);
    return answer;
}
// BareRet's stand-in: reads nothing; one call in four the first name byte moved
// (0 half the time; the entry's last step reads it again after the draws: N48).
U BareRetMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 == 0) B(::bof3::Img(at::kName.v)) = static_cast<unsigned char>((n >> 8) & 1 ? 0 : n >> 16);
    return answer;
}
// R4E's 0x45F020: a record 0..6 (eax), or 0xFF (none) - never 0xFF while
// CommuName_MemberEntryOut runs, which indexes CommuName_RecordNames by it
// (ours aborts there past 7, section 5).
U NthMemberAnswer(const U*, U answer) {
    const U n = sh::Noise();
    if (kAll[g_clone].base == 0x45E350 || n % 5 != 0) return (n >> 8) % 7;
    return n % 3 == 0 ? 0xFF : answer;
}

// R4E's 0x45EE10 (a member's panel): one call in four the cursor moved to 0..7
// after it (CommuName_MemberPanelOut compares each record's place with the
// cursor again after every panel; the group's case 3 alone reached that 3
// times in 4,000 rounds: N70). Never while CommuDraw_Pick runs (it indexes by
// the cursor; it calls no panel either).
U MemberPanelMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 == 0 && kAll[g_clone].base != 0x45C960) B(Rest4c_Count_addr) = static_cast<unsigned char>((n >> 8) % 8);
    return answer;
}

// The thin controls outside round fourteen's end lists (docs/round-14-cleanup.md
// section 7): each a stand-in that moves, after the call, the byte its caller
// reads again - only under the clone that reads it, half the time.
// Music_FadeOutStop: the kept track (CommuDraw_MusicBack plays it after the
// fade: D33).
U KeptTrackMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2 == 0 && kAll[g_clone].base == 0x45CDF0) B(Rest4d_KeptTrack_addr) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// Sound_PlayEffect: the step (CommuName_PanelReset counts it on after sound
// 0x102: N07).
U StepMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2 == 0 && kAll[g_clone].base == 0x45D090) B(Rest4b_BoardStep_addr) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// R4C's 0x45B2C0 (a cell): the column byte to 0..2 or any byte
// (CommuBoard_DrawRowCells compares it with each cell again: B12).
U ColumnMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2 == 0 && kAll[g_clone].base == 0x45C7D0)
        B(Rest4c_Cursor_addr) = static_cast<unsigned char>((n >> 8) & 0x10 ? n >> 16 : (n >> 16) % 3);
    return answer;
}
// R4E's 0x45F1A0 (the entry's box): the answer byte to 0 or not
// (CommuName_SlotEntryOut reads it again at the slide's end: N47).
U EntryDoneMove(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2 == 0 && kAll[g_clone].base == 0x45D7C0) B(Rest4d_EntryDone_addr) = static_cast<unsigned char>((n >> 8) & 1 ? 0 : n >> 16);
    return answer;
}

#define R4D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9), with what each reads
    {R4D_OURS(CommuBoard_DrawFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},       // its pieces read the low words
    {R4D_OURS(CommuDraw_RandBelow), 1, {0xFF}, kG, 0, 0, {}, &RandBelowAnswer},                  // cmp bl, [esp + 8]
    {R4D_OURS(CommuDraw_DrawTitle), 0, {}, kG, 0, 0},
    {R4D_OURS(CommuName_CountSlots), 0, {}, kG, 0, 0, {}, &NearCursor},
    {R4D_OURS(CommuName_NthSlot), 1, {0xFF}, kG, 0, 0, {}, &NthSlotAnswer},   // mov bl, [esp + 8]
    {R4D_OURS(CommuName_DrawSlotBar), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},      // Menu_DrawBox's and the pieces' low words
    {R4D_OURS(CommuName_DrawSlotFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {R4D_OURS(CommuName_DrawHeader), 0, {}, kG, 0, 0, {}, &HeaderMove},
    // other groups' of this wave, by address (docs/rest_4d.md section 6), with what each reads
    {"0x45B2C0", at::kBoardCell.v, at::kBoardCell.v, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &ColumnMove},     // R4C: movsx words, mov al
    {"0x45B400", at::kBoardPiece.v, at::kBoardPiece.v, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // R4C: movsx words, and 0xFF
    {"0x45E870", at::kSlotPanel.v, at::kSlotPanel.v, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},   // R4E: words to Menu_DrawBox / movsx; and 0xFF; al
    {"0x45EC00", at::kListPiece.v, at::kListPiece.v, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},     // R4E: movsx words, and 0xFF
    {"0x45ECC0", at::kSlotHand.v, at::kSlotHand.v, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},       // R4E: movsx words, mov al
    {"0x45ED70", at::kRandomName.v, at::kRandomName.v, 0, {}, kG, 0, 0, {}, &RandomNameAnswer},
    {"0x45EE10", at::kMemberPanel.v, at::kMemberPanel.v, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &MemberPanelMove},   // R4E: as 0x45E870
    {"0x45EF90", at::kMemberPanels.v, at::kMemberPanels.v, 0, {}, kG, 0, 0},
    {"0x45F000", at::kMemberCount.v, at::kMemberCount.v, 0, {}, kG, 0, 0, {}, &NearCursor},
    {"0x45F020", at::kNthMember.v, at::kNthMember.v, 1, {0xFF}, kG, 0, 0, {}, &NthMemberAnswer},   // R4E: mov bl, [esp + 8]
    {"0x45F050", at::kMemberHand.v, at::kMemberHand.v, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},  // R4E: and 0xFFFF; al; and 0xFF
    {"0x45F1A0", at::kEntryBox.v, at::kEntryBox.v, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &EntryDoneMove},     // R4E: movsx bp / word; movzx byte
    {"0x45F5A0", at::kMemberRename.v, at::kMemberRename.v, 0, {}, kG, 0, 0},
    {"0x45F650", at::kSlotRename.v, at::kSlotRename.v, 0, {}, kG, 0, 0},
    // ours, outside the standard set or typed otherwise, with what each reads
    {R4D_OURS(Menu_DrawPanelBox), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // as R2C lists it
    {R4D_OURS(Menu_DrawGreyHLine), 4, {0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},           // rest_2b.cpp: shorts, a word, a byte
    {R4D_OURS(BareRet), 0, {}, kG, 0, 0, {}, &BareRetMove},                                        // a bare ret: reads none
    {R4D_OURS(BareRetZero), 0, {}, kG, 0, 0},                                               // xor al, al: reads none
    // the standard set's, louder here (the thin controls, section 7 of round-14-cleanup.md)
    {R4D_OURS(Music_FadeOutStop), 1, {kW}, kG, 0, 0, {}, &KeptTrackMove},
    {R4D_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &StepMove},
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 5, {kW, kW, 0, 0, 0}, kG, 0, 0, {0, 16}, &Sprintf, nullptr, true},
};
#undef R4D_OURS

// Beyond the harness's field regions (which hold the style byte and records
// 0x903A70..0x903A93, the save block's 0x904560..0x904700 with the first slot
// cells, the text scratch 0x904BA0..0x904BBF, MessagePools' offset words
// 0x803580..0x80397F, the buttons, the input words, Field_Request, the wait
// word, Draw_PassFlags, Cond_Flags with Music_Track, the packet cursor).
const sh::Region kRegions[] = {
    {0x675F80, 0x60},                  // the community's cells 0x675F8C..0x675FCC
    {0x939A30, 0x20},                  // the record pointer 0x939A38, the state bytes 0x939A3C..0x939A41
    {0x9039F0, 0x10},                  // the community's game 0x9039F4
    {0x903A94, 0x903F90 - 0x903A94},   // the character records past the style region
    {0x904700, 0x390},                 // the slot cells to 0x9048AF, the slot names 0x9048F0..0x904A1F
    {0x904BC0, 0x240},                 // the text scratch's rest and Text_Records 0x904CE0..0x904DFF
    {0x803980, 0x400},                 // MessagePools' text the seeded offsets point into
};

// --- the seed ----------------------------------------------------------------------------

void SeedButtons() {
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetW(Key(&Field_ConfirmButtons), confirm);
    SetW(Key(&Field_CancelButtons), cancel);
    const U keys = PickOf(0x8000, 0x2000, 0xA000, 0x1000, 0x4000, 0, sh::Next() & 0xF000);
    SetW(Input_Pressed_addr, PickOf(confirm, cancel, confirm | cancel, 0, 0, keys, keys, keys, sh::Next() & 0xFFFF));
    SetW(Key(&Input_Held), PickOf(0, 0, sh::Next()));
}

void SeedCommu() {
    B(Rest4b_BoardState_addr) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    B(Rest4b_BoardStep_addr) = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 6, 7, 8, sh::Next()));
    B(Rest4c_Step_addr) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    B(Rest4b_BoardFlag_addr) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(Rest4d_Again_addr) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(Rest4c_Count_addr) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 5, 6, 7, 0xFF, 0xFE, 0x80, sh::Next()));
    B(Rest4c_Cursor_addr) = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 0xFF, sh::Next()));
    B(Rest4d_Count_addr) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xC5, 0xC7, 0xC8, 0xFF, sh::Next()));
    SetW(Rest4d_Header_addr, PickOf(0xF0, 0xF1, 0xF1, 0xFFFF, 0xFFFF, sh::Next() % 0x200));
    B(Rest4d_EntryDone_addr) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(::bof3::Img(at::kName.v)) = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    B(Rest4d_TitleB_addr) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, sh::Next()));
    // the draw's picks: small values, so that a category's picks can all be equal
    for (unsigned i = 1; i < 0x28; ++i) B(::bof3::Img(at::kName.v) + i) = static_cast<unsigned char>(sh::Often() ? sh::Next() % 3 : sh::Next());
    for (unsigned c = 0; c < 4; ++c) B(::bof3::Img(at::kPicked.v) + c) = static_cast<unsigned char>(sh::Next() % 10);
    Field_Request = static_cast<unsigned char>(PickOf(2, 2, 0, 1, sh::Next()));
    Transition_Busy = static_cast<unsigned short>(PickOf(0, 0, sh::Next()));
    // the record pointer: one of the scratch's, its +5 a slot mostly
    unsigned char* const record = sh::Scratch(0);
    record[5] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 60 : sh::Next());
    sh::SetPointer(Rest4d_Pointer_addr, record);
}

// The 60 slot cells half in use; the seven records' list bit half set.
void SeedSlots() {
    const bool full = sh::Next() % 8 == 0;
    for (U cell = Community_Records_addr; cell < at::kSlotsEnd; cell += 8)
        B(cell) = static_cast<unsigned char>(full || sh::Half() ? sh::Next() | 1 : 0);
    for (unsigned r = 0; r < 7; ++r) {
        unsigned char& flags = B(CharacterRecords_addr + 0xb + at::kRecordStride * r);
        flags = static_cast<unsigned char>(sh::Half() ? flags | 1 : flags & ~1u);
    }
    for (unsigned i = 0; i < 5 * 60; ++i)
        if (sh::Next() % 6 == 0) B(::bof3::Img(at::kSlotNames.v) + i) = 0;
    for (unsigned r = 0; r < 7; ++r)
        if (sh::Half()) B(CharacterRecords_addr + at::kRecordStride * r + sh::Next() % 5) = 0;
}

// MessagePools: the draw's 256 offsets and the header's first 0x100 pointing
// into the seeded text 0x803980..0x803D7F, which has zeros in it and ends in two.
void SeedMessages() {
    for (U i = 0; i < 0x100; ++i) SetW(::bof3::Img(at::kDrawWords.v) + 2 * i, 0x400 + sh::Next() % 0x3F0);
    for (U i = 0; i < 0x100; ++i)
        if (sh::Often()) SetW(::bof3::Img(at::kPoolWords.v) + 2 * i, 0x400 + sh::Next() % 0x3F0);
    for (U a = 0x803980; a < 0x803D7E; ++a)
        if (sh::Next() % 7 == 0) B(a) = sh::Half() ? 0 : static_cast<unsigned char>(sh::Next() | 0x80);
    B(0x803D7E) = 0;
    B(0x803D7F) = 0;
}

void Seed(unsigned k) {
    g_clone = k;
    SeedButtons();
    SeedCommu();
    SeedSlots();
    SeedMessages();
    if (const Dispatch* d = DispatchOf(kAll[k].base)) B(d->by) = static_cast<unsigned char>(sh::Next() % d->count);
    // CommuName_SlotEntryOut reads the answer again only at the slide's end (+1 to 4): N47
    if (kAll[k].base == 0x45D7C0 && sh::Half()) B(Rest4d_Count_addr) = 3;
}

// The helpers' arguments: coordinates at boundaries or random words; bytes at
// their tests under random upper bytes.
void Args(unsigned k, U* a) {
    const U base = kAll[k].base;
    const auto coordinate = [](U r) { return PickOf(r, r & 0xFFFF, 0, 0x14, 0x4A, 0xFFFF, 0x10000, 0x8000, r % 0x140); };
    const auto byte = [](U r, U v) { return (r & 0xFFFFFF00u) | v; };
    a[0] = coordinate(a[0]);
    a[1] = coordinate(a[1]);
    switch (base) {
    case 0x45C400:   // DrawRows(x, y, shown)
    case 0x45C850:   // DrawCells(x, y, shown)
        a[2] = byte(a[2], PickOf(0, 0, 1, sh::Next() & 0xFF));
        break;
    case 0x45C7D0:   // DrawRowCells(x, y, row, lift)
        a[2] = byte(a[2], PickOf(0, 1, 3, 7, 0xFF, sh::Next() & 0xFF));
        a[3] = byte(a[3], PickOf(0, 0, 1, sh::Next() & 0xFF));
        break;
    case 0x45CE80: {   // RandBelow(limit): never a byte of 0 (the original's endless loop)
        const U limit = PickOf(1, 2, 0x16, 0x31, 0x40, 0x63, 0x7F, 0x80, 0xFF, sh::Next() & 0xFF);
        a[0] = byte(a[0], limit == 0 ? 1 : limit);
        break;
    }
    case 0x45E6D0:   // NthSlot(n)
        a[0] = byte(a[0], PickOf(0, 1, 2, 5, 29, 59, 60, 0xFF, sh::Next() & 0xFF));
        break;
    default: break;
    }
}

// DIV-0075's row, beside the fuzz (the form of DIV-0063's in battle_e6_fuzz.cpp).
// The self-test above compares Capcom's two entry steps with the switch off;
// this runs once Rest4D_Inject has set it, so ours moves the step on. There is
// no original that does: what the entry says ours must do is Capcom's step
// and then 0x939A3F one more - no other byte, no other call, the answer still
// BareRetZero's. So each of ours is wrapped to take that one step back and
// fuzzed against Capcom's as the group's own rows are (the same seed, moves,
// stand-ins and regions): a wrapper that matched with the switch on would
// mean ours does not move the step, or moves more.
void __cdecl SlotEntryAbandoned() {
    ::CommuName_SlotEntry();
    B(Rest4c_Step_addr) = static_cast<unsigned char>(B(Rest4c_Step_addr) - 1);
}
void __cdecl MemberEntryAbandoned() {
    ::CommuName_MemberEntry();
    B(Rest4c_Step_addr) = static_cast<unsigned char>(B(Rest4c_Step_addr) - 1);
}

// --- CommuNameGrid (the New Game round's CR) ----------------------------------------------
//
// Two APIs that are not the live unit: no grid (every call but `available`
// a Fatal - the entry must not reach them), and a grid of the Japan family
// that only a slot's steps see (call 6: they must not reach the rest).
namespace ng = name_grid;
namespace nd = name_grid::detail;

[[noreturn]] void NoCall(const char* what) { bof3::Fatal("rest_4d CommuNameGrid: the entry called %s with no grid for it", what); }
bool NoGridAvailable() { return false; }
bool YesAvailable() { return true; }
ng::Family NoFamily() { NoCall("family"); }
ng::Family JapanFamily() { return ng::Family::kJapan; }
unsigned NoCount() { NoCall("cell_count"); }
unsigned NoFirst() { NoCall("first_page"); }
void NoDraw(ng::Copy, const ng::Cells&, int, int) { NoCall("draw"); }
void NoLegend(ng::Copy, unsigned, int, int) { NoCall("legend"); }
unsigned NoInput(ng::Copy, ng::Cells&, unsigned, const unsigned char*) { NoCall("input"); }
unsigned NoEncode(const unsigned char*, unsigned char*, unsigned) { NoCall("encode"); }
bool NoDecode(const unsigned char*, unsigned, unsigned char*) { NoCall("decode"); }
const commu_name::GridApi kNoGrid{NoGridAvailable, NoFamily, NoCount, NoFirst, NoDraw, NoLegend, NoInput, NoEncode, NoDecode};
const commu_name::GridApi kJapanSlotOnly{YesAvailable, JapanFamily, NoCount, NoFirst, NoDraw, NoLegend, NoInput, NoEncode, NoDecode};

// The checks' count; a mismatch is logged (the first 24) and the test ends in a Fatal.
unsigned g_checks = 0, g_bad = 0;
void Check(bool ok, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void Check(bool ok, const char* fmt, ...) {
    ++g_checks;
    if (ok) return;
    if (++g_bad <= 24) {
        char line[240];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(line, sizeof line, fmt, ap);
        va_end(ap);
        bof3::Log("shadow      rest_4d CommuNameGrid MISMATCH: %s", line);
    }
}

// Two synthetic grids, written to the chunk's layout (docs/new-game-round.md
// 5a) and parsed by NG's reader; the codes and bytes are invented, never a
// disc's. The West's: one page numbered 2, codes 1..4 (next, back, quit,
// finish) in rows 3..6, row 7 empty, glyphs of one byte (0x30 + n) and,
// past the first 60, of two (0x81, n), the space (code 0xFF) the byte 0x20
// at row 6 column 0. The Japan family's: three pages from 0, codes 1..5
// (next, back, page, quit, finish), glyphs of two bytes (0x82, n), the space
// (0x81, 0x40) at page 0 row 2 column 5, holes in rows 5..7 of columns 0..9
// on page 2.
struct SynCell { unsigned char code, kind, b0, b1; };
std::vector<unsigned char> BuildGrid(bool japan) {
    const unsigned pages = japan ? 3 : 1, first = japan ? 0 : 2, codes = japan ? 5 : 4;
    std::vector<unsigned char> b{1, static_cast<unsigned char>(japan ? 0 : 1), static_cast<unsigned char>(pages),
                                 static_cast<unsigned char>(first), static_cast<unsigned char>(codes), 0, 0, 0};
    const char id[12] = {'c', 'r', '-', 's', 'y', 'n', 't', 'h'};
    b.insert(b.end(), id, id + 12);
    unsigned n = 0;
    for (unsigned p = 0; p < pages; ++p)
        for (unsigned r = 0; r < 8; ++r)
            for (unsigned c = 0; c < 12; ++c) {
                SynCell cell{0, 0, 0, 0};
                if (!japan && r == 7) cell = {0, 0, 0, 0};
                else if (r >= 3 && c >= 10) cell = {static_cast<unsigned char>(r - 2), 3, 0, 0};
                else if (japan && p == 2 && r >= 5 && c <= 9) cell = {0, 0, 0, 0};
                else if (japan && p == 0 && r == 2 && c == 5) cell = {0xFF, 2, 0x81, 0x40};
                else if (!japan && r == 6 && c == 0) cell = {0xFF, 1, 0x20, 0};
                else if (japan) { cell = {static_cast<unsigned char>(6 + n), 2, 0x82, static_cast<unsigned char>(n)}; ++n; }
                else if (n < 60) { cell = {static_cast<unsigned char>(6 + n), 1, static_cast<unsigned char>(0x30 + n), 0}; ++n; }
                else { cell = {static_cast<unsigned char>(6 + n), 2, 0x81, static_cast<unsigned char>(n)}; ++n; }
                b.insert(b.end(), {cell.code, cell.kind, cell.b0, cell.b1});
            }
    if (japan) b.insert(b.end(), {1, 0, 0, 4, 2, 1, 0, 4, 3, 4, 1, 4, 4, 2, 0, 2, 5, 3, 0, 2});
    else b.insert(b.end(), {1, 0, 0, 4, 2, 1, 0, 4, 4, 2, 0, 2, 5, 3, 0, 2});
    const unsigned slots = japan ? 7 : 4;
    for (unsigned k = 0; k < 2; ++k) {
        b.insert(b.end(), {static_cast<unsigned char>(slots), 4, 6, 0});
        for (unsigned s = 0; s < slots; ++s) b.insert(b.end(), {static_cast<unsigned char>('a' + s), 'b', 'c', 0, 0, 0});
        b.insert(b.end(), {static_cast<unsigned char>(k == 0 ? 0x78 : 0x4D), 0x2F, 8, 4, 1, 0, 0, 0});
        for (unsigned e = 0; e < 8; ++e) b.insert(b.end(), {static_cast<unsigned char>('A' + e), 'x', 0, 0});
        b.insert(b.end(), {1, 0x2A, 6, 0, 0, 0x10, 0, 0});
    }
    return b;
}

nd::Grid g_west, g_japan;
const nd::Grid* g_live = nullptr;    // the grid the synthetic API answers from
nd::Keys g_keys{};                   // the keys its input step sees
std::vector<unsigned short> g_sounds;
unsigned g_draws = 0, g_legends = 0;
int g_draw_x = 0, g_draw_y = 0;
ng::Cells g_drawn{};

// The test's pair table (DIV-0057's shape): the two-byte unit (0x9F, k) is the
// pair of the Japan glyphs n = 10 + 2k and 11 + 2k.
bool PairOf(std::uint32_t glyph, std::uint32_t* a, std::uint32_t* b) {
    if ((glyph >> 8) != 0x1F) return false;
    const unsigned k = glyph & 0xFF;
    *a = 0x0200 | (10 + 2 * k);
    *b = 0x0200 | (11 + 2 * k);
    return true;
}
void SynPlay(unsigned short id) { g_sounds.push_back(id); }
void PBox(int, int, int, int) {}
void PText(int, int, int, int, const unsigned char*) {}
void PHand(int, int) {}
void PMode(unsigned, unsigned) {}
void PTile(int, int, unsigned, unsigned, unsigned) {}
void PPiece(int, int, unsigned, unsigned, unsigned) {}
void PFrame(int, int, ng::Copy) {}
constexpr nd::Painter kQuiet{PBox, PText, PText, PHand, PMode, PTile, PPiece, PFrame};

bool SynAvailable() { return true; }
ng::Family SynFamily() { return static_cast<ng::Family>(g_live->family); }
unsigned SynCount() { return g_live->family == 0 ? 4u : 5u; }
unsigned SynFirst() { return g_live->first; }
void SynDraw(ng::Copy copy, const ng::Cells& c, int x, int y) {
    if (copy != ng::Copy::kRenamer) bof3::Fatal("rest_4d CommuNameGrid: the renamer drew New Game's copy");
    ++g_draws;
    g_drawn = c;
    g_draw_x = x;
    g_draw_y = y;
    nd::DrawGrid(*g_live, copy, c, x, y, kQuiet);
}
void SynLegend(ng::Copy copy, unsigned page, int x, int y) {
    if (copy != ng::Copy::kRenamer) bof3::Fatal("rest_4d CommuNameGrid: the renamer drew New Game's legend");
    ++g_legends;
    nd::DrawLegend(*g_live, copy, page, x, y, kQuiet, [](std::uint16_t bit) { return bit; });
}
unsigned SynInput(ng::Copy copy, ng::Cells& c, unsigned room, const unsigned char* fallback) {
    return nd::Step(*g_live, copy, c, g_keys, room, SynCount(), fallback, SynPlay);
}
unsigned SynEncode(const unsigned char* cells, unsigned char* out, unsigned room) { return nd::Encode(*g_live, cells, out, room); }
bool SynDecode(const unsigned char* stored, unsigned room, unsigned char* out) { return nd::Decode(*g_live, stored, room, out, PairOf); }
const commu_name::GridApi kSynthetic{SynAvailable, SynFamily, SynCount, SynFirst, SynDraw, SynLegend, SynInput, SynEncode, SynDecode};

// The code of the West glyph whose byte is `ch` (0x30 + n), of the Japan glyph n.
unsigned char WestCode(unsigned char ch) { return static_cast<unsigned char>(6 + (ch - 0x30)); }
unsigned char JapanCode(unsigned n) { return static_cast<unsigned char>(6 + n); }

// An entry of the test's own: the PC's cells as an array.
struct TestEntry {
    unsigned char buffer[commu_name::kBufferBytes + 7];
    unsigned char cursor, column, row, page, done, step;
    commu_name::EntryBytes Bytes() { return {buffer, &cursor, &column, &row, &page, &done, &step}; }
};

void Put5(unsigned char* to, std::initializer_list<unsigned char> cells) {
    unsigned i = 0;
    for (unsigned char c : cells) to[i++] = c;
}

bool CellsAre(const unsigned char* got, std::initializer_list<unsigned char> want) {
    unsigned i = 0;
    for (unsigned char w : want)
        if (got[i++] != w) return false;
    return true;
}

nd::Keys Press(bool confirm = false, bool finish = false) {
    nd::Keys k{};
    k.confirm = confirm;
    k.finish = finish;
    return k;
}

// The renamer's grid paths on the West grid: GridOn, the stored name to
// cells, the entry-in, the input's answer and step, the answered end, the
// drawn cells, the shown name, a round trip through the slot commit's bytes.
void WestChecks() {
    namespace cg = commu_name;
    g_live = &g_west;
    const unsigned char A = WestCode('0'), Bc = WestCode('1'), C = WestCode('2');
    unsigned char out[5];

    // GridOn: the switch, the grid, call 6.
    cg::g_grid = 0;
    Check(!cg::GridOn(true) && !cg::GridOn(false), "GridOn with the switch off");
    cg::g_grid = 1;
    Check(cg::GridOn(true) && cg::GridOn(false), "GridOn with a West grid");

    // The stored name: the PC's 0x20 padding, a blank inside, a NUL, all blank.
    const unsigned char padded[5] = {'0', '1', 0x20, 0x20, 0x20};
    cg::StoredToCells(padded, 5, out);
    Check(CellsAre(out, {A, Bc, 0xFF, 0xFF, 0xFF}), "padded slot name -> %02X %02X %02X", out[0], out[1], out[2]);
    const unsigned char inner[5] = {'0', 0x20, '1', 0x20, 0x20};
    cg::StoredToCells(inner, 5, out);
    Check(CellsAre(out, {A, 0xFF, Bc, 0xFF, 0xFF}), "a blank inside -> %02X %02X %02X", out[0], out[1], out[2]);
    const unsigned char nul[5] = {'0', 0, '1', '2', 0};
    cg::StoredToCells(nul, 5, out);
    Check(CellsAre(out, {A, 0xFF, 0xFF, 0xFF, 0xFF}), "a NUL ends the name");
    const unsigned char blanks[5] = {0x20, 0x20, 0x20, 0x20, 0x20};
    cg::StoredToCells(blanks, 5, out);
    Check(CellsAre(out, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), "an all-blank slot");
    const unsigned char zeros[5] = {0, 0, 0, 0, 0};
    cg::StoredToCells(zeros, 5, out);
    Check(CellsAre(out, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), "an empty slot");
    const unsigned char off[5] = {'0', 0x7E, 0, 0, 0};   // 0x7E is on no cell
    cg::StoredToCells(off, 5, out);
    Check(CellsAre(out, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), "a byte on no cell opens blank");
    const unsigned char six[8] = {'0', '1', '2', '3', '4', '5', 0, 0};   // six cells in a record's field
    cg::StoredToCells(six, 8, out);
    Check(CellsAre(out, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), "six cells open blank");
    const unsigned char wide[8] = {'0', '1', 0x81, 60, 0x81, 61, 0x81, 62};   // five cells, eight bytes
    cg::StoredToCells(wide, 8, out);
    Check(CellsAre(out, {A, Bc, static_cast<unsigned char>(6 + 60), static_cast<unsigned char>(6 + 61), static_cast<unsigned char>(6 + 62)}),
          "five cells of eight bytes");
    const unsigned char cut[5] = {'0', 0x81, 60, '1', 0x81};   // a lead byte at the field's end
    cg::StoredToCells(cut, 5, out);
    Check(CellsAre(out, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), "a lead byte at the end opens blank");

    // Entry-in: the cells, the name cursor, column and row 0, the page 2, the step on.
    TestEntry t{};
    std::memset(t.buffer, 0xEE, sizeof t.buffer);
    t.cursor = 3; t.column = 7; t.row = 4; t.page = 0; t.step = 0;
    cg::GridEntryIn(t.Bytes(), padded, cg::kSlotRoom);
    Check(CellsAre(t.buffer, {A, Bc, 0xFF, 0xFF, 0xFF}) && t.buffer[5] == 0xEE, "entry-in's buffer");
    Check(t.cursor == 0 && t.column == 0 && t.row == 0 && t.page == 2 && t.step == 1, "entry-in's cells %u %u %u %u step %u",
          t.cursor, t.column, t.row, t.page, t.step);

    // The input: nothing pressed - no answer, no step.
    g_keys = {};
    g_sounds.clear();
    t.cursor = 2;
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    Check(t.done == 0 && t.step == 1 && g_sounds.empty(), "an idle frame");
    // A glyph: row 0 column 2 ('2') at the name cursor 2.
    t.column = 2;
    g_keys = Press(true);
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    Check(CellsAre(t.buffer, {A, Bc, C, 0xFF, 0xFF}) && t.cursor == 3 && t.done == 0 && t.step == 1 &&
              g_sounds == std::vector<unsigned short>{0x103},
          "a glyph entered");
    // The byte room: a slot of five, four cells of one byte and a fifth of two.
    Put5(t.buffer, {A, A, A, A, 0xFF});
    t.cursor = 4; t.row = 6; t.column = 1;   // row 6 column 1: glyph 66, two bytes
    g_sounds.clear();
    g_keys = Press(true);
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    const unsigned char at66 = static_cast<unsigned char>(6 + 66);   // row 6 column 1, glyph 66: (0x81, 66)
    Check(t.buffer[4] == 0xFF && g_sounds == std::vector<unsigned short>{0x107}, "a two-byte cell refused in a slot's fifth");
    t.cursor = 3;
    t.buffer[3] = 0xFF;
    g_sounds.clear();
    cg::GridEntryInput(t.Bytes(), cg::kMemberRoom);
    Check(t.buffer[3] == at66 && g_sounds == std::vector<unsigned short>{0x103}, "a two-byte cell taken in a record's field");
    // Finish: the answer 1, the step on, the trailing blanks 0.
    Put5(t.buffer, {A, Bc, C, 0xFF, 0xFF});
    t.row = 0; t.column = 0; t.cursor = 3;
    g_sounds.clear();
    g_keys = Press(false, true);
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    Check(t.done == 1 && t.step == 2 && CellsAre(t.buffer, {A, Bc, C, 0, 0}) && g_sounds == std::vector<unsigned short>{0x104},
          "a finish: done %u step %u", t.done, t.step);
    // The answered end: the layer's bytes, NUL-filled to nine.
    const unsigned char fallback[20] = {'9', '8', 0x20, 0x20, 0x20};
    unsigned n = cg::GridEntryAnswered(t.Bytes(), cg::kSlotRoom, fallback, cg::kSlotRoom);
    Check(n == 3 && std::memcmp(t.buffer, "012\0\0\0\0\0\0", 9) == 0, "answered: %u bytes", n);
    // The slot commit's bytes (0x45F650: five, a NUL as a space), read back.
    unsigned char slot[5];
    for (unsigned k = 0; k < 5; ++k) slot[k] = t.buffer[k] == 0 ? 0x20 : t.buffer[k];
    cg::StoredToCells(slot, 5, out);
    Check(CellsAre(out, {A, Bc, C, 0xFF, 0xFF}), "the committed slot reads back");
    // An all-blank finish: five 0 cells; the answered end takes the default.
    std::memset(t.buffer, 0xFF, 5);
    t.step = 1;
    g_keys = Press(false, true);
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    Check(t.done == 1 && CellsAre(t.buffer, {0, 0, 0, 0, 0}), "an all-blank finish");
    n = cg::GridEntryAnswered(t.Bytes(), cg::kSlotRoom, fallback, cg::kSlotRoom);
    Check(n == 5 && std::memcmp(t.buffer, "98   \0\0\0\0", 9) == 0, "the slot's default, %u bytes", n);
    // A long default is cut at eight and given its NUL.
    std::memset(t.buffer, 0, 5);
    n = cg::GridEntryAnswered(t.Bytes(), cg::kMemberRoom, (const unsigned char*)"ABCDEFGHIJ", 12);
    Check(n == 8 && std::memcmp(t.buffer, "ABCDEFGH\0", 9) == 0, "a long default cut at eight");
    // Quit: the hand on code 3's row (5), confirm - no answer, the step on, no sound.
    Put5(t.buffer, {A, 0xFF, 0xFF, 0xFF, 0xFF});
    t.row = 5; t.column = 10; t.step = 1; t.done = 7;
    g_sounds.clear();
    g_keys = Press(true);
    cg::GridEntryInput(t.Bytes(), cg::kSlotRoom);
    Check(t.done == 0 && t.step == 2 && g_sounds.empty() && t.buffer[0] == A, "a quit: done %u step %u", t.done, t.step);

    // The drawn cells: the PC's while on the grid, the open's while off it.
    t.page = 0; t.row = 3; t.column = 4;
    ng::Cells d = cg::GridDrawnCells(t.Bytes());
    Check(d.page == 2 && d.row == 0 && d.column == 0, "page 0 drawn as the open's");
    t.page = 2; t.row = 9;
    d = cg::GridDrawnCells(t.Bytes());
    Check(d.page == 2 && d.row == 0 && d.column == 0, "row 9 drawn as the open's");
    t.row = 4; t.column = 11;
    d = cg::GridDrawnCells(t.Bytes());
    Check(d.page == 2 && d.row == 4 && d.column == 11, "a hand on the grid drawn where it is");

    // The shown name: a blank inside as the space's byte, the trailing cut, NULs.
    Put5(t.buffer, {A, 0xFF, Bc, 0xFF, 0xFF});
    unsigned char shown[cg::kBufferBytes];
    std::memset(shown, 0xEE, sizeof shown);
    cg::GridShownName(t.Bytes(), shown);
    Check(std::memcmp(shown, "0 1\0\0\0\0\0\0", 9) == 0, "the shown name");
}

// On the Japan family's grid: call 6, the four-cell cap (call 5), a pair code
// opened, the page code, a record's eight bytes.
void JapanChecks() {
    namespace cg = commu_name;
    g_live = &g_japan;
    unsigned char out[5];
    cg::g_grid = 1;
    Check(!cg::GridOn(true) && cg::GridOn(false), "call 6: a slot keeps DIV-0075's end, a member has the grid");

    // Four kana in eight bytes, as they are.
    const unsigned char four[8] = {0x82, 0, 0x82, 1, 0x82, 2, 0x82, 3};
    cg::StoredToCells(four, 8, out);
    Check(CellsAre(out, {JapanCode(0), JapanCode(1), JapanCode(2), JapanCode(3), 0xFF}), "four kana");
    // Five through a pair (DIV-0057): cut to four (call 5).
    const unsigned char five[8] = {0x82, 0, 0x82, 1, 0x82, 2, 0x9F, 1};
    cg::StoredToCells(five, 8, out);
    Check(CellsAre(out, {JapanCode(0), JapanCode(1), JapanCode(2), JapanCode(12), 0xFF}), "five kana cut to four: %02X %02X %02X %02X %02X",
          out[0], out[1], out[2], out[3], out[4]);
    // The PC's one-byte 0x20 as a blank between kana; the two-byte space as one too.
    const unsigned char spaced[8] = {0x82, 0, 0x20, 0x81, 0x40, 0x82, 1, 0};
    cg::StoredToCells(spaced, 8, out);
    Check(CellsAre(out, {JapanCode(0), 0xFF, 0xFF, JapanCode(1), 0xFF}), "blanks between kana");

    // Entry-in opens on page 0; a page code, then the finish and the answered bytes.
    TestEntry t{};
    cg::GridEntryIn(t.Bytes(), four, cg::kMemberRoom);
    Check(t.page == 0 && t.step == 1, "entry-in's page 0");
    t.row = 5; t.column = 10;   // code 3, the page
    g_keys = Press(true);
    g_sounds.clear();
    cg::GridEntryInput(t.Bytes(), cg::kMemberRoom);
    Check(t.page == 1 && t.done == 0 && t.step == 1 && g_sounds == std::vector<unsigned short>{0x103}, "the page code");
    t.cursor = 3;
    g_keys = Press(false, true);
    cg::GridEntryInput(t.Bytes(), cg::kMemberRoom);
    Check(t.done == 1 && t.step == 2, "finish");
    const unsigned n = cg::GridEntryAnswered(t.Bytes(), cg::kMemberRoom, (const unsigned char*)"x", 8);
    Check(n == 8 && std::memcmp(t.buffer, four, 8) == 0 && t.buffer[8] == 0, "four kana answered: %u bytes", n);
    // The drawn cells keep page 2 (the last in use), the legend and the grid draw it.
    t.page = 2; t.row = 4; t.column = 9;
    const ng::Cells d = cg::GridDrawnCells(t.Bytes());
    Check(d.page == 2 && d.row == 4 && d.column == 9, "page 2 drawn where it is");
    g_draws = g_legends = 0;
    cg::g_api->draw(ng::Copy::kRenamer, d, 0x18, 0x67);
    cg::g_api->legend(ng::Copy::kRenamer, d.page, 0xB0, 0x2B);
    Check(g_draws == 1 && g_legends == 1 && g_drawn.page == 2, "the draws on page 2");
}

// A random walk on both grids and both paths: from a stored name drawn at
// random, frames of random keys until a finish or a quit, every frame the
// name within the field's bytes, the name cursor within the family's cells,
// the hand on a cell that is not empty; a finished name, answered and read
// back as the commit leaves it, is the same cells.
std::uint32_t g_rng = 0x1DC2u;
unsigned g_finished = 0, g_quit = 0, g_read_back = 0;   // the walk's ends, for the log
std::uint32_t Rng() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
void Walk(const nd::Grid& grid, bool slot, unsigned rounds) {
    namespace cg = commu_name;
    g_live = &grid;
    const unsigned room = slot ? cg::kSlotRoom : cg::kMemberRoom;
    for (unsigned r = 0; r < rounds; ++r) {
        unsigned char stored[8] = {};
        for (unsigned i = 0; i < room; ++i) {
            const unsigned pick = Rng() % 6;
            stored[i] = pick == 0 ? 0x20 : pick == 1 ? 0 : grid.family == 0 ? static_cast<unsigned char>(0x82) : static_cast<unsigned char>(0x30 + Rng() % 40);
            if (grid.family == 0 && stored[i] == 0x82 && i + 1 < room) stored[++i] = static_cast<unsigned char>(Rng() % 60);
        }
        TestEntry t{};
        cg::GridEntryIn(t.Bytes(), stored, room);
        for (unsigned frame = 0; frame < 400 && t.step == 1; ++frame) {
            const std::uint32_t k = Rng();
            g_keys = {};
            switch (k % 12) {
            case 0: g_keys.up = true; break;
            case 1: g_keys.down = true; break;
            case 2: g_keys.left = true; break;
            case 3: g_keys.right = true; break;
            case 4: case 5: case 6: g_keys.confirm = true; break;
            case 7: g_keys.erase = true; break;
            case 8: g_keys.next = true; break;
            case 9: g_keys.back = true; break;
            case 10: g_keys.page = true; break;
            default: g_keys.finish = frame > 40 && (k >> 8) % 4 == 0; break;
            }
            g_sounds.clear();
            cg::GridEntryInput(t.Bytes(), room);
            const ng::Cells c{{t.buffer[0], t.buffer[1], t.buffer[2], t.buffer[3], t.buffer[4], 0}, t.cursor, t.column, t.row, t.page};
            const int bytes = nd::EncodedLength(grid, c.buffer);
            Check(bytes >= 0 && static_cast<unsigned>(bytes) <= room, "walk: %d bytes in a field of %u", bytes, room);
            Check(t.cursor < (grid.family == 0 ? 4u : 5u), "walk: name cursor %u", t.cursor);
            Check(grid.cell[t.page - grid.first][t.row * 12 + t.column].kind != 0, "walk: the hand on an empty cell");
        }
        if (t.step == 2) ++(t.done == 1 ? g_finished : g_quit);
        if (t.step != 2 || t.done != 1) continue;
        unsigned char cells[5];
        std::memcpy(cells, t.buffer, 5);
        if (cells[0] == 0) continue;   // all blank: the default's
        cg::GridEntryAnswered(t.Bytes(), room, (const unsigned char*)"x", room);
        unsigned char committed[8];
        for (unsigned k = 0; k < room; ++k) committed[k] = slot && t.buffer[k] == 0 ? 0x20 : t.buffer[k];
        unsigned char back[5];
        cg::StoredToCells(committed, room, back);
        for (unsigned k = 0; k < 5; ++k)
            if (cells[k] == 0) cells[k] = 0xFF;
        // a blank inside the name reads back as a blank; trailing ones were cut at the finish
        Check(std::memcmp(back, cells, 5) == 0, "walk: the committed name reads back (%s, round %u)", slot ? "slot" : "member", r);
        ++g_read_back;
    }
}

}  // namespace

void EntryAbandonTest() {
    struct Row { U base; const void* wrapper; };
    static const Row kRows[] = {
        {0x45D730, reinterpret_cast<const void*>(&SlotEntryAbandoned)},     // CommuName_SlotEntry
        {0x45E2C0, reinterpret_cast<const void*>(&MemberEntryAbandoned)},   // CommuName_MemberEntry
    };
    static sh::Clone chosen[2];
    static unsigned index[2];
    const char* const only = std::getenv("BOF3X_R4D_ONLY");
    unsigned n = 0;
    for (const Row& r : kRows)
        for (unsigned k = 0; k < kCount; ++k)
            if (kAll[k].base == r.base && (!only || !*only || std::strstr(kAll[k].name, only))) {
                index[n] = k;
                chosen[n] = kAll[k];
                chosen[n++].ours = r.wrapper;
            }
    if (n == 0) return;
    static unsigned* s_index = index;
    sh::Group g = {"rest_4d DIV-0075", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    // DIV-0075's end is the entry's with no grid: CommuNameGrid's byte may be
    // set by now, and a layer with a grid may be loaded.
    const commu_name::GridApi* const api = commu_name::g_api;
    commu_name::g_api = &kNoGrid;
    sh::Run(g);
    commu_name::g_api = api;
    bof3::Log("shadow      rest_4d DIV-0075: %u entry steps, ours with the switch on, the step one more than Capcom's and "
              "nothing else, 0 MISMATCHES", n);
}

void SelfTest() {
    // BOF3X_R4D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R4D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_4d: BOF3X_R4D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_4d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

// CommuNameGrid's rows (the New Game round's CR). First the harness, the
// form of DIV-0075's row above: with the switch on, an entry with no grid
// must be Capcom's entry with DIV-0075's one step more and nothing else -
// the six entry steps and the member's commit step, the two middle ones
// through the wrappers that take that step back; then the slot's three steps
// again with a grid of the Japan family loaded (call 6: they keep DIV-0075's
// end, and touch nothing of the grid but its family). Then the grid's own
// paths, which have no original to compare with, against the two synthetic
// grids above: each rule of the helpers the steps call (commu_name.h), and a
// random walk. The steps' remaining glue (the draws' coordinates, the panels
// and the commit's two NULs) is the game's, seen live, not here.
void GridTest() {
    const commu_name::GridApi* const api = commu_name::g_api;
    const unsigned char grid = commu_name::g_grid;
    commu_name::g_grid = 1;

    struct Row { U base; const void* wrapper; };
    static const Row kRows[] = {
        {0x45D610, nullptr},                                                  // CommuName_SlotEntryIn
        {0x45D730, reinterpret_cast<const void*>(&SlotEntryAbandoned)},       // CommuName_SlotEntry
        {0x45D7C0, nullptr},                                                  // CommuName_SlotEntryOut
        {0x45E190, nullptr},                                                  // CommuName_MemberEntryIn
        {0x45E2C0, reinterpret_cast<const void*>(&MemberEntryAbandoned)},     // CommuName_MemberEntry
        {0x45E350, nullptr},                                                  // CommuName_MemberEntryOut
        {0x45E520, nullptr},                                                  // CommuName_MemberEntryDone
    };
    constexpr unsigned kSlotRows = 3;   // the first three: a slot's steps
    static sh::Clone chosen[7];
    static unsigned index[7];
    static unsigned* s_index = index;
    const char* const only = std::getenv("BOF3X_R4D_ONLY");
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned n = 0;
        for (unsigned r = 0; r < (pass == 0 ? 7u : kSlotRows); ++r)
            for (unsigned k = 0; k < kCount; ++k)
                if (kAll[k].base == kRows[r].base && (!only || !*only || std::strstr(kAll[k].name, only))) {
                    index[n] = k;
                    chosen[n] = kAll[k];
                    if (kRows[r].wrapper) chosen[n].ours = kRows[r].wrapper;
                    ++n;
                }
        if (n == 0) continue;
        sh::Group g = {pass == 0 ? "rest_4d CommuNameGrid, no grid" : "rest_4d CommuNameGrid, a slot under Japan", chosen, n,
                       kCallees, sizeof kCallees / sizeof kCallees[0], kTables, sizeof kTables / sizeof kTables[0], kRegions,
                       sizeof kRegions / sizeof kRegions[0], [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
        g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
        g.field = true;
        commu_name::g_api = pass == 0 ? &kNoGrid : &kJapanSlotOnly;
        sh::Run(g);
        bof3::Log("shadow      rest_4d CommuNameGrid: %u entry steps with the switch on and %s, Capcom's with DIV-0075's step, "
                  "0 MISMATCHES", n, pass == 0 ? "no grid" : "a Japan grid on a slot");
    }

    // The grid's paths.
    std::string why;
    const std::vector<unsigned char> west = BuildGrid(false), japan = BuildGrid(true);
    if (!nd::ParseChunk(west.data(), static_cast<std::uint32_t>(west.size()), g_west, why))
        bof3::Fatal("rest_4d CommuNameGrid: the synthetic West grid does not parse: %s", why.c_str());
    if (!nd::ParseChunk(japan.data(), static_cast<std::uint32_t>(japan.size()), g_japan, why))
        bof3::Fatal("rest_4d CommuNameGrid: the synthetic Japan grid does not parse: %s", why.c_str());
    commu_name::g_api = &kSynthetic;
    g_checks = g_bad = g_finished = g_quit = g_read_back = 0;
    WestChecks();
    JapanChecks();
    commu_name::g_grid = 1;
    Walk(g_west, true, 600);
    Walk(g_west, false, 600);
    Walk(g_japan, false, 600);
    commu_name::g_api = api;
    commu_name::g_grid = grid;
    if (g_bad != 0) bof3::Fatal("rest_4d CommuNameGrid: %u of %u checks of the grid's paths MISMATCH (logged above)", g_bad, g_checks);
    bof3::Log("shadow      rest_4d CommuNameGrid: %u checks of the grid's paths on two synthetic grids (the walk: %u finished, %u "
              "read back after the commit's bytes, %u quit), 0 MISMATCHES", g_checks, g_finished, g_read_back, g_quit);
}

}  // namespace rest_4d
