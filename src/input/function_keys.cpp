// The function keys' table and its text forms (function_keys.h).
#include "input/function_keys.h"

#include <cctype>
#include <cstddef>

#include "input/bindings.h"

namespace bof3x::input {
namespace {

// The labels are the launcher's, in ASD-STE100 Simplified Technical English
// (CLAUDE.md): what the key does in the game.
constexpr FnActionInfo kActions[kFnActionCount] = {
    {FnAction::kSpeed, "speed", 0x3B, "Double speed: on or off", ""},                    // F1, DIV-0048
    {FnAction::kRows, "reading_rows", 0x3E, "Row above the text: show or hide", ""},     // F4, DIV-0155
    {FnAction::kQuickMenu, "quick_menu", 0x3F, "Quicksave list: open or close", ""},     // F5, the quick menu
    {FnAction::kQuicksave, "quicksave", 0x40, "Make a quicksave", ""},                   // F6
    {FnAction::kQuickload, "quickload", 0x41, "Load the newest quicksave", ""},          // F7
    {FnAction::kWindow, "window", 0x42, "Full screen or window", ""},                    // F8, DIV-0032
    {FnAction::kPause, "pause", 0x43, "Pause (twice: go to the title)", "Title: exit"},  // F9, Capcom's
    {FnAction::kPicture, "picture", 0x57, "Save the picture to a file", "For tests"},    // F11, tooling
    {FnAction::kSaveWrite, "save_write", 0x58, "Save in the first save slot", "Capcom's key"},   // F12, Capcom's
};

std::string Lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

}  // namespace

const FnActionInfo* FnActions() { return kActions; }

const FnAction kFnOrder[kFnActionCount] = {
    FnAction::kQuickMenu, FnAction::kPause, FnAction::kWindow, FnAction::kRows, FnAction::kQuicksave,
    FnAction::kQuickload, FnAction::kSaveWrite, FnAction::kPicture, FnAction::kSpeed,
};

int FnActionFromName(const std::string& name) {
    const std::string n = Lower(name);
    for (int i = 0; i < kFnActionCount; ++i)
        if (n == kActions[i].name) return i;
    return -1;
}

int FnKeyOfF(int n) {
    if (n >= 1 && n <= 10) return 0x3A + n;
    if (n == 11) return 0x57;
    if (n == 12) return 0x58;
    return -1;
}

FnBindings FnBindings::Defaults() {
    FnBindings b{};
    for (int i = 0; i < kFnActionCount; ++i) {
        b.key[i] = kActions[i].default_key;
        b.pad[i] = -1;
    }
    return b;
}

bool FnBindings::operator==(const FnBindings& o) const {
    for (int i = 0; i < kFnActionCount; ++i)
        if (key[i] != o.key[i] || pad[i] != o.pad[i]) return false;
    return true;
}

int FnParse(const std::string& text, char sep, FnBindings& io, std::vector<std::string>* problems) {
    int understood = 0;
    size_t at = 0;
    const auto Problem = [&](const std::string& item, const char* why) {
        if (problems) problems->push_back("\"" + item + "\": " + why);
    };
    while (at <= text.size()) {
        size_t end = text.find(sep, at);
        if (end == std::string::npos) end = text.size();
        const std::string item = Trim(text.substr(at, end - at));
        at = end + 1;
        if (item.empty() || item[0] == '#') continue;
        const size_t eq = item.find('=');
        if (eq == std::string::npos) {
            Problem(item, "no '='");
            continue;
        }
        const std::string name = Lower(Trim(item.substr(0, eq)));
        const std::string value = Trim(item.substr(eq + 1));
        const bool is_key = name.compare(0, 5, "fkey.") == 0, is_pad = name.compare(0, 5, "fpad.") == 0;
        if (!is_key && !is_pad) {
            Problem(item, "not fkey.<action> or fpad.<action>");
            continue;
        }
        const int action = FnActionFromName(name.substr(5));
        if (action < 0) {
            Problem(item, "no such action");
            continue;
        }
        int v = -1;
        if (Lower(value) != "none") {
            v = is_key ? KeyFromName(value) : PadInputFromName(value);
            if (v < 0) {
                Problem(item, is_key ? "not a key name" : "not a pad input's name");
                continue;
            }
        }
        (is_key ? io.key : io.pad)[action] = v;
        ++understood;
    }
    return understood;
}

std::string FnFormat(const FnBindings& b, const char* sep) {
    std::string out;
    for (int i = 0; i < kFnActionCount; ++i) {
        out += std::string("fkey.") + kActions[i].name + "=" + (b.key[i] < 0 ? std::string("none") : KeyName(b.key[i])) + sep;
        out += std::string("fpad.") + kActions[i].name + "=" +
               (b.pad[i] < 0 || b.pad[i] >= static_cast<int>(PadInput::kCount)
                    ? std::string("none")
                    : std::string(PadInputName(static_cast<PadInput>(b.pad[i])))) +
               sep;
    }
    return out;
}

int FnRubyKey(const std::string& lower) {
    if (lower == "none") return -1;
    for (int n = 1; n <= 12; ++n)
        if (lower == "f" + std::to_string(n)) return FnKeyOfF(n);
    return -2;
}

int FnKeyOfMessage(unsigned vk, std::int32_t lparam) {
    if (vk >= 0x70 && vk <= 0x7B) return FnKeyOfF(static_cast<int>(vk - 0x70) + 1);
    const int scan = static_cast<int>((static_cast<std::uint32_t>(lparam) >> 16) & 0xFF);
    if (scan == 0) return -1;
    const int key = scan | ((static_cast<std::uint32_t>(lparam) & (1u << 24)) ? 0x80 : 0);
    for (int n = 1; n <= 12; ++n)
        if (key == FnKeyOfF(n)) return -1;   // an F key's scan code under another virtual key
    return key;
}

std::int32_t FnMessageBitsOfDik(int dik) {
    if (dik <= 0 || dik > 0xFF) return 0;
    int scan = dik & 0x7F;
    bool extended = (dik & 0x80) != 0;
    if (dik == 0x45) extended = true;          // NumLock: Windows flags it extended
    if (dik == 0xC5) extended = false;         // Pause: 0x45, not extended
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(scan) << 16) | (extended ? (1u << 24) : 0u));
}

int FnActionOfKey(const FnBindings& b, int key) {
    if (key < 0) return -1;
    for (FnAction a : kFnOrder)
        if (b.key[static_cast<int>(a)] == key) return static_cast<int>(a);
    return -1;
}

std::vector<bool> FnDoubled(const std::vector<int>& values) {
    std::vector<bool> red(values.size(), false);
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] < 0) continue;
        for (size_t j = 0; j < values.size(); ++j)
            if (j != i && values[j] == values[i]) {
                red[i] = true;
                break;
            }
    }
    return red;
}

FnRed FnRedCells(const std::vector<int>& game_keys, const std::vector<int>& game_pads, const FnBindings& fk) {
    std::vector<int> keys = game_keys, pads = game_pads;
    for (int a = 0; a < kFnActionCount; ++a) {
        keys.push_back(fk.key[a]);
        pads.push_back(fk.pad[a]);
    }
    const std::vector<bool> rk = FnDoubled(keys), rp = FnDoubled(pads);
    FnRed out{};
    out.game_keys.assign(rk.begin(), rk.begin() + static_cast<std::ptrdiff_t>(game_keys.size()));
    out.game_pads.assign(rp.begin(), rp.begin() + static_cast<std::ptrdiff_t>(game_pads.size()));
    for (int a = 0; a < kFnActionCount; ++a) {
        out.fn_key[a] = rk[game_keys.size() + static_cast<size_t>(a)];
        out.fn_pad[a] = rp[game_pads.size() + static_cast<size_t>(a)];
    }
    return out;
}

}  // namespace bof3x::input
