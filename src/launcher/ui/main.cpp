// bof3x-launcher-ui: the drawn launcher (docs/launcher-portable.md, stage P1).
// One set of windows for Windows and Linux, drawn through launcher/ui/ui.h. A
// second program beside the player's launcher until the cutover (stage P6).
//
//   bof3x-launcher-ui [--game <folder>] [--scale <number>] [--keys <list>] [-- <arguments for the game>]
//   bof3x-launcher-ui --capture <file.png> [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--scale <number>]
//
// What stage P1 holds: the title page and Configure's tabs, with Play. Play
// starts the game as the launcher with no window does (launcher/start.h): the
// settings in bof3x.ini as they are, since no page changes one yet. It does
// not make an install (the Sources page is stage P4's) and does not look for
// an install made by an older release (the Win32 launcher's check, in its
// Sources page's file).
//
// --capture draws one face with no window on the screen and writes it as a
// PNG, so a check of a page needs no screen and no game folder: `--page` is
// "title" or a tab ("settings", "controls", "music", "game", "mods", "data"),
// `--sub` and `--sheet` the tab's sub-tab and its page, counted from 0.
// `--keys` presses keys and pad buttons before the picture, one after the
// other ("down,right,enter"; the names are ui.h TestKey's), and the line on
// stdout says the face the window is on then and what it asked, so a check
// of the keyboard and the pad needs no hands. Without --capture, `--keys`
// presses them in the window on the screen ("enter" is Play), so a script
// starts the game through the window.
//
// Its arguments are UTF-8 on both systems (SDL's main). The texts are the
// player's (CLAUDE.md, ASD-STE100).
#include "launcher/config.h"
#include "launcher/start.h"
#include "launcher/ui/frame.h"
#include "launcher/ui/ui.h"

#include "os/env.h"
#include "os/text.h"
#include "os/wide_text.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace {

const char kTitle[] = "Breath of Fire III - bof3x";

const char kUsage[] =
    "usage: bof3x-launcher-ui [--game <folder>] [--scale <number>] [--keys <list>] [-- <arguments for the game>]\n"
    "       bof3x-launcher-ui --capture <file.png> [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>]\n"
    "                         [--scale <number>]\n"
    "  --game <folder>       The folder of the game. Without it, the launcher finds the folder as bof3x-launcher does.\n"
    "  --scale <number>      The size of the window: 1 is 640 x 480. Without it, the launcher uses the scale of the display.\n"
    "  --capture <file.png>  Draws one page with no window and writes it as a picture, then stops.\n"
    "  --page <name>         The page to capture: title, settings, controls, music, game, mods or data.\n"
    "  --sub <n>, --sheet <n>  The sub-tab of the page and its page, counted from 0.\n"
    "  --keys <list>         Keys to press before the picture: up, down, left, right, enter, space, escape, tab,\n"
    "                        pad-up, pad-down, pad-left, pad-right, pad-cross, pad-circle, pad-start.\n";

bool g_window_open = false;

[[noreturn]] void Refuse(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher-ui: %s\n%s", sentence.c_str(), kUsage);
    std::exit(1);
}

// --keys' list, one name each.
std::vector<std::string> Keys(const std::string& keys) {
    std::vector<std::string> presses;
    for (std::size_t at = 0; at < keys.size();) {
        std::size_t end = keys.find(',', at);
        if (end == std::string::npos) end = keys.size();
        if (end > at) presses.push_back(keys.substr(at, end - at));
        at = end + 1;
    }
    return presses;
}

int Capture(const std::string& file, const std::string& page, int sub, int sheet, const std::string& keys,
            double scale) {
    bof3x::ui::FrameState state;
    if (!bof3x::ui::FrameOpen(state, page.c_str())) Refuse("The page " + page + " is not known.");
    if (state.page >= 0) {
        state.sub[state.page] = sub;
        state.sheet[state.page] = sheet;
    }
    const std::vector<std::string> presses = Keys(keys);
    bof3x::ui::OpenOptions options;
    options.title = kTitle;
    options.scale = scale > 0 ? scale : 1;   // a capture does not depend on the display it was made on
    options.hidden = true;
    std::string why;
    if (!bof3x::ui::Open(options, &why)) {
        std::fprintf(stderr, "bof3x-launcher-ui: The window did not open. %s\n", why.c_str());
        return 1;
    }
    // What the window asked, the first time it asked: a check reads it from the line below.
    bof3x::ui::FrameAsk asked = bof3x::ui::FrameAsk::kNone;
    // A page settles over its first frames (sizes measured in one frame are used in the next).
    auto frames = [&](int count, bool save) {
        for (int i = 0; i < count; ++i) {
            if (!bof3x::ui::FrameBegin()) return;
            const bof3x::ui::FrameAsk ask = bof3x::ui::FrameDraw(state);
            if (asked == bof3x::ui::FrameAsk::kNone) asked = ask;
            if (save && i == count - 1) bof3x::ui::SaveFrame(file);
            bof3x::ui::FrameEnd();
        }
    };
    frames(4, false);
    for (const std::string& key : presses) {
        if (!bof3x::ui::TestKey(key.c_str(), true)) {
            bof3x::ui::Close();
            Refuse("The key " + key + " is not known.");
        }
        frames(2, false);
        bof3x::ui::TestKey(key.c_str(), false);
        frames(3, false);
    }
    frames(2, true);
    const bool saved = bof3x::ui::FrameSaved(&why);
    if (saved) {
        const int tab = state.page;
        std::printf("bof3x-launcher-ui: wrote %s (scale %g, renderer %s); page %d sub %d sheet %d, asked %s\n",
                    file.c_str(), bof3x::ui::Scale(), bof3x::ui::RendererName(), tab, tab >= 0 ? state.sub[tab] : 0,
                    tab >= 0 ? state.sheet[tab] : 0,
                    asked == bof3x::ui::FrameAsk::kPlay ? "play" : asked == bof3x::ui::FrameAsk::kExit ? "exit" : "nothing");
    } else {
        std::fprintf(stderr, "bof3x-launcher-ui: Cannot write %s. %s\n", file.c_str(), why.c_str());
    }
    bof3x::ui::Close();
    return saved ? 0 : 1;
}

}  // namespace

// The start's refusals (launcher/start.h): on stderr and in a message box.
[[noreturn]] void bof3x::StartDie(const std::wstring& message) {
    bof3::os::PrintW(stderr, L"bof3x-launcher-ui: " + message + L"\n");
    bof3x::ui::Message(kTitle, bof3::os::ToUtf8(message.c_str()).c_str(), true);
    if (g_window_open) bof3x::ui::Close();
    std::exit(1);
}

int main(int argc, char* argv[]) {
    std::wstring named;
    std::vector<std::wstring> game_args;
    std::string capture, page = "title", keys;
    int sub = 0, sheet = 0;
    double scale = 0;
    auto number = [](const char* text, const char* what) {
        char* end = nullptr;
        const double v = std::strtod(text, &end);
        if (end == text || *end || v < 0) Refuse(std::string(what) + " needs a number.");
        return v;
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--game" && i + 1 < argc) {
            named = bof3::os::ToWide(argv[++i]);
        } else if (arg == "--capture" && i + 1 < argc) {
            capture = argv[++i];
        } else if (arg == "--page" && i + 1 < argc) {
            page = argv[++i];
        } else if (arg == "--keys" && i + 1 < argc) {
            keys = argv[++i];
        } else if (arg == "--sub" && i + 1 < argc) {
            sub = static_cast<int>(number(argv[++i], "--sub"));
        } else if (arg == "--sheet" && i + 1 < argc) {
            sheet = static_cast<int>(number(argv[++i], "--sheet"));
        } else if (arg == "--scale" && i + 1 < argc) {
            scale = number(argv[++i], "--scale");
            if (scale < 0.5 || scale > 8) Refuse("--scale needs a number from 0.5 to 8.");
        } else if (arg == "--") {
            for (++i; i < argc; ++i) game_args.push_back(bof3::os::ToWide(argv[i]));
        } else {
            Refuse("The argument " + arg + " is not known.");
        }
    }
    if (!capture.empty()) return Capture(capture, page, sub, sheet, keys, scale);

    if (named.empty()) {
        wchar_t env[1024];
        const std::uint32_t n = bof3::os::GetEnvW(L"BOF3_GAME_DIR", env, 1024);
        if (n > 0 && n < 1024) named = env;
    }
    const std::wstring own = bof3x::StartOwnDirectory();
    const std::wstring ini = own + bof3x::StartSep(L"\\bof3x.ini");

    // The game: the program beside this one, with this one's ending (headless.cpp).
    std::string self;
    if (!bof3::os::ExecutablePath(&self)) bof3x::StartDie(L"Cannot find the folder of this launcher.");
    const bool dot_exe = self.size() > 4 && self.compare(self.size() - 4, 4, ".exe") == 0;
    const std::wstring ours = own + bof3x::StartSep(dot_exe ? L"\\bof3x.exe" : L"\\bof3x");
    if (wchar_t base[32]; bof3::os::GetEnvW(L"BOF3X_IMAGEBASE", base, 32) > 0) {
        wchar_t* end = nullptr;
        if (std::wcstoul(base, &end, 0) != 0x400000 || !end || *end)
            bof3x::StartDie(L"BOF3X_IMAGEBASE asks for a moved image. This launcher does not start one. Remove "
                            L"BOF3X_IMAGEBASE or set it to 0x400000.");
    }
    if (!bof3x::StartFileExists(ours))
        bof3x::StartDie(ours + L" is not there. It belongs next to this launcher.");

    std::wstring game_dir, exe;
    bof3x::Config cfg;
    const bool exe_read = bof3x::StartFindGame(named, false, &game_dir, &exe);
    const bof3x::StartSetUp set_up = bof3x::StartReadSettings(ini, game_dir, named, false, exe_read, 1, cfg);
    if (set_up != bof3x::StartSetUp::kNone)
        bof3x::StartDie(L"This launcher cannot make an install yet. Start bof3x-launcher and make the install on "
                        L"its Sources page. Then start this launcher again.");

    bof3x::ui::OpenOptions options;
    options.title = kTitle;
    options.scale = scale;
    std::string why;
    if (!bof3x::ui::Open(options, &why))
        bof3x::StartDie(L"The window did not open. " + bof3::os::ToWide(why.c_str()));
    g_window_open = true;
    std::fprintf(stderr, "bof3x-launcher-ui: scale %g, renderer %s\n", bof3x::ui::Scale(), bof3x::ui::RendererName());

    bof3x::ui::FrameState state;
    bof3x::ui::FrameAsk ask = bof3x::ui::FrameAsk::kNone;
    // --keys: after six frames, each key held for two frames and let go for four.
    const std::vector<std::string> presses = Keys(keys);
    for (int frame = 0; ask == bof3x::ui::FrameAsk::kNone; ++frame) {
        if (const int at = frame / 6 - 1; at >= 0 && at < static_cast<int>(presses.size())) {
            const std::string& key = presses[static_cast<std::size_t>(at)];
            if (frame % 6 == 0 && !bof3x::ui::TestKey(key.c_str(), true)) Refuse("The key " + key + " is not known.");
            if (frame % 6 == 2) bof3x::ui::TestKey(key.c_str(), false);
        }
        if (frame > 1 << 20) frame = 1 << 10;   // a window left open for days: past every key, and no overflow
        if (!bof3x::ui::FrameBegin()) {   // the close box: start nothing
            ask = bof3x::ui::FrameAsk::kExit;
            break;
        }
        ask = bof3x::ui::FrameDraw(state);
        bof3x::ui::FrameEnd();
    }
    // Closed before the game starts, which opens the pad and its own window for itself (DIV-0050).
    bof3x::ui::Close();
    g_window_open = false;
    if (ask != bof3x::ui::FrameAsk::kPlay) return 0;

    bof3x::StartApply(game_dir, cfg, false, exe_read);
    bool waited = false;
    const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, nullptr, &waited);
    return waited ? static_cast<int>(code) : 0;
}
