// BOF3X_SHADOW=rest_1d: group R1D's 46 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_1d.md section 4. BOF3X_R1D_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R1D --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them. Shapes: the dispatchers and the state
// handlers kSprite (void, on Sprite_Current), the two cell probes kCall
// answering al. The 27 .data state tables the dispatchers read are DataTables
// (their entries recorders while the fuzz runs); each dispatcher's index byte
// (+2, +3 or the u16 +0x2C) is seeded below its own table's count. Every
// callee is re-listed here (registered before the standard rows: the group's
// listing stands) with the width the callee reads and an answer around what
// the group's code tests.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1d.h"
#include "game/rest_1d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1d {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (band_rows.py --group R1D --clones --harness scenario, 2026-10-04) ---
constexpr sh::CallSite kCalls520E30[] = {{0x0, 0x524DA0}, {0x19, 0x589330}};
constexpr sh::CallSite kCalls520E90[] = {{0x3B, 0x52F570}, {0x5A, 0x52F570}};
constexpr sh::CallSite kCalls520F40[] = {{0x15, 0x51C390}, {0x2F, 0x51C390}, {0x72, 0x572570}, {0x8D, 0x5725C0},
                                         {0xB8, 0x589330}, {0xF6, 0x5725C0}, {0x115, 0x572570}, {0x14D, 0x5725C0},
                                         {0x16C, 0x572570}, {0x193, 0x587740}, {0x1AC, 0x589330}};
constexpr sh::CallSite kCalls521120[] = {{0x51, 0x531CF0}, {0x8F, 0x521200}, {0xA5, 0x521200}, {0xB9, 0x521200}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls521200[] = {{0xC, 0x536700}, {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                         {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                         {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr sh::CallSite kCalls5213A0[] = {{0x3, 0x521510}, {0x19, 0x51DD70}, {0x2A, 0x51C6A0}, {0x59, 0x587740},
                                         {0x6A, 0x589330}, {0x86, 0x5345E0}, {0x93, 0x52E140}, {0xFC, 0x531CF0}};
constexpr sh::CallSite kCalls5217E0[] = {{0x51, 0x531CF0}, {0x8F, 0x5218C0}, {0xA5, 0x5218C0}, {0xB9, 0x5218C0}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls521A20[] = {{0x25, 0x5893A0}};
constexpr sh::CallSite kCalls521C40[] = {{0x29, 0x52E140}, {0x2E, 0x5893A0}};
constexpr sh::CallSite kCalls521EA0[] = {{0x51, 0x531CF0}, {0x8F, 0x521F80}, {0xA5, 0x521F80}, {0xB9, 0x521F80}, {0xD2, 0x589410}};
constexpr sh::CallSite kCalls522140[] = {{0x14, 0x522560}, {0x2E, 0x522560}, {0x57, 0x587740}, {0x70, 0x589330}};
constexpr sh::CallSite kCalls5221E0[] = {{0x2D, 0x530530}, {0x5A, 0x587740}, {0xA9, 0x531CF0}, {0xE2, 0x587740},
                                         {0xF4, 0x522320}, {0x10A, 0x522320}, {0x11E, 0x522320}, {0x137, 0x589410}};
constexpr sh::CallSite kCalls522320[] = {{0xD, 0x536700}, {0x49, 0x522FB0}, {0x53, 0x587740}, {0x5B, 0x5B93D2}, {0x74, 0x522FB0},
                                         {0x7D, 0x591680}, {0xAD, 0x590BB0}, {0xBE, 0x587740}, {0xC4, 0x497710}, {0xE3, 0x497710},
                                         {0x108, 0x522FB0}, {0x10F, 0x534DB0}, {0x123, 0x537480}, {0x12D, 0x497710},
                                         {0x154, 0x522FB0}, {0x15C, 0x5B93D2}, {0x16D, 0x522FB0}, {0x17A, 0x587740}};
constexpr sh::CallSite kCalls5224D0[] = {{0x0, 0x589410}, {0x12, 0x589330}, {0x7A, 0x587740}};

#define R1D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R1D_ROW(name, base, size, calls) #name, base, size, calls, R1D_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define R1D_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite, kCa = sh::Shape::kCall;
// The two cell probes answer al (every caller `test al, al` first: 0x5211B7,
// 0x5211CD, 0x5222DC, 0x5222F2 and their copies); everything else is void.
const sh::Clone kAll[] = {
    {R1D_LEAF(PartyAction9_Form1State1, 0x520E10, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction9_Form1Start, 0x520E30, 0x35, kCalls520E30), 0, false, kSp},
    {R1D_LEAF(PartyFormAction9_Form2, 0x520E70, 0x12), 0, false, kSp},
    {R1D_ROW(PartyFormAction_TurnToSide, 0x520E90, 0x87, kCalls520E90), 0, false, kSp},
    {R1D_LEAF(PartyAction9_Form2, 0x520F20, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction9_Form2Begin, 0x520F40, 0x1D4, kCalls520F40), 0, false, kSp},
    {R1D_ROW(PartyAction9_Form2Resolve, 0x521120, 0xDB, kCalls521120), 0, false, kSp},
    {R1D_ROW(PartyAction9_CellPickup, 0x521200, 0x11F, kCalls521200), 0xFFu, false, kCa},
    {R1D_LEAF(PartyFormAction9_ByForm, 0x521320, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyAction9_ByForm, 0x521340, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyFormAction10_Form0, 0x521360, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction10_Form0, 0x521380, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction10_Form0Begin, 0x5213A0, 0x166, kCalls5213A0), 0, false, kSp},
    {R1D_LEAF(PartyFormAction10_Form1, 0x5215C0, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction10_Form1, 0x5215E0, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction10_Form1Begin, 0x521600, 0x1D4, kCalls520F40), 0, false, kSp},
    {R1D_ROW(PartyAction10_Form1Resolve, 0x5217E0, 0xDB, kCalls5217E0), 0, false, kSp},
    {R1D_ROW(PartyAction10_CellPickup, 0x5218C0, 0x11F, kCalls521200), 0xFFu, false, kCa},
    {R1D_LEAF(PartyFormAction10_Form2, 0x5219E0, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction10_Form2, 0x521A00, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction_WaitEffectDone, 0x521A20, 0x2A, kCalls521A20), 0, false, kSp},
    {R1D_LEAF(PartyFormAction10_ByForm, 0x521A50, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyAction10_ByForm, 0x521A70, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyFormAction11_Form0, 0x521A90, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction11_Form0, 0x521AB0, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction11_Form0Begin, 0x521AD0, 0x166, kCalls5213A0), 0, false, kSp},
    {R1D_ROW(PartyAction_StepCountdown, 0x521C40, 0x33, kCalls521C40), 0, false, kSp},
    {R1D_LEAF(PartyFormAction11_Form1, 0x521C80, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction11_Form1, 0x521CA0, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction11_Form1Begin, 0x521CC0, 0x1D4, kCalls520F40), 0, false, kSp},
    {R1D_ROW(PartyAction11_Form1Resolve, 0x521EA0, 0xDB, kCalls521EA0), 0, false, kSp},
    {R1D_ROW(PartyAction11_CellPickup, 0x521F80, 0x11F, kCalls521200), 0xFFu, false, kCa},
    {R1D_LEAF(PartyFormAction11_ByForm, 0x5220A0, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyAction11_ByForm, 0x5220C0, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyFormAction12_Form0, 0x5220E0, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction12_Form0, 0x522100, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyAction12_Form0State0, 0x522120, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction12_Form0Begin, 0x522140, 0xA0, kCalls522140), 0, false, kSp},
    {R1D_ROW(PartyAction12_Form0Resolve, 0x5221E0, 0x140, kCalls5221E0), 0, false, kSp},
    {R1D_ROW(PartyAction12_CellHit, 0x522320, 0x188, kCalls522320), 0xFFu, false, kCa},
    {R1D_LEAF(PartyAction12_Form0State1, 0x5224B0, 0x12), 0, false, kSp},
    {R1D_ROW(PartyAction12_Form0EffectSet, 0x5224D0, 0x81, kCalls5224D0), 0, false, kSp},
    {R1D_LEAF(PartyFormAction12_Form1, 0x522650, 0x12), 0, false, kSp},
    {R1D_LEAF(PartyFormAction12_ByForm, 0x522670, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyAction12_ByForm, 0x522690, 0x13), 0, false, kSp},
    {R1D_LEAF(PartyFormAction13_Form0, 0x5226B0, 0x12), 0, false, kSp},
};
#undef R1D_ROW
#undef R1D_LEAF
#undef R1D_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 46, "the cut's 46 rows for R1D");

// What each clone is, for its seed: a dispatcher's index byte (2, 3, or 0x2C
// for the u16 form word) and its table's count; 0 for a handler.
struct Dispatch { U base; unsigned by, count; };
const Dispatch kDispatch[] = {
    {0x520E10, 3, 3},    {0x520E70, 2, 3},    {0x520F20, 2, 3},    {0x521320, 0x2C, 3}, {0x521340, 0x2C, 3},
    {0x521360, 2, 3},    {0x521380, 2, 2},    {0x5215C0, 2, 3},    {0x5215E0, 2, 3},    {0x5219E0, 2, 3},
    {0x521A00, 2, 2},    {0x521A50, 0x2C, 3}, {0x521A70, 0x2C, 3}, {0x521A90, 2, 3},    {0x521AB0, 2, 2},
    {0x521C80, 2, 3},    {0x521CA0, 2, 3},    {0x5220A0, 0x2C, 3}, {0x5220C0, 0x2C, 3}, {0x5220E0, 2, 3},
    {0x522100, 2, 2},    {0x522120, 3, 5},    {0x5224B0, 3, 3},    {0x522650, 2, 3},    {0x522670, 0x2C, 3},
    {0x522690, 0x2C, 3}, {0x5226B0, 2, 3},
};
const Dispatch* DispatchOf(U base) {
    for (const Dispatch& d : kDispatch)
        if (d.base == base) return &d;
    return nullptr;
}

// The 27 tables, each to its reader's count (docs/rest_1d.md section 3).
const sh::DataTable kTables[] = {
    {at::kAction9Form1State1Steps, 3},  {at::kFormAction9Form2States, 3},  {at::kAction9Form2States, 3},
    {at::kFormAction9Forms, 3},         {at::kAction9Forms, 3},            {at::kFormAction10Form0States, 3},
    {at::kAction10Form0States, 2},      {at::kFormAction10Form1States, 3}, {at::kAction10Form1States, 3},
    {at::kFormAction10Form2States, 3},  {at::kAction10Form2States, 2},     {at::kFormAction10Forms, 3},
    {at::kAction10Forms, 3},            {at::kFormAction11Form0States, 3}, {at::kAction11Form0States, 2},
    {at::kFormAction11Form1States, 3},  {at::kAction11Form1States, 3},     {at::kFormAction11Forms, 3},
    {at::kAction11Forms, 3},            {at::kFormAction12Form0States, 3}, {at::kAction12Form0States, 2},
    {at::kAction12Form0State0Steps, 5}, {at::kAction12Form0State1Steps, 3}, {at::kFormAction12Form1States, 3},
    {at::kFormAction12Forms, 3},        {at::kAction12Forms, 3},           {at::kFormAction13Form0States, 3},
};

// --- the stand-ins' answers (Noise() and the state only: both passes the same) ----------

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// Sprite_ObjectAt: none (0xFF) half the time, else one of the 34 records 0..0x21
// (what the real one answers; ours aborts on any other).
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 4 ? (n >> 12) % 0x1E : 0x1E + (n >> 12) % 4);
}
// Field_EffectAhead, PartyAction_Kind30Ahead, Effect_FindFree: none half the
// time, else a record 0..19 (what the real ones answer).
U FxRecordOrNone(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 4) % 20);
}
// PartyAction_MemberOnEffect / _MemberBeyondEffect: 0 three times in four, so
// that both answer 0 (the jump) often enough.
U FxMostlyNo(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 4 ? 0 : 1 + (n >> 8) % 0xFF);
}
// AreaMap_ByteAt: each code the two probes compare with and its neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xEF, 0x00, 0xFF};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// MapView_SlopeAt: DamageScratch's flag byte 0 a third of the time, else not 0;
// the slope's low word on either side of 0x40 and at the s16 limits.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = sh::Mem(bof3::addr::DamageScratch);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// MapView_GroundAt: a low word at, around, or 0x40 above the sprite's height
// word +0x3E (read now: the same on both passes), or far from it - so both the
// rise test (> 0x40) and the side probes' (above the height) fall either way.
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0, 1, 0xFFFF, 0x40, 0x41, 0x3F, 0x8000, 0x7FFF, 0x8041};
    return WithAx(answer, n % 5 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}

// Rand (the harness's draw, kRand): also flips Field_InputFlags' bit 1 or 2 a
// third of the time, standing in for a frame's change between the pickups'
// draw and their read of the flags after it (the read's order is then seen;
// the disturbance alone reached it too rarely - docs/rest_1d.md section 6).
U FxRandFlags(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flags = sh::Mem(0x905BA2);
    if (n % 3 == 0 && sh::InRegions(flags, 1)) flags[0] = static_cast<unsigned char>(flags[0] ^ (2u << ((n >> 4) & 1)));
    return answer;
}

// Masks by what each callee reads (symbols.toml's types and the reads cited in
// docs/rest_1d.md section 4): bytes where the callee reads a byte (the
// originals push whole registers whose upper bytes ours cannot hold), the
// cells as 16-bit words, the rest whole.
#define R1D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // R0A's helpers (ours, round fourteen stage A)
    {R1D_OURS(PartyAction_TargetAhead), 0, {}, kF, 0, 0},
    {R1D_OURS(PartyAction_BlockedAhead), 0, {}, kF, 0, 0},
    {R1D_OURS(PartyAction_Kind30Ahead), 0, {}, kG, 0, 0, {}, &FxRecordOrNone},
    {R1D_OURS(PartyAction_MemberOnEffect), 1, {kU8}, kG, 0, 0, {}, &FxMostlyNo},       // `and esi, 0xFF`
    {R1D_OURS(PartyAction_MemberBeyondEffect), 1, {kU8}, kG, 0, 0, {}, &FxMostlyNo},   // `and esi, 0xFF`
    {R1D_OURS(PartyAction_SideProbes), 0, {}, kG, 0, 0},
    {R1D_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, kG, 0, 0},   // the state's byte, x and z movsx words
    // the field engine's
    {R1D_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
    {R1D_OURS(MapView_SlopeAt), 3, {kW, kW, kU8}, kG, 0, 0, {}, &FxSlope},   // the direction byte: AreaMap_Slope reads more only from 10 (rest_1e.md section 5)
    {R1D_OURS(Sprite_EnsureAnimation), 1, {kU8}, kF, 0, 0},                  // compares the low byte; Sprite_SetAnimation's byte
    {R1D_OURS(Sound_PlayEffect), 1, {kU16}, kG, 0, 0},                      // id & 0xFFFF
    {R1D_OURS(Sprite_TurnSense), 1, {kU8}, kG, 0, 0},                       // the target byte
    {R1D_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    {R1D_OURS(Sprite_ScriptTick), 0, {}, kF, 0, 0},
    {R1D_OURS(Sprite_ScriptTickOnce), 0, {}, kF, 0, 0},
    {R1D_OURS(Field_LeaderStepTick), 0, {}, kF, 0, 0},
    {R1D_OURS(Field_JumpStart), 0, {}, kG, 0, 0},
    {R1D_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxRecordOrNone},
    {R1D_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},  // movsx words
    {R1D_OURS(Effect_FindFree), 0, {}, kG, 0, 0, {}, &FxRecordOrNone},
    {R1D_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, kG, 0, 0},       // the state's byte, x and z movsx words
    {R1D_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, kG, 0, 0},             // (short) x and z
    {R1D_OURS(Field_GiveZenny), 1, {kW}, kG, 0, 0},                       // `and edx, 0xFF` before the push
    {R1D_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, kF, 0, 0},              // low bytes; the pushed fourth dword unread
    {R1D_OURS(Msg_OpenSystem), 1, {kU16}, kG, 0, 0},
    {R1D_OURS(Sprite_FlashClut), 1, {kU8}, kG, 0, 0},                     // the colour's low byte
    {R1D_OURS(Char_LoseHp), 2, {kW, kU8}, kG, 0, 0},                      // the amount whole, the member byte
    {"Rand", 0x5B93D2, 0x5B93D2, 0, {}, sh::Answer::kRand, 0, 0, {}, &FxRandFlags},   // Capcom's CRT rand, not ours
    // the group's own, called by E8
    {R1D_OURS(PartyAction9_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1D_OURS(PartyAction10_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1D_OURS(PartyAction11_CellPickup), 2, {kU16, kU16}, kF, 0, 0},
    {R1D_OURS(PartyAction12_CellHit), 2, {kU16, kU16}, kF, 0, 0},
};
#undef R1D_OURS

// Beyond field mode's standard regions: Field_DirectionSteps (read in place by
// the turns, the point ahead and the side probes) and Text_Records' first 16
// bytes (the item name the pickups copy).
const sh::Region kRegions[] = {
    {at::kSteps, 0x40},
    {bof3::addr::Text_Records, 0x10},
};

// --- the seed ----------------------------------------------------------------------------

U Coordinate() {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
U Height() { return PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x7FC0, 0x8040, 0x40, sh::Next() % 0x400u, sh::Next()); }

void SeedSteps() {
    unsigned char* const t = sh::Mem(at::kSteps);
    for (unsigned i = 0; i < 16; ++i) {
        const U v = sh::Half() ? PickOf(0, 0x8000, 0xFFFF8000u)
                               : PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u, 1, 0xFFFFFFFFu, 0x80000000u, sh::Next());
        SetLong(t + 4 * i, static_cast<std::int32_t>(v));
    }
}

// One sprite record's bytes the handlers read: the direction (0..7 mostly;
// above 7 reads the .data after Field_DirectionSteps in place, the same on both
// passes), the counters +9 / +0xA at 0, 1 and 2 (their tests and wraps), +7,
// the effect index +0xB below 20 (ours aborts above it where the original
// writes past Effect_Objects; PartyAction_WaitEffectDone's 0xFF is seeded by
// its own case), the position and the height.
void SeedSprite(unsigned char* s) {
    s[8] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next()));
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, sh::Next()));
    s[7] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    s[0xB] = static_cast<unsigned char>(sh::Next() % 20);
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    SetWord(s + 0x3E, Height());
}

unsigned g_clone;   // the round's clone's index in kAll (set by Seed, read by Disturb)

void Seed(unsigned k) {
    g_clone = k;
    const sh::Clone& c = kAll[k];
    // Sprite_Current one of ObjTrio's records half the time (the leader and the
    // members run these states), else the harness's Sprite_Objects record; every
    // record the disturbance can move it to is seeded.
    if (sh::Half()) Sprite_Current = sh::ObjectOf(sh::Next());
    for (unsigned i = 0; i < 4; ++i) SeedSprite(sh::SpriteRecord(i));
    for (unsigned i = 0; i < 3; ++i) SeedSprite(sh::ObjectOf(i));
    SeedSteps();
    unsigned char* const s = Sprite_Current;
    unsigned char* const fs = Field_State;
    fs[0x138] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, sh::Next()));
    Field_InputFlags = static_cast<unsigned char>(PickOf(0, 2, 4, 6, 1, 0xF9, sh::Next()));
    if (const Dispatch* d = DispatchOf(c.base)) {
        const unsigned index = sh::Next() % d->count;
        if (d->by == 0x2C) SetWord(s + 0x2C, index);
        else s[d->by] = static_cast<unsigned char>(index);
        return;
    }
    switch (c.base) {
    case 0x520E90:   // PartyFormAction_TurnToSide: the chapter byte 0xF a third of the time
        Cond_ByteFA = static_cast<signed char>(PickOf(0xF, 0xF, 0, 0xE, 0x10, 0x8F, sh::Next()));
        s[8] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : sh::Next());
        break;
    case 0x521A20: {   // PartyAction_WaitEffectDone: the record in use or free, or 0xFF (read in place)
        s[0xB] = static_cast<unsigned char>(sh::Next() % 8 == 0 ? 0xFF : sh::Next() % 20);
        if (s[0xB] < 20) Effect_Objects[s[0xB] * 0x80] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        break;
    }
    case 0x521200:
    case 0x5218C0:
    case 0x521F80:   // the pickups: Rand & 0xF around 0xD..0xF
        sh::SetRandHint(PickOf(0xC, 0xD, 0xE, 0xF, 0x1D, 0x3F, 0x40));
        break;
    case 0x522320:   // the cell hit: Rand & 0xF around 7 and 0xB, Rand & 7 around 5
        sh::SetRandHint(PickOf(5, 6, 7, 0xB, 0xC, 0x16, 0x1C));
        fs[0x89] = static_cast<unsigned char>(sh::Next() % 8);
        break;
    default: break;
    }
}

// The pickups' and the cell hit's (x, z): random dwords half the time, else a
// cell at a boundary under random upper halves (every callee reads 16 bits).
void Args(unsigned k, U* a) {
    const U base = kAll[k].base;
    if (base != 0x521200 && base != 0x5218C0 && base != 0x521F80 && base != 0x522320) return;
    for (unsigned i = 0; i < 2; ++i)
        if (sh::Half()) a[i] = (a[i] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, sh::Next() % 0x80u) & 0xFFFF);
}

// What the handlers read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// counters, +7, +0xB (below 20), form word, position and height; Field_State
// +0x89 / +0x138; Field_InputFlags (read after the pickup's first Rand);
// DamageScratch's flag; a dword of Field_DirectionSteps. The harness's own
// moves Sprite_Current among the Sprite_Objects records and its state bytes.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 16) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: s[9 + (v & 1)] = static_cast<unsigned char>((v >> 1) % 3); break;
    case 2: s[0xB] = static_cast<unsigned char>((v >> 1) % 20); break;
    case 3: s[7] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 4: SetWord(s + 0x2C, v >> 4); break;
    case 5: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 6: SetWord(s + 0x3E, v >> 2); break;
    case 14:
    case 15:
    case 7: {
        unsigned char* const fs = Field_State;
        if (sh::InRegions(fs + 0x89, 1) && sh::InRegions(fs + 0x138, 1)) fs[v & 1 ? 0x89 : 0x138] = static_cast<unsigned char>(v >> 1);
        break;
    }
    case 12:
    case 13:
    case 8: Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ (2u << (v % 2))); break;
    case 9: sh::Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 10: SetLong(sh::Mem(at::kSteps) + 4 * (v % 16), static_cast<std::int32_t>(v << 6)); break;
    case 11: Effect_Objects[((v >> 1) % 20) * 0x80] = static_cast<unsigned char>(v & 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_1d: BOF3X_R1D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_1d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_1d
