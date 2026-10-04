// BOF3X_SHADOW=rest_3g: group R3G's 32 functions through the scenario harness
// (scenario_harness.h, used unchanged) in effect mode, once at start-up.
// docs/rest_3g.md section 4. BOF3X_R3G_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R3G --clones --harness scenario
// (2026-10-04, through the round's band14.py wrapper), each extent read again
// to its last instruction (capstone); the cut's sizes are padding past them.
// Shapes: the seven effect states kEffect (their kind each); the cdecl helpers
// kCall (Clone::pointers where an argument is a pointer, ret_mask where a
// caller reads the answer); the game modes' dispatchers and steps, the boss
// placement, the enemies' clear and area 0xBD's frame and build kState. Two
// groups: the 31 at 4,000 rounds a function, and AreaMapBD_BuildView alone
// (each call walks up to 2,240 cells) at fewer. The four mode step tables are
// DataTables. Every callee the group's code calls that no standard set lists,
// or lists otherwise than the group needs, is listed here (registered before
// the standard rows: the group's listing stands).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_3g {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
U FloatBits(float f) {
    U b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}
void PutFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// --- the clone table (band_rows.py --group R3G --clones --harness scenario, 2026-10-04) ---
constexpr sh::CallSite kCalls4925E0[] = {{0xD, 0x492400}};
constexpr sh::CallSite kCalls492620[] = {{0x17, 0x492400}, {0x39, 0x589840}};
constexpr sh::CallSite kCalls492680[] = {{0x29, 0x5720C0}};
constexpr sh::CallSite kCalls492710[] = {{0x26, 0x492DC0}};
constexpr sh::CallSite kCalls492780[] = {{0x29, 0x5720C0}, {0x77, 0x494060}, {0xA0, 0x494110},
                                         {0xBD, 0x494110}, {0xDC, 0x5A7A70}, {0xF5, 0x587740}};
constexpr sh::CallSite kCalls493E50[] = {{0xB, 0x5A7A00},  {0x1D, 0x5A7A50}, {0x37, 0x5A7A50},
                                         {0x4B, 0x5A7A00}, {0xE1, 0x493F70}, {0x106, 0x587740}};
constexpr sh::CallSite kCalls4941B0[] = {{0x26, 0x5B9550}};
constexpr sh::CallSite kCalls494500[] = {{0x3F, 0x494570}};
constexpr sh::CallSite kCalls494570[] = {{0x7, 0x494920}, {0x10A, 0x4946C0}, {0x11B, 0x494F00}};
constexpr sh::CallSite kCalls496440[] = {{0x9, 0x52B480},  {0x10, 0x495040}, {0x15, 0x56E6C0}, {0x1A, 0x592F00},
                                         {0x63, 0x5A7810}, {0x6C, 0x461E50}, {0x71, 0x52CF00}};
constexpr sh::CallSite kCalls4964E0[] = {{0x1C, 0x454770}, {0x24, 0x454810}, {0x2D, 0x52CF40}, {0x34, 0x5A9949},
                                         {0x3C, 0x454810}, {0x58, 0x587BE0}, {0x62, 0x495040}, {0x74, 0x52CF40},
                                         {0x7B, 0x5A9949}, {0xA4, 0x594E00}, {0xBF, 0x587B40}};
constexpr sh::CallSite kCalls4965D0[] = {{0x2, 0x495040},  {0x14, 0x517290}, {0x1B, 0x5A9949}, {0x2F, 0x5A9949},
                                         {0x36, 0x5A9949}, {0x40, 0x454590}, {0x48, 0x454810}, {0x53, 0x5A9949},
                                         {0x5B, 0x454810}, {0x66, 0x4549F0}, {0x6D, 0x4549F0}};
constexpr sh::CallSite kCalls496660[] = {{0x2, 0x5A9949},  {0x9, 0x5A9949},  {0x23, 0x454770}, {0x2A, 0x495040},
                                         {0x3C, 0x517290}, {0x43, 0x5A9949}, {0x6E, 0x517200}};
constexpr sh::CallSite kCalls4966F0[] = {{0x2, 0x495040},  {0x14, 0x517290}, {0x1B, 0x5A9949}, {0x2F, 0x5A9949},
                                         {0x36, 0x5A9949}, {0x40, 0x454590}, {0x48, 0x454810}, {0x53, 0x5A9949},
                                         {0x5B, 0x454810}, {0x66, 0x4549F0}, {0x6D, 0x4549F0}};
constexpr sh::CallSite kCalls496790[] = {{0x0, 0x536B60}, {0x5, 0x5172C0}};
constexpr sh::CallSite kCalls4967B0[] = {{0x0, 0x496A00}, {0x5, 0x5172C0}};
constexpr sh::CallSite kCalls4967C0[] = {{0x0, 0x496AD0}, {0x5, 0x5172C0}};
constexpr sh::CallSite kCalls4FEE70[] = {{0x12, 0x57C140}};
constexpr sh::CallSite kCalls4FEEB0[] = {{0x26, 0x57C140}, {0x34, 0x4FEE70}, {0x51, 0x57C110}, {0x6A, 0x57C0F0},
                                         {0x7C, 0x469FE0}, {0x86, 0x587740}, {0x90, 0x587740}};
constexpr sh::CallSite kCalls5100B0[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x3D, 0x5A75D0}, {0x45, 0x5A77A0},
                                         {0xC3, 0x5A85F0}, {0xC9, 0x5A9290}, {0xDC, 0x572A00}, {0xE5, 0x461E50}};
constexpr sh::CallSite kCalls5101C0[] = {
    {0x18, 0x5A77C0},  {0x21, 0x461E50},  {0x87, 0x5A7650},  {0x8F, 0x5A7780},  {0xDF, 0x5A8250},  {0xE8, 0x5A9110},
    {0x110, 0x5A8250}, {0x119, 0x5A9110}, {0x122, 0x461E50}, {0x147, 0x5A7650}, {0x14F, 0x5A7780}, {0x15E, 0x5A7A00},
    {0x177, 0x5B9550}, {0x183, 0x5A7A50}, {0x19C, 0x5B9550}, {0x1BA, 0x5A8250}, {0x1C3, 0x5A9110}, {0x1CC, 0x5A7A00},
    {0x1E5, 0x5B9550}, {0x1F1, 0x5A7A50}, {0x20A, 0x5B9550}, {0x228, 0x5A8250}, {0x234, 0x5A9110}, {0x23D, 0x461E50},
    {0x258, 0x5A7650}, {0x260, 0x5A7780}, {0x29F, 0x5A8250}, {0x2A8, 0x5A9110}, {0x2D0, 0x5A8250}, {0x2D9, 0x5A9110},
    {0x2E2, 0x461E50}, {0x30A, 0x5A7650}, {0x312, 0x5A7780}, {0x321, 0x5A7A00}, {0x33A, 0x5B9550}, {0x346, 0x5A7A50},
    {0x35F, 0x5B9550}, {0x37D, 0x5A8250}, {0x386, 0x5A9110}, {0x38F, 0x5A7A00}, {0x3A8, 0x5B9550}, {0x3B4, 0x5A7A50},
    {0x3CD, 0x5B9550}, {0x3EB, 0x5A8250}, {0x3F7, 0x5A9110}, {0x400, 0x461E50}, {0x44E, 0x5A77C0}, {0x457, 0x461E50}};
constexpr sh::CallSite kCalls510630[] = {{0x76, 0x5A8060}, {0xD1, 0x5A7BF0}, {0x117, 0x5A8DE0}, {0x121, 0x5A8E00}, {0x132, 0x510780}};
constexpr sh::CallSite kCalls510780[] = {{0x1CC, 0x5A8E30}, {0x1D1, 0x5A8E90}, {0x1DB, 0x5A90B0}, {0x2CD, 0x510BB0}, {0x388, 0x5A8E50},
                                         {0x38D, 0x5A8F60}, {0x3B0, 0x5A7560}, {0x3D7, 0x5A90D0}, {0x3EA, 0x5A9290}};
constexpr sh::CallSite kCalls510BB0[] = {{0x8, 0x5A7780}};

#define R3G_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R3G_CALLS(a) a, R3G_N(a)
#define R3G_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect, kCa = sh::Shape::kCall, kSt = sh::Shape::kState;
constexpr U kAll = 0xFFFFFFFFu;
constexpr U kScr = sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(1, sh::Arg::kScratch) | sh::ArgAt(2, sh::Arg::kScratch);
// Answers: Screen_TriangleWinding's eax (the callers test ax; the whole is
// _ftol's low dword), Quake_VertexLift's ax, Area109_SwitchPattern's eax
// (EffectKind18_09_Pattern compares it whole), Area109_SwitchHook's al
// (Area_CellHook movsx's it), EffectKind0F_CharCount's eax (whole, compared).
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll31[] = {
    {"EffectKindAC_Start", 0x4925C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(EffectKindAC_Start), 0, false, kEf, 0, 3, 0, 0xAC},
    {"EffectKindAC_FadeIn", 0x4925E0, 0x3B, R3G_CALLS(kCalls4925E0), nullptr, 0, nullptr, 0, R3G_FN(EffectKindAC_FadeIn), 0, false, kEf, 0, 3, 0, 0xAC},
    {"EffectKindAC_FadeOut", 0x492620, 0x3F, R3G_CALLS(kCalls492620), nullptr, 0, nullptr, 0, R3G_FN(EffectKindAC_FadeOut), 0, false, kEf, 0, 3, 0, 0xAC},
    {"EffectKindAD_Start", 0x492680, 0x87, R3G_CALLS(kCalls492680), nullptr, 0, nullptr, 0, R3G_FN(EffectKindAD_Start), 0, false, kEf, 0, 5, 0, 0xAD},
    {"EffectKindAD_Rise", 0x492710, 0x40, R3G_CALLS(kCalls492710), nullptr, 0, nullptr, 0, R3G_FN(EffectKindAD_Rise), 0, false, kEf, 0, 5, 0, 0xAD},
    {"EffectKindAE_Start", 0x492780, 0xFE, R3G_CALLS(kCalls492780), nullptr, 0, nullptr, 0, R3G_FN(EffectKindAE_Start), 0, false, kEf, 0, 5, 0, 0xAE},
    {"EffectKindBA_Line", 0x493E50, 0x117, R3G_CALLS(kCalls493E50), nullptr, 0, nullptr, 0, R3G_FN(EffectKindBA_Line), 0, false, kEf, 0, 3, 0, 0xBA},
    {"Screen_TriangleWinding", 0x4941B0, 0x2B, R3G_CALLS(kCalls4941B0), nullptr, 0, nullptr, 0, R3G_FN(Screen_TriangleWinding), kAll, false, kCa, kScr},
    {"Battle_PlaceBossActors", 0x494500, 0x6F, R3G_CALLS(kCalls494500), nullptr, 0, nullptr, 0, R3G_FN(Battle_PlaceBossActors), 0, false, kSt},
    {"Battle_PlaceBossActor", 0x494570, 0x145, R3G_CALLS(kCalls494570), nullptr, 0, nullptr, 0, R3G_FN(Battle_PlaceBossActor), 0, false, kCa},
    {"BattleEnemy_ClearStates", 0x494E70, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(BattleEnemy_ClearStates), 0, false, kSt},
    {"GameMode8_Run", 0x496430, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(GameMode8_Run), 0, false, kSt},
    {"GameMode8_Enter", 0x496440, 0x9A, R3G_CALLS(kCalls496440), nullptr, 0, nullptr, 0, R3G_FN(GameMode8_Enter), 0, false, kSt},
    {"GameMode8_Leave", 0x4964E0, 0xDA, R3G_CALLS(kCalls4964E0), nullptr, 0, nullptr, 0, R3G_FN(GameMode8_Leave), 0, false, kSt},
    {"GameMode9_Run", 0x4965C0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(GameMode9_Run), 0, false, kSt},
    {"GameMode9_Enter", 0x4965D0, 0x84, R3G_CALLS(kCalls4965D0), nullptr, 0, nullptr, 0, R3G_FN(GameMode9_Enter), 0, false, kSt},
    {"GameMode9_Leave", 0x496660, 0x73, R3G_CALLS(kCalls496660), nullptr, 0, nullptr, 0, R3G_FN(GameMode9_Leave), 0, false, kSt},
    {"GameMode10_Run", 0x4966E0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(GameMode10_Run), 0, false, kSt},
    {"GameMode10_Enter", 0x4966F0, 0x84, R3G_CALLS(kCalls4966F0), nullptr, 0, nullptr, 0, R3G_FN(GameMode10_Enter), 0, false, kSt},
    {"GameMode11_Run", 0x496780, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(GameMode11_Run), 0, false, kSt},
    {"GameMode11_Frame", 0x496790, 0x1D, R3G_CALLS(kCalls496790), nullptr, 0, nullptr, 0, R3G_FN(GameMode11_Frame), 0, false, kSt},
    {"GameMode11_Look", 0x4967B0, 0xA, R3G_CALLS(kCalls4967B0), nullptr, 0, nullptr, 0, R3G_FN(GameMode11_Look), 0, false, kSt},
    {"GameMode11_LookEnd", 0x4967C0, 0x2A, R3G_CALLS(kCalls4967C0), nullptr, 0, nullptr, 0, R3G_FN(GameMode11_LookEnd), 0, false, kSt},
    {"Quake_VertexLift", 0x4CF4B0, 0x138, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(Quake_VertexLift), 0xFFFF, false, kCa},
    {"Area109_SwitchPattern", 0x4FEE70, 0x35, R3G_CALLS(kCalls4FEE70), nullptr, 0, nullptr, 0, R3G_FN(Area109_SwitchPattern), kAll, false, kCa},
    {"Area109_SwitchHook", 0x4FEEB0, 0xA0, R3G_CALLS(kCalls4FEEB0), nullptr, 0, nullptr, 0, R3G_FN(Area109_SwitchHook), 0xFF, false, kCa},
    {"EffectKind18Sub41_DrawPanels", 0x5100B0, 0x101, R3G_CALLS(kCalls5100B0), nullptr, 0, nullptr, 0, R3G_FN(EffectKind18Sub41_DrawPanels), 0, false, kCa},
    {"EffectKind18Sub41_DrawRings", 0x5101C0, 0x467, R3G_CALLS(kCalls5101C0), nullptr, 0, nullptr, 0, R3G_FN(EffectKind18Sub41_DrawRings), 0, false, kCa},
    {"AreaMap_FrameAreaBD", 0x510630, 0x141, R3G_CALLS(kCalls510630), nullptr, 0, nullptr, 0, R3G_FN(AreaMap_FrameAreaBD), 0, false, kSt},
    {"AreaMapBD_CellTexture", 0x510BB0, 0x68, R3G_CALLS(kCalls510BB0), nullptr, 0, nullptr, 0, R3G_FN(AreaMapBD_CellTexture), 0, false, kCa},
    {"EffectKind0F_CharCount", 0x5171E0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, R3G_FN(EffectKind0F_CharCount), kAll, false, kCa, sh::ArgAt(0, sh::Arg::kScratch)},
};
const sh::Clone kBuild[] = {
    {"AreaMapBD_BuildView", 0x510780, 0x424, R3G_CALLS(kCalls510780), nullptr, 0, nullptr, 0, R3G_FN(AreaMapBD_BuildView), 0, false, kSt},
};
#undef R3G_FN
#undef R3G_CALLS
#undef R3G_N

enum : unsigned {
    kACStart, kACFadeIn, kACFadeOut, kADStart, kADRise, kAEStart, kBALine, kWinding, kPlaceAll, kPlaceOne, kClearStates,
    kMode8Run, kMode8Enter, kMode8Leave, kMode9Run, kMode9Enter, kMode9Leave, kMode10Run, kMode10Enter, kMode11Run,
    kMode11Frame, kMode11Look, kMode11LookEnd, kQuake, kPattern, kSwitch, kPanels, kRings, kFrameBD, kCellTexture,
    kCharCount, kCount
};
static_assert(kCount == sizeof kAll31 / sizeof kAll31[0], "one enum entry a clone, in order");
constexpr unsigned kBuildView = kCount;   // the second group's one clone, its own seed index

// --- the stand-ins' effects -------------------------------------------------------------

// Writable: inside the regions, or on the caller's stack (an out the caller
// keeps in a local).
bool Writable(const void* p, unsigned n) {
    if (sh::InRegions(p, n)) return true;
    unsigned char here;
    const auto at = reinterpret_cast<std::uintptr_t>(p), sp = reinterpret_cast<std::uintptr_t>(&here);
    return at > sp && at + n < sp + 0x10000;
}
void* Ptr(U a) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a)); }

// BossActor_Find: null a quarter of the time, else a Sprite_Objects record (its
// callers copy 0x80 bytes from it and set its +0 bit 6).
U BossFindEffect(const U*, U) {
    const U n = sh::Noise();
    if (n % 4 == 0) return 0;
    return sh::at::kSprites + sh::at::kSpriteStride * ((n >> 2) % sh::at::kSpriteCount);
}
// Area109_SwitchPattern (ours, Area109_SwitchHook's callee): 1..8 whole, as
// the real one answers (lea eax, [edi + 1]); the caller divides all of it.
U PatternEffect(const U*, U answer) { return 1u + (answer >> 8) % 8u; }
// Gte_StoreScreenXY into MapView_ScreenXY: the cull's inputs. Mostly a point
// far outside both ranges (so a call keeps few cells: each kept one takes a
// draw item), else each coordinate at or about a bound of the two ranges, or
// NaN.
U ScreenXYEffect(const U* a, U answer) {
    unsigned char* const p = static_cast<unsigned char*>(Ptr(a[0]));
    if (!Writable(p, 8)) return answer;
    const U n = sh::Noise();
    static const float kX[] = {-200.0f, -199.99f, -199.0f, 519.99f, 520.0f, 521.0f, -50.0f, -49.99f, -51.0f,
                               369.99f, 370.0f, 371.0f, 0.0f, 160.0f, 400.0f, -100.0f, 600.0f, -600.0f};
    static const float kY[] = {120.0f, 120.01f, 119.99f, 121.0f, 120.99f, 121.01f, 0.0f, 240.0f, -500.0f, 500.0f};
    if (n % 48 != 0) {
        PutFloat(p, (n & 0x100) ? 20000.0f : -20000.0f);
        PutFloat(p + 4, static_cast<float>(static_cast<int>((n >> 9) % 600) - 300));
        return answer;
    }
    const U nan = 0x7FC00000u;
    const U x = (n >> 6) % 23 == 0 ? nan : FloatBits(kX[(n >> 6) % (sizeof kX / sizeof kX[0])]);
    const U y = (n >> 12) % 17 == 0 ? nan : FloatBits(kY[(n >> 12) % (sizeof kY / sizeof kY[0])]);
    std::memcpy(p, &x, 4);
    std::memcpy(p + 4, &y, 4);
    return answer;
}
// Gte_StoreScreenXY3: the three screen points (eight bytes each) of the quad.
U ScreenXY3Effect(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i)
        if (Writable(Ptr(a[i]), 8)) sh::FillBytes(Ptr(a[i]), 8);
    return answer;
}
// Gte_ApplyMatrix: the vector out, three longs (a local of the caller's).
U ApplyEffect(const U* a, U answer) {
    if (Writable(Ptr(a[2]), 12)) sh::FillBytes(Ptr(a[2]), 12);
    return answer;
}

#define R3G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
const sh::Callee kCallees[] = {
    // the group's own, called directly by the group's
    {R3G_OURS(Battle_PlaceBossActor), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0},   // slot, count, kind: each read as its byte
    {R3G_OURS(Area109_SwitchPattern), 0, {}, kG, 0, 0, {}, &PatternEffect},
    {R3G_OURS(AreaMapBD_BuildView), 0, {}, kPh, 0, 0},
    {R3G_OURS(AreaMapBD_CellTexture), 2, {0x0F0000FFu, kAll}, kG, 0, 0},   // bits 24..27 and the low byte; the quad
    // not ours yet: R3F's gradient (rest_3g_callees.h)
    {"0x492400 (R3F)", at::kFadeDraw, at::kFadeDraw, 1, {0xFF}, kG, 0, 0},
    // ours, no standard set lists them (or not as these callers need)
    {R3G_OURS(EffectKindAD_DrawArc), 4, {kAll, kAll, 0xFF, 0xFF}, kG, 0, 0, {12, 12}},
    {R3G_OURS(EffectKindBA_DrawLine), 3, {kAll, kAll, 0xFF}, kG, 0, 0, {12, 12}},
    {R3G_OURS(BossActor_Find), 1, {0xFF}, kG, 0, 0, {}, &BossFindEffect},
    {R3G_OURS(Battle_CopyEnemyData), 2, {0xFF, 0xFF}, kG, 0, 0},
    {R3G_OURS(Battle_SetEnemyOffset), 2, {0xFF, 0xFFFF}, kG, 0, 0},
    {R3G_OURS(Fish_Spawn), 0, {}, kPh, 0, 0},
    {R3G_OURS(GameMode8_Frame), 0, {}, kPh, 0, 0},
    {R3G_OURS(GameMode8_WaitFrame), 0, {}, kPh, 0, 0},
    {R3G_OURS(Field_Frame), 0, {}, kPh, 0, 0},
    {R3G_OURS(Mode11_ObjectFrame), 0, {}, kPh, 0, 0},
    {R3G_OURS(Mode11_FieldFrame), 0, {}, kPh, 0, 0},
    {R3G_OURS(Look_PadControl), 0, {}, kPh, 0, 0},
    {R3G_OURS(Look_Return), 0, {}, kPh, 0, 0},
    {R3G_OURS(Effect_HoldFlag1C), 1, {0xFF}, kG, 0, 0},   // its +9 the argument's byte
    {R3G_OURS(Gte_ApplyMatrix), 3, {kAll, 0, 0}, kG, 0, 0, {18, 6, 0}, &ApplyEffect},
    {R3G_OURS(Gte_LoadVertices3), 1, {kAll}, kG, 0, 0, {24}},
    {R3G_OURS(Gte_Rtpt), 0, {}, kG, 0, 0},
    {R3G_OURS(Gte_StoreScreenXY3), 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &ScreenXY3Effect},
    {R3G_OURS(Gte_StoreScreenXY), 1, {kAll}, kG, 0, 0, {}, &ScreenXYEffect},
};
#undef R3G_OURS

const sh::DataTable kTables[] = {
    {0x656AB8, 4},   // GameMode8_Steps
    {0x656AC8, 3},   // GameMode9_Steps
    {0x656AD4, 3},   // GameMode10_Steps
    {0x656AE0, 3},   // GameMode11_Steps
};
const std::uint8_t kKinds[] = {0xAC, 0xAD, 0xAE, 0xBA};

// The group's own regions beyond effect mode's standard ones.
constexpr U kItemsRegion = 48;   // draw items compared (0x90 each); AreaMapBD_BuildView keeps few cells a call
const sh::Region kRegions[] = {
    {0x904AA0, 0x20},                       // the battle bytes: the event battle 0x904AAA, the formation 0x904AAC, 0x904AB2 / B3
    {at::kEnemies, at::kEnemyStride * at::kEnemyCount},   // the eight enemy records
    {at::kEncounterRowsAt, 0x48},     // Encounter_Rows (8 rows of 9)
    {0x695990, 0x2A4},                      // Quake's .bss: the facing, the lift tables, the block's corner (magic_s23's region)
    {at::kMusicPlaying, 4},                 // the byte compared with Music_Track
    {at::kReturnX, 8},                      // mode 8's return x, z (after Draw_PassFlags)
    {at::kReturnArea, 0xA90},               // mode 8's return area word and DrawLayers 0x8022A0..0x802D20
    {at::kAnglesDrawn, 8},                  // Camera_AnglesDrawn
    {0x7E0688, 4},                          // MapView_Origin
    {0x9039D4, 4},                          // DrawItemPool_Top
    {0x905E80, kItemsRegion * 0x90},        // DrawItems' first items
};

// --- the seed ------------------------------------------------------------------------

unsigned char* S() { return Sprite_Current; }
float SmallFloat() {
    const U n = sh::Noise();
    return static_cast<float>(static_cast<int>(n % 0x800) - 0x400) / static_cast<float>(1u << ((n >> 11) % 4));
}

void Seed(unsigned k) {
    unsigned char* const s = S();
    switch (k) {
    case kACFadeIn:
    case kACFadeOut: s[9] = static_cast<unsigned char>(PickOf(1, 2, 7, 8, 9, 0, sh::Next())); break;
    case kADRise: Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0xB, 0xB, 0xA, 0xC, sh::Next())); break;
    case kBALine: SetWord(s + 0x2E, PickOf(0x149, 0x149, 0x14A, 0x148, 0, sh::Next())); break;
    case kWinding:
        // three screen points: small floats, often two equal, the cross product's sign either way
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const p = sh::Scratch(i);
            PutFloat(p, SmallFloat());
            PutFloat(p + 4, SmallFloat());
        }
        if (sh::Next() % 4 == 0) std::memcpy(sh::Scratch(2), sh::Scratch(1), 8);
        break;
    case kPlaceAll: {
        // an event battle below the battle side's 24 (each names a row below 8)
        Mem(at::kEventBattle)[0] = static_cast<unsigned char>(sh::Next() % 24);
        unsigned char* const rows = Mem(at::kEncounterRowsAt);
        for (unsigned i = 0; i < 0x48; ++i)
            if (sh::Half()) rows[i] = 0xFF;
        break;
    }
    case kMode8Run: Game_Step = static_cast<unsigned short>(sh::Next() % 4); break;
    case kMode9Run:
    case kMode10Run:
    case kMode11Run: Game_Step = static_cast<unsigned short>(sh::Next() % 3); break;
    case kMode8Enter:
    case kFrameBD:
    case kCellTexture:
    case kBuildView: Gfx_BufferIndex = static_cast<unsigned char>(sh::Next() & 1); break;
    case kMode8Leave:
    case kMode9Leave: {
        if (sh::Often()) MoveScript_WaitWordDA = 0;
        const U t = PickOf(0xFF, 3, sh::Next());
        Mem(at::kMusicTrack)[0] = static_cast<unsigned char>(t);
        Mem(at::kMusicPlaying)[0] = static_cast<unsigned char>(sh::Half() ? t : sh::Next());
        break;
    }
    case kMode9Enter:
    case kMode10Enter:
        if (sh::Often()) MoveScript_WaitWordDA = 0;
        break;
    case kMode11Frame: Field_Request = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next())); break;
    case kMode11LookEnd:
        if (sh::Often()) SetWord(Mem(at::kYaw), 0xFD56);
        if (sh::Often()) SetWord(Mem(at::kPitch), PickOf(0x200, 0x200, 0x1FF, 0x201));
        break;
    case kQuake: {
        SetWord(Mem(at::kQuakeX), sh::Next() % 0x40);
        SetWord(Mem(at::kQuakeY), sh::Next() % 0x40);
        break;
    }
    case kSwitch:
        Mem(at::kLeaderFacing)[0] = static_cast<unsigned char>(PickOf(3, 3, 2, sh::Next()));
        break;
    case kCharCount: {
        // a string: Latin and two-byte characters, its NUL anywhere, two NULs
        // at the end of the scratch so a skipped one cannot run past it
        unsigned char* const p = sh::Scratch(0);
        for (unsigned i = 0; i < 0x40; ++i) {
            const U n = sh::Next();
            p[i] = static_cast<unsigned char>(n % 5 == 0 ? 0x80 | (n >> 8) : 0x20 + (n >> 8) % 0x5F);
        }
        if (sh::Half()) p[sh::Next() % 0x3E] = 0;
        if (sh::Next() % 8 == 0) p[0] = 0;
        p[0x3E] = 0;
        p[0x3F] = 0;
        break;
    }
    default: break;
    }
    if (k == kFrameBD) {
        // the angles the view was drawn at: the same half the time, one word off otherwise
        unsigned char* const drawn = Mem(at::kAnglesDrawn);
        std::memcpy(drawn, Mem(0x929EC8), 8);
        if (!sh::Half()) drawn[(sh::Next() % 3) * 2] ^= static_cast<unsigned char>(1 + sh::Next() % 0xFF);
        if (sh::Half()) MapView_Redraw = 0;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case kPlaceOne: a[1] = (a[1] & 0xFFFFFF00u) | (a[1] % at::kEnemyCount); break;   // a count below the eight records
    case kQuake: {
        // each coordinate a cell of the block -2..0x22 (both bounds and past them), a fraction below 0x40
        const auto coord = [](U v, U corner) {
            const U cell = (v >> 8) % 0x25 - 2u + 2u * corner;
            return (cell << 6) - 0x4000u + (v & 0x3F);
        };
        a[0] = coord(a[0], static_cast<U>(static_cast<std::int16_t>(Word(Mem(at::kQuakeX)))));
        a[1] = coord(a[1], static_cast<U>(static_cast<std::int16_t>(Word(Mem(at::kQuakeY)))));
        if (sh::Next() % 8 == 0) a[sh::Next() & 1] = sh::Next();
        break;
    }
    case kSwitch:
        if (sh::Often()) a[0] = (a[0] & 0xFFFFFF00u) | 0x1C;
        if (sh::Often()) a[1] = (a[1] & 0xFFFFFF00u) | 6;
        break;
    case kPanels: a[0] = PickOf(0, 1, 4, 0x20, 0x3C, a[0] & 0xFF, a[0]); break;
    case kRings: a[0] = PickOf(0, 1, 0x10, 0x18, 0x28, 0x29, 0x30, 0xFFFFFFFFu, 0xFFFFFFF0u, 0xFFFFFFE8u, 0xFFFFFFD0u, a[0] % 0x60, a[0]); break;
    case kCellTexture: a[1] = Key(sh::Packets() + 8 * (a[1] % 0x40)); break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = S();
    switch (h % 12) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 1 + (v >> 1) % 8 : v >> 1);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) SetWord(s + 0x2E, (v & 1) ? 0x149u : v >> 1);
        break;
    case 2: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0xB : v >> 1); break;
    case 3: Mem(at::kMusicTrack)[0] = static_cast<unsigned char>((v & 1) ? Mem(at::kMusicPlaying)[0] : v >> 1); break;
    case 4: MapView_Redraw = static_cast<unsigned char>(v % 4); break;
    case 5: SetWord(Mem((v & 1) ? at::kYaw : at::kPitch), (v & 2) ? ((v & 1) ? 0xFD56u : 0x200u) : v >> 2); break;
    case 6: SetLong(Mem((v & 1) ? at::kRows : at::kColumns), static_cast<std::int32_t>((v >> 1) % 6)); break;   // a walk cut short
    case 7: DrawItemPool_Top = static_cast<unsigned short>((v & 1) ? 0x3FE + (v >> 1) % 4 : (v >> 1) % kItemsRegion); break;
    case 8: SetWord(Mem((v & 1) ? at::kOriginX : at::kOriginZ), v >> 1); break;
    case 9: Mem(at::kPlaced)[0] = static_cast<unsigned char>(v); break;
    case 10: SetWord(Mem(sh::at::kVertexScratch + 2 * ((v & 3) % 3)), v >> 2); break;
    case 11: PutFloat(Mem(0x903820 + 4 * (v & 1)), static_cast<float>(static_cast<int>((v >> 1) % 1000) - 500)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R3G_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R3G_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll31[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll31[k];
        }
    static unsigned* s_index = index;
    if (n != 0) {
        sh::Group g = {"rest_3g", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                       sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                       [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
        g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
        g.effect = true;
        g.kinds = kKinds;
        g.n_kinds = sizeof kKinds;
        sh::Run(g);
    }
    if (!only || !*only || std::strstr(kBuild[0].name, only)) {
        sh::Group g = {"rest_3g build", kBuild, 1, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                       kRegions, sizeof kRegions / sizeof kRegions[0], [](unsigned) { Seed(kBuildView); }, &Disturb, 1500};
        g.effect = true;
        g.kinds = kKinds;
        g.n_kinds = sizeof kKinds;
        sh::Run(g);
        n = 1;
    }
    if (n == 0) bof3::Fatal("rest_3g: BOF3X_R3G_ONLY=%s names no clone", only);
}

}  // namespace rest_3g
