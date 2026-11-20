// bof3x-launcher-ui: the drawn launcher (docs/launcher-portable.md, stages P1
// to P3). One set of windows for Windows and Linux, drawn through
// launcher/ui/ui.h. A second program beside the player's launcher until the
// cutover (stage P6).
//
//   bof3x-launcher-ui [--game <folder>] [--scale <number>] [--script <steps>] [-- <arguments for the game>]
//   bof3x-launcher-ui --check [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]
//                     [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]
//                     [--scale <number>]
//
// The window: the title page and Configure. Play applies the pages, saves
// bof3x.ini and starts the game by the steps of the launcher with no window
// (launcher/start.h). Back applies the pages; an Exit after a Back saves them
// and starts nothing; an Exit or the close box with no Back saves nothing
// (docs/launcher-settings.md 2a, "What is saved"). It does not make an
// install (the Sources page is stage P4's) and does not look for an install
// made by an older release (the Win32 launcher's check, in its Sources
// page's file).
//
// --check is the same window with no window on the screen and no game: a
// check of a page needs no screen, no hands and, without --game, no game
// folder. The settings are read from --ini (none: the defaults) and are
// written to --save where the window would save bof3x.ini - on Play, and on
// an Exit after a Back. --capture writes the last frame as a PNG (and means
// --check). The line on stdout says the face the window is on, what it asked
// and whether it saved.
//
// --script is a list of steps with ; between them, done one after the other:
//   open <page> [<sub> [<sheet>]]  the face: title, or a tab (settings, controls, music, game, mods, data)
//                                  with its sub-tab and that sub-tab's page, counted from 0
//   set <widget>=<value>           a widget of the page shown, by its name (ui.h TestSet)
//   press <widget>                 a button, a tab or a radio button, by its name
//   key <name>                     a key or a pad button (ui.h TestKey)
//   shot <file.png>                the frame as it is now
//   bind-key <name>                the bind capture, open over a Controls cell, gets a key as if the
//                                  player pressed it: <name> is the key's name as the cells show it
//                                  (input/bindings.h KeyName: Z, Numpad8, RShift, F6, 0x5C for an unnamed
//                                  one), which names the physical key DirectInput gives that number
//                                  (input/key_sdl.h) - all 159 keys have one. #<n>: SDL's scancode n, a key with no name too
//   bind-pad <name>                the capture gets a pad input: its label as the cells show it
//                                  ("South (A / Cross)", "LB / L1") or its name in the files (south, lb)
//   red <widget>=<0 or 1>          the widget was drawn in the last frame, with red text (1) or not (0)
// A step that names a widget the page does not draw, or a grey one, stops the
// program with exit code 1: a grey control is a thing a check can ask for. So
// do a bind step with no capture open, and a red step the widget does not meet.
// Without --check the steps run in the window on the screen ("press Play"
// starts the game), so a script starts the game through the window.
//
// Its arguments are UTF-8 on both systems (SDL's main). The texts are the
// player's (CLAUDE.md, ASD-STE100).
#include "input/bindings.h"
#include "input/key_sdl.h"
#include "launcher/config.h"
#include "launcher/start.h"
#include "launcher/ui/frame.h"
#include "launcher/ui/pages.h"
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

namespace ui = bof3x::ui;

const char kTitle[] = "Breath of Fire III - bof3x";

const char kUsage[] =
    "usage: bof3x-launcher-ui [--game <folder>] [--scale <number>] [--script <steps>] [-- <arguments for the game>]\n"
    "       bof3x-launcher-ui --check [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]\n"
    "                         [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]\n"
    "                         [--scale <number>]\n"
    "  --game <folder>       The folder of the game. Without it, the launcher finds the folder as bof3x-launcher does.\n"
    "  --scale <number>      The size of the window: 1 is 640 x 480. Without it, the launcher uses the scale of the display.\n"
    "  --check               Shows no window and starts no game. Use it to check a page.\n"
    "  --ini <file>          With --check: the settings file to read. Without it, the settings are the defaults.\n"
    "  --save <file>         With --check: the file that gets the settings when the window saves them.\n"
    "  --script <steps>      Steps to do in the window, with ; between them: open <page> [<sub> [<sheet>]],\n"
    "                        set <widget>=<value>, press <widget>, key <name>, shot <file.png>,\n"
    "                        bind-key <key>, bind-pad <pad input>, red <widget>=<0 or 1>.\n"
    "  --page <name>         With --check: the page to show first: title, settings, controls, music, game, mods or data.\n"
    "  --sub <n>, --sheet <n>  The sub-tab of that page and its page, counted from 0.\n"
    "  --keys <list>         With --check: keys to press, with , between them (the same as key steps).\n"
    "  --capture <file.png>  Writes the last frame as a picture. It also means --check.\n"
    "  --banner-font <file>  A font file for the letters of the banner, to try one.\n";

bool g_window_open = false;
std::string g_banner_font;   // --banner-font

[[noreturn]] void Refuse(const std::string& sentence) {
    if (g_window_open) ui::Close();
    std::fprintf(stderr, "bof3x-launcher-ui: %s\n%s", sentence.c_str(), kUsage);
    std::exit(1);
}

[[noreturn]] void Fail(const std::string& sentence) {
    if (g_window_open) ui::Close();
    std::fprintf(stderr, "bof3x-launcher-ui: %s\n", sentence.c_str());
    std::exit(1);
}

std::vector<std::string> Split(const std::string& text, char at) {
    std::vector<std::string> parts;
    for (std::size_t from = 0; from <= text.size();) {
        std::size_t end = text.find(at, from);
        if (end == std::string::npos) end = text.size();
        const std::size_t b = text.find_first_not_of(' ', from);
        if (b != std::string::npos && b < end) {
            const std::size_t e = text.find_last_not_of(' ', end - 1);
            parts.push_back(text.substr(b, e - b + 1));
        }
        from = end + 1;
    }
    return parts;
}

// The window's loop, for the window on the screen and for --check alike.
struct Session {
    ui::FrameState state;
    ui::Pages pages;
    ui::FrameAsk ask = ui::FrameAsk::kNone;
    bool closed = false;   // the close box

    bool Over() const { return closed || ask != ui::FrameAsk::kNone; }

    void Frames(int count) {
        for (int i = 0; i < count && !Over(); ++i) {
            if (!ui::FrameBegin()) {
                closed = true;
                return;
            }
            ask = ui::FrameDraw(state, pages);
            ui::FrameEnd();
        }
    }

    // One step of --script. A page settles over a few frames (sizes measured
    // in one frame are used in the next), so each step ends with some.
    void Step(const std::string& step) {
        const std::size_t space = step.find(' ');
        const std::string verb = step.substr(0, space);
        const std::string rest = space == std::string::npos ? "" : step.substr(step.find_first_not_of(' ', space));
        if (verb == "open") {
            const std::vector<std::string> words = Split(rest, ' ');
            if (words.empty() || !ui::FrameOpen(state, words[0].c_str())) Refuse("The page in the step \"" + step + "\" is not known.");
            if (state.page >= 0) {
                state.sub[state.page] = words.size() > 1 ? std::atoi(words[1].c_str()) : 0;
                state.sheet[state.page] = words.size() > 2 ? std::atoi(words[2].c_str()) : 0;
            }
            Frames(3);
        } else if (verb == "set" || verb == "press") {
            const std::size_t eq = verb == "set" ? rest.find('=') : std::string::npos;
            if (verb == "set" && eq == std::string::npos) Refuse("The step \"" + step + "\" needs a widget and a value: set <widget>=<value>.");
            const std::string widget = verb == "set" ? rest.substr(0, eq) : rest;
            ui::TestSet(widget.c_str(), verb == "set" ? rest.substr(eq + 1).c_str() : "1");
            for (int i = 0; i < 8 && ui::TestPending() && !Over(); ++i) Frames(1);
            if (ui::TestPending())
                Fail(ui::TestGrey() ? "The widget \"" + widget + "\" is grey. The step \"" + step + "\" did nothing."
                                    : "The page does not show a widget \"" + widget + "\". The step \"" + step +
                                          "\" did nothing.");
            Frames(3);
        } else if (verb == "key") {
            if (!ui::TestKey(rest.c_str(), true)) Refuse("The key " + rest + " is not known.");
            Frames(2);
            ui::TestKey(rest.c_str(), false);
            Frames(3);
        } else if (verb == "bind-key" || verb == "bind-pad") {
            if (!ui::Capturing()) Fail("No bind capture is open. The step \"" + step + "\" did nothing.");
            if (verb == "bind-key") {
                int scancode = 0;
                if (rest.size() > 1 && rest[0] == '#') {
                    scancode = std::atoi(rest.c_str() + 1);
                } else {
                    const int dik = bof3x::input::KeyFromName(rest);
                    scancode = dik > 0 && dik < bof3x::input::kKeyStateBytes ? bof3x::input::ScancodeOfDik(dik) : 0;
                }
                if (scancode <= 0) Refuse("The key " + rest + " is not known.");
                ui::TestCaptureKey(scancode);
            } else {
                int input = -1;
                for (int i = 0; i < static_cast<int>(bof3x::input::PadInput::kCount); ++i)
                    if (rest == bof3x::input::PadInputs()[i].label || rest == bof3x::input::PadInputs()[i].name) input = i;
                if (input < 0) Refuse("The pad input " + rest + " is not known.");
                ui::TestCapturePad(input);
            }
            Frames(3);
        } else if (verb == "red") {
            const std::size_t eq = rest.find('=');
            if (eq == std::string::npos) Refuse("The step \"" + step + "\" needs a widget and a value: red <widget>=<0 or 1>.");
            const std::string widget = rest.substr(0, eq);
            const bool want = rest.substr(eq + 1) != "0";
            bool red = false;
            if (!ui::TestDrawn(widget.c_str(), &red)) Fail("The page does not show a widget \"" + widget + "\".");
            if (red != want)
                Fail("The widget \"" + widget + "\" is " + (red ? "red" : "not red") + ". The step \"" + step + "\" asked otherwise.");
        } else if (verb == "shot") {
            ui::SaveFrame(rest);
            Frames(1);
            std::string why;
            if (!Over() && !ui::FrameSaved(&why)) Fail("Cannot write " + rest + ". " + why);
        } else {
            Refuse("The step \"" + step + "\" is not known.");
        }
    }
};

const char* AskName(const Session& s) {
    return s.closed ? "close" : s.ask == ui::FrameAsk::kPlay ? "play" : s.ask == ui::FrameAsk::kExit ? "exit" : "nothing";
}

// --check: the window with no window on the screen and no game.
int Check(const std::wstring& game_dir, const std::string& ini, const std::string& save, const std::string& page,
          int sub, int sheet, const std::vector<std::string>& steps, const std::string& capture, double scale) {
    bof3x::Config cfg;
    if (!ini.empty()) {
        std::string why;
        if (!bof3x::ConfigLoad(bof3::os::ToWide(ini.c_str()), cfg, why)) Fail("Cannot read " + ini + ".");
        if (!why.empty()) Fail(why);
    }
    Session s;
    if (!ui::FrameOpen(s.state, page.c_str())) Refuse("The page " + page + " is not known.");
    if (s.state.page >= 0) {
        s.state.sub[s.state.page] = sub;
        s.state.sheet[s.state.page] = sheet;
    }
    ui::OpenOptions options;
    options.title = kTitle;
    options.scale = scale > 0 ? scale : 1;   // a check does not depend on the display it was made on
    options.hidden = true;
    if (!g_banner_font.empty()) options.banner_font = g_banner_font.c_str();
    std::string why;
    if (!ui::Open(options, &why)) Fail("The window did not open. " + why);
    g_window_open = true;
    ui::PagesOpen(s.pages, game_dir, cfg);

    s.Frames(4);
    for (const std::string& step : steps) {
        if (s.Over()) break;
        s.Step(step);
    }
    if (!capture.empty() && !s.Over()) {
        s.Frames(1);
        ui::SaveFrame(capture);
        s.Frames(1);
        if (!ui::FrameSaved(&why)) Fail("Cannot write " + capture + ". " + why);
    }

    // What the window on the screen does with the settings (main, below).
    bool saved = false;
    if (s.ask == ui::FrameAsk::kPlay) {
        ui::PagesApply(s.pages);
        if (!game_dir.empty() && !bof3x::ConfigResolveLanguage(game_dir, cfg, why)) Fail(why);
        saved = true;
    } else if (s.Over() && s.pages.applied) {
        saved = true;
    }
    if (saved && !save.empty() && !bof3x::ConfigSave(bof3::os::ToWide(save.c_str()), cfg)) Fail("Cannot write " + save + ".");
    const int tab = s.state.page;
    std::printf("bof3x-launcher-ui: check (scale %g, renderer %s); page %d sub %d sheet %d, asked %s, %s\n",
                ui::Scale(), ui::RendererName(), tab, tab >= 0 ? s.state.sub[tab] : 0,
                tab >= 0 ? s.state.sheet[tab] : 0, AskName(s), saved ? "saved the settings" : "saved nothing");
    ui::Close();
    g_window_open = false;
    return 0;
}

}  // namespace

// The start's refusals (launcher/start.h): on stderr and in a message box.
[[noreturn]] void bof3x::StartDie(const std::wstring& message) {
    bof3::os::PrintW(stderr, L"bof3x-launcher-ui: " + message + L"\n");
    ui::Message(kTitle, bof3::os::ToUtf8(message.c_str()).c_str(), true);
    if (g_window_open) ui::Close();
    std::exit(1);
}

int main(int argc, char* argv[]) {
    std::wstring named;
    std::vector<std::wstring> game_args;
    std::string capture, page = "title", ini, save;
    std::vector<std::string> steps;
    bool check = false;
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
        const bool more = i + 1 < argc;
        if (arg == "--game" && more) {
            named = bof3::os::ToWide(argv[++i]);
        } else if (arg == "--check") {
            check = true;
        } else if (arg == "--capture" && more) {
            capture = argv[++i];
            check = true;
        } else if (arg == "--banner-font" && more) {
            g_banner_font = argv[++i];
        } else if (arg == "--ini" && more) {
            ini = argv[++i];
        } else if (arg == "--save" && more) {
            save = argv[++i];
        } else if (arg == "--page" && more) {
            page = argv[++i];
        } else if (arg == "--script" && more) {
            for (const std::string& step : Split(argv[++i], ';')) steps.push_back(step);
        } else if (arg == "--keys" && more) {
            for (const std::string& key : Split(argv[++i], ',')) steps.push_back("key " + key);
        } else if (arg == "--sub" && more) {
            sub = static_cast<int>(number(argv[++i], "--sub"));
        } else if (arg == "--sheet" && more) {
            sheet = static_cast<int>(number(argv[++i], "--sheet"));
        } else if (arg == "--scale" && more) {
            scale = number(argv[++i], "--scale");
            if (scale < 0.5 || scale > 8) Refuse("--scale needs a number from 0.5 to 8.");
        } else if (arg == "--") {
            for (++i; i < argc; ++i) game_args.push_back(bof3::os::ToWide(argv[i]));
        } else {
            Refuse("The argument " + arg + " is not known.");
        }
    }
    if (check) return Check(named, ini, save, page, sub, sheet, steps, capture, scale);
    if (!ini.empty() || !save.empty()) Refuse("--ini and --save are for --check. The window uses bof3x.ini.");

    if (named.empty()) {
        wchar_t env[1024];
        const std::uint32_t n = bof3::os::GetEnvW(L"BOF3_GAME_DIR", env, 1024);
        if (n > 0 && n < 1024) named = env;
    }
    const std::wstring own = bof3x::StartOwnDirectory();
    const std::wstring ini_path = own + bof3x::StartSep(L"\\bof3x.ini");

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
    const bof3x::StartSetUp set_up = bof3x::StartReadSettings(ini_path, game_dir, named, false, exe_read, 1, cfg);
    if (set_up != bof3x::StartSetUp::kNone)
        bof3x::StartDie(L"This launcher cannot make an install yet. Start bof3x-launcher and make the install on "
                        L"its Sources page. Then start this launcher again.");

    ui::OpenOptions options;
    options.title = kTitle;
    options.scale = scale;
    if (!g_banner_font.empty()) options.banner_font = g_banner_font.c_str();
    std::string why;
    if (!ui::Open(options, &why))
        bof3x::StartDie(L"The window did not open. " + bof3::os::ToWide(why.c_str()));
    g_window_open = true;
    std::fprintf(stderr, "bof3x-launcher-ui: scale %g, renderer %s\n", ui::Scale(), ui::RendererName());

    Session s;
    ui::PagesOpen(s.pages, game_dir, cfg);
    s.Frames(4);
    for (const std::string& step : steps) {
        if (s.Over()) break;
        s.Step(step);
    }
    while (!s.Over()) s.Frames(1);
    // Closed before the game starts, which opens the pad and its own window for itself (DIV-0050).
    ui::Close();
    g_window_open = false;

    auto save_settings = [&] {
        if (!bof3x::ConfigSave(ini_path, cfg))
            bof3::os::PrintW(stderr, L"bof3x-launcher-ui: cannot write " + ini_path + L"; settings not saved\n");
    };
    if (s.ask != ui::FrameAsk::kPlay) {
        // Exit, or the close box: after a Back, what the pages said is saved; nothing starts.
        if (s.pages.applied) save_settings();
        return 0;
    }
    ui::PagesApply(s.pages);
    // DIV-0102: the window's choice of language, under the cache it may have changed.
    if (!bof3x::ConfigResolveLanguage(game_dir, cfg, why)) bof3x::StartDie(bof3x::StartWide(why));
    save_settings();

    bof3x::StartApply(game_dir, cfg, false, exe_read);
    bool waited = false;
    const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, nullptr, &waited);
    return waited ? static_cast<int>(code) : 0;
}
