// Mods, Data: the player's mods (launcher.rc IDD_PAGE_MODS; docs/mods.md).
// sources_dialog.cpp's ModsProc, ModsState, ModRow, ModRowText, ModsCredit,
// ModsReport, ModsToggle, ModsAdd, ModsBrowse, ModsRemove, ModsOnLine,
// ModsOnDone and ModsDraw, less the windows, and the window's side of their
// messages (config_dialog.cpp Proc): kMsgModsChanged raises Pages::mods_stamp,
// so Settings' Row above the text box is made again; kMsgModsLanguage is
// Settings' language as it stands, read each frame; kMsgModsReadingRowAdded
// turns Extras' toggle on; kMsgModsOff is ModsOpen; kMsgModsOffChanged writes
// mods_off= into the settings at once; kMsgModsToggle is the list's box. The
// texts are the Win32 page's, word for word.
//
// tools/mods.py beside the importer, on the same Python, one child at a time.
// `list --json` gives one flat object a mod (sources_dialog.cpp's notes before
// ModRow have its keys). A mod the install cannot play is grey in the list
// with its reason (the owner's rule, 2026-10-10). A mod that is on or off as a
// whole (table-edit, disc-patch) has a check box at the left of its row (the
// owner's call, 2026-11-11): ticked unless mods_off= names it.
#include "launcher/ui/pages.h"

#include "common/home.h"
#include "launcher/ui/tool_run.h"
#include "launcher/ui/ui.h"

#include "os/wide_file.h"

#include <algorithm>

namespace bof3x::ui {

namespace {

enum Job : int { kNone, kModList, kModAdd, kModBuild, kModRemove };

constexpr int kAskMod = 21;   // this page's question to the file picker

std::string Utf8Of(const std::wstring& w) { return ConfigToUtf8(w); }
std::wstring WideOf(const std::string& s) { return ConfigFromUtf8(s); }

struct ModRow {
    std::string id, name, version, kind, base, state, needs, problem;
    std::string author, url, credit;   // who made it (docs/mods.md 2): shown, never opened or run
    std::string language, carries, lacks;
    std::vector<std::string> behaviours, missing;
    bool unverified = false;   // a patch that is not in the release's list of known mods (docs/mods.md 12)
    std::string known_now;     // the mod this release knows that patch as, when it does
    std::vector<std::string> languages;
};

// What the player is told about such a patch (the owner's words, 2026-11-13).
const char kModUnverified[] =
    "This patch is not in the list of known mods of this release. Its author and its function are not verified. "
    "The game takes only the data and text changes. It does not take changes to the program.";

bool ModOnOff(const ModRow& r) { return r.kind == "disc-patch" || r.kind == "table-edit"; }

struct ModsState {
    Pages* p = nullptr;
    Runtime rt;
    std::wstring script, home;
    Job job = kNone;
    bool listed = false;    // the list has been asked for once (on first showing)
    bool changed = false;   // an add, a build or a remove ran: the window is told after the next list
    std::string build_next; // the id an add just installed: built next
    std::string added_kind; // its kind: a reading-row mod turns Extras' toggle on
    std::string off;        // the ini's mods_off=, as the boxes have changed it
    std::string told;       // the mod whose report the log showed last (ModsCredit)
    std::vector<ModRow> rows;
    int sel = -1;
    std::string path, status, log;
};
ModsState g_m;
ToolRun g_run;

void ModLog(ModsState& s, const std::string& line) {
    // the line break before a line, not after: the box ends on text, not on an empty line
    s.log += (s.log.empty() ? "" : "\n") + line;
}

std::string Joined(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& n : names) out += (out.empty() ? "" : ", ") + n;
    return out;
}

// A row as the list shows it: name, author, version, state; and whether it is greyed.
// The kind is on the status line when the row is selected (ModsCredit).
std::string ModRowText(const ModsState& s, const std::string& language, const ModRow& r, bool& greyed) {
    std::string state;
    greyed = true;
    if (!r.missing.empty()) {
        state = "needs " + Joined(r.missing) +
                (r.missing.size() == 1 ? ": this release does not have it" : ": this release does not have them");
    } else if (r.state == "built") {
        if (r.kind == "reading-row" && r.base != language) {
            state = "built; only with " + r.base + " text";
        } else if (r.kind == "disc-patch" && r.language != language &&
                   std::find(r.languages.begin(), r.languages.end(), language) == r.languages.end()) {
            std::string tags = r.language;
            for (const std::string& t : r.languages)
                if (t != r.language) tags += " or " + t;
            state = "built; only with " + tags + " text";
        } else if (ModOnOff(r)) {
            state = ConfigListHas(s.off, r.id) ? "built: off" : "built: on";
            greyed = false;
        } else {
            state = "built: select it in Settings";
            greyed = false;
        }
    } else if (r.state == "not-built") {
        state = "not built: Data, Sources, Build the install";
    } else if (r.state == "needs") {
        state = "needs " + r.needs + " text: add that disc on Data, Sources";
    } else if (r.state == "needs-disc") {
        state = "needs the disc " + r.needs + ": add it on Data, Sources";
    } else {
        state = "problem: " + (r.problem.empty() ? std::string("unreadable") : r.problem);
    }
    const std::string by = r.unverified ? "not verified" : r.author.empty() ? std::string() : "by " + r.author;
    return r.name + "\t" + by + "\t" + r.version + "\t" + state;
}

// The status line for the selected mod: its name and version, who made it, the
// author's own line and address when the mod gives them, and its kind.
void ModsCredit(ModsState& s) {
    if (s.sel < 0 || static_cast<std::size_t>(s.sel) >= s.rows.size()) return;
    const ModRow& r = s.rows[static_cast<std::size_t>(s.sel)];
    if (r.unverified) {
        s.status = r.name + ". " + kModUnverified;
        return;
    }
    std::string text = r.name + " " + r.version;
    text += r.author.empty() ? ". The mod does not give its author." : ", by " + r.author + ".";
    if (!r.credit.empty()) text += " " + r.credit;
    if (!r.url.empty()) text += " " + r.url;
    text += " Kind: " + r.kind + ".";
    if (!r.behaviours.empty()) {
        // where a behaviour is on because a mod asked, the mod says so (docs/mods.md 11)
        text += (r.missing.empty() ? " When it is on, it turns on these behaviours of the game: "
                                   : " It needs these behaviours of the game: ") + Joined(r.behaviours) + ".";
        if (!r.missing.empty()) text += " This release does not have all of them, so the mod stays off.";
    }
    s.status = text;
}

// What the selected mod's build has and does not have (a disc-patch mod's
// report.toml, as mods.py reads it): into the log, once for each selection.
void ModsReport(ModsState& s) {
    if (s.sel < 0 || static_cast<std::size_t>(s.sel) >= s.rows.size()) return;
    const ModRow& r = s.rows[static_cast<std::size_t>(s.sel)];
    if (r.id == s.told) return;
    if (r.unverified) {
        // the status line is one line: the whole sentence goes in the log, once for each selection
        s.told = r.id;
        ModLog(s, r.name + ": " + kModUnverified);
        if (!r.known_now.empty())
            ModLog(s, "This release knows this patch as " + r.known_now + ". Add the file again to get that mod.");
    }
    if (r.state != "built" || (r.carries.empty() && r.lacks.empty())) return;
    s.told = r.id;
    if (!r.carries.empty()) ModLog(s, r.name + ": " + r.carries);
    if (!r.lacks.empty()) ModLog(s, r.name + ": " + r.lacks);
}

// The settings get the new mods_off= at once, and the window saves them on
// any way out (kMsgModsOffChanged: cfg and `applied`).
void TellOff(ModsState& s) {
    s.p->cfg->mods_off = s.off;
    s.p->applied = true;
}

// The box of row `i` changed by the player: the row must have one and not be greyed.
void ModsToggle(ModsState& s, const std::string& language, std::size_t i) {
    if (i >= s.rows.size()) return;
    const ModRow& r = s.rows[i];
    bool greyed = false;
    ModRowText(s, language, r, greyed);
    if (!ModOnOff(r) || greyed) return;
    const bool now_off = !ConfigListHas(s.off, r.id);
    s.off = ConfigListSet(s.off, r.id, now_off);
    TellOff(s);
}

bool ModsStart(ModsState& s, Job job, const std::vector<std::wstring>& args) {
    if (!s.rt.error.empty() || s.job != kNone) return false;
    std::wstring error;
    const ToolEnv env = {{L"PYTHONUTF8", L"1"}, {L"PYTHONIOENCODING", L"utf-8"}, {L"PYTHONUNBUFFERED", L"1"}};
    if (!g_run.Start(s.rt, s.script, args, s.home, env, &error)) {
        ModLog(s, Utf8Of(error));
        return false;
    }
    s.job = job;
    return true;
}

void ModsList(ModsState& s) {
    s.listed = true;
    s.rows.clear();
    ModsStart(s, kModList, {L"list", L"--home", s.home, L"--json"});
}

std::string Trimmed(const std::string& text) {
    const std::size_t b = text.find_first_not_of(" \t\""), e = text.find_last_not_of(" \t\"");
    return b == std::string::npos ? std::string() : text.substr(b, e - b + 1);
}

void ModsAdd(ModsState& s) {
    const std::string path = Trimmed(s.path);
    if (path.empty()) {
        s.status = "Type or select the path of a .zip, .7z, .xdelta or .ppf file first.";
        return;
    }
    if (!bof3::os::IsFileW(WideOf(path))) {
        s.status = "No file is at that path.";
        return;
    }
    s.build_next.clear();
    s.added_kind.clear();
    s.status = "Checking the mod...";
    ModLog(s, "Checking " + path + "...");
    ModsStart(s, kModAdd, {L"add", WideOf(path), L"--home", s.home, L"--json"});
}

void ModsRemove(ModsState& s) {
    if (s.sel < 0 || static_cast<std::size_t>(s.sel) >= s.rows.size()) return;
    const ModRow& r = s.rows[static_cast<std::size_t>(s.sel)];
    ModLog(s, "Removing " + r.name + "...");
    ModsStart(s, kModRemove, {L"remove", WideOf(r.id), L"--home", s.home});
}

void ModsOnLine(ModsState& s, Job job, const std::string& line) {
    if (job == kModList && IsJsonObject(line)) {
        s.rows.push_back({JsonString(line, "id"), JsonString(line, "name"), JsonString(line, "version"),
                          JsonString(line, "kind"), JsonString(line, "base"), JsonString(line, "state"),
                          JsonString(line, "needs"), JsonString(line, "problem"), JsonString(line, "author"),
                          JsonString(line, "url"), JsonString(line, "credit"), JsonString(line, "language"),
                          JsonString(line, "carries"), JsonString(line, "lacks"), JsonStrings(line, "behaviours"),
                          JsonStrings(line, "missing"), JsonTrue(line, "unverified"), JsonString(line, "known_now"),
                          JsonStrings(line, "languages")});
        return;
    }
    if (job == kModAdd && IsJsonObject(line)) {
        if (JsonTrue(line, "ok")) {
            const std::string replaced = JsonString(line, "replaced");
            const std::string author = JsonString(line, "author");
            ModLog(s, "Added: " + JsonString(line, "name") + " " + JsonString(line, "version") +
                          (author.empty() ? std::string() : ", by " + author) +
                          (replaced.empty() ? std::string() : " (replaces " + replaced + ")") + ".");
            if (JsonTrue(line, "unverified")) ModLog(s, kModUnverified);
            s.build_next = JsonString(line, "id");
            s.added_kind = JsonString(line, "kind");
            if (JsonString(line, "state") == "needs") {
                ModLog(s, "It needs " + JsonString(line, "needs") +
                              " text. This install does not have it. Add a disc in that language on Data, Sources. "
                              "Then the mod builds.");
                s.build_next.clear();
            } else if (JsonString(line, "state") == "needs-disc") {
                ModLog(s, "It needs the disc " + JsonString(line, "needs") +
                              ". This install does not have it. Add that disc on Data, Sources. Then the mod builds.");
                s.build_next.clear();
            }
            // a mod added is on: an id the list of mods to keep off still has (the mod was there before) goes out
            if (ConfigListHas(s.off, s.build_next.empty() ? JsonString(line, "id") : s.build_next)) {
                s.off = ConfigListSet(s.off, JsonString(line, "id"), false);
                TellOff(s);
            }
        } else {
            ModLog(s, "Not added: " + JsonString(line, "problem"));
        }
        return;
    }
    if (line.compare(0, 6, "error ") == 0) {
        ModLog(s, "Error: " + line.substr(6));
        return;
    }
    if (!line.empty()) ModLog(s, line);
}

void ModsOnDone(ModsState& s, Job job, std::uint32_t code) {
    s.job = kNone;
    switch (job) {
    case kModList:
        // ModsRender: the selection kept while the list has that row
        if (s.sel >= static_cast<int>(s.rows.size())) s.sel = -1;
        if (code != 0) ModLog(s, "Cannot read the mod list (tools/mods.py exit " + std::to_string(code) + ").");
        if (s.rows.empty()) s.status = "No mods are installed.";   // else what the last step said stays
        if (s.changed) {
            s.changed = false;
            ++s.p->mods_stamp;   // kMsgModsChanged: Settings' Row above the text box is made again
        }
        return;
    case kModAdd:
        if (code == 0) {
            s.changed = true;
            s.path.clear();
            // installed, built or not: Settings' Row above the text box shows (the owner's call, 2026-10-22)
            if (s.added_kind == "reading-row") {
                // kMsgModsReadingRowAdded: Extras' toggle on and the box shown at once. The add is on
                // disk already, so the toggle is kept too: into the settings, saved on any way out.
                s.p->ruby_ui = true;
                s.p->cfg->ruby_ui = true;
                s.p->cfg->ruby_ui_named = true;
                s.p->applied = true;
            }
            if (!s.build_next.empty()) {
                s.status = "Building the mod...";
                ModsStart(s, kModBuild, {L"build", WideOf(s.build_next), L"--home", s.home});
                return;
            }
            s.status = "Added.";
        } else {
            s.status = "Nothing was added. See the log below.";
        }
        break;
    case kModBuild:
        s.told.clear();   // its report is new
        s.status = code == 0 && s.added_kind != "reading-row" ? "The mod is ready. It is on."
                   : code == 0 ? "The mod is ready. Select it in Settings, Row above the text."
                   : code == 2 ? "No install is built yet. The mod builds with the install (Data, Sources)."
                               : "The mod is installed but not built. See the log below.";
        break;
    case kModRemove:
        s.changed = true;
        s.status = code == 0 ? "Removed." : "Not removed. See the log below.";
        break;
    default:
        break;
    }
    ModsList(s);
}

}  // namespace

void ModsOpen(Pages& p) {
    // A window opened again (after the Sources page rebuilt the install) has a
    // new page; a child of the old one is ended, as the Win32 page's WM_DESTROY.
    g_run.Stop();
    ModsState& s = g_m;
    s = ModsState{};
    s.p = &p;
    s.off = p.cfg->mods_off;   // kMsgModsOff
    s.rt = FindRuntime();
    s.home = HomeDir();
    if (s.rt.error.empty()) {
        s.script = s.rt.script.substr(0, s.rt.script.find_last_of(L"\\/")) + ToolSep(L"\\mods.py");
        if (!bof3::os::IsFileW(s.script))
            s.rt.error = L"The mod tool is missing (no mods.py next to " + s.rt.script +
                         L"). Extract all files of the release, not only the launcher.";
    }
    if (!s.rt.error.empty()) {
        s.status = "You cannot add mods: the importer cannot run.";
        ModLog(s, Utf8Of(s.rt.error));
    }
}

void ModsPoll(Pages&) {
    ModsState& s = g_m;
    if (s.job == kNone) return;
    const Job job = s.job;
    for (std::string line; g_run.NextLine(&line);) ModsOnLine(s, job, line);
    if (std::uint32_t code = 1; g_run.Ended(&code)) ModsOnDone(s, job, code);
}

void ModsPage(Pages& p) {
    ModsState& s = g_m;
    s.p = &p;
    if (!s.listed && s.rt.error.empty()) ModsList(s);   // WM_SHOWWINDOW: the list, the first time it is shown
    const std::string language = PagesLanguageChosen(p);   // kMsgModsLanguage: Settings' choice as it stands
    const bool runnable = s.job == kNone && s.rt.error.empty();
    const char* why = !s.rt.error.empty() ? "The importer cannot run." : "The mod tool is running.";

    GroupBegin("Mods", 0, 0, Dx(336), Dy(174));
    Place(Dx(9), Dy(12));
    Size(Dx(320));
    Label("A mod is a .zip file with a mod.toml and data files, or the patch file of a romhack (.xdelta or .ppf), which "
          "can be in a .zip or .7z archive. A mod cannot contain programs. The launcher checks each mod, then builds it "
          "with your copies of the game.");

    std::vector<ListRow> rows;
    for (const ModRow& r : s.rows) {
        ListRow row;
        bool greyed = false;
        row.text = ModRowText(s, language, r, greyed);
        row.grey = greyed;
        row.box = ModOnOff(r);
        row.ticked = row.box && !ConfigListHas(s.off, r.id);
        rows.push_back(row);
    }
    Place(Dx(9), Dy(42));
    Size(Dx(320), Dy(44));
    int toggled = -1;
    // the columns: name, author, version, state - at fixed shares of the list's width (ModsDraw)
    if (CheckList("##mods", &s.sel, rows, {0.30f, 0.50f, 0.58f}, &toggled)) {
        ModsCredit(s);   // LBN_SELCHANGE
        ModsReport(s);
    }
    if (toggled >= 0) ModsToggle(s, language, static_cast<std::size_t>(toggled));   // kMsgModsToggle

    Place(Dx(279), Dy(89));
    Size(Dx(50), Dy(14));
    if (!runnable || s.sel < 0) Grey(runnable ? "Select a mod first." : why);
    if (Button("Remove")) ModsRemove(s);

    Place(Dx(9), Dy(105));
    Label("Add a mod (.zip, .7z, .xdelta or .ppf):");
    Place(Dx(9), Dy(115));
    Size(Dx(210));
    if (!runnable) Grey(why);
    Edit("##modpath", &s.path);
    Place(Dx(223), Dy(114));
    Size(Dx(50), Dy(14));
    if (!runnable) Grey(why);
    if (Button("Browse..."))
        PickFile(kAskMod, "Mods (*.zip, *.7z, *.xdelta, *.ppf)", "zip;7z;xdelta;ppf",
                 "Select a mod (a .zip with a mod.toml, or the patch file or the archive of a romhack)");
    if (std::string picked; Picked(kAskMod, &picked) && !picked.empty()) {
        s.path = picked;
        ModsAdd(s);   // ModsBrowse: a file chosen is added at once
    }
    Place(Dx(279), Dy(114));
    Size(Dx(50), Dy(14));
    if (!runnable) Grey(why);
    if (Button("Add")) ModsAdd(s);

    Place(Dx(9), Dy(131));
    Size(Dx(320));
    LabelCut(s.status.c_str(), false);
    Place(Dx(9), Dy(141));
    Size(Dx(320), Dy(29));
    LogText("modlog", s.log);
    GroupEnd();
}

std::string ModsText() {
    const ModsState& s = g_m;
    std::string out;
    const std::string language = s.p ? PagesLanguageChosen(*s.p) : std::string();
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        const ModRow& r = s.rows[i];
        bool greyed = false;
        std::string text = ModRowText(s, language, r, greyed);
        for (char& c : text)
            if (c == '\t') c = '|';
        out += "mod " + std::to_string(i) + ": " + (ModOnOff(r) ? (ConfigListHas(s.off, r.id) ? "[ ] " : "[x] ") : "") +
               text + (greyed ? " (grey)" : "") + "\n";
    }
    out += "mods_off: " + s.off + "\n";
    out += "status: " + s.status + "\n";
    std::size_t from = 0;
    while (from < s.log.size()) {
        std::size_t end = s.log.find('\n', from);
        if (end == std::string::npos) end = s.log.size();
        out += "log: " + s.log.substr(from, end - from) + "\n";
        from = end + 1;
    }
    return out;
}

}  // namespace bof3x::ui
