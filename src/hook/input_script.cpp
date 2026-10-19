#include "hook/input_script.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/crt_rest.h"
#include "game/game_clock.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "hook/gapwatch.h"
#include "hook/log.h"
#include "hook/statehash.h"
#include "hook/wav_dump.h"
#include "input/sigma.h"
#include "render/render_d3d11.h"

// The recipe language (docs/input-script.md has it with examples). One step a
// line, '#' to end of line is a comment, buttons joined with '+':
//
//   set hold N              frames a press holds the buttons, N > 0 (default 4)
//   set gap N               frames released after each press     (default 8)
//   wait N                  N frames, nothing held
//   press BUTTONS [xK]      K times (default 1): hold, then release
//   hold BUTTONS N          N frames held, no release after
//   until ADDR TYPE OP VALUE [timeout N]
//                           nothing held until the condition is true, checked
//                           once a frame; TYPE u8|u16|u32, OP == != & !&
//                           (& = any of VALUE's bits set). A timeout (default
//                           3600 frames) FAILS the recipe.
//   seek BUTTONS ADDR TYPE OP VALUE [max K]
//                           press BUTTONS (hold, then release) until the
//                           condition is true, checking before each press; K
//                           presses (default 16) without it FAILS the recipe.
//   shot NAME [N [BUTTONS]] hold BUTTONS (default nothing) for N frames
//                           (default 30) - the battle's command cross shows a
//                           command only while its direction is held - for
//                           tools/input_run.py to capture the window. With
//                           BOF3X_SHOT_WAIT set (input_run.py sets it) the
//                           game then FREEZES - clock and all - until the
//                           driver has grabbed the frame: frame-exact. Without
//                           it the shot is logged as the hold starts and the
//                           grab lands wherever the game has got to.
//   peek ADDR TYPE [LABEL]  log the value
//   poke ADDR TYPE VALUE    write the value - to put the game in a state no
//                           input reaches (a corrupted save's byte, say). An
//                           experiment's tool: a recipe with a poke is not a
//                           player's route, and says so in its log.
//   mark TEXT               log the text
//   hold KEY N              a keyboard key (f1..f12, escape, enter, space, or a
//                           virtual-key number) held N frames: WM_KEYDOWN posted
//                           to the game window as the step starts, WM_KEYUP as
//                           it ends - a player's press as Game_WndProc sees it.
//                           `key KEY` is the same held one frame. The recorder
//                           (BOF3X_RECORD) writes pad words only, so a played
//                           route's F9 presses are added by hand: the owner's
//                           `hold f9 4` / `wait 42`.
//                           While the game is paused (F9) WinMain skips the
//                           latch; the recipe still advances a step per frame
//                           through InputScript_PausedLatch, its pad words
//                           dropped, so `key f9` / `wait N` / `key f9` is a
//                           pause of N frames.
//   quit [timeout N]        the game is expected to exit now (F9 twice on the
//                           title, say): nothing held; the process ending with
//                           this step current logs `done` from DllMain's
//                           detach, and N frames (default 600) without it
//                           FAILS the recipe. An exit with any other step
//                           current logs FAILED.
//   end                     stop here
//
// Buttons are the PlayStation pad's, the bit layout Pad_Read builds
// (symbols.toml Input_Held): up down left right cross circle square triangle
// l1 l2 r1 r2 start select. Or @ADDR: whatever u16 is at ADDR when the step
// starts - for the field's button assignments, which a save carries
// (symbols.toml Field_MenuButton): `press @0x903584` opens the menu whichever
// shape the loaded save gave it. A literal button is the PC's - Japan's
// Config, where the recipes are recorded - and on a Western or PSP-EU install
// it is pressed as that release's button while the release's starting words
// stand, until a save's words are loaded (sigma::RecipeToPad; the recorder
// writes the inverse). docs/input-script.md section 2a.
//
// When the recipe ends - end, last line, or a failed until - the pad is handed
// back: Input_Latch's own words stand from the next frame on.

namespace bof3 {

void RandCountFrame();   // below, outside the anonymous namespace
namespace {

enum class Kind { Wait, Press, Hold, Until, Seek, Shot, Peek, Poke, Music, Mark, Key, Quit, End };
enum class Op { Eq, Ne, Any, None };

struct Step {
    Kind kind;
    int line;
    unsigned short buttons = 0;
    std::uint32_t buttons_at = 0;   // @ADDR: read the buttons there instead
    unsigned n = 0;        // Wait/Hold frames, Press count, Until timeout, Shot frames
    unsigned hold = 0, gap = 0;
    std::uint32_t addr = 0;
    unsigned width = 0;    // 1, 2, 4
    Op op = Op::Eq;
    std::uint32_t value = 0;
    std::string text;
};

std::vector<Step> g_steps;
std::size_t g_index = 0;
bool g_quit_expected = false;      // a `quit` step is current: the process ending is the recipe's `done`
unsigned g_t = 0;                  // frames spent in the current step
bool g_active = false;
bool g_scripted = false;   // a recipe was loaded: the latch is ScriptedLatch until the process ends
void DeviceLatch();
bool g_seen_frame = false;
std::uint32_t g_last_frame = 0;    // Frame_Counter at the last new frame
unsigned g_frame = 0;              // frames the recipe has played
unsigned short g_prev = 0, g_cur = 0;
HANDLE g_release = nullptr;         // BOF3X_SHOT_WAIT: set by the driver after each grab
constexpr DWORD kFreezeMs = 3000;   // Windows ghosts a window that pumps nothing for 5 s

struct Button { const char* name; unsigned short bit; };
constexpr Button kButtons[] = {
    {"l2", 0x0001},     {"r2", 0x0002},     {"l1", 0x0004},       {"r1", 0x0008},
    {"triangle", 0x0010}, {"circle", 0x0020}, {"cross", 0x0040},  {"square", 0x0080},
    {"select", 0x0100}, {"start", 0x0800},  {"up", 0x1000},       {"right", 0x2000},
    {"down", 0x4000},   {"left", 0x8000},
};

[[noreturn]] void ParseError(int line, const char* what, const std::string& tok) {
    Fatal("BOF3X_INPUT line %d: %s '%s'", line, what, tok.c_str());
}

std::vector<std::string> Split(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        std::size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

unsigned Number(int line, const std::string& tok) {
    char* end = nullptr;
    unsigned long v = std::strtoul(tok.c_str(), &end, 0);
    if (tok.empty() || *end) ParseError(line, "not a number", tok);
    return static_cast<unsigned>(v);
}

struct Key { const char* name; unsigned vk; };
constexpr Key kKeys[] = {
    {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3}, {"f4", VK_F4}, {"f5", VK_F5}, {"f6", VK_F6},
    {"f7", VK_F7}, {"f8", VK_F8}, {"f9", VK_F9}, {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
    {"escape", VK_ESCAPE}, {"enter", VK_RETURN}, {"space", VK_SPACE},
};

bool IsKeyName(const std::string& tok) {
    for (const Key& k : kKeys)
        if (tok == k.name) return true;
    return false;
}

unsigned VirtualKey(int line, const std::string& tok) {
    for (const Key& k : kKeys)
        if (tok == k.name) return k.vk;
    if (!tok.empty() && (tok[0] >= '0' && tok[0] <= '9')) {
        const unsigned v = Number(line, tok);
        if (v >= 1 && v <= 0xFE) return v;
    }
    ParseError(line, "unknown key", tok);
}

unsigned short Buttons(int line, const std::string& tok) {
    unsigned short bits = 0;
    std::size_t i = 0;
    while (i <= tok.size()) {
        std::size_t j = tok.find('+', i);
        if (j == std::string::npos) j = tok.size();
        const std::string name = tok.substr(i, j - i);
        bool found = false;
        for (const Button& b : kButtons)
            if (name == b.name) { bits |= b.bit; found = true; }
        if (!found) ParseError(line, "unknown button", name);
        i = j + 1;
    }
    return bits;
}

unsigned Width(int line, const std::string& tok) {
    if (tok == "u8") return 1;
    if (tok == "u16") return 2;
    if (tok == "u32") return 4;
    ParseError(line, "type is u8, u16 or u32, not", tok);
}

// Every address a recipe reads is checked once, here: committed and readable
// for the whole width. The exe's image and data never move or unmap, so a
// check at start-up holds for the run.
std::uint32_t Address(int line, const std::string& tok, unsigned width) {
    const std::uint32_t a = Number(line, tok);
    MEMORY_BASIC_INFORMATION mi{};
    const void* p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a));
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT ||
        (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
        a + width > reinterpret_cast<std::uintptr_t>(mi.BaseAddress) + mi.RegionSize)
        ParseError(line, "address not readable", tok);
    return a;
}

std::uint32_t Read(std::uint32_t addr, unsigned width) {
    const void* p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr));
    if (width == 1) return *static_cast<const volatile std::uint8_t*>(p);
    if (width == 2) return *static_cast<const volatile std::uint16_t*>(p);
    return *static_cast<const volatile std::uint32_t*>(p);
}

void SetButtons(Step& st, const std::string& tok) {
    if (!tok.empty() && tok[0] == '@') st.buttons_at = Address(st.line, tok.substr(1), 2);
    else st.buttons = Buttons(st.line, tok);
}

// ADDR TYPE OP VALUE, starting at t[i].
void Condition(Step& st, const std::vector<std::string>& t, std::size_t i) {
    st.width = Width(st.line, t[i + 1]);
    st.addr = Address(st.line, t[i], st.width);
    if (t[i + 2] == "==") st.op = Op::Eq;
    else if (t[i + 2] == "!=") st.op = Op::Ne;
    else if (t[i + 2] == "&") st.op = Op::Any;
    else if (t[i + 2] == "!&") st.op = Op::None;
    else ParseError(st.line, "operator is == != & or !&, not", t[i + 2]);
    st.value = Number(st.line, t[i + 3]);
}

void Load(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) Fatal("BOF3X_INPUT: cannot open %s", path);
    unsigned hold = 4, gap = 8;
    char buf[512];
    int line = 0;
    while (std::fgets(buf, sizeof buf, f)) {
        ++line;
        std::string s(buf);
        if (std::size_t h = s.find('#'); h != std::string::npos) s.resize(h);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        std::vector<std::string> t = Split(s);
        if (t.empty()) continue;
        const std::string& w = t[0];
        Step st{};
        st.line = line;
        auto need = [&](std::size_t lo, std::size_t hi) {
            if (t.size() < lo || t.size() > hi) ParseError(line, "wrong number of words for", w);
        };
        if (w == "set") {
            need(3, 3);
            const unsigned v = Number(line, t[2]);
            if (t[1] == "hold" && v > 0) hold = v;
            else if (t[1] == "gap") gap = v;
            else ParseError(line, "set takes hold N (N > 0) or gap N, not", t[1]);
            continue;
        } else if (w == "wait") {
            need(2, 2);
            st.kind = Kind::Wait;
            st.n = Number(line, t[1]);
        } else if (w == "press") {
            need(2, 3);
            st.kind = Kind::Press;
            SetButtons(st, t[1]);
            st.n = 1;
            if (t.size() == 3) {
                if (t[2].size() < 2 || t[2][0] != 'x') ParseError(line, "repeat count is xN, not", t[2]);
                st.n = Number(line, t[2].substr(1));
            }
            st.hold = hold;
            st.gap = gap;
        } else if (w == "hold") {
            need(3, 3);
            st.n = Number(line, t[2]);
            if (IsKeyName(t[1])) {
                st.kind = Kind::Key;
                st.value = VirtualKey(line, t[1]);
                st.text = t[1];
                if (st.n == 0) ParseError(line, "a key is held one frame or more, not", t[2]);
            } else {
                st.kind = Kind::Hold;
                SetButtons(st, t[1]);
            }
        } else if (w == "until") {
            if (t.size() != 5 && t.size() != 7) ParseError(line, "until ADDR TYPE OP VALUE [timeout N], got", s);
            st.kind = Kind::Until;
            Condition(st, t, 1);
            st.n = 3600;
            if (t.size() == 7) {
                if (t[5] != "timeout") ParseError(line, "expected timeout, got", t[5]);
                st.n = Number(line, t[6]);
            }
        } else if (w == "seek") {
            if (t.size() != 6 && t.size() != 8) ParseError(line, "seek BUTTONS ADDR TYPE OP VALUE [max K], got", s);
            st.kind = Kind::Seek;
            SetButtons(st, t[1]);
            Condition(st, t, 2);
            st.n = 16;
            if (t.size() == 8) {
                if (t[6] != "max") ParseError(line, "expected max, got", t[6]);
                st.n = Number(line, t[7]);
            }
            st.hold = hold;
            st.gap = gap;
        } else if (w == "shot") {
            need(2, 4);
            st.kind = Kind::Shot;
            st.text = t[1];
            st.n = t.size() >= 3 ? Number(line, t[2]) : 30;
            if (t.size() == 4) SetButtons(st, t[3]);
        } else if (w == "peek") {
            need(3, 4);
            st.kind = Kind::Peek;
            st.width = Width(line, t[2]);
            st.addr = Address(line, t[1], st.width);
            st.text = t.size() == 4 ? t[3] : t[1];
        } else if (w == "poke") {
            need(4, 4);
            st.kind = Kind::Poke;
            st.width = Width(line, t[2]);
            st.addr = Address(line, t[1], st.width);
            st.value = Number(line, t[3]);
            MEMORY_BASIC_INFORMATION mi{};
            VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(st.addr)), &mi, sizeof mi);
            if (!(mi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
                ParseError(line, "address not writable", t[1]);
        } else if (w == "music") {
            // music TRACK [FRAMES]: the game's Music_Play(TRACK, FRAMES) called at
            // this frame, as a script's or a menu's call would (docs/input-script.md
            // 3) - a chosen song heard through the engine's own path, for the WAV
            // dump's listen sets (docs/wav-dump.md). FRAMES defaults to 8, the
            // faerie village's music menu's (CommuMusic_Play 0x45F950).
            need(2, 3);
            st.kind = Kind::Music;
            st.value = Number(line, t[1]);
            if (st.value > 0xFF) ParseError(line, "a track is 0..255, not", t[1]);
            st.n = t.size() == 3 ? Number(line, t[2]) : 8;
        } else if (w == "mark") {
            st.kind = Kind::Mark;
            const std::size_t at = s.find_first_not_of(" \t", s.find("mark") + 4);
            if (at != std::string::npos) st.text = s.substr(at);
        } else if (w == "key") {
            need(2, 3);
            st.kind = Kind::Key;
            st.value = VirtualKey(line, t[1]);
            st.text = t[1];
            st.n = t.size() == 3 ? Number(line, t[2]) : 1;
            if (st.n == 0) ParseError(line, "a key is held one frame or more, not", t[2]);
        } else if (w == "quit") {
            if (t.size() != 1 && t.size() != 3) ParseError(line, "quit [timeout N], got", s);
            st.kind = Kind::Quit;
            st.n = 600;
            if (t.size() == 3) {
                if (t[1] != "timeout") ParseError(line, "expected timeout, got", t[1]);
                st.n = Number(line, t[2]);
            }
        } else if (w == "end") {
            need(1, 1);
            st.kind = Kind::End;
        } else {
            ParseError(line, "unknown step", w);
        }
        g_steps.push_back(st);
    }
    std::fclose(f);
}

bool Holds(const Step& s) {
    const std::uint32_t v = Read(s.addr, s.width);
    switch (s.op) {
    case Op::Eq: return v == s.value;
    case Op::Ne: return v != s.value;
    case Op::Any: return (v & s.value) != 0;
    case Op::None: return (v & s.value) == 0;
    }
    return false;
}

void Finish(const char* how) {
    g_active = false;
    Log("input       %s at recipe frame %u (Frame_Counter %u); the pad is the player's again", how, g_frame,
        g_last_frame);
    LogFlush();
}

// BOF3X_SHOT_DIR: the frame written by the game itself, <dir>\<NAME>.bmp
// (render::SaveFrame - the target, not the window, so nothing has to be on
// top). Logged as `saved` or `NOT saved` before the shot line.
std::wstring g_shot_dir;

void SaveShot(const Step& s) {
    if (g_shot_dir.empty()) return;
    std::wstring path = g_shot_dir + L"\\";
    for (char c : s.text) path += static_cast<wchar_t>(static_cast<unsigned char>(c));
    path += L".bmp";
    const bool ok = render::SaveFrame(path.c_str());
    Log("input       shot %s %s %s", s.text.c_str(), ok ? "saved to" : "NOT saved:", Utf8Path(path.c_str()).text);
}

// A frozen shot. Called from the latch at the top of WinMain's loop, where the
// last frame built has just been presented and the next has not begun: the
// window holds one whole frame, and holds it until the driver says it has it
// (or kFreezeMs passes). The game clock stops meanwhile, so the frame deadline
// has no debt to replay and the frames after the shot are presented as any
// others.
void Freeze(const Step& s) {
    const bool clock = GameClock_Pause();
    SaveShot(s);
    ResetEvent(g_release);
    Log("input       shot %s recipe frame %u frozen%s", s.text.c_str(), g_frame,
        clock ? "" : " (clock not ours: the pause will be replayed)");
    LogFlush();
    if (WaitForSingleObject(g_release, kFreezeMs) != WAIT_OBJECT_0)
        Log("input       shot %s: no release from the driver in %lu ms; going on", s.text.c_str(), kFreezeMs);
    GameClock_Resume();
}

// BOF3X_SHOT_EVERY=N (tools/abc_run.py, docs/abc-harness.md): besides the
// recipe's own `shot` steps, the frame presented before every recipe frame
// that is a multiple of N is written to BOF3X_SHOT_DIR as t<FRAME>.bmp, with
// the game's mode and area logged beside it. Nothing waits for a driver: the
// game writes the file itself, with the clock paused for the write so the
// frame deadline has no debt to replay, and InputScript_ShotPending has the
// passes before it drawn even when late (kShotLeadPasses) - so the picture
// is the same frame on every run, and the recipe's inputs and frames are untouched. Instrumentation
// like the `shot` step: off unless set, no ledger entry.
unsigned g_every = 0;

void SaveTimed() {
    if (g_shot_dir.empty() || g_every == 0 || g_frame % g_every != 0) return;
    char name[16];
    std::snprintf(name, sizeof name, "t%06u", g_frame);
    std::wstring path = g_shot_dir + L"\\";
    for (const char* c = name; *c; ++c) path += static_cast<wchar_t>(*c);
    path += L".bmp";
    const bool clock = GameClock_Pause();
    const bool ok = render::SaveFrame(path.c_str());
    if (clock) GameClock_Resume();
    Log("input       frame %s %s mode %u area %u (Frame_Counter %u)", name, ok ? "saved" : "NOT saved",
        (unsigned)Game_Mode, (unsigned)Game_AreaNumber, g_last_frame);
}

unsigned short g_step_buttons = 0;   // the current Press/Hold step's, read as it starts

void Advance() {
    ++g_index;
    g_t = 0;
}

unsigned short StepButtons(const Step& s) {
    if (g_t == 0) {
        // A literal button is the PC's (Japan's Config); @ADDR is already the
        // live Config's. Section 2a of docs/input-script.md.
        g_step_buttons = s.buttons_at ? static_cast<unsigned short>(Read(s.buttons_at, 2))
                                      : bof3x::input::sigma::RecipeToPad(s.buttons);
        if (s.buttons_at)
            Log("input       line %d: @0x%08X holds buttons 0x%04X", s.line, (unsigned)s.buttons_at, g_step_buttons);
        else if (g_step_buttons != s.buttons)
            Log("input       line %d: buttons 0x%04X pressed as 0x%04X (the release's starting words stand)", s.line,
                (unsigned)s.buttons, (unsigned)g_step_buttons);
    }
    return g_step_buttons;
}

// The word for the frame about to run; advances the recipe by one frame.
unsigned short NextWord() {
    for (;;) {
        if (g_index >= g_steps.size()) {
            Finish("done");
            return 0;
        }
        const Step& s = g_steps[g_index];
        switch (s.kind) {
        case Kind::Wait:
            if (g_t < s.n) { ++g_t; return 0; }
            Advance();
            continue;
        case Kind::Hold:
            if (g_t < s.n) { const unsigned short b = StepButtons(s); ++g_t; return b; }
            Advance();
            continue;
        case Kind::Press: {
            const unsigned per = s.hold + s.gap;
            if (g_t < per * s.n) {
                const unsigned short b = StepButtons(s);
                const unsigned phase = g_t++ % per;
                return phase < s.hold ? b : 0;
            }
            Advance();
            continue;
        }
        case Kind::Until:
            if (Holds(s)) {
                Log("input       line %d: until met after %u frames (value 0x%X), recipe frame %u", s.line, g_t,
                    (unsigned)Read(s.addr, s.width), g_frame);
                Advance();
                continue;
            }
            if (g_t >= s.n) {
                Log("input       line %d: until 0x%08X timed out after %u frames, value 0x%X", s.line,
                    (unsigned)s.addr, s.n, (unsigned)Read(s.addr, s.width));
                Finish("FAILED");
                return 0;
            }
            ++g_t;
            return 0;
        case Kind::Seek: {
            // g_t counts frames; a press is hold + gap of them, and the
            // condition is looked at only between presses.
            const unsigned per = s.hold + s.gap;
            const unsigned phase = g_t % per;
            if (phase == 0) {
                if (Holds(s)) {
                    Log("input       line %d: seek met after %u presses (value 0x%X), recipe frame %u", s.line,
                        g_t / per, (unsigned)Read(s.addr, s.width), g_frame);
                    Advance();
                    continue;
                }
                if (g_t / per >= s.n) {
                    Log("input       line %d: seek 0x%08X gave up after %u presses, value 0x%X", s.line,
                        (unsigned)s.addr, s.n, (unsigned)Read(s.addr, s.width));
                    Finish("FAILED");
                    return 0;
                }
            }
            // StepButtons reads @ADDR when g_t is 0, i.e. once for the step.
            const unsigned short b = StepButtons(s);
            ++g_t;
            return phase < s.hold ? b : 0;
        }
        case Kind::Shot:
            if (!g_release && g_t == 0) {
                SaveShot(s);
                Log("input       shot %s recipe frame %u", s.text.c_str(), g_frame);
                LogFlush();
            }
            if (g_t < s.n) { const unsigned short b = StepButtons(s); ++g_t; return b; }
            if (g_release) Freeze(s);
            Advance();
            continue;
        case Kind::Peek:
            Log("input       peek %s = 0x%X (%u) recipe frame %u", s.text.c_str(), (unsigned)Read(s.addr, s.width),
                (unsigned)Read(s.addr, s.width), g_frame);
            Advance();
            continue;
        case Kind::Poke: {
            void* const p = reinterpret_cast<void*>(static_cast<std::uintptr_t>(s.addr));
            if (s.width == 1) *static_cast<volatile std::uint8_t*>(p) = static_cast<std::uint8_t>(s.value);
            else if (s.width == 2) *static_cast<volatile std::uint16_t*>(p) = static_cast<std::uint16_t>(s.value);
            else *static_cast<volatile std::uint32_t*>(p) = s.value;
            Log("input       POKE 0x%08X = 0x%X (u%u) recipe frame %u", (unsigned)s.addr, (unsigned)s.value,
                s.width * 8, g_frame);
            Advance();
            continue;
        }
        case Kind::Music:
            // Through the original's entry, so BOF3X_ORIGINAL=Music_Play plays Capcom's.
            Log("input       music %u (fade %u frames) recipe frame %u (Frame_Counter %u)", (unsigned)s.value, s.n, g_frame,
                (unsigned)Frame_Counter);
            bof3::orig::Music_Play(s.value, static_cast<int>(s.n));
            Advance();
            continue;
        case Kind::Mark:
            Log("input       mark %s recipe frame %u", s.text.c_str(), g_frame);
            Advance();
            continue;
        case Kind::Key: {
            // A press as the player's reaches Game_WndProc: WM_KEYDOWN posted
            // as the step starts (the loop's next PeekMessage dispatches it,
            // before this frame runs), WM_KEYUP with the transition bits as
            // the hold ends. The step costs its N frames, like a pad hold.
            HWND hwnd = static_cast<HWND>(Game_Hwnd);
            if (!hwnd) Fatal("BOF3X_INPUT line %d: key %s before the game window exists", s.line, s.text.c_str());
            if (g_t == 0) {
                PostMessageA(hwnd, WM_KEYDOWN, s.value, 1);
                Log("input       line %d: key %s (vk 0x%02X) down for %u frames, recipe frame %u (Frame_Counter %u, paused %u)",
                    s.line, s.text.c_str(), (unsigned)s.value, s.n, g_frame, (unsigned)Frame_Counter, (unsigned)Game_Paused);
                LogFlush();
            }
            if (g_t < s.n) { ++g_t; return 0; }
            PostMessageA(hwnd, WM_KEYUP, s.value, 0xC0000001u);
            Advance();
            continue;
        }
        case Kind::Quit:
            if (g_t == 0) {
                g_quit_expected = true;
                Log("input       line %d: quit expected within %u frames, recipe frame %u", s.line, s.n, g_frame);
                LogFlush();
            }
            if (g_t >= s.n) {
                g_quit_expected = false;
                Log("input       line %d: the game did not quit within %u frames", s.line, s.n);
                Finish("FAILED");
                return 0;
            }
            ++g_t;
            return 0;
        case Kind::End:
            Finish("done");
            return 0;
        }
    }
}

// Replaces WinMain's call of Input_Latch at 0x4FCDDE. Capcom's latch always
// runs first, through DeviceLatch - the devices are polled and re-acquired as
// they would be - and then, while the recipe plays, pad 1's words are ours.
//
// WinMain calls the latch once per pass of its message loop, and the loop
// spins without running a frame while the window is inactive; Frame_Counter
// only moves when a frame runs. So the recipe advances when Frame_Counter
// changes, and the three words are set from the recipe's own previous and
// current word - a latch repeated inside one frame writes the same three
// values again instead of losing the edge in Input_Pressed.
void __cdecl ScriptedLatch() {
    DeviceLatch();
    if (!g_active) return;
    const std::uint32_t frame = Frame_Counter;
    if (!g_seen_frame || frame != g_last_frame) {
        g_seen_frame = true;
        g_last_frame = frame;
        g_prev = g_cur;
        g_cur = NextWord();
        ++g_frame;
        RandCountFrame();
        if (!g_active) return;
        SaveTimed();
    }
    Input_Held = g_cur;
    Input_Previous = g_prev;
    Input_Pressed = static_cast<unsigned short>((g_prev ^ g_cur) & g_cur);
}

// --- Recording (BOF3X_RECORD) -------------------------------------------
//
// The same latch, the other way round: the player plays, and each frame's pad
// word is written out as a recipe. The word is sampled once, at the first
// latch of a new frame, and then held for the rest of that frame exactly as
// playback holds a recipe's word - so what the game saw while recording is
// what it will see when the recipe is played back, edge for edge. A tap
// shorter than the gap between two frames' first latches is not seen by the
// game either; that is the price of the guarantee. Runs of one word become
// `hold BUTTONS N` or `wait N`; F12 writes `shot recN 1 [BUTTONS]` in place of
// its frame, so the shot costs no frame and the recipe keeps its timing.

// Capcom's latch, and DIV-0033: while the window is not in front the six
// words it wrote are zeroed, because the DirectInput keyboard is opened
// DISCL_BACKGROUND and reads what the player types elsewhere
// (src/game/win_main.cpp). The recipe's words are put in after this, so an
// unattended recipe run is not affected.
void DeviceLatch() {
    GapWatch_Pause();   // BOF3X_GAPWATCH: the hash reads every page and is no reader of the game's (gapwatch.h)
    StateHash_Tick();
    GapWatch_Tick();
    WavDump_Tick();
    bof3::orig::Input_Latch();
    if (WinMain_InputAllowed()) return;
    Input_Held = Input_Previous = Input_Pressed = 0;
    Input2_Held = Input2_Previous = Input2_Pressed = 0;
}

FILE* g_rec = nullptr;
unsigned short g_run_word = 0;
unsigned g_run = 0;
unsigned g_shots = 0;
bool g_f12 = false;

void WriteButtons(unsigned short word) {
    bool first = true;
    for (const Button& b : kButtons)
        if (word & b.bit) {
            std::fprintf(g_rec, "%s%s", first ? "" : "+", b.name);
            first = false;
        }
}

void FlushRun() {
    if (g_run == 0) return;
    if (g_run_word == 0) {
        std::fprintf(g_rec, "wait %u\n", g_run);
    } else {
        std::fprintf(g_rec, "hold ");
        WriteButtons(g_run_word);
        std::fprintf(g_rec, " %u\n", g_run);
    }
    std::fflush(g_rec);
    g_run = 0;
}

void Record(unsigned short word) {
    // F12 from the game's own keyboard state: Pad_Read reads DirectInput's 256
    // key bytes to 0x7DE828 each latch (symbols.toml Pad_Read), and DIK_F12 is
    // 0x58. GetAsyncKeyState saw nothing on the first recording, 2026-09-23 -
    // the game's DirectInput keyboard keeps the key from it.
    // kKeyState: now Key_State (symbols.toml).
    constexpr std::uint32_t kDikF12 = 0x58;
    const bool f12 = (Read(Key_State_addr + kDikF12, 1) & 0x80) != 0;
    const bool shot = f12 && !g_f12;
    g_f12 = f12;
    if (shot) {
        FlushRun();
        std::fprintf(g_rec, "shot rec%u 1", ++g_shots);
        if (word) {
            std::fprintf(g_rec, " ");
            WriteButtons(word);
        }
        std::fprintf(g_rec, "   # recipe frame %u\n", g_frame);
        std::fflush(g_rec);
        Log("input       record: shot rec%u at recipe frame %u", g_shots, g_frame);
        LogFlush();
        return;
    }
    if (g_run && word != g_run_word) FlushRun();
    g_run_word = word;
    ++g_run;
}

void __cdecl RecordingLatch() {
    DeviceLatch();
    const std::uint32_t frame = Frame_Counter;
    if (!g_seen_frame || frame != g_last_frame) {
        g_seen_frame = true;
        g_last_frame = frame;
        g_prev = g_cur;
        g_cur = Input_Held;   // the player's word, as Capcom's latch just read it
        Record(bof3x::input::sigma::PadToRecipe(g_cur));   // written in the PC's terms (input-script.md 2a)
        ++g_frame;
        RandCountFrame();
    }
    Input_Held = g_cur;
    Input_Previous = g_prev;
    Input_Pressed = static_cast<unsigned short>((g_prev ^ g_cur) & g_cur);
}

// Opens the recording and puts RecordingLatch on WinMain's latch call. Runs are
// written when they end; the one still open when the process ends is written
// by InputScript_Stop, at DLL_PROCESS_DETACH. A process ended by Fatal or
// TerminateProcess gets no detach, and its recording keeps the last run lost.
void RecordStart(const char* path) {
    g_rec = std::fopen(path, "w");
    if (!g_rec) Fatal("BOF3X_RECORD: cannot open %s for writing", path);
    char lang[16] = "(unset)", filter[16] = "(unset)";
    GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    GetEnvironmentVariableA("BOF3X_FILTER", filter, sizeof filter);
    std::fprintf(g_rec,
                 "# Recorded by BOF3X_RECORD (src/hook/input_script.cpp): one pad word a frame from\n"
                 "# recipe frame 0. BOF3X_LANG=%s BOF3X_FILTER=%s - play it back with the same\n"
                 "# language, since text timing differs between them. F12 wrote the shots.\n",
                 lang, filter);
    std::fflush(g_rec);
    Log("input       recording the pad to %s (F12 = shot)", path);
    constexpr std::uint32_t kLatchCall = 0x4FCDDE;   // WinMain: call Input_Latch
    constexpr std::uint32_t kInputLatch = bof3::addr::Input_Latch;
    RetargetCall("InputRecord", kLatchCall, kInputLatch, reinterpret_cast<void*>(&RecordingLatch), true);
}

}  // namespace

// --- The Rand count (recorded and scripted runs) ------------------------------
//
// The game's Rand 0x5B93D2 is the CRT rand(), never reseeded (no srand in
// the binary), so what a run draws depends on how many calls came before.
// For a recording and its replay to be compared, each frame's running total
// goes to the log: `randlog     frame F rand K`. The first frame whose K
// differs between the two logs is where a consumer the frame count does not
// fix ran (2026-09-30, the fish that would not replay). Since 2026-10-06 Rand
// is ours (crt_rest.cpp) and counts itself; with Rand left original by
// BOF3X_ORIGINAL, crt_rest puts a counting copy of Capcom's at its entry
// instead. Either way the count is an instrument: it runs whatever
// BOF3X_ORIGINAL says, so a reference side under `*` logs its count too
// (until 2026-10-05 it read 0 there on every frame).
void RandCountStart() {
    // A traced run goes without the count, as it always has: the tracer
    // arms every unowned entry (dllmain.cpp: CallTrace_Start first), Rand's
    // among them when BOF3X_ORIGINAL leaves it Capcom's, and crt_rest makes
    // no copy of Rand under BOF3X_CALLTRACE.
    char trace[4];
    if (GetEnvironmentVariableA("BOF3X_CALLTRACE", trace, sizeof trace)) {
        Log("input       randlog: off under BOF3X_CALLTRACE (Rand's entry is the trace's)");
        return;
    }
    crt_rest::RandCount_Start();
}

void RandCountFrame() {
    if (crt_rest::RandCounting()) Log("randlog     frame %u rand %u", g_frame, crt_rest::RandCount());
}

// BOF3X_STATEHASH without a recipe, under Capcom's WinMain: the tick, then
// the latch as Capcom called it.
void __cdecl HashedLatch() {
    StateHash_Tick();
    WavDump_Tick();
    bof3::orig::Input_Latch();
}

// BOF3X_WAV_DIR alone, under Capcom's WinMain: the dump's frame, then the
// latch as Capcom called it (wav_dump.h). Our WinMain reaches the dump
// through DeviceLatch.
void __cdecl WavLatch() {
    WavDump_Tick();
    bof3::orig::Input_Latch();
}

void InputScript_Stop() {
    if (g_scripted && g_active) {
        // The process is ending with the recipe still playing. A `quit` step
        // asked for exactly that; any other step did not.
        g_active = false;
        // F9's second press quits on its WM_KEYDOWN, inside the key step's
        // own hold: a key step whose next step, past any wait / peek / mark
        // (the owner's recorded `wait 42` after the press), is `quit` counts
        // as the quit step.
        bool key_then_quit = false;
        if (g_index < g_steps.size() && g_steps[g_index].kind == Kind::Key) {
            std::size_t k = g_index + 1;
            while (k < g_steps.size() && (g_steps[k].kind == Kind::Wait || g_steps[k].kind == Kind::Peek ||
                                          g_steps[k].kind == Kind::Mark))
                ++k;
            key_then_quit = k < g_steps.size() && g_steps[k].kind == Kind::Quit;
        }
        if (g_quit_expected || key_then_quit)
            Log("input       done at recipe frame %u (Frame_Counter %u); the game quit as the recipe's quit step expected",
                g_frame, g_last_frame);
        else
            Log("input       FAILED at recipe frame %u (Frame_Counter %u): the game exited with line %d current",
                g_frame, g_last_frame, g_index < g_steps.size() ? g_steps[g_index].line : 0);
        LogFlush();
    }
    if (!g_rec) return;
    FlushRun();
    std::fclose(g_rec);
    g_rec = nullptr;
}

// WinMain, while Game_Paused: Capcom's loop skipped Input_Latch, and ours
// does too - no device is polled, no pad word written, no state-hash tick.
// A playing recipe still advances one step per frame (Frame_Counter runs
// through a pause), its words dropped: that is what lets a recipe time the
// pause between two `key f9`.
void InputScript_PausedLatch() {
    if (!g_scripted || !g_active) return;
    const std::uint32_t frame = Frame_Counter;
    if (!g_seen_frame || frame != g_last_frame) {
        g_seen_frame = true;
        g_last_frame = frame;
        g_prev = g_cur;
        g_cur = NextWord();
        ++g_frame;
        RandCountFrame();
    }
}

bool InputScript_PausedWord(unsigned short* word) {
    if (!g_scripted || !g_active) return false;
    *word = g_cur;
    return true;
}

void InputScript_Start() {
    char path[MAX_PATH];
    char rec[MAX_PATH];
    const DWORD r = GetEnvironmentVariableA("BOF3X_RECORD", rec, sizeof rec);
    const DWORD n = GetEnvironmentVariableA("BOF3X_INPUT", path, sizeof path);
    if (r && n) Fatal("BOF3X_RECORD and BOF3X_INPUT are both set; one latch, one of them");
    const bool hashing = StateHash_Start();
    constexpr std::uint32_t kLatchCall = 0x4FCDDE;   // WinMain: call Input_Latch
    constexpr std::uint32_t kInputLatch = bof3::addr::Input_Latch;  // 0x4FC6A0
    if (r >= sizeof rec) Fatal("BOF3X_RECORD: the path is %lu characters, over MAX_PATH", (unsigned long)r);
    if (n >= sizeof path) Fatal("BOF3X_INPUT: the path is %lu characters, over MAX_PATH", (unsigned long)n);
    if (r > 0) {
        RecordStart(rec);
        RandCountStart();
        return;
    }
    if (n == 0) {
        // No recipe and no recording: Capcom's WinMain calls Input_Latch
        // itself, so the state hash takes the site for its tick and then
        // runs the latch untouched. Our WinMain reaches the tick through
        // DeviceLatch.
        if (hashing) RetargetCall("StateHash", kLatchCall, kInputLatch, reinterpret_cast<void*>(&HashedLatch), true);
        else if (WavDump_On()) RetargetCall("WavDump", kLatchCall, kInputLatch, reinterpret_cast<void*>(&WavLatch), true);
        return;
    }
    RandCountStart();
    Load(path);
    Log("input       %u steps from %s", (unsigned)g_steps.size(), path);
    wchar_t dir[MAX_PATH];
    const DWORD dn = GetEnvironmentVariableW(L"BOF3X_SHOT_DIR", dir, MAX_PATH);
    if (dn > 0 && dn < MAX_PATH) {
        g_shot_dir = dir;
        Log("input       shots are written by the game to %s (BOF3X_SHOT_DIR)", Utf8Path(dir).text);
    }
    char every[16];
    const DWORD en = GetEnvironmentVariableA("BOF3X_SHOT_EVERY", every, sizeof every);
    if (en > 0 && en < sizeof every) {
        char* end = nullptr;
        const unsigned long v = std::strtoul(every, &end, 10);
        if (!end || *end || v == 0 || v > 1000000) Fatal("BOF3X_SHOT_EVERY: '%s' is not a frame count (1..1000000)", every);
        if (g_shot_dir.empty()) Fatal("BOF3X_SHOT_EVERY needs BOF3X_SHOT_DIR: the game writes the frames there");
        g_every = static_cast<unsigned>(v);
        Log("input       a frame every %u recipe frames to the shot directory (BOF3X_SHOT_EVERY)", g_every);
    }
    char wait[8];
    if (GetEnvironmentVariableA("BOF3X_SHOT_WAIT", wait, sizeof wait)) {
        // Named for the process, so the driver can find it by pid.
        wchar_t name[64];
        std::swprintf(name, 64, L"Local\\bof3x_shot_%lu", GetCurrentProcessId());
        g_release = CreateEventW(nullptr, FALSE, FALSE, name);
        if (!g_release) Fatal("BOF3X_SHOT_WAIT: CreateEvent failed, error %lu", GetLastError());
        Log("input       shots freeze the game until released (BOF3X_SHOT_WAIT)");
    }
    // Not an Inject: nothing of Capcom's is replaced, and BOF3X_ORIGINAL has
    // no say - the variable being set is the switch.
    RetargetCall("InputScript", kLatchCall, kInputLatch, reinterpret_cast<void*>(&ScriptedLatch), true);
    g_scripted = true;
    g_active = true;
}

// How many passes before a capture are drawn even when late. One was not
// enough (docs/button-routes.md 9.2): the picture a capture saves also
// depends on whether the passes before the forced one were drawn. Measured
// on a fade, one pass left undrawn: just before the forced pass, the capture
// is the picture of two frames earlier; two before it, 163 pixels move by up
// to 2; three or more before it, nothing.
constexpr unsigned kShotLeadPasses = 3;

// True when a recipe `shot` step starts within the next kShotLeadPasses - 1
// latches. Only steps of known length are walked; an until, a seek or a key
// step ends the walk (its length is the game's to decide).
bool ShotSoon() {
    unsigned frames = 0;   // latches still to run before the shot step is entered
    for (std::size_t i = g_index; i < g_steps.size(); ++i) {
        const Step& s = g_steps[i];
        const unsigned done = i == g_index ? g_t : 0;
        switch (s.kind) {
        case Kind::Shot:
            return frames + 1 < kShotLeadPasses;
        case Kind::Wait:
        case Kind::Hold:
            frames += s.n > done ? s.n - done : 0;
            break;
        case Kind::Press: {
            const unsigned total = (s.hold + s.gap) * s.n;
            frames += total > done ? total - done : 0;
            break;
        }
        case Kind::Peek:
        case Kind::Poke:
        case Kind::Music:
        case Kind::Mark:
            break;   // no frame of their own
        default:
            return false;
        }
        if (frames + 1 >= kShotLeadPasses) return false;
    }
    return false;
}

bool InputScript_ShotPending() {
    if (!g_active || !g_scripted) return false;
    if (g_index < g_steps.size() && g_steps[g_index].kind == Kind::Shot) return true;
    if (ShotSoon()) return true;
    // BOF3X_SHOT_EVERY: a latch within the next kShotLeadPasses writes a frame
    if (g_every == 0) return false;
    for (unsigned k = 1; k <= kShotLeadPasses; ++k)
        if ((g_frame + k) % g_every == 0) return true;
    return false;
}

void InputScript_Latch() {
    if (g_rec) RecordingLatch();
    else if (g_scripted) ScriptedLatch();
    else DeviceLatch();
}

unsigned InputScript_Frame() { return g_scripted ? g_frame : 0; }

}  // namespace bof3
