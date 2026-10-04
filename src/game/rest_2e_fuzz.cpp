// BOF3X_SHADOW=rest_2e: group R2E's 49 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 7), once at start-up. docs/rest_2e.md section 4.
// BOF3X_R2E_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R2E --clones --harness scenario
// (2026-10-04) with the names given; every extent and call site agrees with the
// capstone read. The menu states and the dispatchers are kMenu (the state and
// step bytes drawn below menu_span, each dispatcher's own byte seeded below its
// table); the window helpers, the sorts and the preview helpers kCall, the
// answering one with ret_mask 0xFF.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2e.h"
#include "game/rest_2e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2e {
namespace {

namespace sh = scenario_harness;
using sh::Shape;
using U = std::uint32_t;
using UC = unsigned char;

// band_rows.py's call sites (2026-10-04), each checked against the capstone read.
constexpr sh::CallSite kCalls58B1D0[] = {{0x6, 0x575690}, {0x49, 0x461EB0}, {0x62, 0x587740}, {0x9C, 0x587740}, {0xE8, 0x587740}, {0x14A, 0x587740}};
constexpr sh::CallSite kCalls58B330[] = {{0x6, 0x575690}, {0x61, 0x461EB0}, {0x73, 0x587740}, {0xAE, 0x587740}, {0xFB, 0x587740}, {0x121, 0x58BD40}, {0x15F, 0x587740}};
constexpr sh::CallSite kCalls58B4B0[] = {{0x7, 0x575690}, {0x2D, 0x591C20}, {0x8A, 0x461EB0}, {0x18E, 0x587740}, {0x1C5, 0x587740}, {0x1FD, 0x587740}, {0x206, 0x587740}, {0x234, 0x58BD50}, {0x262, 0x58BD50}, {0x280, 0x587740}};
constexpr sh::CallSite kCalls58B750[] = {{0x6, 0x575690}, {0x2C, 0x591680}, {0x36, 0x591940}, {0x92, 0x587740}, {0xC2, 0x587740}, {0x128, 0x587740}};
constexpr sh::CallSite kCalls58B8A0[] = {{0xA, 0x575690}, {0x4B, 0x591940}, {0x84, 0x531BB0}, {0x9B, 0x461EB0}, {0xEF, 0x587740}, {0x123, 0x497680}, {0x138, 0x587740}, {0x14A, 0x587740}, {0x16D, 0x587740}};
constexpr sh::CallSite kCalls58BA40[] = {{0x7, 0x575690}, {0x2D, 0x591C20}, {0x83, 0x461EB0}, {0x187, 0x587740}, {0x1AD, 0x587740}};
constexpr sh::CallSite kCalls58BD70[] = {{0x46, 0x58BD50}, {0x4D, 0x58BD50}};
constexpr sh::CallSite kCalls58BE00[] = {{0x7, 0x58BD70}, {0x5F, 0x58BD50}, {0x72, 0x58BD50}};
constexpr sh::CallSite kCalls58BEB0[] = {{0x7, 0x58BD70}, {0x5F, 0x58BD50}, {0x72, 0x58BD50}};
constexpr sh::CallSite kCalls58BF60[] = {{0x7, 0x58BD70}, {0x5F, 0x58BD50}, {0x72, 0x58BD50}};
constexpr sh::CallSite kCalls58C010[] = {{0x7, 0x58BD70}, {0x59, 0x58BD50}, {0x6C, 0x58BD50}};
constexpr sh::CallSite kCalls58C0B0[] = {{0x7, 0x58BD70}, {0x6B, 0x591720}, {0x7A, 0x591720}, {0x8C, 0x58BD50}, {0x97, 0x58BD50}};
constexpr sh::CallSite kCalls58C1A0[] = {{0x7, 0x58BD70}, {0x7E, 0x5917A0}, {0x96, 0x5917A0}, {0xA8, 0x58BD50}, {0xAF, 0x58BD50}};
constexpr sh::CallSite kCalls58C2D0[] = {{0x6, 0x575690}, {0xB, 0x58CFC0}, {0x15, 0x587740}};
constexpr sh::CallSite kCalls58C310[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls58C340[] = {{0x7, 0x575690}, {0x70, 0x461EB0}, {0xBA, 0x587740}, {0xD6, 0x587740}, {0x10B, 0x587740}, {0x115, 0x587740}};
constexpr sh::CallSite kCalls58C4A0[] = {{0x9, 0x575690}, {0x49, 0x461EB0}, {0x58, 0x531BB0}, {0xD3, 0x587740}, {0xEA, 0x58D0C0}, {0x102, 0x58D2E0}, {0x136, 0x591940}, {0x179, 0x587740}, {0x193, 0x587740}, {0x1A0, 0x531BB0}, {0x1C8, 0x531BB0}, {0x2AE, 0x58D570}, {0x2C5, 0x587740}};
constexpr sh::CallSite kCalls58CD40[] = {{0x6, 0x575690}, {0x1F, 0x58D7B0}, {0x27, 0x531BB0}, {0x42, 0x531BB0}};
constexpr sh::CallSite kCalls58CDD0[] = {{0xA, 0x575690}, {0x91, 0x591C20}, {0xDF, 0x461EB0}, {0xF1, 0x587740}, {0x11C, 0x587740}, {0x13B, 0x58D700}, {0x159, 0x587740}, {0x166, 0x590BB0}, {0x16F, 0x590660}, {0x177, 0x58D570}, {0x183, 0x587740}, {0x1A3, 0x587740}};
constexpr sh::JumpTable kTables58CDD0[] = {{0x4B, 0x1D8, 5}};
constexpr sh::CallSite kCalls58D0C0[] = {{0xCA, 0x5917A0}, {0x1A1, 0x5917A0}};
constexpr sh::CallSite kCalls58D2E0[] = {{0xDD, 0x5917A0}, {0x1E6, 0x5917A0}};
constexpr sh::CallSite kCalls58D570[] = {{0x83, 0x591B60}, {0x96, 0x590BB0}, {0xB6, 0x590660}};
constexpr sh::CallSite kCalls58D640[] = {{0x41, 0x5917A0}};
constexpr sh::CallSite kCalls58D7D0[] = {{0x6, 0x575690}, {0xB, 0x58ED40}, {0x15, 0x587740}};
constexpr sh::CallSite kCalls58D820[] = {{0x7, 0x575690}, {0x71, 0x461EB0}, {0xBB, 0x587740}, {0xE3, 0x587740}, {0x158, 0x587740}, {0x162, 0x587740}};
constexpr sh::CallSite kCalls58D9C0[] = {{0x9, 0x575690}, {0xA5, 0x591940}, {0xC1, 0x461EB0}, {0xD8, 0x531BB0}, {0xFC, 0x587740}, {0x129, 0x587740}, {0x176, 0x587740}, {0x1A9, 0x587740}, {0x1F1, 0x587740}, {0x219, 0x587740}, {0x25B, 0x587740}};
constexpr sh::CallSite kCalls58DC40[] = {{0x7, 0x575690}, {0x69, 0x591E50}, {0xF7, 0x461EB0}, {0x11B, 0x587740}, {0x14B, 0x587740}, {0x1C7, 0x587740}, {0x208, 0x591E50}, {0x24A, 0x57DA70}, {0x27A, 0x591E50}, {0x2AA, 0x58A3C0}, {0x2C4, 0x587740}, {0x2D5, 0x587740}, {0x2E6, 0x587740}, {0x2F3, 0x531BB0}, {0x36F, 0x531BB0}, {0x3C3, 0x57DA70}, {0x404, 0x587740}};
constexpr sh::CallSite kCalls58E070[] = {{0x8, 0x575690}, {0x70, 0x461EB0}, {0x79, 0x531BB0}, {0xD8, 0x587740}, {0xF0, 0x591E50}, {0x117, 0x591940}, {0x14D, 0x591E50}, {0x191, 0x58A3C0}, {0x1AB, 0x587740}, {0x1BB, 0x587740}, {0x1D4, 0x587740}, {0x1DE, 0x587740}, {0x1F2, 0x531BB0}, {0x223, 0x531BB0}};
constexpr sh::CallSite kCalls58E2C0[] = {{0x6, 0x575690}, {0x60, 0x587740}, {0x94, 0x587740}, {0xB2, 0x591E50}, {0xCA, 0x590C90}, {0xDE, 0x591E50}, {0xFD, 0x587740}, {0x13D, 0x587740}};
constexpr sh::CallSite kCalls58E420[] = {{0x9, 0x575690}, {0x86, 0x461EB0}, {0x96, 0x531BB0}, {0xB1, 0x587740}, {0xDE, 0x587740}, {0x11B, 0x587740}, {0x14D, 0x587740}, {0x191, 0x587740}, {0x1FF, 0x587740}};
constexpr sh::CallSite kCalls58E640[] = {{0x8, 0x575690}, {0x63, 0x461EB0}, {0x89, 0x587740}, {0xC3, 0x587740}, {0x110, 0x587740}, {0x135, 0x58EE40}, {0x17F, 0x587740}};
constexpr sh::CallSite kCalls58E7E0[] = {{0x6, 0x575690}, {0x57, 0x591E50}, {0x8F, 0x461EB0}, {0xDE, 0x587740}, {0x117, 0x591E50}, {0x132, 0x587740}, {0x14D, 0x587740}, {0x15C, 0x587740}, {0x170, 0x591E50}, {0x192, 0x591E50}, {0x1A5, 0x58BD50}, {0x1C3, 0x587740}};
constexpr sh::CallSite kCalls58E9D0[] = {{0x6, 0x575690}, {0x1D, 0x58F050}, {0x25, 0x531BB0}, {0x40, 0x531BB0}};
constexpr sh::CallSite kCalls58EA70[] = {{0x6, 0x575690}, {0x1C, 0x58F000}, {0x3E, 0x587740}};
constexpr sh::CallSite kCalls58EAD0[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls58EB00[] = {{0x6, 0x575690}, {0x82, 0x461EB0}, {0x173, 0x587740}, {0x190, 0x587740}};
constexpr sh::CallSite kCalls58ECC0[] = {{0x6, 0x575690}, {0x28, 0x587740}};
constexpr sh::CallSite kCalls58ED10[] = {{0x6, 0x575690}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R2E_FN(name) reinterpret_cast<const void*>(&::name)
#define R2E_ROW(name, base, size, calls, shape) {#name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, R2E_FN(name), 0, false, shape}
#define R2E_BARE(name, base, size, shape) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R2E_FN(name), 0, false, shape}
constexpr Shape kM = Shape::kMenu, kC = Shape::kCall;
const sh::Clone kClones[] = {
    R2E_ROW(FieldItems_ArrangeCategory, 0x58B1D0, 0x15F, kCalls58B1D0, kM),
    R2E_ROW(FieldItems_ArrangeHow, 0x58B330, 0x17B, kCalls58B330, kM),
    R2E_ROW(FieldItems_ArrangeMove, 0x58B4B0, 0x298, kCalls58B4B0, kM),
    R2E_ROW(FieldItems_DiscardConfirm, 0x58B750, 0x144, kCalls58B750, kM),
    R2E_ROW(FieldItems_UseOnMember, 0x58B8A0, 0x194, kCalls58B8A0, kM),
    R2E_ROW(FieldItems_ViewList32, 0x58BA40, 0x1E3, kCalls58BA40, kM),
    R2E_BARE(FieldItems_InitWindows, 0x58BC30, 0x108, kC),
    R2E_BARE(FieldItems_Sort, 0x58BD40, 0x10, kC),
    {"FieldMenu_SwapBytes", 0x58BD50, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R2E_FN(FieldMenu_SwapBytes), 0, false, kC,
     sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(1, sh::Arg::kScratch)},
    R2E_ROW(FieldItemSort_Compact, 0x58BD70, 0x8C, kCalls58BD70, kC),
    R2E_ROW(FieldItemSort_ConsumableFlag1, 0x58BE00, 0xA6, kCalls58BE00, kC),
    R2E_ROW(FieldItemSort_ConsumableFlag2, 0x58BEB0, 0xA6, kCalls58BEB0, kC),
    R2E_ROW(FieldItemSort_WeaponsByPower, 0x58BF60, 0xA7, kCalls58BF60, kC),
    R2E_ROW(FieldItemSort_ArmourByPower, 0x58C010, 0x9C, kCalls58C010, kC),
    R2E_ROW(FieldItemSort_ByIconKind, 0x58C0B0, 0xEB, kCalls58C0B0, kC),
    R2E_ROW(FieldItemSort_EquipableFirst, 0x58C1A0, 0xF7, kCalls58C1A0, kC),
    R2E_BARE(FieldItems_CloseWindows, 0x58C2A0, 0x12, kC),
    R2E_BARE(FieldEquip_Run, 0x58C2C0, 0xE, kM),
    R2E_ROW(FieldEquip_Open, 0x58C2D0, 0x38, kCalls58C2D0, kM),
    R2E_ROW(FieldMenu_CountdownState, 0x58C310, 0x23, kCalls58C310, kM),
    R2E_ROW(FieldEquip_TopMenu, 0x58C340, 0x152, kCalls58C340, kM),
    R2E_ROW(FieldEquip_PickMember, 0x58C4A0, 0x2F7, kCalls58C4A0, kM),
    R2E_ROW(FieldEquip_Close, 0x58CD40, 0x8D, kCalls58CD40, kM),
    {"FieldEquip_RemoveSlot", 0x58CDD0, 0x1EC, kCalls58CDD0, SH_N(kCalls58CDD0), nullptr, 0, kTables58CDD0, SH_N(kTables58CDD0),
     R2E_FN(FieldEquip_RemoveSlot), 0, false, kM},
    R2E_BARE(FieldEquip_InitWindows, 0x58CFC0, 0xF4, kC),
    R2E_ROW(FieldEquip_BestByPower, 0x58D0C0, 0x220, kCalls58D0C0, kC),
    R2E_ROW(FieldEquip_BestByOrder, 0x58D2E0, 0x290, kCalls58D2E0, kC),
    R2E_ROW(FieldEquip_ApplyPreview, 0x58D570, 0xC6, kCalls58D570, kC),
    R2E_ROW(FieldEquip_PreviewItem, 0x58D640, 0xBD, kCalls58D640, kC),
    {"FieldEquip_PreviewRemove", 0x58D700, 0xA2, nullptr, 0, nullptr, 0, nullptr, 0, R2E_FN(FieldEquip_PreviewRemove), 0xFF, false, kC},
    R2E_BARE(FieldEquip_CloseWindows, 0x58D7B0, 0xD, kC),
    R2E_BARE(FieldAbility_Run, 0x58D7C0, 0xE, kM),
    R2E_ROW(FieldAbility_Open, 0x58D7D0, 0x44, kCalls58D7D0, kM),
    R2E_ROW(FieldAbility_TopMenu, 0x58D820, 0x198, kCalls58D820, kM),
    R2E_ROW(FieldAbility_PickMember, 0x58D9C0, 0x27A, kCalls58D9C0, kM),
    R2E_ROW(FieldAbility_PickAbility, 0x58DC40, 0x42A, kCalls58DC40, kM),
    R2E_ROW(FieldAbility_PickTarget, 0x58E070, 0x24C, kCalls58E070, kM),
    R2E_ROW(FieldAbility_ShareConfirm, 0x58E2C0, 0x144, kCalls58E2C0, kM),
    R2E_BARE(FieldAbility_ArrangeRun, 0x58E410, 0xE, kM),
    R2E_ROW(FieldAbility_ArrangeMember, 0x58E420, 0x21E, kCalls58E420, kM),
    R2E_ROW(FieldAbility_ArrangeHow, 0x58E640, 0x19D, kCalls58E640, kM),
    R2E_ROW(FieldAbility_ArrangeMove, 0x58E7E0, 0x1EC, kCalls58E7E0, kM),
    R2E_ROW(FieldAbility_Close, 0x58E9D0, 0x8B, kCalls58E9D0, kM),
    R2E_BARE(FieldAbility_ViewRun, 0x58EA60, 0xE, kM),
    R2E_ROW(FieldAbility_ViewOpen, 0x58EA70, 0x5A, kCalls58EA70, kM),
    R2E_ROW(FieldAbility_ViewWait, 0x58EAD0, 0x23, kCalls58EAD0, kM),
    R2E_ROW(FieldAbility_ViewBrowse, 0x58EB00, 0x1BA, kCalls58EB00, kM),
    R2E_ROW(FieldAbility_ViewLeave, 0x58ECC0, 0x44, kCalls58ECC0, kM),
    R2E_ROW(FieldAbility_ViewEnd, 0x58ED10, 0x30, kCalls58ED10, kM),
};
#undef R2E_ROW
#undef R2E_BARE
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];
static_assert(kCount == 49, "the group's 49 functions");

U AddrOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// The .data tables the group's dispatchers read in place (swapped for recorders
// while the fuzz runs). FieldItems_ArrangeSteps is R2D's 0x58B1C0's to read.
const sh::DataTable kTables[] = {
    {AddrOf(FieldItems_Sorts), 7}, {AddrOf(FieldEquip_States), 9}, {AddrOf(FieldAbility_States), 10},
    {AddrOf(FieldAbility_ArrangeSteps), 3}, {AddrOf(FieldAbility_ViewSteps), 5},
};

UC& B(U a) { return *sh::Mem(a); }
void SetW(U a, U v) { move_script::SetWord(sh::Mem(a), v); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the stand-ins ----------------------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// FieldMenu_SwapBytes exchanges what its two pointers name: its callers bubble
// on, reading the lists again (a quieter stand-in would hide the order).
std::uint32_t SwapFx(const std::uint32_t* a, std::uint32_t answer) {
    auto* const x = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    auto* const y = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    if (sh::InRegions(x, 1) && sh::InRegions(y, 1)) {
        const UC t = *x;
        *x = *y;
        *y = t;
    }
    return answer;
}
// FieldItemSort_Compact moves the category's entries: its six callers read the
// lists after it - one id and its count moved, from the log's noise.
std::uint32_t CompactFx(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    const U at = 0x904154 + ((n >> 8) % 0x200);
    if ((n & 3) == 0) B(at) = 0;
    else if ((n & 3) == 1) B(at) = static_cast<UC>(n >> 24);
    return answer;
}

constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kAll32 = 0xFFFFFFFFu;
#define R2E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)

const sh::Callee kCallees[] = {
    // the group's own, called directly (E8): their arguments logged by what they read
    {R2E_OURS(FieldMenu_SwapBytes), 2, {kAll32, kAll32}, kG, 0, 0, {}, &SwapFx},
    {R2E_OURS(FieldItemSort_Compact), 0, {}, kG, 0, 0, {}, &CompactFx},
    {R2E_OURS(FieldItems_Sort), 1, {0xFF}, kG, 0, 0},
    {R2E_OURS(FieldEquip_InitWindows), 0, {}, kG, 0, 0},
    {R2E_OURS(FieldEquip_CloseWindows), 0, {}, kG, 0, 0},
    {R2E_OURS(FieldEquip_BestByPower), 1, {0xFF}, kG, 0, 0},
    {R2E_OURS(FieldEquip_BestByOrder), 1, {0xFF}, kG, 0, 0},
    {R2E_OURS(FieldEquip_ApplyPreview), 0, {}, kG, 0, 0},
    {R2E_OURS(FieldEquip_PreviewRemove), 0, {}, sh::Answer::kFlag, 0, 0},
    // other groups' of this wave, by address (rest_2e_callees.h)
    {"0x58A3C0 (R2D)", at::kAbilityUse, at::kAbilityUse, 4, {0xFF, 0xFF, 0xFF, kAll32}, sh::Answer::kByte, 0, 6},
    {"0x58ED40 (R2F)", at::kAbilityWindows, at::kAbilityWindows, 0, {}, kG, 0, 0},
    {"0x58EE40 (R2F)", at::kAbilitySort, at::kAbilitySort, 1, {0xFF}, kG, 0, 0},
    {"0x58F000 (R2F)", at::kAbilityViewWindow, at::kAbilityViewWindow, 0, {}, kG, 0, 0},
    {"0x58F050 (R2F)", at::kAbilityCloseWindows, at::kAbilityCloseWindows, 0, {}, kG, 0, 0},
    // ours the standard set lacks, or lists with masks wider than what the
    // callee reads where Capcom pushes a register over a leftover (docs/rest_2e.md
    // section 3)
    {R2E_OURS(Item_EquipMask), 2, {0xFF, 0xFF}, kG, 0, 0},
    {R2E_OURS(ItemUse_Dispatch), 3, {0xFF, 0xFF, kAll32}, sh::Answer::kByte, 0, 5},
    {R2E_OURS(Item_NamePtr), 2, {0xFF, 0xFF}, kG, 0, 0, {}, nullptr, nullptr, true},
    {R2E_OURS(Inventory_Remove), 3, {0xFF, 0xFF, 0xFF}, sh::Answer::kFlag, 0, 0},
};
#undef R2E_OURS

// Beyond the harness's field regions (the menu block, the style cells and the
// first records 0x903A70..0x903A93, Cond_Flags with the party list, the save
// block's 0x904098..0x904160 and 0x904560.., the camera cells with 0x905BA1).
const sh::Region kRegions[] = {
    {sh::at::kWindows, sh::at::kWindowCount * sh::at::kWindowStride},   // WindowRecords 0x803160..0x803477
    {0x903584, 0x10},                                                  // Field_ConfirmButtons / Field_CancelButtons
    {0x903A94, 0x903F90 - 0x903A94},                                   // CharacterRecords past the style region
    {0x904160, 0x400},                                                 // the inventory's id and count lists to 0x904560
    {0x939880, 0x50},                                                  // the screens' per-member and per-category bytes
    {0x6BDFA8, 0x18},                                                  // the preview bytes, the two top-menu cursors
    {0x937F8C, 4},                                                     // 0x937F8E, the member Equipment leaves
};

// --- the seed ------------------------------------------------------------------------------

bool Is(unsigned k, const char* name) { return std::strcmp(kClones[k].name, name) == 0; }
bool Has(unsigned k, const char* part) { return std::strstr(kClones[k].name, part) != nullptr; }

// A member id whose record index (MoveScript_EffectState) is one of the eight.
UC SafeId() {
    for (int tries = 0; tries < 16; ++tries) {
        const auto id = static_cast<UC>(sh::Next() % 24);
        if (MoveScript_EffectState[id] < 8) return id;
    }
    return 0;
}

void SeedButtons() {
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetW(0x90358E, confirm | (sh::Half() ? 0 : 1u << (sh::Next() % 16)));
    SetW(0x903590, cancel);
    const U other = sh::Next() & 0xFFFF & ~(confirm | cancel);
    U pressed = PickOf(confirm, cancel, confirm | cancel, 0, other, confirm | other, cancel | other);
    if (sh::Half()) pressed |= PickOf(0x2000, 0x8000, 0xA000, 0x1000, 0x4000, 0x5000, 0);
    SetW(0x7E1BEC, pressed);
    SetW(0x7E1BEE, sh::Next());
}

// The inventory: lists mostly empty, a run of entries at the front, ids inside
// the name tables, counts small.
void SeedInventory() {
    for (U i = 0; i < 0x200; ++i) {
        const U n = sh::Next();
        UC id = 0;
        if (n % 5 == 0) id = static_cast<UC>(1 + (n >> 8) % 0x5B);
        B(0x904154 + i) = id;
    }
    const U run = sh::Next() % 12;
    const U list = sh::Next() % 4;
    for (U i = 0; i < run; ++i) B(0x904154 + 0x80 * list + i) = static_cast<UC>(1 + sh::Next() % 0x5B);
    for (U i = 0; i < 0x200; ++i) B(0x904354 + i) = static_cast<UC>(PickOf(1, 1, 2, 9, 99, sh::Next()));
    for (U i = 0; i < 0x20; ++i) B(0x904554 + i) = static_cast<UC>(sh::Half() ? 0 : sh::Next() % 0x40);
}

void SeedParty() {
    for (U i = 0; i < 6; ++i) B(0x904062 + i) = SafeId();
}

void SeedMenu() {
    B(0x929F04) = static_cast<UC>(PickOf(1, 1, 2, 3, 4, 5, 0, sh::Next()));
    B(0x929F06) = static_cast<UC>(PickOf(0, 1, 2, 0xFF, 3, sh::Next() % 3));
    B(0x929F08) = static_cast<UC>(PickOf(0, 1, 2, 0xFF, 3, sh::Next() % 3));
    B(0x929F0B) = static_cast<UC>(PickOf(0, 1, 0, 1, 0xFF, sh::Next()));
    B(0x6BDFAF) = static_cast<UC>(PickOf(0, 1, 2, 3, 0xFF, sh::Next() % 4));
    B(0x6BDFB7) = static_cast<UC>(PickOf(0, 1, 2, 3, 0xFF, sh::Next() % 4));
    for (U i = 0; i < 6; ++i) B(0x6BDFA8 + i) = static_cast<UC>(PickOf(0, 0, sh::Next() % 0x40, sh::Next()));
}

void SeedWindows(bool items) {
    // the category or slot, the list's top and cursor and pick
    B(0x80333E) = static_cast<UC>(items ? sh::Next() % 4 : PickOf(0, 1, 2, 3, 4, 5, 6, sh::Next() % 0x10));
    const UC top = static_cast<UC>(PickOf(0, 1, 8, 9, 0xA, 0xE, 0xF, 0x17, 0x18, 0x6E, 0x6F, 0x77, sh::Next() & 0x7F));
    B(0x80333F) = top;
    B(0x803340) = static_cast<UC>(items ? PickOf(0, 1, 0x1E, 0x1F, 0x7E, 0x7F, top, top + 8u, top + 9u, top - 1u, sh::Next() & 0x7F)
                                        : PickOf(0, 1, 2, sh::Next() % 3));
    B(0x803341) = static_cast<UC>(PickOf(0xFF, 0xFF, 0, 1, sh::Next() & 0x7F));
    SetW(0x803346, PickOf(0, 0, 0, 0x10, sh::Next()));
    B(0x803360) = static_cast<UC>(PickOf(1, 2, 3, sh::Next()));
    B(0x803362) = static_cast<UC>(sh::Next() % 4);
    B(0x803363) = static_cast<UC>(PickOf(0, 1, 2, 3, 0xFF));
    B(0x803365) = static_cast<UC>(PickOf(0, sh::Next() % 0x40, sh::Next()));
    B(0x8031F3) = static_cast<UC>(PickOf(0, 0, 1, sh::Next()));
    B(0x803337) = static_cast<UC>(PickOf(0, 0, 1, sh::Next()));
    // the Ability screen's cells
    B(0x8033A3) = static_cast<UC>(PickOf(0, 0, 1, 4));
    B(0x8033A8) = static_cast<UC>(PickOf(1, 3, sh::Next()));
    B(0x8033AA) = static_cast<UC>(PickOf(0, 1, 2, 0xFF, 3, sh::Next() % 3));
    B(0x8033AB) = static_cast<UC>(PickOf(0, 1, 2, 3, 3, 0xFF, 4));
    B(0x8033AC) = static_cast<UC>(PickOf(0, 1, 8, 9, 0xFF, sh::Next() % 10));
    B(0x8033AD) = static_cast<UC>(PickOf(0xFF, 0xFF, 0, 1, sh::Next() % 10));
    const UC vtop = static_cast<UC>(PickOf(0, 1, 8, 9, 0xA, sh::Next() % 0x12));
    B(0x8033CE) = vtop;
    B(0x8033CF) = static_cast<UC>(PickOf(0, 1, 0x10, 0x11, vtop, vtop + 8u, vtop + 9u, sh::Next() % 0x12));
    B(0x8033CC) = static_cast<UC>(PickOf(0, 0, 0x10, 0xF0));
    B(0x8033D0) = static_cast<UC>(PickOf(0, sh::Next()));
    B(0x8033F3) = static_cast<UC>(PickOf(0, 1, 2, 3, 0xFF));
}

void Seed(unsigned k) {
    const bool items = Has(k, "FieldItem");
    SeedButtons();
    SeedInventory();
    SeedParty();
    SeedMenu();
    SeedWindows(items);
    // FieldItems_ViewList32 reads only the id list: category 4 (the key items) too
    if (Is(k, "FieldItems_ViewList32") && sh::Half()) B(0x80333E) = static_cast<UC>(sh::Next() % 5);
    // the dispatchers' bytes inside their tables
    if (Is(k, "FieldEquip_Run")) B(0x929F01) = static_cast<UC>(sh::Next() % 9);
    if (Is(k, "FieldAbility_Run")) B(0x929F01) = static_cast<UC>(sh::Next() % 10);
    if (Is(k, "FieldAbility_ArrangeRun")) B(0x929F02) = static_cast<UC>(sh::Next() % 3);
    if (Is(k, "FieldAbility_ViewRun")) B(0x929F02) = static_cast<UC>(sh::Next() % 5);
    // the type indexes 0x9398C0 + type + 4 member, written by the Ability states
    if (Has(k, "FieldAbility")) B(0x8033AB) = static_cast<UC>(PickOf(0, 1, 2, 3, sh::Next() % 4));
    // the equipment previews against the records they were drawn from
    if (Has(k, "FieldEquip") && sh::Half()) {
        const UC* const rec = sh::Mem(0x903A70 + 0xA4u * MoveScript_EffectState[B(0x904062 + B(0x803340) % 3)]);
        for (U i = 0; i < 6; ++i)
            if (sh::Half()) B(0x6BDFA8 + i) = rec[0x12 + i];
    }
}

void Args(unsigned k, std::uint32_t* a) {
    const std::uint32_t hi = a[9] & 0xFFFFFF00u;
    if (Is(k, "FieldItems_Sort"))
        a[0] = hi | (sh::Next() % 7);
    else if (Is(k, "FieldEquip_BestByPower") || Is(k, "FieldEquip_BestByOrder"))
        a[0] = hi | PickOf(0, 1, 2, sh::Next() % 3);
}

// --- the disturbance (from its hash only) -----------------------------------------------------

void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    const unsigned w = (h >> 16) & 0xFF;
    switch (h % 16) {
    case 0: B(0x80333E) = static_cast<UC>(v % 4); break;
    case 1: B(0x803340) = static_cast<UC>(v % 3); break;
    case 2: B(0x803341) = static_cast<UC>(v & 1 ? 0xFF : w & 0x7F); break;
    case 3: B(0x80333F) = static_cast<UC>(v & 0x7F); break;
    case 4: SetW(0x803346, v & 1 ? 0 : w); break;
    case 5: B(0x8033AA) = static_cast<UC>(v % 3); break;
    case 6: B(0x8033AB) = static_cast<UC>(v % 4); break;
    case 7: B(0x8033AC) = static_cast<UC>(v % 10); break;
    case 8: B(0x8033AD) = static_cast<UC>(v & 1 ? 0xFF : w % 10); break;
    case 9: B(0x929F06) = static_cast<UC>(v % 3); break;
    case 10: B(0x929F0B) = static_cast<UC>(v & 1); break;
    case 11: B(0x6BDFAF + 8 * (v & 1)) = static_cast<UC>(w % 4); break;   // 0x6BDFAF or 0x6BDFB7
    case 12: B(0x904154 + (w << 1 | (v & 1))) = static_cast<UC>(h >> 24); break;
    case 13: B(0x6BDFA8 + v % 6) = static_cast<UC>(w); break;
    case 14: {
        // a member id whose record is one of the eight, from the hash alone
        UC id = static_cast<UC>(w % 24);
        for (int tries = 0; tries < 24 && MoveScript_EffectState[id] >= 8; ++tries) id = static_cast<UC>((id + 1) % 24);
        B(0x904062 + v % 3) = id;
        break;
    }
    case 15: B(0x8033F3) = static_cast<UC>(v % 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R2E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_2e: BOF3X_R2E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.menu_span = 3;
    sh::Run(g);
}

}  // namespace rest_2e
