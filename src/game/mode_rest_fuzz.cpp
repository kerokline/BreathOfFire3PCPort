// BOF3X_SHADOW=mode_rest: group PM's fourteen functions through the scenario
// harness (scenario_harness.h, used unchanged) in field mode, once at
// start-up - the harness game modes 8..11 took (rest_3g_fuzz.cpp).
// docs/mode-rest.md section 4. BOF3X_PM_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows: capstone 2026-10-05, each extent read to its last
// instruction (the dispatchers' and mode 4's and 6's tail jumps included);
// every E8 / E9 leaving a copy is listed. Shapes: the dispatchers and the
// steps kState (void, no arguments); Sound_MusicPlaying is group PS's clone
// (its callers test the whole of it). The two step tables are DataTables,
// swapped for recorders while the fuzz runs. Every callee no standard set
// lists is listed here.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/mode_rest.h"
#include "game/mode_rest_callees.h"
#include "game/sound_rest.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_rest {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
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

// --- the clone table (capstone, 2026-10-05) -----------------------------------------
constexpr sh::CallSite kCalls495BC0[] = {{0x5, 0x454590},  {0xD, 0x454810},  {0x18, 0x5A9949}, {0x20, 0x454810},
                                         {0x2B, 0x4549F0}, {0x32, 0x4549F0}, {0x4E, 0x495040}, {0x55, 0x496830},
                                         {0x67, 0x587C20}, {0x72, 0x587BE0}};
constexpr sh::CallSite kCalls495C50[] = {
    {0x0, 0x5172F0},  {0x7, 0x495040},   {0xE, 0x496830},   {0x15, 0x5A9949},  {0x3C, 0x5367E0},  {0x44, 0x454810},
    {0x4F, 0x5A9949}, {0x57, 0x454810},  {0x7C, 0x454770},  {0x92, 0x454590},  {0x9A, 0x454810},  {0xA5, 0x5A9949},
    {0xAD, 0x454810}, {0xB6, 0x4560D0},  {0xBD, 0x495040},  {0xC4, 0x4967F0},  {0xD6, 0x587C20},  {0xE1, 0x587BA0},
    {0x140, 0x5B93D2}, {0x179, 0x594E00}, {0x191, 0x57C110}, {0x1AD, 0x594E00}, {0x1D2, 0x594E00}, {0x1EA, 0x59E330}};
constexpr sh::CallSite kCalls495E60[] = {{0x23, 0x517240}};
constexpr sh::CallSite kCalls495EA0[] = {{0x0, 0x532660}, {0x5, 0x517490}, {0xA, 0x517290}};
constexpr sh::CallSite kCalls495EC0[] = {{0x1, 0x5326B0}, {0x8, 0x517490}, {0xD, 0x517290}, {0x1C, 0x4DF820}, {0xA1, 0x536AC0}};
constexpr sh::CallSite kCalls495F90[] = {{0x1, 0x532860}, {0x8, 0x517490}, {0xD, 0x517290}, {0x12, 0x454810}};
constexpr sh::CallSite kCalls495FD0[] = {{0x2, 0x533CE0},  {0xA, 0x532A70},  {0xF, 0x517490},  {0x14, 0x517290},
                                         {0x2D, 0x587B40}, {0x59, 0x454590}, {0x8C, 0x454590}, {0xA0, 0x454590},
                                         {0xB0, 0x587AE0}, {0xB8, 0x59E330}};
constexpr sh::CallSite kCalls4960D0[] = {{0x1, 0x532B60}, {0x8, 0x517490}, {0xD, 0x517290}, {0x12, 0x454810}, {0x47, 0x531BB0}};
constexpr sh::CallSite kCalls496130[] = {{0x0, 0x532C10}, {0x5, 0x532D10}, {0xA, 0x517490}, {0xF, 0x517290}};
constexpr sh::CallSite kCalls496150[] = {{0x10, 0x454810}, {0x26, 0x533CE0}, {0x2E, 0x532D50}, {0x52, 0x5720C0},
                                         {0x58, 0x5725F0}, {0x82, 0x587AE0}, {0x9A, 0x52FEB0}, {0xA2, 0x5341E0},
                                         {0xA9, 0x532D10}, {0xAE, 0x517490}, {0xB3, 0x517290}};
constexpr sh::CallSite kCalls496230[] = {{0xA, 0x496A00}, {0x11, 0x496250}, {0x16, 0x517290}};

#define PM_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define PM_CALLS(a) a, PM_N(a)
#define PM_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSt = sh::Shape::kState, kCa = sh::Shape::kCall;
constexpr U kAll = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape}
const sh::Clone kAll14[] = {
    {"GameMode3_Run", 0x495BB0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, PM_FN(GameMode3_Run), 0, false, kSt},
    {"GameMode3_Enter", 0x495BC0, 0x82, PM_CALLS(kCalls495BC0), nullptr, 0, nullptr, 0, PM_FN(GameMode3_Enter), 0, false, kSt},
    {"GameMode3_Leave", 0x495C50, 0x202, PM_CALLS(kCalls495C50), nullptr, 0, nullptr, 0, PM_FN(GameMode3_Leave), 0, false, kSt},
    {"GameMode4_Run", 0x495E60, 0x28, PM_CALLS(kCalls495E60), nullptr, 0, nullptr, 0, PM_FN(GameMode4_Run), 0, false, kSt},
    {"GameMode5_Run", 0x495E90, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, PM_FN(GameMode5_Run), 0, false, kSt},
    {"GameMode5_TurnSense", 0x495EA0, 0x17, PM_CALLS(kCalls495EA0), nullptr, 0, nullptr, 0, PM_FN(GameMode5_TurnSense), 0, false, kSt},
    {"GameMode5_Turn", 0x495EC0, 0xCF, PM_CALLS(kCalls495EC0), nullptr, 0, nullptr, 0, PM_FN(GameMode5_Turn), 0, false, kSt},
    {"GameMode5_ToPlaces", 0x495F90, 0x31, PM_CALLS(kCalls495F90), nullptr, 0, nullptr, 0, PM_FN(GameMode5_ToPlaces), 0, false, kSt},
    {"GameMode5_Load", 0x495FD0, 0xF1, PM_CALLS(kCalls495FD0), nullptr, 0, nullptr, 0, PM_FN(GameMode5_Load), 0, false, kSt},
    {"GameMode5_Script", 0x4960D0, 0x5D, PM_CALLS(kCalls4960D0), nullptr, 0, nullptr, 0, PM_FN(GameMode5_Script), 0, false, kSt},
    {"GameMode5_Place", 0x496130, 0x1C, PM_CALLS(kCalls496130), nullptr, 0, nullptr, 0, PM_FN(GameMode5_Place), 0, false, kSt},
    {"GameMode5_Leave", 0x496150, 0xD7, PM_CALLS(kCalls496150), nullptr, 0, nullptr, 0, PM_FN(GameMode5_Leave), 0, false, kSt},
    {"GameMode6_Run", 0x496230, 0x1B, PM_CALLS(kCalls496230), nullptr, 0, nullptr, 0, PM_FN(GameMode6_Run), 0, false, kSt},
};
#undef PM_FN
#undef PM_CALLS
#undef PM_N

enum : unsigned {
    kRun3, kEnter3, kLeave3, kRun4, kRun5, kTurnSense, kTurn, kToPlaces, kLoad, kScript, kPlace, kLeave5, kRun6,
    kCount   // Sound_MusicPlaying is group PS's clone (sound_rest_fuzz.cpp)
};
static_assert(kCount == sizeof kAll14 / sizeof kAll14[0], "one enum entry a clone, in order");

// --- the stand-ins' effects: what a caller reads again after the call, moved ---------
// (the harness's own disturbance reaches the group's case on about one call in
// 24, too seldom for a read that follows one call of one branch)

// Rand (GameMode3_Leave's trip out): the leader's facing and the pending area's
// cells, read after it.
U RandEffect(const U*, U answer) {
    const U n = sh::Noise();
    switch (n % 5) {
    case 0: Mem(at::kLeaderFacing)[0] = static_cast<unsigned char>(n >> 8); break;
    case 1: Mem(at::kPendingFlags)[0] = static_cast<unsigned char>(n >> 8); break;
    case 2: SetWord(Mem(at::kPendingPlace), n >> 8); break;
    case 3: SetLong(Mem(at::kPendingX), static_cast<std::int32_t>(n)); break;
    default: SetLong(Mem(at::kPendingZ), static_cast<std::int32_t>(n)); break;
    }
    return answer;
}
// Flags_Clear (the trip home): the saved area, x and z, read after it.
U TripEffect(const U*, U answer) {
    const U n = sh::Noise();
    switch (n % 3) {
    case 0: SetWord(Mem(at::kTripArea), n >> 8); break;
    case 1: SetLong(Mem(at::kTripX), static_cast<std::int32_t>(n)); break;
    default: SetLong(Mem(at::kTripZ), static_cast<std::int32_t>(n)); break;
    }
    return answer;
}
// AreaMap_Elevation (GameMode5_Leave): 0x904AE5 bit 6, 0x904AE8 bit 3 and the
// track, read after it.
U ElevationEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n & 1) Mem(at::kBattleFlags)[0] ^= 0x40;
    if (n & 2) Mem(at::kBattleFlags2)[0] ^= 8;
    if ((n & 0xC) == 0) Music_Track = 0xFF;
    return answer;
}
// PartySet_Select (GameMode5_Turn): the formation, read again after it.
U FormationEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 3 != 0) Mem(at::kFormation)[0] = static_cast<unsigned char>(n >> 8);
    return answer;
}
// LoadDatFile (GameMode5_Load's event battle): the event battle, read again
// after it.
U LoadEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) Mem(at::kEventBattle)[0] = static_cast<unsigned char>((n >> 8) % 56);
    return answer;
}

#define PM_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kFl = sh::Answer::kFlag;
const sh::Callee kCallees[] = {
    // standard rows re-listed with an effect (the masks the standard set's)
    {"Rand", 0x5B93D2, KeyOf(&::Rand), 0, {}, sh::Answer::kRand, 0, 0, {}, &RandEffect},
    {PM_OURS(Flags_Clear), 2, {kAll, 0xFF}, kG, 0, 0, {}, &TripEffect},
    {PM_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},
    {PM_OURS(LoadDatFile), 1, {kAll}, kG, 0, 0, {}, &LoadEffect},
    // group PS's (sound_rest.cpp), called directly by the mode-3 steps
    {PM_OURS(Sound_MusicPlaying), 0, {}, kFl, 0, 0},
    // Music_IsPlaying by its address (Sound_MusicPlaying's jump, read through
    // SH_AT so that it is the address in the game)
    {"Music_IsPlaying", ::bof3::addr::Music_IsPlaying, ::bof3::addr::Music_IsPlaying, 0, {}, kFl, 0, 0},
    // ours, no standard set lists them; the answers in al are kFlag (each
    // caller keeps al in bl and tests it)
    {PM_OURS(Menu_Frame), 0, {}, kPh, 0, 0},
    {PM_OURS(Menu_WaitTransition), 1, {0xFF}, kG, 0, 0},
    {PM_OURS(Field_WaitTransition), 1, {0xFF}, kG, 0, 0},
    {PM_OURS(CommuSim_RollOffers), 0, {}, kPh, 0, 0},
    {PM_OURS(Window_ResetAll), 0, {}, kPh, 0, 0},
    {PM_OURS(Field_FrameScripted), 0, {}, kPh, 0, 0},
    {PM_OURS(Field_ObjectsFrame), 0, {}, kPh, 0, 0},
    {PM_OURS(Encounter_PartyTurnSense), 0, {}, kPh, 0, 0},
    {PM_OURS(Encounter_PartyTurn), 0, {}, kFl, 0, 0},
    {PM_OURS(Encounter_PartyToPlaces), 0, {}, kFl, 0, 0},
    {PM_OURS(Encounter_PartyAtPlaces), 0, {}, kPh, 0, 0},
    {PM_OURS(Encounter_PartyScriptOnce), 0, {}, kFl, 0, 0},
    {PM_OURS(PartySet_Select), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &FormationEffect},   // field_event.cpp: both read as bytes
    {PM_OURS(Party_PlaceAtSlots), 0, {}, kPh, 0, 0},
    {PM_OURS(Party_ScriptTicks), 0, {}, kPh, 0, 0},
    {PM_OURS(Party_PlacesByList), 0, {}, kPh, 0, 0},
    {PM_OURS(Field_ZoneCounterRoll), 1, {kAll}, kG, 0, 0},
    {PM_OURS(Field_AfterBattleTally), 0, {}, kPh, 0, 0},
    {PM_OURS(Look_PadControl), 0, {}, kPh, 0, 0},
    {PM_OURS(GameMode_LookEnd), 0, {}, kPh, 0, 0},
};
#undef PM_OURS

const sh::DataTable kTables[] = {
    {0x656A74, 3},   // GameMode3_Steps
    {0x656A84, 8},   // GameMode5_Steps
};

// The group's own regions beyond field mode's standard ones (none overlaps
// one of them: docs/mode-rest.md section 4).
const sh::Region kRegions[] = {
    {0x66C7E8, 4},          // Game_Mode, Game_Step (effect mode's; not field mode's)
    {at::kMessageBits, 4},  // the message cells' 0x7DEE44
    {at::kMusicWord, 2},    // the word 0x7E0678
    {at::kPendingX, 4},     // the trip's destination x 0x903860
    {at::kTripAsked, 4},    // 0x905B60, 0x905B61
    {0x905BA8, 1},          // Draw_SortOnX
    {0x92BF19, 1},          // Draw_OtSlot
    {at::kBattleBytes, 0x50},   // the battle bytes 0x904AA0..0x904AEF
    {at::kElevationBase, 4},    // 0x939860
    {at::kPendingPlace, 2},     // 0x937F82
};

// --- the seed ------------------------------------------------------------------------

void Seed(unsigned k) {
    switch (k) {
    case kRun3: Game_Step = static_cast<unsigned short>(sh::Next() % 3); break;
    case kRun5: Game_Step = static_cast<unsigned short>(sh::Next() % 8); break;
    case kRun6: Game_Step = static_cast<unsigned short>(sh::Half() ? 0 : PickOf(1, 2, 0x100, sh::Next())); break;
    case kRun4: Mem(at::kMessageBits)[0] = static_cast<unsigned char>(sh::Next()); break;
    case kEnter3: SetWord(Mem(at::kMusicWord), PickOf(0x6E, 0x6E, 0x6F, 0x6D, 0x16E, sh::Next())); break;
    case kLeave3:
        Mem(at::kPartyChanged)[0] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        SetWord(Mem(at::kMusicWord), PickOf(0x6E, 0x6E, 0x6F, sh::Next()));
        Mem(at::kTripAsked)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        Mem(at::kAreaAsked)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        Mem(at::kTripOut)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        SetWord(Mem(at::kAreaWord), PickOf(0xBD, 0xBD, 0xBC, 0xBE, 0x1BD, sh::Next()));
        break;
    case kTurn: {
        SetLong(Mem(at::kFormation), static_cast<std::int32_t>(
                                         (sh::Next() & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 4 : sh::Next() & 0xFF)));
        break;
    }
    case kToPlaces: Field_Kind2Hold = static_cast<unsigned char>(sh::Often() ? 0 : sh::Next()); break;
    case kLoad:
        Music_Track = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0xFE, sh::Next()));
        Mem(at::kEventBattle)[0] = static_cast<unsigned char>(PickOf(0, 0, sh::Next() % 56, sh::Next()));
        Cond_ByteFA = static_cast<signed char>(PickOf(7, 8, 9, 0, 0xFF, 0x7F, 0x80, sh::Next()));
        break;
    case kLeave5:
        Field_Kind2Hold = static_cast<unsigned char>(sh::Often() ? 0 : sh::Next());
        Music_Track = static_cast<unsigned char>(PickOf(0xFF, 0xFE, sh::Next()));
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only; the case by sh::DisturbCase).
void Disturb(U h) {
    const U v = h >> 3;
    switch (sh::DisturbCase(h, 15)) {
    case 0: Game_Step = static_cast<unsigned short>(v % 8); break;
    case 1: Mem(at::kBattleFlags)[0] = static_cast<unsigned char>(v); break;
    case 2: Mem(at::kFormation)[0] = static_cast<unsigned char>((v & 1) ? (v >> 1) % 4 : v >> 1); break;
    case 3: Mem(at::kEventBattle)[0] = static_cast<unsigned char>((v & 1) ? (v >> 1) % 56 : 0); break;
    case 4: Music_Track = static_cast<unsigned char>((v & 1) ? 0xFF : v >> 1); break;
    case 5: Field_Kind2Hold = static_cast<unsigned char>((v & 1) ? 0 : v >> 1); break;
    case 6: Mem((v & 1) ? at::kTripAsked : at::kAreaAsked)[0] = static_cast<unsigned char>((v & 2) ? 0 : v >> 2); break;
    case 7: Mem(at::kTripOut)[0] = static_cast<unsigned char>((v & 1) ? 0 : v >> 1); break;
    case 8: SetWord(Mem(at::kMusicWord), (v & 1) ? 0x6Eu : v >> 1); break;
    case 9: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v); break;
    case 10: Mem((v & 1) ? at::kLeaderFacing : at::kPendingFlags)[0] = static_cast<unsigned char>(v >> 1); break;
    case 11: SetWord(Mem(at::kTripX + 4 * ((v & 3) % 3)), v >> 2); break;
    case 12: Field_InputFlags = static_cast<unsigned char>(v); break;
    case 13: Mem(at::kPartyChanged)[0] = static_cast<unsigned char>((v & 1) ? 0 : v >> 1); break;
    case 14: Mem(at::kPartyList + (v & 3) % 3)[0] = static_cast<unsigned char>(v >> 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_PM_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_PM_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll14[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll14[k];
        }
    if (n == 0) bof3::Fatal("mode_rest: BOF3X_PM_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"mode_rest", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.field = true;
    sh::Run(g);
}

}  // namespace mode_rest
