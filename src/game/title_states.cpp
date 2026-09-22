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
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// Bytes the handlers touch that have no name (docs/title-states.md has the
// PSX twin of each).
constexpr std::uint32_t kPlayClock = 0x9040C8;      // four bytes zeroed starting the demo (PSX 0x80144FBC)
constexpr std::uint32_t kPartyCombo = 0x90412C;     // 0xFF when the demo ends (PSX 0x80145020)
constexpr std::uint32_t kMusicTrack = 0x904131;     // the track Music_Play last started, 0xFF none
constexpr std::uint32_t kScenarioBytes = 0x8034E0;  // Cond_ByteFA and the nine bytes after it (PSX 0x8014686C)
constexpr std::uint32_t kScenarioFlags = 0x929ED0;  // the address of the chapter's Cond_Flags dword (PSX 0x80146868)
constexpr std::uint32_t kSubState = 0x9039F0;       // the dispatcher 0x56D8B0's bytes (PSX 0x801448E4)
constexpr std::uint32_t kSubInitTable = 0x662CD8;   // 16 bytes Scenario_SubInit copies (PSX 0x801C94A4)
constexpr std::uint32_t kStoryFlags = 0x904030;     // Cond_Flags + 0xA0, where they go (PSX 0x80144F24)
constexpr std::uint32_t kDemoEntry = 0x495800;      // task 0's entry for the demo (the field task)
constexpr std::uint32_t kMenuEntry = 0x588E70;      // task 0's entry for the title menu
constexpr std::uint32_t kModeDispatch = 0x56D690;   // Field_ModeDispatch, another file's: called by address
const std::uint32_t kCondFlags = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Cond_Flags));

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them. Five are ours (Title_FadeMusic,
// Title_StopMusic, Title_DrawBadge, Scenario_Start, Scenario_Load), called
// through here all the same so that each function is tested alone.
// Field_ModeDispatch belongs to another file and is called through its
// address.
struct Callees {
    void (__cdecl* dropped)(unsigned char);
    void (__cdecl* load_dat)(int);
    int (__cdecl* load_done)();
    void (__cdecl* sleep)(int);
    void (__cdecl* clut_restore)();
    void (__cdecl* music_play)(unsigned int, int);
    void (__cdecl* transition)(unsigned char);
    void (__cdecl* fade_music)();
    void (__cdecl* stop_music)();
    void (__cdecl* effect_clear)();
    void (__cdecl* scenario_start)(int);
    void (__cdecl* task_create)(int, void*);
    void (__cdecl* music_fade_out)(int);
    void (__cdecl* music_fade_stop)(int);
    void (__cdecl* badge)();
    void (__cdecl* task_exit)();
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned char* (__cdecl* sprite)(int, int, unsigned, unsigned, unsigned);
    void (__cdecl* shade)(unsigned char*, unsigned char);
    void (__cdecl* scenario_load)();
    void (__cdecl* loading_frame)();
    void (__cdecl* mode_dispatch)();
};
const Callees kOriginals = {
    Port_DroppedCall, LoadDatFile, File_LoadDone, Task_Sleep, Gfx_ClutStripRestore, Music_Play, Transition_Start,
    Title_FadeMusic, Title_StopMusic, Effect_ClearAll, Scenario_Start, Task_Create, Music_FadeOut, Music_FadeOutStop,
    Title_DrawBadge, Task_Exit, Gpu_SetDrawMode, Gfx_CommitPrim, Title_Sprite, Prim_SetShade, Scenario_Load,
    Field_LoadingFrame, reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kModeDispatch)),
};
Callees g = kOriginals;

// Task 0's state word, +0 of its record: 0 while no task 0 runs.
std::uint16_t TaskZeroState() { return Word(At(bof3::addr::Task_Records)); }

// The 16-bit words the handlers count with, as the original's `inc word` /
// `dec word` in memory: wrapping at 0x10000.
void BumpState() { Title_State = static_cast<std::uint16_t>(Title_State + 1); }

}  // namespace

// original 0x462200: Title_States entry 0 (PSX 0x801D0C90). Loads the title's
// DAT, then starts the backdrop's fade-in and the title music.
//
// As the original has it: Port_DroppedCall(2) first (the PSX's call there,
// emptied by the port); the wait on File_LoadDone sleeps a frame at a time,
// asked first before any sleep (on the PC it is always done); the CLUT strip
// restored and Gfx_ClutStripDirty counted up - not set to 1 - after the
// load; the state word read after the calls.
extern "C" void __cdecl Title_StateLoad(void) {
    g.dropped(2);
    g.load_dat(0x136);
    while (g.load_done() == 0) g.sleep(1);
    g.clut_restore();
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    g.music_play(0x8D, 8);
    BumpState();
    Title_Fade = 1;
    Title_Scroll = 0xC8;
    Title_Bright = 0;
    Title_LogoState = 0;
    Title_LogoShadeA = 0;
    Title_LogoShadeB = 0;
    Title_StartArmed = 1;
}

// original 0x462290: entry 1 (PSX 0x801D0D5C). Waits for the backdrop's
// fade-in to finish (Title_DrawBackdrop sets fade 2), then holds 360 frames.
extern "C" void __cdecl Title_StateFadeIn(void) {
    if (Title_Fade != 2) return;
    BumpState();
    Title_Timer = 0x168;
}

// original 0x4622B0: entry 2 (PSX 0x801D0D94). Counts the hold down; at 0 the
// backdrop fades out, the logo in, and the logo holds 900 frames.
//
// As the original has it: the timer is decremented as a u16 before the test,
// so a timer of 0 wraps to 0xFFFF and holds 65,535 frames more
// (Title_CheckStart sets it to 1 to cut the hold short, never 0).
extern "C" void __cdecl Title_StateBackdrop(void) {
    Title_Timer = static_cast<std::uint16_t>(Title_Timer - 1);
    if (Title_Timer != 0) return;
    BumpState();
    Title_Fade = 3;
    Title_LogoState = 1;
    Title_Timer = 0x384;
}

// original 0x4622E0: entry 3 (PSX 0x801D0DF0). Counts the logo's hold down; at
// 0 starts the screen transition and fades the music.
//
// As the original has it: the state word is read after the two calls.
extern "C" void __cdecl Title_StateLogo(void) {
    Title_Timer = static_cast<std::uint16_t>(Title_Timer - 1);
    if (Title_Timer != 0) return;
    g.transition(0);
    g.fade_music();
    BumpState();
}

// original 0x462300: entry 4 (PSX 0x801D0E54). Once no transition holds the
// wait word, starts the demo: the title's fades off, the music stopped, the
// effect objects cleared, the play clock and the draw-pass flags zeroed,
// scenario chapter 0x10 started and task 0 created on the field task.
//
// As the original has it: Port_DroppedCall(0)'s pushed argument stays on the
// stack under Scenario_Start's until one `add esp, 0x10` - no callee reads
// past its own. Scenario_Start runs on task 1's stack and may sleep there
// while its DAT loads (on the PC it never does).
extern "C" void __cdecl Title_StateStartDemo(void) {
    if (MoveScript_WaitWordDA != 0) return;
    Title_Fade = 0;
    Title_LogoState = 0;
    Title_StartArmed = 0;
    g.stop_music();
    g.dropped(0);
    g.effect_clear();
    At(kPlayClock)[3] = 0;
    At(kPlayClock)[2] = 0;
    At(kPlayClock)[1] = 0;
    At(kPlayClock)[0] = 0;
    Draw_PassFlags = 0;
    g.scenario_start(0x10);
    g.task_create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(kDemoEntry)));
    BumpState();
}

// original 0x462370: entry 5 (PSX 0x801D0F00), every frame of the demo. When
// task 0 has ended, back to the title (state 0). Start pressed during it (with
// the load done and no wait) ends the demo: MoveScript_Var7 1, transition 4,
// the music faded out. Then the corner badge (a tail jump in the original).
//
// As the original has it: the state set to 0 returns without the badge; the
// pad is read as the dword at Input_Pressed and only bit 0x800 tested.
extern "C" void __cdecl Title_StateDemo(void) {
    if (TaskZeroState() == 0) {
        Title_State = 0;
        return;
    }
    if (g.load_done() != 0 && MoveScript_WaitWordDA == 0 && (Input_Pressed & 0x800) != 0) {
        MoveScript_Var7 = 1;
        g.transition(4);
        g.music_fade_stop(8);
        BumpState();
    }
    g.badge();
}

// original 0x4623D0: entry 6 (PSX 0x801D0FB8), after Start ended the demo:
// when task 0 is gone, the party combination byte 0xFF and back to state 0;
// until then the badge (a tail jump).
extern "C" void __cdecl Title_StateDemoEnd(void) {
    if (TaskZeroState() == 0) {
        At(kPartyCombo)[0] = 0xFF;
        Title_State = 0;
        return;
    }
    g.badge();
}

// original 0x4623F0: entry 7 (PSX 0x801D1000), after Start on the logo: once
// no transition holds the wait word, stops the music, creates task 0 on the
// title menu 0x588E70 and ends task 1.
//
// As the original has it: Task_Exit is a tail jump that never returns - it
// clears this task's record and enters the scheduler on the scheduler's own
// stack, abandoning task 1's (this frame included). A stand-in that returns
// makes this return, as the original's jump would to its caller.
extern "C" void __cdecl Title_StateMenu(void) {
    if (MoveScript_WaitWordDA != 0) return;
    g.stop_music();
    g.dropped(0);
    g.task_create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(kMenuEntry)));
    g.task_exit();
}

// original 0x4624B0: fade the music out over 0x10 frames, unless none plays
// (PSX 0x801D1134, which fades through a per-track table).
extern "C" void __cdecl Title_FadeMusic(void) {
    if (At(kMusicTrack)[0] != 0xFF) g.music_fade_out(0x10);
}

// original 0x4624D0: fade out and stop over 0xA frames, and forget the track,
// unless none plays (PSX 0x801D1184).
//
// As the original has it: 0xFF is stored after the call, whatever the call
// left there.
extern "C" void __cdecl Title_StopMusic(void) {
    if (At(kMusicTrack)[0] == 0xFF) return;
    g.music_fade_stop(0xA);
    At(kMusicTrack)[0] = 0xFF;
}

// original 0x4624F0: the demo's corner badge (PSX 0x801D11E4) - sprite 0xA at
// (0xC0, 4) while Title_BadgeOn and bit-anything Draw_PassFlags are set. The
// badge goes off for good once the field runs its area change (Field_Request
// 5 in Game_Mode 2) with no transition waiting.
//
// As the original has it: the three tests in that order, Field_Request and
// Title_BadgeOn as bytes, Game_Mode and the wait word as words; the packet
// cursor read for the draw mode at the call. The PSX asks a video-mode
// function for the tpage; the port always takes 0x9D.
extern "C" void __cdecl Title_DrawBadge(void) {
    if (Field_Request == 5 && Game_Mode == 2 && MoveScript_WaitWordDA == 0) Title_BadgeOn = 0;
    if (Draw_PassFlags == 0 || Title_BadgeOn == 0) return;
    g.draw_mode(Gfx_PacketNext, 0, 0, 0x9D, 0);
    g.commit(2, 0xC);
    g.sprite(0xC0, 4, 0xA, 2, 0);
}

// original 0x462820: the logo's first set (PSX 0x801D12CC): sprite 1 through
// OT slot 1, sprites 8 and 9 through slot 2, each semi-transparent by `flag`
// and shaded `shade`; then, once the backdrop is gone and the logo shown,
// sprite 7 blinking - the "press Start" prompt, by where it appears
// (hypothesis) - on Title_BlinkCount, a 64-frame triangle from 0 to 0x7C and
// back from 0x80 to 4.
//
// As the original has it: flag and shade are dwords whose low byte alone is
// read (Title_Sprite masks the one, Prim_SetShade stores the other as a
// byte; ours are masked here because a Capcom caller leaves the upper bytes
// stale); the packet cursor is read again for the second draw mode, after
// the first sprite's commit moved it; Title_Fade and Title_LogoState are read
// after the three sprites; the counter is incremented as a word in memory and
// its low byte read back; the blinking sprite is opaque and not shaded
// through Prim_SetShade - its three colour bytes are written here.
extern "C" void __cdecl Title_DrawSetA(unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    g.draw_mode(Gfx_PacketNext, 0, 0, 0x2F, 0);
    g.commit(1, 0xC);
    g.shade(g.sprite(0x106, 0x82, 1, 1, semi), level);
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xBD, 0);
    g.commit(2, 0xC);
    g.shade(g.sprite(0x14, 0xD0, 8, 2, semi), level);
    g.shade(g.sprite(0xA4, 0xD0, 9, 2, semi), level);
    if (Title_Fade != 0 || Title_LogoState != 2) {
        Title_BlinkCount = 0;
        return;
    }
    unsigned char* const prim = g.sprite(0x30, 0xB8, 7, 2, 0);
    Title_BlinkCount = static_cast<std::uint16_t>(Title_BlinkCount + 1);
    const unsigned count = Title_BlinkCount & 0xFFu;
    const unsigned step = (count & 0x1Fu) << 2;
    const auto blink = static_cast<unsigned char>((count & 0x20u) ? 0x80u - step : step);
    prim[4] = blink;
    prim[5] = blink;
    prim[6] = blink;
}

// original 0x462930: the logo's third set (PSX 0x801D1494): while `flag`
// (the logo still fading), sprites 0xF and 0x10 semi-transparent at x and
// x + 0xE0 under tpage 0xD9; always sprites 4 and 5 there under tpage 0xB9,
// semi-transparent by `flag`; all four shaded `shade`.
//
// As the original has it: the flag's low byte decides the first pair (`test
// bl, bl`); x + 0xE0 is 32-bit arithmetic, taken as s16 by Title_Sprite; the
// packet cursor read at each draw mode.
extern "C" void __cdecl Title_DrawSetC(int x, int y, unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    const int right = static_cast<int>(static_cast<unsigned>(x) + 0xE0u);
    if (semi != 0) {
        g.draw_mode(Gfx_PacketNext, 0, 0, 0xD9, 0);
        g.commit(2, 0xC);
        g.shade(g.sprite(x, y, 0xF, 2, 1), level);
        g.shade(g.sprite(right, y, 0x10, 2, 1), level);
    }
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xB9, 0);
    g.commit(2, 0xC);
    g.shade(g.sprite(x, y, 4, 2, semi), level);
    g.shade(g.sprite(right, y, 5, 2, semi), level);
}

// original 0x462A00: the logo's second set (PSX 0x801D1664): sprites 2 at
// (x, y) and 3 at (x + 0xF0, y + 0x70) under tpage 0xBB, semi-transparent by
// `flag`, shaded `shade`.
extern "C" void __cdecl Title_DrawSetB(int x, int y, unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xBB, 0);
    g.commit(2, 0xC);
    g.shade(g.sprite(x, y, 2, 2, semi), level);
    g.shade(g.sprite(static_cast<int>(static_cast<unsigned>(x) + 0xF0u), static_cast<int>(static_cast<unsigned>(y) + 0x70u),
                     3, 2, semi),
            level);
}

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
// (MoveScript_Var7 is 0x8034E4, Field_EdgeBitsPrev 0x8034E8). File_LoadDone
// is asked before any frame; on the PC it is always done, so the loop body
// never runs.
extern "C" void __cdecl Scenario_Start(int chapter) {
    const auto c = static_cast<signed char>(chapter);
    unsigned char* const s = At(kScenarioBytes);
    s[0] = static_cast<unsigned char>(c);
    s[2] = 0;
    s[3] = 0;
    s[4] = 0;
    SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    SetWord(s + 8, 0);
    const std::uint32_t flags = kCondFlags + static_cast<std::uint32_t>(8 * static_cast<int>(c));
    SetLong(At(flags), 0);
    SetLong(At(kScenarioFlags), static_cast<std::int32_t>(flags));
    g.scenario_load();
    while (g.load_done() == 0) {
        if (Game_AreaNumber != 0xFFFF && Field_Request != 5) g.loading_frame();
        g.sleep(1);
    }
    g.mode_dispatch();
}

// original 0x56D670: the current chapter's DAT (PSX 0x801A880C, which loads
// 0x295 + chapter, SCENA<chapter>.EMI). As the original has it: the chapter
// byte sign-extended.
extern "C" void __cdecl Scenario_Load(void) {
    g.load_dat(static_cast<int>(Cond_ByteFA) + 0x306);
}

// original 0x56D8C0: state 0 of the dispatcher 0x56D8B0 (PSX 0x801A8C34):
// its state byte to 1 so the next frame runs entry 1 (0x56D920), the six
// bytes and two words around it zeroed, and 16 bytes of the story flags
// (Cond_Flags + 0xA0) seeded from the table at 0x662CD8.
//
// As the original has it: four dword copies (the PSX copies bytewise); the
// table and the flags do not overlap, so the order is unobservable.
extern "C" void __cdecl Scenario_SubInit(void) {
    unsigned char* const s = At(kSubState);
    s[2] = 1;
    s[0] = 0;
    s[3] = 0;
    s[4] = 0;
    SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    SetWord(s + 8, 0);
    for (unsigned i = 0; i < 16; i += 4) SetLong(At(kStoryFlags + i), Long(At(kSubInitTable + i)));
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
    {0x66C808, 0, 7},     // Title_State, low byte
    {0x66C809, 0, 1},     // Title_State, high byte
    {0x66C7F9, 0, 3},     // Title_Fade
    {0x66C7FA, 0, 3},     // Title_LogoState
    {0x66C7FB, 0, 1},     // Title_StartArmed
    {0x66C7F8, 0, 1},     // Title_BadgeOn
    {0x66C806, 1, 0},     // Title_BlinkCount
    {0x66C807, 0, 1},
    {kMusicTrack, 0xFE, 0xFF},
    {0x937F90, 1, 0},     // Gfx_ClutStripDirty
    {kPlayClock, 1, 0},
    {kPlayClock + 3, 1, 0},
    {0x7E0918, 1, 0},     // Draw_PassFlags
    {kScenarioBytes + 4, 1, 0},   // MoveScript_Var7
    {kPartyCombo, 1, 0},
    {0x66C7D8, 4, 6},     // Field_Request
    {0x904EFC, 0xFE, 0xFF},  // Game_AreaNumber, low byte
    {0x904EFD, 0xFE, 0xFF},
    {0x66C7D0, 0, 1},     // task 0's state
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
    At(kMusicTrack)[0] = static_cast<unsigned char>(track);
    Disturb();
}
void __cdecl StubTransition(unsigned kind) { Record(7, kind & 0xFF); Disturb(); }
void __cdecl StubFadeMusic() { Record(8); Disturb(); }
void __cdecl StubStopMusic() { Record(9); Disturb(); }
void __cdecl StubEffectClear() { Record(10); Disturb(); }
void __cdecl StubScenarioStart(int chapter) { Record(11, static_cast<std::uint32_t>(chapter) & 0xFF); Disturb(); }
void __cdecl StubTaskCreate(int slot, void* entry) {
    Record(12, static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entry)));
    Disturb();
}
// The fades may end the track or leave it: what Title_StopMusic stores over.
void __cdecl StubMusicFadeOut(int frames) {
    Record(13, static_cast<std::uint32_t>(frames));
    At(kMusicTrack)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
void __cdecl StubMusicFadeStop(int frames) {
    Record(14, static_cast<std::uint32_t>(frames));
    At(kMusicTrack)[0] = static_cast<unsigned char>(Hash() >> 8);
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
void __cdecl StubModeDispatch() { Record(26, At(kScenarioBytes)[0]); Disturb(); }

// The stand-ins take bytes as dwords and mask them: the copies push whole
// registers.
template <typename T> T As(const void* p) { return reinterpret_cast<T>(const_cast<void*>(p)); }
const Callees kStubs = {
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
    {bof3::addr::Task_Records, 0x48},   // task 0's state, Field_Request, Game_Mode, task 1's words,
                                        // MoveScript_WaitWordDA
    {0x7E1BEC, 4},                      // Input_Pressed, read as a dword
    {0x7E0918, 1},                      // Draw_PassFlags
    {0x937F90, 1},                      // Gfx_ClutStripDirty
    {kScenarioBytes, 0x10},             // Cond_ByteFA .. Field_EdgeBitsPrev
    {kScenarioFlags, 4},
    {kSubState, 0x10},
    {0x903F90 - 0x400, 0x808},          // Cond_Flags - 0x400: every chapter's dword, the story flags,
                                        // the play clock, the party combination and the music track
    {0x904EFC, 2},                      // Game_AreaNumber
    {kSubInitTable, 0x10},
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

void SetTaskZero(bool running) { SetWord(At(bof3::addr::Task_Records), running ? 1 + Next() % 2 : 0); }

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
        if (Often()) MoveScript_WaitWordDA = 0;
        else if (Often()) MoveScript_WaitWordDA = static_cast<std::uint16_t>(Next() % 2 ? 0x100 : 1);
        break;
    case kStateDemo: {
        SetTaskZero(Next() % 4 != 0);
        if (Often()) MoveScript_WaitWordDA = 0;
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
        if (Often()) At(kMusicTrack)[0] = kTrack[Next() % 5];
        break;
    }
    case kBadge: {
        static const unsigned char kRequest[] = {5, 5, 4, 6, 0x85};
        static const std::uint16_t kMode[] = {2, 2, 1, 3, 0x102};
        if (Often()) Field_Request = kRequest[Next() % 5];
        if (Often()) Game_Mode = Pick(kMode, 5);
        if (Often()) MoveScript_WaitWordDA = 0;
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
        if (Often()) At(kScenarioBytes)[0] = kChapter[Next() % 5];
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
unsigned Byte(const State& s, std::uint32_t address) { return s.memory[address - bof3::addr::Task_Records]; }
unsigned Half(const State& s, std::uint32_t address) { return Byte(s, address) | Byte(s, address + 1) << 8; }
bool Logged(const State& s, std::uint32_t what) {
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i) if (s.log[i].what == what) return true;
    return false;
}
void Cover(unsigned k, const State& in, const State& out) {
    constexpr std::uint32_t aState = 0x66C808, aBadge = 0x66C7F8, aBlink = 0x66C806;
    // (State 7 does not move the word: it ends the task - counted by the call.)
    if (k < kStateMenu && Half(out, aState) == ((Half(in, aState) + 1) & 0xFFFF)) ++g_cover.state_advanced[k];
    if (k == kStateMenu && Logged(out, 16)) ++g_cover.state_advanced[k];
    if ((k == kStateDemo || k == kStateDemoEnd) && Half(in, bof3::addr::Task_Records) == 0) ++g_cover.demo_over;
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
    g = kStubs;

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
    g = kOriginals;
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
    BOF3_INJECT(Title_StateLoad);
    BOF3_INJECT(Title_StateFadeIn);
    BOF3_INJECT(Title_StateBackdrop);
    BOF3_INJECT(Title_StateLogo);
    BOF3_INJECT(Title_StateStartDemo);
    BOF3_INJECT(Title_StateDemo);
    BOF3_INJECT(Title_StateDemoEnd);
    BOF3_INJECT(Title_StateMenu);
    BOF3_INJECT(Title_FadeMusic);
    BOF3_INJECT(Title_StopMusic);
    BOF3_INJECT(Title_DrawBadge);
    BOF3_INJECT(Title_DrawSetA);
    BOF3_INJECT(Title_DrawSetC);
    BOF3_INJECT(Title_DrawSetB);
    BOF3_INJECT(Scenario_Start);
    BOF3_INJECT(Scenario_Load);
    BOF3_INJECT(Scenario_SubInit);
}
