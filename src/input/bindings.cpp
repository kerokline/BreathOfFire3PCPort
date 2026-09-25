#include "input/bindings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bof3x::input {
namespace {

// In the Controls dialog's row order: row r's cells are IDC_CT_KEY1 + r and
// so on (src/launcher/resource.h, launcher.rc), so never reorder alone.
const ActionInfo kActions[kActionCount] = {
    {kUp, "up", "Up"},           {kDown, "down", "Down"},     {kLeft, "left", "Left"},
    {kRight, "right", "Right"},  {kCross, "cross", "Cross"},  {kCircle, "circle", "Circle"},
    {kSquare, "square", "Square"}, {kTriangle, "triangle", "Triangle"}, {kL1, "l1", "L1"},
    {kL2, "l2", "L2"},           {kR1, "r1", "R1"},           {kR2, "r2", "R2"},
    {kStart, "start", "Start"},  {kSelect, "select", "Select"},
};

// DirectInput scancodes. The names are the dialog's and the ini's.
const KeyInfo kKeys[] = {
    {0x01, "Esc"},        {0x02, "1"},          {0x03, "2"},          {0x04, "3"},
    {0x05, "4"},          {0x06, "5"},          {0x07, "6"},          {0x08, "7"},
    {0x09, "8"},          {0x0A, "9"},          {0x0B, "0"},          {0x0C, "Minus"},
    {0x0D, "Equals"},     {0x0E, "Backspace"},  {0x0F, "Tab"},        {0x10, "Q"},
    {0x11, "W"},          {0x12, "E"},          {0x13, "R"},          {0x14, "T"},
    {0x15, "Y"},          {0x16, "U"},          {0x17, "I"},          {0x18, "O"},
    {0x19, "P"},          {0x1A, "LBracket"},   {0x1B, "RBracket"},   {0x1C, "Enter"},
    {0x1D, "LCtrl"},      {0x1E, "A"},          {0x1F, "S"},          {0x20, "D"},
    {0x21, "F"},          {0x22, "G"},          {0x23, "H"},          {0x24, "J"},
    {0x25, "K"},          {0x26, "L"},          {0x27, "Semicolon"},  {0x28, "Apostrophe"},
    {0x29, "Grave"},      {0x2A, "LShift"},     {0x2B, "Backslash"},  {0x2C, "Z"},
    {0x2D, "X"},          {0x2E, "C"},          {0x2F, "V"},          {0x30, "B"},
    {0x31, "N"},          {0x32, "M"},          {0x33, "Comma"},      {0x34, "Period"},
    {0x35, "Slash"},      {0x36, "RShift"},     {0x37, "NumpadMultiply"}, {0x38, "LAlt"},
    {0x39, "Space"},      {0x3A, "CapsLock"},   {0x3B, "F1"},         {0x3C, "F2"},
    {0x3D, "F3"},         {0x3E, "F4"},         {0x3F, "F5"},         {0x40, "F6"},
    {0x41, "F7"},         {0x42, "F8"},         {0x43, "F9"},         {0x44, "F10"},
    {0x45, "NumLock"},    {0x46, "ScrollLock"}, {0x47, "Numpad7"},    {0x48, "Numpad8"},
    {0x49, "Numpad9"},    {0x4A, "NumpadMinus"}, {0x4B, "Numpad4"},   {0x4C, "Numpad5"},
    {0x4D, "Numpad6"},    {0x4E, "NumpadPlus"}, {0x4F, "Numpad1"},    {0x50, "Numpad2"},
    {0x51, "Numpad3"},    {0x52, "Numpad0"},    {0x53, "NumpadPeriod"}, {0x57, "F11"},
    {0x58, "F12"},        {0x9C, "NumpadEnter"}, {0x9D, "RCtrl"},     {0xB5, "NumpadDivide"},
    {0xB8, "RAlt"},       {0xC5, "Pause"},      {0xC7, "Home"},       {0xC8, "Up"},
    {0xC9, "PageUp"},     {0xCB, "Left"},       {0xCD, "Right"},      {0xCF, "End"},
    {0xD0, "Down"},       {0xD1, "PageDown"},   {0xD2, "Insert"},     {0xD3, "Delete"},
};

const PadInputInfo kPadInputs[] = {
    {PadInput::kSouth, "south", "South (A / Cross)"},
    {PadInput::kEast, "east", "East (B / Circle)"},
    {PadInput::kWest, "west", "West (X / Square)"},
    {PadInput::kNorth, "north", "North (Y / Triangle)"},
    {PadInput::kLb, "lb", "LB / L1"},
    {PadInput::kRb, "rb", "RB / R1"},
    {PadInput::kLt, "lt", "LT / L2"},
    {PadInput::kRt, "rt", "RT / R2"},
    {PadInput::kStart, "start", "Start / Options"},
    {PadInput::kBack, "back", "Back / Select / Share"},
    {PadInput::kGuide, "guide", "Guide / PS"},
    {PadInput::kLs, "ls", "Left stick click"},
    {PadInput::kRs, "rs", "Right stick click"},
    {PadInput::kDpadUp, "dpad_up", "D-pad up"},
    {PadInput::kDpadDown, "dpad_down", "D-pad down"},
    {PadInput::kDpadLeft, "dpad_left", "D-pad left"},
    {PadInput::kDpadRight, "dpad_right", "D-pad right"},
    {PadInput::kLsUp, "ls_up", "Left stick up"},
    {PadInput::kLsDown, "ls_down", "Left stick down"},
    {PadInput::kLsLeft, "ls_left", "Left stick left"},
    {PadInput::kLsRight, "ls_right", "Left stick right"},
    {PadInput::kRsUp, "rs_up", "Right stick up"},
    {PadInput::kRsDown, "rs_down", "Right stick down"},
    {PadInput::kRsLeft, "rs_left", "Right stick left"},
    {PadInput::kRsRight, "rs_right", "Right stick right"},
};
static_assert(sizeof kPadInputs / sizeof kPadInputs[0] == static_cast<size_t>(PadInput::kCount));

// Indexed by Layout, in enum order; the Controls dialog's layout combo lists
// the same three in the same order.
const char* const kLayoutNames[] = {"positional", "nintendo", "auto"};

bool EqualsNoCase(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; ++i) {
        const char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] + 32) : a[i];
        const char y = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] + 32) : b[i];
        if (x != y) return false;
    }
    return i == a.size() && b[i] == 0;
}

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// `text` split on `sep`, each piece split on the first '='; the callback gets
// (name, value) trimmed. Pieces without '=' are skipped.
template <typename F>
void ForEachItem(const std::string& text, char sep, F&& f) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(sep, start);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(start, end - start);
        const size_t eq = item.find('=');
        if (eq != std::string::npos) f(Trim(item.substr(0, eq)), Trim(item.substr(eq + 1)));
        start = end + 1;
    }
}

}  // namespace

const ActionInfo* Actions() { return kActions; }

unsigned short ParseActions(const std::string& text) {
    unsigned short bits = 0;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('+', start);
        if (end == std::string::npos) end = text.size();
        const std::string name = Trim(text.substr(start, end - start));
        if (name.empty()) return 0;
        bool found = false;
        for (const ActionInfo& a : kActions)
            if (EqualsNoCase(name, a.name)) {
                bits |= a.bit;
                found = true;
                break;
            }
        if (!found) return 0;
        start = end + 1;
    }
    return bits;
}

std::string FormatActions(unsigned short bits) {
    std::string out;
    for (const ActionInfo& a : kActions)
        if (bits & a.bit) {
            if (!out.empty()) out += '+';
            out += a.name;
        }
    return out.empty() ? "none" : out;
}

const KeyInfo* Keys() { return kKeys; }
int KeyCount() { return static_cast<int>(sizeof kKeys / sizeof kKeys[0]); }

int KeyFromName(const std::string& name) {
    for (const KeyInfo& k : kKeys)
        if (EqualsNoCase(name, k.name)) return k.dik;
    if (name.size() > 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        char* end = nullptr;
        const long v = std::strtol(name.c_str() + 2, &end, 16);
        if (end != name.c_str() + 2 && *end == 0 && v > 0 && v < 256) return static_cast<int>(v);
    }
    return -1;
}

std::string KeyName(int dik) {
    for (const KeyInfo& k : kKeys)
        if (k.dik == dik) return k.name;
    char buf[8];
    std::snprintf(buf, sizeof buf, "0x%02X", dik & 0xFF);
    return buf;
}

const PadInputInfo* PadInputs() { return kPadInputs; }

int PadInputFromName(const std::string& name) {
    for (const PadInputInfo& p : kPadInputs)
        if (EqualsNoCase(name, p.name)) return static_cast<int>(p.input);
    return -1;
}

const char* PadInputName(PadInput input) { return kPadInputs[static_cast<int>(input)].name; }

const char* LayoutName(Layout layout) { return kLayoutNames[static_cast<int>(layout)]; }

int LayoutFromName(const std::string& name) {
    for (int i = 0; i < 3; ++i)
        if (EqualsNoCase(name, kLayoutNames[i])) return i;
    return -1;
}

Bindings Bindings::Defaults() {
    Bindings b;
    // Key_TableDefault 0x66C648 in order (docs/controls.md section 1).
    b.keys = {
        {0xC8, kUp},        {0xD0, kDown},         {0xCB, kLeft},          {0xCD, kRight},
        {0x2C, kTriangle},  {0x2D, kCross},        {0x2E, kSquare},        {0x2F, kCircle},
        {0x1E, kL1},        {0x10, kL2},           {0x1F, kR1},            {0x11, kR2},
        {0x1C, kStart},     {0x36, kSelect},       {0x01, kCross},         {0x39, kCircle},
        {0x48, kUp},        {0x50, kDown},         {0x4B, kLeft},          {0x4D, kRight},
        {0x49, kUp | kRight}, {0x4F, kDown | kLeft}, {0x47, kUp | kLeft},  {0x51, kRight | kDown},
    };
    b.pad = {
        {PadInput::kSouth, kCross},    {PadInput::kEast, kCircle},     {PadInput::kWest, kSquare},
        {PadInput::kNorth, kTriangle}, {PadInput::kLb, kL1},           {PadInput::kRb, kR1},
        {PadInput::kLt, kL2},          {PadInput::kRt, kR2},           {PadInput::kStart, kStart},
        {PadInput::kBack, kSelect},    {PadInput::kDpadUp, kUp},       {PadInput::kDpadDown, kDown},
        {PadInput::kDpadLeft, kLeft},  {PadInput::kDpadRight, kRight}, {PadInput::kLsUp, kUp},
        {PadInput::kLsDown, kDown},    {PadInput::kLsLeft, kLeft},     {PadInput::kLsRight, kRight},
    };
    b.layout = Layout::kPositional;
    return b;
}

bool Bindings::operator==(const Bindings& o) const {
    if (layout != o.layout || keys.size() != o.keys.size() || pad.size() != o.pad.size()) return false;
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i].dik != o.keys[i].dik || keys[i].bits != o.keys[i].bits) return false;
    for (size_t i = 0; i < pad.size(); ++i)
        if (pad[i].input != o.pad[i].input || pad[i].bits != o.pad[i].bits) return false;
    return true;
}

int ParseKeys(const std::string& text, char sep, std::vector<KeyBinding>& out) {
    int n = 0;
    ForEachItem(text, sep, [&](const std::string& name, const std::string& value) {
        const int dik = KeyFromName(name);
        const unsigned short bits = ParseActions(value);
        if (dik < 0 || (bits == 0 && !EqualsNoCase(value, "none"))) return;
        if (bits != 0) out.push_back({static_cast<unsigned char>(dik), bits});
        ++n;
    });
    return n;
}

int ParsePad(const std::string& text, char sep, std::vector<PadBinding>& out, Layout& layout) {
    int n = 0;
    ForEachItem(text, sep, [&](const std::string& name, const std::string& value) {
        if (EqualsNoCase(name, "layout")) {
            const int l = LayoutFromName(value);
            if (l < 0) return;
            layout = static_cast<Layout>(l);
            ++n;
            return;
        }
        const int input = PadInputFromName(name);
        const unsigned short bits = ParseActions(value);
        if (input < 0 || (bits == 0 && !EqualsNoCase(value, "none"))) return;
        if (bits != 0) out.push_back({static_cast<PadInput>(input), bits});
        ++n;
    });
    return n;
}

std::string FormatKeys(const std::vector<KeyBinding>& keys, const char* prefix, const char* sep) {
    std::string out;
    for (const KeyBinding& k : keys) out += prefix + KeyName(k.dik) + "=" + FormatActions(k.bits) + sep;
    return out;
}

std::string FormatPad(const std::vector<PadBinding>& pad, Layout layout, const char* prefix, const char* sep) {
    std::string out = std::string(prefix) + "layout=" + LayoutName(layout) + sep;
    for (const PadBinding& p : pad) out += prefix + std::string(PadInputName(p.input)) + "=" + FormatActions(p.bits) + sep;
    return out;
}

}  // namespace bof3x::input
