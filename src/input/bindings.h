// The physical bindings: which keyboard keys and which pad inputs mean which
// PlayStation button (docs/controls.md section 4.2). Shared by the launcher,
// which stores them in bof3x.ini and edits them in its Controls dialog, and
// by the DLL, which receives them as BOF3X_KEYS / BOF3X_PAD and applies them
// in src/game/pad_read.cpp (DIV-0050). Plain C++, no Windows headers (the
// .cpp takes the DIK_ names of the default table from dinput.h).
//
// Two text forms, one grammar: `name=action[+action]` items separated by
// `sep`, where a key item's name is a key's (below) and a pad item's name is
// a pad input's. The ini uses one item a line under the prefixes `key.` and
// `pad.`; the environment uses a comma-separated list.
#pragma once

#include <string>
#include <vector>

namespace bof3x::input {

// The PlayStation pad's bits, the layout Pad_Read builds (docs/input-script.md
// section 2). Never renumber: they are the game's.
enum Action : unsigned short {
    kL2 = 0x1, kR2 = 0x2, kL1 = 0x4, kR1 = 0x8, kTriangle = 0x10, kCircle = 0x20, kCross = 0x40,
    kSquare = 0x80, kSelect = 0x100, kStart = 0x800, kUp = 0x1000, kRight = 0x2000, kDown = 0x4000,
    kLeft = 0x8000,
};

// The fourteen, in the order the Controls dialog lists them.
struct ActionInfo {
    Action bit;
    const char* name;    // in the files: "up", "cross", "l1" ...
    const char* label;   // for people: "Up", "Cross", "L1" ...
};
constexpr int kActionCount = 14;
const ActionInfo* Actions();   // kActionCount entries

// "up+right" <-> 0x3000. ParseActions returns 0 for anything it does not
// understand (an empty list, an unknown name). FormatActions of 0 is "none".
unsigned short ParseActions(const std::string& text);
std::string FormatActions(unsigned short bits);

// Keyboard keys by DirectInput scancode (DIK_*). Names are what the ini and
// the dialog show: "Z", "Space", "Numpad8", "RShift", "Up". A scancode with
// no name is written and read as "0x1A"-style hex, so a table hand-written
// for BOF3.CFG (docs/controls.md section 1) survives the round trip.
struct KeyInfo {
    unsigned char dik;
    const char* name;
};
const KeyInfo* Keys();
int KeyCount();
int KeyFromName(const std::string& name);   // -1 if unknown and not hex
std::string KeyName(int dik);

// Pad inputs, SDL's gamepad model by position: the face buttons south / east
// / west / north, the shoulders and triggers, the d-pad, the sticks' four
// directions each and their clicks, start / back / guide.
enum class PadInput : unsigned char {
    kSouth, kEast, kWest, kNorth, kLb, kRb, kLt, kRt, kStart, kBack, kGuide, kLs, kRs,
    kDpadUp, kDpadDown, kDpadLeft, kDpadRight,
    kLsUp, kLsDown, kLsLeft, kLsRight, kRsUp, kRsDown, kRsLeft, kRsRight,
    kCount
};
struct PadInputInfo {
    PadInput input;
    const char* name;    // "south", "lb", "dpad_up", "ls_up" ...
    const char* label;   // "South (A / Cross)", "LB / L1", "D-pad up", "Left stick up" ...
};
const PadInputInfo* PadInputs();   // kCount entries, in enum order
int PadInputFromName(const std::string& name);   // -1 if unknown
const char* PadInputName(PadInput input);

// The face buttons' meaning: by position (south is the lower button whatever
// its letter), swapped in pairs for a Nintendo-lettered pad, or decided by
// the pad's own labels when it is opened.
enum class Layout : unsigned char { kPositional, kNintendo, kAuto };
const char* LayoutName(Layout layout);
int LayoutFromName(const std::string& name);   // -1 if unknown

struct KeyBinding {
    unsigned char dik;
    unsigned short bits;
};
struct PadBinding {
    PadInput input;
    unsigned short bits;
};

struct Bindings {
    std::vector<KeyBinding> keys;   // in table order; the game's table holds 32 at most
    std::vector<PadBinding> pad;
    Layout layout = Layout::kPositional;

    // The original's default key table, DefaultKeys() (the engine's rule,
    // equal to Key_TableDefault in the player's BOF3.exe where there is one),
    // and the DIV-0050 pad map.
    static Bindings Defaults();
    bool operator==(const Bindings& o) const;
    bool operator!=(const Bindings& o) const { return !(*this == o); }
};

constexpr int kKeyTableMax = 32;

// Key_Table's shape, and Key_TableDefault's: (u16 DIK scancode, u16 pad bits)
// pairs, ended by a key of 0 or by `bytes`. Appends to `out`; false if a key
// is not a DIK scancode (above 0xFF), which means the bytes are not the table.
bool KeysFromTable(const unsigned char* table, size_t bytes, std::vector<KeyBinding>& out);

// The default key table as a rule (the owner, 2026-10-11; docs/controls.md
// section 1): for each PlayStation button, the port's keys by DirectInput
// key name - the arrows and the keypad the directions (the keypad's corners
// the diagonals), Z X C V triangle cross square circle, A Q S W L1 L2 R1 R2,
// Enter start, right Shift select, Esc cross, Space circle. 24 entries, in the
// order Key_TableDefault 0x66C648 holds them.
constexpr int kDefaultKeyCount = 24;
const std::vector<KeyBinding>& DefaultKeys();

// The same as Key_TableDefault's bytes: `bytes` (at most kKeyTableMax * 4)
// of (u16 key, u16 bits) pairs, the entries past the rule's zero. What the
// engine writes at 0x66C648 and compares with the player's BOF3.exe, and what
// the launcher compares with the file.
void DefaultKeyTable(unsigned char* out, size_t bytes);

// `name=action` items separated by `sep`. Parse* accept unknown items by
// skipping them and return how many items were understood (a `name=none`
// item counts but adds nothing); `layout=NAME` is a pad item. Parse* append
// to `out`. Format* write every entry.
int ParseKeys(const std::string& text, char sep, std::vector<KeyBinding>& out);
int ParsePad(const std::string& text, char sep, std::vector<PadBinding>& out, Layout& layout);
std::string FormatKeys(const std::vector<KeyBinding>& keys, const char* prefix, const char* sep);
std::string FormatPad(const std::vector<PadBinding>& pad, Layout layout, const char* prefix, const char* sep);

}  // namespace bof3x::input
