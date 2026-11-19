// The function keys (the owner's call, 2026-11-18; docs/controls.md section
// 4.3): the game's own actions that a key starts - double speed, the row above
// the text, the quick menu, quicksave and quick-load, the window, the pause,
// the picture to a file and Capcom's quick save write - as one table shared by
// the launcher, which shows and edits it on the Controls tab's third sub-page
// and stores it in bof3x.ini, and the DLL, which receives it as BOF3X_FKEYS
// and asks it which action a key message or a pad press holds
// (src/game/platform/function_keys_game.cpp).
//
// Each action holds at most one keyboard key and one pad input, either of
// which may be none. A key is a number of the launcher's key cells (the PC
// set-1 scan code, the extended flag as bit 7: input/bindings.h's KeyName); a
// pad input is input/bindings.h's PadInput. Nothing stops one key or pad input
// from holding two things - two actions, or an action and a game button: the
// launcher draws every such cell in red (FnDoubled), and the game does both
// (an action and a game button), or of two actions the first in kFnOrder.
//
// Plain C++, no system header: the host tests (tools/fkeys_host,
// tools/launcher_host) and the Linux probe build it as the DLL does.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bof3x::input {

// The actions, in the page's order (by their default key). Never renumber
// without the page: the launcher's cell ids are base + action.
enum class FnAction : unsigned char {
    kSpeed, kRows, kQuickMenu, kQuicksave, kQuickload, kWindow, kPause, kPicture, kSaveWrite, kCount
};
constexpr int kFnActionCount = static_cast<int>(FnAction::kCount);

struct FnActionInfo {
    FnAction action;
    const char* name;           // in the files: fkey.<name>= and fpad.<name>=
    int default_key;            // a key number (the F key), never none
    const char* label;          // the page's row, for people
    const char* note;           // the page's note beside the row, or "" (the tooling's actions)
};
const FnActionInfo* FnActions();   // kFnActionCount entries, in FnAction's order

// The order in which a key message is offered to the actions: the order of
// Game_WndProc's tests before this table (quick menu, pause, window, the row
// above the text, quicksave, quick-load, Capcom's save write, the picture,
// double speed). A key that two actions hold starts the first of them alone,
// as BOF3X_RUBY_KEY on a key another action held did before.
extern const FnAction kFnOrder[kFnActionCount];

// An action's code name -> the action, or -1.
int FnActionFromName(const std::string& name);

// The key number of function key F`n` (1..12): F1..F10 0x3B..0x44, F11 0x57,
// F12 0x58 (DirectInput's numbers, which the key cells hold); -1 otherwise.
int FnKeyOfF(int n);

struct FnBindings {
    int key[kFnActionCount];   // a key number, or -1 for none
    int pad[kFnActionCount];   // a PadInput, or -1 for none
    // F1, F4, F5, F6, F7, F8, F9, F11, F12, in FnAction's order; no pad input.
    static FnBindings Defaults();
    bool operator==(const FnBindings& o) const;
    bool operator!=(const FnBindings& o) const { return !(*this == o); }
};

// The text forms: `fkey.<action>=<key>` (a key name as input/bindings.h's
// KeyFromName reads it - "F6", "Z", "Numpad8", "0x1A" - or "none") and
// `fpad.<action>=<input>` (a pad input's name, "south", "rpaddle1" ... or
// "none"), items separated by `sep`. Parse applies each item it understands
// over `io`, in order, and returns how many; every item it does not (an
// unknown prefix, action, key or input, or no '=') is left out and described
// in `problems`, when given. Format writes every action, key then pad, each
// item followed by `sep`.
int FnParse(const std::string& text, char sep, FnBindings& io, std::vector<std::string>* problems);
std::string FnFormat(const FnBindings& b, const char* sep);

// BOF3X_RUBY_KEY's value, the row above the text's key before this table
// (DIV-0155): "f1".."f12" (lower case, as the game reads it after folding)
// -> that key's number; "none" -> -1; anything else -> -2.
int FnRubyKey(const std::string& lower);

// A key message's key number, as the launcher's capture would store the key:
// `vk` and `lparam` as a WM_KEYDOWN carries them. F1..F12 by their virtual-key
// numbers alone (0x70..0x7B: before this table the game compared nothing
// else, and an input recipe's key carries no scan code); any other key by the
// scan code in bits 16..23 and the extended flag in bit 24 (scan | 0x80), as
// the capture's WH_KEYBOARD hook reads the same lParam. -1 when there is no
// scan code (a recipe's key that is not an F key), or when the scan code is
// an F key's but the virtual key is not (a key some layout or tool remaps).
int FnKeyOfMessage(unsigned vk, std::int32_t lparam);

// The lParam bits 16..24 that Windows gives the key whose DirectInput number
// is `dik` (key_sdl.h's numbering): bits 16..23 its scan code, bit 24 its
// extended flag - with the one pair DirectInput numbers otherwise, both of the
// key 0x45: NumLock (DIK 0x45) comes as 0x45 extended and Pause (DIK 0xC5) as
// 0x45 plain. For the SDL window off Windows, so its key messages read as
// Windows' do (FnKeyOfMessage gives Pause 0x45 and NumLock 0xC5 there, as the
// capture stores them). 0 for 0 or a number above 0xFF.
std::int32_t FnMessageBitsOfDik(int dik);

// The action `key` starts: the first in kFnOrder that holds it, or -1 (none,
// or `key` is -1).
int FnActionOfKey(const FnBindings& b, int key);

// For each entry of `values` (key numbers or pad inputs; -1 for an empty
// cell), whether another entry holds the same value: the cells the launcher
// draws in red. An empty cell is never doubled.
std::vector<bool> FnDoubled(const std::vector<int>& values);

// The red cells of the Controls tab's three pages at once: `game_keys` and
// `game_pads` the two pages' key and pad cells in any order (-1 empty; keys a
// cell does not show, the keypad's diagonals, may follow - their answers are
// there but no cell is drawn), `fk` the Function keys page's. A key is
// compared with keys and a pad input with pad inputs.
struct FnRed {
    std::vector<bool> game_keys, game_pads;
    bool fn_key[kFnActionCount], fn_pad[kFnActionCount];
};
FnRed FnRedCells(const std::vector<int>& game_keys, const std::vector<int>& game_pads, const FnBindings& fk);

}  // namespace bof3x::input
