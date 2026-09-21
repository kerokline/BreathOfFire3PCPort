#include "hook/input_script.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The recipe language (docs/input-script.md has it with examples). One step a
// line, '#' to end of line is a comment, buttons joined with '+':
//
//   set hold N              frames a press holds the buttons     (default 4)
//   set gap N               frames released after each press     (default 8)
//   wait N                  N frames, nothing held
//   press BUTTONS [xK]      K times (default 1): hold, then release
//   hold BUTTONS N          N frames held, no release after
//   until ADDR TYPE OP VALUE [timeout N]
//                           nothing held until the condition is true, checked
//                           once a frame; TYPE u8|u16|u32, OP == != & !&
//                           (& = any of VALUE's bits set). A timeout (default
//                           3600 frames) FAILS the recipe.
//   shot NAME [N]           log "input       shot NAME" and hold nothing for N
//                           frames (default 30) while tools/input_run.py
//                           captures the window
//   peek ADDR TYPE [LABEL]  log the value
//   mark TEXT               log the text
//   end                     stop here
//
// Buttons are the PlayStation pad's, the bit layout Pad_Read builds
// (symbols.toml Input_Held): up down left right cross circle square triangle
// l1 l2 r1 r2 start select. Or @ADDR: whatever u16 is at ADDR when the step
// starts - for the field's button assignments, which a save carries
// (symbols.toml Field_MenuButton): `press @0x903584` opens the menu whichever
// shape the loaded save gave it.
//
// When the recipe ends - end, last line, or a failed until - the pad is handed
// back: Input_Latch's own words stand from the next frame on.

namespace bof3 {
namespace {

enum class Kind { Wait, Press, Hold, Until, Shot, Peek, Mark, End };
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
unsigned g_t = 0;                  // frames spent in the current step
bool g_active = false;
bool g_seen_frame = false;
std::uint32_t g_last_frame = 0;    // Frame_Counter at the last new frame
unsigned g_frame = 0;              // frames the recipe has played
unsigned short g_prev = 0, g_cur = 0;

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
            st.kind = Kind::Hold;
            SetButtons(st, t[1]);
            st.n = Number(line, t[2]);
        } else if (w == "until") {
            if (t.size() != 5 && t.size() != 7) ParseError(line, "until ADDR TYPE OP VALUE [timeout N], got", s);
            st.kind = Kind::Until;
            st.width = Width(line, t[2]);
            st.addr = Address(line, t[1], st.width);
            if (t[3] == "==") st.op = Op::Eq;
            else if (t[3] == "!=") st.op = Op::Ne;
            else if (t[3] == "&") st.op = Op::Any;
            else if (t[3] == "!&") st.op = Op::None;
            else ParseError(line, "operator is == != & or !&, not", t[3]);
            st.value = Number(line, t[4]);
            st.n = 3600;
            if (t.size() == 7) {
                if (t[5] != "timeout") ParseError(line, "expected timeout, got", t[5]);
                st.n = Number(line, t[6]);
            }
        } else if (w == "shot") {
            need(2, 3);
            st.kind = Kind::Shot;
            st.text = t[1];
            st.n = t.size() == 3 ? Number(line, t[2]) : 30;
        } else if (w == "peek") {
            need(3, 4);
            st.kind = Kind::Peek;
            st.width = Width(line, t[2]);
            st.addr = Address(line, t[1], st.width);
            st.text = t.size() == 4 ? t[3] : t[1];
        } else if (w == "mark") {
            st.kind = Kind::Mark;
            const std::size_t at = s.find_first_not_of(" \t", s.find("mark") + 4);
            if (at != std::string::npos) st.text = s.substr(at);
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

unsigned short g_step_buttons = 0;   // the current Press/Hold step's, read as it starts

void Advance() {
    ++g_index;
    g_t = 0;
}

unsigned short StepButtons(const Step& s) {
    if (g_t == 0) {
        g_step_buttons = s.buttons_at ? static_cast<unsigned short>(Read(s.buttons_at, 2)) : s.buttons;
        if (s.buttons_at)
            Log("input       line %d: @0x%08X holds buttons 0x%04X", s.line, (unsigned)s.buttons_at, g_step_buttons);
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
        case Kind::Shot:
            if (g_t == 0) {
                Log("input       shot %s recipe frame %u", s.text.c_str(), g_frame);
                LogFlush();
            }
            if (g_t < s.n) { ++g_t; return 0; }
            Advance();
            continue;
        case Kind::Peek:
            Log("input       peek %s = 0x%X (%u) recipe frame %u", s.text.c_str(), (unsigned)Read(s.addr, s.width),
                (unsigned)Read(s.addr, s.width), g_frame);
            Advance();
            continue;
        case Kind::Mark:
            Log("input       mark %s recipe frame %u", s.text.c_str(), g_frame);
            Advance();
            continue;
        case Kind::End:
            Finish("done");
            return 0;
        }
    }
}

// Replaces WinMain's call of Input_Latch at 0x4FCDDE. Capcom's latch always
// runs first - the devices are polled and re-acquired as they would be - and
// then, while the recipe plays, pad 1's words are ours.
//
// WinMain calls the latch once per pass of its message loop, and the loop
// spins without running a frame while the window is inactive; Frame_Counter
// only moves when a frame runs. So the recipe advances when Frame_Counter
// changes, and the three words are set from the recipe's own previous and
// current word - a latch repeated inside one frame writes the same three
// values again instead of losing the edge in Input_Pressed.
void __cdecl ScriptedLatch() {
    Input_Latch();
    if (!g_active) return;
    const std::uint32_t frame = Frame_Counter;
    if (!g_seen_frame || frame != g_last_frame) {
        g_seen_frame = true;
        g_last_frame = frame;
        g_prev = g_cur;
        g_cur = NextWord();
        ++g_frame;
        if (!g_active) return;
    }
    Input_Held = g_cur;
    Input_Previous = g_prev;
    Input_Pressed = static_cast<unsigned short>((g_prev ^ g_cur) & g_cur);
}

}  // namespace

void InputScript_Start() {
    char path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_INPUT", path, sizeof path);
    if (n == 0 || n >= sizeof path) return;
    Load(path);
    Log("input       %u steps from %s", (unsigned)g_steps.size(), path);
    // Not an Inject: nothing of Capcom's is replaced, and BOF3X_ORIGINAL has
    // no say - the variable being set is the switch.
    constexpr std::uint32_t kLatchCall = 0x4FCDDE;   // WinMain: call Input_Latch
    constexpr std::uint32_t kInputLatch = 0x4FC6A0;  // Input_Latch; symbols.gen.h binds the name as a macro
    RetargetCall("InputScript", kLatchCall, kInputLatch, reinterpret_cast<void*>(&ScriptedLatch));
    g_active = true;
}

}  // namespace bof3
