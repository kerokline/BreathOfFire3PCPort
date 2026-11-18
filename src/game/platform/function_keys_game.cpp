// DIVERGENCE (function keys; numbered at the merge): the function keys' table
// in the game (game/function_keys_game.h). Before it, Game_WndProc compared
// each key message with F1, F5, F6, F7, F8, F9, F11, F12 and BOF3X_RUBY_KEY's
// key; now it asks this table, which the launcher's Controls tab can rebind to
// any key and to a pad input. With no BOF3X_FKEYS the table is the old keys,
// compared as before: by their virtual-key numbers, in the old order.
#include "game/function_keys_game.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "hook/log.h"
#include "input/bindings.h"
#include "input/function_keys.h"
#include "input/pad_sdl.h"
#include "os/os.h"

namespace {

namespace in = bof3x::input;

in::FnBindings g_table = in::FnBindings::Defaults();
bool g_loaded = false;
bool g_any_pad = false;
bool g_down[in::kFnActionCount];   // each action's pad input as the last pass read it

// The table as one line, for the log.
std::string Described(const in::FnBindings& b) {
    std::string line = in::FnFormat(b, ", ");
    if (line.size() >= 2) line.resize(line.size() - 2);
    return line;
}

}  // namespace

namespace bof3 {

void FunctionKeys_Load() {
    if (g_loaded) return;
    g_loaded = true;
    g_table = in::FnBindings::Defaults();
    // The row above the text's key (DIV-0155): BOF3X_RUBY_KEY = f1..f12 (any
    // case; default f4) or none, read here since this table holds it. A key
    // another action holds too is offered to them in kFnOrder, so f5, f8 and
    // f9 still leave the row without a key and f6 still takes F6 from the
    // quicksave, as when msgbox.cpp compared it after F5, F9 and F8.
    {
        char key[16] = "f4";
        const std::uint32_t n = os::GetEnv("BOF3X_RUBY_KEY", key, sizeof key);
        if (n >= sizeof key) Fatal("BOF3X_RUBY_KEY is %u characters: f1..f12 or none", n);
        for (char* c = key; *c; ++c)
            if (*c >= 'A' && *c <= 'Z') *c = static_cast<char>(*c - 'A' + 'a');
        const int k = in::FnRubyKey(key);
        if (k == -2) Fatal("BOF3X_RUBY_KEY is \"%s\": f1..f12 or none", key);
        g_table.key[static_cast<int>(in::FnAction::kRows)] = k;
    }
    char text[4096];
    const std::uint32_t n = os::GetEnv("BOF3X_FKEYS", text, sizeof text);
    if (n > 0) {
        const bool harness = os::GetEnv("BOF3X_INPUT", nullptr, 0) > 0 || os::GetEnv("BOF3X_RECORD", nullptr, 0) > 0;
        if (harness) {
            Log("fkeys       BOF3X_FKEYS not read: an input recipe (BOF3X_INPUT) or the recorder (BOF3X_RECORD) "
                "names the default keys, so the default layout stays");
        } else if (n >= sizeof text) {
            Log("fkeys       BOF3X_FKEYS is %u characters, over %u: not read, the default layout stays", n,
                static_cast<unsigned>(sizeof text - 1));
        } else {
            std::vector<std::string> problems;
            const int understood = in::FnParse(text, ',', g_table, &problems);
            for (const std::string& p : problems) Log("fkeys       BOF3X_FKEYS: %s - left out", p.c_str());
            Log("fkeys       BOF3X_FKEYS: %d items understood, %u left out", understood,
                static_cast<unsigned>(problems.size()));
        }
    }
    g_any_pad = false;
    for (int i = 0; i < in::kFnActionCount; ++i) {
        g_any_pad = g_any_pad || g_table.pad[i] >= 0;
        g_down[i] = true;   // held at the start counts once released
    }
    if (g_table != in::FnBindings::Defaults() || n > 0) Log("fkeys       %s", Described(g_table).c_str());
}

int FunctionKeys_ActionOfKey(unsigned vk, std::int32_t lparam) {
    FunctionKeys_Load();
    return in::FnActionOfKey(g_table, in::FnKeyOfMessage(vk, lparam));
}

unsigned FunctionKeys_PadPressed(bool allowed) {
    FunctionKeys_Load();
    if (!g_any_pad) return 0;
    in::PadSdl_Poll();
    unsigned fired = 0;
    bool taken[static_cast<int>(in::PadInput::kCount)] = {};
    for (in::FnAction a : in::kFnOrder) {
        const int i = static_cast<int>(a);
        const int input = g_table.pad[i];
        if (input < 0 || input >= static_cast<int>(in::PadInput::kCount)) continue;
        const bool down = in::PadSdl_InputDown(static_cast<in::PadInput>(input));
        const bool edge = down && !g_down[i];
        g_down[i] = down;
        if (!allowed || !edge || taken[input]) continue;
        taken[input] = true;   // of two actions on one input, the first in kFnOrder
        fired |= 1u << i;
        Log("fkeys       %s from the pad (%s)", in::FnActions()[i].name, in::PadInputName(static_cast<in::PadInput>(input)));
    }
    return fired;
}

}  // namespace bof3
