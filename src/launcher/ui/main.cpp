// bof3x-launcher-ui: the drawn launcher (docs/launcher-portable.md, stages P1
// to P4). One set of windows for Windows and Linux, drawn through
// launcher/ui/ui.h. A second program beside the player's launcher until the
// cutover (stage P6).
//
//   bof3x-launcher-ui [--game <folder>] [--scale <number>] [--script <steps>] [-- <arguments for the game>]
//   bof3x-launcher-ui --check [--start] [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]
//                     [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]
//                     [--scale <number>]
//
// The start, as launcher.cpp's (lines 418-500): the game folder found, the
// settings read (launcher/start.h), then the Sources page first when there is
// no install, or a game folder to set up in place (StartReadSettings's
// StartSetUp) - closed with no build, nothing starts; built, the game folder
// and the cache are found again and the settings read again. Then the check
// of an install made by an older release (the Sources page's
// SourcesCheckStale): stale, the player is asked; no starts nothing, yes opens
// the Sources page with the rebuild. Locate ffmpeg's choice is saved whatever
// the page did. Then the window: the title page and Configure. Play applies
// the pages, saves bof3x.ini and starts the game by the steps of the launcher
// with no window. Back applies the pages; an Exit after a Back saves them and
// starts nothing; an Exit or the close box with no Back saves nothing
// (docs/launcher-settings.md 2a, "What is saved"). Data's Sources... opens the
// Sources page; after a build there the pages are applied and saved, the game
// folder and the cache found again, the settings read again, and the window
// shown anew on Data (the Win32 window's kDialogSourcesChanged).
//
// --check is the same window with no window on the screen and no game: a
// check of a page needs no screen, no hands and, without --game, no game
// folder. The settings are read from --ini (none: the defaults) and are
// written to --save where the window would save bof3x.ini - on Play, on an
// Exit after a Back, and after the Sources page; once written there, they are
// read again from --save. Without --start the check reads --ini as it is and
// takes --game as the game folder; --start runs the window's start above
// (StartFindGame, StartReadSettings on --ini, the Sources page first, the
// stale check). No question, picker or link is shown: the steps answer them.
// Each tool started is said on stdout with its process id and arguments.
// --capture writes the last frame as a PNG (and means --check). The line on
// stdout says the face the window is on, what it asked and whether it saved.
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
//   pick [<path>]                  the picker a press opened gets that path as the player's choice
//                                  (none: the player chose nothing)
//   answer yes|no                  the question asked (the rebuild, Stop) gets that answer
//   wait [<seconds>]               frames until no tool runs (the importer, tools/mods.py); the limit,
//                                  1800 s if not given, ends the tools and the program with exit code 1
//   report sources|mods            the page as text on stdout: its list, status and log
//   report saves                   the save editor as text (stage P5): its boxes, fields, notes, messages and questions
//   game-program <file>|-          the save editor asks whether a program of this file name runs, in place of
//                                  the game's (bof3x.exe; bof3x on Linux): a stand-in for "a game is running"
//   game-window <class>|-          the same for the window class it asks for on Windows (Bof3)
// A step that names a widget the page does not draw, or a grey one, stops the
// program with exit code 1: a grey control is a thing a check can ask for. So
// do a bind step with no capture open, a red step the widget does not meet, a
// pick with no picker open and an answer with no question.
// Without --check the steps run in the window on the screen ("press Play"
// starts the game), so a script starts the game through the window.
//
// Its arguments are UTF-8 on both systems (SDL's main). The texts are the
// player's (CLAUDE.md, ASD-STE100).
#include "common/home.h"
#include "input/bindings.h"
#include "input/key_sdl.h"
#include "launcher/config.h"
#include "launcher/start.h"
#include "launcher/ui/frame.h"
#include "launcher/ui/pages.h"
#include "launcher/ui/tool_run.h"
#include "launcher/ui/ui.h"

#include "os/clock.h"
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
    "       bof3x-launcher-ui --check [--start] [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]\n"
    "                         [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]\n"
    "                         [--scale <number>]\n"
    "  --game <folder>       The folder of the game. Without it, the launcher finds the folder as bof3x-launcher does.\n"
    "  --scale <number>      The size of the window: 1 is 640 x 480. Without it, the launcher uses the scale of the display.\n"
    "  --check               Shows no window and starts no game. Use it to check a page.\n"
    "  --start               With --check: first do the start of the window (find the game, read --ini, the Sources\n"
    "                        page for a first run, the check of an older install).\n"
    "  --ini <file>          With --check: the settings file to read. Without it, the settings are the defaults.\n"
    "  --save <file>         With --check: the file that gets the settings when the window saves them.\n"
    "  --script <steps>      Steps to do in the window, with ; between them: open <page> [<sub> [<sheet>]],\n"
    "                        set <widget>=<value>, press <widget>, key <name>, shot <file.png>,\n"
    "                        bind-key <key>, bind-pad <pad input>, red <widget>=<0 or 1>, pick [<path>],\n"
    "                        answer yes|no, wait [<seconds>], report sources|mods|saves,\n"
    "                        game-program <file>|-, game-window <class>|-.\n"
    "  --page <name>         With --check: the page to show first: title, settings, controls, music, game, mods or data.\n"
    "  --sub <n>, --sheet <n>  The sub-tab of that page and its page, counted from 0.\n"
    "  --keys <list>         With --check: keys to press, with , between them (the same as key steps).\n"
    "  --capture <file.png>  Writes the last frame as a picture. It also means --check.\n"
    "  --banner-font <file>  A font file for the letters of the banner, to try one.\n";

bool g_window_open = false;
bool g_check = false;        // --check: no window, message, question or picker on the screen
std::string g_banner_font;   // --banner-font
double g_scale = 0;          // --scale

[[noreturn]] void Leave(int code) {
    // No tool is left running behind the launcher (the Win32 Mods page's WM_DESTROY).
    ui::ToolsEnd();
    if (g_window_open) ui::Close();
    g_window_open = false;
    std::exit(code);
}

[[noreturn]] void Refuse(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher-ui: %s\n%s", sentence.c_str(), kUsage);
    Leave(1);
}

[[noreturn]] void Fail(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher-ui: %s\n", sentence.c_str());
    Leave(1);
}

void Say(const std::string& line) {
    std::printf("bof3x-launcher-ui: check: %s\n", line.c_str());
    std::fflush(stdout);
}

// The window, opened the first time a frame is owed (the window on the screen
// waits for the start's steps, which need none, so it does not show empty while
// the stale check runs); a check opens its hidden window at once.
void EnsureWindow() {
    if (g_window_open) return;
    ui::OpenOptions options;
    options.title = kTitle;
    options.scale = g_check ? (g_scale > 0 ? g_scale : 1) : g_scale;   // a check does not depend on the display it was made on
    options.hidden = g_check;
    if (!g_banner_font.empty()) options.banner_font = g_banner_font.c_str();
    std::string why;
    if (!ui::Open(options, &why)) {
        if (g_check) Fail("The window did not open. " + why);
        bof3x::StartDie(L"The window did not open. " + bof3::os::ToWide(why.c_str()));
    }
    g_window_open = true;
    if (!g_check) std::fprintf(stderr, "bof3x-launcher-ui: scale %g, renderer %s\n", ui::Scale(), ui::RendererName());
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

bool StartSwitchSet(const wchar_t* name) { return bof3::os::GetEnvW(name, nullptr, 0) > 0; }

// The window's loop, for the window on the screen and for --check alike, with
// the start's steps before it (launcher.cpp 418-500) as phases between frames.
struct Session {
    ui::FrameState state;
    ui::Pages pages;
    ui::FrameAsk ask = ui::FrameAsk::kNone;
    bool closed = false;   // the close box

    // The start.
    enum class Phase {
        kFind,      // the game folder found and the settings read: then the Sources page, the question or the window
        kAsk,       // the stale install's question, waiting for its answer (a check's)
        kSources,   // the Sources page shown
        kWindow,    // the title page and Configure
        kOver,      // the start ended with nothing to start: the Sources page closed with no build, or No
    };
    Phase phase = Phase::kFind;
    bool start = false;        // StartFindGame, StartReadSettings and the stale check (the window; --check --start)
    std::wstring named;        // --game or BOF3_GAME_DIR
    std::wstring read_ini, save_ini;
    bof3x::Config cfg;
    std::wstring game_dir, exe;
    bool exe_read = false;
    bool set_up_page = false;  // the Sources page was opened by the start, not by Data's Sources...
    bool reopen_data = false;  // the window is shown anew on Data (the Win32 g_reopen)
    ui::StaleCheck stale;
    std::string asked;         // the question last said on stdout (a check)

    bool Over() const { return closed || ask != ui::FrameAsk::kNone || phase == Phase::kOver; }

    // Where the window saves bof3x.ini; a check's --save, read again from then on.
    void Save(const char* what) {
        if (save_ini.empty()) return;
        if (!bof3x::ConfigSave(save_ini, cfg)) {
            bof3::os::PrintW(stderr, L"bof3x-launcher-ui: cannot write " + save_ini + L"; settings not saved\n");
            return;
        }
        if (g_check) {
            Say(std::string("saved the settings (") + what + ")");
            read_ini = save_ini;
        }
    }

    // The start's steps up to the next frame they need.
    void Advance() {
        for (;;) {
            switch (phase) {
            case Phase::kFind:
                Find();
                continue;
            case Phase::kAsk: {
                const std::string text = ui::SourcesRebuildText(stale);
                if (g_check && asked != text) {
                    asked = text;
                    Say(std::string("asked: ") + ui::kRebuildTitle);
                    for (const std::string& line : Split(text, '\n')) Say("  " + line);
                }
                const int answer = ui::Ask(ui::kRebuildTitle, text.c_str());
                if (answer < 0) {
                    EnsureWindow();   // a check: the steps answer it, between frames
                    return;
                }
                if (answer == 0) {   // No: the game does not start, and the install does not change
                    phase = Phase::kOver;
                    return;
                }
                OpenSources(false, true, std::wstring());
                return;
            }
            case Phase::kSources:
                if (pages.sources_open) return;
                pages.sources_closed = false;
                if (set_up_page) {
                    Save("the Sources page");   // Locate ffmpeg's choice, kept whatever happened
                    if (!pages.sources_built) {  // closed without an install: start nothing
                        phase = Phase::kOver;
                        return;
                    }
                    phase = Phase::kFind;
                    continue;
                }
                if (pages.sources_built) {
                    // A rebuilt install may be another game folder and cache: the
                    // pages are applied and saved, then found and read again.
                    ui::PagesApply(pages);
                    Save("the Sources page built the install");
                    reopen_data = true;
                    phase = Phase::kFind;
                    continue;
                }
                phase = Phase::kWindow;
                return;
            case Phase::kWindow:
                if (pages.sources_open) {   // Data's Sources...
                    set_up_page = false;
                    phase = Phase::kSources;
                }
                return;
            case Phase::kOver:
                return;
            }
        }
    }

    void OpenSources(bool first_run, bool rebuild, const std::wstring& folder) {
        EnsureWindow();
        pages.cfg = &cfg;
        ui::SourcesBegin(pages, first_run, rebuild, folder);
        set_up_page = true;
        phase = Phase::kSources;
        if (g_check)
            Say(std::string("the Sources page opened (") +
                (rebuild ? "rebuild" : !folder.empty() ? "set up the game folder in place" : first_run ? "first run" : "") + ")");
    }

    void Find() {
        cfg = bof3x::Config{};
        if (!start) {
            // A check without --start: --ini as it is, --game as the game folder.
            if (!read_ini.empty()) {
                std::string why;
                if (!bof3x::ConfigLoad(read_ini, cfg, why)) Fail("Cannot read " + bof3::os::ToUtf8(read_ini.c_str()) + ".");
                if (!why.empty()) Fail(why);
            }
            game_dir = named;
            ShowWindow();
            return;
        }
        exe_read = bof3x::StartFindGame(named, false, &game_dir, &exe);
        const bof3x::StartSetUp set_up = bof3x::StartReadSettings(read_ini, game_dir, named, false, exe_read, 1, cfg);
        if (set_up != bof3x::StartSetUp::kNone) {
            // The Sources page first (start.h StartSetUpFor): its first run, or
            // this game folder set up in place - the page adds it as a source,
            // and the importer, not the launcher, says whether it is the PC
            // game; then the cache's image is built from it and the game
            // starts from the same folder.
            OpenSources(true, false, set_up == bof3x::StartSetUp::kThisFolder ? game_dir : std::wstring());
            return;
        }
        // HANDOFF item 11 (docs/install-staleness.md): the home's install against
        // this release's importer, piece by piece. Only the install the Sources
        // page made and can make again - its game folder and its cache, no other
        // cache named - and never under a self-test. Stale: the player is asked.
        const std::wstring home_game = bof3x::HomeGameDefault();
        if (!bof3x::StartSelfTestOnly() && named.empty() && cfg.cache.empty() && !StartSwitchSet(L"BOF3X_CACHE") &&
            !StartSwitchSet(L"BOF3X_EXEIMAGE") && !home_game.empty() && game_dir == bof3x::StartFullPath(home_game)) {
            stale = ui::SourcesCheckStale();
            if (!stale.error.empty()) std::fprintf(stderr, "bof3x-launcher-ui: %s\n", stale.error.c_str());
            if (g_check) Say(stale.stale ? "the install is stale" : "the install is not stale");
            if (stale.stale) {
                phase = Phase::kAsk;
                return;
            }
        }
        ShowWindow();
    }

    void ShowWindow() {
        EnsureWindow();
        ui::PagesOpen(pages, game_dir, cfg);
        if (reopen_data) {
            ui::FrameOpen(state, "data");
            reopen_data = false;
        }
        phase = Phase::kWindow;
    }

    void Frames(int count) {
        for (int i = 0; i < count; ++i) {
            Advance();
            if (Over()) return;
            if (!ui::FrameBegin()) {
                closed = true;
                return;
            }
            if (phase != Phase::kAsk) ask = ui::FrameDraw(state, pages);   // the question: nothing drawn under it
            ui::FrameEnd();
            Advance();
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
        } else if (verb == "pick") {
            for (int i = 0; i < 8 && !ui::TestPicking() && !Over(); ++i) Frames(1);
            if (!ui::TestPicking()) Fail("No picker is open. The step \"" + step + "\" did nothing.");
            ui::TestPick(rest);
            Frames(3);
        } else if (verb == "answer") {
            if (rest != "yes" && rest != "no") Refuse("The step \"" + step + "\" needs yes or no.");
            for (int i = 0; i < 8 && ui::TestAsked().empty() && !Over(); ++i) Frames(1);
            if (ui::TestAsked().empty()) Fail("No question is asked. The step \"" + step + "\" did nothing.");
            Say("answered " + rest + ": " + ui::TestAsked().substr(0, ui::TestAsked().find('\n')));
            ui::TestAnswer(rest == "yes");
            Frames(3);
        } else if (verb == "wait") {
            const double limit = rest.empty() ? 1800 : std::atof(rest.c_str());
            if (limit <= 0) Refuse("The step \"" + step + "\" needs a number of seconds.");
            const std::uint64_t t0 = bof3::os::TickMs64();
            Frames(3);
            while (ui::ToolsBusy() && !Over()) {
                if (static_cast<double>(bof3::os::TickMs64() - t0) > limit * 1000) {
                    const int ended = ui::ToolsEnd();
                    Fail("The tools ran for more than " + rest + " s (" + std::to_string(ended) + " ended). The step \"" +
                         step + "\" stopped the check.");
                }
                ui::Rest(100);
                Frames(1);
            }
            Frames(3);
            char took[64];
            std::snprintf(took, sizeof took, "waited %.1f s for the tools", static_cast<double>(bof3::os::TickMs64() - t0) / 1000.0);
            Say(took);
        } else if (verb == "report") {
            std::string text;
            if (rest == "sources") text = ui::SourcesText();
            else if (rest == "mods") text = ui::ModsText();
            else if (rest == "saves") text = ui::SavesText();   // the save editor (stage P5)
            else Refuse("The step \"" + step + "\" needs sources, mods or saves.");
            for (const std::string& line : Split(text, '\n')) std::printf("bof3x-launcher-ui: report: %s\n", line.c_str());
            std::fflush(stdout);
        } else if (verb == "game-program" || verb == "game-window") {
            // The save editor's question "a game is running" (stage P5), asked for a stand-in.
            const std::string name = rest == "-" ? std::string() : rest;
            if (rest.empty()) Refuse("The step \"" + step + "\" needs a name, or - for none.");
            ui::SavesGameMarks(verb == "game-program" ? &name : nullptr, verb == "game-window" ? &name : nullptr);
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
    return s.closed                             ? "close"
           : s.ask == ui::FrameAsk::kPlay        ? "play"
           : s.ask == ui::FrameAsk::kExit        ? "exit"
           : s.phase == Session::Phase::kOver    ? "nothing (the start ended)"
                                                 : "nothing";
}

// --check: the window with no window on the screen and no game.
int Check(const std::wstring& game_dir, const std::string& ini, const std::string& save, const std::string& page,
          int sub, int sheet, const std::vector<std::string>& steps, const std::string& capture, bool start) {
    ui::ToolsSayStarts(true);
    Session s;
    if (!ui::FrameOpen(s.state, page.c_str())) Refuse("The page " + page + " is not known.");
    if (s.state.page >= 0) {
        s.state.sub[s.state.page] = sub;
        s.state.sheet[s.state.page] = sheet;
    }
    s.start = start;
    s.named = game_dir;
    s.read_ini = bof3::os::ToWide(ini.c_str());
    s.save_ini = bof3::os::ToWide(save.c_str());
    EnsureWindow();

    s.Frames(4);
    for (const std::string& step : steps) {
        if (s.Over()) break;
        s.Step(step);
    }
    std::string why;
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
        if (!s.game_dir.empty() && !bof3x::ConfigResolveLanguage(s.game_dir, s.cfg, why)) Fail(why);
        saved = true;
    } else if (s.closed && s.phase == Session::Phase::kSources && s.set_up_page) {
        saved = true;   // the close box on the start's Sources page: Locate ffmpeg's choice kept
    } else if (s.Over() && s.phase != Session::Phase::kOver && s.pages.applied) {
        saved = true;
    }
    if (saved && !save.empty() && !bof3x::ConfigSave(s.save_ini, s.cfg)) Fail("Cannot write " + save + ".");
    if (const int ended = ui::ToolsEnd(); ended > 0) Say("ended " + std::to_string(ended) + " running tool(s)");
    const int tab = s.state.page;
    std::printf("bof3x-launcher-ui: check (scale %g, renderer %s); page %d sub %d sheet %d%s, asked %s, %s\n",
                ui::Scale(), ui::RendererName(), tab, tab >= 0 ? s.state.sub[tab] : 0,
                tab >= 0 ? s.state.sheet[tab] : 0, s.pages.sources_open ? " (the Sources page shown)" : "",
                AskName(s), saved ? "saved the settings" : "saved nothing");
    ui::Close();
    g_window_open = false;
    return 0;
}

}  // namespace

// The start's refusals (launcher/start.h): on stderr, and in a message box
// unless this is a check.
[[noreturn]] void bof3x::StartDie(const std::wstring& message) {
    bof3::os::PrintW(stderr, L"bof3x-launcher-ui: " + message + L"\n");
    if (!g_check) ui::Message(kTitle, bof3::os::ToUtf8(message.c_str()).c_str(), true);
    Leave(1);
}

int main(int argc, char* argv[]) {
    std::wstring named;
    std::vector<std::wstring> game_args;
    std::string capture, page = "title", ini, save;
    std::vector<std::string> steps;
    bool check = false, start = false;
    int sub = 0, sheet = 0;
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
        } else if (arg == "--start") {
            start = true;
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
            g_scale = number(argv[++i], "--scale");
            if (g_scale < 0.5 || g_scale > 8) Refuse("--scale needs a number from 0.5 to 8.");
        } else if (arg == "--") {
            for (++i; i < argc; ++i) game_args.push_back(bof3::os::ToWide(argv[i]));
        } else {
            Refuse("The argument " + arg + " is not known.");
        }
    }
    g_check = check;
    if (check) return Check(named, ini, save, page, sub, sheet, steps, capture, start);
    if (!ini.empty() || !save.empty() || start) Refuse("--ini, --save and --start are for --check. The window uses bof3x.ini.");

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

    Session s;
    s.start = true;
    s.named = named;
    s.read_ini = ini_path;
    s.save_ini = ini_path;
    s.Advance();   // the start's steps: the window opens when the first frame is owed
    if (!s.Over()) s.Frames(4);
    for (const std::string& step : steps) {
        if (s.Over()) break;
        s.Step(step);
    }
    while (!s.Over()) s.Frames(1);
    // The tools ended, and the window closed before the game starts, which
    // opens the pad and its own window for itself (DIV-0050).
    ui::ToolsEnd();
    if (g_window_open) ui::Close();
    g_window_open = false;

    if (s.phase == Session::Phase::kOver) return 0;   // the start ended: what it saved is saved
    if (s.closed && s.phase == Session::Phase::kSources && s.set_up_page) {
        s.Save("the Sources page");   // Locate ffmpeg's choice, kept whatever happened
        return 0;
    }
    if (s.ask != ui::FrameAsk::kPlay) {
        // Exit, or the close box: after a Back, what the pages said is saved; nothing starts.
        if (s.pages.applied) s.Save("exit");
        return 0;
    }
    ui::PagesApply(s.pages);
    // DIV-0102: the window's choice of language, under the cache it may have changed.
    std::string why;
    if (!bof3x::ConfigResolveLanguage(s.game_dir, s.cfg, why)) bof3x::StartDie(bof3x::StartWide(why));
    s.Save("play");

    bof3x::StartApply(s.game_dir, s.cfg, false, s.exe_read);
    bool waited = false;
    const std::uint32_t code = bof3x::StartGame(ours, game_args, s.game_dir, nullptr, &waited);
    return waited ? static_cast<int>(code) : 0;
}
