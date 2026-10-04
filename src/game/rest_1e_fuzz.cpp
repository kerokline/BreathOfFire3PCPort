// BOF3X_SHADOW=rest_1e: group R1E's 47 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_1e.md section 4. BOF3X_R1E_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R1E --clones --harness scenario
// (2026-10-04, through the round's band14.py), each extent read again to its
// last instruction (capstone): every extent is the tool's (the cut's sizes
// include the nop padding). Shapes: the 42 states and dispatchers kSprite (void,
// no arguments, on Sprite_Current), the five cell helpers kCall (x, z) answering
// al (ret_mask 0xFF: every caller tests al first). The 28 state tables the
// dispatchers jump through are swapped for recorders; each dispatcher's index is
// drawn below its own table's length (none bounds it).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1e.h"
#include "game/rest_1e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1e {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/band_rows.py --group R1E --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCallsBegin522760[] = {{0x15, 0x51C390}, {0x2F, 0x51C390}, {0x72, 0x572570}, {0x8D, 0x5725C0},
                                              {0xB8, 0x589330}, {0xF6, 0x5725C0}, {0x115, 0x572570}, {0x14D, 0x5725C0},
                                              {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCallsBegin5230D0[] = {{0x15, 0x51C390}, {0x2F, 0x51C390}, {0x72, 0x572570}, {0x8D, 0x5725C0},
                                              {0xB8, 0x589330}, {0xF6, 0x5725C0}, {0x115, 0x572570}, {0x14D, 0x5725C0},
                                              {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCallsBegin523550[] = {{0x15, 0x51C390}, {0x2F, 0x51C390}, {0x72, 0x572570}, {0x8D, 0x5725C0},
                                              {0xB8, 0x589330}, {0xF6, 0x5725C0}, {0x115, 0x572570}, {0x14D, 0x5725C0},
                                              {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCalls522940[] = {{0x51, 0x531CF0}, {0x8F, 0x522A20}, {0xA5, 0x522A20}, {0xB9, 0x522A20}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls5232B0[] = {{0x51, 0x531CF0}, {0x8F, 0x523390}, {0xA5, 0x523390}, {0xB9, 0x523390}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls523730[] = {{0x51, 0x531CF0}, {0x8F, 0x523810}, {0xA5, 0x523810}, {0xB9, 0x523810}, {0xD2, 0x589410}};
// the three cell pickups, the same code at three addresses
constexpr sh::CallSite kCallsPickup[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCallsStrikeBegin[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls522C90[] = {{0x36, 0x587740}, {0x3E, 0x530530}, {0x4F, 0x587740}, {0xB5, 0x531CF0}, {0xEE, 0x587740},
                                         {0x100, 0x522E20}, {0x116, 0x522E20}, {0x12A, 0x522E20}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls523BD0[] = {{0x36, 0x587740}, {0x3E, 0x530530}, {0x4F, 0x587740}, {0xB5, 0x531CF0}, {0xEE, 0x587740},
                                         {0x100, 0x523D20}, {0x116, 0x523D20}, {0x12A, 0x523D20}, {0x143, 0x589410}};
// the two strike cells, the same code at two addresses
constexpr sh::CallSite kCallsStrikeCell[] = {{0xD, 0x536700},   {0x49, 0x522FB0},  {0x53, 0x587740},  {0x5B, 0x5B93D2},
                                             {0x74, 0x522FB0},  {0x7D, 0x591680},  {0xAD, 0x590BB0},  {0xBE, 0x587740},
                                             {0xC4, 0x497710},  {0xE3, 0x497710},  {0x108, 0x522FB0}, {0x10F, 0x534DB0},
                                             {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0}, {0x15C, 0x5B93D2},
                                             {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls523050[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls5239F0[] = {{0x3E, 0x5725C0}, {0x58, 0x572570}, {0x8F, 0x5725C0}, {0xA9, 0x572570}, {0xCF, 0x589330}};

#define R1E_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R1E_CALLS(a) a, R1E_N(a)
#define R1E_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape}
const sh::Clone kAll[] = {
    {"PartyAction_NoAction", 0x5226D0, 0xD, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction_NoAction), 0, false, kSp},
    {"PartyFormAction13_Form1", 0x5226E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction13_Form1), 0, false, kSp},
    {"PartyAction13_Form1", 0x522700, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_Form1), 0, false, kSp},
    {"PartyFormAction13_Form2", 0x522720, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction13_Form2), 0, false, kSp},
    {"PartyAction13_Form2", 0x522740, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_Form2), 0, false, kSp},
    {"PartyAction13_Form2Begin", 0x522760, 0x1D4, R1E_CALLS(kCallsBegin522760), nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_Form2Begin), 0, false, kSp},
    {"PartyAction13_Form2Resolve", 0x522940, 0xDB, R1E_CALLS(kCalls522940), nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_Form2Resolve), 0, false, kSp},
    {"PartyAction13_CellPickup", 0x522A20, 0x11F, R1E_CALLS(kCallsPickup), nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction13_ByForm", 0x522B40, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction13_ByForm), 0, false, kSp},
    {"PartyAction13_ByForm", 0x522B60, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction13_ByForm), 0, false, kSp},
    {"PartyFormAction14_Form0", 0x522B80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction14_Form0), 0, false, kSp},
    {"PartyFormAction14_Form1", 0x522BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction14_Form1), 0, false, kSp},
    {"PartyAction14_Form1", 0x522BC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form1), 0, false, kSp},
    {"PartyAction14_Form1Mode0", 0x522BE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form1Mode0), 0, false, kSp},
    {"PartyAction14_StrikeBegin", 0x522C00, 0x87, R1E_CALLS(kCallsStrikeBegin), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_StrikeBegin), 0, false, kSp},
    {"PartyAction14_Strike", 0x522C90, 0x14C, R1E_CALLS(kCalls522C90), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Strike), 0, false, kSp},
    {"PartyAction_StrikeWait", 0x522DE0, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction_StrikeWait), 0, false, kSp},
    {"PartyAction14_StrikeCell", 0x522E20, 0x188, R1E_CALLS(kCallsStrikeCell), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_StrikeCell), 0xFFu, false, kCa},
    {"PartyAction14_Form1Mode1", 0x523030, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form1Mode1), 0, false, kSp},
    {"PartyAction14_Mode1Begin", 0x523050, 0x35, R1E_CALLS(kCalls523050), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Mode1Begin), 0, false, kSp},
    {"PartyFormAction14_Form2", 0x523090, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction14_Form2), 0, false, kSp},
    {"PartyAction14_Form2", 0x5230B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form2), 0, false, kSp},
    {"PartyAction14_Form2Begin", 0x5230D0, 0x1D4, R1E_CALLS(kCallsBegin5230D0), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form2Begin), 0, false, kSp},
    {"PartyAction14_Form2Resolve", 0x5232B0, 0xDB, R1E_CALLS(kCalls5232B0), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_Form2Resolve), 0, false, kSp},
    {"PartyAction14_CellPickup", 0x523390, 0x11F, R1E_CALLS(kCallsPickup), nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction14_ByForm", 0x5234B0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction14_ByForm), 0, false, kSp},
    {"PartyAction14_ByForm", 0x5234D0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction14_ByForm), 0, false, kSp},
    {"PartyFormAction15_Form0", 0x5234F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction15_Form0), 0, false, kSp},
    {"PartyFormAction15_Form1", 0x523510, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction15_Form1), 0, false, kSp},
    {"PartyAction15_Form1", 0x523530, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_Form1), 0, false, kSp},
    {"PartyAction15_Form1Begin", 0x523550, 0x1D4, R1E_CALLS(kCallsBegin523550), nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_Form1Begin), 0, false, kSp},
    {"PartyAction15_Form1Resolve", 0x523730, 0xDB, R1E_CALLS(kCalls523730), nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_Form1Resolve), 0, false, kSp},
    {"PartyAction15_CellPickup", 0x523810, 0x11F, R1E_CALLS(kCallsPickup), nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction15_Form2", 0x523930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction15_Form2), 0, false, kSp},
    {"PartyAction15_Form2", 0x523950, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_Form2), 0, false, kSp},
    {"PartyFormAction15_ByForm", 0x523970, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction15_ByForm), 0, false, kSp},
    {"PartyAction15_ByForm", 0x523990, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction15_ByForm), 0, false, kSp},
    {"PartyFormAction16_Form0", 0x5239B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction16_Form0), 0, false, kSp},
    {"PartyAction16_Form0", 0x5239D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_Form0), 0, false, kSp},
    {"PartyAction_ProbeBegin", 0x5239F0, 0xE7, R1E_CALLS(kCalls5239F0), nullptr, 0, nullptr, 0, R1E_FN(PartyAction_ProbeBegin), 0, false, kSp},
    {"PartyFormAction16_Form1", 0x523AE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyFormAction16_Form1), 0, false, kSp},
    {"PartyAction16_Form1", 0x523B00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_Form1), 0, false, kSp},
    {"PartyAction16_Form1Mode0", 0x523B20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_Form1Mode0), 0, false, kSp},
    {"PartyAction16_StrikeBegin", 0x523B40, 0x87, R1E_CALLS(kCallsStrikeBegin), nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_StrikeBegin), 0, false, kSp},
    {"PartyAction16_Strike", 0x523BD0, 0x14C, R1E_CALLS(kCalls523BD0), nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_Strike), 0, false, kSp},
    {"PartyAction16_StrikeCell", 0x523D20, 0x188, R1E_CALLS(kCallsStrikeCell), nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_StrikeCell), 0xFFu, false, kCa},
    {"PartyAction16_Form1Mode1", 0x523EB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1E_FN(PartyAction16_Form1Mode1), 0, false, kSp},
};
#undef R1E_FN
#undef R1E_CALLS
#undef R1E_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 47, "the cut's 47 rows");

// What each clone dispatches on, for the seed: the byte +2 or +3 or the word
// +0x2C below its table's length; 0 for the states and helpers. In kAll's order.
struct Index { std::uint8_t by, entries; };
constexpr Index kIndex[kCount] = {
    {0, 0},    {2, 3}, {2, 3}, {2, 3}, {2, 3}, {0, 0}, {0, 0}, {0, 0}, {0x2C, 3}, {0x2C, 3},   // 0x5226D0..0x522B60
    {2, 3},    {2, 3}, {2, 2}, {3, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {3, 3},               // 0x522B80..0x523030
    {0, 0},    {2, 3}, {2, 3}, {0, 0}, {0, 0}, {0, 0}, {0x2C, 3}, {0x2C, 3},                 // 0x523050..0x5234D0
    {2, 3},    {2, 3}, {2, 3}, {0, 0}, {0, 0}, {0, 0}, {2, 3}, {2, 2}, {0x2C, 3}, {0x2C, 3}, // 0x5234F0..0x523990
    {2, 3},    {2, 3}, {0, 0}, {2, 3}, {2, 2}, {3, 5}, {0, 0}, {0, 0}, {0, 0}, {3, 3},       // 0x5239B0..0x523EB0
};

// The 28 tables (symbols.toml [[data]]), each its own length.
const sh::DataTable kTables[] = {
    {0x65FEC4, 3}, {0x65FED0, 3}, {0x65FEDC, 3}, {0x65FEE8, 3}, {0x65FEF4, 3}, {0x65FF00, 3}, {0x65FF0C, 3},
    {0x65FF18, 3}, {0x65FF24, 2}, {0x65FF2C, 5}, {0x65FF40, 3}, {0x65FF4C, 3}, {0x65FF58, 3}, {0x65FF64, 3},
    {0x65FF70, 3}, {0x65FF7C, 3}, {0x65FF88, 3}, {0x65FF94, 3}, {0x65FFA0, 3}, {0x65FFAC, 2}, {0x65FFB4, 3},
    {0x65FFC0, 3}, {0x65FFCC, 3}, {0x65FFD8, 3}, {0x65FFE4, 3}, {0x65FFF0, 2}, {0x65FFF8, 5}, {0x66000C, 3},
};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Steps() { return sh::Mem(at::kSteps); }
unsigned char* Effect(unsigned i) { return sh::EffectRecord(i); }

// --- the stand-ins' answers (Noise() and the state only: both passes the same) -----

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// AreaMap_ByteAt: every code the cell helpers compare with, and neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF2, 0xF2, 0xF2, 0xF8, 0xF8, 0xF0, 0xF1, 0xF4, 0xF6, 0xF6, 0xF7, 0xF7,
                               0xF3, 0xF5, 0xF9, 0xEF, 0x00, 0xFF};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// Sprite_ObjectAt: none half the time, else 0..0x21 (the 30 sprites and the
// extra four: what the real one answers; the states index the records by it).
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 0x22);
}
// Field_EffectAhead: none half the time, else a record 0..19 (what the real
// one answers; the strike indexes Effect_Objects by it).
U FxEffectAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 20);
}
// MapView_SlopeAt: DamageScratch's flag 0 a third of the time; the slope's low
// word on either side of 0x40.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = sh::Mem(bof3::addr::DamageScratch);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// MapView_GroundAt: a low word about the sprite's height (the side probes'
// test) and about the height + 0x40 (the Begin states' rise test).
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0, 1, 0xFFFF, 0x40, 0x41, 0x3F, 0x42, 0x8000, 0x7FFF, 0x8040, 0x8041};
    return WithAx(answer, n % 6 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}
// Rand: half the time a low nibble at the cell helpers' boundaries (the
// pickup's 0xD / 0xF, the strike's 7 / 0xB / 0xC, & 3 zero, & 7 at 5 / 6), else
// any; garbage above (each caller masks al).
U FxRand(const U*, U answer) {
    static const U kNibbles[] = {0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0xB, 0xC, 0xD, 0xE, 0xF};
    const U n = sh::Noise();
    return n % 2 ? answer : WithAl(answer, (n & 0xF0) | kNibbles[(n >> 8) % (sizeof kNibbles / sizeof kNibbles[0])]);
}
// PartyAction_SideProbes writes +0x2B (1, then 0 on a steep side).
U FxSideProbes(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s + 0x2B, 1)) s[0x2B] = static_cast<unsigned char>(sh::Noise() % 3 == 0 ? 0 : 1);
    return answer;
}

// The group's own callees and those the standard set records otherwise. The
// cell helpers are called by Resolve / Strike with dwords whose upper halves
// are the original's uninitialised stack: 16 bits each, what every callee of
// theirs reads (docs/rest_1e.md section 2). Elsewhere the helpers pass their
// arguments on whole, the same dwords on both passes: masks whole.
#define R1E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own cell helpers: unsigned char (unsigned x, unsigned z)
    {R1E_OURS(PartyAction13_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1E_OURS(PartyAction14_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1E_OURS(PartyAction15_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1E_OURS(PartyAction14_StrikeCell), 2, {kU16, kU16}, kF, 0, 0},
    {R1E_OURS(PartyAction16_StrikeCell), 2, {kU16, kU16}, kF, 0, 0},
    // R0A's: unsigned char (void) x 2, void (void), void (state, x, z) (the
    // byte and two movsx words it reads; the callers push a fourth dword)
    {R1E_OURS(PartyAction_TargetAhead), 0, {}, kF, 0, 0},
    {R1E_OURS(PartyAction_BlockedAhead), 0, {}, kF, 0, 0},
    {R1E_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0, {}, &FxSideProbes},
    {R1E_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0},
    // void (unsigned state, unsigned x, unsigned z): the byte, two movsx words
    {R1E_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0},
    // void (unsigned amount)
    {R1E_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0},
    // void (unsigned x, unsigned z)
    {R1E_OURS(AreaMap_ClearCell), 2, {kW, kW}, kG, 0, 0},
    // unsigned char (short x, short y)
    {R1E_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    // unsigned char (long x, long y, unsigned margin)
    {R1E_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    // unsigned char (void)
    {R1E_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxEffectAhead},
    // long (long x, long y, unsigned long direction): the Begin states push eax
    // whole (Sprite_Current's address under the direction byte), and
    // AreaMap_Slope reads the dword's upper bytes for a direction of 8 or more
    {R1E_OURS(MapView_SlopeAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxSlope},
    // long (long x, long z)
    {R1E_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    // void (unsigned colour): its low byte (symbols.toml)
    {R1E_OURS(Sprite_FlashClut), 1, {kU8}, kG, 0, 0},
    // int (void): Capcom's C runtime (its name is its address)
    {"Rand", 0x5B93D2, 0x5B93D2, 0, {}, kG, 0, 0, {}, &FxRand},
};
#undef R1E_OURS

// Beyond field mode's standard regions: Field_DirectionSteps (every state that
// steps reads it) and Text_Records' first 16 bytes (the item name copied).
const sh::Region kRegions[] = {
    {at::kSteps, 0x40},
    {bof3::addr::Text_Records, 0x10},
};

// --- the seed -------------------------------------------------------------------------

U Coordinate() {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
U Height() {
    return PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x7FC0, 0xFFC0, 0x40, sh::Next() % 0x400u, sh::Next());
}
void SeedSteps() {
    unsigned char* const t = Steps();
    for (unsigned i = 0; i < 16; ++i) {
        const U v = sh::Half() ? PickOf(0, 0x8000, 0xFFFF8000u)
                               : PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u, 0x4000, 1, 0xFFFFFFFFu, 0x7FFFFFFFu,
                                        0x80000000u, sh::Next());
        SetLong(t + 4 * i, static_cast<std::int32_t>(v));
    }
}
// 0..7 mostly (even and odd: the turns), else a byte past the table (read in
// place, the same on both passes).
unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }

void SeedSprite() {
    unsigned char* const s = Sprite_Current;
    s[8] = Direction();
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    SetWord(s + 0x3E, Height());
    // the countdown the resolve and strike states take to 0 (1), already 0, or more
    s[0xA] = static_cast<unsigned char>(PickOf(1, 1, 1, 0, 2, 0xFF, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    s[0x2B] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    // the effect record the strike wait reads (0..19; past them ours aborts)
    s[6] = static_cast<unsigned char>(sh::Next() % 20);
    SetWord(s + 0x2C, PickOf(0, 1, 2, 0xFF00, 0xFFFF, sh::Next()));
}
void SeedEffects() {
    for (unsigned i = 0; i < 20; ++i) {
        unsigned char* const e = Effect(i);
        e[0] = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
        e[1] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, sh::Next()));
    }
}

void Seed(unsigned k) {
    SeedSteps();
    SeedSprite();
    SeedEffects();
    Field_Kind2Hold = static_cast<unsigned char>(PickOf(0, 0, 0, 1, sh::Next()));
    Field_InputFlags = static_cast<unsigned char>(PickOf(0, 2, 4, 6, sh::Next()));
    unsigned char* const s = Sprite_Current;
    const Index ix = kIndex[k];
    if (ix.by == 0x2C) SetWord(s + 0x2C, sh::Next() % ix.entries);
    else if (ix.by != 0) s[ix.by] = static_cast<unsigned char>(sh::Next() % ix.entries);
}

// The cell helpers' arguments: a cell 0..0x7F mostly, at the s16 / u16 limits
// otherwise, under random upper halves (the dwords are passed on whole).
void Args(unsigned k, U* a) {
    if (kAll[k].shape != kCa) return;
    for (unsigned i = 0; i < 2; ++i)
        a[i] = (a[i] & 0xFFFF0000u) | (PickOf(sh::Next() % 0x80u, sh::Next() % 0x80u, 0, 0x7FFF, 0x8000, 0xFFFF, sh::Next()) & 0xFFFF);
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// position, height, +0x2B, +0xA, +0xB, the form word +0x2C; the scratch flag;
// an effect record's +0 / +1; Field_Kind2Hold; Field_State +0x89; a dword of
// Field_DirectionSteps; Field_InputFlags.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x40)) return;
    switch (h % 12) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 2: SetWord(s + 0x3E, v >> 2); break;
    case 3: s[0x2B] = static_cast<unsigned char>(v); break;
    case 4: s[0xA] = static_cast<unsigned char>(v & 1 ? 1 : v >> 1); break;
    case 5: SetWord(s + 0x2C, v >> 3); break;
    case 6: sh::Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 7: {
        unsigned char* const e = Effect(v % 20);
        e[(v >> 5) & 1] = static_cast<unsigned char>((v >> 6) & 1 ? 1 : v >> 7);
        break;
    }
    case 8: Field_Kind2Hold = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 9: {
        unsigned char* const state = Field_State;
        if (sh::InRegions(state + 0x89, 1)) state[0x89] = static_cast<unsigned char>(v);
        break;
    }
    case 10: SetLong(Steps() + 4 * (v % 16), static_cast<std::int32_t>(v << 6)); break;
    case 11: Field_InputFlags = static_cast<unsigned char>(v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_1e: BOF3X_R1E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_1e
