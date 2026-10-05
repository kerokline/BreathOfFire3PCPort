// BOF3X_SHADOW=rest_1f: group R1F's 49 functions through the scenario
// harness's field mode (scenario_harness.h, used unchanged), once at
// start-up. docs/rest_1f.md section 4. BOF3X_R1F_ONLY=<name> runs the clones
// whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R1F --clones --harness
// scenario (2026-10-04, through the round's band14.py), every extent and call
// site read again to the last instruction (capstone). The state handlers and
// dispatchers are kSprite (void, Sprite_Current one of the first four sprite
// records), the helpers kCall answering al where their callers read it. The
// 24 tables the dispatchers jump through are swapped for recorders on both
// sides; each dispatcher's index byte or word is seeded below its own table.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1f.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-04), each checked against the capstone read.
constexpr sh::CallSite kCalls523ED0[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls523F10[] = {{0x0, 0x589410}, {0x12, 0x589330}, {0x4C, 0x587740}, {0x90, 0x587740}};
constexpr sh::CallSite kCallsBegin[] = {{0x15, 0x51C390}, {0x2F, 0x51C390}, {0x72, 0x572570}, {0x8D, 0x5725C0}, {0xB8, 0x589330}, {0xF6, 0x5725C0}, {0x115, 0x572570}, {0x14D, 0x5725C0}, {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCalls5241D0[] = {{0x51, 0x531CF0}, {0x8F, 0x5242B0}, {0xA5, 0x5242B0}, {0xB9, 0x5242B0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls524670[] = {{0x51, 0x531CF0}, {0x8F, 0x524750}, {0xA5, 0x524750}, {0xB9, 0x524750}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls525070[] = {{0x51, 0x531CF0}, {0x8F, 0x525150}, {0xA5, 0x525150}, {0xB9, 0x525150}, {0xD2, 0x589410}};
constexpr sh::CallSite kCallsPickup[] = {{0xC, 0x536700}, {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2}, {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0}, {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls5249D0[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls524A60[] = {{0x36, 0x587740}, {0x3E, 0x530530}, {0x4F, 0x587740}, {0xB5, 0x531CF0}, {0xEE, 0x587740}, {0x100, 0x524BB0}, {0x116, 0x524BB0}, {0x12A, 0x524BB0}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls524BB0[] = {{0xD, 0x536700}, {0x49, 0x522FB0}, {0x53, 0x587740}, {0x5B, 0x5B93D2}, {0x74, 0x522FB0}, {0x7D, 0x591680}, {0xAD, 0x590BB0}, {0xBE, 0x587740}, {0xC4, 0x497710}, {0xE3, 0x497710}, {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0}, {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls5252B0[] = {{0x14, 0x589810}, {0x55, 0x589330}, {0x6A, 0x587740}};
constexpr sh::CallSite kCalls527640[] = {{0x94, 0x5287B0}, {0xCF, 0x527DB0}, {0xE3, 0x527DB0}, {0xF7, 0x527DB0}, {0x125, 0x5280F0}, {0x13E, 0x5280F0}, {0x14D, 0x527FF0}, {0x17B, 0x528070}, {0x1A5, 0x528070}, {0x1C4, 0x5280F0}, {0x1CB, 0x528070}, {0x1EA, 0x5280F0}, {0x1F1, 0x528070}, {0x200, 0x528190}, {0x22D, 0x527DB0}, {0x253, 0x5280F0}, {0x296, 0x5280A0}, {0x2A8, 0x5280F0}, {0x2C6, 0x5280A0}, {0x2D8, 0x5280F0}, {0x2F1, 0x527DB0}, {0x317, 0x5280F0}, {0x33C, 0x5280F0}, {0x346, 0x528070}, {0x364, 0x5280A0}, {0x376, 0x5280F0}, {0x399, 0x5280A0}, {0x3AF, 0x5280F0}, {0x3EA, 0x527DB0}, {0x425, 0x528070}, {0x458, 0x5280A0}, {0x495, 0x527DB0}, {0x4A5, 0x527470}, {0x4B5, 0x527470}, {0x4C5, 0x527470}, {0x521, 0x528120}, {0x548, 0x528120}, {0x560, 0x528120}, {0x583, 0x528070}, {0x5A2, 0x5280A0}, {0x5CF, 0x5280A0}, {0x602, 0x527DB0}, {0x612, 0x527470}, {0x622, 0x527470}, {0x632, 0x527470}, {0x693, 0x528120}, {0x6BA, 0x528120}, {0x6D3, 0x528120}, {0x6F6, 0x528070}, {0x716, 0x5280A0}, {0x745, 0x5280A0}};
constexpr sh::CallSite kCalls527FF0[] = {{0x55, 0x5280F0}, {0x66, 0x5280F0}};
constexpr sh::CallSite kCalls5280A0[] = {{0x31, 0x5725C0}};
constexpr sh::CallSite kCalls528190[] = {{0x1E, 0x5280A0}, {0x30, 0x5280F0}, {0x58, 0x5280A0}, {0x70, 0x5280A0}, {0x82, 0x5280F0}, {0xA2, 0x5280A0}, {0xB4, 0x5280F0}, {0xDE, 0x5280A0}, {0xF0, 0x5280F0}, {0x105, 0x5280A0}, {0x117, 0x5280F0}};
constexpr sh::CallSite kCalls5287B0[] = {{0x18, 0x528370}, {0x2F, 0x528370}, {0x49, 0x528370}, {0x61, 0x528370}, {0x7D, 0x528370}, {0x97, 0x528370}, {0xB1, 0x528370}};
constexpr sh::CallSite kCalls5288C0[] = {{0x2B, 0x5720C0}, {0x51, 0x589200}, {0x64, 0x5891F0}};

#define R_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R_FN(name) reinterpret_cast<const void*>(&::name)
// A state handler or dispatcher (void, Sprite_Current) and a helper (kCall,
// `ret` the part of eax its callers read: 0xFF for al, 0 for nothing).
#define R_STATE(name, base, size, calls) {#name, base, size, calls, R_N(calls), nullptr, 0, nullptr, 0, R_FN(name), 0, false, Shape::kSprite}
#define R_LEAF(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R_FN(name), 0, false, Shape::kSprite}
#define R_CALL(name, base, size, calls, ret) {#name, base, size, calls, R_N(calls), nullptr, 0, nullptr, 0, R_FN(name), ret, false, Shape::kCall}
#define R_CALL0(name, base, size, ret) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R_FN(name), ret, false, Shape::kCall}
const sh::Clone kAll[] = {
    R_LEAF(PartyFormAction16_ByForm, 0x5243D0, 0x13),
    R_LEAF(PartyAction16_ByForm, 0x5243F0, 0x13),
    R_LEAF(PartyFormAction16_Form2, 0x523FB0, 0x12),
    R_LEAF(PartyAction16_Form2, 0x523FD0, 0x12),
    R_STATE(PartyAction16_Form2Begin, 0x523FF0, 0x1D4, kCallsBegin),
    R_STATE(PartyAction16_Form2Resolve, 0x5241D0, 0xDB, kCalls5241D0),
    R_CALL(PartyAction16_CellPickup, 0x5242B0, 0x11F, kCallsPickup, 0xFFu),
    R_LEAF(PartyFormAction17_ByForm, 0x524930, 0x13),
    R_LEAF(PartyAction17_ByForm, 0x524950, 0x13),
    R_LEAF(PartyFormAction17_Form0, 0x524410, 0x12),
    R_LEAF(PartyFormAction17_Form1, 0x524450, 0x12),
    R_LEAF(PartyFormAction17_Form2, 0x5248F0, 0x12),
    R_LEAF(PartyAction17_Form0, 0x524430, 0x12),
    R_LEAF(PartyAction17_Form1, 0x524470, 0x12),
    R_LEAF(PartyAction17_Form2, 0x524910, 0x12),
    R_STATE(PartyAction17_Form1Begin, 0x524490, 0x1D4, kCallsBegin),
    R_STATE(PartyAction17_Form1Resolve, 0x524670, 0xDB, kCalls524670),
    R_CALL(PartyAction17_CellPickup, 0x524750, 0x11F, kCallsPickup, 0xFFu),
    R_LEAF(PartyFormAction18_ByForm, 0x525330, 0x13),
    R_LEAF(PartyAction18_ByForm, 0x525350, 0x13),
    R_LEAF(PartyFormAction18_Form0, 0x524970, 0x12),
    R_LEAF(PartyFormAction18_Form1, 0x524E50, 0x12),
    R_LEAF(PartyFormAction18_Form2, 0x525270, 0x12),
    R_LEAF(PartyAction18_Form0, 0x524990, 0x12),
    R_LEAF(PartyAction18_Form0Sub0, 0x5249B0, 0x12),
    R_LEAF(PartyAction18_Form0Sub1, 0x524D40, 0x12),
    R_STATE(PartyAction18_Form0Sub0Begin, 0x5249D0, 0x87, kCalls5249D0),
    R_STATE(PartyAction18_Form0Sub0Strike, 0x524A60, 0x14C, kCalls524A60),
    R_CALL(PartyAction18_CellStrike, 0x524BB0, 0x188, kCalls524BB0, 0xFFu),
    R_STATE(PartyAction18_ProbeStart, 0x524D60, 0x35, kCalls523ED0),
    R_LEAF(PartyAction18_Form1, 0x524E70, 0x12),
    R_STATE(PartyAction18_Form1Begin, 0x524E90, 0x1D4, kCallsBegin),
    R_STATE(PartyAction18_Form1Resolve, 0x525070, 0xDB, kCalls525070),
    R_CALL(PartyAction18_CellPickup, 0x525150, 0x11F, kCallsPickup, 0xFFu),
    R_LEAF(PartyAction18_Form2, 0x525290, 0x12),
    R_STATE(PartyAction_ProbeStart, 0x523ED0, 0x35, kCalls523ED0),
    R_STATE(PartyAction_EffectCountdown, 0x523F10, 0x97, kCalls523F10),
    R_STATE(PartyAction_SpawnKind1B, 0x5252B0, 0x7B, kCalls5252B0),
    R_CALL(Field_CellAheadRaised, 0x527640, 0x766, kCalls527640, 0xFFu),
    R_CALL0(Field_CellClass5, 0x527DB0, 0x23B, 0xFFu),
    R_CALL(Field_CornerTurn, 0x527FF0, 0x74, kCalls527FF0, 0xFFu),
    R_CALL(Field_SlopeBetween, 0x5280A0, 0x4F, kCalls5280A0, 0xFFu),
    R_CALL(Field_RaisedEdgeTurns, 0x528190, 0x122, kCalls528190, 0),
    R_CALL(Field_ReadCellsRaised, 0x5287B0, 0xC3, kCalls5287B0, 0),
    R_LEAF(LeaderPanel_Run, 0x528880, 0x12),
    R_LEAF(LeaderPanel_S0, 0x5288A0, 0x12),
    R_STATE(LeaderPanel_S0Begin, 0x5288C0, 0x75, kCalls5288C0),
    R_LEAF(LeaderPanel_S0Wait, 0x528940, 0x26),
    R_LEAF(LeaderPanel_S0End, 0x528970, 0x2C),
};
#undef R_CALL0
#undef R_CALL
#undef R_LEAF
#undef R_STATE
#undef R_FN
#undef R_N

enum : unsigned {
    k16FormAction, k16ByForm, k16FormAction2, k16Form2, k16Begin, k16Resolve, k16Pickup,
    k17FormAction, k17ByForm, k17FormAction0, k17FormAction1, k17FormAction2, k17Form0, k17Form1, k17Form2, k17Begin,
    k17Resolve, k17Pickup,
    k18FormAction, k18ByForm, k18FormAction0, k18FormAction1, k18FormAction2, k18Form0, k18Form0Sub0, k18Form0Sub1,
    k18Sub0Begin, k18Sub0Strike, k18CellStrike, k18ProbeStart, k18Form1, k18Begin, k18Resolve, k18Pickup, k18Form2,
    kProbeStart, kEffectCountdown, kSpawnKind1B,
    kCellAheadRaised, kCellClass5, kCornerTurn, kSlopeBetween, kRaisedEdgeTurns, kReadCellsRaised,
    kPanelRun, kPanelS0, kPanelS0Begin, kPanelS0Wait, kPanelS0End, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
constexpr U kSteps = 0x6697B0;         // Field_DirectionSteps
constexpr U kCellOffsets = 0x66971C;   // the cell-ahead offsets, two signed bytes a direction
constexpr U kCells = 0x903850;         // the sloped flag and the cells 1..0xF
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Sc() { return Sprite_Current; }

// --- the stand-ins' answers and effects (Noise() and the state only) -----------------------

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// Sprite_ObjectAt: none half the time, else a record 0..0x21 (what the real
// one answers: 30 Sprite_Objects and four extra).
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 0x22);
}
// Field_EffectAhead: none half the time, else an effect record 0..19.
U FxEffectAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 20);
}
// AreaMap_ByteAt: the cell codes the pickups and the strike compare with, and
// their neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xEF, 0x00, 0xFF, 0xF2, 0xF8, 0xF6};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// MapView_GroundAt: a low word at Sprite_Current's height word plus 0x40, one
// either side, 0, the s16 limits, or random (the Begin states' rise test).
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sc();
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0x40, 0x41, 0x3F, 0, 1, 0xFFFF, 0x8040, 0x7FFF, 0x8000};
    return WithAx(answer, n % 5 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}
// MapView_SlopeAt: the sloped flag 0 a third of the time; the low word on either
// side of 0x40 and at the s16 limits.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = Mem(kCells);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// In the disturbance's role, louder than the real ones (which write none of
// these): a quarter of the time a class value into one of the cells 8..0xB.
// Field_CellAheadRaised reads those cells again after its class calls and
// after Field_CornerTurn; the group's case 6 alone, about one call in 290, left
// controls C117, C118 and C120 unrefused (2026-10-05, docs/rest_1f.md section 5).
void StirClassCell() {
    const U n = sh::Noise();
    if ((n & 3) != 0) return;
    static const unsigned char kClasses[] = {0xB0, 0x70, 0x10, 0x20, 0xA0, 0xA1, 0xA2, 0xA3};
    unsigned char* const cell = Mem(kCells + 8 + (n >> 2) % 4);
    if (sh::InRegions(cell, 1)) cell[0] = kClasses[(n >> 4) % 8];
}
// A class answer (Field_CellClass, Field_CellClass5): the values the callers
// compare with; and StirClassCell.
U FxClass(const U*, U answer) {
    static const U kClasses[] = {0xB0, 0x70, 0x10, 0x20, 0xA0, 0xA1, 0xA2, 0xA3, 0xA5, 0x00, 0x21, 0xFF};
    const U n = sh::Noise();
    StirClassCell();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kClasses[(n >> 3) % (sizeof kClasses / sizeof kClasses[0])]);
}
// Field_SlopeBetween: in the disturbance's role (the real one writes neither),
// a quarter of the time one of the sprite's cell words +0x36 / +0x3A moved -
// Field_CellAheadRaised and Field_RaisedEdgeTurns read them again after the
// call; the group's case 9 alone left control C124 unrefused (2026-10-05).
U FxSlopeBetween(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const s = Sc();
    unsigned char* const word = s + ((n >> 2) & 1 ? 0x36 : 0x3A);
    if ((n & 3) == 0 && sh::InRegions(word, 2)) SetWord(word, n >> 16);
    return answer;
}
// Field_TurnUnless and Field_CellPairTurn write the facing +8, which the
// callers read again: half the time a direction (or rarely any byte).
// Louder than the real ones: a quarter of the time a class value lands in one
// of the cells 8..0xB, which Field_CellAheadRaised reads again after the turn.
U FxTurn(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const s = Sc();
    if ((n & 1) && sh::InRegions(s + 8, 1)) s[8] = static_cast<unsigned char>(n % 16 == 1 ? n >> 8 : (n >> 8) % 8);
    if (((n >> 16) & 3) == 0) {
        static const unsigned char kClasses[] = {0xB0, 0x70, 0x10, 0x20, 0xA0, 0xA1, 0xA2, 0xA3};
        unsigned char* const cell = Mem(kCells + 8 + (n >> 18) % 4);
        if (sh::InRegions(cell, 1)) cell[0] = kClasses[(n >> 20) % 8];
    }
    return answer;
}
// Field_CornerTurn: 0 two times in three, so that Field_CellAheadRaised's
// corner path goes on to its slope sides and Field_RaisedEdgeTurns; and
// StirClassCell.
U FxCorner(const U*, U answer) {
    const U n = sh::Noise();
    StirClassCell();
    return WithAl(answer, n % 3 == 0 ? 1 + (n >> 8) % 0xFF : 0);
}
U FxPairTurn(const U* a, U answer) {
    FxTurn(a, answer);
    const U n = sh::Noise();
    return WithAl(answer, n % 3 == 0 ? 1 : 0);
}

#define R_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own, called by E8 from its other functions (the cells and
    // points as words: the callers push dwords whose upper halves are their
    // own leftovers; every one of these reads 16 bits)
    {R_OURS(PartyAction16_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R_OURS(PartyAction17_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R_OURS(PartyAction18_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R_OURS(PartyAction18_CellStrike), 2, {kU16, kU16}, kF, 0, 0},
    {R_OURS(Field_CellClass5), 5, {kW, kW, kW, kW, kW}, kG, 0, 0, {}, &FxClass},
    {R_OURS(Field_CornerTurn), 0, {}, kF, 0, 0, {}, &FxCorner},
    {R_OURS(Field_SlopeBetween), 5, {kU16, kU16, kU16, kU16, kW}, kF, 0, 0, {}, &FxSlopeBetween},
    {R_OURS(Field_RaisedEdgeTurns), 2, {kU16, kU16}, kG, 0, 0},
    {R_OURS(Field_ReadCellsRaised), 4, {kU16, kU16, kU16, kU16}, kG, 0, 0},
    // R0A's (docs/rest_0a.md section 7): the answers in al, the spawn's state a
    // byte and its cell words (a fourth dword pushed, not read)
    {R_OURS(PartyAction_TargetAhead), 0, {}, kF, 0, 0},
    {R_OURS(PartyAction_BlockedAhead), 0, {}, kF, 0, 0},
    {R_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0},
    {R_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0},
    // member_sprites' cell helpers (each argument an immediate but Field_CellKind's)
    {R_OURS(Field_TurnUnless), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxTurn},
    {R_OURS(Field_CellPairTurn), 4, {kW, kW, kW, kW}, kG, 0, 0, {}, &FxPairTurn},
    {R_OURS(Field_CellSlope), 1, {kW}, kG, 0, 0},
    {R_OURS(Field_CellClass), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxClass},
    {R_OURS(Field_CellKind), 4, {kU16, kU16, kU16, kU16}, kG, 0, 0},
    // ours, in no standard set: Effect_SpawnAtCell reads the state's byte and
    // the cell words (movsx), AreaMap_ClearCell the cell words
    {R_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0},
    {R_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, kG, 0, 0},
    {R_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0},
    {R_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxEffectAhead},
    {R_OURS(Sprite_FlashClut), 1, {kW}, kG, 0, 0},
    // standard ones re-listed with the answers these functions branch on
    {R_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    {R_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    {R_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    // the direction a byte (the Begin states push it over Sprite_Current's
    // address): AreaMap_Slope reads the upper bytes only for a direction of 10
    // or more, unreachable while directions stay 0..7 (rest_1e.md section 5)
    {R_OURS(MapView_SlopeAt), 3, {kW, kW, kU8}, kG, 0, 0, {}, &FxSlope},
};
#undef R_OURS

// The tables the dispatchers read in place, swapped for recorders on both
// sides (docs/rest_1f.md section 3: each count is the run of code pointers to
// the next table a dispatcher reads).
#define R_TABLE(name) {Key(name), name##_count}
const sh::DataTable kTables[] = {
    R_TABLE(PartyFormAction16_Form2States), R_TABLE(PartyAction16_Form2States),
    R_TABLE(PartyFormAction16_Forms), R_TABLE(PartyAction16_Forms),
    R_TABLE(PartyFormAction17_Form0States), R_TABLE(PartyAction17_Form0States),
    R_TABLE(PartyFormAction17_Form1States), R_TABLE(PartyAction17_Form1States),
    R_TABLE(PartyFormAction17_Form2States), R_TABLE(PartyAction17_Form2States),
    R_TABLE(PartyFormAction17_Forms), R_TABLE(PartyAction17_Forms),
    R_TABLE(PartyFormAction18_Form0States), R_TABLE(PartyAction18_Form0Subs),
    R_TABLE(PartyAction18_Form0Sub0Steps), R_TABLE(PartyAction18_Form0Sub1Steps),
    R_TABLE(PartyFormAction18_Form1States), R_TABLE(PartyAction18_Form1States),
    R_TABLE(PartyFormAction18_Form2States), R_TABLE(PartyAction18_Form2States),
    R_TABLE(PartyFormAction18_Forms), R_TABLE(PartyAction18_Forms),
    R_TABLE(LeaderPanel_Stages), R_TABLE(LeaderPanel_Stage0Steps),
};
#undef R_TABLE

// Beyond field mode's standard regions (which hold Sprite_Current and the
// sprite records, ObjTrio, Field_State, Effect_Objects, the cells 0x903850..
// 0x90385F, Field_InputFlags, Field_ScriptFlags, Field_Request and the wait
// word): Field_DirectionSteps and the cell-ahead offsets (.data image tables,
// seeded so that each row's boundaries are met), Text_Records' first 16 bytes
// (the pickups' and the strike's name copy), and the leader panel's cells
// 0x6BC700.. and 0x939A00.. (effect_1e's regions).
const sh::Region kRegions[] = {
    {kSteps, 0x40},
    {kCellOffsets, 0x10},
    {bof3::addr::Text_Records, 0x10},
    {0x6BC700, 0x20},
    {0x939A00, 0x30},
};

// --- the seed -----------------------------------------------------------------------------

// A 16.16 coordinate: a cell at 0, small, at the s16 and u16 limits or random;
// a fraction 0 three times in eight (`whole`: Field_CellAheadRaised's rounds,
// which return at once when both are 0, one time in five), else a half, a
// quarter, 1, 0xFFFF or random.
U Coordinate(bool rare_whole) {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = rare_whole ? PickOf(0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next())
                              : PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
U Height() { return PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x7FC0, 0xFFC0, 0x40, sh::Next() % 0x400u, sh::Next()); }

void SeedSteps() {
    unsigned char* const t = Mem(kSteps);
    for (unsigned i = 0; i < 16; ++i) {
        const U v = sh::Half() ? PickOf(0, 0x8000, 0xFFFF8000u)
                               : PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u, 0x4000, 1, 0xFFFFFFFFu, 0x7FFFFFFFu,
                                        0x80000000u, sh::Next());
        SetLong(t + 4 * i, static_cast<std::int32_t>(v));
    }
    // the cell-ahead offsets: -1, 0, 1 (where 1 counts 2) mostly
    unsigned char* const o = Mem(kCellOffsets);
    for (unsigned i = 0; i < 16; ++i) o[i] = static_cast<unsigned char>(sh::Often() ? PickOf(0xFF, 0, 1) : PickOf(2, 0xFE, 0x80, 0x7F, sh::Next()));
}

// A cell's kind or class: the values the class rules and the callers compare with.
unsigned char CellValue() {
    return static_cast<unsigned char>(PickOf(0xB0, 0x70, 0x10, 0xFF, 0x20, 0x21, 0x22, 0x2F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA5,
                                             0x00, 0x52, sh::Next()));
}
// Field_CellClass5's rounds: half the cells a slope or the two the rules
// single out (0x70, 0xB0), so that its three- and two-cell slope rules are met.
void SeedCells(unsigned k) {
    unsigned char* const c = Mem(kCells);
    c[0] = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
    for (unsigned i = 1; i < 16; ++i)
        c[i] = k == kCellClass5 && sh::Half() ? static_cast<unsigned char>(PickOf(0xA0, 0xA1, 0xA2, 0xA3, 0xA0, 0xA2, 0x70, 0xB0))
                                             : CellValue();
}

unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }

// The round's effect index for the disturbance (PartyAction_EffectCountdown's
// +0xB), so that it moves the record that function writes after a call.
unsigned g_effect = 20;

void SeedSprite(unsigned char* s, unsigned k) {
    s[7] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    s[8] = Direction();
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 5, sh::Next()));
    // +0xB: an effect index below 20 where a function indexes the records by it
    s[0xB] = static_cast<unsigned char>(k == kEffectCountdown ? sh::Next() % 20 : PickOf(0, 1, 2, 0xFF, sh::Next() % 20, sh::Next()));
    SetWord(s + 0x2C, PickOf(0, 1, 2, 0xFF, 0x7F00, sh::Next()));
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate(k == kCellAheadRaised)));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate(k == kCellAheadRaised)));
    SetWord(s + 0x3E, Height());
    s[0x70] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
}

// The index each dispatcher reads, below its own table.
void SeedIndex(unsigned k) {
    unsigned char* const s = Sc();
    switch (k) {
    case k16FormAction: SetWord(s + 0x2C, sh::Next() % PartyFormAction16_Forms_count); break;
    case k16ByForm: SetWord(s + 0x2C, sh::Next() % PartyAction16_Forms_count); break;
    case k17FormAction: SetWord(s + 0x2C, sh::Next() % PartyFormAction17_Forms_count); break;
    case k17ByForm: SetWord(s + 0x2C, sh::Next() % PartyAction17_Forms_count); break;
    case k18FormAction: SetWord(s + 0x2C, sh::Next() % PartyFormAction18_Forms_count); break;
    case k18ByForm: SetWord(s + 0x2C, sh::Next() % PartyAction18_Forms_count); break;
    case k16FormAction2: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction16_Form2States_count); break;
    case k16Form2: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction16_Form2States_count); break;
    case k17FormAction0: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction17_Form0States_count); break;
    case k17FormAction1: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction17_Form1States_count); break;
    case k17FormAction2: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction17_Form2States_count); break;
    case k17Form0: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction17_Form0States_count); break;
    case k17Form1: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction17_Form1States_count); break;
    case k17Form2: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction17_Form2States_count); break;
    case k18FormAction0: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction18_Form0States_count); break;
    case k18FormAction1: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction18_Form1States_count); break;
    case k18FormAction2: s[2] = static_cast<unsigned char>(sh::Next() % PartyFormAction18_Form2States_count); break;
    case k18Form0: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction18_Form0Subs_count); break;
    case k18Form1: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction18_Form1States_count); break;
    case k18Form2: s[2] = static_cast<unsigned char>(sh::Next() % PartyAction18_Form2States_count); break;
    case k18Form0Sub0: s[3] = static_cast<unsigned char>(sh::Next() % PartyAction18_Form0Sub0Steps_count); break;
    case k18Form0Sub1: s[3] = static_cast<unsigned char>(sh::Next() % PartyAction18_Form0Sub1Steps_count); break;
    case kPanelRun: s[2] = static_cast<unsigned char>(sh::Next() % LeaderPanel_Stages_count); break;
    case kPanelS0: s[3] = static_cast<unsigned char>(sh::Next() % LeaderPanel_Stage0Steps_count); break;
    default: break;
    }
}

void Seed(unsigned k) {
    SeedSteps();
    SeedCells(k);
    for (unsigned i = 0; i < 4; ++i) SeedSprite(sh::SpriteRecord(i), k);
    SeedIndex(k);
    g_effect = k == kEffectCountdown ? Sc()[0xB] : 20;
    Field_InputFlags = static_cast<unsigned char>(PickOf(0, 2, 4, 6, 1, sh::Next()));
    Field_ScriptFlags = static_cast<unsigned short>(sh::Next() % 8 == 0 ? Field_ScriptFlags | 0x400 : Field_ScriptFlags & ~0x400u);
    Field_State[0x89] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    MoveScript_WaitWordDA = static_cast<unsigned short>(PickOf(0, 0, sh::Next()));
    unsigned char* const record4 = Effect_Objects + 4 * 0x80;
    record4[1] = static_cast<unsigned char>(PickOf(4, 4, 3, 5, sh::Next()));
    sh::SetRandHint(PickOf(0xD, 0xE, 0xF, 0xC, 0, 4, 6, 7, 5, 0xB, sh::Next()));
}

// The arguments of the kCall helpers: the cell indices of Field_CellClass5 (the
// list ends at the first 0), the rest random (each read as the callee reads it).
void Args(unsigned k, U* a) {
    if (k != kCellClass5) return;
    const unsigned n = sh::Next() % 6;   // 0..5 cells before the first 0
    for (unsigned i = 0; i < 5; ++i) {
        // a cell 1..0xF before the end; the end a 0; past it anything below 0x10
        const U index = i < n ? 1 + sh::Next() % 0xF : i == n ? 0 : sh::Next() % 0x10;
        a[i] = (a[i] & 0xFFFFFF00u) | index;
    }
}

// What these read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's facing, its
// counters +0xA / +7, its fractions and height, its form word; the cells 0 and
// 8..0xB; the round's effect record; Field_InputFlags; record 4's state. No
// case moves +0xB or Field_Request.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sc();
    switch (sh::DisturbCase(h, 12)) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: s[0xA] = static_cast<unsigned char>(v % 3); break;
    case 2: s[7] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 3: SetWord(s + (v & 1 ? 0x34 : 0x38), v & 2 ? 0 : v >> 2); break;
    case 4: SetWord(s + 0x3E, v >> 2); break;
    case 5: Mem(kCells)[0] = static_cast<unsigned char>(v & 1); break;
    case 6: {
        static const unsigned char kClasses[] = {0xB0, 0x70, 0x10, 0x20, 0xA0, 0xA1, 0xA2, 0xA3};
        Mem(kCells)[8 + v % 4] = kClasses[(v >> 2) % 8];
        break;
    }
    case 7:
        if (g_effect < 20) Effect_Objects[g_effect * 0x80 + 8 + (v & 2)] = static_cast<unsigned char>(v >> 2);
        break;
    case 8: Field_InputFlags = static_cast<unsigned char>(v); break;
    case 9: SetWord(s + 0x36 + (v & 1) * 4, v >> 1); break;
    case 10: Effect_Objects[4 * 0x80 + 1] = static_cast<unsigned char>(v & 1 ? 4 : v >> 1); break;
    case 11: SetWord(s + 0x2C, v >> 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_1f: BOF3X_R1F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0],
                   kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 8000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.sprite_span = 2;   // +1..+4 below the smallest table (two entries); each dispatcher's own index is seeded
    sh::Run(g);
}

}  // namespace rest_1f
