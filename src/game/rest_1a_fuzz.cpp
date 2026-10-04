// BOF3X_SHADOW=rest_1a: group R1A's 49 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_1a.md section 4. BOF3X_R1A_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R1A --clones --harness
// scenario (through the round's band14.py, 2026-10-04), each extent read again
// to its last instruction (capstone); every extent is the tool's. Shapes: the
// 45 state handlers and dispatchers kSprite (void, no arguments, run on
// Sprite_Current - an ObjTrio record for the member states, as
// Field_MemberFrame runs them), the four cell helpers kCall answering al
// (ret_mask 0xFF: every caller tests al). A dispatcher's index is drawn below
// its own table's length (the tables are back to back and unbounded); the 26
// tables the group reads are swapped for recorders while the fuzz runs.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1a.h"
#include "game/rest_1a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1a {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/band_rows.py --group R1A --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCalls51BAA0[] = {{0xEC, 0x589330}, {0x104, 0x536730}, {0x11F, 0x51AD60}};
constexpr sh::CallSite kCalls51BD10[] = {{0x78, 0x5365D0}, {0x7F, 0x536550}, {0x86, 0x5364D0}};
constexpr sh::CallSite kCalls51BEB0[] = {{0x7, 0x57C140}, {0x15, 0x531BB0}, {0x88, 0x52F570}, {0xB0, 0x52F570}, {0xCF, 0x52F570}};
constexpr sh::CallSite kCalls51BFD0[] = {{0x14, 0x51C390},  {0x2E, 0x51C390},  {0x77, 0x5725C0}, {0xA5, 0x589330},  {0xE2, 0x5725C0},
                                         {0xFC, 0x572570},  {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51C190[] = {{0x51, 0x531CF0}, {0x8F, 0x51C270}, {0xA5, 0x51C270}, {0xB9, 0x51C270}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51C270[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51C490[] = {{0x25, 0x589330}, {0x77, 0x589330}};
constexpr sh::CallSite kCalls51C530[] = {{0x3, 0x521510},  {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740},
                                         {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls51C7C0[] = {{0x14, 0x51C390},  {0x2E, 0x51C390},  {0x77, 0x5725C0}, {0xA5, 0x589330},  {0xE2, 0x5725C0},
                                         {0xFC, 0x572570},  {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51C980[] = {{0x51, 0x531CF0}, {0x8F, 0x51CA60}, {0xA5, 0x51CA60}, {0xB9, 0x51CA60}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51CA60[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51CC60[] = {{0x1A, 0x589200}, {0x2C, 0x5891F0}, {0x7C, 0x589330}};
constexpr sh::CallSite kCalls51CD10[] = {{0x14, 0x51C390},  {0x2E, 0x51C390},  {0x77, 0x5725C0}, {0xA5, 0x589330},  {0xE2, 0x5725C0},
                                         {0xFC, 0x572570},  {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr sh::CallSite kCalls51CED0[] = {{0x51, 0x531CF0}, {0x8F, 0x51CFB0}, {0xA5, 0x51CFB0}, {0xB9, 0x51CFB0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls51CFB0[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls51D0F0[] = {{0x24, 0x52F570}, {0x43, 0x52F570}};
constexpr sh::CallSite kCalls51D160[] = {{0x23, 0x589330}, {0x75, 0x589330}};
constexpr sh::CallSite kCalls51D260[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x48, 0x524DA0}, {0x61, 0x589330}};
constexpr sh::CallSite kCalls51D2F0[] = {{0x36, 0x587740}, {0x3E, 0x530530}, {0x4F, 0x587740},  {0xB5, 0x531CF0}, {0xEE, 0x587740},
                                         {0x100, 0x51D4E0}, {0x116, 0x51D4E0}, {0x12A, 0x51D4E0}, {0x143, 0x589410}};
constexpr sh::CallSite kCalls51D440[] = {{0x1C, 0x5366A0}, {0x3E, 0x589410}, {0x50, 0x5891F0}, {0x64, 0x5893A0}, {0x74, 0x589410}};
constexpr sh::CallSite kCalls51D4E0[] = {{0xD, 0x536700},   {0x49, 0x522FB0},  {0x53, 0x587740},  {0x5B, 0x5B93D2},  {0x74, 0x522FB0},
                                         {0x7D, 0x591680},  {0xAD, 0x590BB0},  {0xBE, 0x587740},  {0xC4, 0x497710},  {0xE3, 0x497710},
                                         {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710}, {0x154, 0x522FB0},
                                         {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls51D690[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls51D6D0[] = {{0x37, 0x5893A0}};

#define R1A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R1A_CALLS(a) a, R1A_N(a)
#define R1A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape}
const sh::Clone kAll[] = {
    {"Member_ResumeUnlessHeld800", 0x51BA80, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(Member_ResumeUnlessHeld800), 0, false, kSp},
    {"Member_FormActionState", 0x51BAA0, 0x125, R1A_CALLS(kCalls51BAA0), nullptr, 0, nullptr, 0, R1A_FN(Member_FormActionState), 0, false, kSp},
    {"Member_JumpState", 0x51BCF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(Member_JumpState), 0, false, kSp},
    {"Member_JumpAir", 0x51BD10, 0x8D, R1A_CALLS(kCalls51BD10), nullptr, 0, nullptr, 0, R1A_FN(Member_JumpAir), 0, false, kSp},
    {"PartyFormAction0_Form0", 0x51BE90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction0_Form0), 0, false, kSp},
    {"PartyFormAction_Form0Begin", 0x51BEB0, 0xFC, R1A_CALLS(kCalls51BEB0), nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction_Form0Begin), 0, false, kSp},
    {"PartyAction0_Form0", 0x51BFB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form0), 0, false, kSp},
    {"PartyAction0_Form0Begin", 0x51BFD0, 0x1B1, R1A_CALLS(kCalls51BFD0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form0Begin), 0, false, kSp},
    {"PartyAction0_Form0Resolve", 0x51C190, 0xDB, R1A_CALLS(kCalls51C190), nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form0Resolve), 0, false, kSp},
    {"PartyAction0_CellPickup", 0x51C270, 0x11F, R1A_CALLS(kCalls51C270), nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction0_Form1", 0x51C430, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction0_Form1), 0, false, kSp},
    {"PartyAction0_Form1", 0x51C450, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form1), 0, false, kSp},
    {"PartyFormAction0_Form2", 0x51C470, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction0_Form2), 0, false, kSp},
    {"PartyFormAction_Form2Turn", 0x51C490, 0x7E, R1A_CALLS(kCalls51C490), nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction_Form2Turn), 0, false, kSp},
    {"PartyAction0_Form2", 0x51C510, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form2), 0, false, kSp},
    {"PartyAction0_Form2Begin", 0x51C530, 0x166, R1A_CALLS(kCalls51C530), nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_Form2Begin), 0, false, kSp},
    {"PartyFormAction0_ByForm", 0x51C740, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction0_ByForm), 0, false, kSp},
    {"PartyAction0_ByForm", 0x51C760, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction0_ByForm), 0, false, kSp},
    {"PartyFormAction1_Form0", 0x51C780, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction1_Form0), 0, false, kSp},
    {"PartyAction1_Form0", 0x51C7A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_Form0), 0, false, kSp},
    {"PartyAction1_Form0Begin", 0x51C7C0, 0x1B1, R1A_CALLS(kCalls51C7C0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_Form0Begin), 0, false, kSp},
    {"PartyAction1_Form0Resolve", 0x51C980, 0xDB, R1A_CALLS(kCalls51C980), nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_Form0Resolve), 0, false, kSp},
    {"PartyAction1_CellPickup", 0x51CA60, 0x11F, R1A_CALLS(kCalls51CA60), nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction1_Form1", 0x51CB80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction1_Form1), 0, false, kSp},
    {"PartyAction1_Form1", 0x51CBA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_Form1), 0, false, kSp},
    {"PartyFormAction1_Form2", 0x51CBC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction1_Form2), 0, false, kSp},
    {"PartyAction1_Form2", 0x51CBE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_Form2), 0, false, kSp},
    {"PartyFormAction1_ByForm", 0x51CC00, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction1_ByForm), 0, false, kSp},
    {"PartyAction1_ByForm", 0x51CC20, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction1_ByForm), 0, false, kSp},
    {"PartyFormAction2_Form0", 0x51CC40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction2_Form0), 0, false, kSp},
    {"PartyFormAction_Form0Turn", 0x51CC60, 0x83, R1A_CALLS(kCalls51CC60), nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction_Form0Turn), 0, false, kSp},
    {"PartyAction2_Form0", 0x51CCF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form0), 0, false, kSp},
    {"PartyAction2_Form0Begin", 0x51CD10, 0x1B1, R1A_CALLS(kCalls51CD10), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form0Begin), 0, false, kSp},
    {"PartyAction2_Form0Resolve", 0x51CED0, 0xDB, R1A_CALLS(kCalls51CED0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form0Resolve), 0, false, kSp},
    {"PartyAction2_CellPickup", 0x51CFB0, 0x11F, R1A_CALLS(kCalls51CFB0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_CellPickup), 0xFFu, false, kCa},
    {"PartyFormAction2_Form1", 0x51D0D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction2_Form1), 0, false, kSp},
    {"PartyFormAction_Form1Begin", 0x51D0F0, 0x70, R1A_CALLS(kCalls51D0F0), nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction_Form1Begin), 0, false, kSp},
    {"PartyFormAction_Form1Turn", 0x51D160, 0x7C, R1A_CALLS(kCalls51D160), nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction_Form1Turn), 0, false, kSp},
    {"PartyAction2_Form1", 0x51D1E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form1), 0, false, kSp},
    {"PartyFormAction2_Form2", 0x51D200, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyFormAction2_Form2), 0, false, kSp},
    {"PartyAction2_Form2", 0x51D220, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2), 0, false, kSp},
    {"PartyAction2_Form2State0", 0x51D240, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2State0), 0, false, kSp},
    {"PartyAction2_Form2Aim", 0x51D260, 0x87, R1A_CALLS(kCalls51D260), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2Aim), 0, false, kSp},
    {"PartyAction2_Form2Strike", 0x51D2F0, 0x14C, R1A_CALLS(kCalls51D2F0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2Strike), 0, false, kSp},
    {"PartyAction_FinishPalette", 0x51D440, 0x94, R1A_CALLS(kCalls51D440), nullptr, 0, nullptr, 0, R1A_FN(PartyAction_FinishPalette), 0, false, kSp},
    {"PartyAction2_CellStrike", 0x51D4E0, 0x188, R1A_CALLS(kCalls51D4E0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_CellStrike), 0xFFu, false, kCa},
    {"PartyAction2_Form2State1", 0x51D670, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2State1), 0, false, kSp},
    {"PartyAction2_Form2Reaim", 0x51D690, 0x35, R1A_CALLS(kCalls51D690), nullptr, 0, nullptr, 0, R1A_FN(PartyAction2_Form2Reaim), 0, false, kSp},
    {"PartyAction_WaitEffect", 0x51D6D0, 0x3D, R1A_CALLS(kCalls51D6D0), nullptr, 0, nullptr, 0, R1A_FN(PartyAction_WaitEffect), 0, false, kSp},
};
#undef R1A_FN
#undef R1A_CALLS
#undef R1A_N

// One enum entry a clone, in kAll's order (the seed switches on it).
enum : unsigned {
    kResume800, kFormActionState, kJumpState, kJumpAir, kFA0Form0, kFAForm0Begin, kA0Form0, kA0Form0Begin, kA0Resolve,
    kA0Pickup, kFA0Form1, kA0Form1, kFA0Form2, kFAForm2Turn, kA0Form2, kA0Form2Begin, kFA0ByForm, kA0ByForm, kFA1Form0,
    kA1Form0, kA1Form0Begin, kA1Resolve, kA1Pickup, kFA1Form1, kA1Form1, kFA1Form2, kA1Form2, kFA1ByForm, kA1ByForm,
    kFA2Form0, kFAForm0Turn, kA2Form0, kA2Form0Begin, kA2Resolve, kA2Pickup, kFA2Form1, kFAForm1Begin, kFAForm1Turn,
    kA2Form1, kFA2Form2, kA2Form2, kA2State0, kA2Aim, kA2Strike, kFinishPalette, kA2CellStrike, kA2State1, kA2Reaim,
    kWaitEffect, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

// The dispatchers: the index byte each reads (2 or 3: Sprite_Current +2 / +3;
// 0x2C: the u16 there) and its table's length (the run to the next table's
// start, read by hand: docs/rest_1a.md section 2).
struct Dispatch { unsigned k, at, count; };
constexpr Dispatch kDispatch[] = {
    {kJumpState, 2, 4},  {kFA0Form0, 2, 3},  {kA0Form0, 2, 3},   {kFA0Form1, 2, 3},  {kA0Form1, 2, 2},
    {kFA0Form2, 2, 3},   {kA0Form2, 2, 2},   {kFA0ByForm, 0x2C, 3}, {kA0ByForm, 0x2C, 3}, {kFA1Form0, 2, 3},
    {kA1Form0, 2, 3},    {kFA1Form1, 2, 3},  {kA1Form1, 2, 2},   {kFA1Form2, 2, 3},  {kA1Form2, 2, 3},
    {kFA1ByForm, 0x2C, 3}, {kA1ByForm, 0x2C, 3}, {kFA2Form0, 2, 3}, {kA2Form0, 2, 3}, {kFA2Form1, 2, 3},
    {kA2Form1, 2, 2},    {kFA2Form2, 2, 3},  {kA2Form2, 2, 2},   {kA2State0, 3, 5},  {kA2State1, 3, 3},
};

// The 26 tables the group reads, each swapped for recorders while the fuzz
// runs. A table's entries register one recorder per address, so the tables
// holding the same handlers (0x65F9B4 / 0x65FA0C / 0x65FA68, 0x65F9CC /
// 0x65FA24 / 0x65FA80, 0x65F9D8 / 0x65FA30 / 0x65FA8C) share theirs.
const sh::DataTable kTables[] = {
    {at::kMemberJumpSteps, 4},        {at::kFormAction0Form0States, 3}, {at::kAction0Form0States, 3},
    {at::kFormAction0Form1States, 3}, {at::kAction0Form1States, 2},     {at::kFormAction0Form2States, 3},
    {at::kAction0Form2States, 2},     {at::kFormAction0Forms, 3},       {at::kAction0Forms, 3},
    {at::kFormAction1Form0States, 3}, {at::kAction1Form0States, 3},     {at::kFormAction1Form1States, 3},
    {at::kAction1Form1States, 2},     {at::kFormAction1Form2States, 3}, {at::kAction1Form2States, 3},
    {at::kFormAction1Forms, 3},       {at::kAction1Forms, 3},           {at::kFormAction2Form0States, 3},
    {at::kAction2Form0States, 3},     {at::kFormAction2Form1States, 3}, {at::kAction2Form1States, 2},
    {at::kFormAction2Form2States, 3}, {at::kAction2Form2States, 2},     {at::kAction2Form2State0Steps, 5},
    {at::kAction2Form2State1Steps, 3}, {at::kFormActions, at::kFormActionCount},
};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Sc() { return Sprite_Current; }

// --- the stand-ins' answers (Noise() and the state only: both passes the same) -----

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// Sprite_ObjectAt: none (0xFF) half the time, else what it answers - an index
// 0..0x21 (0x1D, 0x1E and 0x21 often: either side of the extra four's start).
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    static const U kEdges[] = {0, 0x1D, 0x1E, 0x21, 1, 0x1F};
    return WithAl(answer, n % 2 ? 0xFF : (n >> 4) % 3 ? (n >> 8) % 0x22 : kEdges[(n >> 12) % 6]);
}
// AreaMap_ByteAt: each code the cell tests compare with, and neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF8, 0xEF, 0xF9, 0x00, 0xFF};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// Effect_FindFree, Field_EffectAhead, PartyAction_Kind30Ahead: none (0xFF) a
// third of the time, else a record 0..19 (what each answers).
U FxRecord(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 3 == 0 ? 0xFF : (n >> 4) % 20);
}
// Rand: its low nibble at the draws' edges most of the time (& 0xF against 7,
// 0xB, 0xD and 0xF; & 7 against 5; & 3 against 0), bits above anything.
U FxRand(const U*, U answer) {
    static const U kNibbles[] = {0, 3, 4, 5, 6, 7, 8, 0xB, 0xC, 0xD, 0xE, 0xF, 0xF, 0xD};
    const U n = sh::Noise();
    const U nib = n % 4 == 0 ? (n >> 4) & 0xF : kNibbles[(n >> 4) % (sizeof kNibbles / sizeof kNibbles[0])];
    return (answer & 0x7FF0u) | nib;
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
// Sprite_TurnSense: 0xFF or 1 (what it answers), any byte one time in eight.
U FxTurnSense(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : n % 2 ? 0xFF : 1);
}
// PartyAction_SideProbes writes Sprite_Current +0x2B (0 or 1).
U FxSideProbes(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s + 0x2B, 1)) s[0x2B] = static_cast<unsigned char>(sh::Noise() % 2);
    return answer;
}

// Masks by what each callee reads (symbols.toml's types and evidence, re-read
// where a caller pushes a whole register for a byte or a word): the map cells
// as words (AreaMap_ByteAt, AreaMap_ClearCell, Effect_SpawnAtCell(High) and
// the group's own cell helpers read 16 bits each - the originals push them
// with their own stack's upper halves), the state bytes and member indices as
// bytes (Member_ClearState: and eax, 0xFF; the effect index of R0A's two:
// its low byte), MapView_SlopeAt's direction as a byte (pushed in bl over the
// caller's ebx), every other argument whole.
#define R1A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // unsigned char (long x, long y, unsigned margin)
    {R1A_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    // unsigned char (short x, short y)
    {R1A_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    // unsigned char (void)
    {R1A_OURS(Effect_FindFree), 0, {}, kG, 0, 0, {}, &FxRecord},
    {R1A_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxRecord},
    {R1A_OURS(PartyAction_Kind30Ahead), 0, {}, kG, 0, 0, {}, &FxRecord},
    {R1A_OURS(PartyAction_TargetAhead), 0, {}, kF, 0, 0, {}},
    {R1A_OURS(PartyAction_BlockedAhead), 0, {}, kF, 0, 0, {}},
    // unsigned char (unsigned index): the low byte read
    {R1A_OURS(PartyAction_MemberOnEffect), 1, {kU8}, kF, 0, 0, {}},
    {R1A_OURS(PartyAction_MemberBeyondEffect), 1, {kU8}, kF, 0, 0, {}},
    // void (void)
    {R1A_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0, {}, &FxSideProbes},
    {R1A_OURS(Leader_Rise), 0, {}, kG, 0, 0, {}},
    {R1A_OURS(Leader_Sink), 0, {}, kG, 0, 0, {}},
    {R1A_OURS(Leader_TurnBack), 0, {}, kG, 0, 0, {}},
    {R1A_OURS(Member_Follow), 0, {}, kG, 0, 0, {}},
    // void (unsigned state, unsigned x, unsigned z): the state's byte, the words
    {R1A_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    {R1A_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0, {}},
    // void (unsigned x, unsigned z)
    {R1A_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, kG, 0, 0, {}},
    // int (void): Capcom's C runtime
    {"Rand", 0x5B93D2u, 0x5B93D2u, 0, {}, kG, 0, 0, {}, &FxRand},
    // void (unsigned amount): the caller's & 0xFF whole
    {R1A_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0, {}},
    // long (long x, long y, unsigned long direction)
    {R1A_OURS(MapView_SlopeAt), 3, {kW, kW, kU8}, kG, 0, 0, {}, &FxSlope},
    // long (long x, long z)
    {R1A_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    // unsigned char (unsigned target): the target's byte
    {R1A_OURS(Sprite_TurnSense), 1, {kU8}, kG, 0, 0, {}, &FxTurnSense},
    // void (unsigned member): and eax, 0xFF
    {R1A_OURS(Member_ClearState), 1, {kU8}, kG, 0, 0, {}},
    // void (unsigned colour): the low byte indexes its table (pushed 0)
    {R1A_OURS(Sprite_FlashClut), 1, {kW}, kG, 0, 0, {}},
    // void (unsigned short *dst, unsigned index): dst logged by value - the
    // standard row hashes 8 bytes at it, which the callee only writes, and the
    // palettes at 0x80D380 are alike at start-up (a stride planted 0x20 for
    // 0x40 passed that row)
    {R1A_OURS(Sprite_LoadPalette), 2, {kW, kW}, kG, 0, 0, {}},
    // the group's own cell helpers, called by E8: (x, z) as words, al tested
    {R1A_OURS(PartyAction0_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1A_OURS(PartyAction1_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1A_OURS(PartyAction2_CellPickup), 2, {kU16, kU16}, kF, 0, 0, {}},
    {R1A_OURS(PartyAction2_CellStrike), 2, {kU16, kU16}, kF, 0, 0, {}},
};
#undef R1A_OURS

// Beyond field mode's standard regions: Text_Records' first record, which the
// two item paths copy a name into.
const sh::Region kRegions[] = {
    {bof3::addr::Text_Records, 0x10},
};

// --- the seed -------------------------------------------------------------------------

// A 16.16 coordinate: a cell at 0, small, at the s16 and u16 limits or random;
// a fraction 0 (often), a half, a quarter, 1, 0xFFFF or random.
U Coordinate() {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
// A direction: 0..7 mostly; above that the original reads Field_DirectionSteps
// past its 8 rows (in place, the same on both passes).
unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }
U StepX(unsigned d) { return static_cast<U>(Long(sh::Mem(at::kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(sh::Mem(at::kSteps + d * 8 + 4))); }
unsigned char* Member(unsigned i) { return sh::ObjectOf(i); }

void SeedPosition() {
    unsigned char* const s = Sc();
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
}
// The point two steps ahead with no fraction in x, in z, or both, half the
// time each (the resolve's and the strike's cell probes one on).
void SeedPointWhole() {
    unsigned char* const s = Sc();
    const unsigned d = s[8];
    if (sh::Half()) SetLong(s + 0x34, static_cast<std::int32_t>((sh::Next() << 16) - 2u * StepX(d)));
    if (sh::Half()) SetLong(s + 0x38, static_cast<std::int32_t>((sh::Next() << 16) - 2u * StepZ(d)));
}
// A height word around another: equal, within, at and past 0x80 each way, at
// the s16 limits, or random.
U HeightNear(U h) {
    return (h + PickOf(0, 1, 0xFFFF, 0x80, 0x81, 0xFF80, 0xFF7F, 0x7F, 0x7FFF, 0x8000, sh::Next())) & 0xFFFF;
}

// A member state runs on a party member's ObjTrio record, as
// Field_MemberFrame runs it; +6 the member it follows, 0..2.
unsigned char* AsMember() {
    unsigned char* const s = Member(sh::Next());
    Sprite_Current = s;
    s[6] = static_cast<unsigned char>(sh::Next() % 3);
    return s;
}

void SeedFormAction() {
    unsigned char* const s = AsMember();
    sh::Mem(at::kLeaderKind)[0] = static_cast<unsigned char>(PickOf(3, 6, 3, 6, 0, 2, 7, sh::Next()));
    if (sh::Half()) sh::Mem(at::kLeaderState)[0] = 0xA;
    if (sh::Half()) sh::Mem(at::kLeaderStep)[0] = 1;
    if (sh::Often()) Field_Request = 0;
    unsigned char* const r = Member(s[6]);
    r[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, sh::Next()));
    SetLong(r + 0xC, static_cast<std::int32_t>(PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, sh::Next())));
    SetLong(r + 0x10, static_cast<std::int32_t>(PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, sh::Next())));
    r[0x70] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next() | 1));
    const U k = r[9];
    const U ax = static_cast<U>(Long(r + 0xC)) * k + static_cast<U>(Long(r + 0x34));
    const U az = static_cast<U>(Long(r + 0x10)) * k + static_cast<U>(Long(r + 0x38));
    static const U kD[] = {0, 1, 0x17FFF, 0x18000, 0x18001, 0x1FFFF, 0x20000, 0x20001, 0xFFFE8000u, 0xFFFE7FFFu, 0xFFFE0000u,
                           0xFFFDFFFFu, 0x80000000u};
    if (s != r) {
        SetLong(s + 0x34, static_cast<std::int32_t>(ax + (sh::Often() ? sh::Pick(kD, 13) : sh::Next())));
        SetLong(s + 0x38, static_cast<std::int32_t>(az + (sh::Often() ? sh::Pick(kD, 13) : sh::Next())));
    }
    if (sh::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1C00u);
    sh::Mem(at::kPartySet)[0] = static_cast<unsigned char>(sh::Next() % at::kFormActionCount | (sh::Half() ? 0x80 : 0));
    if (sh::Half()) Field_State[0x137] = 0;
}

void SeedJumpAir() {
    unsigned char* const s = AsMember();
    unsigned char* const r = Member(s[6]);
    if (sh::Half()) r[0x137] = 3;
    SetWord(r + 0x3E, PickOf(0, 0x7FFF, 0x8000, 0xFF80, sh::Next()));
    if (s != r) SetWord(s + 0x3E, HeightNear(Word(r + 0x3E)));
}

// A turning state: +8 at +3 half the time, +3 one of the facings compared,
// the countdown +9 at 1 often.
void SeedTurn(unsigned char lo, unsigned char hi) {
    unsigned char* const s = Sc();
    s[3] = static_cast<unsigned char>(PickOf(lo, hi, lo, hi, sh::Next() % 8, sh::Next()));
    s[8] = static_cast<unsigned char>(sh::Half() ? s[3] : sh::Next() % 16);
    s[9] = static_cast<unsigned char>(PickOf(1, 1, 1, 2, 0, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(1, 0xFF, sh::Next()));
}

void Seed(unsigned k) {
    unsigned char* s = Sc();
    for (const Dispatch& d : kDispatch)
        if (d.k == k) {
            const U i = sh::Next() % d.count;
            if (d.at == 0x2C) SetWord(s + 0x2C, i);
            else s[d.at] = static_cast<unsigned char>(i);
            return;
        }
    switch (k) {
    case kResume800:
        AsMember();
        if (sh::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x800u);
        break;
    case kFormActionState: SeedFormAction(); break;
    case kJumpAir: SeedJumpAir(); break;
    case kFAForm0Begin:
        s[8] = Direction();
        if (sh::Half()) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ 0x40);
        break;
    case kA0Form0Begin:
    case kA1Form0Begin:
    case kA2Form0Begin:
    case kA2Aim:
        s[8] = Direction();
        SeedPosition();
        break;
    case kA0Resolve:
    case kA1Resolve:
    case kA2Resolve:
        s[0xA] = static_cast<unsigned char>(PickOf(1, 1, 1, 1, 2, 0, 0xFF));
        s[8] = Direction();
        SeedPosition();
        SeedPointWhole();
        break;
    case kA0Pickup:
    case kA1Pickup:
    case kA2Pickup:
    case kA2CellStrike:
        if (sh::Half()) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags & ~6u);
        break;
    case kFAForm0Turn:
        SeedTurn(3, 5);
        s[4] = static_cast<unsigned char>(PickOf(0x50, 0x50, 0x40, 0x41, 0x52, sh::Next()));
        break;
    case kFAForm1Begin: s[8] = static_cast<unsigned char>(sh::Next() % 16); break;
    case kFAForm1Turn: SeedTurn(1, 7); break;
    case kFAForm2Turn: SeedTurn(3, 5); break;
    case kA0Form2Begin:
        s[8] = Direction();
        SeedPosition();
        if (sh::Half()) Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] & ~1u);
        break;
    case kA2Strike:
        s[0xA] = static_cast<unsigned char>(PickOf(1, 1, 1, 0, 2, sh::Next()));
        s[8] = Direction();
        SeedPosition();
        SeedPointWhole();
        break;
    case kFinishPalette:
        s[0xB] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 2, 3, sh::Next()));
        if (sh::Half()) Field_Request = 0;
        break;
    case kWaitEffect: {
        s[0xB] = static_cast<unsigned char>(sh::Next() % 20);
        unsigned char* const e = sh::EffectRecord(s[0xB]);
        e[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        e[1] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, sh::Next()));
        if (sh::Half()) Field_Kind2Hold = 0;
        break;
    }
    default: break;
    }
}

// The cell helpers' (x, z): a cell's word at 0, 1, the s16 and u16 limits or
// random, the upper half random (each callee reads the word).
void Args(unsigned k, U* a) {
    switch (k) {
    case kA0Pickup:
    case kA1Pickup:
    case kA2Pickup:
    case kA2CellStrike:
        for (unsigned i = 0; i < 2; ++i)
            a[i] = (a[i] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40, sh::Next()) & 0xFFFF);
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// state bytes, counters, pose, position and height; Field_State +0x137 /
// +0x138; DamageScratch's flag; Field_InputFlags; Text_Records' first dword.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sc();
    if (!sh::InRegions(s, 0x40)) return;
    switch (h % 11) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: s[2 + v % 2] = static_cast<unsigned char>(v >> 1); break;
    case 2: s[9 + v % 3] = static_cast<unsigned char>(v & 0x10 ? (v >> 5) : (v >> 5) % 3); break;
    case 3: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 4: SetWord(s + 0x3E, v >> 2); break;
    case 5: {
        unsigned char* const f = Field_State;
        if (sh::InRegions(f + 0x137, 2)) f[0x137 + v % 2] = static_cast<unsigned char>(v >> 1);
        break;
    }
    case 6: sh::Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 7: Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ (v & 1 ? 2 : 4)); break;
    case 8: s[4 + (v % 2)] = static_cast<unsigned char>(v >> 1); break;
    case 9: SetWord(s + 0x2C, v % 3); break;
    case 10: SetLong(sh::Mem(bof3::addr::Text_Records + 4 * (v % 4)), static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_1a: BOF3X_R1A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_1a
