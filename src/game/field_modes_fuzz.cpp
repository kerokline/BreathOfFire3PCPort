// BOF3X_SHADOW=field_modes: a differential fuzz of the seventeen functions of
// field_modes.cpp against byte-copies of Capcom's, once at start-up
// (docs/field-modes.md section 5). Every call out of a copy is re-aimed at a
// recording stand-in (capstone lists below); every table either side reads -
// the chapter vtables, the call tables, the two tail tables and scenario
// 16's state / run table - is swapped for recording entries for the
// duration; the three scenes' own jump tables are relocated into the copies.
// One round: one function, random bytes in every region any of them
// touches, then that function's boundaries seeded; theirs, then from the
// same state ours; the regions, the recorders' log and the stack arguments
// the table entries saw compared.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_modes_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_modes {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n, g_seed, g_load_calls, g_load_limit;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
// The typed symbols are macros for lvalues: their addresses, taken once.
const std::uint32_t kAngles = Addr(Camera_Angles), kEffectRecords = Addr(Effect_Objects),
                    kStripSource = Addr(Gfx_ClutStripSource), kStrip = Addr(Gfx_ClutStrip), kFocusZ = Addr(&MapView_FocusZ);

// What a callee may change that its caller reads again after it: the step,
// the timer, the wait word, the request, the counters, the slot word, the
// pass flags, the run, the camera, the music track, Cond_ByteFD and the flag
// pointer (which only the recorders below ever see - nothing dereferences
// it).
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 5 == 0) At(kStep)[0] = static_cast<unsigned char>(h % 3 ? (h >> 8) % 14 : h >> 8);
    if (h % 7 == 0) SetWord(At(kTimer), h % 2 ? (h >> 9) % 4 : h >> 12);
    if (h % 6 == 0) MoveScript_WaitWordDA = static_cast<unsigned short>(h % 4 ? 0 : h >> 16);
    if (h % 9 == 0) Field_Request = static_cast<unsigned char>(h % 2 ? 2 : h >> 20);
    if (h % 11 == 0) At(kCounters + (h >> 5) % 4)[0] = static_cast<unsigned char>(h >> 13);
    if (h % 8 == 0) SetWord(At(kSlot), h % 3 ? (h >> 7) % 0xC4 : h >> 15);
    if (h % 10 == 0) Draw_PassFlags = static_cast<unsigned char>(h >> 18);
    if (h % 13 == 0) MoveScript_Var7 = static_cast<signed char>(h >> 11);
    if (h % 4 == 0) SetWord(At(kAngles + 2 * ((h >> 3) % 3)), h >> 17);
    if (h % 12 == 0) At(kMusicTrack)[0] = static_cast<unsigned char>(h % 3 ? 0xFF : h >> 9);
    if (h % 14 == 0) Cond_ByteFD = static_cast<unsigned char>(h % 2 ? 2 : h >> 22);
    if (h % 15 == 0) SetLong(At(kFlagsPtr), static_cast<std::int32_t>(h));
    if (h % 16 == 0) Game_AreaNumber = static_cast<unsigned short>(h >> 10);
}

void __cdecl StubCallA(unsigned n) { Record(1, n); Disturb(); }
void __cdecl StubChangeArea(int a, int b, int c, int d) {
    Record(2, static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(c), static_cast<std::uint32_t>(d));
    Disturb();
}
void __cdecl StubStartMusic(int n) { Record(3, static_cast<std::uint32_t>(n)); Disturb(); }
// Not done for the round's first g_load_limit asks, then done.
int __cdecl StubLoadDone() {
    Record(4);
    Disturb();
    return ++g_load_calls > g_load_limit ? 1 : 0;
}
void __cdecl StubSleep(int frames) { Record(5, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubTaskEnd() { Record(6); }
void __cdecl StubObjTrio() { Record(7); Disturb(); }
// The callee takes the index as (index & 0xFF) >> 3 and & 7: that much is
// logged; the answer comes from the hash, so both sides see the same one.
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(8, Addr(bits), index & 0xFF);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 2 ? 0 : 1 + (h >> 8) % 0xFF);
}
void __cdecl StubFlagsSet(unsigned char* bits, unsigned index) { Record(9, Addr(bits), index & 0xFF); Disturb(); }
void __cdecl StubScriptFlags() { Record(10); Disturb(); }
void __cdecl StubSetElevation(int value) { Record(11, static_cast<std::uint32_t>(value)); Disturb(); }
void __cdecl StubKind2Place(unsigned char arg) { Record(12, arg); Disturb(); }
// A slot of the first eight records, or none: what the caller indexes by.
unsigned char __cdecl StubEffectAlloc() {
    Record(13);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 5 == 0 ? 0xFF : (h >> 8) % 8);
}
void __cdecl StubPlaceParty(int n) { Record(14, static_cast<std::uint32_t>(n)); Disturb(); }
void __cdecl StubSound(unsigned short id) { Record(15, id); Disturb(); }
void __cdecl StubMusicStop(int frames) { Record(16, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubTransition(unsigned char kind) { Record(17, kind); Disturb(); }
void __cdecl StubMusicPlay(unsigned track, int unknown) { Record(18, track, static_cast<std::uint32_t>(unknown)); Disturb(); }
const unsigned char* __cdecl StubText(int x, int y, int color, int count, const unsigned char* text) {
    Record(19, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(color),
           static_cast<std::uint32_t>(count), Addr(text));
    Disturb();
    return text;
}
void __cdecl StubOpenScript(unsigned short id) { Record(20, id); Disturb(); }
void __cdecl StubViewShift() { Record(21); }
// The callee reads its level as a byte; the original pushes a register
// whose upper bytes are whatever it held.
void __cdecl StubClutFade(unsigned level) { Record(22, level & 0xFF); Disturb(); }
void __cdecl StubClutRestore() { Record(23); Disturb(); }
void __cdecl StubArea1F() { Record(24); Disturb(); }
void __cdecl StubArea04() { Record(25); Disturb(); }
void __cdecl StubArea02() { Record(26); Disturb(); }
void __cdecl StubModeTail() { Record(27); Disturb(); }

// Table entries: which slot ran. Each also logs the two argument words above
// its return address - Scenario_CallA's entries take the caller's arguments
// where they lie, so a thunk that moved them would show; for the argumentless
// dispatchers the words are the caller's frame and are not compared (the
// dispatchers call rather than jump, docs/field-modes.md section 5).
bool g_log_args;
template <unsigned N>
__attribute__((noinline)) void __cdecl Slot(std::uint32_t a, std::uint32_t b) {
    Record(40 + N, g_log_args ? a : 0, g_log_args ? b : 0);
    Disturb();
}
using SlotFn = void (__cdecl*)(std::uint32_t, std::uint32_t);
constexpr SlotFn kSlots[16] = {Slot<0>, Slot<1>, Slot<2>, Slot<3>, Slot<4>, Slot<5>, Slot<6>, Slot<7>,
                               Slot<8>, Slot<9>, Slot<10>, Slot<11>, Slot<12>, Slot<13>, Slot<14>, Slot<15>};

const Callees kStubs = {
    StubCallA, StubChangeArea, StubStartMusic, StubLoadDone, StubSleep, StubTaskEnd, StubObjTrio, StubFlagsTest,
    StubFlagsSet, StubScriptFlags, StubSetElevation, StubKind2Place, StubEffectAlloc, StubPlaceParty, StubSound,
    StubMusicStop, StubTransition, StubMusicPlay, StubText, StubOpenScript, StubViewShift, StubClutFade,
    StubClutRestore, StubArea1F, StubArea04, StubArea02, StubModeTail,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5341A0: return f(&StubCallA);
    case kChangeArea: return f(&StubChangeArea);
    case kStartMusic: return f(&StubStartMusic);
    case 0x454810: return f(&StubLoadDone);
    case 0x5A9949: return f(&StubSleep);
    case kTaskEnd: return f(&StubTaskEnd);
    case 0x57C810: return f(&StubObjTrio);
    case 0x57C140: return f(&StubFlagsTest);
    case 0x57C0F0: return f(&StubFlagsSet);
    case 0x57C7C0: return f(&StubScriptFlags);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x5734F0: return f(&StubKind2Place);
    case kEffectAlloc: return f(&StubEffectAlloc);
    case kPlaceParty: return f(&StubPlaceParty);
    case 0x587740: return f(&StubSound);
    case 0x587B40: return f(&StubMusicStop);
    case 0x495040: return f(&StubTransition);
    case 0x587AE0: return f(&StubMusicPlay);
    case 0x516B30: return f(&StubText);
    case kOpenScript: return f(&StubOpenScript);
    case kViewShift: return f(&StubViewShift);
    case 0x56C0A0: return f(&StubClutFade);
    case 0x56C110: return f(&StubClutRestore);
    case 0x56B3A0: return f(&StubArea1F);
    case 0x56B400: return f(&StubArea04);
    case 0x56B450: return f(&StubArea02);
    case 0x56D8B0: return f(&StubModeTail);
    default: bof3::Fatal("field_modes: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The seventeen copies and their calls out, by capstone 2026-09-22 (the
// instruction's offset; every other jump stays inside). The three scenes'
// jump tables are relocated into the copies; the dispatchers' tables are
// absolute and swapped below.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kModeDispatchCalls[] = {{0x10, 0x56D8B0}};
constexpr Call kStartCalls[] = {{0x2, 0x5341A0}, {0x1C, 0x594E00}, {0x50, 0x587A20}, {0x58, 0x454810}, {0x63, 0x5A9949}, {0x6B, 0x454810}};
constexpr Call kEnterAreaCalls[] = {{0x1B, 0x56B3A0}, {0x2F, 0x56B400}, {0x43, 0x56B450}};
constexpr Call kArea1FCalls[] = {{0x23, 0x57C810}, {0x30, 0x57C140}, {0x4A, 0x57C0F0}};
constexpr Call kArea04Calls[] = {{0x8, 0x57C140}, {0x22, 0x57C0F0}, {0x2A, 0x57C7C0}, {0x46, 0x57C810}};
constexpr Call kArea02Calls[] = {{0x8, 0x57C140}, {0x27, 0x5725F0}, {0x2E, 0x5734F0}, {0x3F, 0x589810}, {0x9C, 0x57C810},
                                 {0xBE, 0x57C0F0}, {0xCF, 0x57C140}, {0xE4, 0x57C0F0}, {0xEB, 0x531F90}};
constexpr Call kEndCalls[] = {{0x15, 0x56C110}, {0x1F, 0x587740}, {0x29, 0x587740}, {0x3C, 0x587B40}, {0x4B, 0x5A99AD}};
constexpr Call kScene2Calls[] = {{0x27, 0x495040}, {0x30, 0x587AE0}, {0x6A, 0x516B30}, {0xBE, 0x516B30}, {0xD6, 0x495040},
                                 {0x11F, 0x516B30}, {0x12F, 0x57C7C0}, {0x134, 0x57C810}, {0x13B, 0x495040}, {0x183, 0x495040},
                                 {0x1B1, 0x516B30}, {0x1F8, 0x516B30}, {0x20F, 0x495040}, {0x249, 0x516B30}, {0x260, 0x594E00},
                                 {0x2A3, 0x589810}, {0x31E, 0x594E00}, {0x32C, 0x587B40}};
constexpr Call kScene3Calls[] = {{0x27, 0x495040}, {0x5B, 0x516B30}, {0x94, 0x516B30}, {0xAB, 0x495040}, {0xEC, 0x516B30},
                                 {0xFB, 0x495040}, {0x111, 0x589810}, {0x1BC, 0x516B30}, {0x1D8, 0x56C110}, {0x1FD, 0x56C0A0},
                                 {0x225, 0x516B30}, {0x278, 0x516B30}, {0x283, 0x56C0A0}, {0x28D, 0x56C110}, {0x2BE, 0x587AE0},
                                 {0x2D1, 0x594E00}, {0x334, 0x56FCA0}};
constexpr Call kScene4Calls[] = {{0x27, 0x4976D0}, {0x66, 0x4976D0}, {0xD5, 0x516B30}, {0xEC, 0x56C0A0}, {0x108, 0x56C0A0},
                                 {0x114, 0x56C110}, {0x15A, 0x589810}, {0x1C2, 0x589810}, {0x26C, 0x495040}, {0x2A1, 0x495040},
                                 {0x2A8, 0x587B40}, {0x2DB, 0x516B30}, {0x325, 0x516B30}, {0x338, 0x495040}, {0x37B, 0x516B30}};
enum : unsigned {
    kModeDispatch, kModeTail, kModeTailRun, kCallA, kFrame, kStart, kEnterArea, kArea1F, kArea04, kArea02, kRun, kEnd,
    kScene2, kScene3, kScene4, kClutFade, kClutRestore, kCount
};
const Clone kClones[kCount] = {
    {"Field_ModeDispatch", 0x56D690, 0x15, kModeDispatchCalls, 1},
    {"Field_ModeTail", 0x56D8B0, 0xE, nullptr, 0},
    {"Field_ModeTailRun", 0x56D920, 0xE, nullptr, 0},
    {"Scenario_CallA", 0x5341A0, 0x1B, nullptr, 0},
    {"Scena16_Frame", 0x56B2A0, 0xE, nullptr, 0},
    {"Scena16_Start", 0x56B2B0, 0x86, kStartCalls, 6},
    {"Scena16_EnterArea", 0x56B340, 0x57, kEnterAreaCalls, 3},
    {"Scena16_Area1F", 0x56B3A0, 0x5A, kArea1FCalls, 3},
    {"Scena16_Area04", 0x56B400, 0x4C, kArea04Calls, 4},
    {"Scena16_Area02", 0x56B450, 0x108, kArea02Calls, 9},
    {"Scena16_Run", 0x56B560, 0xE, nullptr, 0},
    {"Scena16_End", 0x56B570, 0x51, kEndCalls, 5},
    // Each scene: the code, two bytes of padding and its jump table.
    {"Scena16_Scene2", 0x56B5D0, 0x37C, kScene2Calls, 18},
    {"Scena16_Scene3", 0x56B950, 0x364, kScene3Calls, 17},
    {"Scena16_Scene4", 0x56BCC0, 0x3B8, kScene4Calls, 15},
    {"ClutStrip_FadeTo", 0x56C0A0, 0x68, nullptr, 0},
    {"ClutStrip_Restore", 0x56C110, 0x20, nullptr, 0},
};
constexpr move_script::Table kScene2Table = {0x13, 0x34C, 12};
constexpr move_script::Table kScene3Table = {0x13, 0x33C, 10};
constexpr move_script::Table kScene4Table = {0x13, 0x384, 13};

// The tables swapped for the fuzz: the words saved, then recording slots or
// pointers to recording tables written over them.
constexpr unsigned kChapters = 20, kTailSlots = 2, kTailRunSlots = 8, kStateSlots = 9;
std::uint32_t g_vtables[kChapters][5];     // each chapter's vtable: slot 0 recorded as Slot<chapter % 16>
std::uint32_t g_call_tables[kChapters][256];
struct Swap { std::uint32_t at, words; std::uint32_t saved[kChapters]; };
Swap g_swaps[] = {{kScenarioTables, kChapters, {}}, {kCallATables, kChapters, {}}, {kTailTable, kTailSlots, {}},
                  {kTailRunTable, kTailRunSlots, {}}, {kStateTable, kStateSlots, {}}};
void SwapTables() {
    for (Swap& s : g_swaps) std::memcpy(s.saved, At(s.at), 4 * s.words);
    for (unsigned c = 0; c < kChapters; ++c) {
        g_vtables[c][0] = Addr(reinterpret_cast<const void*>(kSlots[c % 16]));
        for (unsigned j = 1; j < 5; ++j) g_vtables[c][j] = 0;
        // Scattered, so that an index off by any power of two lands elsewhere.
        for (unsigned j = 0; j < 256; ++j)
            g_call_tables[c][j] = Addr(reinterpret_cast<const void*>(kSlots[((c * 31 + j) * 2654435761u) >> 28]));
        SetLong(At(kScenarioTables + 4 * c), static_cast<std::int32_t>(Addr(g_vtables[c])));
        SetLong(At(kCallATables + 4 * c), static_cast<std::int32_t>(Addr(g_call_tables[c])));
    }
    for (unsigned j = 0; j < kTailSlots; ++j) SetLong(At(kTailTable + 4 * j), static_cast<std::int32_t>(Addr(reinterpret_cast<const void*>(kSlots[j]))));
    for (unsigned j = 0; j < kTailRunSlots; ++j)
        SetLong(At(kTailRunTable + 4 * j), static_cast<std::int32_t>(Addr(reinterpret_cast<const void*>(kSlots[2 + j]))));
    for (unsigned j = 0; j < kStateSlots; ++j)
        SetLong(At(kStateTable + 4 * j), static_cast<std::int32_t>(Addr(reinterpret_cast<const void*>(kSlots[5 + j]))));
}
void RestoreTables() {
    for (const Swap& s : g_swaps) std::memcpy(At(s.at), s.saved, 4 * s.words);
}

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x8034E0, 0x18},                         // the chapter, state, run, step, timer, Cond_ByteFD
    {0x7E0918, 1},                            // Draw_PassFlags
    {0x9039A2, 2},                            // Field_ScriptFlags
    {0x9039F0, 8},                            // the tail dispatchers' bytes
    {kPending, 0xC},
    {0x903840, 0x14},                         // Camera_Distance, the counters, the slot word
    {0x66C7D8, 1},                            // Field_Request
    {kBadge, 1},
    {0x66C810, 2},                            // MoveScript_WaitWordDA
    {kMusicTrack, 1},
    {kAngles, 0xC},                           // the angles and the flag pointer
    {kEffectRecords, 0x400},                  // the first eight effect records
    {0x802D74, 8},
    {0x905E60, 8},                            // Field_Kind2Z, Field_Kind2X
    {0x937F90, 1},                            // Gfx_ClutStripDirty
    {0x937F98, 1},
    {kStripSource, 0x20},
    {kStrip, 0x20},
    {kFocusZ, 4},
    {0x802038, 4},
    {0x7E068A, 2},
    {0x7E0978, 4},
    {0x904EFC, 2},                            // Game_AreaNumber
    {0x803584, 6},                            // script pool offsets 2..4
    {0x8035A0, 4},                            // and 0x10, 0x11
};
constexpr unsigned kRegionBytes = 0x18 + 1 + 2 + 8 + 0xC + 0x14 + 1 + 1 + 2 + 1 + 0xC + 0x400 + 8 + 8 + 1 + 1 + 0x20 + 0x20 + 4 +
                                  4 + 2 + 4 + 2 + 6 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_load_calls = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }

// Each branch's boundaries, on top of the random bytes. The dispatchers'
// indices are kept inside the swapped tables: outside them the original
// jumps through whatever words follow.
void Seed(unsigned k) {
    if (Often()) MoveScript_WaitWordDA = 0;
    Cond_ByteFA = static_cast<signed char>(Next() % kChapters);
    switch (k) {
    case kModeTail: At(kTailPhase)[0] = static_cast<unsigned char>(Next() % kTailSlots); break;
    case kModeTailRun: At(kTailKind)[0] = static_cast<unsigned char>(Next() % kTailRunSlots); break;
    case kFrame: At(kState)[0] = static_cast<unsigned char>(Next() % kStateSlots); break;
    case kRun: MoveScript_Var7 = static_cast<signed char>(Next() % (kStateSlots - 3)); break;
    case kEnterArea: {
        static const std::uint16_t kAreas[] = {2, 4, 0x1F, 0, 1, 3, 5, 0x1E, 0x20, 0x21, 0x102, 0xFF1F};
        if (Often()) Game_AreaNumber = kAreas[Next() % 12];
        break;
    }
    case kArea04: if (Often()) Cond_ByteFD = 2; break;
    case kEnd: if (Often()) At(kMusicTrack)[0] = 0xFF; break;
    case kScene2:
    case kScene3:
    case kScene4: {
        const unsigned steps = k == kScene2 ? 12 : k == kScene3 ? 10 : 13;
        At(kStep)[0] = static_cast<unsigned char>(Next() % 8 ? Next() % (steps + 1) : Next());
        static const std::uint16_t kTimers[] = {0, 1, 2, 0x1E, 0x1F, 0x20, 0x21, 0x7E, 0x7F, 0x80, 0x90, 0x91, 0x92, 0x70,
                                                0x71, 0x72, 0x10F, 0x110, 0x111, 0x12F, 0x130, 0x131, 0x200, 0xFFFF};
        if (Often()) SetWord(At(kTimer), kTimers[Next() % 24]);
        // Scene 3's fade ends at 0x20 (step 5) and 0 (step 7); scene 4's
        // step 4 turns on 0x130 - timer at 0x20, 0x9F and 0xBF.
        static const std::uint16_t kScene3Timers[] = {0, 1, 2, 0x1E, 0x1F, 0x20};
        static const std::uint16_t kScene4Timers[] = {0x130 - 0xBF + 1, 0x130 - 0xBF, 0x130 - 0xBF - 1, 0x130 - 0x9F + 1, 0x130 - 0x9F,
                                                0x130 - 0x9F - 1, 0x130 - 0x20 + 1, 0x130 - 0x20, 0x130 - 0x20 - 1, 0x130, 0, 1};
        if (k == kScene3 && Next() % 2) SetWord(At(kTimer), kScene3Timers[Next() % 6]);
        if (k == kScene4 && Next() % 2) SetWord(At(kTimer), kScene4Timers[Next() % 12]);
        static const unsigned char kCounts[] = {3, 4, 5, 8, 0xB};
        if (Often()) At(kCounters)[0] = kCounts[Next() % 5];
        if (Often()) At(kCounters + 2)[0] = static_cast<unsigned char>(Next() % 2 ? 0x23 : 0x22);
        if (Often()) At(kCounters + 3)[0] = static_cast<unsigned char>(Next() % 2 ? 0x54 : 0x60);
        if (Often()) Cond_ByteFD = 2;
        if (Next() % 2) Field_Request = 2;
        if (Often()) MapView_FocusZ = 0x4400;
        break;
    }
    case kClutFade: break;
    default: break;
    }
}

// What the rounds reached, from the original's side: each scene's steps by
// the step it started at, the areas entered, the flag branches.
struct Coverage {
    unsigned scene[3][14];
    unsigned area[4];       // 0x1F, 4, 2, other
    unsigned effect, no_slot, tail, fade;
} g_cover;
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + address - r.at];
        at += r.size;
    }
    return 0;
}
void Cover(unsigned k, const State& in, const State& out) {
    if (k >= kScene2 && k <= kScene4) {
        const unsigned step = Byte(in, kStep);
        ++g_cover.scene[k - kScene2][step < 13 ? step : 13];
    }
    if (k == kEnterArea) {
        const unsigned area = Byte(in, 0x904EFC) | Byte(in, 0x904EFD) << 8;
        ++g_cover.area[area == 0x1F ? 0 : area == 4 ? 1 : area == 2 ? 2 : 3];
    }
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what == 13) {
            if (i + 1 < kLog && Byte(out, kSlot) == 0xFF) ++g_cover.no_slot;
            else ++g_cover.effect;
        }
        if (out.log[i].what == 21) ++g_cover.tail;
        if (out.log[i].what == 22) ++g_cover.fade;
    }
}

using VoidFn = void (__cdecl*)();
using ArgFn = void (__cdecl*)(std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    void* theirs[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[20];
        if (c.n_calls > 20) bof3::Fatal("field_modes: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        theirs[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    move_script::Relocate(theirs[kScene2], kClones[kScene2].base, kClones[kScene2].size, kScene2Table);
    move_script::Relocate(theirs[kScene3], kClones[kScene3].base, kClones[kScene3].size, kScene3Table);
    move_script::Relocate(theirs[kScene4], kClones[kScene4].base, kClones[kScene4].size, kScene4Table);

    constexpr unsigned kRounds = 34000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("field_modes: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    SwapTables();
    g = kStubs;

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Field_ModeDispatch), reinterpret_cast<const void*>(&Field_ModeTail),
        reinterpret_cast<const void*>(&Field_ModeTailRun), reinterpret_cast<const void*>(&Scenario_CallA),
        reinterpret_cast<const void*>(&Scena16_Frame), reinterpret_cast<const void*>(&Scena16_Start),
        reinterpret_cast<const void*>(&Scena16_EnterArea), reinterpret_cast<const void*>(&Scena16_Area1F),
        reinterpret_cast<const void*>(&Scena16_Area04), reinterpret_cast<const void*>(&Scena16_Area02),
        reinterpret_cast<const void*>(&Scena16_Run), reinterpret_cast<const void*>(&Scena16_End),
        reinterpret_cast<const void*>(&Scena16_Scene2), reinterpret_cast<const void*>(&Scena16_Scene3),
        reinterpret_cast<const void*>(&Scena16_Scene4), reinterpret_cast<const void*>(&ClutStrip_FadeTo),
        reinterpret_cast<const void*>(&ClutStrip_Restore)};
    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        for (unsigned i = 0; i < kRegionBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        g_seed = Next();
        g_load_limit = Next() % 4;
        g_log_args = k == kCallA;
        static const std::uint32_t kLevels[] = {0, 1, 0x1E, 0x1F, 0x20, 0xFF, 0x11F, 0xFFFFFF1F};
        const std::uint32_t args[2] = {k == kClutFade && Often() ? kLevels[Next() % 8] : Next() % 4 ? Next() % 0x300 : Next(),
                                       Next()};
        Apply(input);
        Seed(k);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? ours[k] : theirs[k];
            if (k == kCallA || k == kClutFade) reinterpret_cast<ArgFn>(const_cast<void*>(fn))(args[0], args[1]);
            else reinterpret_cast<VoidFn>(const_cast<void*>(fn))();
            Capture(pass ? our_out : their_out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      field_modes self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                      their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    RestoreTables();
    Apply(saved);
    bof3::Log("shadow      field_modes self-test: %u rounds (%u per function), %u calls to the stand-ins, %u MISMATCHES; "
              "the scenario bytes, the camera, counters, effects, CLUT strip, view and music bytes, the stand-ins' log and "
              "the call table entries' arguments compared",
              kRounds, per[0], calls, bad);
    const Coverage& c = g_cover;
    bof3::Log("shadow      field_modes coverage: scene 2 steps 0..12+: %u %u %u %u %u %u %u %u %u %u %u %u %u; "
              "scene 3: %u %u %u %u %u %u %u %u %u %u %u; scene 4: %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
              c.scene[0][0], c.scene[0][1], c.scene[0][2], c.scene[0][3], c.scene[0][4], c.scene[0][5], c.scene[0][6],
              c.scene[0][7], c.scene[0][8], c.scene[0][9], c.scene[0][10], c.scene[0][11], c.scene[0][12] + c.scene[0][13],
              c.scene[1][0], c.scene[1][1], c.scene[1][2], c.scene[1][3], c.scene[1][4], c.scene[1][5], c.scene[1][6],
              c.scene[1][7], c.scene[1][8], c.scene[1][9], c.scene[1][10] + c.scene[1][11] + c.scene[1][12] + c.scene[1][13],
              c.scene[2][0], c.scene[2][1], c.scene[2][2], c.scene[2][3], c.scene[2][4], c.scene[2][5], c.scene[2][6],
              c.scene[2][7], c.scene[2][8], c.scene[2][9], c.scene[2][10], c.scene[2][11], c.scene[2][12], c.scene[2][13]);
    bof3::Log("shadow      field_modes coverage: areas 0x1F %u, 4 %u, 2 %u, other %u; effects placed %u, no slot %u; "
              "view shifted %u; fades %u",
              c.area[0], c.area[1], c.area[2], c.area[3], c.effect, c.no_slot, c.tail, c.fade);
    if (bad) bof3::Fatal("the field's mode handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace field_modes
