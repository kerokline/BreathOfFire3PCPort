// The Sources page (sources_dialog.h, docs/install-sources.md). A Win32
// DIALOGEX like the settings (launcher.rc IDD_SOURCES): no toolkit.
//
// Every check and every build is tools/importer.py's, run as a child process
// with no window; its stdout and stderr come back merged through one pipe, a
// line at a time, on a worker thread that posts each line here. The contract
// (docs/install-sources.md section 3):
//
//   sources --home H list --json      one identify-shaped JSON object a line
//   sources --home H add PATH --json  one object; exit 0 added, 1 refused
//   sources --home H remove PATH      exit 0 removed
//   setup --home H --json-progress    "progress <done> <total> <text>" lines,
//                                     then "done game=... cache=..." or
//                                     "error <text>" and a non-zero exit
//
// An identify-shaped object is flat: path, ok, build, kind, label, problem.
// Only one child runs at a time; the page is greyed while it does.
#include "launcher/sources_dialog.h"

#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/home.h"
#include "launcher/resource.h"

namespace bof3x {
namespace {

constexpr UINT kMsgLine = WM_APP + 1;   // wParam: the job; lParam: a new std::wstring, one line
constexpr UINT kMsgDone = WM_APP + 2;   // wParam: the job; lParam: the child's exit code

enum Job : int { kNone, kList, kAdd, kRemove, kSetup };

constexpr const wchar_t* kFfmpegUrl = L"https://ffmpeg.org/download.html#build-windows";

// ---- Where the importer is --------------------------------------------------

struct Runtime {
    std::wstring python, script;
    std::wstring error;   // empty: found
};

bool IsFile(const std::wstring& path) {
    const DWORD at = GetFileAttributesW(path.c_str());
    return at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring EnvString(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
    return n > 0 && n < MAX_PATH ? std::wstring(buf, n) : std::wstring();
}

std::wstring LauncherDir() {
    wchar_t self[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    std::wstring dir(self, n);
    return dir.substr(0, dir.find_last_of(L"\\/"));
}

// The release's runtime\ beside the launcher first: its own Python and the
// importer with the fixtures it reads (tools/make_release.py). Else a source
// checkout's tools\importer.py - the launcher sits in build\, so up to three
// folders up - on BOF3X_PYTHON or the python.exe on PATH. BOF3X_PYTHON wins
// over the bundled interpreter too, for testing a release against another.
Runtime FindRuntime() {
    Runtime rt;
    const std::wstring dir = LauncherDir();
    const std::wstring env_python = EnvString(L"BOF3X_PYTHON");
    if (IsFile(dir + L"\\runtime\\tools\\importer.py")) {
        rt.script = dir + L"\\runtime\\tools\\importer.py";
        rt.python = !env_python.empty() ? env_python : dir + L"\\runtime\\python\\python.exe";
    } else {
        std::wstring up = dir;
        for (int i = 0; i < 4 && !up.empty(); ++i) {
            if (IsFile(up + L"\\tools\\importer.py")) {
                rt.script = up + L"\\tools\\importer.py";
                break;
            }
            const size_t cut = up.find_last_of(L"\\/");
            if (cut == std::wstring::npos) break;
            up = up.substr(0, cut);
        }
        if (!env_python.empty()) {
            rt.python = env_python;
        } else {
            wchar_t found[MAX_PATH];
            const DWORD n = SearchPathW(nullptr, L"python.exe", nullptr, MAX_PATH, found, nullptr);
            if (n > 0 && n < MAX_PATH) rt.python.assign(found, n);
        }
    }
    if (rt.script.empty())
        rt.error = L"The importer is missing: there is no runtime\\tools\\importer.py beside the launcher. "
                   L"Unzip the whole release, not just the launcher.";
    else if (rt.python.empty() || !IsFile(rt.python))
        rt.error = L"The importer's Python is missing (" + (rt.python.empty() ? std::wstring(L"none on PATH") : rt.python) +
                   L"). Unzip the whole release, not just the launcher.";
    return rt;
}

// ---- The child process ------------------------------------------------------

// One argument for CreateProcess's command line, by the C runtime's rules:
// backslashes are literal except before a quote.
std::wstring Quote(const std::wstring& arg) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c;
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}

std::wstring Widen(const std::string& s) { return ConfigFromUtf8(s); }

struct Reader {
    HWND dlg;
    Job job;
    HANDLE pipe;
    HANDLE process;
};

void PostLine(HWND dlg, Job job, const std::string& bytes) {
    std::string line = bytes;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    PostMessageW(dlg, kMsgLine, job, reinterpret_cast<LPARAM>(new std::wstring(Widen(line))));
}

DWORD WINAPI ReadChild(void* param) {
    Reader* r = static_cast<Reader*>(param);
    std::string pending;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(r->pipe, buf, sizeof buf, &got, nullptr) && got > 0) {
        pending.append(buf, got);
        size_t at;
        while ((at = pending.find('\n')) != std::string::npos) {
            PostLine(r->dlg, r->job, pending.substr(0, at));
            pending.erase(0, at + 1);
        }
    }
    if (!pending.empty()) PostLine(r->dlg, r->job, pending);
    WaitForSingleObject(r->process, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(r->process, &code);
    CloseHandle(r->pipe);
    PostMessageW(r->dlg, kMsgDone, r->job, static_cast<LPARAM>(code));
    delete r;
    return 0;
}

// Sets a variable for the child and puts the launcher's own back afterwards:
// the game, started later from this process, inherits nothing of it.
class ScopedEnv {
public:
    void Set(const wchar_t* name, const std::wstring& value) {
        saved_.push_back({name, EnvString(name), GetEnvironmentVariableW(name, nullptr, 0) > 0});
        SetEnvironmentVariableW(name, value.c_str());
    }
    ~ScopedEnv() {
        for (auto it = saved_.rbegin(); it != saved_.rend(); ++it)
            SetEnvironmentVariableW(it->name, it->had ? it->value.c_str() : nullptr);
    }

private:
    struct Saved {
        const wchar_t* name;
        std::wstring value;
        bool had;
    };
    std::vector<Saved> saved_;
};

// ---- A flat JSON object, as the importer prints one -------------------------

void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Where key's value starts, or npos. The keys are the importer's own and the
// values never hold `"key":`, so a plain search is enough.
size_t JsonValueAt(const std::string& line, const char* key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t at = line.find(k);
    if (at == std::string::npos) return at;
    at += k.size();
    while (at < line.size() && line[at] == ' ') ++at;
    if (at >= line.size() || line[at] != ':') return std::string::npos;
    ++at;
    while (at < line.size() && line[at] == ' ') ++at;
    return at < line.size() ? at : std::string::npos;
}

bool IsJsonObject(const std::string& line) { return !line.empty() && line.front() == '{' && line.back() == '}'; }

// A string value, unescaped to UTF-8; empty when absent or null.
std::string JsonString(const std::string& line, const char* key) {
    size_t at = JsonValueAt(line, key);
    std::string out;
    if (at == std::string::npos || line[at] != '"') return out;
    for (++at; at < line.size() && line[at] != '"'; ++at) {
        if (line[at] != '\\' || at + 1 >= line.size()) {
            out += line[at];
            continue;
        }
        const char e = line[++at];
        if (e == 'n') out += '\n';
        else if (e == 't') out += '\t';
        else if (e == 'r') out += '\r';
        else if (e == 'b' || e == 'f') continue;
        else if (e == 'u' && at + 4 < line.size()) {
            unsigned cp = std::strtoul(line.substr(at + 1, 4).c_str(), nullptr, 16);
            at += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && at + 6 < line.size() && line[at + 1] == '\\' && line[at + 2] == 'u') {
                const unsigned lo = std::strtoul(line.substr(at + 3, 4).c_str(), nullptr, 16);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    at += 6;
                }
            }
            AppendUtf8(out, cp);
        } else {
            out += e;   // \" \\ \/
        }
    }
    return out;
}

bool JsonTrue(const std::string& line, const char* key) {
    const size_t at = JsonValueAt(line, key);
    return at != std::string::npos && line.compare(at, 4, "true") == 0;
}

// ---- The page ---------------------------------------------------------------

struct State {
    Config* cfg = nullptr;
    bool first_run = false;
    Runtime rt;
    std::wstring home;
    Job job = kNone;
    HANDLE process = nullptr;
    bool closing = false;   // Close pressed while a child ran: end once it has
    bool built = false;     // a build finished: the caller re-reads the install
    bool added = false;     // the last add succeeded: build next
    std::vector<std::wstring> paths;   // the list box's, in order
};

State* Get(HWND dlg) { return reinterpret_cast<State*>(GetWindowLongPtrW(dlg, GWLP_USERDATA)); }

std::wstring Text(HWND dlg, int id) {
    const int n = GetWindowTextLengthW(GetDlgItem(dlg, id));
    std::wstring w(static_cast<size_t>(n) + 1, L'\0');
    GetDlgItemTextW(dlg, id, w.data(), n + 1);
    w.resize(static_cast<size_t>(n));
    const size_t b = w.find_first_not_of(L" \t\""), e = w.find_last_not_of(L" \t\"");
    return b == std::wstring::npos ? std::wstring() : w.substr(b, e - b + 1);
}

void Log(HWND dlg, const std::wstring& line) {
    HWND log = GetDlgItem(dlg, IDC_SO_LOG);
    const int end = GetWindowTextLengthW(log);
    SendMessageW(log, EM_SETSEL, end, end);
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>((line + L"\r\n").c_str()));
}

void Status(HWND dlg, const std::wstring& text) { SetDlgItemTextW(dlg, IDC_SO_STATUS, text.c_str()); }

void Greyed(HWND dlg, const State& s) {
    const bool idle = s.job == kNone, runnable = idle && s.rt.error.empty();
    for (int id : {IDC_SO_PATH, IDC_SO_FILE, IDC_SO_FOLDER, IDC_SO_ADD}) EnableWindow(GetDlgItem(dlg, id), runnable);
    EnableWindow(GetDlgItem(dlg, IDC_SO_BUILD), runnable && !s.paths.empty());
    EnableWindow(GetDlgItem(dlg, IDC_SO_REMOVE),
                 runnable && SendDlgItemMessageW(dlg, IDC_SO_LIST, LB_GETCURSEL, 0, 0) != LB_ERR);
    EnableWindow(GetDlgItem(dlg, IDC_SO_FFLOCATE), idle);
    SetDlgItemTextW(dlg, IDCANCEL, idle ? L"&Close" : L"&Stop");
}

void FfmpegNote(HWND dlg, const Config& cfg) {
    std::wstring note;
    const std::wstring located = ConfigFromUtf8(cfg.ffmpeg), env = EnvString(L"BOF3X_FFMPEG");
    wchar_t found[MAX_PATH];
    if (!located.empty() && IsFile(located)) note = L"ffmpeg: " + located;
    else if (!env.empty() && IsFile(env)) note = L"ffmpeg: " + env + L" (BOF3X_FFMPEG)";
    else if (const DWORD n = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr); n > 0 && n < MAX_PATH)
        note = L"ffmpeg: " + std::wstring(found, n) + L" (on PATH)";
    else
        note = L"ffmpeg not found. Only a PSP disc's music needs it; without it that music is left out.";
    SetDlgItemTextW(dlg, IDC_SO_FFNOTE, note.c_str());
}

bool Start(HWND dlg, State& s, Job job, const std::wstring& args) {
    if (!s.rt.error.empty() || s.job != kNone) return false;
    std::wstring line = Quote(s.rt.python) + L" " + Quote(s.rt.script) + L" " + args;

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        Log(dlg, L"Cannot make a pipe for the importer (error " + std::to_wstring(GetLastError()) + L").");
        return false;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    BOOL ok;
    {
        ScopedEnv env;
        env.Set(L"PYTHONUTF8", L"1");
        env.Set(L"PYTHONIOENCODING", L"utf-8");
        env.Set(L"PYTHONUNBUFFERED", L"1");
        if (!s.cfg->ffmpeg.empty()) env.Set(L"BOF3X_FFMPEG", ConfigFromUtf8(s.cfg->ffmpeg));
        ok = CreateProcessW(s.rt.python.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            s.home.c_str(), &si, &pi);
    }
    const DWORD err = GetLastError();
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        CloseHandle(rd);
        Log(dlg, L"Cannot start the importer: " + s.rt.python + L" (error " + std::to_wstring(err) + L").");
        return false;
    }
    CloseHandle(pi.hThread);
    s.job = job;
    s.process = pi.hProcess;
    HANDLE t = CreateThread(nullptr, 0, ReadChild, new Reader{dlg, job, rd, pi.hProcess}, 0, nullptr);
    if (t) CloseHandle(t);
    Greyed(dlg, s);
    return true;
}

std::wstring HomeArg(const State& s) { return L"--home " + Quote(s.home); }

void StartList(HWND dlg, State& s) {
    SendDlgItemMessageW(dlg, IDC_SO_LIST, LB_RESETCONTENT, 0, 0);
    s.paths.clear();
    Start(dlg, s, kList, L"sources " + HomeArg(s) + L" list --json");
}

void StartSetup(HWND dlg, State& s) {
    SendDlgItemMessageW(dlg, IDC_SO_PROGRESS, PBM_SETPOS, 0, 0);
    Status(dlg, L"Building the install...");
    Log(dlg, L"Building the install in " + s.home + L"...");
    Start(dlg, s, kSetup, L"setup " + HomeArg(s) + L" --json-progress");
}

void OnLine(HWND dlg, State& s, Job job, const std::wstring& wline) {
    const std::string line = ConfigToUtf8(wline);
    if (job == kList && IsJsonObject(line)) {
        const std::wstring path = Widen(JsonString(line, "path")), label = Widen(JsonString(line, "label"));
        std::wstring row = (label.empty() ? std::wstring(L"(unknown)") : label) + L"   " + path;
        if (!JsonTrue(line, "ok")) row = L"[problem] " + row;
        SendDlgItemMessageW(dlg, IDC_SO_LIST, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
        s.paths.push_back(path);
        return;
    }
    if (job == kAdd && IsJsonObject(line)) {
        const std::wstring label = Widen(JsonString(line, "label")), problem = Widen(JsonString(line, "problem"));
        s.added = JsonTrue(line, "ok");
        if (s.added) Log(dlg, L"Recognised: " + label + L".");
        else Log(dlg, L"Not added: " + (problem.empty() ? std::wstring(L"not a release this project knows.") : problem));
        return;
    }
    if (job == kSetup && line.compare(0, 9, "progress ") == 0) {
        unsigned done = 0, total = 0;
        int used = 0;
        if (std::sscanf(line.c_str() + 9, "%u %u %n", &done, &total, &used) >= 2 && total > 0) {
            SendDlgItemMessageW(dlg, IDC_SO_PROGRESS, PBM_SETRANGE32, 0, total);
            SendDlgItemMessageW(dlg, IDC_SO_PROGRESS, PBM_SETPOS, done, 0);
            const std::wstring what = Widen(line.substr(9 + static_cast<size_t>(used)));
            Status(dlg, L"Step " + std::to_wstring(done) + L" of " + std::to_wstring(total) + L": " + what);
            Log(dlg, what);
            return;
        }
    }
    if (job == kSetup && line.compare(0, 6, "error ") == 0) {
        Log(dlg, L"Error: " + Widen(line.substr(6)));
        return;
    }
    if (!wline.empty()) Log(dlg, wline);
}

void OnDone(HWND dlg, State& s, Job job, DWORD code) {
    CloseHandle(s.process);
    s.process = nullptr;
    s.job = kNone;
    Greyed(dlg, s);   // idle until a next child starts, if one does
    if (s.closing) {
        EndDialog(dlg, s.built ? 1 : 0);
        return;
    }
    switch (job) {
    case kList:
        if (code != 0) Log(dlg, L"The importer could not read the source list (exit " + std::to_wstring(code) + L").");
        else if (s.paths.empty()) Status(dlg, L"No sources yet.");
        break;
    case kAdd:
        if (code == 0 && s.added) {
            SetDlgItemTextW(dlg, IDC_SO_PATH, L"");
            StartSetup(dlg, s);   // added: the install is rebuilt with it at once
            return;
        }
        Status(dlg, L"That source was not added; see below.");
        break;
    case kRemove:
        Status(dlg, code == 0 ? L"Removed. Build the install again for it to take effect." : L"Not removed; see below.");
        StartList(dlg, s);
        return;
    case kSetup:
        if (code == 0) {
            s.built = true;
            SendDlgItemMessageW(dlg, IDC_SO_PROGRESS, PBM_SETRANGE32, 0, 1);
            SendDlgItemMessageW(dlg, IDC_SO_PROGRESS, PBM_SETPOS, 1, 0);
            Status(dlg, L"The install is ready.");
            Log(dlg, L"The install is ready.");
            if (!s.cfg->cache.empty())
                Log(dlg, L"Note: your settings name another cache folder (cache=); empty the Cache folder box in "
                         L"Settings to use this install's own.");
            if (s.first_run) {   // nothing else to do here: on to the settings
                EndDialog(dlg, 1);
                return;
            }
        } else {
            Status(dlg, L"The build failed; the install as it was is unchanged.");
        }
        StartList(dlg, s);
        return;
    default:
        break;
    }
    Greyed(dlg, s);
}

void PickFile(HWND dlg) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = dlg;
    ofn.lpstrFilter = L"Game sources (*.cue, *.bin, *.iso, *.zip, BOF3.exe)\0*.cue;*.bin;*.iso;*.zip;BOF3.exe\0"
                      L"All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"A disc image, a .zip of one, or the PC game's BOF3.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) SetDlgItemTextW(dlg, IDC_SO_PATH, path);
}

void PickFolder(HWND dlg) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    BROWSEINFOW bi{};
    bi.hwndOwner = dlg;
    bi.lpszTitle = L"The PC game's folder (the one holding BOF3.exe and DAT)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) SetDlgItemTextW(dlg, IDC_SO_PATH, path);
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(co)) CoUninitialize();
}

void LocateFfmpeg(HWND dlg, State& s) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = dlg;
    ofn.lpstrFilter = L"ffmpeg.exe\0ffmpeg.exe\0Programs (*.exe)\0*.exe\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Locate ffmpeg.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        s.cfg->ffmpeg = ConfigToUtf8(path);
        FfmpegNote(dlg, *s.cfg);
    }
}

void Add(HWND dlg, State& s) {
    std::wstring path = Text(dlg, IDC_SO_PATH);
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    if (path.empty()) {
        Status(dlg, L"Type or pick a path first.");
        return;
    }
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Status(dlg, L"There is nothing at that path.");
        return;
    }
    s.added = false;
    Status(dlg, L"Checking the source against the known releases (a disc takes a minute)...");
    Log(dlg, L"Checking " + path + L"...");
    Start(dlg, s, kAdd, L"sources " + HomeArg(s) + L" add " + Quote(path) + L" --json");
}

void Remove(HWND dlg, State& s) {
    const LRESULT i = SendDlgItemMessageW(dlg, IDC_SO_LIST, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR || static_cast<size_t>(i) >= s.paths.size()) return;
    Start(dlg, s, kRemove, L"sources " + HomeArg(s) + L" remove " + Quote(s.paths[static_cast<size_t>(i)]));
}

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* s = reinterpret_cast<State*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        SetDlgItemTextW(dlg, IDC_SO_INTRO,
                        s->first_run
                            ? L"No install was found. Add your own copy of the game to make one: the PC game's folder, "
                              L"a PlayStation disc image (.cue/.bin, or a .zip of it) or a PSP .iso. Each is checked "
                              L"against the known releases before anything is built."
                            : L"The copies of the game this install is built from. Adding one checks it against the "
                              L"known releases and rebuilds the install with it.");
        SetDlgItemTextW(dlg, IDC_SO_HOME, (L"Install folder: " + s->home).c_str());
        FfmpegNote(dlg, *s->cfg);
        if (!s->rt.error.empty()) {
            Status(dlg, L"The importer cannot run.");
            Log(dlg, s->rt.error);
        } else {
            StartList(dlg, *s);
        }
        Greyed(dlg, *s);
        SetForegroundWindow(dlg);
        return TRUE;
    }
    case kMsgLine: {
        std::wstring* line = reinterpret_cast<std::wstring*>(lp);
        OnLine(dlg, *Get(dlg), static_cast<Job>(wp), *line);
        delete line;
        return TRUE;
    }
    case kMsgDone:
        OnDone(dlg, *Get(dlg), static_cast<Job>(wp), static_cast<DWORD>(lp));
        return TRUE;
    case WM_COMMAND: {
        State& s = *Get(dlg);
        switch (LOWORD(wp)) {
        case IDC_SO_ADD:
            Add(dlg, s);
            return TRUE;
        case IDC_SO_FILE:
            PickFile(dlg);
            return TRUE;
        case IDC_SO_FOLDER:
            PickFolder(dlg);
            return TRUE;
        case IDC_SO_REMOVE:
            Remove(dlg, s);
            return TRUE;
        case IDC_SO_BUILD:
            StartSetup(dlg, s);
            return TRUE;
        case IDC_SO_FFLOCATE:
            LocateFfmpeg(dlg, s);
            return TRUE;
        case IDC_SO_FFGET:
            ShellExecuteW(dlg, L"open", kFfmpegUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDC_SO_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE) Greyed(dlg, s);
            return TRUE;
        case IDCANCEL:
            if (s.job == kNone) {
                EndDialog(dlg, s.built ? 1 : 0);
            } else if (MessageBoxW(dlg,
                                   s.job == kSetup ? L"Stop the build? The install as it was is kept."
                                                   : L"Stop the importer?",
                                   L"Sources", MB_YESNO | MB_ICONQUESTION) == IDYES && s.process) {
                s.closing = true;
                TerminateProcess(s.process, 1);   // the reader posts kMsgDone, which ends the page
            }
            return TRUE;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

}  // namespace

bool SourcesDialogRun(HWND owner, Config& cfg, bool first_run) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    State s;
    s.cfg = &cfg;
    s.first_run = first_run;
    s.rt = FindRuntime();
    s.home = HomeDir();
    const INT_PTR r = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SOURCES), owner, Proc,
                                      reinterpret_cast<LPARAM>(&s));
    if (r == -1) {
        MessageBoxW(owner, L"The Sources page could not be created.", L"bof3x-launcher", MB_OK | MB_ICONERROR);
        return false;
    }
    return s.built;
}

}  // namespace bof3x
