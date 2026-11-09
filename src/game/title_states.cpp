// The title's state handlers - the eight entries of Title_States that task
// 1's body Title_Task calls one a frame - with the two music helpers and the
// demo's corner badge they call, the logo's three draw sets under
// Title_DrawLogo, and the scenario start the demo goes through (0x56D5E0, the
// DAT load 0x56D670 under it, and 0x56D8C0, the first thing the scenario's
// dispatcher runs). docs/title-states.md.
#include "game/title_states.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// Bytes the handlers touch that have no name (docs/title-states.md has the
// PSX twin of each).
constexpr ::bof3::image::Va kPlayClock{0x9040C8};      // four bytes zeroed starting the demo (PSX 0x80144FBC)
// kPartyCombo: now AreaW0b_PartySet (symbols.toml). 0xFF when the demo ends (PSX 0x80145020)
// kMusicTrack: now Music_Track (symbols.toml). the track Music_Play last started, 0xFF none
constexpr ::bof3::image::Va kScenarioBytes{0x8034E0};  // Cond_ByteFA and the nine bytes after it (PSX 0x8014686C)
constexpr ::bof3::image::CodeVa kModeDispatch{bof3::addr::Field_ModeDispatch};   // Field_ModeDispatch, another file's: called by address
const std::uint32_t kCondFlags = Low32(Cond_Flags);

}  namespace title_states { bool g_psp_title = false; }
namespace {
const title_states::CalleesTitleStates kOriginalsTitleStates = {
    Port_DroppedCall, LoadDatFile, File_LoadDone, Task_Sleep, Gfx_ClutStripRestore, Music_Play, Transition_Start,
    Title_FadeMusic, Title_StopMusic, Effect_ClearAll, Scenario_Start, Task_Create, Music_FadeOut, Music_FadeOutStop,
    Title_DrawBadge, Task_Exit, Gpu_SetDrawMode, Gfx_CommitPrim, Title_Sprite, Prim_SetShade, Scenario_Load,
    Field_LoadingFrame, reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(::bof3::Code(kModeDispatch.v))),
};
}  namespace title_states { CalleesTitleStates gTitleStates = kOriginalsTitleStates; }
// original 0x56D5E0: start scenario chapter `chapter` (PSX 0x801A870C). The
// chapter byte is Cond_ByteFA (PSX 0x8014686C, the index into the scenario
// vtable table the dispatcher Field_ModeDispatch reads); the scenario's
// state bytes after it are cleared, and the chapter's own Cond_Flags dword
// zeroed and remembered at 0x929ED0. Then its DAT (0x306 + chapter), a wait
// for it - a field frame without the event script each frame, unless no
// area is loaded (Game_AreaNumber 0xFFFF) or an area change is pending
// (Field_Request 5) - and the scenario's first frame (a tail jump).
//
// As the original has it: the argument's low byte only, sign-extended for
// the Cond_Flags index, so chapters 0x80..0xFF reach below Cond_Flags; no
// bounds check. The bytes 0x8034E1..0x8034E9 are zeroed whatever the chapter
// (MoveScript_Var7 is 0x8034E4, Field_StepCountPrev 0x8034E8). File_LoadDone
// is asked before any frame; on the PC it is always done, so the loop body
// never runs.
extern "C" void __cdecl Scenario_Start(int chapter) {
    const auto c = static_cast<signed char>(chapter);
    unsigned char* const s = move_script::At(::bof3::Img(kScenarioBytes.v));
    s[0] = static_cast<unsigned char>(c);
    s[2] = 0;
    s[3] = 0;
    s[4] = 0;
    move_script::SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    move_script::SetWord(s + 8, 0);
    const std::uint32_t flags = kCondFlags + static_cast<std::uint32_t>(8 * static_cast<int>(c));
    move_script::SetLong(move_script::At(flags), 0);
    Chapter_FlagRow = static_cast<std::int32_t>(flags);
    title_states::gTitleStates.scenario_load();
    while (title_states::gTitleStates.load_done() == 0) {
        if (Game_AreaNumber != 0xFFFF && Field_Request != 5) title_states::gTitleStates.loading_frame();
        title_states::gTitleStates.sleep(1);
    }
    title_states::gTitleStates.mode_dispatch();
}

// original 0x56D670: the current chapter's DAT (PSX 0x801A880C, which loads
// 0x295 + chapter, SCENA<chapter>.EMI). As the original has it: the chapter
// byte sign-extended.
extern "C" void __cdecl Scenario_Load(void) {
    title_states::gTitleStates.load_dat(static_cast<int>(Cond_ByteFA) + 0x306);
}

// original 0x56D8C0: state 0 of the dispatcher 0x56D8B0 (PSX 0x801A8C34):
// its state byte to 1 so the next frame runs entry 1 (0x56D920), the six
// bytes and two words around it zeroed, and 16 bytes of the story flags
// (Cond_Flags + 0xA0) seeded from the table at 0x662CD8.
//
// As the original has it: four dword copies (the PSX copies bytewise); the
// table and the flags do not overlap, so the order is unobservable.
extern "C" void __cdecl Scenario_SubInit(void) {
    unsigned char* const s = move_script::At(::bof3::Img(ScenaSc7_DrawPool_addr));
    s[2] = 1;
    s[0] = 0;
    s[3] = 0;
    s[4] = 0;
    move_script::SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    move_script::SetWord(s + 8, 0);
    for (unsigned i = 0; i < 16; i += 4) move_script::SetLong(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0 + i), move_script::Long(move_script::At(::bof3::Img(Scenario_SubInitTable_addr) + i)));
}

namespace {

// --- BOF3X_SHADOW=title_states: a differential fuzz, once at start-up ------
// Seventeen byte-copies, every call out re-aimed at the recorders below (the
// tail jumps included). One round: one of the seventeen, random bytes in
// every region any of them touches, then each branch's boundaries seeded;
// theirs, then from the same state ours; the regions, the primitive buffers,
// the packet cursor and the recorders' log compared.

constexpr unsigned kLog = 48;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed, g_load_zeros, g_load_calls;
unsigned char g_prim[0x100], g_sprite_out[0x80];

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    const auto prim = reinterpret_cast<std::uintptr_t>(g_prim), out = reinterpret_cast<std::uintptr_t>(g_sprite_out);
    if (at >= prim && at < prim + sizeof g_prim) return 0x1000 + static_cast<std::uint32_t>(at - prim);
    if (at >= out && at < out + sizeof g_sprite_out) return 0x2000 + static_cast<std::uint32_t>(at - out);
    return static_cast<std::uint32_t>(at);
}

// A callee may change what its caller reads or writes after it: every byte
// below is one some handler reads again, or stores, after a call - so a read
// moved before a call, or a store moved across one, shows.
struct Watch { std::uint32_t at; unsigned char lo, hi; };   // a value in lo..hi, or any when lo > hi
const Watch kWatch[] = {
    {::bof3::Img(0x66C808), 0, 7},     // Title_State, low byte
    {::bof3::Img(0x66C809), 0, 1},     // Title_State, high byte
    {::bof3::Img(0x66C7F9), 0, 3},     // Title_Fade
    {::bof3::Img(0x66C7FA), 0, 3},     // Title_LogoState
    {::bof3::Img(0x66C7FB), 0, 1},     // Title_StartArmed
    {::bof3::Img(0x66C7F8), 0, 1},     // Title_BadgeOn
    {::bof3::Img(0x66C806), 1, 0},     // Title_BlinkCount
    {::bof3::Img(0x66C807), 0, 1},
    {::bof3::Img(Music_Track_addr), 0xFE, 0xFF},
    {::bof3::Img(0x937F90), 1, 0},     // Gfx_ClutStripDirty
    {::bof3::Img(kPlayClock.v), 1, 0},
    {::bof3::Img(kPlayClock.v + 3), 1, 0},
    {::bof3::Img(0x7E0918), 1, 0},     // Draw_PassFlags
    {::bof3::Img(kScenarioBytes.v + 4), 1, 0},   // MoveScript_Var7
    {::bof3::Img(AreaW0b_PartySet_addr), 1, 0},
    {::bof3::Img(0x66C7D8), 4, 6},     // Field_Request
    {::bof3::Img(0x904EFC), 0xFE, 0xFF},  // Game_AreaNumber, low byte
    {::bof3::Img(0x904EFD), 0xFE, 0xFF},
    {::bof3::Img(0x66C7D0), 0, 1},     // task 0's state
};
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const Watch& w = kWatch[(h >> 4) % (sizeof kWatch / sizeof kWatch[0])];
    const unsigned v = (h >> 12) & 0xFF;
    At(w.at)[0] = static_cast<unsigned char>(w.lo > w.hi ? v : w.lo + v % (w.hi - w.lo + 1u));
}

void __cdecl StubDropped(unsigned b) { Record(1, b & 0xFF); Disturb(); }
void __cdecl StubLoadDat(int index) { Record(2, static_cast<std::uint32_t>(index)); Disturb(); }
// Not done for the round's first g_load_zeros questions, then done.
int __cdecl StubLoadDone() {
    Record(3);
    Disturb();
    return g_load_calls++ < g_load_zeros ? 0 : 1;
}
void __cdecl StubSleep(int frames) { Record(4, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubClutRestore() { Record(5); Disturb(); }
void __cdecl StubMusicPlay(unsigned track, int unknown) {
    Record(6, track, static_cast<std::uint32_t>(unknown));
    Music_Track = static_cast<unsigned char>(track);
    Disturb();
}
void __cdecl StubTransition(unsigned kind) { Record(7, kind & 0xFF); Disturb(); }
void __cdecl StubFadeMusic() { Record(8); Disturb(); }
void __cdecl StubStopMusic() { Record(9); Disturb(); }
void __cdecl StubEffectClear() { Record(10); Disturb(); }
void __cdecl StubScenarioStart(int chapter) { Record(11, static_cast<std::uint32_t>(chapter) & 0xFF); Disturb(); }
void __cdecl StubTaskCreate(int slot, void* entry) {
    Record(12, static_cast<std::uint32_t>(slot), Low32(entry));
    Disturb();
}
// The fades may end the track or leave it: what Title_StopMusic stores over.
void __cdecl StubMusicFadeOut(int frames) {
    Record(13, static_cast<std::uint32_t>(frames));
    Music_Track = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
void __cdecl StubMusicFadeStop(int frames) {
    Record(14, static_cast<std::uint32_t>(frames));
    Music_Track = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
void __cdecl StubBadge() { Record(15); Disturb(); }
void __cdecl StubTaskExit() { Record(16); Disturb(); }
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(20, Id(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage ^ static_cast<std::uint32_t>(tw) << 16);
    std::memset(prim + 4, static_cast<int>(Hash() & 0xFF), 8);
    Disturb();
}
// The commit moves the packet cursor, as the real one does, so that a cursor
// read too early shows.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(21, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x80u;
    Disturb();
}
// Title_Sprite reads x and y as s16 and index and semi as bytes: that much is
// logged. It returns a primitive of its own, filled with noise.
unsigned char* __cdecl StubSprite(int x, int y, unsigned index, unsigned slot, unsigned semi) {
    Record(22, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), index & 0xFF, slot ^ (semi & 0xFF) << 16);
    unsigned char* const out = g_sprite_out + (Hash() % 4) * 0x20;
    std::memset(out, static_cast<int>(Hash() >> 8 & 0xFF), 0x20);
    Disturb();
    return out;
}
// Prim_SetShade, which stores the level's byte in the three colour bytes.
void __cdecl StubShade(unsigned char* prim, unsigned level) {
    Record(23, Id(prim), level & 0xFF);
    prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(level);
}
void __cdecl StubScenarioLoad() { Record(24); Disturb(); }
void __cdecl StubLoadingFrame() { Record(25, Field_Request, Game_AreaNumber); Disturb(); }
void __cdecl StubModeDispatch() { Record(26, static_cast<unsigned char>(Cond_ByteFA)); Disturb(); }

// The stand-ins take bytes as dwords and mask them: the copies push whole
// registers.
template <typename T> T As(const void* p) { return reinterpret_cast<T>(const_cast<void*>(p)); }
const title_states::CalleesTitleStates kStubs = {
    As<void (__cdecl*)(unsigned char)>(reinterpret_cast<const void*>(&StubDropped)), StubLoadDat, StubLoadDone, StubSleep, StubClutRestore,
    StubMusicPlay, As<void (__cdecl*)(unsigned char)>(reinterpret_cast<const void*>(&StubTransition)), StubFadeMusic, StubStopMusic,
    StubEffectClear, StubScenarioStart, StubTaskCreate, StubMusicFadeOut, StubMusicFadeStop, StubBadge, StubTaskExit,
    StubDrawMode, StubCommit, StubSprite, As<void (__cdecl*)(unsigned char*, unsigned char)>(reinterpret_cast<const void*>(&StubShade)),
    StubScenarioLoad, StubLoadingFrame, StubModeDispatch,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x4DF820: return f(&StubDropped);
    case 0x454590: return f(&StubLoadDat);
    case 0x454810: return f(&StubLoadDone);
    case 0x5A9949: return f(&StubSleep);
    case 0x4549B0: return f(&StubClutRestore);
    case 0x587AE0: return f(&StubMusicPlay);
    case 0x495040: return f(&StubTransition);
    case 0x4624B0: return f(&StubFadeMusic);
    case 0x4624D0: return f(&StubStopMusic);
    case 0x5898A0: return f(&StubEffectClear);
    case 0x56D5E0: return f(&StubScenarioStart);
    case 0x5A9914: return f(&StubTaskCreate);
    case 0x587BE0: return f(&StubMusicFadeOut);
    case 0x587B40: return f(&StubMusicFadeStop);
    case 0x4624F0: return f(&StubBadge);
    case 0x5A99AD: return f(&StubTaskExit);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x462560: return f(&StubSprite);
    case 0x462A70: return f(&StubShade);
    case 0x56D670: return f(&StubScenarioLoad);
    case 0x517290: return f(&StubLoadingFrame);
    case 0x56D690: return f(&StubModeDispatch);
    default: bof3::Fatal("title_states: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The seventeen copies and their calls and tail jumps out, by capstone
// 2026-09-22: every other jump stays inside, none has a jump table.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kLoadCalls[] = {{0x2, 0x4df820}, {0xc, 0x454590}, {0x14, 0x454810}, {0x1f, 0x5a9949},
                               {0x27, 0x454810}, {0x30, 0x4549b0}, {0x4a, 0x587ae0}};
constexpr Call kLogoCalls[] = {{0xb, 0x495040}, {0x13, 0x4624b0}};
constexpr Call kStartDemoCalls[] = {{0x1e, 0x4624d0}, {0x24, 0x4df820}, {0x29, 0x5898a0}, {0x4e, 0x56d5e0}, {0x59, 0x5a9914}};
constexpr Call kDemoCalls[] = {{0x14, 0x454810}, {0x3a, 0x495040}, {0x41, 0x587b40}, {0x50, 0x4624f0}};
constexpr Call kDemoEndCalls[] = {{0x1b, 0x4624f0}};
constexpr Call kMenuCalls[] = {{0xa, 0x4624d0}, {0x11, 0x4df820}, {0x1d, 0x5a9914}, {0x25, 0x5a99ad}};
constexpr Call kFadeCalls[] = {{0xb, 0x587be0}};
constexpr Call kStopCalls[] = {{0xb, 0x587b40}};
constexpr Call kBadgeCalls[] = {{0x47, 0x5a77c0}, {0x50, 0x461e50}, {0x62, 0x462560}};
constexpr Call kSetACalls[] = {{0x10, 0x5a77c0}, {0x19, 0x461e50}, {0x31, 0x462560}, {0x3c, 0x462a70},
                               {0x53, 0x5a77c0}, {0x5f, 0x461e50}, {0x70, 0x462560}, {0x77, 0x462a70},
                               {0x8b, 0x462560}, {0x92, 0x462a70}, {0xbb, 0x462560}};
constexpr Call kSetCCalls[] = {{0x29, 0x5a77c0}, {0x32, 0x461e50}, {0x3f, 0x462560}, {0x46, 0x462a70},
                               {0x59, 0x462560}, {0x63, 0x462a70}, {0x7d, 0x5a77c0}, {0x86, 0x461e50},
                               {0x92, 0x462560}, {0x99, 0x462a70}, {0xab, 0x462560}, {0xb5, 0x462a70}};
constexpr Call kSetBCalls[] = {{0x15, 0x5a77c0}, {0x1e, 0x461e50}, {0x36, 0x462560},
                               {0x41, 0x462a70}, {0x56, 0x462560}, {0x60, 0x462a70}};
constexpr Call kScenarioCalls[] = {{0x43, 0x56d670}, {0x48, 0x454810}, {0x65, 0x517290},
                                   {0x6c, 0x5a9949}, {0x74, 0x454810}, {0x7d, 0x56d690}};
constexpr Call kScenarioLoadCalls[] = {{0xd, 0x454590}};
enum : unsigned {
    kStateLoad, kStateFadeIn, kStateBackdrop, kStateLogo, kStateStartDemo, kStateDemo, kStateDemoEnd, kStateMenu,
    kFadeMusic, kStopMusic, kBadge, kSetA, kSetC, kSetB, kScenarioStart, kScenarioLoad, kSubInit, kCount
};
#define TS_CLONE(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
const Clone kClones[kCount] = {
    TS_CLONE("Title_StateLoad", 0x462200, 0x8A, kLoadCalls),
    {"Title_StateFadeIn", 0x462290, 0x1A, nullptr, 0},
    {"Title_StateBackdrop", 0x4622B0, 0x28, nullptr, 0},
    TS_CLONE("Title_StateLogo", 0x4622E0, 0x20, kLogoCalls),
    TS_CLONE("Title_StateStartDemo", 0x462300, 0x6A, kStartDemoCalls),
    TS_CLONE("Title_StateDemo", 0x462370, 0x55, kDemoCalls),
    TS_CLONE("Title_StateDemoEnd", 0x4623D0, 0x20, kDemoEndCalls),
    TS_CLONE("Title_StateMenu", 0x4623F0, 0x2B, kMenuCalls),
    TS_CLONE("Title_FadeMusic", 0x4624B0, 0x12, kFadeCalls),
    TS_CLONE("Title_StopMusic", 0x4624D0, 0x1B, kStopCalls),
    TS_CLONE("Title_DrawBadge", 0x4624F0, 0x6B, kBadgeCalls),
    TS_CLONE("Title_DrawSetA", 0x462820, 0x105, kSetACalls),
    TS_CLONE("Title_DrawSetC", 0x462930, 0xC2, kSetCCalls),
    TS_CLONE("Title_DrawSetB", 0x462A00, 0x6D, kSetBCalls),
    TS_CLONE("Scenario_Start", 0x56D5E0, 0x82, kScenarioCalls),
    TS_CLONE("Scenario_Load", 0x56D670, 0x14, kScenarioLoadCalls),
    {"Scenario_SubInit", 0x56D8C0, 0x5B, nullptr, 0},
};
#undef TS_CLONE

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {::bof3::Img(bof3::addr::Task_Records), 0x48},   // task 0's state, Field_Request, Game_Mode, task 1's words,
                                        // Transition_Busy
    {::bof3::Img(0x7E1BEC), 4},                      // Input_Pressed, read as a dword
    {::bof3::Img(0x7E0918), 1},                      // Draw_PassFlags
    {::bof3::Img(0x937F90), 1},                      // Gfx_ClutStripDirty
    {::bof3::Img(kScenarioBytes.v), 0x10},             // Cond_ByteFA .. Field_StepCountPrev
    {::bof3::Img(Chapter_FlagRow_addr), 4},
    {::bof3::Img(ScenaSc7_DrawPool_addr), 0x10},
    {::bof3::Img(0x903F90) - 0x400, 0x808},          // Cond_Flags - 0x400: every chapter's dword, the story flags,
                                        // the play clock, the party combination and the music track
    {::bof3::Img(0x904EFC), 2},                      // Game_AreaNumber
    {::bof3::Img(Scenario_SubInitTable_addr), 0x10},
};
constexpr unsigned kRegionBytes = 0x48 + 4 + 1 + 1 + 0x10 + 4 + 0x10 + 0x808 + 2 + 0x10;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[sizeof g_prim], sprite_out[sizeof g_sprite_out];
    std::uint32_t packet;   // Gfx_PacketNext, as an Id
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.sprite_out, g_sprite_out, sizeof g_sprite_out);
    s.packet = Id(Gfx_PacketNext);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_sprite_out, s.sprite_out, sizeof g_sprite_out);
    Gfx_PacketNext = g_prim + (s.packet - 0x1000);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_load_calls = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
std::uint16_t Pick(const std::uint16_t* values, unsigned n) { return values[Next() % n]; }

void SetTaskZero(bool running) { SetWord(At(::bof3::Img(bof3::addr::Task_Records)), running ? 1 + Next() % 2 : 0); }

// Each branch's boundaries, on top of the random bytes; `args` are the four
// dwords every function is called with (cdecl: the ones it does not take are
// ignored).
void Seed(unsigned k, std::uint32_t (&args)[4]) {
    static const std::uint16_t kTimer[] = {0, 1, 2, 0xFFFF, 0x8000};
    switch (k) {
    case kStateFadeIn:
        Title_Fade = static_cast<unsigned char>(Often() ? 2 : Next() % 2 ? Next() % 4 : Next());
        break;
    case kStateBackdrop:
    case kStateLogo:
        if (Often()) Title_Timer = Pick(kTimer, 5);
        break;
    case kStateStartDemo:
    case kStateMenu:
        if (Often()) Transition_Busy = 0;
        else if (Often()) Transition_Busy = static_cast<std::uint16_t>(Next() % 2 ? 0x100 : 1);
        break;
    case kStateDemo: {
        SetTaskZero(Next() % 4 != 0);
        if (Often()) Transition_Busy = 0;
        static const std::uint16_t kPad[] = {0x800, 0x800, 0xFFFF, 0x7FF, 0xF7FF, 0x1000, 0x400, 0x0};
        if (Often()) Input_Pressed = Pick(kPad, 8);
        break;
    }
    case kStateDemoEnd:
        SetTaskZero(Next() % 2 != 0);
        break;
    case kFadeMusic:
    case kStopMusic: {
        static const unsigned char kTrack[] = {0xFF, 0xFF, 0xFE, 0x00, 0x7F};
        if (Often()) Music_Track = kTrack[Next() % 5];
        break;
    }
    case kBadge: {
        static const unsigned char kRequest[] = {5, 5, 4, 6, 0x85};
        static const std::uint16_t kMode[] = {2, 2, 1, 3, 0x102};
        if (Often()) Field_Request = kRequest[Next() % 5];
        if (Often()) Game_Mode = Pick(kMode, 5);
        if (Often()) Transition_Busy = 0;
        if (Often()) Draw_PassFlags = static_cast<unsigned char>(Next() % 2 ? 0 : 1u << (Next() % 8));
        if (Often()) Title_BadgeOn = static_cast<unsigned char>(Next() % 2 ? 0 : 1u << (Next() % 8));
        break;
    }
    case kSetA: {
        if (Often()) Title_Fade = 0;
        if (Often()) Title_LogoState = static_cast<unsigned char>(Next() % 3 ? 2 : Next() % 4);
        static const std::uint16_t kCount[] = {0x1E, 0x1F, 0x20, 0x3F, 0x40, 0xFF, 0x100, 0xFFFF};
        if (Often()) Title_BlinkCount = Pick(kCount, 8);
        [[fallthrough]];
    }
    case kSetB:
    case kSetC:
        // Flags and shades with stale upper bytes, the flag's low byte often 0.
        args[2] = Next() % 2 ? (Next() & 0xFFFFFF00u) | (Next() % 3 ? 0 : Next() % 2) : Next();
        args[3] = Next();
        if (Next() % 4 == 0) args[0] = static_cast<std::uint32_t>(0x7FFFFF00 + Next() % 0x100);
        break;
    case kScenarioStart: {
        static const std::uint32_t kChapter[] = {0x10, 0, 0x7F, 0x80, 0xFF};
        args[0] = (Next() & 0xFFFFFF00u) | (Often() ? kChapter[Next() % 5] : Next() & 0xFF);
        if (Next() % 4 == 0) Game_AreaNumber = 0xFFFF;
        if (Next() % 4 == 0) Field_Request = 5;
        break;
    }
    case kScenarioLoad: {
        static const unsigned char kChapter[] = {0x10, 0, 0x7F, 0x80, 0xFF};
        if (Often()) Cond_ByteFA = kChapter[Next() % 5];
        break;
    }
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned state_advanced[8];   // rounds in which a state handler moved Title_State on by one
    unsigned demo_over, demo_start, badge_off, badge_drawn, blink, music_stopped, loops;
} g_cover;
unsigned Byte(const State& s, std::uint32_t address) { return s.memory[address - ::bof3::Img(bof3::addr::Task_Records)]; }
unsigned Half(const State& s, std::uint32_t address) { return Byte(s, address) | Byte(s, address + 1) << 8; }
bool Logged(const State& s, std::uint32_t what) {
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i) if (s.log[i].what == what) return true;
    return false;
}
void Cover(unsigned k, const State& in, const State& out) {
    const std::uint32_t aState = ::bof3::Img(0x66C808), aBadge = ::bof3::Img(0x66C7F8), aBlink = ::bof3::Img(0x66C806);
    // (State 7 does not move the word: it ends the task - counted by the call.)
    if (k < kStateMenu && Half(out, aState) == ((Half(in, aState) + 1) & 0xFFFF)) ++g_cover.state_advanced[k];
    if (k == kStateMenu && Logged(out, 16)) ++g_cover.state_advanced[k];
    if ((k == kStateDemo || k == kStateDemoEnd) && Half(in, ::bof3::Img(bof3::addr::Task_Records)) == 0) ++g_cover.demo_over;
    if (k == kStateDemo && Logged(out, 14)) ++g_cover.demo_start;
    if (k == kBadge && Byte(in, aBadge) != 0 && Byte(out, aBadge) == 0) ++g_cover.badge_off;
    if (k == kBadge && Logged(out, 22)) ++g_cover.badge_drawn;
    if (k == kSetA && Half(out, aBlink) != 0) ++g_cover.blink;
    if ((k == kStopMusic || k == kFadeMusic) && (Logged(out, 13) || Logged(out, 14))) ++g_cover.music_stopped;
    if ((k == kStateLoad || k == kScenarioStart) && Logged(out, 4)) ++g_cover.loops;
}

using Fn4 = void (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 34000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("title_states: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    Gfx_PacketNext = g_prim;
    Capture(saved);
    title_states::gTitleStates = kStubs;

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Title_StateLoad), reinterpret_cast<const void*>(&Title_StateFadeIn),
        reinterpret_cast<const void*>(&Title_StateBackdrop), reinterpret_cast<const void*>(&Title_StateLogo),
        reinterpret_cast<const void*>(&Title_StateStartDemo), reinterpret_cast<const void*>(&Title_StateDemo),
        reinterpret_cast<const void*>(&Title_StateDemoEnd), reinterpret_cast<const void*>(&Title_StateMenu),
        reinterpret_cast<const void*>(&Title_FadeMusic), reinterpret_cast<const void*>(&Title_StopMusic),
        reinterpret_cast<const void*>(&Title_DrawBadge), reinterpret_cast<const void*>(&Title_DrawSetA),
        reinterpret_cast<const void*>(&Title_DrawSetC), reinterpret_cast<const void*>(&Title_DrawSetB),
        reinterpret_cast<const void*>(&Scenario_Start), reinterpret_cast<const void*>(&Scenario_Load),
        reinterpret_cast<const void*>(&Scenario_SubInit)};
    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, packet); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.packet = 0x1000 + Next() % 0x40;
        g_seed = Next();
        g_load_zeros = Next() % 3 == 0 ? 1 + Next() % 3 : 0;
        std::uint32_t args[4] = {Next() % 2 ? Next() % 0x200 : Next(), Next() % 2 ? Next() % 0x100 : Next(), Next(), Next()};
        Apply(input);
        Seed(k, args);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? ours[k] : theirs[k];
            reinterpret_cast<Fn4>(const_cast<void*>(fn))(args[0], args[1], args[2], args[3]);
            Capture(pass ? our_out : their_out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      title_states self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                          their_out.log_n, our_out.log_n);
        }
    }
    title_states::gTitleStates = kOriginalsTitleStates;
    Apply(saved);
    Gfx_PacketNext = saved_packet;
    bof3::Log("shadow      title_states self-test: %u rounds (%u per function), %u calls to the stand-ins, %u MISMATCHES; "
              "task 0 and 1's words, the pad, the scenario, flag, music and draw bytes, the primitives, the packet "
              "cursor and the stand-ins' log compared",
              kRounds, per[0], calls, bad);
    if (bad) {
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      title_states: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    }
    const Coverage& c = g_cover;
    bof3::Log("shadow      title_states coverage: state advanced 0..7: %u %u %u %u %u %u %u %u; demo over %u, ended by "
              "Start %u; badge off %u, drawn %u; blinking %u; music faded %u; load waits %u",
              c.state_advanced[0], c.state_advanced[1], c.state_advanced[2], c.state_advanced[3], c.state_advanced[4],
              c.state_advanced[5], c.state_advanced[6], c.state_advanced[7], c.demo_over, c.demo_start, c.badge_off,
              c.badge_drawn, c.blink, c.music_stopped, c.loops);
    if (bad) bof3::Fatal("the title's state handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void TitleStates_Inject() {
    if (bof3::WantsShadow("title_states")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[16];
            if (c.n_calls > 16) bof3::Fatal("title_states: %s has %d calls", c.name, c.n_calls);
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        SelfTest(clones);
    }
    title_states::g_psp_title = psp_code::PspBase();
    if (title_states::g_psp_title) bof3::Log("DIV-0131    the title's logo: the PSP base's own pieces (D256)");
    BOF3_INJECT(Scenario_Start);
    BOF3_INJECT(Scenario_Load);
    BOF3_INJECT(Scenario_SubInit);
}
