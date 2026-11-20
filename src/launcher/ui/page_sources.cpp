// The Sources page (launcher.rc IDD_SOURCES; docs/install-sources.md): the
// player's copies of the game, the build, and the check of an install made by
// an older release (docs/install-staleness.md). sources_dialog.cpp's
// SourcesDialogRun, Proc, State, StartList, StartSetup, OnLine, OnDone, Add,
// Remove, the pickers, LocateFfmpeg, FfmpegNote, Greyed, SourcesCheckStale and
// SourcesAskRebuild, less the windows. The texts are the Win32 page's, word
// for word.
//
// Every check and every build is tools/importer.py's, run by ToolRun
// (tool_run.h), one child at a time; the page is grey while one runs. The
// contract is sources_dialog.cpp's header's (docs/install-sources.md 4).
//
// The Win32 window is 400 x 276 dialog units, larger than the area the page
// is shown in (614 x 402 of the picture's pixels, about 350 x 214 units), so
// it is laid out again in that area, every control and text kept: the
// controls at the right (Remove, Add, the ffmpeg buttons, Build and Close)
// are 54 units further left, the list and the log are lower, and the ffmpeg
// note has two lines, with its buttons under it.
#include "launcher/ui/pages.h"

#include "common/home.h"
#include "launcher/ui/tool_run.h"
#include "launcher/ui/ui.h"

#include "os/env.h"
#include "os/text.h"
#include "os/wide_file.h"

#include <cinttypes>
#include <cstdio>
#include <cwctype>
#include <filesystem>

namespace bof3x::ui {

const char kRebuildTitle[] = "Breath of Fire III - the install is out of date";

namespace {

enum Job : int { kNone, kList, kAdd, kRemove, kSetup };

constexpr int kAskFile = 11, kAskFolder = 12, kAskFfmpeg = 13;   // this page's questions to the pickers

constexpr bool kWindowsNames = std::filesystem::path::preferred_separator == L'\\';
const char* const kFfmpegUrl =
    kWindowsNames ? "https://ffmpeg.org/download.html#build-windows" : "https://ffmpeg.org/download.html#build-linux";

std::string Utf8Of(const std::wstring& w) { return ConfigToUtf8(w); }
std::wstring WideOf(const std::string& s) { return ConfigFromUtf8(s); }

std::wstring EnvString(const wchar_t* name) {
    wchar_t buf[1024];
    const std::uint32_t n = bof3::os::GetEnvW(name, buf, 1024);
    return n > 0 && n < 1024 ? std::wstring(buf, n) : std::wstring();
}

struct State {
    Pages* p = nullptr;
    bool first_run = false;
    bool rebuild = false;   // a stale install the player agreed to rebuild: build at once, end when done
    Runtime rt;
    std::wstring home;
    Job job = kNone;
    bool closing = false;   // Stop answered yes while a child ran: end once it has
    bool built = false;     // a build finished: the caller re-reads the install
    bool added = false;     // the last add succeeded: build next
    bool asking_stop = false;   // the Stop question is open
    std::vector<std::wstring> paths;   // the list's, in order
    std::vector<std::string> rows;
    int sel = -1;
    // A game folder with no image, to set up in place: added once the list is
    // read, unless listed already; `in_place` is it once it is a listed
    // source, and the build then makes the home's cache alone.
    std::wstring folder;
    bool folder_asked = false;
    std::wstring in_place;
    std::string intro, home_line, ffnote, path, status, log;
    unsigned done = 0, total = 100;   // the progress bar's place and range (a Win32 bar starts at 0 of 100)
};
State g_s;
ToolRun g_run;

void Log(State& s, const std::string& line) { s.log += line + "\n"; }   // the Win32 log's line and "\r\n"

void FfmpegNote(State& s) {
    const Config& cfg = *s.p->cfg;
    const std::wstring located = WideOf(cfg.ffmpeg), env = EnvString(L"BOF3X_FFMPEG");
    const std::wstring found = FindProgram(kWindowsNames ? L"ffmpeg.exe" : L"ffmpeg");
    if (!located.empty() && bof3::os::IsFileW(located)) s.ffnote = "ffmpeg: " + Utf8Of(located);
    else if (!env.empty() && bof3::os::IsFileW(env)) s.ffnote = "ffmpeg: " + Utf8Of(env) + " (BOF3X_FFMPEG)";
    else if (!found.empty()) s.ffnote = "ffmpeg: " + Utf8Of(found) + " (on PATH)";
    else
        s.ffnote = "ffmpeg was not found. Only the logo movie of a PC install and the sound and movie of a PSP disc need "
                   "it. Without ffmpeg, the install lacks them.";
}

bool Start(State& s, Job job, const std::vector<std::wstring>& args) {
    if (!s.rt.error.empty() || s.job != kNone) return false;
    ToolEnv env = {{L"PYTHONUTF8", L"1"}, {L"PYTHONIOENCODING", L"utf-8"}, {L"PYTHONUNBUFFERED", L"1"}};
    if (!s.p->cfg->ffmpeg.empty()) env.push_back({L"BOF3X_FFMPEG", WideOf(s.p->cfg->ffmpeg)});
    std::wstring error;
    if (!g_run.Start(s.rt, s.rt.script, args, s.home, env, &error)) {
        Log(s, Utf8Of(error));
        return false;
    }
    s.job = job;
    return true;
}

void StartList(State& s) {
    s.rows.clear();
    s.paths.clear();
    s.sel = -1;
    Start(s, kList, {L"sources", L"--home", s.home, L"list", L"--json"});
}

void StartSetup(State& s) {
    s.done = 0;
    s.status = "Building the install...";
    Log(s, "Building the install in " + Utf8Of(s.home) + "...");
    // A PC folder set up in place: the importer builds the home's cache from the
    // sources and lays out no game folder; the folder is the game folder still.
    std::vector<std::wstring> args = {L"setup", L"--home", s.home, L"--json-progress"};
    if (!s.in_place.empty()) {
        args.push_back(L"--game-in-place");
        args.push_back(s.in_place);
    }
    Start(s, kSetup, args);
}

// Whether two paths name the same folder: made absolute, the trailing
// separator dropped, case not counted where the system does not count it
// (Windows).
bool SamePath(const std::wstring& a, const std::wstring& b) {
    auto full = [](const std::wstring& p) {
        std::wstring out;
        if (!bof3::os::FullPathW(p, &out)) out = p;
        while (out.size() > 3 && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
        return out;
    };
    std::wstring x = full(a), y = full(b);
    if (x.empty()) return false;
    if (kWindowsNames) {
        for (wchar_t& c : x) c = static_cast<wchar_t>(std::towupper(c));
        for (wchar_t& c : y) c = static_cast<wchar_t>(std::towupper(c));
    }
    return x == y;
}

std::string Trimmed(const std::string& text) {
    const std::size_t b = text.find_first_not_of(" \t\""), e = text.find_last_not_of(" \t\"");
    return b == std::string::npos ? std::string() : text.substr(b, e - b + 1);
}

void Add(State& s) {
    std::wstring path = WideOf(Trimmed(s.path));
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    if (path.empty()) {
        s.status = "Type or select a path first.";
        return;
    }
    if (!bof3::os::IsFileW(path) && !bof3::os::IsDirectoryW(path)) {
        s.status = "Nothing is at that path.";
        return;
    }
    s.added = false;
    s.status = "Checking the copy (approximately 1 minute for each disc)...";
    Log(s, "Checking " + Utf8Of(path) + "...");
    Start(s, kAdd, {L"sources", L"--home", s.home, L"add", path, L"--json"});
}

void Remove(State& s) {
    if (s.sel < 0 || static_cast<std::size_t>(s.sel) >= s.paths.size()) return;
    Start(s, kRemove, {L"sources", L"--home", s.home, L"remove", s.paths[static_cast<std::size_t>(s.sel)]});
}

void OnLine(State& s, Job job, const std::string& line) {
    if (job == kList && IsJsonObject(line)) {
        const std::string path = JsonString(line, "path"), label = JsonString(line, "label");
        std::string row = (label.empty() ? std::string("(unknown)") : label) + "   " + path;
        if (!JsonTrue(line, "ok")) row = "[problem] " + row;
        s.rows.push_back(row);
        s.paths.push_back(WideOf(path));
        return;
    }
    if (job == kAdd && IsJsonObject(line)) {
        const std::string label = JsonString(line, "label"), problem = JsonString(line, "problem");
        // A folder of images answers once for each source in it.
        const std::string path = JsonString(line, "path");
        const bool ok = JsonTrue(line, "ok");
        s.added = s.added || ok;
        if (ok && !s.folder.empty() && SamePath(WideOf(path), s.folder)) s.in_place = s.folder;
        if (ok) Log(s, "Recognised: " + label + " (" + path + ").");
        else Log(s, "Not added: " + path + ": " + (problem.empty() ? std::string("this release is not known.") : problem));
        return;
    }
    if (job == kSetup && line.compare(0, 9, "progress ") == 0) {
        unsigned done = 0, total = 0;
        int used = 0;
        if (std::sscanf(line.c_str() + 9, "%u %u %n", &done, &total, &used) >= 2 && total > 0) {
            s.total = total;
            s.done = done;
            const std::string what = line.substr(9 + static_cast<std::size_t>(used));
            s.status = "Step " + std::to_string(done) + " of " + std::to_string(total) + ": " + what;
            Log(s, what);
            return;
        }
    }
    if (job == kSetup && line.compare(0, 6, "error ") == 0) {
        Log(s, "Error: " + line.substr(6));
        return;
    }
    if (!line.empty()) Log(s, line);
}

// The page closes (EndDialog): the window is told, with whether a build finished.
void End(State& s, bool built) {
    s.p->sources_open = false;
    s.p->sources_closed = true;
    s.p->sources_built = built;
}

void OnDone(State& s, Job job, std::uint32_t code) {
    s.job = kNone;   // idle until a next child starts, if one does
    if (s.closing) {
        End(s, s.built);
        return;
    }
    switch (job) {
    case kList:
        if (code != 0) Log(s, "The importer could not read the source list (exit " + std::to_string(code) + ").");
        else if (s.paths.empty()) s.status = "No sources yet.";
        if (code == 0 && !s.folder.empty() && !s.folder_asked) {
            // The game folder to set up in place: built from at once when it is
            // a listed source, else added first (the importer identifies it).
            s.folder_asked = true;
            for (const std::wstring& p : s.paths)
                if (SamePath(p, s.folder)) s.in_place = s.folder;
            if (!s.in_place.empty()) {
                StartSetup(s);
            } else {
                s.path = Utf8Of(s.folder);
                Add(s);
            }
        }
        return;
    case kAdd:
        if (code == 0 && s.added) {
            s.path.clear();
            StartSetup(s);   // added: the install is rebuilt with it at once
            return;
        }
        s.status = !s.folder.empty() && s.in_place.empty()
                       ? "The importer cannot use this game folder. See the log below. You can add another copy of the game."
                       : "Nothing was added. See the log below.";
        return;
    case kRemove:
        s.status = code == 0 ? "Removed. Build the install again to apply the change." : "Not removed. See the log below.";
        StartList(s);
        return;
    case kSetup:
        if (code == 0) {
            s.built = true;
            s.total = 1;
            s.done = 1;
            s.status = "The install is ready.";
            Log(s, "The install is ready.");
            if (!s.p->cfg->cache.empty())
                Log(s, "Note: your settings use a different cache folder (cache=). To use the cache of this install, "
                       "clear the Cache folder box on the Data tab.");
            if (s.first_run || s.rebuild) {   // nothing else to do here: on to the settings, or the game
                End(s, true);
                return;
            }
        } else {
            s.status = "The build failed. The install did not change.";
        }
        StartList(s);
        return;
    default:
        return;
    }
}

// Close, or Stop while a child runs (the Win32 page's IDCANCEL).
void CloseOrStop(State& s) {
    if (s.job == kNone) End(s, s.built);
    else s.asking_stop = true;
}

}  // namespace

void SourcesBegin(Pages& p, bool first_run, bool rebuild, const std::wstring& folder) {
    g_run.Stop();   // a page opened again never inherits a child (none runs while it is closed)
    State& s = g_s;
    s = State{};
    s.p = &p;
    s.first_run = first_run;
    s.rebuild = rebuild;
    s.folder = first_run && !rebuild ? folder : std::wstring();
    s.rt = FindRuntime();
    s.home = HomeDir();
    p.sources_open = true;
    p.sources_closed = false;
    p.sources_built = false;
    s.intro = s.rebuild ? "An older release built this install. The importer will build it again from the copies of the "
                          "game below. It keeps the parts that do not change."
              : !s.folder.empty()
                  ? "This game folder is not set up yet. The importer checks the folder and makes the game data from it, "
                    "one time only. The game then starts from this folder, and your saves stay in it."
              : s.first_run ? "No install was found. To make one, add your copy of the game: the PC game folder, a "
                              "PlayStation disc image (.cue/.bin or .zip) or a PSP .iso. The importer checks each copy "
                              "before it builds."
                            : "These are the copies of the game that this install uses. When you add a copy, the importer "
                              "checks it and builds the install again.";
    s.home_line = "Install folder: " + Utf8Of(s.home);
    FfmpegNote(s);
    if (!s.folder.empty())
        Log(s, "Game folder: " + Utf8Of(s.folder) + ". The importer reads it and does not change or remove its files.");
    if (!s.rt.error.empty()) {
        s.status = "The importer cannot run.";
        Log(s, Utf8Of(s.rt.error));
    } else if (s.rebuild) {
        StartSetup(s);
    } else {
        StartList(s);
    }
}

void SourcesPoll(Pages&) {
    State& s = g_s;
    if (s.job == kNone) return;
    const Job job = s.job;
    for (std::string line; g_run.NextLine(&line);) OnLine(s, job, line);
    if (std::uint32_t code = 1; g_run.Ended(&code)) OnDone(s, job, code);
}

void SourcesPage(Pages& p) {
    State& s = g_s;
    const bool idle = s.job == kNone, runnable = idle && s.rt.error.empty();
    const char* why = !s.rt.error.empty() ? "The importer cannot run." : "The importer is running.";

    Place(0, 0);
    Size(Dx(346));
    Label(s.intro.c_str());
    Place(0, Dy(27));
    Label("Your sources:");
    Place(0, Dy(36));
    Size(Dx(292), Dy(40));
    List("##sources", &s.sel, s.rows);
    Place(Dx(296), Dy(36));
    Size(Dx(50), Dy(14));
    if (!runnable || s.sel < 0) Grey(runnable ? "Select a source first." : why);
    if (Button("Remove")) Remove(s);

    Place(0, Dy(80));
    Label("Add a source:");
    Place(0, Dy(89));
    Size(Dx(184));
    if (!runnable) Grey(why);
    Edit("##path", &s.path);
    Place(Dx(188), Dy(88));
    Size(Dx(50), Dy(14));
    if (!runnable) Grey(why);
    if (Button("File..."))
        PickFile(kAskFile, "Game sources (*.cue, *.bin, *.iso, *.zip, BOF3.exe)", "cue;bin;iso;zip;exe",
                 "Select a disc image, a .zip of a disc image, or BOF3.exe of the PC game");
    Place(Dx(242), Dy(88));
    Size(Dx(50), Dy(14));
    if (!runnable) Grey(why);
    if (Button("Folder..."))
        PickFolder(kAskFolder, "Select the PC game folder (with BOF3.exe and DAT) or a folder of disc images. "
                               "The importer adds each known release");
    Place(Dx(296), Dy(88));
    Size(Dx(50), Dy(14));
    if (!runnable) Grey(why);
    if (Button("Add")) Add(s);
    if (std::string picked; Picked(kAskFile, &picked) && !picked.empty()) s.path = picked;
    if (std::string picked; Picked(kAskFolder, &picked) && !picked.empty()) s.path = picked;

    Place(0, Dy(106));
    Size(Dx(346));
    Label(s.ffnote.c_str());
    Place(Dx(184), Dy(123));
    Size(Dx(76), Dy(14));
    if (!idle) Grey("The importer is running.");
    if (Button("Locate ffmpeg.exe..."))
        PickFile(kAskFfmpeg, "ffmpeg.exe", kWindowsNames ? "exe" : nullptr, "Locate ffmpeg.exe");
    if (std::string picked; Picked(kAskFfmpeg, &picked) && !picked.empty()) {
        p.cfg->ffmpeg = picked;
        FfmpegNote(s);
    }
    Place(Dx(264), Dy(123));
    Size(Dx(82), Dy(14));
    if (Button("Get ffmpeg (website)...")) OpenLink(kFfmpegUrl);

    Place(0, Dy(141));
    Size(Dx(346), Dy(9));
    Progress(s.total > 0 ? static_cast<float>(s.done > s.total ? s.total : s.done) / static_cast<float>(s.total) : 0.0f);
    Place(0, Dy(152));
    Size(Dx(346));
    LabelCut(s.status.c_str(), false);
    Place(0, Dy(162));
    Size(Dx(346), Dy(35));
    LogText("log", s.log);

    Place(0, Dy(203));
    Size(Dx(230));
    LabelCut(s.home_line.c_str(), true);
    Place(Dx(234), Dy(200));
    Size(Dx(64), Dy(14));
    if (!runnable || s.paths.empty()) Grey(runnable ? "Add a source first." : why);
    if (Button("Build the install")) StartSetup(s);
    Place(Dx(302), Dy(200));
    Size(Dx(44), Dy(14));
    const bool close = Button(idle ? "Close##close" : "Stop##close");
    if (close || Pressed(Press::kBack)) CloseOrStop(s);

    if (s.asking_stop) {
        const int answer = Ask("Sources", s.job == kSetup ? "Stop the build? The install does not change." : "Stop the importer?");
        if (answer >= 0) {
            s.asking_stop = false;
            if (answer == 1 && s.job != kNone) {
                s.closing = true;
                g_run.Stop();   // its end comes through SourcesPoll, which ends the page
            }
        }
    }
}

std::string SourcesText() {
    const State& s = g_s;
    std::string out = "intro: " + s.intro + "\n" + s.home_line + "\n" + s.ffnote + "\n";
    for (std::size_t i = 0; i < s.rows.size(); ++i) out += "source " + std::to_string(i) + ": " + s.rows[i] + "\n";
    out += "status: " + s.status + "\n";
    out += "progress: " + std::to_string(s.done) + " of " + std::to_string(s.total) + "\n";
    std::size_t from = 0;
    while (from < s.log.size()) {
        std::size_t end = s.log.find('\n', from);
        if (end == std::string::npos) end = s.log.size();
        out += "log: " + s.log.substr(from, end - from) + "\n";
        from = end + 1;
    }
    return out;
}

// ---- The check of an older release's install ----------------------------------

namespace {

// A step of the cache (tools/build_reuse.py's names) as the player reads it.
std::string PieceName(const std::string& step) {
    struct Row {
        const char* step;
        const char* name;
    };
    static const Row kRows[] = {
        {"cache", "the whole install"},     {"base", "the game data"},
        {"exe", "the game's program data"}, {"bgm", "the music"},
        {"snd", "the sound effects"},       {"snd-psp", "the sound effects"},
        {"manifest", "the install's index"}, {"fmv", "the logo movie"},
        {"opt", "the optional layers"},     {"game-pc", "the game folder"},
        {"game-install", "the game folder"},
    };
    for (const Row& r : kRows)
        if (step == r.step) return r.name;
    if (step.compare(0, 4, "loc-") == 0) return "the " + step.substr(4) + " text";
    if (step.compare(0, 4, "opt-") == 0) return "the optional layer " + step.substr(4);
    // A second-language or mod reading-row layer: its step is its layer's name.
    if (step.compare(0, 5, "ruby-") == 0) return "the reading-row layer " + step.substr(5);
    return step;
}

// The stamp a fresh answer is remembered by: the launcher's and the
// importer's file sizes and write times. A new release, or a rebuilt source
// checkout, changes one of them, and the install is judged again. The Win32
// launcher's text: each as its 64-bit size and time in 16 hex digits.
std::string StaleStamp(const Runtime& rt) {
    std::string self;
    if (!bof3::os::ExecutablePath(&self)) return std::string();
    std::string stamp;
    for (const std::wstring& path : {bof3::os::ToWide(self.c_str()), rt.script}) {
        std::uint64_t size = 0, time = 0;
        if (path.empty() || !bof3::os::StampW(path, &size, &time)) return std::string();
        char part[64];
        std::snprintf(part, sizeof part, "%016" PRIx64 " %016" PRIx64 "\n", size, time);
        stamp += part;
    }
    return stamp;
}

std::string ReadSmall(const std::wstring& path) {
    const bof3::os::InFile f = bof3::os::InOpenW(path, true);
    if (f == bof3::os::kNoInFile) return std::string();
    char buf[512];
    std::uint32_t got = 0;
    const bool ok = bof3::os::InRead(f, buf, sizeof buf, &got);
    bof3::os::InClose(f);
    return ok ? std::string(buf, got) : std::string();
}

}  // namespace

StaleCheck SourcesCheckStale() {
    StaleCheck check;
    const Runtime rt = FindRuntime();
    const std::wstring home = HomeDir(), cache = HomeCacheDefault();
    if (!rt.error.empty() || home.empty() || cache.empty()) return check;   // nothing to judge, or nothing to judge it by
    const std::string stamp = StaleStamp(rt);
    const std::wstring remembered = cache + ToolSep(L"\\stale-checked.txt");
    if (!stamp.empty() && ReadSmall(remembered) == stamp) return check;    // judged fresh by this launcher and importer
    std::string out;
    std::uint32_t code = 1;
    if (!ToolCapture(rt, {L"stale", L"--home", home}, home, 120000, &out, &code)) {
        check.error = "The importer cannot check this install (it did not finish).";
        return check;
    }
    // `stale <step>\t<reason>` a piece, then `stale N` (exit 1); `fresh` (0); `none` (2).
    std::size_t at = 0;
    while (at < out.size()) {
        std::size_t end = out.find('\n', at);
        if (end == std::string::npos) end = out.size();
        std::string line = out.substr(at, end - at);
        at = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const std::size_t tab = line.find('\t');
        if (line.compare(0, 6, "stale ") == 0 && tab != std::string::npos)
            check.pieces.push_back(PieceName(line.substr(6, tab - 6)) + " (" + line.substr(tab + 1) + ")");
    }
    if (code == 0) {
        if (!stamp.empty()) bof3::os::WriteWholeW(remembered, stamp);
    } else if (code == 1 && !check.pieces.empty()) {
        check.stale = true;
    } else if (code != 2) {
        check.error = "The importer cannot check this install (exit " + std::to_string(code) + "):\n" + out.substr(0, 600);
    }
    return check;
}

std::string SourcesRebuildText(const StaleCheck& check) {
    std::string text = "An older release built this install. This release makes these parts differently:\n\n";
    std::size_t shown = 0;
    for (const std::string& piece : check.pieces) {
        if (++shown > 12) {
            text += "  ... and " + std::to_string(check.pieces.size() - 12) + " more\n";
            break;
        }
        text += "  - " + piece + "\n";
    }
    text += "\nBuild it again now from your copies of the game? The importer keeps the parts that did not change, "
            "so this is usually faster than the first build.\n\nNo: the game does not start, and the install does not change.";
    return text;
}

}  // namespace bof3x::ui
