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
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
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
    {0x45C8C0, at::kState, 3}, {0x45C8D0, at::kStep, 2},  {0x45C950, at::kStep, 5},  {0x45D040, at::kState, 5},
    {0x45D080, at::kStep, 9},  {0x45D4D0, at::kStep2, 3}, {0x45D600, at::kStep2, 5}, {0x45DAA0, at::kStep, 9},
    {0x45E050, at::kStep2, 3}, {0x45E180, at::kStep2, 5},
};
const Dispatch* DispatchOf(U base) {
    for (const Dispatch& d : kDispatch)
        if (d.base == base) return &d;
    return nullptr;
}

const sh::DataTable kTables[] = {
    {at::kDrawStates, 3},  {at::kDrawOpenSteps, 2},     {at::kDrawShowSteps, 5},  {at::kNameStates, 5},
    {at::kSlotSteps, 9},   {at::kSlotRandomSteps, 3},   {at::kSlotEntrySteps, 5}, {at::kMemberSteps, 9},
    {at::kMemberRandomSteps, 3}, {at::kMemberEntrySteps, 5},
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
    case 0: B(at::kStep) = static_cast<unsigned char>(v); break;
    case 1: B(at::kStep2) = static_cast<unsigned char>(v); break;
    case 2: B(at::kState) = static_cast<unsigned char>(v); break;
    case 3: B(at::kCursor) = static_cast<unsigned char>(pick ? v % 8 : v); break;
    case 4: B(at::kYes) = static_cast<unsigned char>(v & 0x10 ? v : v & 3); break;
    case 5: B(at::kCount) = static_cast<unsigned char>(v & 0x10 ? v : v % 6); break;
    case 6: SetW(at::kHeader, v % 4 == 0 ? 0xFFFFu : v % 4 == 1 ? 0xF1u : v % 4 == 2 ? 0xF0u : (h >> 16) & 0x1FF); break;
    case 7: B(at::kEntryDone) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 8: B(at::kPicked + (h >> 16) % 4) = static_cast<unsigned char>(v % 10); break;
    case 9: B(at::kName + ((h >> 16) & 1 ? 0 : (h >> 17) % 0x28)) = static_cast<unsigned char>(v % 4); break;
    case 10: SetW(at::kPressed, h >> 16); break;
    case 11: B(at::kEntryColumn) = static_cast<unsigned char>(v); break;
    case 12: B(at::kKeptTrack) = static_cast<unsigned char>(v); break;
    case 13: B(at::kSlotNames + (h >> 16) % 300) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 14: B(at::kRecords + at::kRecordStride * ((h >> 16) % 7) + (v & 1 ? 0xB : (h >> 20) % 5)) ^= static_cast<unsigned char>(v | 1); break;
    default: break;
    }
}
void Disturb(U h) { Move(h); }

U Stir(const U*, U answer) { return answer; }

// Crt_sprintf as the standard set's FxSprintf (up to seven letters and a NUL at
// the destination), but logging the numbers each of the group's formats takes:
// three for 0x653088, one for 0x5E10C0, none for the blanks - the calls push
// two, three or five words, so the standard row's third word is the caller's
// stack in two of them.
U Sprintf(const U* a, U) {
    if (a[1] == at::kBoardFmt3) sh::Note(a[2], a[3], a[4]);
    else if (a[1] == at::kNumberFmt) sh::Note(a[2]);
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
    sh::FillBytes(sh::Mem(at::kName), 0x20);
    return sh::Noise() % 0x25;
}
// R4E's 0x45F020: a record 0..6 (eax), or 0xFF (none) - never 0xFF while
// CommuName_MemberEntryOut runs, which indexes CommuName_RecordNames by it
// (ours aborts there past 7, section 5).
U NthMemberAnswer(const U*, U answer) {
    const U n = sh::Noise();
    if (kAll[g_clone].base == 0x45E350 || n % 5 != 0) return (n >> 8) % 7;
    return n % 3 == 0 ? 0xFF : answer;
}

#define R4D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9), with what each reads
    {R4D_OURS(CommuBoard_DrawFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},       // its pieces read the low words
    {R4D_OURS(CommuDraw_RandBelow), 1, {0xFF}, kG, 0, 0},                  // cmp bl, [esp + 8]
    {R4D_OURS(CommuDraw_DrawTitle), 0, {}, kG, 0, 0},
    {R4D_OURS(CommuName_CountSlots), 0, {}, kG, 0, 0},
    {R4D_OURS(CommuName_NthSlot), 1, {0xFF}, kG, 0, 0},                    // mov bl, [esp + 8]
    {R4D_OURS(CommuName_DrawSlotBar), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},      // Menu_DrawBox's and the pieces' low words
    {R4D_OURS(CommuName_DrawSlotFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {R4D_OURS(CommuName_DrawHeader), 0, {}, kG, 0, 0},
    // other groups' of this wave, by address (docs/rest_4d.md section 6), with what each reads
    {"0x45B2C0", at::kBoardCell, at::kBoardCell, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},     // R4C: movsx words, mov al
    {"0x45B400", at::kBoardPiece, at::kBoardPiece, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // R4C: movsx words, and 0xFF
    {"0x45E870", at::kSlotPanel, at::kSlotPanel, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},   // R4E: words to Menu_DrawBox / movsx; and 0xFF; al
    {"0x45EC00", at::kListPiece, at::kListPiece, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},     // R4E: movsx words, and 0xFF
    {"0x45ECC0", at::kSlotHand, at::kSlotHand, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},       // R4E: movsx words, mov al
    {"0x45ED70", at::kRandomName, at::kRandomName, 0, {}, kG, 0, 0, {}, &RandomNameAnswer},
    {"0x45EE10", at::kMemberPanel, at::kMemberPanel, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},   // R4E: as 0x45E870
    {"0x45EF90", at::kMemberPanels, at::kMemberPanels, 0, {}, kG, 0, 0},
    {"0x45F000", at::kMemberCount, at::kMemberCount, 0, {}, kG, 0, 0},
    {"0x45F020", at::kNthMember, at::kNthMember, 1, {0xFF}, kG, 0, 0, {}, &NthMemberAnswer},   // R4E: mov bl, [esp + 8]
    {"0x45F050", at::kMemberHand, at::kMemberHand, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},  // R4E: and 0xFFFF; al; and 0xFF
    {"0x45F1A0", at::kEntryBox, at::kEntryBox, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},     // R4E: movsx bp / word; movzx byte
    {"0x45F5A0", at::kMemberRename, at::kMemberRename, 0, {}, kG, 0, 0},
    {"0x45F650", at::kSlotRename, at::kSlotRename, 0, {}, kG, 0, 0},
    // ours, outside the standard set or typed otherwise, with what each reads
    {R4D_OURS(Menu_DrawPanelBox), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // as R2C lists it
    {R4D_OURS(Menu_DrawGreyHLine), 4, {0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},           // rest_2b.cpp: shorts, a word, a byte
    {R4D_OURS(BareRet), 0, {}, kG, 0, 0, {}, &Stir},                                        // a bare ret: reads none
    {R4D_OURS(BareRetZero), 0, {}, kG, 0, 0},                                               // xor al, al: reads none
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 5, {kW, kW, 0, 0, 0}, kG, 0, 0, {0, 16}, &Sprintf, nullptr, true},
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
    SetW(at::kPressed, PickOf(confirm, cancel, confirm | cancel, 0, 0, keys, keys, keys, sh::Next() & 0xFFFF));
    SetW(Key(&Input_Held), PickOf(0, 0, sh::Next()));
}

void SeedCommu() {
    B(at::kState) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    B(at::kStep) = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 6, 7, 8, sh::Next()));
    B(at::kStep2) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    B(at::kHow) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(at::kAgain) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(at::kCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 5, 6, 7, 0xFF, 0xFE, 0x80, sh::Next()));
    B(at::kYes) = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 0xFF, sh::Next()));
    B(at::kCount) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xC5, 0xC7, 0xC8, 0xFF, sh::Next()));
    SetW(at::kHeader, PickOf(0xF0, 0xF1, 0xF1, 0xFFFF, 0xFFFF, sh::Next() % 0x200));
    B(at::kEntryDone) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(at::kName) = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    B(at::kTitleB) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, sh::Next()));
    // the draw's picks: small values, so that a category's picks can all be equal
    for (unsigned i = 1; i < 0x28; ++i) B(at::kName + i) = static_cast<unsigned char>(sh::Often() ? sh::Next() % 3 : sh::Next());
    for (unsigned c = 0; c < 4; ++c) B(at::kPicked + c) = static_cast<unsigned char>(sh::Next() % 10);
    Field_Request = static_cast<unsigned char>(PickOf(2, 2, 0, 1, sh::Next()));
    MoveScript_WaitWordDA = static_cast<unsigned short>(PickOf(0, 0, sh::Next()));
    // the record pointer: one of the scratch's, its +5 a slot mostly
    unsigned char* const record = sh::Scratch(0);
    record[5] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 60 : sh::Next());
    sh::SetPointer(at::kPointer, record);
}

// The 60 slot cells half in use; the seven records' list bit half set.
void SeedSlots() {
    const bool full = sh::Next() % 8 == 0;
    for (U cell = at::kSlots; cell < at::kSlotsEnd; cell += 8)
        B(cell) = static_cast<unsigned char>(full || sh::Half() ? sh::Next() | 1 : 0);
    for (unsigned r = 0; r < 7; ++r) {
        unsigned char& flags = B(at::kRecordFlags + at::kRecordStride * r);
        flags = static_cast<unsigned char>(sh::Half() ? flags | 1 : flags & ~1u);
    }
    for (unsigned i = 0; i < 5 * 60; ++i)
        if (sh::Next() % 6 == 0) B(at::kSlotNames + i) = 0;
    for (unsigned r = 0; r < 7; ++r)
        if (sh::Half()) B(at::kRecords + at::kRecordStride * r + sh::Next() % 5) = 0;
}

// MessagePools: the draw's 256 offsets and the header's first 0x100 pointing
// into the seeded text 0x803980..0x803D7F, which has zeros in it and ends in two.
void SeedMessages() {
    for (U i = 0; i < 0x100; ++i) SetW(at::kDrawWords + 2 * i, 0x400 + sh::Next() % 0x3F0);
    for (U i = 0; i < 0x100; ++i)
        if (sh::Often()) SetW(at::kPoolWords + 2 * i, 0x400 + sh::Next() % 0x3F0);
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

}  // namespace

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

}  // namespace rest_4d
