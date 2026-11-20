// bof3x-launcher: the player's launcher, drawn (docs/launcher-portable.md,
// stages P1 to P6). One set of windows for Windows and Linux, drawn through
// launcher/ui/ui.h. Since the cutover (stage P6, 2026-11-20) it is
// bof3x-launcher on both systems, and the Win32 dialogs are gone; it takes the
// Win32 launcher's command line and start as they were, so every harness's
// command keeps its meaning (docs/launcher-settings.md section 4).
//
//   bof3x-launcher [--game <folder>] [--config|--no-config] [--host exe|bof3] [--scale <number>]
//                  [--script <steps>] [-- <arguments for the game>]
//   bof3x-launcher [--game <folder>] [--set <key>=<value>]... [--list]
//   bof3x-launcher --check [--start] [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]
//                  [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]
//                  [--scale <number>]
//
// The game folder: --game, BOF3_GAME_DIR, then StartFindGame (launcher/start.h).
// The window: neither --config nor --no-config, bof3x.ini's show_launcher
// decides (1, the default: the window); --config shows it whatever the file
// says; --no-config shows none. With no window, and under a self-test
// (BOF3X_SELFTEST_ONLY), the game starts with the settings as they are and
// nothing on the screen: no Sources page and no stale-install question under
// --no-config or a self-test (StartSetUpFor; the stale check), as the Win32
// launcher did. With show_launcher=0 and no --no-config, the Sources page's
// first run and the stale question still come first, as they did there.
//
// The hosted build (--host bof3, or a switch only it serves: BOF3X_ORIGINAL,
// BOF3X_SHADOW, BOF3X_CALLTRACE, BOF3X_MP3_COMPARE, BOF3X_MP3=original) and a
// moved image (BOF3X_IMAGEBASE other than 0x400000) are the developer
// launcher's, bof3x-launcher-dev.exe (src/launcher/launcher.cpp, Windows and
// 32-bit): this launcher holds no code that writes into another process. It
// hands such a run to the developer launcher beside it, with the same
// arguments, environment and redirected output, and ends with its exit code
// (os/process.h own_output). With none beside it (a release), or on Linux,
// where there is no hosted build, it stops with a sentence.
//
// --set and --list change one setting in bof3x.ini or show the settings, and
// stop: no window, no game (the launcher with no window of stage P0 had them).
//
// A refusal (StartDie) is said on stderr and ends the launcher with exit code
// 1. It is also shown in a message box, but only where a player can be looking
// and no script waits (docs/HANDOFF.md, Traps): not under --check, not under
// --no-config, not under a self-test, and not when stdout and stderr both go
// to a file or a pipe (os/process.h OutputRedirected). That covers a failed
// hand-over to the developer launcher in a scripted run.
//
// The start with the window, as launcher.cpp's was (its lines 418-500): the game folder found, the
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
//   click <x> <y>                  the mouse's left button pressed and let go at that place of the window, in the
//                                  picture's pixels (640 x 480): what a widget makes of a real click
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
//   report jukebox                 the Jukebox as text: its sources, note, songs (grey ones with why),
//                                  status and battle-chord box (stage P5)
//   rest <seconds>                 frames for that long, as the window idles: a song plays on (stage P5)
//   report saves                  the save editor as text (stage P5): its boxes, fields, notes, messages and questions
//   game-program <file>|-          the save editor asks whether a program of this file name runs, in place of
//                                  the game's (bof3x.exe; bof3x on Linux): a stand-in for "a game is running"
//   game-window <class>|-          the same for the window class it asks for on Windows (Bof3)
// The Jukebox in a check plays to SDL's dummy sound driver whatever the
// environment says, so a check never sounds; --samples <file> also writes the
// samples it hands to the sound into that file (16-bit stereo, 44,100 Hz, no
// header) for the proof against the Win32 player (stage P5).
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
#include "os/process.h"
#include "os/text.h"
#include "os/wide_file.h"
#include "os/wide_text.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace ui = bof3x::ui;

const char kTitle[] = "Breath of Fire III - bof3x";

const char kUsage[] =
    "usage: bof3x-launcher [--game <folder>] [--config|--no-config] [--host exe|bof3] [--scale <number>]\n"
    "                      [--script <steps>] [-- <arguments for the game>]\n"
    "       bof3x-launcher [--game <folder>] [--set <key>=<value>]... [--list]\n"
    "       bof3x-launcher --check [--start] [--ini <file>] [--game <folder>] [--save <file>] [--script <steps>]\n"
    "                      [--page <name>] [--sub <n>] [--sheet <n>] [--keys <list>] [--capture <file.png>]\n"
    "                      [--scale <number>]\n"
    "  --game <folder>       The folder of the game. Without it, the launcher uses BOF3_GAME_DIR, then the current\n"
    "                        folder if it is a game folder, then the game folder of the home, then the PC game\n"
    "                        folder that the home plays in place.\n"
    "  --config              Shows the window. Without --config and --no-config, show_launcher in bof3x.ini decides.\n"
    "  --no-config           Shows no window and no message. The game starts with the settings in bof3x.ini.\n"
    "  --host exe|bof3       exe starts bof3x (the default). bof3 starts the hosted build, which the developer\n"
    "                        launcher (bof3x-launcher-dev.exe) starts on Windows.\n"
    "  --set <key>=<value>   Changes one setting in bof3x.ini, then stops. You can use --set more than one time.\n"
    "  --list                Shows the settings in bof3x.ini, then stops.\n"
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
    "  --banner-font <file>  A font file for the letters of the banner, to try one.\n"
    "  --samples <file>      With --check: writes the samples that the Jukebox plays into the file.\n"
    "                        Steps for the Jukebox: report jukebox, rest <seconds>.\n";

bool g_window_open = false;
bool g_check = false;        // --check: no window, message, question or picker on the screen
bool g_no_box = false;       // a refusal shows no message box (the rule in the header above)
std::string g_banner_font;   // --banner-font
double g_scale = 0;          // --scale
std::string g_samples;       // --samples (stage P5)

[[noreturn]] void Leave(int code) {
    // No tool is left running behind the launcher (the Win32 Mods page's WM_DESTROY).
    ui::ToolsEnd();
    if (g_window_open) ui::Close();
    g_window_open = false;
    std::exit(code);
}

[[noreturn]] void Refuse(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher: %s\n%s", sentence.c_str(), kUsage);
    Leave(1);
}

[[noreturn]] void Fail(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher: %s\n", sentence.c_str());
    Leave(1);
}

void Say(const std::string& line) {
    std::printf("bof3x-launcher: check: %s\n", line.c_str());
    std::fflush(stdout);
}

// --list: the setting lines of bof3x.ini as ConfigSave writes them, with no
// comment and no section (the launcher with no window's, stage P0).
void List(const bof3x::Config& cfg) {
    const std::string text = bof3x::ConfigText(cfg);
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        std::printf("%s\n", line.c_str());
    }
}

// The switches that need Capcom's code, which only the hosted build maps
// (src/hook/exe_host.cpp RefuseHostedSwitches; launcher.cpp's HostedOnlyAsked
// before stage P6).
bool HostedOnlyAsked() {
    for (const wchar_t* name : {L"BOF3X_ORIGINAL", L"BOF3X_SHADOW", L"BOF3X_CALLTRACE", L"BOF3X_MP3_COMPARE"})
        if (bof3::os::GetEnvW(name, nullptr, 0) > 0) return true;
    wchar_t mp3[16] = {};
    const std::uint32_t n = bof3::os::GetEnvW(L"BOF3X_MP3", mp3, 16);
    return n > 0 && n < 16 && std::wcscmp(mp3, L"original") == 0;
}

// The hosted build and a moved image are the developer launcher's
// (launcher.cpp's ForwardToDevLauncher before stage P6): the whole run handed
// to bof3x-launcher-dev.exe beside this one, with the same arguments and
// environment, on this launcher's own file or pipe when a script gave one
// (os/process.h own_output), and this launcher ends with its exit code. A
// release ships no developer launcher, and Linux has no hosted build: there
// it stops here.
[[noreturn]] void ForwardToDevLauncher(int argc, char** argv, const std::wstring& own, bool windows) {
    if (!windows)
        bof3x::StartDie(L"--host bof3, the hosted-only switches (BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_CALLTRACE, "
                        L"BOF3X_MP3_COMPARE, BOF3X_MP3=original) and a moved image (BOF3X_IMAGEBASE) need the hosted "
                        L"build. The hosted build is on Windows only. This launcher starts bof3x (--host exe).");
    const std::wstring dev = own + bof3x::StartSep(L"\\bof3x-launcher-dev.exe");
    if (!bof3x::StartFileExists(dev))
        bof3x::StartDie(L"--host bof3, the hosted-only switches (BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_CALLTRACE, "
                        L"BOF3X_MP3_COMPARE, BOF3X_MP3=original) and a moved image (BOF3X_IMAGEBASE) are the developer "
                        L"launcher's, bof3x-launcher-dev.exe, which a source build writes beside this one; a release "
                        L"does not ship it.");
    bof3::os::ChildSpec spec;
    spec.program = dev;
    for (int i = 1; i < argc; ++i) spec.args.push_back(bof3::os::ToWide(argv[i]));
    spec.own_output = true;
    std::fflush(stdout);
    std::fflush(stderr);
    bof3::os::Child child;
    std::uint32_t error = 0;
    if (!bof3::os::ChildStart(spec, &child, &error))
        bof3x::StartDie(L"CreateProcess failed for " + dev + L" (" + std::to_wstring(error) + L")");
    std::uint32_t code = 1;
    bof3::os::ChildWait(child, &code);
    bof3::os::ChildClose(child);
    std::exit(static_cast<int>(code));
}

// Until 2026-10-15 the game wrote bof3x.exe.new with an icon generated from
// the game's data and the bof3x.icon stamp, and the launcher moved the new
// file over bof3x.exe (DIV-0095). Rewriting an executable and running it is
// what Windows Defender took for defence evasion, so bof3x.exe now carries our
// own icon from the build and is never rewritten. What an older build left
// behind beside it is deleted; nothing of it is used (launcher.cpp's
// RemoveOldIconFiles before stage P6).
void RemoveOldIconFiles(const std::wstring& own) {
    for (const wchar_t* name : {L"\\bof3x.exe.new", L"\\bof3x.icon", L"\\bof3x.icon-work.exe"}) {
        const std::wstring path = own + bof3x::StartSep(name);
        std::error_code ec;
        if (bof3x::StartFileExists(path) && std::filesystem::remove(bof3::os::PathW(path), ec))
            bof3x::StartSay(path + L", an older build's icon file, deleted (bof3x.exe keeps its own icon)\n");
    }
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
    if (!g_check) std::fprintf(stderr, "bof3x-launcher: scale %g, renderer %s\n", ui::Scale(), ui::RendererName());
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
        kNoWindow,  // the start is done and no window is wanted (--no-config, show_launcher=0): the game starts
    };
    Phase phase = Phase::kFind;
    bool start = false;        // StartFindGame, StartReadSettings and the stale check (the window; --check --start)
    // --config 1, --no-config 0, neither -1: then show_launcher decides (the Win32 launcher's want_dialog).
    // A check's start always shows the window.
    int want_window = 1;
    std::wstring named;        // --game or BOF3_GAME_DIR
    std::wstring read_ini, save_ini;
    bof3x::Config cfg;
    std::wstring game_dir, exe;
    bool exe_read = false;
    bool set_up_page = false;  // the Sources page was opened by the start, not by Data's Sources...
    bool reopen_data = false;  // the window is shown anew on Data (the Win32 g_reopen)
    ui::StaleCheck stale;
    std::string asked;         // the question last said on stdout (a check)

    bool Over() const {
        return closed || ask != ui::FrameAsk::kNone || phase == Phase::kOver || phase == Phase::kNoWindow;
    }

    // Where the window saves bof3x.ini; a check's --save, read again from then on.
    void Save(const char* what) {
        if (save_ini.empty()) return;
        if (!bof3x::ConfigSave(save_ini, cfg)) {
            bof3::os::PrintW(stderr, L"bof3x-launcher: cannot write " + save_ini + L"; settings not saved\n");
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
                    want_window = 1;   // shown anew whatever show_launcher now says (the Win32 want_dialog = 1)
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
            case Phase::kNoWindow:
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
        const bof3x::StartSetUp set_up =
            bof3x::StartReadSettings(read_ini, game_dir, named, false, exe_read, want_window, cfg);
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
        // cache named - and never under a self-test or --no-config. Stale: the player is asked.
        const std::wstring home_game = bof3x::HomeGameDefault();
        if (!bof3x::StartSelfTestOnly() && want_window != 0 && named.empty() && cfg.cache.empty() && !StartSwitchSet(L"BOF3X_CACHE") &&
            !StartSwitchSet(L"BOF3X_EXEIMAGE") && !home_game.empty() && game_dir == bof3x::StartFullPath(home_game)) {
            stale = ui::SourcesCheckStale();
            if (!stale.error.empty()) std::fprintf(stderr, "bof3x-launcher: %s\n", stale.error.c_str());
            if (g_check) Say(stale.stale ? "the install is stale" : "the install is not stale");
            if (stale.stale) {
                phase = Phase::kAsk;
                return;
            }
        }
        if (want_window == 1 || (want_window == -1 && cfg.show_launcher)) ShowWindow();
        else phase = Phase::kNoWindow;   // the settings as they are: no page is opened, nothing is saved
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
        } else if (verb == "click") {
            const std::vector<std::string> words = Split(rest, ' ');
            if (words.size() != 2) Refuse("The step \"" + step + "\" needs a place: click <x> <y>.");
            const float x = static_cast<float>(std::atof(words[0].c_str())), y = static_cast<float>(std::atof(words[1].c_str()));
            ui::TestMouse(x, y, false);   // the pointer there first, so the widget under it knows
            Frames(2);
            ui::TestMouse(x, y, true);
            Frames(2);
            ui::TestMouse(x, y, false);
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
            else if (rest == "jukebox") text = ui::JukeboxText();   // stage P5
            else if (rest == "saves") text = ui::SavesText();   // the save editor (stage P5)
            else Refuse("The step \"" + step + "\" needs sources, mods, jukebox or saves.");
            for (const std::string& line : Split(text, '\n')) std::printf("bof3x-launcher: report: %s\n", line.c_str());
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
        } else if (verb == "rest") {
            // Stage P5: frames for that long, as the window idles (the Jukebox's sound plays on)
            const double seconds = std::atof(rest.c_str());
            if (seconds <= 0) Refuse("The step \"" + step + "\" needs a number of seconds.");
            const std::uint64_t t0 = bof3::os::TickMs64();
            while (static_cast<double>(bof3::os::TickMs64() - t0) < seconds * 1000 && !Over()) {
                ui::Rest(100);
                Frames(1);
            }
            Frames(3);
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
    ui::JukeboxCheck(g_samples);   // stage P5: a check's Jukebox never sounds
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
    std::printf("bof3x-launcher: check (scale %g, renderer %s); page %d sub %d sheet %d%s, asked %s, %s\n",
                ui::Scale(), ui::RendererName(), tab, tab >= 0 ? s.state.sub[tab] : 0,
                tab >= 0 ? s.state.sheet[tab] : 0, s.pages.sources_open ? " (the Sources page shown)" : "",
                AskName(s), saved ? "saved the settings" : "saved nothing");
    ui::Close();
    g_window_open = false;
    return 0;
}

}  // namespace

// The start's refusals (launcher/start.h): on stderr, and in a message box
// only where no script waits for the launcher (g_no_box, the header's rule).
[[noreturn]] void bof3x::StartDie(const std::wstring& message) {
    bof3::os::PrintW(stderr, L"bof3x-launcher: " + message + L"\n");
    if (!g_check && !g_no_box) ui::Message(kTitle, bof3::os::ToUtf8(message.c_str()).c_str(), true);
    Leave(1);
}

int main(int argc, char* argv[]) {
    // A windowed program started from a terminal writes its lines there; with
    // its output redirected (a script's pipe or file) nothing changes.
    bof3::os::AttachParentTerminal();
    std::wstring named;
    std::vector<std::wstring> game_args;
    std::string capture, page = "title", ini, save;
    std::vector<std::string> steps;
    std::vector<std::pair<std::string, std::string>> sets;
    bool check = false, start = false, list = false;
    int want_window = -1;   // --config 1, --no-config 0, neither: show_launcher decides
    int host_asked = -1;    // --host: 0 exe, 1 bof3; -1 not given
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
        } else if (arg == "--config") {
            want_window = 1;
        } else if (arg == "--no-config") {
            want_window = 0;
        } else if (arg == "--host" && more) {
            const std::string host = argv[++i];
            if (host == "bof3") host_asked = 1;
            else if (host == "exe") host_asked = 0;
            else Refuse("--host " + host + ": use exe (bof3x, the default) or bof3 (BOF3.exe, the hosted build).");
        } else if (arg == "--set" && more) {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos || eq == 0) Refuse("--set needs a key and a value: --set <key>=<value>.");
            sets.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
        } else if (arg == "--list") {
            list = true;
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
        } else if (arg == "--samples" && more) {   // stage P5
            g_samples = argv[++i];
        } else if (arg == "--") {
            for (++i; i < argc; ++i) game_args.push_back(bof3::os::ToWide(argv[i]));
        } else {
            Refuse("The argument " + arg + " is not known.");
        }
    }
    g_check = check;
    // The box rule (the header): no box under --no-config, a self-test, or a script's file or pipe.
    g_no_box = want_window == 0 || bof3x::StartSelfTestOnly() || bof3::os::OutputRedirected();
    if (check) {
        if (want_window >= 0 || host_asked >= 0 || list || !sets.empty())
            Refuse("--config, --no-config, --host, --set and --list are not for --check.");
        return Check(named, ini, save, page, sub, sheet, steps, capture, start);
    }
    if (!g_samples.empty()) Refuse("--samples is for --check.");
    if (!ini.empty() || !save.empty() || start) Refuse("--ini, --save and --start are for --check. The window uses bof3x.ini.");

    const std::wstring own = bof3x::StartOwnDirectory();
    // Settings. The file lives next to the launcher, not in the game folder,
    // which stays the player's own (CLAUDE.md rule 1) apart from BOF3.CFG.
    const std::wstring ini_path = own + bof3x::StartSep(L"\\bof3x.ini");

    // --set and --list: bof3x.ini changed or shown, and nothing more (the
    // launcher with no window's, stage P0, with its texts).
    if (list || !sets.empty()) {
        if (want_window >= 0 || host_asked >= 0 || !steps.empty() || !game_args.empty())
            Refuse("--set and --list change and show bof3x.ini only. Use them without --config, --no-config, --host, "
                   "--script and --.");
        bof3x::Config cfg;
        std::string why;
        bof3x::ConfigLoad(ini_path, cfg, why);   // no file: the defaults
        if (!why.empty()) bof3x::StartDie(bof3x::StartWide(why) + L"\n\n" + ini_path);
        for (const auto& [key, value] : sets) {
            if (!bof3x::ConfigSetLine(cfg, key, value, why)) Refuse(why);
            std::printf("bof3x-launcher: bof3x.ini: %s=%s\n", key.c_str(), value.c_str());
        }
        if (!sets.empty() && !bof3x::ConfigSave(ini_path, cfg))
            bof3x::StartDie(L"Cannot write " + ini_path + L". The settings did not change.");
        if (list) List(cfg);
        return 0;
    }

    if (named.empty()) {
        wchar_t env[1024];
        const std::uint32_t n = bof3::os::GetEnvW(L"BOF3_GAME_DIR", env, 1024);
        if (n > 0 && n < 1024) named = env;
    }

    // The game: the program beside this one, with this one's ending - bof3x.exe
    // and its bof3x.dll on Windows, bof3x on Linux.
    std::string self;
    if (!bof3::os::ExecutablePath(&self)) bof3x::StartDie(L"Cannot find the folder of this launcher.");
    const bool windows = self.size() > 4 && self.compare(self.size() - 4, 4, ".exe") == 0;
    if (windows) {
        const std::wstring dll = own + bof3x::StartSep(L"\\bof3x.dll");
        if (!bof3x::StartFileExists(dll)) bof3x::StartDie(dll + L" is missing; it belongs next to this launcher.");
    }
    // Not given: our own executable - unless the environment asks for what
    // only the hosted build has (Capcom's code: the A/B switch, the shadow
    // fuzz, the call trace, Capcom's MP3 decoder and its comparison; bof3x
    // refuses each), so the documented self-test and A/B commands keep their meaning.
    const bool hosted = host_asked >= 0 ? host_asked == 1 : HostedOnlyAsked();
    if (hosted) ForwardToDevLauncher(argc, argv, own, windows);
    // BOF3X_IMAGEBASE (a developer switch, docs/relocatable-image-design.md
    // 1.5; no ini key): bof3x.exe written at another base, bof3x-<BASE>.exe,
    // which the developer launcher starts and fences. 0x400000 is bof3x.exe itself.
    const std::wstring ours = own + bof3x::StartSep(windows ? L"\\bof3x.exe" : L"\\bof3x");
    if (wchar_t base_text[32]; bof3::os::GetEnvW(L"BOF3X_IMAGEBASE", base_text, 32) > 0) {
        wchar_t* end = nullptr;
        const std::uint64_t base = std::wcstoull(base_text, &end, 0);
        if (!end || *end || end == base_text)
            bof3x::StartDie(L"BOF3X_IMAGEBASE=" + std::wstring(base_text) + L" is not a number");
        if (base != 0x400000) ForwardToDevLauncher(argc, argv, own, windows);
        bof3x::StartSay(L"BOF3X_IMAGEBASE=" + std::wstring(base_text) + L": starting " + ours + L"\n");
    }
    if (!bof3x::StartFileExists(ours))
        bof3x::StartDie(ours + (windows ? L" is missing; it belongs next to this launcher (or start the hosted build "
                                          L"with --host bof3)."
                                        : L" is missing; it belongs next to this launcher."));
    RemoveOldIconFiles(own);

    Session s;
    s.start = true;
    s.want_window = want_window;
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
    // kNoWindow: --no-config or show_launcher=0 - the settings as read, nothing saved (as the Win32 launcher)
    if (s.phase != Session::Phase::kNoWindow) {
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
    }

    bof3x::StartApply(s.game_dir, s.cfg, false, s.exe_read);
    bool waited = false;
    const std::uint32_t code = bof3x::StartGame(ours, game_args, s.game_dir, nullptr, &waited);
    return waited ? static_cast<int>(code) : 0;
}
