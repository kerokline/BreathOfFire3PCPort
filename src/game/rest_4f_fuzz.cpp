// BOF3X_SHADOW=rest_4f: group R4F's 52 functions through the scenario harness in
// effect mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// sections 7 and 8), once at start-up. docs/rest_4f.md section 4.
// BOF3X_R4F_ONLY=<name> runs the clones whose name contains it, BOF3X_R4F_ROUNDS
// the rounds (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R4F --clones --harness scenario
// (2026-10-05, through the round's scratch wrapper band14.py), each extent and
// call site checked against the capstone read and the names given; 0x4648F0's
// extent ends at its tail jmp to 0x464970, which is a function of its own (a
// table cell). Shapes: the Config screen's states kMenu (0x929F02 below 5), its
// draws kCall; the effect states kEffect (Sprite_Current one of the 20
// Effect_Objects records, +5 the kind's); the effect draws with arguments kCall;
// WorldMap_ExitRecords answers a pointer (ret_mask all).
//
// Four originals are copied by this file and handed to the harness as a jump
// (rest_2h_fuzz.cpp's way): the Config draws 0x461710, 0x461970, 0x461A50 and
// 0x461AF0, whose call sites DIV-0011 (always) and DIV-0017 / DIV-0026 (under a
// Latin overlay) re-aim before this module's inject - the harness's
// CloneOriginal refuses a re-aimed site; here each site is aimed at the recorder
// for where it reaches now.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4f.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

extern "C" {
void* g_r4f_panel_copy = nullptr;
void* g_r4f_options_copy = nullptr;
void* g_r4f_pad_copy = nullptr;
void* g_r4f_padrow_copy = nullptr;
__attribute__((naked)) void R4fPanelTheirs() { asm("jmp *_g_r4f_panel_copy"); }
__attribute__((naked)) void R4fOptionsTheirs() { asm("jmp *_g_r4f_options_copy"); }
__attribute__((naked)) void R4fPadTheirs() { asm("jmp *_g_r4f_pad_copy"); }
__attribute__((naked)) void R4fPadRowTheirs() { asm("jmp *_g_r4f_padrow_copy"); }
}

namespace rest_4f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char& B(U a) { return P(a)[0]; }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
U SiteTarget(U site) { return site + 5 + static_cast<U>(Long(P(site + 1))); }

// --- band_rows.py's call sites (2026-10-05), each checked against the capstone read ---
constexpr sh::CallSite kCalls460CD0[] = {{0x2, 0x495040}};
constexpr sh::CallSite kCalls460CF0[] = {{0x6, 0x575690}, {0x1D, 0x574AB0}, {0x26, 0x461710}, {0x34, 0x4616F0}};
constexpr sh::CallSite kCalls460D50[] = {{0x15, 0x587740}, {0x23, 0x575690}, {0x4B, 0x574AB0}, {0x61, 0x461710}, {0x80, 0x4616F0}};
constexpr sh::CallSite kCalls460E10[] = {{0x10, 0x575690}, {0x27, 0x574AB0}, {0x3B, 0x497740}, {0x4B, 0x516B30}, {0x54, 0x461710},
                                         {0x62, 0x4616F0}, {0x7F, 0x5905D0}, {0x92, 0x461EB0}, {0xA9, 0x587740}, {0xCC, 0x587740},
                                         {0xF7, 0x587740}, {0x13E, 0x587740}, {0x148, 0x587740}, {0x170, 0x587740}, {0x180, 0x4616F0},
                                         {0x19A, 0x5905D0}, {0x226, 0x587740}, {0x230, 0x587740}};
constexpr sh::CallSite kCalls461070[] = {{0x7, 0x575690}, {0x1E, 0x574AB0}, {0x32, 0x497740}, {0x44, 0x516B30}, {0x4D, 0x461710},
                                         {0x5C, 0x4616F0}, {0x93, 0x587740}, {0xBF, 0x461EB0}, {0xD5, 0x587740}, {0x11D, 0x587740},
                                         {0x152, 0x587740}, {0x187, 0x587740}, {0x1BC, 0x587740}, {0x1F0, 0x587740}, {0x225, 0x587740},
                                         {0x24C, 0x587740}, {0x281, 0x587740}, {0x2A8, 0x587740}, {0x2F9, 0x587740}};
constexpr sh::JumpTable kTables461070[] = {{0x7C, 0x328, 6}};
constexpr sh::CallSite kCalls4613B0[] = {{0x6, 0x575690}, {0x1D, 0x574AB0}, {0x26, 0x461710}, {0x36, 0x4616F0}, {0x67, 0x497740},
                                         {0x78, 0x516B30}, {0x88, 0x461A50}, {0xA4, 0x5905D0}, {0xC8, 0x587740}, {0x197, 0x461EB0},
                                         {0x1AA, 0x587740}, {0x1CB, 0x587740}, {0x1E5, 0x587740}, {0x202, 0x587740}, {0x221, 0x587740},
                                         {0x247, 0x587740}};
constexpr sh::CallSite kCalls461620[] = {{0x6, 0x575690}, {0x2A, 0x574AB0}, {0x33, 0x461710}, {0x3E, 0x4616F0}, {0x45, 0x495040},
                                         {0x7A, 0x574AB0}, {0x90, 0x461710}, {0xAC, 0x4616F0}};
constexpr sh::CallSite kCalls4616F0[] = {{0x17, 0x574890}};
// this file's copies: the four Config draws (their re-aimed sites by where they reach)
constexpr sh::CallSite kCalls461710[] = {{0x68, 0x4DF820}, {0xA2, 0x461800}, {0xD1, 0x461970}};
constexpr sh::CallSite kCalls461970[] = {{0x89, 0x516B30}, {0xB9, 0x516E70}};
constexpr sh::CallSite kCalls461A50[] = {{0x34, 0x4DF820}, {0x69, 0x461AF0}, {0x81, 0x461C00}};
constexpr sh::CallSite kCalls461AF0[] = {{0x1A, 0x57CF60}, {0x53, 0x516B30}, {0x5F, 0x5A7650}, {0xB7, 0x461E50}, {0xC3, 0x5A7650}, {0xF4, 0x461E50}};
constexpr sh::CallSite kCalls462AC0[] = {{0x0, 0x462A90}};
constexpr sh::CallSite kCalls462F10[] = {{0x12, 0x5A77C0}, {0x23, 0x461E50}, {0x2F, 0x5A7710}, {0x7F, 0x5A77A0}, {0x8F, 0x5A7780}, {0xA0, 0x461E50}};
constexpr sh::CallSite kCalls462FC0[] = {{0x2, 0x462F10}};
constexpr sh::CallSite kCalls462FF0[] = {{0x2, 0x462F10}, {0x43, 0x589840}};
constexpr sh::CallSite kCalls463060[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls4630B0[] = {{0x81, 0x5720C0}, {0xC2, 0x463350}};
constexpr sh::CallSite kCalls4631B0[] = {{0x79, 0x5720C0}, {0xBA, 0x463350}, {0xDC, 0x5720C0}, {0x123, 0x5720C0}, {0x164, 0x463350}};
constexpr sh::CallSite kCalls463350[] = {{0xB, 0x5A7650}, {0x57, 0x5A8250}, {0x60, 0x5A9110}, {0xAD, 0x5A8250}, {0xB6, 0x5A9110}, {0xD8, 0x461E50}};
constexpr sh::CallSite kCalls463460[] = {{0xCD, 0x587740}};
constexpr sh::CallSite kCalls463540[] = {{0x12, 0x463D80}, {0x39, 0x587740}};
constexpr sh::CallSite kCalls463590[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x51, 0x587740}, {0x6D, 0x587740}};
constexpr sh::CallSite kCalls463610[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x6D, 0x587740}};
constexpr sh::CallSite kCalls463690[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x64, 0x463D80}, {0x8B, 0x587740}};
constexpr sh::CallSite kCalls463730[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x64, 0x463D80}, {0x83, 0x463D80},
                                         {0xAA, 0x587740}, {0xC3, 0x587740}};
constexpr sh::CallSite kCalls463810[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x64, 0x463D80}, {0x83, 0x463D80},
                                         {0xA0, 0x463D80}, {0xC6, 0x587740}, {0xDF, 0x587740}};
constexpr sh::CallSite kCalls463910[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x64, 0x463D80}, {0x83, 0x463D80},
                                         {0xA0, 0x463D80}, {0xBA, 0x463D80}, {0xE1, 0x587740}};
constexpr sh::CallSite kCalls463A10[] = {{0x12, 0x463D80}, {0x2A, 0x463D80}, {0x47, 0x463D80}, {0x64, 0x463D80}, {0x83, 0x463D80},
                                         {0xA0, 0x463D80}, {0xBA, 0x463D80}, {0xD2, 0x463D80}, {0xF9, 0x587740}, {0x112, 0x587740}};
constexpr sh::CallSite kCalls463B40[] = {{0xC, 0x463BA0}, {0x56, 0x589840}};
constexpr sh::CallSite kCalls463BA0[] = {{0x32, 0x5A7A50}, {0x40, 0x5A7A00}, {0x84, 0x5A7A50}, {0x96, 0x5A7A00}, {0xDD, 0x5A75F0},
                                         {0xE2, 0x5A7B90}, {0xEC, 0x57BFF0}, {0xF6, 0x57C070}, {0x14F, 0x5A84A0}, {0x158, 0x5A9310},
                                         {0x19D, 0x5A7780}, {0x1AE, 0x461E50}, {0x1B6, 0x5A7BC0}};
constexpr sh::CallSite kCalls463D80[] = {{0x45, 0x5A7A50}, {0x58, 0x5A7A00}, {0x9C, 0x5A75F0}, {0xA1, 0x5A7B90}, {0xAB, 0x57BFF0},
                                         {0xB5, 0x57C070}, {0xDF, 0x5A84A0}, {0xE5, 0x5A9310}, {0x10E, 0x5A7780}, {0x122, 0x461E50},
                                         {0x12A, 0x5A7BC0}};
constexpr sh::CallSite kCalls463F00[] = {{0x80, 0x4641D0}};
constexpr sh::CallSite kCalls463FA0[] = {{0x7D, 0x4641D0}, {0x84, 0x589840}};
constexpr sh::CallSite kCalls464030[] = {{0x4D, 0x4641D0}, {0x52, 0x589840}};
constexpr sh::CallSite kCalls464090[] = {{0x67, 0x578EB0}};
constexpr sh::CallSite kCalls464140[] = {{0x30, 0x494110}, {0x74, 0x4641D0}, {0x7D, 0x589840}};
constexpr sh::CallSite kCalls4641D0[] = {{0x17, 0x5A75D0}, {0x12F, 0x5A79E0}, {0x146, 0x5A79A0}, {0x152, 0x5A77A0}, {0x15A, 0x5A7780}, {0x170, 0x461E50}};
constexpr sh::CallSite kCalls464730[] = {{0x39, 0x464BA0}};
constexpr sh::CallSite kCalls464780[] = {{0x1F, 0x464BA0}};
constexpr sh::CallSite kCalls4647B0[] = {{0x49, 0x464BA0}};
constexpr sh::CallSite kCalls4648F0[] = {{0x71, 0x464970}};   // the tail jmp to EffectKind02_CopyLeader
constexpr sh::CallSite kCalls464970[] = {{0x9E, 0x5890E0}};
constexpr sh::CallSite kCalls464A40[] = {{0x9C, 0x5890E0}};
constexpr sh::CallSite kCalls464AF0[] = {{0x6A, 0x464970}};   // the tail jmp to EffectKind02_CopyLeader

#define R4F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R4F_FN(name) reinterpret_cast<const void*>(&::name)
#define R4F_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, R4F_N(calls), nullptr, 0, nullptr, 0, R4F_FN(name), ret, false, shape
#define R4F_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R4F_FN(name), ret, false, shape
#define R4F_COPY(name, shape) #name, 0, 6, nullptr, 0, nullptr, 0, nullptr, 0, R4F_FN(name), 0, false, shape
constexpr Shape kMe = Shape::kMenu;
constexpr Shape kCa = Shape::kCall;
constexpr Shape kEf = Shape::kEffect;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
sh::Clone g_clones[] = {
    {R4F_LEAF(ConfigScreen_Run, 0x460CB0, 0xE, kMe, 0)},
    {R4F_LEAF(ConfigScreen_OpenRun, 0x460CC0, 0xE, kMe, 0)},
    {R4F_CLONE(ConfigScreen_OpenFade, 0x460CD0, 0x1E, kCalls460CD0, kMe, 0)},
    {R4F_CLONE(ConfigScreen_OpenWait, 0x460CF0, 0x60, kCalls460CF0, kMe, 0)},
    {R4F_CLONE(ConfigScreen_SlideIn, 0x460D50, 0xB1, kCalls460D50, kMe, 0)},
    {R4F_CLONE(ConfigScreen_TopBar, 0x460E10, 0x253, kCalls460E10, kMe, 0)},
    {"ConfigScreen_Rows", 0x461070, 0x340, kCalls461070, R4F_N(kCalls461070), nullptr, 0, kTables461070, R4F_N(kTables461070),
     R4F_FN(ConfigScreen_Rows), 0, false, kMe},
    {R4F_CLONE(ConfigScreen_Controller, 0x4613B0, 0x263, kCalls4613B0, kMe, 0)},
    {R4F_CLONE(ConfigScreen_SlideOut, 0x461620, 0xCB, kCalls461620, kMe, 0)},
    {R4F_CLONE(ConfigScreen_DrawButtons, 0x4616F0, 0x20, kCalls4616F0, kCa, 0)},
    {R4F_COPY(Config_DrawPanel, kCa)},               // this file's copy
    {R4F_COPY(Config_DrawRowOptions, kCa)},          // this file's copy
    {R4F_COPY(Config_DrawControllerPanel, kCa)},     // this file's copy
    {R4F_COPY(Config_DrawControllerRow, kCa)},       // this file's copy
    {R4F_CLONE(WorldMap_ExitRecords, 0x462AC0, 0x1B, kCalls462AC0, kCa, kW)},
    {R4F_CLONE(EffectKind07_DrawSprite, 0x462F10, 0xAA, kCalls462F10, kCa, 0), 0, 0, 0, 7},
    {R4F_CLONE(EffectKind07_Hold, 0x462FC0, 0x2F, kCalls462FC0, kEf, 0), 0, 5, 0, 7},
    {R4F_CLONE(EffectKind07_FadeOut, 0x462FF0, 0x49, kCalls462FF0, kEf, 0), 0, 5, 0, 7},
    {R4F_CLONE(EffectKind08_Start, 0x463060, 0x4C, kCalls463060, kEf, 0), 0, 4, 0, 8},
    {R4F_CLONE(EffectKind08_TraceLine, 0x4630B0, 0xF2, kCalls4630B0, kEf, 0), 0, 4, 0, 8},
    {R4F_CLONE(EffectKind08_TraceTwo, 0x4631B0, 0x195, kCalls4631B0, kEf, 0), 0, 4, 0, 8},
    {R4F_CLONE(EffectKind08_DrawLine, 0x463350, 0xE5, kCalls463350, kCa, 0), 0, 0, 0, 8},
    {R4F_CLONE(EffectKind09_Start, 0x463460, 0xDF, kCalls463460, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open1, 0x463540, 0x4B, kCalls463540, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open2, 0x463590, 0x7F, kCalls463590, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open3, 0x463610, 0x80, kCalls463610, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open4, 0x463690, 0x9D, kCalls463690, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open5, 0x463730, 0xD5, kCalls463730, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open6, 0x463810, 0xF2, kCalls463810, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open7, 0x463910, 0xF3, kCalls463910, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Open8, 0x463A10, 0x130, kCalls463A10, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_Spin, 0x463B40, 0x5B, kCalls463B40, kEf, 0), 0, 10, 0, 9},
    {R4F_CLONE(EffectKind09_DrawBlades, 0x463BA0, 0x1DB, kCalls463BA0, kCa, 0), 0, 0, 0, 9},
    {R4F_CLONE(EffectKind09_DrawFan, 0x463D80, 0x153, kCalls463D80, kCa, 0), 0, 0, 0, 9},
    {R4F_CLONE(EffectKind0B_Start, 0x463F00, 0x92, kCalls463F00, kEf, 0), 0, 5, 0, 0xB},
    {R4F_CLONE(EffectKind0B_Rise, 0x463FA0, 0x8B, kCalls463FA0, kEf, 0), 0, 5, 0, 0xB},
    {R4F_CLONE(EffectKind0B_Drop, 0x464030, 0x57, kCalls464030, kEf, 0), 0, 5, 0, 0xB},
    {R4F_CLONE(EffectKind0B_Attach, 0x464090, 0xA8, kCalls464090, kEf, 0), 0, 5, 0, 0xB},
    {R4F_CLONE(EffectKind0B_Follow, 0x464140, 0x86, kCalls464140, kEf, 0), 0, 5, 0, 0xB},
    {R4F_CLONE(EffectKind0B_DrawQuad, 0x4641D0, 0x17A, kCalls4641D0, kCa, 0), 0, 0, 0, 0xB},
    {R4F_LEAF(EffectKind02_Start, 0x464680, 0xA9, kEf, 0), 0, 5, 0, 2},
    {R4F_CLONE(EffectKind02_SlideIn, 0x464730, 0x42, kCalls464730, kEf, 0), 0, 5, 0, 2},
    {R4F_CLONE(EffectKind02_Body, 0x464780, 0x28, kCalls464780, kEf, 0), 0, 5, 4, 2},
    {R4F_CLONE(EffectKind02_SlideOut, 0x4647B0, 0x52, kCalls4647B0, kEf, 0), 0, 5, 0, 2},
    {R4F_LEAF(EffectKind02_RunMode1, 0x464810, 0x12, kEf, 0), 0, 5, 4, 2},
    {R4F_LEAF(EffectKind02_Mode1Tint, 0x464830, 0xBB, kEf, 0), 0, 5, 4, 2},
    {R4F_CLONE(EffectKind02_Mode1Brighten, 0x4648F0, 0x76, kCalls4648F0, kEf, 0), 0, 5, 4, 2},
    {R4F_CLONE(EffectKind02_CopyLeader, 0x464970, 0xA3, kCalls464970, kEf, 0), 0, 5, 4, 2},
    {R4F_LEAF(EffectKind02_RunMode3, 0x464A20, 0x12, kEf, 0), 0, 5, 4, 2},
    {R4F_CLONE(EffectKind02_Mode3Tint, 0x464A40, 0xA1, kCalls464A40, kEf, 0), 0, 5, 4, 2},
    {R4F_CLONE(EffectKind02_Mode3Darken, 0x464AF0, 0x6F, kCalls464AF0, kEf, 0), 0, 5, 4, 2},
    {R4F_LEAF(EffectKind02_Mode3Wait, 0x464B60, 0x3F, kEf, 0), 0, 5, 4, 2},
};
#undef R4F_COPY
#undef R4F_LEAF
#undef R4F_CLONE

enum : unsigned {
    kRun, kOpenRun, kOpenFade, kOpenWait, kSlideIn, kTopBar, kRows, kController, kSlideOut, kButtons,
    kPanel, kOptions, kPad, kPadRow, kExits,
    k07Draw, k07Hold, k07Fade, k08Start, k08Trace, k08Two, k08Line,
    k09Start, k09Open1, k09Open2, k09Open3, k09Open4, k09Open5, k09Open6, k09Open7, k09Open8, k09Spin, k09Blades, k09Fan,
    k0BStart, k0BRise, k0BDrop, k0BAttach, k0BFollow, k0BQuad,
    k02Start, k02SlideIn, k02Body, k02SlideOut, k02Mode1, k02Tint1, k02Bright, k02Copy, k02Mode3, k02Tint3, k02Dark, k02Wait,
    kCount
};
static_assert(kCount == sizeof g_clones / sizeof g_clones[0], "one enum entry a clone, in order");

// --- this file's copies --------------------------------------------------------------------
// Every site re-aimed at a trampoline into the recorder that stands in for its
// callee (sh::StandIn, looked up when called), keyed by where the site reaches
// now - Capcom's callee, or what a divergence re-aimed it at.
using Fn10 = U (__cdecl*)(U, U, U, U, U, U, U, U, U, U);
constexpr unsigned kTramps = 16;
U g_tramp_key[kTramps];
unsigned g_tramp_n;
template <int I> U __cdecl Tramp(U a0, U a1, U a2, U a3, U a4, U a5, U a6, U a7, U a8, U a9) {
    return reinterpret_cast<Fn10>(const_cast<void*>(sh::StandIn(g_tramp_key[I])))(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}
template <std::size_t... I> auto MakeTramps(std::index_sequence<I...>) {
    struct T { const void* f[sizeof...(I)]; };
    return T{{reinterpret_cast<const void*>(&Tramp<I>)...}};
}
const auto kTrampFns = MakeTramps(std::make_index_sequence<kTramps>{});
const void* TrampFor(U key) {
    for (unsigned i = 0; i < g_tramp_n; ++i)
        if (g_tramp_key[i] == key) return kTrampFns.f[i];
    if (g_tramp_n == kTramps) bof3::Fatal("rest_4f: more than %u trampolines", kTramps);
    g_tramp_key[g_tramp_n] = key;
    return kTrampFns.f[g_tramp_n++];
}
void* Copy(const char* name, U base, U size, const sh::CallSite* calls, int n) {
    static bof3::CloneCall made[8];
    if (n > 8) bof3::Fatal("rest_4f: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const U reaches = SiteTarget(base + calls[i].offset);
        made[i] = {calls[i].offset, TrampFor(reaches), reaches};
    }
    return bof3::CloneOriginal(name, base, size, made, n);
}
U Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("rest_4f: the wrapper at %p is not FF 25", static_cast<const void*>(p));
    return Key(p);
}

// --- the effects ---------------------------------------------------------------------------

// The caller's stack from just below this frame to its base (effect_1a_fuzz.cpp).
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes). The
// draws here write up to 0x48 bytes past the cursor before it moves (the
// POLY_FT4), so the cursor stops 0x90 short of the end (rest_3f_fuzz.cpp's spare).
constexpr unsigned kPacketsSize = 0x800;
constexpr unsigned kSpare = 0x90;
void Advance(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + size + kSpare <= base + kPacketsSize) Gfx_PacketNext = next + size;
}
U FxCommit(const U* a, U answer) {
    Advance(a[1] & 0xFF);
    return answer;
}
// Gte_RotTransPers: the screen point as two small whole floats where the
// caller's primitive is, the depth cue's dword on the caller's stack
// (effect_1a_fuzz.cpp's).
U FxRotTransPers(const U* a, U answer) {
    unsigned char* const sxy = P(a[1]);
    if (sh::InRegions(sxy, 8))
        for (unsigned i = 0; i < 2; ++i) {
            const float f = static_cast<float>(static_cast<std::int16_t>(sh::Noise() >> 9));
            std::memcpy(sxy + 4 * i, &f, 4);
        }
    if (OnStack(P(a[2]), 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}
// Sound_PlayEffect (the standard row re-listed, louder): half the time the five
// settings the disturbance's case 4 moves, each to 0..3 from the noise (both
// passes the same) - ConfigScreen_Rows reads a setting again after its sound,
// and the group's case alone reached that 3 times in 60,000 rounds (control
// C21, docs/rest_4f.md section 8).
U FxSound(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2 == 0) {
        static const U kCells[] = {at::kSetting0, at::kSetting3, at::kStyle, at::kBackdrop, at::kSetting4};
        for (unsigned i = 0; i < 5; ++i) B(kCells[i]) = static_cast<unsigned char>((n >> (2 + 2 * i)) & 3);
    }
    return answer;
}

#define R4F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
sh::Callee g_callees[] = {
    // the group's own called directly, by name, each mask what the callee reads
    // (docs/rest_4f.md section 4): coordinates go on to word readers (Menu_DrawBox,
    // Text_DrawAt, the line's & 0xFFFF), so their high halves - a caller's
    // leftover in the original - are not compared
    {R4F_OURS(ConfigScreen_DrawButtons), 3, {kU8, kU16, kU16}, kG, 0, 0},
    {R4F_OURS(Config_DrawPanel), 2, {kU16, kU16}, kG, 0, 0},
    {R4F_OURS(Config_DrawRowOptions), 5, {kU16, kU16, kU8, kU8, kW}, kG, 0, 0},   // row & 0xFF, the setting movzx byte, state a dword
    {R4F_OURS(Config_DrawControllerPanel), 2, {kU16, kU16}, kG, 0, 0},
    {R4F_OURS(Config_DrawControllerRow), 3, {kU16, kU16, kU8}, kG, 0, 0},           // row & 0xFF
    {R4F_OURS(EffectKind07_DrawSprite), 1, {kU8}, kG, 0, 0},                       // and eax, 0xFF
    // the heights' high words only (movsx word [high half]); the colour's three bytes
    {R4F_OURS(EffectKind08_DrawLine), 7, {kW, kW, 0xFFFF0000u, kW, kW, 0xFFFF0000u, 0x00FFFFFFu}, kG, 0, 0},
    {R4F_OURS(EffectKind09_DrawFan), 4, {kW, kW, kW, kW}, kG, 0, 0},
    {R4F_OURS(EffectKind09_DrawBlades), 0, {}, kPh, 0, 0},
    {R4F_OURS(EffectKind0B_DrawQuad), 0, {}, kPh, 0, 0},
    {R4F_OURS(EffectKind02_CopyLeader), 0, {}, kPh, 0, 0},
    // ours, other groups', the standard set lacks
    {R4F_OURS(Menu_DrawButtonRow), 5, {kU16, kU16, kU8, kU8, 0}, kG, 0, 0},       // menu_windows.cpp: the fifth unused
    {R4F_OURS(Config_DrawRowLabel), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},           // field_c1.cpp: row & 0xFF, state & 0xFF
    {R4F_OURS(Config_DrawControllerCell), 3, {kU16, kU16, kU8}, kG, 0, 0},          // config_text.cpp: bits 0x04..0x80 tested
    {R4F_OURS(EffectHud_Draw), 2, {kU16, kU16}, kG, 0, 0},                          // effect_1a.cpp: movsx words
    // standard rows re-listed: the cursor's spare for the commits; the vertex
    // and the depth cue on the callers' stacks (the vertex's x, y, z hashed, not
    // its fourth word: DIV-0023, the original's stale stack, ours 0)
    {R4F_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {0, 0}, &FxCommit, nullptr, true},
    {R4F_OURS(Gte_RotTransPers), 3, {0, kW, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTransPers, nullptr, true},
    // the sound's id a word (the standard row's mask); a setting moved under it (FxSound)
    {R4F_OURS(Sound_PlayEffect), 1, {kU16}, kG, 0, 0, {}, &FxSound},
    // the Config titles: the text hashed to its NUL (the options' strings and the
    // names are the image's; the system messages the harness's text buffer)
    {R4F_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kW}, kG, 0, 0, {0, 0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
    // DIV-0011's Menu_DrawFrame, DIV-0017 / DIV-0026's ConfigText_DrawSelected:
    // keyed at start-up by where their sites reach (x, y s16, w, h bytes;
    // Text_DrawAt's), each without its divergence a second row of an address
    // listed above (skipped)
    {R4F_OURS(Port_DroppedCall), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    {R4F_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kW}, kG, 0, 0, {0, 0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
};
constexpr unsigned kFrameRow = sizeof g_callees / sizeof g_callees[0] - 2;
constexpr unsigned kSelectedRow = sizeof g_callees / sizeof g_callees[0] - 1;
#undef R4F_OURS

// The tables this group's dispatchers read in place (lengths: symbols.toml
// [[data]], each checked by hand).
const sh::DataTable kTables[] = {
    {at::kConfigStates, at::kConfigStateCount}, {at::kConfigOpenStates, at::kConfigOpenCount},
    {at::kKind02Modes, at::kKind02ModeCount},   {at::kKind02Mode1, at::kKind02Mode1Count},
    {at::kKind02Mode3, at::kKind02Mode3Count},
};

const std::uint8_t kKinds[] = {2, 7, 8, 9, 0xB};

// Beyond the standard regions: the button words 0 and 1 (0x903580; the rest are
// the effect mode's kMenuButtons), palette 0x7B in the CLUT shadow.
const sh::Region kRegions[] = {
    {at::kButtons, 4},
    {at::kClut7B, at::kClutBytes},
};

// --- the seed ------------------------------------------------------------------------------

unsigned g_current = 0;   // the clone being fuzzed (Seed sets it; Disturb reads it)

bool IsKind8Trace(unsigned k) { return k == k08Trace || k == k08Two; }
unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
// The cursor in the range function k keeps it in, mostly, from v alone (the
// disturbance draws only from its hash).
unsigned char CursorFor(unsigned k, U v) {
    static const unsigned char kTopEdges[] = {0, 1, 2, 7, 0xFF};
    static const unsigned char kRowEdges[] = {2, 7, 1, 8, 0xFF, 0x80};
    switch (k) {
    case kTopBar: return (v & 1) ? static_cast<unsigned char>((v >> 1) & 1) : kTopEdges[(v >> 1) % sizeof kTopEdges];
    case kRows: return (v & 1) ? static_cast<unsigned char>(2 + (v >> 1) % 6) : kRowEdges[(v >> 1) % sizeof kRowEdges];
    case kController: return static_cast<unsigned char>(8 + v % 6);
    default: return static_cast<unsigned char>(v);
    }
}
void SetFloat(unsigned char* p, float f) { std::memcpy(p, &f, 4); }

// Kind 8's object (+6, below 30) and the record's z near it, so that the walk is
// a few steps either way, and the dark line's start 0x138000 a few steps above
// the object's z most of the time.
void SeedTrace(unsigned char* e) {
    const unsigned idx = sh::Next() % at::kObjectCount;
    e[6] = static_cast<unsigned char>(idx);
    unsigned char* const o = Sprite_Objects + idx * 0xA4u;
    const U target = sh::Often() ? 0x138000u - PickOf(0, 0x8000, 0x7FFF, 0x8001, 0x10000, sh::Next() % 0xA0000)
                                 : static_cast<U>(sh::Next() % 0x200000);
    SetLong(o + 0x38, static_cast<std::int32_t>(target));
    const U z = sh::Often() ? target - PickOf(0, 0x8000, 0x7FFF, 0x8001, 1, sh::Next() % 0xA0000)
                            : target + PickOf(1, 0x8000, sh::Next() % 0x40000);
    SetLong(e + 0x38, static_cast<std::int32_t>(z));
}

void Seed(unsigned k) {
    g_current = k;
    // every record the disturbance may make current
    for (unsigned r = 0; r < sh::at::kEffectCount; ++r) {
        unsigned char* const e = Rec(r);
        e[6] = static_cast<unsigned char>(sh::Next() % at::kObjectCount);
        if (IsKind8Trace(k)) SeedTrace(e);
        e[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 0x78, 0xFF, sh::Next()));
        if (sh::Half()) e[0x5D] = static_cast<unsigned char>(PickOf(0, 2, 4, 0x80, 0x90, 0xB0, 0xC0, 0x7F, 0x10));
    }
    unsigned char* const s = Sprite_Current;
    // the menu block and the settings
    B(at::kOpenState) = static_cast<unsigned char>(sh::Next() % at::kConfigOpenCount);
    B(at::kCounter) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, sh::Next()));
    B(at::kCursor) = CursorFor(k, sh::Next());
    if (sh::Half()) {
        static const U kKeys[] = {0, 0x1000, 0x4000, 0xA000, 0x2000, 0x8000, 0x800, 0x20, 0x40, 0x10, 0x80, 0x24, 0xFC};
        Input_Pressed = static_cast<unsigned short>(kKeys[sh::Next() % (sizeof kKeys / sizeof kKeys[0])] | (sh::Half() ? 0 : sh::Next() & 0xFF));
    }
    if (sh::Half()) Field_ConfirmButtons = static_cast<unsigned short>(PickOf(0, 0x20, 0x60, 0xA000, sh::Next()));
    if (sh::Half()) Field_CancelButtons = static_cast<unsigned short>(PickOf(0, 0x40, 0x800, sh::Next()));
    if (sh::Half()) Game_Step = static_cast<unsigned short>(PickOf(4, 5, 4, 2));
    if (sh::Half()) MoveScript_WaitWordDA = 0;
    for (U cell = at::kSetting0; cell <= at::kSetting4; ++cell)
        if (sh::Often()) B(cell) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0x7F, 0x80, 0xFF));
    switch (k) {
    case k07Hold:
    case k07Fade: s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next())); break;
    case k08Start:
    case k08Line: break;
    case k08Trace:
    case k08Two: B(at::kChapterByte4A) = static_cast<unsigned char>(PickOf(0x32, 0x34, sh::Next())); break;
    case k09Open1: SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(0x58, 0x50, 0x60, sh::Next()))); break;
    case k09Open2: SetLong(s + 0x1C, static_cast<std::int32_t>(PickOf(0x10, 0x90, 0x20, 0xA0, sh::Next()))); break;
    case k09Open3: s[0xB] = static_cast<unsigned char>(PickOf(0x50, 0x40, sh::Next())); break;
    case k09Open4: s[8] = static_cast<unsigned char>(PickOf(0x58, 0x50, sh::Next())); break;
    case k09Open5: s[0xA] = static_cast<unsigned char>(PickOf(0x8, 0x58, 0x10, sh::Next())); break;
    case k09Open6: s[6] = static_cast<unsigned char>(PickOf(0x8, 0x58, 0x10, sh::Next())); break;
    case k09Open7: s[7] = static_cast<unsigned char>(PickOf(0x58, 0x50, sh::Next())); break;
    case k09Open8:
        SetLong(s + 0x20, static_cast<std::int32_t>(PickOf(0x8, 0x58, 0x10, sh::Next())));
        B(at::kChapterCount) = static_cast<unsigned char>(sh::Next());
        break;
    case k09Spin:
    case k09Blades:
        s[9] = static_cast<unsigned char>(PickOf(0, 1, 0x78, sh::Next()));
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0x7C, 0x7F, 0x80, 0x81, 0, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0xB4, 0xB5, 0xB6, 0xB7, 0xFFFFFFFFu, sh::Next())));
        break;
    case k0BStart:
    case k0BRise:
    case k0BDrop:
    case k0BFollow:
    case k0BQuad:
        if (sh::Often()) {
            SetFloat(s + 0x74, static_cast<float>(static_cast<std::int16_t>(sh::Next())) / 4.0f);
            SetFloat(s + 0x78, static_cast<float>(static_cast<std::int16_t>(sh::Next())) / 8.0f);
            SetFloat(s + 0x7C, 0.01f);
        }
        s[0x5D] = static_cast<unsigned char>(PickOf(0, 2, 4, 0x80, sh::Next()));
        s[9] = static_cast<unsigned char>(PickOf(0, 3, 4, 7, 8, sh::Next() % 0x40, sh::Next()));
        Draw_PassFlags = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
        break;
    case k02SlideIn:
    case k02SlideOut:
    case k02Body:
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0xB6, 0xB5, 0xB7, 0xAE, 0xE8, 0xE9, 0xF0, sh::Next())));
        break;
    case k02Mode1: s[3] = static_cast<unsigned char>(sh::Next() % at::kKind02Mode1Count); break;
    case k02Mode3: s[3] = static_cast<unsigned char>(sh::Next() % at::kKind02Mode3Count); break;
    case k02Tint1:
    case k02Bright:
    case k02Copy:
    case k02Tint3:
    case k02Dark:
    case k02Wait:
        SetWord(P(at::kKind02Word), PickOf(0x37, 0x38, 0x39, 0x8000, 0x7FFF, sh::Next()));
        B(at::kLeader + 2) = static_cast<unsigned char>(PickOf(1, 0, 2, sh::Next()));
        if (k == k02Bright) s[0x5D] = static_cast<unsigned char>(PickOf(0xB0, 0xA0, 0x70, 0x6F, sh::Next()));
        if (k == k02Dark) s[0x5D] = static_cast<unsigned char>(PickOf(0x90, 0x80, 0xA0, sh::Next()));
        if (k == k02Tint1 || k == k02Tint3) B(at::kLeader + 0x27) = static_cast<unsigned char>(PickOf(0x7B, 0x7A, sh::Next()));
        break;
    default: break;
    }
}

// The draws' arguments: the coordinates small over leftover upper bytes, the
// rows and settings in their ranges (a controller row past six aborts ours,
// docs/rest_4f.md section 7), the state 3 half the time.
void Args(unsigned k, U* a) {
    switch (k) {
    case kButtons:
        a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0xFF, 0, 1, sh::Next() & 0xFF);
        a[1] = PickOf(0x70, sh::Next() & 0xFF);
        a[2] = PickOf(0x26, 0x16, 0xFFFFFFF6u, sh::Next() & 0xFF);
        break;
    case kPanel:
    case kPad:
        a[0] = PickOf(0x1C, 0x5C, 0xA8, 0x15C, sh::Next() & 0x1FF);
        a[1] = PickOf(0x40, 0x62, sh::Next() & 0xFF);
        break;
    case kOptions:
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0x21, 0x61, sh::Next() & 0x1FF);
        a[1] = PickOf(0x49, 0x58, sh::Next() & 0xFF);
        a[2] = (sh::Next() & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 6 : sh::Next() % 8);
        a[3] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 4, sh::Next() & 0xFF);
        a[4] = PickOf(3, 3, 0, 1, 2, 0x103, sh::Next());
        break;
    case kPadRow:
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0xAD, sh::Next() & 0x1FF);
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0x68, sh::Next() & 0xFF);
        a[2] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kPadNameCount);
        break;
    case k07Draw: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(1, 0, 2, 3, sh::Next() & 0xFF); break;
    case k08Line:
        a[6] = (sh::Next() & 0xFF000000u) | PickOf(0xC0C0C0, 0x303030, sh::Next() & 0xFFFFFF);
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the current record's +9, one of
// its bytes the states and draws read again (+0x5D..+0x5F, +6..+8, +0xA, +0xB),
// one of its dwords (+0xC, +0x10, +0x18, +0x1C, +0x20, +0x64, +0x68, +0x6C), the
// Config screen's counter or cursor (the cursor in the range the function keeps
// it in), a setting, the chapter's byte 0x90384A, Input_Pressed.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    const bool in = sh::InRegions(s, 0x80);
    switch (sh::DisturbCase(h, 7)) {
    case 0:
        if (in) s[9] = static_cast<unsigned char>(v);
        break;
    case 1: {
        static const unsigned char kOffsets[] = {0x5D, 0x5E, 0x5F, 6, 7, 8, 0xA, 0xB};
        if (in) s[kOffsets[v & 7]] = static_cast<unsigned char>(v >> 3);
        break;
    }
    case 2: {
        static const unsigned char kOffsets[] = {0xC, 0x10, 0x18, 0x1C, 0x20, 0x64, 0x68, 0x6C};
        static const U kValues[] = {0x10, 0x60, 0x80, 0xB6, 0x20, 0xA0, 0x7C, 0};
        if (in) SetLong(s + kOffsets[v & 7], static_cast<std::int32_t>((v & 8) ? kValues[(v >> 4) & 7] : v >> 7));
        break;
    }
    case 3:
        if (v & 1) B(at::kCounter) = static_cast<unsigned char>((v >> 1) % 6);
        else B(at::kCursor) = CursorFor(g_current, v >> 1);
        break;
    case 4: {
        static const U kCells[] = {at::kSetting0, at::kSetting3, at::kStyle, at::kBackdrop, at::kSetting4};
        B(kCells[v % 5]) = static_cast<unsigned char>((v >> 3) & 3);
        break;
    }
    case 5: B(at::kChapterByte4A) = static_cast<unsigned char>((v & 1) ? ((v & 2) ? 0x32u : 0x34u) : v >> 2); break;
    default: Input_Pressed = static_cast<unsigned short>(v); break;
    }
}

}  // namespace

void SelfTest() {
    // this file's copies, each site aimed where it reaches now
    g_r4f_panel_copy = Copy("Config_DrawPanel", 0x461710, 0xEF, kCalls461710, R4F_N(kCalls461710));
    g_r4f_options_copy = Copy("Config_DrawRowOptions", 0x461970, 0xE0, kCalls461970, R4F_N(kCalls461970));
    g_r4f_pad_copy = Copy("Config_DrawControllerPanel", 0x461A50, 0x9D, kCalls461A50, R4F_N(kCalls461A50));
    g_r4f_padrow_copy = Copy("Config_DrawControllerRow", 0x461AF0, 0x101, kCalls461AF0, R4F_N(kCalls461AF0));
    g_clones[kPanel].base = Wrapper(&R4fPanelTheirs);
    g_clones[kOptions].base = Wrapper(&R4fOptionsTheirs);
    g_clones[kPad].base = Wrapper(&R4fPadTheirs);
    g_clones[kPadRow].base = Wrapper(&R4fPadRowTheirs);
    // the divergences' sites: rows of their own when they reach ours
    const U frame = SiteTarget(at::kPanelFrameSite);
    if (SiteTarget(at::kPadFrameSite) != frame)
        bof3::Fatal("rest_4f: the two frame sites reach 0x%X and 0x%X", (unsigned)frame, (unsigned)SiteTarget(at::kPadFrameSite));
    if (frame != bof3::addr::Port_DroppedCall) {
        sh::Callee& row = g_callees[kFrameRow];
        row.name = "Menu_DrawFrame (DIV-0011)";
        row.address = at::kPanelFrameSite;
        row.key = frame;
    }
    const U selected = SiteTarget(at::kOptionBigSite);
    if (SiteTarget(at::kPadNameSite) != selected)
        bof3::Fatal("rest_4f: the two text sites reach 0x%X and 0x%X", (unsigned)selected, (unsigned)SiteTarget(at::kPadNameSite));
    if (selected != bof3::addr::Text_DrawAt) {
        sh::Callee& row = g_callees[kSelectedRow];
        row.name = "ConfigText_DrawSelected (DIV-0017 / DIV-0026)";
        row.address = at::kOptionBigSite;
        row.key = selected;
    }

    // BOF3X_R4F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R4F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(g_clones[k].name, only)) {
            index[n] = k;
            chosen[n++] = g_clones[k];
        }
    if (n == 0) bof3::Fatal("rest_4f: BOF3X_R4F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_4f", chosen, n, g_callees, sizeof g_callees / sizeof g_callees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    if (const char* r = std::getenv("BOF3X_R4F_ROUNDS")) g.rounds = static_cast<unsigned>(std::atoi(r));
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    g.menu_span = at::kConfigStateCount;
    sh::Run(g);
}

}  // namespace rest_4f
