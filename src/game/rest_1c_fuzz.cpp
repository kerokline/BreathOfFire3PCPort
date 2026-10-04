// BOF3X_SHADOW=rest_1c: group R1C's 51 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up:
// 46 kSprite state handlers and dispatchers run on Sprite_Current, five kCall
// helpers (x, z) answering in al. docs/rest_1c.md section 4.
// BOF3X_R1C_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R1C --clones --harness scenario
// (2026-10-04, through the round's band14.py), each extent read again to its
// last instruction (capstone). The 28 dispatch tables the dispatchers jump
// through are swapped for recorders while the fuzz runs; each dispatcher's
// index (+2, +3 or u16 +0x2C) is drawn below its own table's length. Every
// other callee is a stand-in of the group's own, registered before the
// harness's standard rows (the group's listing stands), or a standard row.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_1c.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1c {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/band_rows.py --group R1C --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCalls51F210[] = {{0x14, 0x51C390}, {0x2E, 0x51C390}, {0x77, 0x5725C0}, {0xA5, 0x589330}, {0xE2, 0x5725C0},
                                         {0xFC, 0x572570}, {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51F3D0[] = {{0x51, 0x531CF0}, {0x8F, 0x51F4B0}, {0xA5, 0x51F4B0}, {0xB9, 0x51F4B0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51F4B0[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51F670[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls51F700[] = {{0x36, 0x587740},  {0x3E, 0x530530},  {0x4F, 0x587740},  {0xB5, 0x531CF0}, {0xEE, 0x587740},
                                         {0x100, 0x51F880}, {0x116, 0x51F880}, {0x12A, 0x51F880}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls51F850[] = {{0x0, 0x589410}, {0x12, 0x5891F0}};
constexpr sh::CallSite kCalls51F880[] = {{0xD, 0x536700},   {0x49, 0x522FB0},  {0x53, 0x587740},  {0x5B, 0x5B93D2},  {0x74, 0x522FB0},
                                         {0x7D, 0x591680},  {0xAD, 0x590BB0},  {0xBE, 0x587740},  {0xC4, 0x497710},  {0xE3, 0x497710},
                                         {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0},
                                         {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls51FA30[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls51FAF0[] = {{0x3, 0x521510},  {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740},
                                         {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls51FC80[] = {{0x25, 0x589330}, {0x77, 0x589330}};
constexpr sh::CallSite kCalls51FD40[] = {{0x15, 0x51C390},  {0x2F, 0x51C390},  {0x72, 0x572570},  {0x8D, 0x5725C0},
                                         {0xB8, 0x589330},  {0xF6, 0x5725C0},  {0x115, 0x572570}, {0x14D, 0x5725C0},
                                         {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCalls51FF20[] = {{0x51, 0x531CF0}, {0x8F, 0x520000}, {0xA5, 0x520000}, {0xB9, 0x520000}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls520000[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls5201A0[] = {{0x3, 0x521510},  {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740},
                                         {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls520350[] = {{0xC, 0x589410}, {0x1F, 0x589330}, {0x31, 0x589410}};
constexpr sh::CallSite kCalls520400[] = {{0x15, 0x51C390},  {0x2F, 0x51C390},  {0x72, 0x572570},  {0x8D, 0x5725C0},
                                         {0xB8, 0x589330},  {0xF6, 0x5725C0},  {0x115, 0x572570}, {0x14D, 0x5725C0},
                                         {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCalls5205E0[] = {{0x51, 0x531CF0}, {0x8F, 0x5206C0}, {0xA5, 0x5206C0}, {0xB9, 0x5206C0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls5206C0[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls520840[] = {{0x24, 0x52F570}, {0x43, 0x52F570}};
constexpr sh::CallSite kCalls5208D0[] = {{0x3, 0x521510},  {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740},
                                         {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls520AA0[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls520B30[] = {{0x36, 0x587740},  {0x3E, 0x530530},  {0x4F, 0x587740},  {0xB5, 0x531CF0}, {0xEE, 0x587740},
                                         {0x100, 0x520C80}, {0x116, 0x520C80}, {0x12A, 0x520C80}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls520C80[] = {{0xD, 0x536700},   {0x49, 0x522FB0},  {0x53, 0x587740},  {0x5B, 0x5B93D2},  {0x74, 0x522FB0},
                                         {0x7D, 0x591680},  {0xAD, 0x590BB0},  {0xBE, 0x587740},  {0xC4, 0x497710},  {0xE3, 0x497710},
                                         {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0},
                                         {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};

// What each clone is, for the seed: a dispatcher by u16 +0x2C, by +2 or by +3
// (with its table's length), a state handler, or a helper (x, z).
enum class Kind : std::uint8_t { kForm, kState, kStep, kHandler, kHelper };
struct Row {
    sh::Clone clone;
    Kind kind;
    std::uint8_t span;   // a dispatcher's table length
};

#define R1C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R1C_CALLS(a) a, R1C_N(a)
#define R1C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite;
constexpr sh::Shape kCa = sh::Shape::kCall;
// A dispatcher: no call, one jmp through its table.
#define R1C_DISPATCH(name, base, size, kind, span) \
    {{#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R1C_FN(name), 0, false, kSp}, kind, span}
// A state handler (void) and a helper (al: every caller tests al only).
#define R1C_STATE(name, base, size, calls) {{#name, base, size, R1C_CALLS(calls), nullptr, 0, nullptr, 0, R1C_FN(name), 0, false, kSp}, Kind::kHandler, 0}
#define R1C_HELPER(name, base, size, calls) {{#name, base, size, R1C_CALLS(calls), nullptr, 0, nullptr, 0, R1C_FN(name), 0xFFu, false, kCa}, Kind::kHelper, 0}
const Row kRows[] = {
    R1C_STATE(PartyAction6_Form0Begin, 0x51F210, 0x1B1, kCalls51F210),
    R1C_STATE(PartyAction6_Form0Resolve, 0x51F3D0, 0xDB, kCalls51F3D0),
    R1C_HELPER(PartyAction6_CellPickup, 0x51F4B0, 0x11F, kCalls51F4B0),
    R1C_DISPATCH(PartyFormAction6_Form1, 0x51F5D0, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction6_Form1, 0x51F5F0, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyFormAction6_Form2, 0x51F610, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction6_Form2, 0x51F630, 0x12, Kind::kState, 2),
    R1C_DISPATCH(PartyAction6_Form2State0, 0x51F650, 0x12, Kind::kStep, 5),
    R1C_STATE(PartyAction6_Form2Begin, 0x51F670, 0x87, kCalls51F670),
    R1C_STATE(PartyAction6_Form2Resolve, 0x51F700, 0x14C, kCalls51F700),
    R1C_STATE(PartyAction_TickThenFace, 0x51F850, 0x23, kCalls51F850),
    R1C_HELPER(PartyAction6_CellHit, 0x51F880, 0x188, kCalls51F880),
    R1C_DISPATCH(PartyAction6_Form2State1, 0x51FA10, 0x12, Kind::kStep, 3),
    R1C_STATE(PartyAction6_Form2Probe, 0x51FA30, 0x35, kCalls51FA30),
    R1C_DISPATCH(PartyFormAction6_ByForm, 0x51FA70, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyAction6_ByForm, 0x51FA90, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyFormAction7_Form0, 0x51FAB0, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction7_Form0, 0x51FAD0, 0x12, Kind::kState, 2),
    R1C_STATE(PartyAction7_Form0Begin, 0x51FAF0, 0x166, kCalls51FAF0),
    R1C_DISPATCH(PartyFormAction7_Form1, 0x51FC60, 0x12, Kind::kState, 3),
    R1C_STATE(PartyAction_TurnStep, 0x51FC80, 0x7E, kCalls51FC80),
    R1C_DISPATCH(PartyFormAction7_Form2, 0x51FD00, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction7_Form2, 0x51FD20, 0x12, Kind::kState, 3),
    R1C_STATE(PartyAction7_Form2Begin, 0x51FD40, 0x1D4, kCalls51FD40),
    R1C_STATE(PartyAction7_Form2Resolve, 0x51FF20, 0xDB, kCalls51FF20),
    R1C_HELPER(PartyAction7_CellPickup, 0x520000, 0x11F, kCalls520000),
    R1C_DISPATCH(PartyFormAction7_ByForm, 0x520120, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyAction7_ByForm, 0x520140, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyFormAction8_Form0, 0x520160, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction8_Form0, 0x520180, 0x12, Kind::kState, 2),
    R1C_STATE(PartyAction8_Form0Begin, 0x5201A0, 0x166, kCalls5201A0),
    R1C_DISPATCH(PartyFormAction8_Form1, 0x520310, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction8_Form1, 0x520330, 0x12, Kind::kState, 3),
    R1C_STATE(PartyAction_WaitEffect, 0x520350, 0x63, kCalls520350),
    R1C_DISPATCH(PartyFormAction8_Form2, 0x5203C0, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction8_Form2, 0x5203E0, 0x12, Kind::kState, 3),
    R1C_STATE(PartyAction8_Form2Begin, 0x520400, 0x1D4, kCalls520400),
    R1C_STATE(PartyAction8_Form2Resolve, 0x5205E0, 0xDB, kCalls5205E0),
    R1C_HELPER(PartyAction8_CellPickup, 0x5206C0, 0x11F, kCalls5206C0),
    R1C_DISPATCH(PartyFormAction8_ByForm, 0x5207E0, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyAction8_ByForm, 0x520800, 0x13, Kind::kForm, 3),
    R1C_DISPATCH(PartyFormAction9_Form0, 0x520820, 0x12, Kind::kState, 3),
    R1C_STATE(PartyAction_TurnToSide, 0x520840, 0x70, kCalls520840),
    R1C_DISPATCH(PartyAction9_Form0, 0x5208B0, 0x12, Kind::kState, 2),
    R1C_STATE(PartyAction9_Form0Begin, 0x5208D0, 0x166, kCalls5208D0),
    R1C_DISPATCH(PartyFormAction9_Form1, 0x520A40, 0x12, Kind::kState, 3),
    R1C_DISPATCH(PartyAction9_Form1, 0x520A60, 0x12, Kind::kState, 2),
    R1C_DISPATCH(PartyAction9_Form1State0, 0x520A80, 0x12, Kind::kStep, 5),
    R1C_STATE(PartyAction9_Form1Begin, 0x520AA0, 0x87, kCalls520AA0),
    R1C_STATE(PartyAction9_Form1Resolve, 0x520B30, 0x14C, kCalls520B30),
    R1C_HELPER(PartyAction9_CellHit, 0x520C80, 0x188, kCalls520C80),
};
#undef R1C_HELPER
#undef R1C_STATE
#undef R1C_DISPATCH
#undef R1C_FN
#undef R1C_CALLS
#undef R1C_N
constexpr unsigned kCount = sizeof kRows / sizeof kRows[0];
static_assert(kCount == 51, "the 51 functions of rest_1c.cpp");

// The 28 dispatch tables (docs/rest_1c.md section 3: each one's length is the
// run of handlers up to the next table, read by hand; no reader bounds its
// index). Swapped for handler recorders while the fuzz runs.
const sh::DataTable kTables[] = {
    {0x65FC3C, 3}, {0x65FC48, 3}, {0x65FC54, 3}, {0x65FC60, 2}, {0x65FC68, 5}, {0x65FC7C, 3}, {0x65FC88, 3},
    {0x65FC94, 3}, {0x65FCA0, 3}, {0x65FCAC, 2}, {0x65FCB4, 3}, {0x65FCC0, 3}, {0x65FCCC, 3}, {0x65FCD8, 3},
    {0x65FCE4, 3}, {0x65FCF0, 3}, {0x65FCFC, 2}, {0x65FD04, 3}, {0x65FD10, 3}, {0x65FD1C, 3}, {0x65FD28, 3},
    {0x65FD34, 3}, {0x65FD40, 3}, {0x65FD4C, 3}, {0x65FD58, 2}, {0x65FD60, 3}, {0x65FD6C, 2}, {0x65FD74, 5},
};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
constexpr U kSteps = 0x6697B0;        // Field_DirectionSteps: 8 rows of two longs
constexpr U kTextRecords = 0x904CE0;  // Text_Records: the item name copies
unsigned char* Steps() { return sh::Mem(kSteps); }

// --- the stand-ins' answers (Noise() and the state only: both passes the same) -----

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// PartyAction_TargetAhead / BlockedAhead: 0 half the time, else 1 or any byte.
U FxAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0 : (n >> 8) % 4 ? 1 : n >> 16);
}
// PartyAction_Kind30Ahead: none (0xFF) a third of the time, else 0..19 (all it answers).
U FxKind30(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 3 == 0 ? 0xFF : (n >> 4) % 20);
}
// Field_EffectAhead: none half the time, else a record 0..19 (all it answers).
U FxEffectAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 20);
}
// Sprite_ObjectAt: none half the time, else an object 0..0x21 (the 30 and the
// four extra; all it answers), the boundary 0x1D / 0x1E often. (From Noise()
// only: Pick draws the seed's stream, which the two passes do not share.)
U FxObjectAt(const U*, U answer) {
    static const U kEdges[] = {0, 0x1D, 0x1E, 0x21};
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 3 == 0 ? kEdges[(n >> 10) % 4] : (n >> 12) % 0x22);
}
// AreaMap_ByteAt: each code the pickups and the cell hits compare with, and
// their neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xEF, 0x00, 0xFF};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// MapView_SlopeAt: AreaMap_Slope's "sloped" byte 0x903850 0 a third of the
// time, else 1 or any non-zero byte; the slope's low word on either side of
// 0x40 and at the s16 limits.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = sh::Mem(bof3::addr::DamageScratch);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0, 0x1000};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// MapView_GroundAt: a low word at, around, or 0x40 above the sprite's height
// word +0x3E (read now: the same on both passes) - the side probes compare the
// ground with the height, the ground-rise test (0x51FD40) the rise with 0x40.
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0, 1, 0xFFFF, 0x40, 0x41, 0x3F, 0x8040, 0x8000, 0x7FFF, 0x7FC0};
    return WithAx(answer, n % 5 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}

// Masks by what each callee reads (symbols.toml's types, their evidence): the
// map cells as 16-bit words (AreaMap_ByteAt sign-extends both; the pickups,
// the cell hits and Effect_SpawnAtCell / SpawnAtCellHigh read the cells as
// movsx words, AreaMap_ClearCell as field_hidden.md section 3 has it); the
// effect index of PartyAction_MemberOnEffect / BeyondEffect its low byte (the
// originals pass a dword whose upper bytes are their caller's ecx); the
// slope's direction its low byte (AreaMap_Slope reads only that byte: the
// originals push a whole register, 0x51FD40 one holding Sprite_Current's upper
// bytes); every other argument whole.
#define R1C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // round fourteen's stage A (R0A), ours
    {R1C_OURS(PartyAction_TargetAhead), 0, {}, kG, 0, 0, {}, &FxAhead},
    {R1C_OURS(PartyAction_BlockedAhead), 0, {}, kG, 0, 0, {}, &FxAhead},
    {R1C_OURS(PartyAction_Kind30Ahead), 0, {}, kG, 0, 0, {}, &FxKind30},
    {R1C_OURS(PartyAction_MemberOnEffect), 1, {kU8}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction_MemberBeyondEffect), 1, {kU8}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0, {}},
    // void (unsigned state, unsigned x, unsigned z): the state's byte, the words
    {R1C_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    {R1C_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    // void (unsigned amount): the byte product, pushed whole
    {R1C_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0, {}},
    // void (unsigned x, unsigned z)
    {R1C_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, kG, 0, 0, {}},
    // unsigned char (void): 0..19 or 0xFF
    {R1C_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxEffectAhead},
    // unsigned char (long x, long y, unsigned margin): 0..0x21 or 0xFF
    {R1C_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    // void (unsigned colour): the low byte indexes four words (0 pushed here)
    {R1C_OURS(Sprite_FlashClut), 1, {kU8}, kG, 0, 0, {}},
    // unsigned char (unsigned target): the callers store al, whatever it is
    {R1C_OURS(Sprite_TurnSense), 1, {kW}, kG, 0, 0, {}},
    // unsigned char (short x, short y)
    {R1C_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    // long (long x, long y, unsigned long direction)
    {R1C_OURS(MapView_SlopeAt), 3, {kW, kW, kU8}, kG, 0, 0, {}, &FxSlope},
    // long (long x, long z)
    {R1C_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    // the group's own, called by E8 from its others: unsigned char (x, z), the
    // cells' low words (each passes them to AreaMap_ByteAt and the spawns)
    {R1C_OURS(PartyAction6_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction7_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction8_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction6_CellHit), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1C_OURS(PartyAction9_CellHit), 2, {kU16, kU16}, kF, 0, 0, {}},
};
#undef R1C_OURS

// Beyond field mode's standard regions (Sprite_Current and the sprite records,
// Sprite_ObjectsExtra, ObjTrio and Field_State, Effect_Objects, DamageScratch's
// first bytes, Field_ScriptFlags, Field_InputFlags, Field_Request):
// Field_DirectionSteps, which the steps read (rows 3 and 5 by address), so that
// the seed can put every row's boundaries in (.data, restored after the run as
// every region is); Text_Records, where the item names are copied.
const sh::Region kRegions[] = {
    {kSteps, 0x40},
    {kTextRecords, 0x20},
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
// A direction byte: 0..7 mostly (odd and even: the first turn takes the even);
// above 7 reads the .data after Field_DirectionSteps in place, the same on
// both passes.
unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }

// Field_State: the bytes the handlers read and write (+0x89 the member,
// +0x128, +0x137, +0x138 bit 0), inside its ObjTrio record.
void SeedFieldState() {
    unsigned char* const f = Field_State;
    if (!sh::InRegions(f + 0x138, 1)) return;
    f[0x138] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 0xFE, sh::Next()));
    f[0x89] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
}

void Seed(unsigned k) {
    const Row& r = kRows[k];
    SeedSteps();
    SeedFieldState();
    unsigned char* const s = Sprite_Current;
    s[8] = Direction();
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    SetWord(s + 0x3E, Height());
    SetWord(s + 0x2C, PickOf(0, 1, 2, 0x10, 0xFF, 0xFFFF, sh::Next()));
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 5, 0xB, 0xFF, sh::Next()));
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 3, 0xFF, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, 7, 0xFF, 1, 0xFF, sh::Next()));
    s[7] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    Field_InputFlags = static_cast<unsigned char>(PickOf(0, 2, 4, 6, 1, 8, sh::Next()));
    switch (r.kind) {
    case Kind::kForm: SetWord(s + 0x2C, sh::Next() % r.span); break;
    case Kind::kState: s[2] = static_cast<unsigned char>(sh::Next() % r.span); break;
    case Kind::kStep: s[3] = static_cast<unsigned char>(sh::Next() % r.span); break;
    default: break;
    }
    // PartyAction_TurnStep: the side +3 at 3 or 5 (or another), +8 at it half the time
    if (r.clone.base == 0x51FC80) {
        s[3] = static_cast<unsigned char>(PickOf(3, 5, 3, 5, 4, 0, sh::Next()));
        if (sh::Half()) s[8] = s[3];
    }
    // PartyAction_WaitEffect: the effect index a record mostly (0xFF and past the
    // 20 read in place), the record in use or free
    if (r.clone.base == 0x520350) {
        s[0xB] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 20 : PickOf(0xFF, 20, 21, sh::Next()));
        for (unsigned i = 0; i < 20; ++i) sh::EffectRecord(i)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    }
    // Rand's nibble at the draws' edges: the pickups' 0xC..0xF and & 3, the
    // cell hits' & 0xF at 6 / 7 / 0xB / 0xC and & 7 at 5 / 6
    sh::SetRandHint(PickOf(0xC, 0xD, 0xE, 0xF, 0x0, 0x6, 0x7, 0xB, 0x5, 0x4, 0x3));
}

// The helpers' (x, z): each low word a cell at 0, 1, the s16 and u16 limits
// (where the cell one on wraps) or random, under random upper halves.
void Args(unsigned k, U* a) {
    if (kRows[k].kind != Kind::kHelper) return;
    a[0] = (a[0] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40, sh::Next()) & 0xFFFF);
    a[1] = (a[1] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40, sh::Next()) & 0xFFFF);
}

// What the handlers read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// position, height, form word and the bytes +6, +7, +9, +0xA, +0xB, +0x2B;
// Field_State's bytes; an effect record's in-use byte (the one +0xB names, for
// PartyAction_WaitEffect); the sloped flag; a row of Field_DirectionSteps.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 12) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 2: SetWord(s + 0x3E, v >> 2); break;
    case 3: s[0xA] = static_cast<unsigned char>(v & 1 ? (v >> 1) % 3 : v >> 1); break;
    case 4: s[9] = static_cast<unsigned char>(v & 1 ? (v >> 1) % 3 : v >> 1); break;
    case 5: s[0xB] = static_cast<unsigned char>(v & 1 ? (v >> 1) % 20 : v >> 1); break;
    case 6: s[v & 1 ? 7 : 6] = static_cast<unsigned char>(v >> 1); break;
    case 7: s[v & 1 ? 0x2B : 0x2C] = static_cast<unsigned char>(v >> 1); break;
    case 8: {
        unsigned char* const f = Field_State;
        static const unsigned kAt[] = {0x89, 0x128, 0x137, 0x138};
        const unsigned at = kAt[v % 4];
        if (sh::InRegions(f + at, 1)) f[at] = static_cast<unsigned char>(v >> 2);
        break;
    }
    case 9: {
        const unsigned i = s[0xB] < 20 ? s[0xB] : v % 20;
        sh::EffectRecord(i)[0] = static_cast<unsigned char>((v >> 5) & 1);
        break;
    }
    case 10: sh::Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 11: SetLong(Steps() + 4 * (v % 16), static_cast<std::int32_t>(v << 6)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kRows[k].clone.name, only)) {
            index[n] = k;
            chosen[n++] = kRows[k].clone;
        }
    if (n == 0) bof3::Fatal("rest_1c: BOF3X_R1C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1c",
                   chosen,
                   n,
                   kCallees,
                   sizeof kCallees / sizeof kCallees[0],
                   kTables,
                   sizeof kTables / sizeof kTables[0],
                   kRegions,
                   sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); },
                   &Disturb,
                   4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_1c
