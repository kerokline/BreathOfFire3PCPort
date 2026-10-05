// BOF3X_SHADOW=rest_2b: group R2B's 38 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 7), once at start-up. docs/rest_2b.md section 4.
// BOF3X_R2B_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R2B --clones --harness scenario
// (2026-10-04, through the scratch wrapper band14.py) with the names given and
// three changes the capstone read made: the three SCENA rows are jump-table
// cases (dropped), 0x57EDE0's extent stops at its jmp (0xC: 0x57EDF0 is a
// function of its own, added), and 0x57E720 (code no list had) is added.
// Every other extent and call site agrees with the read. The screen's states
// and steps are kMenu, the models' states kState, the helpers kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2b.h"
#include "game/rest_2b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-04), each checked against the capstone read.
constexpr sh::CallSite kCalls57CE10[] = {{0x1, 0x589810}};
constexpr sh::CallSite kCalls57CE80[] = {{0x1, 0x589810}};
constexpr sh::CallSite kCalls57CEF0[] = {{0x8, 0x5A7650}, {0x65, 0x461E50}};
constexpr sh::CallSite kCalls57D520[] = {{0xC2, 0x57D760}, {0xF3, 0x57D760}, {0x12B, 0x57D760}, {0x166, 0x57D760},
                                         {0x19B, 0x57D760}, {0x1D0, 0x57D760}, {0x202, 0x57D760}, {0x22A, 0x57D760}};
constexpr sh::CallSite kCalls57E010[] = {{0x1, 0x57E9A0}, {0x32, 0x57EAE0}, {0x3C, 0x57ECC0}};
constexpr sh::CallSite kCalls57E080[] = {{0x2, 0x495040}, {0xA, 0x57E9F0}};
constexpr sh::CallSite kCalls57E0A0[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls57E0F0[] = {{0x6, 0x575690}, {0xD, 0x495040}, {0x15, 0x57EAC0}};
constexpr sh::CallSite kCalls57E120[] = {{0x34, 0x575690}};
constexpr sh::CallSite kCalls57E160[] = {{0x19, 0x591B60}, {0x2B, 0x591B60}, {0x3D, 0x591B60}, {0x4E, 0x591B60},
                                         {0x5D, 0x57EB20}, {0x84, 0x59E330}, {0x9D, 0x59E330}, {0xB7, 0x59E330},
                                         {0xD2, 0x59E330}, {0xF0, 0x59E330}, {0x104, 0x59E330}, {0x118, 0x59E330}};
constexpr sh::CallSite kCalls57E290[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls57E2B0[] = {{0xC, 0x461EB0}, {0x1E, 0x587740}, {0x50, 0x587740}, {0x83, 0x587740},
                                         {0xF0, 0x587740}, {0x10B, 0x587740}, {0x123, 0x587740}, {0x13E, 0x587740},
                                         {0x15F, 0x587740}, {0x1CE, 0x57EDD0}, {0x1D3, 0x57F320}};
constexpr sh::CallSite kCalls57E490[] = {{0xC, 0x461EB0}, {0x36, 0x587740}, {0x84, 0x587740}, {0x204, 0x587740},
                                         {0x26F, 0x57EDD0}, {0x274, 0x57F320}};
constexpr sh::JumpTable kTables57E490[] = {{0xE1, 0x27C, 4}};
constexpr sh::CallSite kCalls57E720[] = {{0xE, 0x57EDD0}, {0x13, 0x57F320}};
constexpr sh::CallSite kCalls57E810[] = {{0x80, 0x587740}};
constexpr sh::CallSite kCalls57E9A0[] = {{0x6, 0x5919B0}, {0x16, 0x5919B0}, {0x26, 0x5919B0}, {0x36, 0x5919B0}};
constexpr sh::CallSite kCalls57ECC0[] = {{0x6A, 0x5720C0}, {0xEC, 0x5720C0}};
constexpr sh::CallSite kCalls57EDE0[] = {{0x7, 0x57EDF0}};
constexpr sh::CallSite kCalls57EDF0[] = {{0xCF, 0x57EEF0}};
constexpr sh::CallSite kCalls57EEF0[] = {{0x28, 0x5A7B90}, {0x3E, 0x57BFF0}, {0x48, 0x5A8DE0}, {0x52, 0x5A8E00},
                                         {0x7F, 0x5A8120}, {0xA4, 0x57BED0}, {0xAE, 0x57C070}, {0xBB, 0x5A8DA0},
                                         {0xFB, 0x5A79A0}, {0x12B, 0x5A75D0}, {0x21A, 0x5A85F0}, {0x220, 0x5A9290},
                                         {0x2C3, 0x5A8C00}, {0x2F7, 0x5A8CA0}, {0x314, 0x5A79E0}, {0x323, 0x5A7780},
                                         {0x32F, 0x4941B0}, {0x340, 0x461E50}, {0x361, 0x5A7BC0}};
constexpr sh::CallSite kCalls57F270[] = {{0x29, 0x57EDF0}};
constexpr sh::CallSite kCalls57F2A0[] = {{0x1F, 0x5720C0}, {0x43, 0x5720C0}, {0x67, 0x57EDF0}};
constexpr sh::CallSite kCalls57F310[] = {{0x0, 0x57EDF0}};
constexpr sh::CallSite kCalls57F330[] = {{0x7, 0x57F340}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kMe = Shape::kMenu;
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {G_LEAF(Field_TriggerCounterF0, 0x56E040, 0xA, kCa, 0xFFu)},
    {G_CLONE(Effect_Spawn, 0x57CE10, 0x63, kCalls57CE10, kCa, 0xFFu)},
    {G_CLONE(Effect_SpawnAt, 0x57CE80, 0x69, kCalls57CE80, kCa, 0xFFu)},
    {G_CLONE(Menu_DrawGreyHLine, 0x57CEF0, 0x6F, kCalls57CEF0, kCa, 0)},
    {G_CLONE(Menu_DrawOutlineNotched, 0x57D520, 0x23A, kCalls57D520, kCa, 0)},
    {G_LEAF(Shisu_ModeDispatch, 0x57DFF0, 0x11, kMe, 0)},
    {G_CLONE(Shisu_Begin, 0x57E010, 0x5B, kCalls57E010, kMe, 0)},
    {G_LEAF(Shisu_OpenDispatch, 0x57E070, 0xE, kMe, 0)},
    {G_CLONE(Shisu_OpenFade, 0x57E080, 0x16, kCalls57E080, kMe, 0)},
    {G_CLONE(Shisu_OpenWait, 0x57E0A0, 0x35, kCalls57E0A0, kMe, 0)},
    {G_LEAF(Shisu_CloseDispatch, 0x57E0E0, 0xE, kMe, 0)},
    {G_CLONE(Shisu_CloseFade, 0x57E0F0, 0x21, kCalls57E0F0, kMe, 0)},
    {G_CLONE(Shisu_CloseWait, 0x57E120, 0x3B, kCalls57E120, kMe, 0)},
    {G_CLONE(Shisu_Result, 0x57E160, 0x125, kCalls57E160, kMe, 0)},
    {G_CLONE(Shisu_PickDispatch, 0x57E290, 0x1D, kCalls57E290, kMe, 0)},
    {G_CLONE(Shisu_PickSide, 0x57E2B0, 0x1D8, kCalls57E2B0, kMe, 0)},
    {"Shisu_PickCounts", 0x57E490, 0x28C, kCalls57E490, SH_N(kCalls57E490), nullptr, 0, kTables57E490, SH_N(kTables57E490),
     G_FN(Shisu_PickCounts), 0, false, kMe},
    {G_CLONE(Shisu_PickShow, 0x57E720, 0x18, kCalls57E720, kMe, 0)},
    {G_LEAF(Shisu_ShowStart, 0x57E740, 0x13, kMe, 0)},
    {G_LEAF(Shisu_ShowDrop, 0x57E760, 0x25, kMe, 0)},
    {G_LEAF(Shisu_ShowLanded, 0x57E790, 0x76, kMe, 0)},
    {G_CLONE(Shisu_ShowFlash, 0x57E810, 0xAA, kCalls57E810, kMe, 0)},
    {G_LEAF(Shisu_ShowFade, 0x57E8C0, 0xDA, kMe, 0)},
    {G_CLONE(Shisu_CountItems, 0x57E9A0, 0x44, kCalls57E9A0, kCa, 0)},
    {G_LEAF(Shisu_SetupWindows, 0x57E9F0, 0xC4, kCa, 0)},
    {G_LEAF(Shisu_CloseWindows, 0x57EAC0, 0x19, kCa, 0)},
    {G_LEAF(Shisu_ScaleIndex, 0x57EAE0, 0x37, kCa, 0xFFu)},
    {G_LEAF(Shisu_Score, 0x57EB20, 0x19F, kCa, 0)},
    {G_CLONE(Shisu_InitModels, 0x57ECC0, 0x108, kCalls57ECC0, kCa, 0)},
    {G_LEAF(Shisu_ModelBDispatch, 0x57EDD0, 0xE, kSt, 0)},
    {G_CLONE(Shisu_ModelBTurn, 0x57EDE0, 0xC, kCalls57EDE0, kSt, 0)},
    {G_CLONE(Shisu_ModelBDraw, 0x57EDF0, 0xF5, kCalls57EDF0, kSt, 0)},
    {G_CLONE(Shisu_DrawModel, 0x57EEF0, 0x377, kCalls57EEF0, kCa, 0)},
    {G_CLONE(Shisu_ModelBSquare, 0x57F270, 0x2E, kCalls57F270, kSt, 0)},
    {G_CLONE(Shisu_ModelBDrop, 0x57F2A0, 0x6C, kCalls57F2A0, kSt, 0)},
    {G_CLONE(Shisu_ModelBStill, 0x57F310, 0x5, kCalls57F310, kSt, 0)},
    {G_LEAF(Shisu_ModelADispatch, 0x57F320, 0xE, kSt, 0)},
    {G_CLONE(Shisu_ModelATurn, 0x57F330, 0xC, kCalls57F330, kSt, 0)},
};
#undef G_LEAF
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kTrigger, kSpawn, kSpawnAt, kHLine, kOutline, kModeDispatch, kBegin, kOpenDispatch, kOpenFade, kOpenWait,
    kCloseDispatch, kCloseFade, kCloseWait, kResult, kPickDispatch, kPickSide, kPickCounts, kPickShow, kShowStart,
    kShowDrop, kShowLanded, kShowFlash, kShowFade, kCountItems, kSetupWindows, kCloseWindows, kScaleIndex, kScore,
    kInitModels, kBDispatch, kBTurn, kBDraw, kDrawModel, kBSquare, kBDrop, kBStill, kADispatch, kATurn, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return Mem(a)[0]; }
std::int32_t Signed(U v) { return static_cast<std::int32_t>(v); }

// The cells (rest_2b.cpp's).
constexpr U kMode = 0x929F00, kState = 0x929F01, kStep = 0x929F02, kTimer = 0x929F04;
constexpr U kOwned = 0x9399E0, kGiven = 0x9399E4, kRounds = 0x9399E8, kSide = 0x9399E9, kCursor = 0x9399EA,
            kLevel = 0x9399EB, kScoreCell = 0x9399FC;
constexpr U kModelA = 0x9398E0, kModelB = 0x939960;
constexpr U kAState = kModelA + 1, kADone = kModelA + 6, kBState = kModelB + 1, kBDone = kModelB + 6,
            kBY = kModelB + 0x3C, kBColour = kModelB + 0x5D, kBAngle = kModelB + 0x6C;
constexpr U kSaved = 0x6BC878;
constexpr U kModelFile = 0x628C88, kLevelByte = 0x904101, kStyle = 0x903A5A;

// The scratch the models' headers and quads point into (the harness's ten
// argument slots, 0x40 bytes each, one region): headers at slot 0 (A +0, B
// +0x10, the file +0x20), quads from slot 1 (A's four, then B's four).
unsigned char* HeaderA() { return sh::Scratch(0); }
unsigned char* HeaderB() { return sh::Scratch(0) + 0x10; }
unsigned char* File() { return sh::Scratch(0) + 0x20; }
unsigned char* QuadsA() { return sh::Scratch(1); }
unsigned char* QuadsB() { return sh::Scratch(1) + 4 * 0x28; }

// --- the stand-ins' effects (Noise() and the state only: both passes the same) -----

float SmallFloat() { return static_cast<float>(static_cast<int>(sh::Noise() % 641) - 320); }
void PutFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// Input_AutoRepeat: an answer whose second byte takes each branch (0xA0, 0x10,
// 0x40, none), a third of the time the pressed word as it came.
U RepeatEffect(const U* a, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) return a[0];
    static const U kAnswers[] = {0, 0, 0x1000, 0x4000, 0x8000, 0x2000, 0xA000, 0x5000, 0x0100, 0xF000};
    return (n >> 2) % 5 == 0 ? answer : kAnswers[(n >> 4) % 10];
}
// Shisu_Score: the score at Shisu_Result's thresholds, give or take one.
U ScoreEffect(const U*, U answer) {
    static const U kScores[] = {0x32, 0x33, 0x31, 0x59, 0x5A, 0x77, 0x78, 0x95, 0x96, 0xA9, 0xAA, 0xB3, 0xB4,
                                0xB5, 0, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu};
    const U n = sh::Noise();
    SetLong(Mem(kScoreCell), Signed(n % 4 == 0 ? n : kScores[(n >> 2) % 18]));
    return answer;
}
// AreaMap_Elevation: two answers in three at model B's y's high word, give or
// take one (Shisu_ModelBDrop compares y + 0x200000 against it << 16).
U ElevationEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) return answer;
    const U high = static_cast<U>(Long(Mem(kBY))) >> 16;
    return (answer & 0xFFFF0000u) | ((high + (n >> 4) % 3 - 1) & 0xFFFFu);
}
// 0x4941B0 (the winding): ax at the callers' test's edge - 0, 1, -1, a high
// half with ax 0 - or anything.
U WindingEffect(const U*, U answer) {
    static const U kAnswers[] = {0, 1, 0xFFFF, 0x10000, 0xFFFF0000u, 0x7FFF, 0x8000};
    const U n = sh::Noise();
    return n % 3 == 0 ? answer : kAnswers[(n >> 2) % 7];
}
// Gfx_CommitPrim: the packet cursor on by the size, as the real one moves it.
U CommitEffect(const U* a, U answer) {
    unsigned char* const p = sh::Pointer(sh::at::kPacketNext);
    const U size = a[1] & 0xFF;
    if (sh::InRegions(p + size, 0x48)) sh::SetPointer(sh::at::kPacketNext, p + size);
    return answer;
}
// Sprite_ObjectMatrix: the whole MATRIX (on the caller's stack) filled - the
// copy, the scale and Camera_LoadMatrix read every byte of it.
U ObjectMatrixEffect(const U* a, U answer) {
    sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[0])), 0x20);
    return answer;
}
// Gte_ScaleMatrix: the rotation's nine shorts changed in place, as the real one
// scales them (so a control that copies before the scale is seen).
U ScaleEffect(const U* a, U answer) {
    sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[0])), 18);
    return answer;
}
// Light_ObjectDirection: out[0..2].
U LightEffect(const U* a, U answer) {
    sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[0])), 6);
    return answer;
}
// Gte_RotTransPers4: the four screen points, small whole floats.
U PersEffect(const U* a, U answer) {
    for (unsigned i = 4; i < 8; ++i) {
        auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[i]));
        if (!sh::InRegions(p, 8)) continue;
        PutFloat(p, SmallFloat());
        PutFloat(p + 4, SmallFloat());
    }
    return answer;
}
// Gte_PrimDepths4_10: the four depths +0x10, +0x20, +0x30, +0x40.
U DepthsEffect(const U* a, U answer) {
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    for (unsigned k = 1; k <= 4; ++k)
        if (sh::InRegions(p + 0x10 * k, 4)) PutFloat(p + 0x10 * k, static_cast<float>(sh::Noise() % 100) / 100.0f);
    return answer;
}
// Gte_VectorNormalS: the three shorts out.
U NormalEffect(const U* a, U answer) {
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    if (sh::InRegions(p, 6)) sh::FillBytes(p, 6);
    return answer;
}
// Gte_NormalColor as shipped: the colour in copied over the out.
U ColourEffect(const U* a, U answer) {
    const auto* const in = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    auto* const out = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    if (sh::InRegions(out, 3))
        for (unsigned i = 0; i < 3; ++i) out[i] = in[i];
    return answer;
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9) by the group's
    {G_OURS(Shisu_CountItems), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_ScaleIndex), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_InitModels), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_SetupWindows), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_CloseWindows), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_Score), 0, {}, kG, 0, 0, {}, &ScoreEffect},
    {G_OURS(Shisu_ModelBDispatch), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_ModelADispatch), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_ModelBDraw), 0, {}, kG, 0, 0},
    {G_OURS(Shisu_DrawModel), 1, {kAll}, kG, 0, 0, {0x80}},           // the record hashed: Shisu_ModelBDraw's scale and tint live only during the call
    // not ours yet: R2C's model A draw, R3G's winding (rest_2b_callees.h)
    {"0x57F340", at::kModelADraw, at::kModelADraw, 0, {}, kG, 0, 0},
    {"0x4941B0", at::kWinding, at::kWinding, 3, {kAll, kAll, kAll}, kG, 0, 0, {8, 8, 8}, &WindingEffect, nullptr, true},
    // ours, with the width each reads
    {G_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0},
    {G_OURS(Input_AutoRepeat), 1, {kAll}, kG, 0, 0, {}, &RepeatEffect},
    {G_OURS(Menu_DrawBackdrop), 1, {0xFF}, kG, 0, 0},                 // Config's byte; the caller pushes eax over leftovers
    {G_OURS(Transition_Start), 1, {0xFF}, kG, 0, 0},
    {G_OURS(Window_ResetAll), 0, {}, kG, 0, 0},
    {G_OURS(Inventory_Remove), 3, {0xFF, 0xFF, 0xFF}, kF, 0, 0},      // each argument's byte (SX's read); a fourth 0 pushed
    {G_OURS(Inventory_Count), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0},
    {G_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},
    {G_OURS(Menu_DrawLine), 8, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF, 0xFF, 3}, kG, 0, 0},   // s16 x / y, the colour bytes, abr & 3
    {G_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {G_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {G_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    {G_OURS(Gte_PopMatrix), 0, {}, kG, 0, 0},
    // the matrices are the callers' locals: logged by their bytes, not their address
    {G_OURS(Sprite_ObjectMatrix), 1, {0}, kG, 0, 0, {}, &ObjectMatrixEffect},
    {G_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {0x14}},
    {G_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {0x20}},
    {G_OURS(Gte_ScaleMatrix), 2, {0, 0}, kG, 0, 0, {18, 12}, &ScaleEffect},
    {G_OURS(Light_ObjectDirection), 2, {0, kAll}, kG, 0, 0, {0, 12}, &LightEffect},
    {G_OURS(Camera_LoadMatrix), 1, {0}, kG, 0, 0, {0x20}},
    {G_OURS(Gte_SetMatrix2), 1, {0}, kG, 0, 0, {6}},                 // the first row: the rest is the original's stale stack (L1)
    {G_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {G_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {G_OURS(Gte_RotTransPers4), 9, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0}, kG, 0, 0, {6, 6, 6, 6}, &PersEffect},
    {G_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0, {}, &DepthsEffect},
    {G_OURS(Gte_VectorNormalS), 2, {0, kAll}, kG, 0, 0, {12, 0}, &NormalEffect},
    {G_OURS(Gte_NormalColor), 3, {kAll, 0, kAll}, kG, 0, 0, {6, 4, 0}, &ColourEffect},
    {G_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {G_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
};
#undef G_OURS

// The tables the dispatchers read in place, swapped for recorders on both
// sides; each count is its reader's reach (docs/rest_2b.md section 3).
const sh::DataTable kTables[] = {
    {Key(Shisu_Modes), Shisu_Modes_count},
    {Key(Shisu_OpenStates), Shisu_OpenStates_count},
    {Key(Shisu_CloseStates), Shisu_CloseStates_count},
    {Key(Shisu_PickStates), Shisu_PickStates_count},
    {Key(Shisu_ShowSteps), Shisu_ShowSteps_count},
    {Key(Shisu_ModelBStates), Shisu_ModelBStates_count},
    {Key(MasterFigure_States), MasterFigure_States_count},
};

// Beyond field mode's standard regions: the two models and the screen's cells,
// the kept colours, WindowRecords, the rank byte, Game_Mode / Game_Step,
// Prim_VertexScratch, the model file pointer, the window colours of styles
// 0..7 (the style byte is seeded below 8).
const sh::Region kRegions[] = {
    {kModelA, 0x120},
    {kSaved, 8},
    {sh::at::kWindows, sh::at::kWindowCount * sh::at::kWindowStride},
    {0x903F68, 4},
    {0x66C7E8, 4},
    {0x9037A0, 0x40},
    {kModelFile, 4},
    {0x80B7A8, 0x200},
};

// --- the seed ---------------------------------------------------------------------------

// A model header (count 0..4, so the quads stay inside the scratch; flags any)
// and its four quads (random bytes, compared).
void SeedModel(unsigned char* record, unsigned char* header, unsigned char* quads) {
    header[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 1, 2));
    header[3] = static_cast<unsigned char>(PickOf(0, 0x40, 0x80, 0xC0, 0x41, 0x42, 0x43, 0xC3, sh::Next()));
    sh::SetPointer(Key(record + 0x54), header);
    sh::SetPointer(Key(record + 0x50), quads);
    record[0x48] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
}

void Seed(unsigned k) {
    // the style row inside the region; the screen's menu cells near their bounds
    B(kStyle) = static_cast<unsigned char>(sh::Next() % 8);
    B(kMode) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 0xFF, sh::Next()));
    B(kState) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, sh::Next()));
    B(kStep) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 0xFF, sh::Next()));
    B(kTimer) = static_cast<unsigned char>(PickOf(0, 8, 0x10, 0x78, 0x80, 0x88, 0xF0, 0xF8, sh::Next()));
    // the dispatchers' indexes below their tables' lengths (each reads its
    // byte before any call)
    switch (k) {
    case kModeDispatch: B(kMode) = static_cast<unsigned char>(sh::Next() % Shisu_Modes_count); break;
    case kOpenDispatch: B(kState) = static_cast<unsigned char>(sh::Next() % Shisu_OpenStates_count); break;
    case kCloseDispatch: B(kState) = static_cast<unsigned char>(sh::Next() % Shisu_CloseStates_count); break;
    case kPickDispatch: B(kState) = static_cast<unsigned char>(sh::Next() % Shisu_PickStates_count); break;
    case kPickShow: B(kStep) = static_cast<unsigned char>(sh::Next() % Shisu_ShowSteps_count); break;
    default: break;
    }
    B(kBState) = static_cast<unsigned char>(sh::Next() % Shisu_ModelBStates_count);
    B(kAState) = static_cast<unsigned char>(sh::Next() % MasterFigure_States_count);
    B(kADone) = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
    B(kBDone) = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
    // the counts, the rounds, the side and the row at their compares
    for (unsigned i = 0; i < 4; ++i) B(kOwned + i) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 0x63, sh::Next()));
    B(kGiven) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    for (unsigned i = 1; i < 4; ++i)
        B(kGiven + i) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 5, 0x13, 0x14, 0x15, sh::Next()));
    B(kRounds) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 7, 8, 9, sh::Next()));
    B(kSide) = static_cast<unsigned char>(PickOf(0, 0, 1, 1, 2, 0xFF));
    B(kCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0, 1, 2, 3, 0xFF, 4, sh::Next()));
    B(kLevel) = static_cast<unsigned char>(PickOf(0, 9, 0xA, 0xC, 0x10, 1, sh::Next()));
    B(kLevelByte) = static_cast<unsigned char>(PickOf(0x25, 0x26, 0x27, 0x28, 0x2C, 0x2D, 0x31, 0x32, 0x3B, 0x3C, 0x40,
                                                      0x41, 0x43, 0x44, 0, 0xFF, sh::Next()));
    // model B's colour against 0xFF - 3 g2 and 3 g2 (Shisu_ModelBDraw)
    const unsigned three = 3u * B(kGiven + 2);
    B(kBColour) = static_cast<unsigned char>(PickOf(0xFF - three, 0xFE - three, 0x100 - three, 0x80, 0xFF, 0, sh::Next()));
    B(kBColour + 1) = static_cast<unsigned char>(PickOf(0xFF - three, 0xFE - three, 0x100 - three, 0x80, 0xFF, sh::Next()));
    B(kBColour + 2) = static_cast<unsigned char>(PickOf(three, three + 1, three - 1, 0, 0x80, sh::Next()));
    SetLong(Mem(kBAngle), Signed(PickOf(0, 0x1000, 0x2000, 0xFC0, 0x40, 0xFFF, 0x1001, 0x800, 0x1800, sh::Next())));
    // model B's y on a whole unit half the time (Shisu_ModelBDrop compares it,
    // 0x200000 on, with the elevation << 16)
    if (sh::Half()) SetLong(Mem(kBY), Signed(sh::Next() & 0xFFFF0000u));
    // the two models' headers and quads, and the first four sprite records'
    // (the disturbance can move Sprite_Current onto one inside Shisu_DrawModel)
    SeedModel(Mem(kModelA), HeaderA(), QuadsA());
    SeedModel(Mem(kModelB), HeaderB(), QuadsB());
    for (unsigned i = 0; i < 4; ++i) SeedModel(sh::SpriteRecord(i), sh::Half() ? HeaderA() : HeaderB(), sh::Half() ? QuadsA() : QuadsB());
    sh::SetPointer(kModelFile, File());
    // the pad
    MoveScript_WaitWordDA = static_cast<unsigned short>(PickOf(0, 0, 1, sh::Next()));
    Input_Pressed = static_cast<unsigned short>(PickOf(0, 0x1000, 0x4000, 0x5000, 0x8000, 0x2000, 0xF000, 0x20, 0x40,
                                                       0x60, 0x10, sh::Next()));
    Field_ConfirmButtons = static_cast<unsigned short>(PickOf(0x20, 0x40, 0x60, 0, sh::Next()));
    Field_CancelButtons = static_cast<unsigned short>(PickOf(0x40, 0x10, 0, sh::Next()));
    // Sprite_Current: one of the first four sprite records (or a model)
    Sprite_Current = sh::Often() ? sh::SpriteRecord(sh::Next()) : Mem(sh::Half() ? kModelA : kModelB);
}

// The arguments of the kCall functions, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kDrawModel: a[0] = PickOf(kModelA, kModelB); break;
    case kHLine: a[3] = PickOf(0, 1, 0x100, 0xFF00, a[3]); break;
    case kOutline: a[4] = (a[4] & 0xFFFFFF00u) | PickOf(0, 1, 0xF0, 0xF1, 0x10, 0x21, 0x40, 0x81, a[4] & 0xFF); break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    switch (h % 13) {
    case 0: B(kMode) = static_cast<unsigned char>(b % 5); break;
    case 1: B(kState) = static_cast<unsigned char>(b % 3); break;
    case 2: B(kStep) = static_cast<unsigned char>(b % 5); break;
    case 3: B(kTimer) = static_cast<unsigned char>(b & 1 ? 0x80 : b); break;
    case 4: B(kRounds) = static_cast<unsigned char>(b % 10); break;
    case 5: B(kSide) = static_cast<unsigned char>(b & 1 ? b >> 1 & 1 : b); break;
    case 6: B(kCursor) = static_cast<unsigned char>(b % 5); break;
    case 7: B(kGiven + (b & 3)) = static_cast<unsigned char>((b >> 2) % 0x16); break;
    case 8: SetLong(Mem(kBY), Signed(v)); break;
    case 9: Game_Step = static_cast<unsigned short>(v); break;
    case 10: sh::SetPointer(kModelFile, File() + (b & 1 ? 0 : 0x10)); break;
    case 11: if (b & 1) Field_Kind2X = static_cast<long>(v); else Field_Kind2Z = static_cast<long>(v); break;
    case 12: SetLong(Mem(kBAngle), Signed(v)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R2B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_2b: BOF3X_R2B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.menu_span = 2;   // the shortest state table (Shisu_OpenStates, Shisu_CloseStates); the longer ones seeded per function
    sh::Run(g);
}

}  // namespace rest_2b
