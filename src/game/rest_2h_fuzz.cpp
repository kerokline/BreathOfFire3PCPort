// BOF3X_SHADOW=rest_2h: group R2H's 36 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// sections 7 and 8), once at start-up. docs/rest_2h.md section 4.
// BOF3X_R2H_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R2H --clones --harness scenario
// (2026-10-04) with the names given and 0x59E160 added; every extent and call
// site agrees with the capstone read. The kinds' runs and slides are kState
// (void, the window record the dword at 0x905B84), the draws kCall with the
// window record (or the reserve list's object) as a[0].
//
// Four originals are copied by this file and handed to the harness as a jump
// (field_s_fuzz.cpp's way): 0x59AA80 and 0x59DBF0, whose call sites DIV-0011
// and DIV-0059 re-aim before this module's inject (the harness's CloneOriginal
// refuses a re-aimed site; here each is aimed at the recorder for where it
// reaches now); 0x59C130, whose bound immediate the seed moves in the copy and
// ours is aimed at; and 0x5A9620, a stdcall callback, reached through a cdecl
// adaptor.
#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

extern "C" {
void* g_r2h_reserve_copy = nullptr;
void* g_r2h_items_copy = nullptr;
void* g_r2h_slide_copy = nullptr;
void* g_r2h_enum_copy = nullptr;
__attribute__((naked)) void R2hReserveTheirs() { asm("jmp *_g_r2h_reserve_copy"); }
__attribute__((naked)) void R2hItemsTheirs() { asm("jmp *_g_r2h_items_copy"); }
__attribute__((naked)) void R2hSlideTheirs() { asm("jmp *_g_r2h_slide_copy"); }
// cdecl (instance, context) -> the stdcall copy, which pops its two words.
__attribute__((naked)) void R2hEnumTheirs() {
    asm("pushl 8(%esp)\n\t"
        "pushl 8(%esp)\n\t"
        "call *_g_r2h_enum_copy\n\t"
        "ret");
}
}

namespace rest_2h {
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
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
U SiteTarget(U site) { return site + 5 + static_cast<U>(Long(Mem(site + 1))); }

// --- band_rows.py's call sites (2026-10-04), each checked against the capstone read ---
constexpr sh::CallSite kCalls59AA80[] = {{0x18, 0x4DF820}, {0xB1, 0x57CF60}, {0xDB, 0x573F30}, {0xF0, 0x516B30}, {0x105, 0x574400}, {0x11A, 0x5B9380}, {0x131, 0x517090}, {0x1B8, 0x516E70}, {0x1D0, 0x574400}, {0x217, 0x5B9380}, {0x228, 0x517090}, {0x25E, 0x5B9380}, {0x273, 0x517090}, {0x288, 0x574400}, {0x2DB, 0x5B9380}, {0x2F3, 0x517090}, {0x309, 0x5B9380}, {0x31D, 0x517090}};
constexpr sh::CallSite kCalls59ADE0[] = {{0x19, 0x59AE00}};
constexpr sh::CallSite kCalls59AE00[] = {{0x96, 0x57CF60}, {0xB1, 0x57DF00}, {0x11C, 0x57D360}, {0x12F, 0x497740}, {0x146, 0x516B30}, {0x168, 0x57D360}, {0x17B, 0x497740}, {0x192, 0x516B30}, {0x20E, 0x57CF60}, {0x235, 0x57CF60}, {0x254, 0x57D800}, {0x272, 0x516B30}, {0x288, 0x57D910}, {0x29E, 0x57D910}, {0x2C3, 0x57D860}, {0x2EE, 0x57D860}, {0x314, 0x57D860}, {0x33D, 0x57D860}, {0x363, 0x57D860}, {0x37B, 0x57D860}, {0x3A4, 0x57D860}, {0x3C5, 0x57D860}, {0x3DD, 0x57D860}, {0x406, 0x57DD10}};
constexpr sh::CallSite kCalls59BE50[] = {{0x19, 0x585090}};
constexpr sh::CallSite kCalls59BEA0[] = {{0x19, 0x585500}};
constexpr sh::CallSite kCalls59BEC0[] = {{0x29, 0x573560}};
constexpr sh::CallSite kCalls59BF00[] = {{0x6, 0x59BF10}};
constexpr sh::CallSite kCalls59BF10[] = {{0x32, 0x57CF60}, {0x57, 0x516B30}, {0x6D, 0x57D910}, {0xD2, 0x516B30}, {0xF7, 0x516B30}, {0x130, 0x516B30}, {0x155, 0x516B30}, {0x17E, 0x516B30}, {0x19D, 0x57D910}, {0x1B9, 0x57D910}, {0x1F2, 0x57D910}};
constexpr sh::CallSite kCalls59C110[] = {{0x19, 0x59C2C0}};
constexpr sh::CallSite kCalls59C190[] = {{0x19, 0x59C870}};
constexpr sh::CallSite kCalls59C1E0[] = {{0x19, 0x59C8F0}};
constexpr sh::CallSite kCalls59C260[] = {{0x21, 0x585940}};
constexpr sh::CallSite kCalls59C2C0[] = {{0xC2, 0x57CF60}, {0xDD, 0x57DF00}, {0x13B, 0x59C780}, {0x161, 0x497740}, {0x178, 0x516B30}, {0x196, 0x57D360}, {0x1B6, 0x516B30}, {0x1EE, 0x497740}, {0x204, 0x516B30}, {0x226, 0x57D360}, {0x245, 0x516B30}, {0x2C1, 0x57CF60}, {0x2E5, 0x57CF60}, {0x304, 0x57D800}, {0x320, 0x516B30}, {0x336, 0x57D910}, {0x34C, 0x57D910}, {0x370, 0x57D860}, {0x39B, 0x57D860}, {0x3C0, 0x57D860}, {0x3E8, 0x57D860}, {0x40D, 0x57D860}, {0x425, 0x57D860}, {0x44E, 0x57D860}, {0x46F, 0x57D860}, {0x486, 0x57D860}, {0x4AF, 0x57DD10}};
constexpr sh::CallSite kCalls59C780[] = {{0x2D, 0x57C140}, {0x61, 0x59C810}};
constexpr sh::JumpTable kTables59C780[] = {{0x15, 0x7C, 4}};
constexpr sh::CallSite kCalls59C870[] = {{0x26, 0x57CF60}, {0x39, 0x5762D0}, {0x4A, 0x497740}, {0x69, 0x516B30}};
constexpr sh::CallSite kCalls59C8F0[] = {{0x26, 0x57CF60}, {0x6E, 0x59CA00}, {0xA3, 0x5762D0}, {0xC6, 0x57CF60}, {0xDD, 0x5762D0}, {0x100, 0x516B30}};
constexpr sh::CallSite kCalls59CA00[] = {{0x36, 0x5A77C0}, {0x3F, 0x461E50}, {0x4B, 0x5A7710}, {0xED, 0x461E50}};
constexpr sh::CallSite kCalls59CC10[] = {{0x3E, 0x59D640}};
constexpr sh::CallSite kCalls59CC90[] = {{0x19, 0x59DBF0}};
constexpr sh::CallSite kCalls59CCE0[] = {{0x13, 0x59E160}};
constexpr sh::CallSite kCalls59DB70[] = {{0x8, 0x5A7720}, {0x63, 0x5A7780}, {0x6C, 0x461E50}};
constexpr sh::CallSite kCalls59DBF0[] = {{0x70, 0x57D9A0}, {0x87, 0x591720}, {0x154, 0x57CF60}, {0x16F, 0x57DF00}, {0x1FB, 0x57DBF0}, {0x21C, 0x57DBF0}, {0x23D, 0x57DBF0}, {0x297, 0x57CF60}, {0x2BD, 0x57CF60}, {0x2EC, 0x57D800}, {0x30A, 0x516B30}, {0x328, 0x5B9380}, {0x346, 0x517090}, {0x35C, 0x57D910}, {0x375, 0x57D910}, {0x399, 0x57D860}, {0x3C4, 0x57D860}, {0x3EA, 0x57D860}, {0x413, 0x57D860}, {0x439, 0x57D860}, {0x450, 0x57D860}, {0x478, 0x57D860}, {0x49D, 0x57D860}, {0x4C5, 0x57D860}, {0x4EB, 0x57D860}, {0x502, 0x57D860}, {0x51A, 0x57D860}, {0x544, 0x57DD10}};
constexpr move_script::Table kTable59DBF0 = {0x19, 0x554, 5};
// 0x59E160 (catalogued as the renderer's; read 2026-10-04)
constexpr sh::CallSite kCalls59E160[] = {{0x1A, 0x57CF60}, {0x48, 0x516B30}, {0x63, 0x57D910}, {0x7A, 0x57CF60}, {0xA2, 0x516B30}, {0xBE, 0x57D910}};
constexpr sh::CallSite kCalls5A9620[] = {{0x48, at::kStricmp}};

// --- this file's copies ------------------------------------------------------------------
// Every site re-aimed at a trampoline into the recorder that stands in for its
// callee (sh::StandIn, looked up when called: the same log entry, disturbance
// and answer as a site the harness re-aims), keyed by the callee's address -
// or, for a site a divergence re-aims, by where it reaches now.
using Fn10 = U (__cdecl*)(U, U, U, U, U, U, U, U, U, U);
constexpr unsigned kTramps = 24;
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
    if (g_tramp_n == kTramps) bof3::Fatal("rest_2h: more than %u trampolines", kTramps);
    g_tramp_key[g_tramp_n] = key;
    return kTrampFns.f[g_tramp_n++];
}

// `site`: the one call site a divergence may re-aim (0 for none).
void* Copy(const char* name, U base, U size, const sh::CallSite* calls, int n, U site) {
    static bof3::CloneCall made[32];
    if (n > 32) bof3::Fatal("rest_2h: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const bool moved = site != 0 && base + calls[i].offset == site;
        const U reaches = moved ? SiteTarget(site) : calls[i].target;
        made[i] = {calls[i].offset, TrampFor(reaches), reaches};
    }
    return bof3::CloneOriginal(name, base, size, made, n);
}

U Wrapper(void (*f)(), unsigned char first) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != first) bof3::Fatal("rest_2h: the wrapper at %p is not FF %02X", static_cast<const void*>(p), first);
    return Key(p);
}

// cdecl into the stdcall callback (ours).
extern "C" U __cdecl R2hEnumOurs(U instance, U context) {
    return static_cast<U>(::DInput_EnumJoystick(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(instance)),
                                                reinterpret_cast<void*>(static_cast<std::uintptr_t>(context))));
}

// --- the clones -------------------------------------------------------------------------
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
sh::Clone g_clones[] = {
    {"MenuList_ReserveWinDraw", 0, 6, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(MenuList_ReserveWinDraw), 0, false, kCa},   // this file's copy
    {G_CLONE(MenuList_GeneWinRun, 0x59ADE0, 0x20, kCalls59ADE0, kSt, 0)},
    {G_CLONE(MenuList_GeneWinDraw, 0x59AE00, 0x415, kCalls59AE00, kCa, 0)},
    {G_CLONE(ShopWin_SharedListRun, 0x59BE50, 0x20, kCalls59BE50, kSt, 0)},
    {G_LEAF(ShopWin_SharedListSlideTo28, 0x59BE70, 0x28, kSt, 0)},
    {G_CLONE(ShopWin_SharedMemberRun, 0x59BEA0, 0x20, kCalls59BEA0, kSt, 0)},
    {G_CLONE(ShopWin_MemberStatusRun, 0x59BEC0, 0x32, kCalls59BEC0, kSt, 0)},
    {G_CLONE(ShopWin_RowMenuRun, 0x59BF00, 0xD, kCalls59BF00, kSt, 0)},
    {G_CLONE(ShopWin_DrawRowMenu, 0x59BF10, 0x1FE, kCalls59BF10, kCa, 0)},
    {G_CLONE(ShopWin_MasterListRun, 0x59C110, 0x20, kCalls59C110, kSt, 0)},
    {"MasterWin_SlideOut", 0, 6, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(MasterWin_SlideOut), 0, false, kSt},   // this file's copy
    {G_LEAF(MasterWin_SlideTo64, 0x59C160, 0x28, kSt, 0)},
    {G_CLONE(ShopWin_MasterCaptionRun, 0x59C190, 0x20, kCalls59C190, kSt, 0)},
    {G_LEAF(MasterWin_CaptionSlideTo8C, 0x59C1B0, 0x28, kSt, 0)},
    {G_CLONE(ShopWin_PupilsRun, 0x59C1E0, 0x20, kCalls59C1E0, kSt, 0)},
    {G_LEAF(MasterWin_PupilsSlideDown, 0x59C200, 0x28, kSt, 0)},
    {G_LEAF(MasterWin_PupilsSlideUp, 0x59C230, 0x28, kSt, 0)},
    {G_CLONE(ShopWin_ItemCountRun, 0x59C260, 0x2A, kCalls59C260, kSt, 0)},
    {G_LEAF(ShopWin_ItemCountSlideToD2, 0x59C290, 0x28, kSt, 0)},
    {G_CLONE(MasterWin_DrawList, 0x59C2C0, 0x4BE, kCalls59C2C0, kCa, 0)},
    {"MasterWin_Available", 0x59C780, 0x8C, kCalls59C780, SH_N(kCalls59C780), nullptr, 0, kTables59C780, SH_N(kTables59C780), G_FN(MasterWin_Available), 0xFF, false, kCa},
    {G_LEAF(MasterWin_SkillKnown, 0x59C810, 0x58, kCa, 0xFF)},
    {G_CLONE(MasterWin_DrawCaption, 0x59C870, 0x73, kCalls59C870, kCa, 0)},
    {G_CLONE(MasterWin_DrawPupils, 0x59C8F0, 0x10D, kCalls59C8F0, kCa, 0)},
    {G_CLONE(MasterWin_DrawPortrait, 0x59CA00, 0xF8, kCalls59CA00, kCa, 0)},
    {G_LEAF(BattleMenuWin_ItemListSlideLeft, 0x59CB90, 0x26, kSt, 0)},
    {G_CLONE(BattleMenuWin_EquipRun, 0x59CC10, 0x47, kCalls59CC10, kSt, 0)},
    {G_LEAF(BattleMenuWin_EquipSlideTo62, 0x59CC60, 0x28, kSt, 0)},
    {G_CLONE(BattleMenuWin_EquipItemsRun, 0x59CC90, 0x20, kCalls59CC90, kSt, 0)},
    {G_LEAF(BattleMenuWin_EquipItemsSlideTo98, 0x59CCB0, 0x28, kSt, 0)},
    {G_CLONE(BattleMenuWin_VerbPairRun, 0x59CCE0, 0x1C, kCalls59CCE0, kSt, 0)},
    {G_CLONE(BattleEquipWin_DrawBar, 0x59DB70, 0x76, kCalls59DB70, kCa, 0)},
    {"BattleEquipWin_DrawItems", 0, 6, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(BattleEquipWin_DrawItems), 0, false, kCa},   // this file's copy
    {G_CLONE(Menu_DrawVerbPair, 0x59E160, 0xCB, kCalls59E160, kCa, 0)},
    {"DInput_EnumJoystick", 0, 15, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&R2hEnumOurs), 0xFFFFFFFFu, false, kCa},   // this file's copy, a cdecl adaptor
    {G_LEAF(Cfg_SetKeyTable, 0x5A9860, 0x15, kCa, 0)},
};
#undef G_LEAF
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kReserve, kGeneRun, kGeneDraw, kSharedRun, kSharedSlide, kMemberRun, kStatusRun, kRowRun, kRowDraw, kMasterRun,
    kMasterOut, kMasterIn, kCaptionRun, kCaptionSlide, kPupilsRun, kPupilsDown, kPupilsUp, kCountRun, kCountSlide,
    kMasterDraw, kAvailable, kSkill, kCaptionDraw, kPupilsDraw, kPortrait, kItemLeft, kEquipRun, kEquipSlide, kItemsRun,
    kItemsSlide, kVerbRun, kBar, kItemsDraw, kVerbPair, kEnum, kKeys, kCount
};
static_assert(kCount == sizeof g_clones / sizeof g_clones[0], "one enum entry a clone, in order");

// --- the stand-ins' effects (Noise() and the state only: both passes the same) ----------

// Crt_sprintf: up to seven letters and a NUL into the buffer, as the real one
// writes the figure (Text_DrawFont8 hashes the string after it).
U SprintfEffect(const U* a, U answer) {
    auto* const out = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(out, 8)) return answer;
    const U n = sh::Noise();
    const unsigned len = n % 8;
    for (unsigned i = 0; i < len; ++i) out[i] = static_cast<unsigned char>('0' + (n >> (3 + i)) % 10);
    out[len] = 0;
    return answer;
}

// The joystick enumeration's two COM calls, through fake objects: stdcall
// methods that log what they are handed (sh::Record under the call site's
// address) and answer from the log; CreateDevice writes the device (one of two
// fakes) on success, QueryInterface a noise pointer.
constexpr U kCreateSite = 0x5A9638, kQuerySite = 0x5A965A;
unsigned char g_com[0x40];   // +0 the IDirectInput, +0x10 / +0x18 two devices (constant: not a region)
U g_vt_di[4], g_vt_dev[1];

HRESULT __stdcall FakeCreateDevice(void* self, const GUID* guid, void** out, void* outer) {
    sh::Record(kCreateSite, Key(self), sh::HashBytes(guid, 16), Key(out), Key(outer));
    const U n = sh::Noise();
    if (n % 3 == 0) {
        if (n & 8) *out = nullptr;
        return static_cast<HRESULT>(n | 1);
    }
    *out = g_com + ((n & 0x10) ? 0x10 : 0x18);
    return 0;
}
HRESULT __stdcall FakeQueryInterface(void* self, const IID* iid, void** out) {
    sh::Record(kQuerySite, Key(self), sh::HashBytes(iid, 16), Key(out));
    *out = reinterpret_cast<void*>(static_cast<std::uintptr_t>(sh::Noise()));
    return static_cast<HRESULT>(sh::Noise());
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
sh::Callee g_callees[] = {
    // the group's own, called directly (E8) by the group's
    {G_OURS(MenuList_GeneWinDraw), 1, {kAll}, kG, 0, 0},
    {G_OURS(ShopWin_DrawRowMenu), 1, {kAll}, kG, 0, 0},
    {G_OURS(MasterWin_DrawList), 1, {kAll}, kG, 0, 0},
    {G_OURS(MasterWin_Available), 1, {kU8}, kF, 0, 0},                       // mov eax, [esp + 4]; and eax, 0xFF
    {G_OURS(MasterWin_SkillKnown), 1, {kU8}, kF, 0, 0},                      // mov dl, [esp + 8]
    {G_OURS(MasterWin_DrawCaption), 1, {kAll}, kG, 0, 0},
    {G_OURS(MasterWin_DrawPupils), 1, {kAll}, kG, 0, 0},
    {G_OURS(MasterWin_DrawPortrait), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},  // and edx / eax, 0xFFFF; cmp byte [esp + 0xC]; al = [esp + 0x38]
    {G_OURS(BattleEquipWin_DrawItems), 1, {kAll}, kG, 0, 0},
    {G_OURS(Menu_DrawVerbPair), 3, {kU16, kU16, kU8}, kG, 0, 0},             // x, y into word readers; bl = [esp + 0x34]
    // ours, other groups', with what each reads
    {G_OURS(SharedList_DrawList), 1, {kAll}, kG, 0, 0},
    {G_OURS(SharedList_DrawMember), 1, {kAll}, kG, 0, 0},
    {G_OURS(SharedList_DrawItemCount), 2, {kU16, kU16}, kG, 0, 0},
    {G_OURS(BattleEquipWin_Draw), 6, {kU8, kU16, kU16, kAll, kU8, kAll}, kG, 0, 0},   // member & 0xFF, x / y words, set, flags' bits, record
    {G_OURS(Menu_DrawMemberStatus), 5, {kU16, kU16, kU8, kAll, kAll}, kG, 0, 0},       // the fifth word pushed (0), not read
    {G_OURS(Menu_DrawTile16), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},                    // field_o.cpp: x, y & 0xFFFF, u << 4 a byte, dim a byte
    {G_OURS(Gpu_SetSprt8), 1, {kAll}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {G_OURS(Port_DroppedCall), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},                   // reads none; the frame's arguments logged
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kAll, kAll, kAll}, kG, 0, 0, {0, sh::kDerefString}, &SprintfEffect, nullptr, true},
    {"_stricmp", at::kStricmp, at::kStricmp, 2, {kAll, kAll}, kF, 0, 0, {sh::kDerefString, sh::kDerefString}, nullptr, nullptr, true},
    {"IDirectInput::CreateDevice", kCreateSite, kCreateSite, 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {"IUnknown::QueryInterface", kQuerySite, kQuerySite, 3, {kAll, kAll, kAll}, kG, 0, 0},
    // DIV-0011's Menu_DrawFrame and DIV-0059's ListTitle_DrawAt: keyed at start-up
    // by where their sites reach (x, y s16, w, h bytes; Text_DrawAt's), each
    // without its divergence a second row of an address listed above (skipped)
    {G_OURS(Port_DroppedCall), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    {G_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0},
};
constexpr unsigned kFrameRow = sizeof g_callees / sizeof g_callees[0] - 2;
constexpr unsigned kTitleRow = sizeof g_callees / sizeof g_callees[0] - 1;
#undef G_OURS

// The step tables the runs read in place, swapped for recorders on both sides;
// each count is the run of handlers to the next table (docs/rest_2h.md section 3).
const sh::DataTable kTables[] = {
    {at::kGeneSteps, 3},       {at::kSharedListSteps, 5},    {at::kSharedMemberSteps, 5}, {at::kMemberStatusSteps, 3},
    {at::kMasterListSteps, 4}, {at::kMasterCaptionSteps, 3}, {at::kPupilsSteps, 3},       {at::kItemCountSteps, 3},
    {at::kEquipSteps, 4},      {at::kEquipItemsSteps, 4},
};

// Beyond field mode's standard regions.
unsigned char g_instance[0x240];   // a DIDEVICEINSTANCEA and its product name at +0x12C
unsigned char g_reserve[0x40];     // the reserve list's ids
const sh::Region kRegions[] = {
    {at::kWindows, 22 * at::kWindowStride},   // WindowRecords
    {0x903A94, 0x903F90 - 0x903A94},          // CharacterRecords past the standard 0x903A14 + 0x80
    {0x904160, 0x400},                        // the inventory's id and count lists, 0x904160..0x904560
    {0x6BE0A0, 0x1C0},                        // the gene list 0x6BE0B0, the item list 0x6BE0C4 / 0x6BE144, and past them
    {0x7DE7A8, 0x80},                         // Key_Table
    {0x7DE928, 0x14},                         // DInput_Object, _Joystick, _Joystick2, _JoystickFound
    {0, sizeof g_instance},
    {0, sizeof g_reserve},
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seed ---------------------------------------------------------------------------

unsigned char* Window(unsigned k) { return Mem(at::kWindows + (k % 22) * at::kWindowStride); }
unsigned char* Record(unsigned n) { return Mem(at::kRecords + (n % 8) * at::kRecordStride); }
unsigned char* g_window;   // the record a kCall draw is handed (a[0])
unsigned g_self_index;

// Member ids whose 0x66972C byte is a record 0..7 (read at start-up, in place).
unsigned char g_good_ids[256];
unsigned g_good_n;
unsigned char GoodId() { return g_good_n ? g_good_ids[sh::Next() % g_good_n] : 0; }

// Each run's step table length (its +3 is drawn below it).
unsigned StepsOf(unsigned k) {
    switch (k) {
    case kGeneRun: return 3;
    case kSharedRun: return 5;
    case kMemberRun: return 5;
    case kStatusRun: return 3;
    case kMasterRun: return 4;
    case kCaptionRun: return 3;
    case kPupilsRun: return 3;
    case kCountRun: return 3;
    case kEquipRun: return 4;
    case kItemsRun: return 4;
    default: return 0;
    }
}

// A slide's word at and around its arrival: bound - by, +/- 1, 2, 0x10, 0x20.
void SeedSlide(unsigned char* w, unsigned at_word, int by, int bound) {
    const int arrive = bound - by;
    const U d = PickOf(0, 0, 1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu, 0x10, 0xFFFFFFF0u, 0x20, 0xFFFFFFE0u, 0x40, sh::Next());
    SetWord(w + at_word, static_cast<U>(arrive) + (sh::Often() ? d : sh::Next()));
}

void SeedWindow(unsigned char* w) {
    w[3] = static_cast<unsigned char>(sh::Next() % 3);
    SetWord(w + 4, PickOf(0, 0x10, 0x28, 0x8C, 0xFF88, 0x7FFF, 0x8000, sh::Next() % 0x140, sh::Next()));
    SetWord(w + 6, PickOf(0, 0x10, 0x26, 0x80, 0xF0, 0xFFEC, sh::Next() % 0xF0, sh::Next()));
    w[8] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 0x10, 0x11, 0x14, 0xF0, 0xF4, 0xF5, sh::Next()));
    w[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, sh::Next()));
    w[0xA] = static_cast<unsigned char>(1 + sh::Next() % 6);   // the lists' top: 1..6 (MasterWin_DrawList's rows stay in its 17)
    w[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x10, 0x11, sh::Next() % 0x14, sh::Next()));
    w[0xC] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, 0x10, sh::Next() % 0x12, sh::Next()));
    w[0xD] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0x10, 0x11, 0xF0, 0xF1, 0xFF, sh::Next()));
    w[0x10] = static_cast<unsigned char>(PickOf(0, 0x10, 0x11, 0xF0, 0xF5, sh::Next()));
}

// The character records' bytes the draws test.
void SeedRecords() {
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const r = Record(n);
        r[9] = static_cast<unsigned char>(PickOf(0, 1, 4, 0xB, sh::Next() % 12, sh::Next()));
        r[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
        r[0x10] = static_cast<unsigned char>(PickOf(0, 0x20, 0x80, 0xA0, sh::Next()));
        r[0x11] = static_cast<unsigned char>(PickOf(0, 0x20, sh::Next()));
        SetWord(r + 0x18, PickOf(0, 1, 2, 0x100, sh::Next()));
        r[0x1E] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        r[0x1F] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 17, sh::Next()));
        const U ap_max = PickOf(0, 4, 8, 0x40, 0x41, 0x3F, sh::Next() & 0xFFFF);
        SetWord(r + 0x22, ap_max);
        SetWord(r + 0x1A, PickOf(0, 1, ap_max >> 2, (ap_max >> 2) + 1, (ap_max >> 2) - 1u, sh::Next()));
    }
    Mem(at::kWindow1Master)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 17, sh::Next()));
    for (unsigned i = 0; i < 6; ++i) Mem(0x904062 + i)[0] = static_cast<unsigned char>(sh::Often() ? GoodId() : sh::Next());
}

// A master's requirement list's skills placed (or not) in the slots and the shared list.
void SeedSkills(unsigned master) {
    if (master >= at::kMasterCount) return;
    const auto* p = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(at::kRequirements + 4 * master)))));
    for (unsigned i = 0; i < 8 && p[i] != 0xFF; ++i) {
        if (!sh::Often()) continue;
        if (sh::Half()) Mem(at::kSkillSlots + (sh::Next() % 7) * at::kRecordStride + sh::Next() % 10)[0] = p[i];
        else Mem(at::kSharedList + sh::Next() % 0x80)[0] = p[i];
    }
}

void Seed(unsigned k) {
    // the window the handler is on, and the one a draw is handed
    g_self_index = sh::Next() % 22;
    unsigned char* const self = Window(g_self_index);
    sh::SetPointer(at::kCurrent, self);
    g_window = sh::Half() ? self : Window(sh::Next());
    SeedWindow(self);
    SeedWindow(g_window);
    if (const unsigned n = StepsOf(k)) self[3] = static_cast<unsigned char>(sh::Next() % n);
    SeedRecords();
    Mem(at::kStyle)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    Frame_Counter = sh::Next();
    // the gene bits: some of the eighteen, rarely all (the list then has no 0 in its 18)
    SetLong(Mem(at::kGenes), static_cast<std::int32_t>(sh::Often() ? sh::Next() & 0x1FFFF : PickOf(0, 0x3FFFF, 0x7FFFF, sh::Next())));
    // the masters' bits
    for (unsigned i = 0; i < 3; ++i) Mem(at::kMasterBits + i)[0] = static_cast<unsigned char>(sh::Next());
    Mem(at::kPartyBits)[0] = static_cast<unsigned char>(sh::Next());
    switch (k) {
    case kSharedSlide: SeedSlide(self, 4, 0x20, 0x28); break;
    case kMasterOut: {
        // the copy's bound: the original's, DIV-0041's, or any
        const U bound = PickOf(0xFFFFFF88u, 0xFFFFFF53u, 0, 0xFFFFFFFFu, 0x7FFF, 0xFFFF8000u, sh::Next());
        SetLong(static_cast<unsigned char*>(g_r2h_slide_copy) + 6, static_cast<std::int32_t>(bound));
        SeedSlide(self, 4, -0x20, static_cast<short>(bound));
        break;
    }
    case kMasterIn: SeedSlide(self, 4, 0x20, 0x64); break;
    case kCaptionSlide: SeedSlide(self, 4, -0x20, 0x8C); break;
    case kPupilsDown: SeedSlide(self, 6, 0x10, 0xF0); break;
    case kPupilsUp: SeedSlide(self, 6, -0x10, 0x80); break;
    case kCountSlide: SeedSlide(self, 4, -0x20, 0xD2); break;
    case kItemLeft: SeedSlide(self, 4, -0x20, 0x53); break;
    case kEquipSlide: SeedSlide(self, 4, 0x20, 0x62); break;
    case kItemsSlide: SeedSlide(self, 4, 0x20, 0x98); break;
    case kReserve: {
        // the object: a count of 0..5 and a list of ids (mostly ones with a record)
        g_window[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5));
        for (unsigned i = 0; i < sizeof g_reserve; ++i) g_reserve[i] = sh::Often() ? GoodId() : static_cast<unsigned char>(sh::Next() % 0x18);
        sh::SetPointer(Key(g_window + 0x20), g_reserve + sh::Next() % 8);
        break;
    }
    case kAvailable: SeedSkills(sh::Next() % at::kMasterCount); break;
    case kSkill:
        SeedSkills(sh::Next() % at::kMasterCount);
        break;
    case kMasterDraw:
    case kMasterRun:
        for (unsigned i = 0; i < 4; ++i) SeedSkills(sh::Next() % at::kMasterCount);
        break;
    case kPortrait:
    case kPupilsDraw:
    case kPupilsRun:
        Mem(at::kWindow1Master)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3));
        for (unsigned n = 0; n < 8; ++n) Record(n)[0x1F] = static_cast<unsigned char>(PickOf(0, 1, 2, 3));
        Cond_ByteFA = static_cast<signed char>(PickOf(7, 8, 9, 0, 0xFF, 0x80, sh::Next()));
        break;
    case kItemsDraw:
    case kItemsRun: {
        unsigned char* const w = k == kItemsDraw ? g_window : self;
        w[0xC] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
        w[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, 0x10, 0x78, 0x79, sh::Next()));
        // the inventory: ids with gaps, counts
        for (U a = 0x9041D4; a < 0x904354; ++a) Mem(a)[0] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    }
    case kKeys: break;
    case kEnum: {
        sh::SetPointer(Key(&DInput_Object), g_com);
        sh::SetPointer(Key(&DInput_Joystick), sh::Half() ? g_com + 0x10 : nullptr);
        for (unsigned i = 0; i < sizeof g_instance; ++i) g_instance[i] = static_cast<unsigned char>(sh::Next());
        g_instance[0x12C + sh::Next() % 0x40] = 0;
        break;
    }
    default: break;
    }
}

// The arguments of the kCall functions, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kReserve:
    case kGeneDraw:
    case kRowDraw:
    case kMasterDraw:
    case kCaptionDraw:
    case kPupilsDraw:
    case kItemsDraw: a[0] = Key(g_window); break;
    case kAvailable: a[0] = (a[0] & 0xFFFFFF00u) | PickOf(0xB, 0xC, 0xD, 0xE, sh::Next() % at::kMasterCount, sh::Next() % at::kMasterCount); break;
    case kSkill: {
        const U pick = sh::Half() ? Mem(at::kSkillSlots + (a[1] % 7) * at::kRecordStride + a[2] % 10)[0]
                                  : (sh::Half() ? Mem(at::kSharedList + a[2] % 0x80)[0] : a[0]);
        a[0] = (a[0] & 0xFFFFFF00u) | (pick & 0xFF);
        break;
    }
    case kPortrait:
        a[2] = (a[2] & 0xFFFFFF00u) | PickOf(4, 4, 0xB, 0, 1, 2, 3, a[2] % 12, a[2]);
        a[3] = (a[3] & 0xFFFFFF00u) | PickOf(0, 1, 2, a[3]);
        break;
    case kVerbPair: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 1, 0xFF, 2, a[2]); break;
    case kKeys: a[0] = Key(sh::Scratch(0)); break;
    case kEnum: a[0] = Key(g_instance); break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call --------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    unsigned char* const self = Window(g_self_index);
    switch (h % 12) {
    case 0: sh::SetPointer(at::kCurrent, Window(b)); break;   // another record (the runs re-read 0x905B84)
    case 1: SetWord(self + (b & 1 ? 4 : 6), v & 0x1FF); break;
    case 2: Mem(at::kStyle)[0] = b; break;
    case 3: g_window[0xB] = static_cast<unsigned char>(b % 0x12); break;
    case 4: g_window[8] = static_cast<unsigned char>(1 + b % 3); break;
    case 5: SetWord(g_window + (b & 1 ? 4 : 6), v & 0x1FF); break;
    case 6: Mem(at::kWindow1Master)[0] = static_cast<unsigned char>(b % 4); break;
    case 7: Record(b)[(b >> 3) & 1 ? 0xB : 0x1F] = static_cast<unsigned char>(v); break;
    case 8: Mem(at::kUseIds + b % 0x80)[0] = static_cast<unsigned char>(v); break;
    case 9: g_window[0xC] = static_cast<unsigned char>(b % 3); break;
    case 10: Mem(at::kBattleParty + b % 3)[0] = static_cast<unsigned char>(v); break;
    case 11: g_window[9] = static_cast<unsigned char>(b % 7); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // the four copies of this file
    g_r2h_reserve_copy = Copy("MenuList_ReserveWinDraw", 0x59AA80, 0x35D, kCalls59AA80,
                              static_cast<int>(sizeof kCalls59AA80 / sizeof kCalls59AA80[0]), at::kFrameSite);
    g_r2h_items_copy = Copy("BattleEquipWin_DrawItems", 0x59DBF0, 0x568, kCalls59DBF0,
                            static_cast<int>(sizeof kCalls59DBF0 / sizeof kCalls59DBF0[0]), at::kTitleSite);
    move_script::Relocate(g_r2h_items_copy, 0x59DBF0, 0x568, kTable59DBF0);
    g_r2h_slide_copy = bof3::CloneOriginal("MasterWin_SlideOut", 0x59C130, 0x28);
    g_r2h_enum_copy = Copy("DInput_EnumJoystick", 0x5A9620, 0x64, kCalls5A9620,
                           static_cast<int>(sizeof kCalls5A9620 / sizeof kCalls5A9620[0]), 0);
    g_clones[kReserve].base = Wrapper(&R2hReserveTheirs, 0x25);
    g_clones[kItemsDraw].base = Wrapper(&R2hItemsTheirs, 0x25);
    g_clones[kMasterOut].base = Wrapper(&R2hSlideTheirs, 0x25);
    g_clones[kEnum].base = Wrapper(&R2hEnumTheirs, 0x74);
    // the divergences' sites: rows of their own when they reach ours
    const U frame = SiteTarget(at::kFrameSite);
    if (frame != bof3::addr::Port_DroppedCall) {
        sh::Callee& row = g_callees[kFrameRow];
        row.name = "Menu_DrawFrame (DIV-0011)";
        row.address = at::kFrameSite;
        row.key = frame;
        for (unsigned i = 0; i < 4; ++i) row.masks[i] = i < 2 ? kU16 : kU8;
    }
    const U title = SiteTarget(at::kTitleSite);
    if (title != bof3::addr::Text_DrawAt) {
        sh::Callee& row = g_callees[kTitleRow];
        row.name = "ListTitle_DrawAt (DIV-0059)";
        row.address = at::kTitleSite;
        row.key = title;
    }
    // the fakes and the regions of this file's own
    g_vt_di[0] = g_vt_di[1] = g_vt_di[2] = 0;
    g_vt_di[3] = KeyOf(&FakeCreateDevice);
    g_vt_dev[0] = KeyOf(&FakeQueryInterface);
    std::memset(g_com, 0, sizeof g_com);
    SetLong(g_com, static_cast<std::int32_t>(Key(g_vt_di)));
    SetLong(g_com + 0x10, static_cast<std::int32_t>(Key(g_vt_dev)));
    SetLong(g_com + 0x18, static_cast<std::int32_t>(Key(g_vt_dev)));
    const unsigned n_regions = sizeof kRegions / sizeof kRegions[0];
    for (unsigned i = 0; i < n_regions; ++i) g_regions[i] = kRegions[i];
    g_regions[n_regions - 2].at = Key(g_instance);
    g_regions[n_regions - 1].at = Key(g_reserve);
    for (unsigned id = 0; id < 256; ++id)
        if (Mem(at::kMemberMap + id)[0] < 8) g_good_ids[g_good_n++] = static_cast<unsigned char>(id);
    // MasterWin_SlideOut reads its bound from the copy's immediate
    g_master_bound = Key(g_r2h_slide_copy) + 6;

    // BOF3X_R2H_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2H_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(g_clones[k].name, only)) {
            index[n] = k;
            chosen[n++] = g_clones[k];
        }
    if (n == 0) bof3::Fatal("rest_2h: BOF3X_R2H_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2h", chosen, n, g_callees, sizeof g_callees / sizeof g_callees[0], kTables,
                   sizeof kTables / sizeof kTables[0], g_regions, n_regions,
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
    g_master_bound = at::kMasterBound;
}

}  // namespace rest_2h
