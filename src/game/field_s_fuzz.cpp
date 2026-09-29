// BOF3X_SHADOW=field_s: group FS's 53 functions through the scenario harness in
// field mode (scenario_harness.h, docs/scenario_harness.md section 7), once at
// start-up. docs/field_s.md section 4. BOF3X_FS_RUN=<function name> runs one
// (the negative controls' runs).
//
// The clone rows are tools/band_rows.py's (--group FS --clones, 2026-09-29),
// each read against the disassembly, names given; the extents are the code's
// (the cut's sizes include padding; 0x58C7A0's 1440 covered 0x58CAE0 too, a
// start of its own - docs/field_s.md section 2). The shapes: every state
// handler of a menu table kMenu (the state and step bytes drawn below
// menu_span 3, the longer dispatchers' step seeded per function), the helpers
// kCall with their arguments set by Args, the two that answer in al with
// ret_mask 0xFF.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

// PartyForm_DrawReserve's copy is this file's (section 4 of docs/field_s.md):
// its first call site is DIV-0011's, re-aimed at Menu_DrawFrame before this
// module's inject, which the harness's CloneOriginal refuses. The harness gets
// a six-byte `jmp [copy]` in its place (scena_sc12_fuzz.cpp's way).
extern "C" {
void* g_fs_reserve_copy = nullptr;
__attribute__((naked)) void FsReserveTheirs() { asm("jmp *_g_fs_reserve_copy"); }
}

namespace field_s {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;

// tools/band_rows.py --group FS --clones, 2026-09-29.
constexpr sh::CallSite kCalls57FF80[] = {{0x11, 0x574AB0}, {0x1B, 0x497740}, {0x2C, 0x516B30}, {0x37, 0x588D20}, {0x3F, 0x5747D0}, {0x56, 0x587740}, {0x70, 0x587740}};
constexpr sh::CallSite kCalls580310[] = {{0x7, 0x57C140}, {0x1F, 0x4976D0}, {0x39, 0x57C140}, {0x4C, 0x57C0F0}, {0x56, 0x495040}};
constexpr sh::CallSite kCalls580380[] = {{0x11, 0x580970}, {0x47, 0x57C140}, {0x76, 0x57C110}, {0xA0, 0x531BB0}, {0xAC, 0x533CE0}, {0x101, 0x533BA0}, {0x110, 0x5891F0}, {0x19D, 0x56F670}, {0x1B5, 0x579740}};
constexpr sh::CallSite kCalls580A60[] = {{0x6, 0x531BB0}, {0x109, 0x495040}};
constexpr sh::CallSite kCalls580B80[] = {{0xC, 0x495040}};
constexpr sh::CallSite kCalls580BB0[] = {{0x0, 0x581720}};
constexpr sh::CallSite kCalls580BE0[] = {{0x6, 0x575690}, {0xE, 0x5811E0}};
constexpr sh::CallSite kCalls580C20[] = {{0x3, 0x581720}, {0x13, 0x497740}, {0x24, 0x516B30}, {0x54, 0x591940}, {0x5B, 0x497740}, {0x6C, 0x516B30}, {0x86, 0x461EB0}, {0x8F, 0x531BB0}, {0x176, 0x587740}, {0x19D, 0x587740}, {0x1C5, 0x581590}, {0x1DF, 0x587740}, {0x209, 0x587740}, {0x247, 0x495040}};
constexpr sh::CallSite kCalls580E90[] = {{0xA, 0x580970}, {0x23, 0x581720}};
constexpr sh::CallSite kCalls580EC0[] = {{0x0, 0x580970}};
constexpr sh::CallSite kCalls580EE0[] = {{0x3, 0x580970}, {0x24, 0x5367E0}, {0x40, 0x57C140}, {0x9A, 0x533BA0}, {0xA9, 0x5891F0}, {0xDE, 0x531BB0}, {0xEA, 0x533CE0}, {0x112, 0x533BA0}, {0x120, 0x5891F0}, {0x1D2, 0x56F670}, {0x238, 0x56F670}, {0x273, 0x5720C0}, {0x2AD, 0x56F670}, {0x2B4, 0x495040}};
constexpr sh::CallSite kCalls5811E0[] = {{0x6, 0x531BB0}, {0x7B, 0x573560}, {0x8B, 0x5B9380}, {0xB1, 0x517090}, {0xDF, 0x581300}, {0x106, 0x574AB0}};
constexpr sh::CallSite kCalls581300[] = {{0x13, 0x4DF820}, {0x8A, 0x57CF60}, {0xA5, 0x573F30}, {0xB7, 0x516B30}, {0xC9, 0x574400}, {0xE1, 0x5B9380}, {0xEF, 0x517090}, {0x13B, 0x516E70}, {0x150, 0x574400}, {0x182, 0x5B9380}, {0x193, 0x517090}, {0x1B7, 0x5B9380}, {0x1C8, 0x517090}, {0x1DD, 0x574400}, {0x21A, 0x5B9380}, {0x22B, 0x517090}, {0x241, 0x5B9380}, {0x24F, 0x517090}};
constexpr sh::CallSite kCalls581590[] = {{0x32, 0x58BD50}, {0x5C, 0x58BD50}, {0x104, 0x587740}, {0x110, 0x531BB0}, {0x155, 0x531BB0}};
constexpr sh::CallSite kCalls581720[] = {{0xA, 0x575690}, {0x11, 0x531BB0}, {0x2C, 0x574AB0}, {0x8F, 0x573560}, {0x9F, 0x5B9380}, {0xC1, 0x517090}, {0xE2, 0x581300}, {0x137, 0x573CE0}, {0x186, 0x573CE0}};
constexpr sh::CallSite kCalls5837F0[] = {{0xE, 0x587740}, {0x19, 0x584010}, {0x40, 0x583EA0}};
constexpr sh::CallSite kCalls583880[] = {{0x4, 0x531BB0}, {0xD, 0x584010}, {0x14, 0x497740}, {0x25, 0x516B30}, {0x3E, 0x583EA0}, {0x63, 0x573CE0}, {0x77, 0x461EB0}, {0x8E, 0x587740}, {0xBA, 0x587740}, {0x119, 0x587740}, {0x148, 0x587740}, {0x177, 0x587740}};
constexpr sh::CallSite kCalls583A20[] = {{0x3, 0x584010}, {0x14, 0x497740}, {0x25, 0x516B30}, {0x3F, 0x583EA0}, {0x64, 0x573CE0}, {0x83, 0x5905D0}, {0x94, 0x461EB0}, {0xAB, 0x587740}, {0xD8, 0x587740}, {0x145, 0x587740}, {0x175, 0x587740}, {0x1A5, 0x587740}};
constexpr sh::CallSite kCalls583BE0[] = {{0x2, 0x584010}, {0x9, 0x497740}, {0x1A, 0x516B30}, {0x35, 0x583EA0}, {0x5A, 0x573CE0}, {0x62, 0x5747D0}, {0xDB, 0x587740}};
constexpr sh::CallSite kCalls583CD0[] = {{0x2, 0x584010}, {0x28, 0x583EA0}};
constexpr sh::CallSite kCalls583D30[] = {{0x16, 0x583EA0}, {0x1D, 0x584010}, {0x42, 0x573CE0}, {0x4E, 0x584120}, {0x96, 0x590660}};
constexpr sh::CallSite kCalls583DE0[] = {{0x14, 0x583EA0}, {0x1B, 0x584010}, {0x27, 0x584120}};
constexpr sh::CallSite kCalls583E30[] = {{0x14, 0x583EA0}, {0x1B, 0x584010}, {0x40, 0x573CE0}, {0x50, 0x584120}};
constexpr sh::CallSite kCalls583EA0[] = {{0x22, 0x57CF60}, {0x2D, 0x5762D0}, {0xE6, 0x497740}, {0x109, 0x497740}, {0x11B, 0x516B30}, {0x12E, 0x5B9380}, {0x142, 0x516F60}};
constexpr sh::CallSite kCalls584010[] = {{0x32, 0x574AB0}, {0x3F, 0x531BB0}, {0xC4, 0x573560}, {0xD4, 0x531BB0}, {0xF8, 0x574610}};
constexpr sh::CallSite kCalls584120[] = {{0xD, 0x497740}, {0x1E, 0x516B30}};
constexpr sh::CallSite kCalls584190[] = {{0x0, 0x584F90}};
constexpr sh::CallSite kCalls5841B0[] = {{0x67, 0x461EB0}, {0x79, 0x587740}, {0xAF, 0x5918E0}, {0xC1, 0x5919B0}, {0xD3, 0x587740}, {0xDF, 0x5918E0}, {0x107, 0x587740}, {0x1AA, 0x587740}, {0x1CC, 0x587740}, {0x1D6, 0x587740}, {0x242, 0x587740}, {0x268, 0x587740}, {0x282, 0x5918E0}};
constexpr sh::CallSite kCalls584470[] = {{0x54, 0x587740}, {0x8C, 0x587740}, {0x96, 0x587740}, {0xAA, 0x591B60}, {0x158, 0x587740}};
constexpr sh::CallSite kCalls584600[] = {{0x28, 0x591940}, {0x7F, 0x461EB0}, {0x91, 0x587740}, {0xC8, 0x587740}, {0x135, 0x587740}, {0x13F, 0x587740}, {0x1BF, 0x587740}};
constexpr sh::CallSite kCalls5847F0[] = {{0x86, 0x461EB0}, {0xCC, 0x587740}, {0x10E, 0x587740}, {0x13D, 0x587740}, {0x14B, 0x590C90}, {0x15E, 0x587740}, {0x177, 0x587740}, {0x181, 0x587740}};
constexpr sh::CallSite kCalls5849B0[] = {{0x6E, 0x461EB0}, {0x15B, 0x587740}, {0x1B9, 0x587740}, {0x1E8, 0x587740}};
constexpr sh::CallSite kCalls584C10[] = {{0x41, 0x461EB0}, {0x53, 0x587740}, {0x7C, 0x587740}, {0xAF, 0x587740}, {0xD3, 0x5857E0}, {0xEA, 0x587740}, {0xF4, 0x587740}};
constexpr sh::CallSite kCalls584D30[] = {{0x6C, 0x461EB0}, {0x158, 0x587740}, {0x19E, 0x587740}, {0x1B7, 0x587740}, {0x1C6, 0x587740}, {0x213, 0x587740}};
constexpr sh::CallSite kCalls584F90[] = {{0xC0, 0x5918E0}};
constexpr sh::CallSite kCalls585090[] = {{0x2E, 0x57CF60}, {0x49, 0x57DF00}, {0xC1, 0x5918A0}, {0x126, 0x57DC90}, {0x14C, 0x57DC90}, {0x16B, 0x57DC90}, {0x1DA, 0x57CF60}, {0x203, 0x57CF60}, {0x222, 0x57D800}, {0x240, 0x516B30}, {0x256, 0x591AC0}, {0x26E, 0x5B9380}, {0x28D, 0x517090}, {0x2A3, 0x57D910}, {0x2B9, 0x57D910}, {0x2DD, 0x57D860}, {0x308, 0x57D860}, {0x32E, 0x57D860}, {0x357, 0x57D860}, {0x37D, 0x57D860}, {0x395, 0x57D860}, {0x3BE, 0x57D860}, {0x3E4, 0x57D860}, {0x40C, 0x57D860}, {0x432, 0x57D860}, {0x45A, 0x57DD10}};
constexpr sh::CallSite kCalls585500[] = {{0x2F, 0x57CF60}, {0x61, 0x5918A0}, {0xAE, 0x57DC90}, {0xD0, 0x57DC90}, {0xF3, 0x57DC90}, {0x125, 0x57D800}, {0x157, 0x516B30}, {0x17D, 0x57D910}, {0x1A7, 0x57D910}, {0x1DD, 0x57D860}, {0x208, 0x57D860}, {0x22D, 0x57D860}, {0x255, 0x57D860}, {0x277, 0x57D860}, {0x293, 0x57D860}, {0x2BC, 0x57D860}};
constexpr sh::CallSite kCalls5857F0[] = {{0x28, 0x58BD50}};
constexpr sh::CallSite kCalls585840[] = {{0x5, 0x5857F0}, {0x4F, 0x58BD50}};
constexpr sh::CallSite kCalls5858C0[] = {{0x5, 0x5857F0}, {0x4F, 0x58BD50}};
constexpr sh::CallSite kCalls585940[] = {{0x1F, 0x57CF60}, {0x38, 0x516B30}, {0x43, 0x5919B0}, {0x58, 0x5B9380}, {0x6C, 0x516F60}, {0x7A, 0x57D910}, {0x95, 0x57D910}, {0xAD, 0x57D910}};
constexpr sh::CallSite kCalls58C7A0[] = {{0xB, 0x575690}, {0x53, 0x461EB0}, {0x78, 0x587740}, {0xA6, 0x587740}, {0xC5, 0x531BB0}, {0xDC, 0x587740}, {0x109, 0x587740}, {0x1DA, 0x591C20}, {0x1FC, 0x587740}, {0x255, 0x587740}, {0x262, 0x590BB0}, {0x26B, 0x590660}, {0x27E, 0x587740}, {0x29E, 0x587740}, {0x2A8, 0x587740}, {0x2D0, 0x531BB0}, {0x2F8, 0x531BB0}};
constexpr sh::JumpTable kTables58C7A0[] = {{0x196, 0x324, 5}};
constexpr sh::CallSite kCalls58CAE0[] = {{0x8, 0x575690}, {0x5A, 0x591C20}, {0x73, 0x461EB0}, {0x7D, 0x58D640}, {0x1BF, 0x587740}, {0x1EE, 0x587740}, {0x1F6, 0x58D570}, {0x215, 0x587740}, {0x22D, 0x587740}};

// The copy of 0x581300: every site re-aimed at a trampoline into the recorder
// that stands in for its callee (sh::StandIn: the same log entry, disturbance
// and answer a site the harness re-aims gets) - the first at the recorder keyed
// by where the site reaches now (DIV-0011's Menu_DrawFrame, listed below).
using Fn10 = U (__cdecl*)(U, U, U, U, U, U, U, U, U, U);
U g_tramp_key[8];
template <int I> U __cdecl Tramp(U a0, U a1, U a2, U a3, U a4, U a5, U a6, U a7, U a8, U a9) {
    return reinterpret_cast<Fn10>(const_cast<void*>(sh::StandIn(g_tramp_key[I])))(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}
const void* const kTramps[] = {
    reinterpret_cast<const void*>(&Tramp<0>), reinterpret_cast<const void*>(&Tramp<1>), reinterpret_cast<const void*>(&Tramp<2>),
    reinterpret_cast<const void*>(&Tramp<3>), reinterpret_cast<const void*>(&Tramp<4>), reinterpret_cast<const void*>(&Tramp<5>),
    reinterpret_cast<const void*>(&Tramp<6>), reinterpret_cast<const void*>(&Tramp<7>),
};
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

void* CopyReserve() {
    // the callees of kCalls581300 by the address the disassembly shows, and the key ours calls
    const U callees[8][2] = {
        {0x4DF820, FrameCallTarget()},
        {0x57CF60, KeyOf(&::Menu_DrawBox)},
        {0x573F30, KeyOf(&::Menu_DrawItemIcon)},
        {0x516B30, KeyOf(&::Text_DrawAt)},
        {at::kDrawTag, at::kDrawTag},
        {0x5B9380, KeyOf(Crt_sprintf)},
        {0x517090, KeyOf(&::Text_DrawFont8)},
        {0x516E70, KeyOf(&::Text_DrawSmall)},
    };
    constexpr int n = static_cast<int>(sizeof kCalls581300 / sizeof kCalls581300[0]);
    static bof3::CloneCall calls[n];
    for (int i = 0; i < n; ++i) {
        int t = -1;
        for (int j = 0; j < 8; ++j)
            if (callees[j][0] == kCalls581300[i].target) t = j;
        if (t < 0) bof3::Fatal("field_s: no trampoline for 0x%X", (unsigned)kCalls581300[i].target);
        g_tramp_key[t] = callees[t][1];
        // the frame's site reaches Menu_DrawFrame under DIV-0011: expected is where it reaches now
        const U expected = i == 0 ? FrameCallTarget() : kCalls581300[i].target;
        calls[i] = {kCalls581300[i].offset, kTramps[t], expected};
    }
    return bof3::CloneOriginal("PartyForm_DrawReserve", 0x581300, 0x285, calls, n);
}

std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("field_s: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return Key(p);
}

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FS_FN(name) reinterpret_cast<const void*>(&::name)
#define FS_ROW(name, base, size, calls) {#name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, FS_FN(name), 0, false, kM}
#define FS_BARE(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, FS_FN(name), 0, false, kM}
constexpr sh::Shape kM = sh::Shape::kMenu, kC = sh::Shape::kCall;
const sh::Clone kAll[] = {
    FS_ROW(FieldSave_Confirm, 0x57FF80, 0x85, kCalls57FF80),
    FS_ROW(Rest_Begin, 0x580310, 0x6B, kCalls580310),
    FS_ROW(Rest_PlaceParty, 0x580380, 0x1D8, kCalls580380),
    FS_BARE(PartyForm_OpenStep, 0x580A50, 0xE),
    FS_ROW(PartyForm_Setup, 0x580A60, 0x116, kCalls580A60),
    FS_ROW(PartyForm_FadeWait, 0x580B80, 0x21, kCalls580B80),
    FS_ROW(PartyForm_OpenWait, 0x580BB0, 0x23, kCalls580BB0),
    FS_ROW(PartyForm_SlideIn, 0x580BE0, 0x35, kCalls580BE0),
    FS_ROW(PartyForm_Choose, 0x580C20, 0x257, kCalls580C20),
    FS_BARE(PartyForm_LeaveStep, 0x580E80, 0xE),
    FS_ROW(PartyForm_LeaveFade, 0x580E90, 0x28, kCalls580E90),
    FS_ROW(PartyForm_LeaveBlack, 0x580EC0, 0x1A, kCalls580EC0),
    FS_ROW(PartyForm_Reload, 0x580EE0, 0x2CC, kCalls580EE0),
    FS_BARE(PartyForm_End, 0x5811B0, 0x29),
    {"PartyForm_DrawSliding", 0x5811E0, 0x113, kCalls5811E0, SH_N(kCalls5811E0), nullptr, 0, nullptr, 0, FS_FN(PartyForm_DrawSliding), 0, false, kC},
    {"PartyForm_DrawReserve", Wrapper(&FsReserveTheirs), 6, nullptr, 0, nullptr, 0, nullptr, 0, FS_FN(PartyForm_DrawReserve), 0, false, kC},   // this file's copy
    {"PartyForm_Swap", 0x581590, 0x18F, kCalls581590, SH_N(kCalls581590), nullptr, 0, nullptr, 0, FS_FN(PartyForm_Swap), 0xFF, false, kC},
    {"PartyForm_Draw", 0x581720, 0x18F, kCalls581720, SH_N(kCalls581720), nullptr, 0, nullptr, 0, FS_FN(PartyForm_Draw), 0, false, kC},
    {"ShopBrowse_InitWindows", 0x5836E0, 0x90, nullptr, 0, nullptr, 0, nullptr, 0, FS_FN(ShopBrowse_InitWindows), 0, false, kC},
    {"ShopBrowse_OpenDetail", 0x583770, 0x65, nullptr, 0, nullptr, 0, nullptr, 0, FS_FN(ShopBrowse_OpenDetail), 0, false, kC},
    FS_ROW(ShopResist_Open, 0x5837F0, 0x82, kCalls5837F0),
    FS_ROW(ShopResist_PickMember, 0x583880, 0x196, kCalls583880),
    FS_ROW(ShopResist_PickBit, 0x583A20, 0x1BA, kCalls583A20),
    FS_ROW(ShopResist_Confirm, 0x583BE0, 0xF0, kCalls583BE0),
    FS_ROW(ShopResist_Close, 0x583CD0, 0x5D, kCalls583CD0),
    FS_ROW(ShopResist_Grant, 0x583D30, 0xAB, kCalls583D30),
    FS_ROW(ShopResist_Farewell, 0x583DE0, 0x47, kCalls583DE0),
    FS_ROW(ShopResist_Notice, 0x583E30, 0x64, kCalls583E30),
    {"ShopResist_DrawBits", 0x583EA0, 0x166, kCalls583EA0, SH_N(kCalls583EA0), nullptr, 0, nullptr, 0, FS_FN(ShopResist_DrawBits), 0, false, kC},
    {"ShopResist_DrawMembers", 0x584010, 0x103, kCalls584010, SH_N(kCalls584010), nullptr, 0, nullptr, 0, FS_FN(ShopResist_DrawMembers), 0, false, kC},
    {"ShopResist_Message", 0x584120, 0x5D, kCalls584120, SH_N(kCalls584120), nullptr, 0, nullptr, 0, FS_FN(ShopResist_Message), 0xFF, false, kC},
    FS_ROW(SharedList_Begin, 0x584190, 0x19, kCalls584190),
    FS_ROW(SharedList_Menu, 0x5841B0, 0x2A4, kCalls5841B0),
    FS_BARE(SharedList_MoveStep, 0x584460, 0xE),
    FS_ROW(SharedList_UseItem, 0x584470, 0x16D, kCalls584470),
    FS_BARE(Menu_StepAfterTimer, 0x5845E0, 0x15),
    FS_ROW(SharedList_PickMember, 0x584600, 0x1E3, kCalls584600),
    FS_ROW(SharedList_PickSlot, 0x5847F0, 0x1B3, kCalls5847F0),
    FS_ROW(SharedList_PickShared, 0x5849B0, 0x1FD, kCalls5849B0),
    FS_BARE(SharedList_SortStep, 0x584BB0, 0xE),
    FS_BARE(SharedList_SortOpen, 0x584BC0, 0x50),
    FS_ROW(SharedList_SortMenu, 0x584C10, 0x117, kCalls584C10),
    FS_ROW(SharedList_Arrange, 0x584D30, 0x23A, kCalls584D30),
    {"SharedList_Setup", 0x584F90, 0xFC, kCalls584F90, SH_N(kCalls584F90), nullptr, 0, nullptr, 0, FS_FN(SharedList_Setup), 0, false, kC},
    {"SharedList_DrawList", 0x585090, 0x469, kCalls585090, SH_N(kCalls585090), nullptr, 0, nullptr, 0, FS_FN(SharedList_DrawList), 0, false, kC},
    {"SharedList_DrawMember", 0x585500, 0x2D3, kCalls585500, SH_N(kCalls585500), nullptr, 0, nullptr, 0, FS_FN(SharedList_DrawMember), 0, false, kC},
    {"SharedList_Sort", 0x5857E0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, FS_FN(SharedList_Sort), 0, false, kC},
    {"SharedList_Compact", 0x5857F0, 0x4E, kCalls5857F0, SH_N(kCalls5857F0), nullptr, 0, nullptr, 0, FS_FN(SharedList_Compact), 0, false, kC},
    {"SharedList_SortCostDown", 0x585840, 0x76, kCalls585840, SH_N(kCalls585840), nullptr, 0, nullptr, 0, FS_FN(SharedList_SortCostDown), 0, false, kC},
    {"SharedList_SortCostUp", 0x5858C0, 0x76, kCalls5858C0, SH_N(kCalls5858C0), nullptr, 0, nullptr, 0, FS_FN(SharedList_SortCostUp), 0, false, kC},
    {"SharedList_DrawItemCount", 0x585940, 0xB9, kCalls585940, SH_N(kCalls585940), nullptr, 0, nullptr, 0, FS_FN(SharedList_DrawItemCount), 0, false, kC},
    {"Equip_ChooseSlot", 0x58C7A0, 0x338, kCalls58C7A0, SH_N(kCalls58C7A0), nullptr, 0, kTables58C7A0, SH_N(kTables58C7A0), FS_FN(Equip_ChooseSlot), 0, false, kM},
    FS_ROW(Equip_ChooseItem, 0x58CAE0, 0x251, kCalls58CAE0),
};
#undef FS_ROW
#undef FS_BARE
#undef FS_FN
#undef SH_N
constexpr unsigned kAllN = sizeof kAll / sizeof kAll[0];
static_assert(kAllN == 53, "the group's 53 functions");

// The .data tables the group's dispatchers read in place (swapped for
// recorders while the fuzz runs).
const sh::DataTable kTables[] = {
    {at::kPartyFormOpenSteps, 3}, {at::kPartyFormLeaveSteps, 4}, {at::kSharedListMoveSteps, 5},
    {at::kSharedListSortSteps, 3}, {at::kSharedListSorts, 3},
};

unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return *Mem(a); }
void SetW(U a, U v) { move_script::SetWord(Mem(a), v); }
void SetL(U a, U v) { move_script::SetLong(Mem(a), static_cast<std::int32_t>(v)); }
U L(U a) { return static_cast<U>(move_script::Long(Mem(a))); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the moves ------------------------------------------------------------------
//
// What the functions read again after a call and a caller could have moved: the
// menu block's cursor and pick cells, the reserve's count and answer, the
// shared list's cells, the item and slot windows' cells, the party list, the
// zenny, a record's +0xA / +0xB / +0x1D, the area, 0x904152, Field_StatusBits'
// bit 6, the member count. Kept inside what the functions index with them.
void Move(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    const unsigned w = (h >> 16) & 0xFF;
    switch (h % 24) {
    case 0: B(at::kCursor) = static_cast<unsigned char>(v % 3); break;
    case 1: B(at::kColumn) = static_cast<unsigned char>(v & 1); break;
    case 2: B(at::kRow) = static_cast<unsigned char>(static_cast<int>(v % 6) - 1); break;
    case 3: B(at::kPickColumn) = static_cast<unsigned char>(w % 4 == 0 ? 0x7F : w % 4 == 1 ? 0 : w % 4 == 2 ? 1 : v); break;
    case 4: B(at::kAnswer) = static_cast<unsigned char>(v); break;
    case 5: B(at::kPickRow) = static_cast<unsigned char>(v % 8); break;
    case 6: B(at::kReserveCount) = static_cast<unsigned char>(v % 6); break;
    case 7: B(at::kPartyFormResult) = static_cast<unsigned char>(v & 1 ? 0xFF : w % 8); break;
    case 8: B(at::kPickIndex) = static_cast<unsigned char>(v % 8); break;
    case 9: B(at::kSharedChoice) = static_cast<unsigned char>(v); break;
    case 10: B(0x8031FB) = static_cast<unsigned char>(v); break;
    case 11: B(0x8031FA) = static_cast<unsigned char>(v % 0x78); break;
    case 12: B(0x8031FC) = static_cast<unsigned char>(v & 1 ? 0xFF : w & 0x7F); break;
    case 13: B(0x8031D7) = static_cast<unsigned char>(v % 10); break;
    case 14: B(0x80321E) = static_cast<unsigned char>(v % 4); break;
    case 15: B(0x803340) = static_cast<unsigned char>(v % 3); break;
    case 16: B(0x80333E) = static_cast<unsigned char>(v % 7); break;
    case 17: B(0x803362 + (v & 3)) = static_cast<unsigned char>(w); break;
    case 18: B(at::kPartyList + v % 6) = static_cast<unsigned char>(w % 24); break;   // MoveScript_EffectState's 24
    case 19: SetL(at::kZenny, L(at::kZenny) ^ (1u << (v & 15))); break;
    case 20: {
        static const unsigned char kOffsets[] = {0xA, 0xB, 0x1D};
        B(at::kRecords + (v % 8) * at::kRecordStride + kOffsets[w % 3]) = static_cast<unsigned char>(h >> 24);
        break;
    }
    case 21: B(at::kLoneParty) = static_cast<unsigned char>(v & 1); break;
    case 22: Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ 0x40); break;
    case 23: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    default: break;
    }
}

std::uint32_t Stir(const std::uint32_t*, std::uint32_t answer) {
    Move(sh::Noise());
    return answer;
}
// Msg_SystemPtr's answer lands in the harness's text buffer (as the standard
// set's FxText); Text_DrawAt and TextRecord_Set compare the pointer.
std::uint32_t TextAnswer(const std::uint32_t*, std::uint32_t answer) { return Key(sh::Text() + (answer & 0xF0)); }
// Menu_ListScroll writes both out-bytes on every path (menu_windows.cpp): the
// offset any byte, moving 0 or 1; the top and the state as a step can leave
// them. SharedList_DrawList reads all four after it.
std::uint32_t ListScroll(const std::uint32_t* a, std::uint32_t answer) {
    auto* const top = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    auto* const offset = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    auto* const moving = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    auto* const state = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[3]));
    const U n = sh::Noise();
    *offset = static_cast<unsigned char>(n);
    *moving = static_cast<unsigned char>((n >> 8) & 1);
    if (sh::InRegions(top, 1) && (n & 0x10000)) *top = static_cast<unsigned char>(*top + ((n >> 17) & 1 ? 1 : 0xFF));
    if (sh::InRegions(state, 1) && (n & 0x40000)) *state = static_cast<unsigned char>(n >> 24);
    return (answer & 0xFFFFFF00u) | (sh::InRegions(top, 1) ? *top : answer & 0xFF);
}

// Crt_sprintf as the standard set's FxSprintf: up to seven letters and a NUL
// at the destination (always the text scratch 0x904BA0 here), the count
// answered.
std::uint32_t Sprintf(const std::uint32_t* a, std::uint32_t) {
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return 0;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}

constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kAll32 = 0xFFFFFFFFu;
#define FS_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)

sh::Callee g_callees[] = {
    // the group's own, called directly (E8 / E9): their arguments logged by what they read
    {FS_OURS(PartyForm_Draw), 0, {}, kG, 0, 0},
    {FS_OURS(PartyForm_DrawSliding), 0, {}, kG, 0, 0},
    {FS_OURS(PartyForm_DrawReserve), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {FS_OURS(PartyForm_Swap), 0, {}, sh::Answer::kByte, 0xFE, 0x07, {}, &Stir},
    {FS_OURS(ShopResist_DrawBits), 5, {0xFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(ShopResist_DrawMembers), 1, {0xFF}, kG, 0, 0},
    {FS_OURS(ShopResist_Message), 2, {kAll32, 0xFF}, sh::Answer::kFlag, 0, 0, {}, &Stir},
    {FS_OURS(SharedList_Setup), 0, {}, kG, 0, 0, {}, &Stir},
    {FS_OURS(SharedList_Sort), 1, {0xFF}, kG, 0, 0},
    {FS_OURS(SharedList_Compact), 0, {}, kG, 0, 0},
    // nobody's, or another group's this wave (FO's 0x574400), by address
    {"0x574400", at::kDrawTag, at::kDrawTag, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {"0x58D640", at::kEquipPreview, at::kEquipPreview, 0, {}, kG, 0, 0},
    {"0x58D570", at::kEquipApply, at::kEquipApply, 0, {}, kG, 0, 0, {}, &Stir},
    // the standard set re-listed where Capcom pushes a register over a callee's
    // leftover, with what each callee reads (docs/field_s.md section 3)
    {FS_OURS(Menu_DrawTitleBox), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawMemberStatus), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawMoneyBox), 4, {0xFFFF, 0xFFFF, 0, kAll32}, kG, 0, 0},
    {FS_OURS(Menu_DrawCursorBox), 6, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawBox), 6, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawBackdrop), 1, {0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawItemIcon), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawHand), 3, {0xFFFF, 0xFFFF, 0}, kG, 0, 0},
    {FS_OURS(Menu_DrawBorder), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawSkillRow), 7, {0xFFFF, 0xFFFF, 0xFF, 0xFF, kAll32, 0xFF, 0xFF}, kG, 0, 0, {0, 0, 0, 0, 16, 0, 0}, nullptr, nullptr, true},
    {FS_OURS(Menu_DrawPieces), 4, {0xFFFF, 0xFFFF, kAll32, 0xFF}, kG, 0, 0, {0, 0, 16, 0}, nullptr, nullptr, true},
    {FS_OURS(Menu_DrawPiece), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Menu_DrawScrollBar), 7, {kAll32, 0xFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF, 0xFF}, kG, 0, 0, {16, 0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},
    {FS_OURS(Menu_ListScroll), 4, {kAll32, 0, 0, kAll32}, sh::Answer::kFlag, 0, 0, {}, &ListScroll},
    {FS_OURS(Text_DrawAt), 5, {0xFFFF, 0xFFFF, 0xFF, 0xFF, kAll32}, kG, 0, 0},
    {FS_OURS(Text_DrawFont8), 4, {0xFFFF, 0xFFFF, 0x3F, kAll32}, kG, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},
    {FS_OURS(Text_DrawFont12), 4, {0xFFFF, 0xFFFF, 0x3F, kAll32}, kG, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},
    {FS_OURS(Msg_SystemPtr), 1, {0xFFFF}, kG, 0, 0, {}, &TextAnswer},
    {FS_OURS(Skill_FlagIndex), 1, {0xFF}, sh::Answer::kFlag, 0, 0},
    {FS_OURS(Item_HelpMessage), 2, {0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Inventory_Add), 3, {0xFF, 0xFF, 0xFF}, sh::Answer::kFlag, 0, 0},
    {FS_OURS(Inventory_Count), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(AbilityList_Add), 4, {0xFF, 0xFF, 0xFF, 0xFF}, sh::Answer::kFlag, 0, 0},
    {FS_OURS(PartySet_Load), 4, {0xFF, 0xFF, 0xFF, 0xFF}, kG, 0, 0},
    {FS_OURS(Field_MemberSprite), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {FS_OURS(Party_Count), 1, {0xFF}, sh::Answer::kByte, 0, 3},
    {FS_OURS(Port_DroppedCall), 4, {kAll32, kAll32, kAll32, kAll32}, kG, 0, 0},
    {FS_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &Stir},
    // every format here takes one number: three words (the standard row logs a
    // fourth, the caller's stack)
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kAll32, kAll32, kAll32}, kG, 0, 0, {0, 16}, &Sprintf, nullptr, true},
    // DIV-0011's Menu_DrawFrame at PartyForm_DrawReserve's first site (x, y, w, h;
    // menu_frame.cpp reads x, y as s16, w, h as bytes): keyed at start-up by where
    // the site reaches; without the divergence a second Port_DroppedCall row
    // (skipped: its address is listed above)
    {FS_OURS(Port_DroppedCall), 4, {kAll32, kAll32, kAll32, kAll32}, kG, 0, 0},
};
constexpr unsigned kFrameRow = sizeof g_callees / sizeof g_callees[0] - 1;
#undef FS_OURS

// --- the state -------------------------------------------------------------------

// Beyond the harness's field regions (which hold the menu block, the style
// byte and records 0x903A70..0x903A93, Cond_Flags with the party lists and the
// zenny, the save block's bytes with the shared list, ObjTrio with
// Field_Members).
const sh::Region kRegions[] = {
    {sh::at::kWindows, sh::at::kWindowCount * sh::at::kWindowStride},   // WindowRecords 0x803160..0x803477
    {0x6BC880, 0x48},                                                  // the reserve, its flags, the joined list, the cursors
    {0x903A94, 0x903F90 - 0x903A94},                                   // CharacterRecords past the style region
    {0x903584, 0x10},                                                  // Field_ConfirmButtons / Field_CancelButtons
    {at::kEquipBytes, 0x50},                                           // FieldMenu's per-member bytes 0x939880..0x9398CF
};

void Disturb(std::uint32_t h) { Move(h); }

// --- the seed ----------------------------------------------------------------------

const char* g_only = nullptr;   // BOF3X_FS_RUN
const sh::Clone* g_clones = kAll;
unsigned g_n = kAllN;
sh::Clone g_one[1];

bool Is(unsigned k, const char* name) { return std::strcmp(g_clones[k].name, name) == 0; }

// A member id whose record index (MoveScript_EffectState) is one of the eight.
unsigned char SafeId() {
    for (int tries = 0; tries < 16; ++tries) {
        const auto id = static_cast<unsigned char>(sh::Next() % 24);
        if (MoveScript_EffectState[id] < 8) return id;
    }
    return 0;
}

void SeedButtons() {
    // one confirm bit and one cancel bit; the pressed word hits either, both or none
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetW(at::kConfirm, confirm | (sh::Half() ? 0 : 1u << (sh::Next() % 16)));
    SetW(at::kCancel, cancel);
    const U other = sh::Next() & 0xFFFF & ~(confirm | cancel);
    const U pressed = PickOf(confirm, cancel, confirm | cancel, 0, other, confirm | other, cancel | other);
    SetW(at::kPressed, pressed);
    SetW(at::kPressed + 2, sh::Next());
}

void SeedParty() {
    for (unsigned i = 0; i < 6; ++i) B(at::kPartyList + i) = sh::Next() % 6 != 0 ? SafeId() : 0xFF;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const obj = ObjTrio + i * at::kObjStride;
        obj[0x148] = static_cast<unsigned char>(sh::Next() % 8);   // Field_Members: a record index
        obj[0x89] = SafeId();
    }
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 4);
    for (unsigned r = 0; r < 8; ++r) {
        unsigned char* const rec = Mem(at::kRecords + r * at::kRecordStride);
        rec[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
        rec[0x1D] = static_cast<unsigned char>(PickOf(0, 0, 1u << (sh::Next() % 8), sh::Next()));
        rec[0xA] = static_cast<unsigned char>(PickOf(1, 10, 50, 99, sh::Next()));
        move_script::SetWord(rec + 0x18, PickOf(0, 1, 2, sh::Next() % 1000));
        move_script::SetWord(rec + 0x1A, PickOf(0, 1, 5, 25, 26, sh::Next() % 1000));
        move_script::SetWord(rec + 0x22, PickOf(0, 4, 100, 104, sh::Next() % 1000));
    }
}

void SeedMenu() {
    B(at::kTimer) = static_cast<unsigned char>(PickOf(1, 1, 2, 3, 5, 6, 7, 0, sh::Next()));
    B(at::kCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, 3, sh::Next() % 3));
    B(at::kColumn) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    B(at::kRow) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, 4, 5, sh::Next() % 3));
    B(at::kPickColumn) = static_cast<unsigned char>(PickOf(0x7F, 0x7F, 0, 1));
    B(at::kPickRow) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 7, 8, 0xFF, 0x7F, sh::Next() % 8));
    B(at::kAnswer) = static_cast<unsigned char>(PickOf(0, 1, 0xB, 0xC, 0xE, 0xF, sh::Next()));
    B(at::kMessageEnd) = static_cast<unsigned char>(PickOf(2, 0xC, sh::Next()));
    // the reserve's bytes, then its count - which is the reserve's fourth byte
    // (docs/field_s.md section 7, L2): at most 5, so every loop over it stays
    // inside the region
    for (unsigned i = 0; i < 5; ++i) B(at::kReserve + i) = sh::Often() ? SafeId() : static_cast<unsigned char>(sh::Next());
    B(at::kReserveCount) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5));
    B(at::kPartyFormResult) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, SafeId(), sh::Next()));
    B(at::kJoinedCount) = static_cast<unsigned char>(PickOf(0, 1, 3, 5, 8));
    B(at::kPickIndex) = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 7, 0xFF, sh::Next() % 8));
    for (unsigned i = 0; i < 8; ++i) B(at::kJoined + i) = static_cast<unsigned char>(sh::Next() % 8);
    B(at::kSharedChoice) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
}

void SeedWindows() {
    B(0x803187) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(0x8031CF) = static_cast<unsigned char>(PickOf(0, 0, 1, 3, sh::Next()));
    B(0x8031D6) = static_cast<unsigned char>(sh::Next() % 8);
    B(0x8031D7) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, sh::Next() % 10, 0xFF));
    B(0x8031FA) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0x6E, 0x6F, 0x76, 0x77, sh::Next() % 0x78));
    B(0x8031FB) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0x10, 0x7E, 0x7F, B(0x8031FA), B(0x8031FA) + 8u, B(0x8031FA) + 9u, sh::Next() & 0x7F));
    B(0x8031FC) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, sh::Next() & 0x7F));
    B(0x8031FD) = static_cast<unsigned char>(PickOf(0, 0, 0, 0x10, sh::Next()));
    B(0x80321E) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF));
    B(0x803340) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, sh::Next() % 3));
    B(0x80333E) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 0xFF));
    B(0x803362) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0x6E, 0x6F, 0x77, sh::Next() % 0x78));
    B(0x803363) = static_cast<unsigned char>(PickOf(0, 1, 9, 0x6E, 0x6F, 0x7E, 0x7F, B(0x803362), B(0x803362) + 9u, sh::Next() & 0x7F));
    B(0x803365) = static_cast<unsigned char>(PickOf(0, sh::Next()));
    SetW(0x803368, PickOf(0, 0, 0, 0x10, sh::Next()));
    for (unsigned i = 0; i < 0x80; ++i)
        if (sh::Next() % 5 == 0) B(at::kSharedList + i) = 0;
}

// Game_AreaNumber among the areas whose descriptor is set, or the ones compared.
void SeedArea() {
    U area = PickOf(0xBB, 0xC1, 0xBF, 0x85, 0x5C, sh::Next() % 200, sh::Next() % 200);
    for (int tries = 0; tries < 8 && L(at::kAreaDescriptors + 4 * area) == 0; ++tries) area = sh::Next() % 200;
    if (L(at::kAreaDescriptors + 4 * area) == 0) area = 0xBB;
    Game_AreaNumber = static_cast<unsigned short>(area);
}

void Seed(unsigned k) {
    SeedButtons();
    SeedParty();
    SeedMenu();
    SeedWindows();
    SeedArea();
    B(at::kLoneParty) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    if (sh::Half()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ 0x40);
    if (sh::Half()) SetW(at::kWait, 0);
    // the chapter compared with 0xE (Rest_*), the row pointer with it
    if (sh::Half()) {
        B(at::kChapter) = 0xE;
        sh::SetPointer(at::kFlagRow, Mem(sh::at::kCondFlags + 8 * 0xE));
    }
    // the zenny at ten times a member's level (ShopResist)
    const U level = Mem(at::kRecords + ObjTrio[(B(at::kCursor) % 3) * at::kObjStride + 0x148] * at::kRecordStride)[0xA];
    SetL(at::kZenny, PickOf(10 * level, 10 * level - 1, 10 * level + 1, 0, sh::Next() % 100000));
    // the dispatchers' steps inside their tables
    if (Is(k, "PartyForm_OpenStep") || Is(k, "SharedList_SortStep")) B(at::kStep) = static_cast<unsigned char>(sh::Next() % 3);
    if (Is(k, "PartyForm_LeaveStep")) B(at::kStep) = static_cast<unsigned char>(sh::Next() % 4);
    if (Is(k, "SharedList_MoveStep")) B(at::kStep) = static_cast<unsigned char>(sh::Next() % 5);
    // ShopResist_Grant writes the record of the cursor's member: kept to the three
    if (Is(k, "ShopResist_Grant")) B(at::kCursor) = static_cast<unsigned char>(sh::Next() % 3);
    // Equip_ChooseSlot writes the slot byte of the record its member names
    if (Is(k, "Equip_ChooseSlot"))
        for (unsigned i = 0; i < 3; ++i) B(at::kPartyList + i) = SafeId();
    // the item window's cursor against its category byte
    B(0x803360) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
}

// the arguments each helper reads
void Args(unsigned k, std::uint32_t* a) {
    const std::uint32_t hi = a[9] & 0xFFFFFF00u;
    if (Is(k, "ShopResist_Message")) {
        a[0] = at::kAnswer;
        const U index = B(at::kAnswer);
        a[1] = hi | (PickOf(index + 1, index + 2, index, sh::Next()) & 0xFF);
    } else if (Is(k, "SharedList_DrawList") || Is(k, "SharedList_DrawMember")) {
        a[0] = sh::at::kWindows + sh::at::kWindowStride * (sh::Next() % 21);
    } else if (Is(k, "SharedList_Sort")) {
        a[0] = hi | (sh::Next() % 3);
    } else if (Is(k, "ShopResist_DrawBits")) {
        a[0] = hi | PickOf(0, 1, 2, sh::Next());
        a[3] = PickOf(0, a[3] & 0xFFFFFF00u, a[3]);
        a[4] = (a[4] & 0xFFFFFF00u) | PickOf(0, 3, 7, 8, 0xFF, sh::Next());
    } else if (Is(k, "ShopResist_DrawMembers")) {
        a[0] = PickOf(0, 1, hi, a[0]);
    }
}

}  // namespace

void SelfTest() {
    g_only = std::getenv("BOF3X_FS_RUN");
    if (g_only != nullptr && *g_only != 0) {
        for (unsigned k = 0; k < kAllN; ++k)
            if (std::strcmp(kAll[k].name, g_only) == 0) {
                g_one[0] = kAll[k];
                g_clones = g_one;
                g_n = 1;
            }
        if (g_n != 1) bof3::Fatal("field_s: BOF3X_FS_RUN=%s names no function of the group", g_only);
    }
    if (FrameCallTarget() != bof3::addr::Port_DroppedCall) {
        // an address of its own (the site's), keyed by Menu_DrawFrame
        sh::Callee& frame = g_callees[kFrameRow];
        frame.name = "Menu_DrawFrame (DIV-0011)";
        frame.address = at::kFrameSite;
        frame.key = FrameCallTarget();
        for (unsigned i = 0; i < 4; ++i) frame.masks[i] = i < 2 ? 0xFFFF : 0xFF;
    }
    g_fs_reserve_copy = CopyReserve();
    sh::Group group = {
        "field_s", g_clones, g_n, g_callees, sizeof g_callees / sizeof g_callees[0], kTables, sizeof kTables / sizeof kTables[0],
        kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 4000, nullptr, 0, &Args,
    };
    group.field = true;
    group.menu_span = 3;
    sh::Run(group);
}

}  // namespace field_s
