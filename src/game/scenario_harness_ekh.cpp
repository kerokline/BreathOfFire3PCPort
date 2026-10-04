// BOF3X_SHADOW=scenario_harness_ekh: the scenario harness's effect mode proved
// on Capcom's code (round thirteen group EKH, docs/scenario_harness.md section
// 8.8). Each round-thirteen shape is driven with a row of the cut
// analysis/round13_cut.tsv on both sides, so any difference is the harness's
// own: an effect record not put back, Sprite_Current drawn differently per
// pass, a span drawn outside its table, a louder stand-in that draws from the
// wrong stream, a region missing.
//
// "Ours" is the original in place for the six that call nothing (or reach out
// only through a .data table, which the harness swaps for recorders on both
// sides alike). For the two that call a callee - the spawner calls
// Effect_FindFree, the release state tail-jumps to Effect_Release - the
// original in place would reach the real callee where the copy reaches a
// recorder, so "ours" is a second byte-copy of the original whose call is
// re-aimed at a trampoline asking the harness for the callee's stand-in, as
// ours does through SH_CALL: Capcom's code on both sides still.
//
// BOF3X_EKH_CONTROL=1 or 2 plants a control (a mutant of the copy standing in
// as ours): 1 the leaf state writing +1 = 2 instead of 1, 2 the spawner writing
// the new record's kind 0x25 instead of 0x24. Each must end in a Fatal.
//
// Extents read to the last instruction with capstone (2026-09-29, EKH).
//
// Every row here is ours now (round thirteen's groups took all eight, and
// sprite_pose.cpp Effect_FindFree / Effect_Release), so each is named by its
// address constant bof3::addr::Name - the ORIGINAL's address, the same value
// the literal had: the copies are Capcom's bytes, and "ours in place" is the
// original only because this self-test runs before every effect group's inject
// (inject_all.cpp). Never &::Name here - that is our function, not Capcom's
// (round thirteen's end fold, docs/scenario_harness.md section 8.10).
#include "game/scenario_harness_ekh.h"

#include <cstdint>
#include <cstdlib>

#include "bof3/symbols.gen.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace sh = scenario_harness;
namespace at = scenario_harness::at;
using sh::Arg;
using sh::ArgAt;
using sh::Shape;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
const void* InPlace(std::uint32_t address) { return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)); }

// The two callees the copies standing in as ours reach: the harness's stand-in
// for each, asked for at the call (the harness registers it inside Run).
extern "C" std::uint32_t __cdecl EkhFindFree() {
    return reinterpret_cast<std::uint32_t (__cdecl*)()>(const_cast<void*>(sh::StandIn(Key(reinterpret_cast<const void*>(&::Effect_FindFree)))))();
}
extern "C" std::uint32_t __cdecl EkhRelease() {
    return reinterpret_cast<std::uint32_t (__cdecl*)()>(const_cast<void*>(sh::StandIn(Key(reinterpret_cast<const void*>(&::Effect_Release)))))();
}

// The copies' call sites (the E8 / E9 offsets): the spawner's call of
// Effect_FindFree at +0xC, the release state's tail jmp at +0xE.
constexpr sh::CallSite kCalls46F7D0[] = {{0xC, bof3::addr::Effect_FindFree}};
constexpr sh::CallSite kCalls472770[] = {{0xE, bof3::addr::Effect_Release}};

enum : unsigned { kLeaf, kDispatch1, kDispatch2, kSubState, kAngle, kRecordArg, kSpawner, kRelease, kCount };

sh::Clone g_clones[] = {
    // kEffect, a hidden state handler reached by a .data cell: E1A's 0x462BC0
    // EffectKind01_Start (in 0x462AC0; the cell 0x653A44, entry 0 of
    // EffectKind01_States, whose dispatcher 0x462BA0 EffectKind01_Run is
    // Effect_KindHandlers[1]): +0x54 = the dword +0x54
    // of Sprite_ObjectsExtra record (+0x18), then +1 = 1
    {"E1A EffectKind01_Start", bof3::addr::EffectKind01_Start, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectKind01_Start), 0, false, Shape::kEffect, 0, 2, 0, 1},
    // kEffect, a kind's dispatcher taken whole with its table: E1D's 0x46F2B0
    // EffectKind21_Run, Effect_KindHandlers[0x21]: jmp [0x654284
    // (EffectKind21_States) + +1 * 4], unbounded; the table has six entries
    // before 0x65429C, the next dispatcher's (state_span 6)
    {"E1D EffectKind21_Run", bof3::addr::EffectKind21_Run, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectKind21_Run), 0, false, Shape::kEffect, 0, 6, 0, 0x21},
    // kEffect, a kind-0x18 sub-state dispatcher: E5A's 0x4FD470
    // EffectKind18_04_Run, EffectKind18_States[4]: jmp [0x65DAE8
    // (EffectKind18_04_States) + +2 * 4]; four code pointers,
    // then bytes (sub_span 4)
    {"E5A EffectKind18_04_Run", bof3::addr::EffectKind18_04_Run, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectKind18_04_Run), 0, false, Shape::kEffect, 0, 0, 4, 0x18},
    // kEffect, a kind-0x18 sub-state: E5B's 0x500D20 EffectKind18Sub0F_Open,
    // entry 3 of 0x65E068 EffectKind18Sub0F_States (the table of 0x500930
    // EffectKind18Sub0F_Run = EffectKind18_States[15]; six entries before
    // 0x65E080, 0x5011A0's): +2 = 4 when the leader stands within two cells of
    // the record, unless Cond_ByteFD is 0 and Cond_ByteFE equals +0xA
    {"E5B EffectKind18Sub0F_Open", bof3::addr::EffectKind18Sub0F_Open, 0xD1, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectKind18Sub0F_Open), 0, false, Shape::kEffect, 0, 16, 6, 0x18},
    // kCall answering in eax: E2E's 0x479970 EffectAngle_Mean (a, b): the mean of two 12-bit
    // angles, the short way round
    {"E2E EffectAngle_Mean", bof3::addr::EffectAngle_Mean, 0x44, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectAngle_Mean), 0xFFFFFFFFu, false, Shape::kCall},
    // kCall handed a record: E3C's 0x4857C0 EffectKind73_SparkInit (record)
    // fills +0..+0x16 of the record it is handed from Sprite_Current's point.
    // Its callers hand it a shard record (0x485810 EffectKind73_FindSpark's
    // answer, 0x92BF80..); here Arg::kEffect hands it
    // an effect record, which proves the mechanics, not its callers' use
    {"E3C EffectKind73_SparkInit", bof3::addr::EffectKind73_SparkInit, 0x41, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(bof3::addr::EffectKind73_SparkInit), 0, false, Shape::kCall, ArgAt(0, Arg::kEffect)},
    // kEffect, a spawner: E1D's 0x46F7D0 EffectKind23_SpawnRays, entry 1 of
    // kind 0x23's table 0x6542A8 EffectKind23_States:
    // with +9 bit 2 set, Effect_FindFree; a record found gets +0 = 1, +5 =
    // 0x24; then +9 down, +1 up at 0. Ours: a second copy, its call re-aimed
    // at EkhFindFree (set in Run below)
    {"E1D EffectKind23_SpawnRays", bof3::addr::EffectKind23_SpawnRays, 0x4F, kCalls46F7D0, 1, nullptr, 0, nullptr, 0, nullptr, 0, false, Shape::kEffect, 0, 3, 0, 0x23},
    // kEffect, a release: E2A's 0x472770 EffectKind2D_End (entry 10 of the run 0x6543AC):
    // MsgBoxState +4 bit 1, the counter 0x903848 = 0x32, then a tail jmp to
    // Effect_Release. Ours: a second copy, its jmp re-aimed at EkhRelease
    {"E2A EffectKind2D_End", bof3::addr::EffectKind2D_End, 0x13, kCalls472770, 1, nullptr, 0, nullptr, 0, nullptr, 0, false, Shape::kEffect},
};
static_assert(sizeof g_clones / sizeof g_clones[0] == kCount, "one enum entry a clone, in order");

// The tables the two dispatchers jump through, swapped for recorders on both sides.
// (EffectKind21_States, EffectKind18_04_States: data, whose names are macros
// casting the address - the literals stay.)
const sh::DataTable kTables[] = {{0x654284, 6}, {0x65DAE8, 4}};
const std::uint8_t kKinds[] = {0x01, 0x18, 0x21, 0x23, 0x2E};

void Seed(unsigned k) {
    auto* const s = static_cast<unsigned char*>(Sprite_Current);
    switch (k) {
    case kLeaf:
        // +0x18 a Sprite_ObjectsExtra record (the original reads 0x802054 + 0xA4 * it)
        s[0x18] = static_cast<unsigned char>(sh::Next() % 4);
        s[0x19] = s[0x1A] = s[0x1B] = 0;
        break;
    case kSubState: {
        // the leader within, at and past two cells of the record half the time;
        // Cond_ByteFD 0 and Cond_ByteFE equal to +0xA a third of the time each
        unsigned char* const leader = ObjTrio;
        s[1] = 15;   // EffectKind18_States[15], 0x500930, whose +2 table holds this state
        if (sh::Half()) {
            const std::int32_t dx = static_cast<std::int32_t>(sh::Next() % 5) - 2, dz = static_cast<std::int32_t>(sh::Next() % 5) - 2;
            const std::uint32_t x = (static_cast<std::uint32_t>(static_cast<std::int16_t>(s[0x36] | s[0x37] << 8)) + dx) << 16 | (sh::Next() & 0xFFFF);
            const std::uint32_t z = (static_cast<std::uint32_t>(static_cast<std::int16_t>(s[0x3A] | s[0x3B] << 8)) + dz) << 16 | (sh::Next() & 0xFFFF);
            for (unsigned i = 0; i < 4; ++i) leader[0x34 + i] = static_cast<unsigned char>(x >> (8 * i));
            for (unsigned i = 0; i < 4; ++i) leader[0x38 + i] = static_cast<unsigned char>(z >> (8 * i));
        }
        if (sh::Next() % 3 == 0) sh::Mem(at::kByteFD)[0] = 0;
        if (sh::Next() % 3 == 0) sh::Mem(at::kCameraCells)[0] = s[0xA];
        if (sh::Half()) s[8] = 0;
        break;
    }
    case kSpawner:
        // +9 bit 2 (a spawn) half the time; +9 1 (the count runs out: +1 up) a
        // third of the time - the two cannot meet in one call
        if (sh::Half()) s[9] |= 4;
        if (sh::Next() % 3 == 0) s[9] = 1;
        break;
    default: break;
    }
}

void Args(unsigned k, std::uint32_t* a) {
    if (k != kAngle) return;
    // two angles, 12 bits each; the second near the first half the time, and
    // across the 0x800 either side
    a[0] = sh::Next() & 0xFFF;
    a[1] = sh::Half() ? (a[0] + 0x800 + (sh::Next() % 5) - 2) & 0xFFF : sh::Next();
}

// The two copies standing in as ours, and the controls (a byte of one patched).
void MakeCopies() {
    const bof3::CloneCall spawn[] = {{0xC, reinterpret_cast<const void*>(&EkhFindFree), bof3::addr::Effect_FindFree}};
    const bof3::CloneCall release[] = {{0xE, reinterpret_cast<const void*>(&EkhRelease), bof3::addr::Effect_Release}};
    auto* const spawner = static_cast<unsigned char*>(bof3::CloneOriginal("ekh ours EffectKind23_SpawnRays", bof3::addr::EffectKind23_SpawnRays, 0x4F, spawn, 1));
    auto* const releaser = static_cast<unsigned char*>(bof3::CloneOriginal("ekh ours EffectKind2D_End", bof3::addr::EffectKind2D_End, 0x13, release, 1));
    g_clones[kSpawner].ours = spawner;
    g_clones[kRelease].ours = releaser;
    const char* const control = std::getenv("BOF3X_EKH_CONTROL");
    if (!control) return;
    if (control[0] == '1') {
        // the leaf state's `mov byte [eax + 1], 1` at +0x1E: its immediate +0x21 to 2
        auto* const leaf = static_cast<unsigned char*>(bof3::CloneOriginal("ekh control EffectKind01_Start", bof3::addr::EffectKind01_Start, 0x23));
        if (leaf[0x21] != 1) bof3::Fatal("scenario_harness_ekh: 0x462BC0 +0x21 holds 0x%X, not 1", leaf[0x21]);
        leaf[0x21] = 2;
        g_clones[kLeaf].ours = leaf;
        bof3::Log("shadow      scenario_harness_ekh CONTROL 1: 0x462BC0's copy writes +1 = 2 (must be refused)");
    } else if (control[0] == '2') {
        // the spawner's `mov byte [eax + 5], 0x24` at +0x2D: its immediate +0x30 to 0x25
        if (spawner[0x30] != 0x24) bof3::Fatal("scenario_harness_ekh: 0x46F7D0 +0x30 holds 0x%X, not 0x24", spawner[0x30]);
        spawner[0x30] = 0x25;
        bof3::Log("shadow      scenario_harness_ekh CONTROL 2: 0x46F7D0's copy spawns kind 0x25 (must be refused)");
    }
}

}  // namespace

void ScenarioHarnessEkh_Inject() {
    if (!bof3::WantsShadow("scenario_harness_ekh")) return;
    MakeCopies();
    sh::Group g = {"scenario_harness_ekh", g_clones, kCount, nullptr, 0, kTables, sizeof kTables / sizeof kTables[0],
                   nullptr, 0, Seed, nullptr, 0};
    g.args = Args;
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}
