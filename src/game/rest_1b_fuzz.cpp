// BOF3X_SHADOW=rest_1b: group R1B's 47 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_1b.md section 3. BOF3X_R1B_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R1B --clones --harness scenario
// (2026-10-04, through the round's band14.py), each extent read again to its
// last instruction (capstone). The 29 dispatchers and 14 states are kSprite
// (void, run on Sprite_Current); the four cell handlers kCall (x, z) answering
// al. The 29 .data tables are DataTables (their entries recorders on both
// sides), each index seeded below its table's own count. Every callee the
// states test is a stand-in of the group's own, registered before the
// harness's standard rows: R0A's seven helpers, Sprite_ObjectAt (an object
// 0..0x21 or none), AreaMap_ByteAt (the cell codes 0xF0..0xF8 and their
// neighbours), Field_EffectAhead (a record 0..19 or none), the slope and the
// ground, and the group's own four cell handlers where the states call them.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1b.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1b {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/band_rows.py --group R1B --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCalls51D790[] = {{0x14, 0x51C390}, {0x2E, 0x51C390}, {0x77, 0x5725C0}, {0xA5, 0x589330}, {0xE2, 0x5725C0}, {0xFC, 0x572570}, {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51D950[] = {{0x51, 0x531CF0}, {0x8F, 0x51DAA0}, {0xA5, 0x51DAA0}, {0xB9, 0x51DAA0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51DAA0[] = {{0xC, 0x536700}, {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2}, {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0}, {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51DC00[] = {{0x3, 0x521510}, {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740}, {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls51DE20[] = {{0x0, 0x5893A0}, {0x11, 0x589810}, {0x53, 0x587740}};
constexpr sh::CallSite kCalls51DF10[] = {{0x14, 0x51C390}, {0x2E, 0x51C390}, {0x77, 0x5725C0}, {0xA5, 0x589330}, {0xE2, 0x5725C0}, {0xFC, 0x572570}, {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51E0D0[] = {{0x51, 0x531CF0}, {0x8F, 0x51E1B0}, {0xA5, 0x51E1B0}, {0xB9, 0x51E1B0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51E1B0[] = {{0xC, 0x536700}, {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2}, {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0}, {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51E310[] = {{0x3, 0x521510}, {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740}, {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls51E4E0[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls51E570[] = {{0x36, 0x587740}, {0x3E, 0x530530}, {0x4F, 0x587740}, {0xB5, 0x531CF0}, {0xEE, 0x587740}, {0x100, 0x51E6C0}, {0x116, 0x51E6C0}, {0x12A, 0x51E6C0}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls51E6C0[] = {{0xD, 0x536700}, {0x49, 0x522FB0}, {0x53, 0x587740}, {0x5B, 0x5B93D2}, {0x74, 0x522FB0}, {0x7D, 0x591680}, {0xAD, 0x590BB0}, {0xBE, 0x587740}, {0xC4, 0x497710}, {0xE3, 0x497710}, {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0}, {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls51E870[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls51ED50[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x57, 0x587740}, {0x70, 0x589330}};
constexpr sh::CallSite kCalls51EDF0[] = {{0x2D, 0x530530}, {0x5A, 0x587740}, {0xA9, 0x531CF0}, {0xE2, 0x587740}, {0xF4, 0x51EF30}, {0x10A, 0x51EF30}, {0x11E, 0x51EF30}, {0x137, 0x589410}};
constexpr sh::CallSite kCalls51EF30[] = {{0xD, 0x536700}, {0x49, 0x522FB0}, {0x53, 0x587740}, {0x5B, 0x5B93D2}, {0x74, 0x522FB0}, {0x7D, 0x591680}, {0xAD, 0x590BB0}, {0xBE, 0x587740}, {0xC4, 0x497710}, {0xE3, 0x497710}, {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0}, {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls51F0E0[] = {{0xF, 0x587740}, {0x29, 0x589330}};
constexpr sh::CallSite kCalls51F130[] = {{0x37, 0x5893A0}};

#define R1B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R1B_CALLS(a) a, R1B_N(a)
#define R1B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite;
constexpr sh::Shape kCa = sh::Shape::kCall;
// The dispatchers (no call: a jmp through the table) and the states answer
// nothing a caller reads (Field_ActionState and Field_FormActionState ignore
// eax); the four cell handlers answer al, which each of their 12 call sites
// tests first (`test al, al`).
const sh::Clone kAll[] = {
    {"PartyFormAction2_ByForm", 0x51D710, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction2_ByForm), 0, false, kSp},
    {"PartyAction2_ByForm", 0x51D730, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction2_ByForm), 0, false, kSp},
    {"PartyFormAction3_Form0", 0x51D750, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction3_Form0), 0, false, kSp},
    {"PartyAction3_Form0", 0x51D770, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form0), 0, false, kSp},
    {"PartyAction3_Form0Begin", 0x51D790, 0x1B1, R1B_CALLS(kCalls51D790), nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form0Begin), 0, false, kSp},
    {"PartyAction3_Form0Resolve", 0x51D950, 0xDB, R1B_CALLS(kCalls51D950), nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form0Resolve), 0, false, kSp},
    {"PartyAction3_CellPickup", 0x51DAA0, 0x11F, R1B_CALLS(kCalls51DAA0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction3_Form1", 0x51DBC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction3_Form1), 0, false, kSp},
    {"PartyAction3_Form1", 0x51DBE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form1), 0, false, kSp},
    {"PartyAction3_Form1Begin", 0x51DC00, 0x166, R1B_CALLS(kCalls51DC00), nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form1Begin), 0, false, kSp},
    {"PartyFormAction3_Form2", 0x51DDE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction3_Form2), 0, false, kSp},
    {"PartyAction3_Form2", 0x51DE00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_Form2), 0, false, kSp},
    {"PartyAction_SpawnKind3A", 0x51DE20, 0x6E, R1B_CALLS(kCalls51DE20), nullptr, 0, nullptr, 0, R1B_FN(PartyAction_SpawnKind3A), 0, false, kSp},
    {"PartyFormAction3_ByForm", 0x51DE90, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction3_ByForm), 0, false, kSp},
    {"PartyAction3_ByForm", 0x51DEB0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction3_ByForm), 0, false, kSp},
    {"PartyFormAction4_Form0", 0x51DED0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction4_Form0), 0, false, kSp},
    {"PartyAction4_Form0", 0x51DEF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form0), 0, false, kSp},
    {"PartyAction4_Form0Begin", 0x51DF10, 0x1B1, R1B_CALLS(kCalls51DF10), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form0Begin), 0, false, kSp},
    {"PartyAction4_Form0Resolve", 0x51E0D0, 0xDB, R1B_CALLS(kCalls51E0D0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form0Resolve), 0, false, kSp},
    {"PartyAction4_CellPickup", 0x51E1B0, 0x11F, R1B_CALLS(kCalls51E1B0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction4_Form1", 0x51E2D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction4_Form1), 0, false, kSp},
    {"PartyAction4_Form1", 0x51E2F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form1), 0, false, kSp},
    {"PartyAction4_Form1Begin", 0x51E310, 0x166, R1B_CALLS(kCalls51E310), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form1Begin), 0, false, kSp},
    {"PartyFormAction4_Form2", 0x51E480, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction4_Form2), 0, false, kSp},
    {"PartyAction4_Form2", 0x51E4A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2), 0, false, kSp},
    {"PartyAction4_Form2State0", 0x51E4C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2State0), 0, false, kSp},
    {"PartyAction4_Form2Aim", 0x51E4E0, 0x87, R1B_CALLS(kCalls51E4E0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2Aim), 0, false, kSp},
    {"PartyAction4_Form2Hit", 0x51E570, 0x14C, R1B_CALLS(kCalls51E570), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2Hit), 0, false, kSp},
    {"PartyAction4_CellHit", 0x51E6C0, 0x188, R1B_CALLS(kCalls51E6C0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_CellHit), 0xFFu, false, kCa},
    {"PartyAction4_Form2State1", 0x51E850, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2State1), 0, false, kSp},
    {"PartyAction4_Form2Again", 0x51E870, 0x35, R1B_CALLS(kCalls51E870), nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_Form2Again), 0, false, kSp},
    {"PartyFormAction4_ByForm", 0x51E8B0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction4_ByForm), 0, false, kSp},
    {"PartyAction4_ByForm", 0x51E8D0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction4_ByForm), 0, false, kSp},
    {"PartyFormAction5_Form0", 0x51E8F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction5_Form0), 0, false, kSp},
    {"PartyFormAction5_Form1", 0x51ECF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction5_Form1), 0, false, kSp},
    {"PartyAction5_Form1", 0x51ED10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1), 0, false, kSp},
    {"PartyAction5_Form1State0", 0x51ED30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1State0), 0, false, kSp},
    {"PartyAction5_Form1Aim", 0x51ED50, 0xA0, R1B_CALLS(kCalls51ED50), nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1Aim), 0, false, kSp},
    {"PartyAction5_Form1Hit", 0x51EDF0, 0x140, R1B_CALLS(kCalls51EDF0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1Hit), 0, false, kSp},
    {"PartyAction5_CellHit", 0x51EF30, 0x188, R1B_CALLS(kCalls51EF30), nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_CellHit), 0xFFu, false, kCa},
    {"PartyAction5_Form1State1", 0x51F0C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1State1), 0, false, kSp},
    {"PartyAction5_Form1Again", 0x51F0E0, 0x46, R1B_CALLS(kCalls51F0E0), nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1Again), 0, false, kSp},
    {"PartyAction5_Form1Wait", 0x51F130, 0x3C, R1B_CALLS(kCalls51F130), nullptr, 0, nullptr, 0, R1B_FN(PartyAction5_Form1Wait), 0, false, kSp},
    {"PartyFormAction5_Form2", 0x51F170, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction5_Form2), 0, false, kSp},
    {"PartyFormAction5_ByForm", 0x51F190, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction5_ByForm), 0, false, kSp},
    {"PartyFormAction6_Form0", 0x51F1D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyFormAction6_Form0), 0, false, kSp},
    {"PartyAction6_Form0", 0x51F1F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1B_FN(PartyAction6_Form0), 0, false, kSp},
};
#undef R1B_FN
#undef R1B_CALLS
#undef R1B_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 47, "the cut's 47 rows");

// What each clone is, for the seed: a dispatcher by the word +0x2C, the byte +2
// or the byte +3 with its table's count; or one of the state shapes.
enum Role : std::uint8_t { kByWord, kByState, kByStep, kBegin, kResolve, kPickup, kPush, kSpawn3A, kAim, kHit, kCellHit, kAgain, kWait };
struct Spec { Role role; std::uint8_t count; };
constexpr Spec kSpecs[kCount] = {
    {kByWord, 3}, {kByWord, 3}, {kByState, 3}, {kByState, 3}, {kBegin, 0}, {kResolve, 0}, {kPickup, 0}, {kByState, 3},
    {kByState, 2}, {kPush, 0}, {kByState, 3}, {kByState, 3}, {kSpawn3A, 0}, {kByWord, 3}, {kByWord, 3}, {kByState, 3},
    {kByState, 3}, {kBegin, 0}, {kResolve, 0}, {kPickup, 0}, {kByState, 3}, {kByState, 2}, {kPush, 0}, {kByState, 3},
    {kByState, 2}, {kByStep, 5}, {kAim, 0}, {kHit, 0}, {kCellHit, 0}, {kByStep, 3}, {kAgain, 0}, {kByWord, 3},
    {kByWord, 3}, {kByState, 3}, {kByState, 3}, {kByState, 2}, {kByStep, 5}, {kAim, 0}, {kHit, 0}, {kCellHit, 0},
    {kByStep, 3}, {kAgain, 0}, {kWait, 0}, {kByState, 3}, {kByWord, 3}, {kByState, 3}, {kByState, 3},
};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// The 29 tables the dispatchers read in place, each to the next table's start
// (docs/rest_1b.md section 2): their entries recorders on both sides.
// symbols.toml's count, checked against the one written here
template <unsigned Toml, unsigned Here> constexpr unsigned Count() {
    static_assert(Toml == Here, "a table's count differs from symbols.toml's");
    return Toml;
}
#define R1B_T(name, n) {Key(name), Count<name##_count, n>()}
const sh::DataTable kTables[] = {
    R1B_T(PartyFormAction2_Forms, 3), R1B_T(PartyAction2_Forms, 3), R1B_T(PartyFormAction3_Form0States, 3),
    R1B_T(PartyAction3_Form0States, 3), R1B_T(PartyFormAction3_Form1States, 3), R1B_T(PartyAction3_Form1States, 2),
    R1B_T(PartyFormAction3_Form2States, 3), R1B_T(PartyAction3_Form2States, 3), R1B_T(PartyFormAction3_Forms, 3),
    R1B_T(PartyAction3_Forms, 3), R1B_T(PartyFormAction4_Form0States, 3), R1B_T(PartyAction4_Form0States, 3),
    R1B_T(PartyFormAction4_Form1States, 3), R1B_T(PartyAction4_Form1States, 2), R1B_T(PartyFormAction4_Form2States, 3),
    R1B_T(PartyAction4_Form2States, 2), R1B_T(PartyAction4_Form2State0Steps, 5), R1B_T(PartyAction4_Form2State1Steps, 3),
    R1B_T(PartyFormAction4_Forms, 3), R1B_T(PartyAction4_Forms, 3), R1B_T(PartyFormAction5_Form0States, 3),
    R1B_T(PartyFormAction5_Form1States, 3), R1B_T(PartyAction5_Form1States, 2), R1B_T(PartyAction5_Form1State0Steps, 5),
    R1B_T(PartyAction5_Form1State1Steps, 3), R1B_T(PartyFormAction5_Form2States, 3), R1B_T(PartyFormAction5_Forms, 3),
    R1B_T(PartyFormAction6_Form0States, 3), R1B_T(PartyAction6_Form0States, 3),
};
#undef R1B_T

template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
constexpr U kSteps = 0x6697B0;   // Field_DirectionSteps: 8 rows of two longs
constexpr U kTextRecords = bof3::addr::Text_Records;     // the 16 bytes the item names are copied into
unsigned char* Steps() { return sh::Mem(kSteps); }
unsigned char* Effect(unsigned i) { return sh::EffectRecord(i); }
unsigned char* S() { return Sprite_Current; }

// --- the stand-ins' answers (Noise() and the state only: both passes the same) -----

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// Sprite_ObjectAt: none (0xFF) half the time, else one of the 34 objects
// (0..0x1D Sprite_Objects, 0x1E..0x21 Sprite_ObjectsExtra) - what the real one
// answers; the states mark it, so any other byte would index past both tables
// (ours aborts there).
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    static const U kEdges[] = {0, 0x1D, 0x1E, 0x21};   // from the noise: Pick draws the seed's stream
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 4 ? (n >> 12) % 0x22 : kEdges[(n >> 16) % 4]);
}
// Field_EffectAhead: none half the time, else a record 0..19 (the states index
// Effect_Objects by it).
U FxEffectAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 20);
}
// AreaMap_ByteAt: each code the cell handlers compare with and its neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xEF, 0x00, 0xFF, 0x72};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// MapView_SlopeAt: DamageScratch's flag byte 0 a third of the time, else not
// 0; the slope's low word on either side of 0x40 and at the s16 limits.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = sh::Mem(bof3::addr::DamageScratch);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0, 0x1000};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// MapView_GroundAt: a low word at, one either side of, or far from the
// sprite's height word +0x3E (read now: the same on both passes).
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0, 1, 0xFFFF, 2, 0xFFFE, 0x8000, 0x7FFF};
    return WithAx(answer, n % 5 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}

// Masks by what each callee reads (symbols.toml's types, its evidence): the map
// cells as 16-bit words (AreaMap_ByteAt and AreaMap_ClearCell read the low
// words, Effect_SpawnAtCell and Effect_SpawnAtCellHigh movsx them, the group's
// cell handlers pass them on), the state bytes and the effect index as bytes,
// MapView_SlopeAt's direction as a byte (the Begin states push ebx whole, its
// upper bytes their caller's), every other argument whole.
#define R1B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // R0A's helpers (rest_0a.h): al 0 / 1, an index 0..19 or 0xFF, or nothing read
    {R1B_OURS(PartyAction_TargetAhead), 0, {}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction_BlockedAhead), 0, {}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction_Kind30Ahead), 0, {}, sh::Answer::kByte, 0xFF, 0x13, {}},
    {R1B_OURS(PartyAction_MemberOnEffect), 1, {kU8}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction_MemberBeyondEffect), 1, {kU8}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0, {}},
    {R1B_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    // unsigned char (long x, long y, unsigned margin)
    {R1B_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    // unsigned char (short x, short y)
    {R1B_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    // unsigned char (void)
    {R1B_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxEffectAhead},
    // long (long x, long y, unsigned long direction)
    {R1B_OURS(MapView_SlopeAt), 3, {kW, kW, kU8}, kG, 0, 0, {}, &FxSlope},
    // long (long x, long z)
    {R1B_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    // void (unsigned state, unsigned x, unsigned z): the state's byte, the words
    {R1B_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    // void (unsigned amount): pushed `and edx, 0xFF`, whole
    {R1B_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0, {}},
    // void (unsigned x, unsigned z)
    {R1B_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, kG, 0, 0, {}},
    // void (unsigned colour): an immediate 0
    {R1B_OURS(Sprite_FlashClut), 1, {kW}, kG, 0, 0, {}},
    // the group's own cell handlers where the states call them (E8, 12 sites):
    // al 0 a third of the time, the cells as words
    {R1B_OURS(PartyAction3_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction4_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction4_CellHit), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1B_OURS(PartyAction5_CellHit), 2, {kU16, kU16}, kF, 0, 0, {}},
};
#undef R1B_OURS

// Beyond field mode's standard regions (which hold Sprite_Current and the
// sprite records, ObjTrio, Field_State's pointer, Field_Request, Effect_Objects,
// Sprite_ObjectsExtra, DamageScratch's flag, Field_ScriptFlags 0x9039A2,
// Field_InputFlags and Field_Kind2Hold): Field_DirectionSteps, which the
// states read in place (seeded at its boundaries; .data, restored after the
// run), and Text_Records' 16 bytes the item names are copied into.
const sh::Region kRegions[] = {
    {kSteps, 0x40},
    {kTextRecords, 0x10},
};

// --- the seed -------------------------------------------------------------------------

// A 16.16 coordinate: a cell at 0, small, at the s16 and u16 limits or random;
// a fraction 0 (often: no cell one on), a half, a quarter, 1, 0xFFFF or random.
U Coordinate() {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
// Field_DirectionSteps: half the time the exe's shape (half a cell, 0x8000,
// signed per axis), else each dword a boundary or random.
void SeedSteps() {
    unsigned char* const t = Steps();
    for (unsigned i = 0; i < 16; ++i) {
        const U v = sh::Half() ? PickOf(0, 0x8000, 0xFFFF8000u)
                               : PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u, 0x4000, 1, 0xFFFFFFFFu, 0x7FFFFFFFu,
                                        0x80000000u, sh::Next());
        SetLong(t + 4 * i, static_cast<std::int32_t>(v));
    }
}
// Sprite_Current's direction: 0..7 mostly (odd and even: the turns test bit
// 0); 8..15 and any byte read the .data after the table (in place, the same on
// both passes).
unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }

void SeedSprite() {
    unsigned char* const s = S();
    s[8] = Direction();
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    SetWord(s + 0x3E, PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x40, sh::Next() % 0x400u, sh::Next()));
    // the counter +0xA at and either side of the 1 the count-downs end on
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 1, 1, 2, 0xFF, sh::Next()));
    // +0xB the effect index PartyAction5_Form1Wait reads unchecked: 0..19
    s[0xB] = static_cast<unsigned char>(sh::Next() % 20);
    // the script position PartyAction_SpawnKind3A tests against 0xA
    SetWord(s + 0x58, PickOf(0xA, 0xA, 0xA, 9, 0xB, 0, 0x10A, sh::Next()));
}

void SeedField() {
    unsigned char* const state = Field_State;
    state[0x138] = static_cast<unsigned char>(PickOf(0, 0, 0, 1, 2, 3, 0xFE, sh::Next()));
    Field_Kind2Hold = static_cast<unsigned char>(PickOf(0, 0, 0, 1, sh::Next()));
    Field_InputFlags = static_cast<unsigned char>(PickOf(0, 2, 4, 6, 1, 0xF9, sh::Next()));
    // the draws the cell handlers compare (& 0xF against 7, 0xB, 0xD, 0xF; & 7
    // against 5; & 3 against 0)
    sh::SetRandHint(PickOf(0xD, 0xE, 0xF, 0xC, 6, 7, 0xB, 5, 0x10, 0x14));
    for (unsigned i = 0; i < 20; ++i) {
        unsigned char* const e = Effect(i);
        e[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 1, 2, sh::Next()));
        e[1] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, sh::Next()));
    }
}

void Seed(unsigned k) {
    SeedSteps();
    SeedSprite();
    SeedField();
    const Spec& c = kSpecs[k];
    unsigned char* const s = S();
    switch (c.role) {
    case kByWord: SetWord(s + 0x2C, sh::Next() % c.count); break;
    case kByState: s[2] = static_cast<unsigned char>(sh::Next() % c.count); break;
    case kByStep: s[3] = static_cast<unsigned char>(sh::Next() % c.count); break;
    default: break;
    }
}

// The cells the four handlers are handed: a cell word at its boundaries under
// a random upper half (the original's callers push dwords whose upper halves
// are their own stack).
void Args(unsigned k, U* a) {
    if (kSpecs[k].role != kPickup && kSpecs[k].role != kCellHit) return;
    for (unsigned i = 0; i < 2; ++i)
        a[i] = (a[i] & 0xFFFF0000u) | (PickOf(0, 1, 0x40, 0x7FFF, 0x8000, 0xFFFF, sh::Next() % 0x80u, sh::Next()) & 0xFFFF);
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// position, height, counter +0xA, effect index +0xB (0..19), form word, script
// position; Field_State's +0x138 and +0x89; an effect record's +0, +1, +8,
// +0xA; Field_Kind2Hold, Field_InputFlags; a row of Field_DirectionSteps.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = S();
    switch (h % 12) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 2: SetWord(s + 0x3E, v >> 2); break;
    case 3: s[0xA] = static_cast<unsigned char>(v & 1 ? 1 : v >> 1); break;
    case 4: s[0xB] = static_cast<unsigned char>((v >> 1) % 20); break;
    case 5: SetWord(s + (v & 1 ? 0x2C : 0x58), v & 2 ? 0xA : v >> 2); break;
    case 6: {
        unsigned char* const state = Field_State;
        if (sh::InRegions(state + 0x89, 1) && sh::InRegions(state + 0x138, 1)) state[v & 1 ? 0x89 : 0x138] = static_cast<unsigned char>(v >> 1);
        break;
    }
    case 7: {
        unsigned char* const e = Effect(v % 20);
        static const unsigned kAt[] = {0, 1, 8, 0xA};
        e[kAt[(v >> 5) & 3]] = static_cast<unsigned char>(v >> 7);
        break;
    }
    case 8: Field_Kind2Hold = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 9: Field_InputFlags = static_cast<unsigned char>(v); break;
    case 10: SetLong(Steps() + 4 * (v % 16), static_cast<std::int32_t>(v << 6)); break;
    case 11: {
        // the effect record the round's index names, as case 7 a random one
        unsigned char* const e = Effect(s[0xB] < 20 ? s[0xB] : v % 20);
        e[(v >> 3) & 1] = static_cast<unsigned char>(v & 1 ? (v >> 4) & 1 : v >> 4);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_1b: BOF3X_R1B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_1b
