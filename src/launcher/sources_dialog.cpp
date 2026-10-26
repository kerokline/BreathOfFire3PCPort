// The Sources page (sources_dialog.h, docs/install-sources.md). A Win32
// DIALOGEX like the settings (launcher.rc IDD_SOURCES): no toolkit.
//
// Every check and every build is tools/importer.py's, run as a child process
// with no window; its stdout and stderr come back merged through one pipe, a
// line at a time, on a worker thread that posts each line here. The contract
// (docs/install-sources.md section 3):
//
//   sources --home H list --json      one identify-shaped JSON object a line
//   sources --home H add PATH --json  one object; exit 0 added, 1 refused. A
//                                     folder that is not the PC game's is
//                                     searched: an object for each source in
//                                     it, exit 0 when any was added
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

enum Job : int { kNone, kList, kAdd, kRemove, kSetup, kModList, kModAdd, kModBuild, kModRemove };

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
        rt.error = L"The importer is missing (runtime\\tools\\importer.py). "
                   L"Extract all files of the release, not only the launcher.";
    else if (rt.python.empty() || !IsFile(rt.python))
        rt.error = L"The importer cannot find Python (" + (rt.python.empty() ? std::wstring(L"none on PATH") : rt.python) +
                   L"). Extract all files of the release, not only the launcher.";
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

// RunCapture's watch: ends a child still running after `timeout` ms.
struct WatchCtx {
    HANDLE process;
    DWORD timeout;
    bool killed;
};
DWORD WINAPI Watch(void* p) {
    auto* c = static_cast<WatchCtx*>(p);
    if (WaitForSingleObject(c->process, c->timeout) == WAIT_TIMEOUT) {
        c->killed = true;
        TerminateProcess(c->process, 1);
    }
    return 0;
}

// Runs the importer with `args` to the end, with no window, and returns its
// merged output and exit code; false when it could not start or ran past
// `timeout_ms` (then ended). For the one short command the launcher waits on
// before the game (`stale`); the page's own commands run on ReadChild.
bool RunCapture(const Runtime& rt, const std::wstring& args, const std::wstring& cwd, DWORD timeout_ms,
                std::string& out, DWORD& code) {
    std::wstring line = Quote(rt.python) + L" " + Quote(rt.script) + L" " + args;
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
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
        ok = CreateProcessW(rt.python.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            cwd.c_str(), &si, &pi);
    }
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        CloseHandle(rd);
        return false;
    }
    CloseHandle(pi.hThread);
    // Read on this thread until the child closes its end: the pipe never fills.
    // The timeout ends a child that hangs; its end of the pipe closes with it.
    WatchCtx ctx{pi.hProcess, timeout_ms, false};
    HANDLE watch = CreateThread(nullptr, 0, Watch, &ctx, 0, nullptr);
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got > 0) out.append(buf, got);
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (watch) {
        WaitForSingleObject(watch, INFINITE);
        CloseHandle(watch);
    }
    code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return !ctx.killed;
}

// The stamp a fresh answer is remembered by: the launcher's and the
// importer's file sizes and write times. A new release, or a rebuilt source
// checkout, changes one of them, and the install is judged again.
std::wstring StaleStamp(const Runtime& rt) {
    std::wstring stamp;
    wchar_t self[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, self, MAX_PATH);
    for (const std::wstring& path : {std::wstring(self, n < MAX_PATH ? n : 0), rt.script}) {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (path.empty() || !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return std::wstring();
        wchar_t part[96];
        std::swprintf(part, 96, L"%08lx%08lx %08lx%08lx\n", a.nFileSizeHigh, a.nFileSizeLow,
                      a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime);
        stamp += part;
    }
    return stamp;
}

std::wstring ReadSmall(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return std::wstring();
    char buf[512];
    const size_t n = std::fread(buf, 1, sizeof buf, f);
    std::fclose(f);
    return ConfigFromUtf8(std::string(buf, n));
}

// A step of the cache (tools/build_reuse.py's names) as the player reads it.
std::wstring PieceName(const std::string& step) {
    struct Row { const char* step; const wchar_t* name; };
    static const Row kRows[] = {
        {"cache", L"the whole install"},   {"base", L"the game data"},
        {"exe", L"the game's program data"}, {"bgm", L"the music"},
        {"snd", L"the sound effects"},     {"snd-psp", L"the sound effects"},
        {"manifest", L"the install's index"}, {"fmv", L"the logo movie"},
        {"opt", L"the optional layers"},   {"game-pc", L"the game folder"},
        {"game-install", L"the game folder"},
    };
    for (const Row& r : kRows)
        if (step == r.step) return r.name;
    if (step.compare(0, 4, "loc-") == 0) return L"the " + ConfigFromUtf8(step.substr(4)) + L" text";
    if (step.compare(0, 4, "opt-") == 0) return L"the optional layer " + ConfigFromUtf8(step.substr(4));
    // A second-language or mod reading-row layer: its step is its layer's name.
    if (step.compare(0, 5, "ruby-") == 0) return L"the reading-row layer " + ConfigFromUtf8(step.substr(5));
    return ConfigFromUtf8(step);
}

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
    bool rebuild = false;   // a stale install the player agreed to rebuild: build at once, end when done
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
        note = L"ffmpeg was not found. Only PSP disc music needs it. Without ffmpeg, the install has no PSP music.";
    SetDlgItemTextW(dlg, IDC_SO_FFNOTE, note.c_str());
}

// Starts `script args` on the runtime's Python with no window, in `cwd`; its
// merged output comes back to `dlg` a line at a time (kMsgLine), then its exit
// code (kMsgDone), both tagged with `job`. False with `error` set when it
// could not start. The Sources page's children and the Mods page's.
bool StartChild(HWND dlg, Job job, const Runtime& rt, const std::wstring& script, const std::wstring& args,
                const std::wstring& cwd, const std::string& ffmpeg, HANDLE& process, std::wstring& error) {
    std::wstring line = Quote(rt.python) + L" " + Quote(script) + L" " + args;
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        error = L"Cannot make a pipe for the importer (error " + std::to_wstring(GetLastError()) + L").";
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
        if (!ffmpeg.empty()) env.Set(L"BOF3X_FFMPEG", ConfigFromUtf8(ffmpeg));
        ok = CreateProcessW(rt.python.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            cwd.c_str(), &si, &pi);
    }
    const DWORD err = GetLastError();
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        CloseHandle(rd);
        error = L"Cannot start the importer: " + rt.python + L" (error " + std::to_wstring(err) + L").";
        return false;
    }
    CloseHandle(pi.hThread);
    process = pi.hProcess;
    HANDLE t = CreateThread(nullptr, 0, ReadChild, new Reader{dlg, job, rd, pi.hProcess}, 0, nullptr);
    if (t) CloseHandle(t);
    return true;
}

bool Start(HWND dlg, State& s, Job job, const std::wstring& args) {
    if (!s.rt.error.empty() || s.job != kNone) return false;
    std::wstring error;
    if (!StartChild(dlg, job, s.rt, s.rt.script, args, s.home, s.cfg->ffmpeg, s.process, error)) {
        Log(dlg, error);
        return false;
    }
    s.job = job;
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
        // A folder of images answers once for each source in it.
        const std::wstring path = Widen(JsonString(line, "path"));
        const bool ok = JsonTrue(line, "ok");
        s.added = s.added || ok;
        if (ok) Log(dlg, L"Recognised: " + label + L" (" + path + L").");
        else Log(dlg, L"Not added: " + path + L": " +
                      (problem.empty() ? std::wstring(L"this release is not known.") : problem));
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
        Status(dlg, L"Nothing was added. See the log below.");
        break;
    case kRemove:
        Status(dlg, code == 0 ? L"Removed. Build the install again to apply the change." : L"Not removed. See the log below.");
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
                Log(dlg, L"Note: your settings use a different cache folder (cache=). To use the cache of this install, "
                         L"clear the Cache folder box on the Data tab.");
            if (s.first_run || s.rebuild) {   // nothing else to do here: on to the settings, or the game
                EndDialog(dlg, 1);
                return;
            }
        } else {
            Status(dlg, L"The build failed. The install did not change.");
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
    ofn.lpstrTitle = L"Select a disc image, a .zip of a disc image, or BOF3.exe of the PC game";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) SetDlgItemTextW(dlg, IDC_SO_PATH, path);
}

void PickFolder(HWND dlg) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    BROWSEINFOW bi{};
    bi.hwndOwner = dlg;
    bi.lpszTitle = L"Select the PC game folder (with BOF3.exe and DAT) or a folder of disc images. "
                   L"The importer adds each known release";
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
        Status(dlg, L"Type or select a path first.");
        return;
    }
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Status(dlg, L"Nothing is at that path.");
        return;
    }
    s.added = false;
    Status(dlg, L"Checking the copy (approximately 1 minute for each disc)...");
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
                        s->rebuild
                            ? L"An older release built this install. The importer will build it again from the copies of the "
                              L"game below. It keeps the parts that do not change."
                        : s->first_run
                            ? L"No install was found. To make one, add your copy of the game: the PC game folder, "
                              L"a PlayStation disc image (.cue/.bin or .zip) or a PSP .iso. The importer checks "
                              L"each copy before it builds."
                            : L"These are the copies of the game that this install uses. When you add a copy, the importer "
                              L"checks it and builds the install again.");
        SetDlgItemTextW(dlg, IDC_SO_HOME, (L"Install folder: " + s->home).c_str());
        FfmpegNote(dlg, *s->cfg);
        if (!s->rt.error.empty()) {
            Status(dlg, L"The importer cannot run.");
            Log(dlg, s->rt.error);
        } else if (s->rebuild) {
            StartSetup(dlg, *s);
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
                                   s.job == kSetup ? L"Stop the build? The install does not change."
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

// ---- The Mods tab (docs/mods.md) -----------------------------------------------
//
// tools/mods.py beside the importer, on the same Python, one child at a time.
// `list --json` gives one flat object a mod: id, name, version, kind,
// requires, base, layer, state (built, not-built, needs, problem), needs,
// problem. A mod the install cannot play is greyed in the list with its reason
// (the owner's rule, 2026-10-10): a language it needs and the install lacks,
// a layer not built, or a base that is not the language chosen in Settings.

struct ModRow {
    std::string id, name, version, kind, base, state, needs, problem;
};

struct ModsState {
    Runtime rt;
    std::wstring script, home;
    Job job = kNone;
    HANDLE process = nullptr;
    bool listed = false;    // the list has been asked for once (on first showing)
    bool changed = false;   // an add, a build or a remove ran: the window is told after the next list
    std::string build_next; // the id an add just installed: built next
    std::string added_kind; // its kind: a reading-row mod turns Extras' toggle on (kMsgModsReadingRowAdded)
    std::string language;   // Settings' choice (kMsgModsLanguage); empty for the Chinese
    std::vector<ModRow> rows;
};

ModsState* GetMods(HWND page) { return reinterpret_cast<ModsState*>(GetWindowLongPtrW(page, GWLP_USERDATA)); }

void ModLog(HWND page, const std::wstring& line) {
    HWND log = GetDlgItem(page, IDC_MOD_LOG);
    const int end = GetWindowTextLengthW(log);
    SendMessageW(log, EM_SETSEL, end, end);
    // the line break before a line, not after: the box ends on text, not on an empty line
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(((end ? L"\r\n" : L"") + line).c_str()));
    SendMessageW(log, EM_SCROLLCARET, 0, 0);
}

void ModStatus(HWND page, const std::wstring& text) { SetDlgItemTextW(page, IDC_MOD_STATUS, text.c_str()); }

void ModsGreyed(HWND page, const ModsState& s) {
    const bool runnable = s.job == kNone && s.rt.error.empty();
    for (int id : {IDC_MOD_PATH, IDC_MOD_BROWSE, IDC_MOD_ADD}) EnableWindow(GetDlgItem(page, id), runnable);
    EnableWindow(GetDlgItem(page, IDC_MOD_REMOVE),
                 runnable && SendDlgItemMessageW(page, IDC_MOD_LIST, LB_GETCURSEL, 0, 0) != LB_ERR);
}

// A row as the list shows it: name, version, kind, state; and whether it is greyed.
std::wstring ModRowText(const ModsState& s, const ModRow& r, bool& greyed) {
    std::wstring state;
    greyed = true;
    if (r.state == "built") {
        if (r.kind == "reading-row" && r.base != s.language) {
            state = L"built; only with " + Widen(r.base) + L" text";
        } else {
            state = L"built: select it in Settings";
            greyed = false;
        }
    } else if (r.state == "not-built") {
        state = L"not built: Data, Sources, Build the install";
    } else if (r.state == "needs") {
        state = L"needs " + Widen(r.needs) + L" text: add that disc on Data, Sources";
    } else {
        state = L"problem: " + Widen(r.problem.empty() ? std::string("unreadable") : r.problem);
    }
    return Widen(r.name) + L"\t" + Widen(r.version) + L"\t" + Widen(r.kind) + L"\t" + state;
}

void ModsRender(HWND page, const ModsState& s) {
    HWND list = GetDlgItem(page, IDC_MOD_LIST);
    const LRESULT sel = SendMessageW(list, LB_GETCURSEL, 0, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const ModRow& r : s.rows) {
        bool greyed = false;
        const std::wstring text = ModRowText(s, r, greyed);
        const LRESULT i = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(list, LB_SETITEMDATA, static_cast<WPARAM>(i), greyed ? 1 : 0);
    }
    if (sel != LB_ERR && static_cast<size_t>(sel) < s.rows.size()) SendMessageW(list, LB_SETCURSEL, sel, 0);
    ModsGreyed(page, s);
}

bool ModsStart(HWND page, ModsState& s, Job job, const std::wstring& args) {
    if (!s.rt.error.empty() || s.job != kNone) return false;
    std::wstring error;
    if (!StartChild(page, job, s.rt, s.script, args, s.home, std::string(), s.process, error)) {
        ModLog(page, error);
        return false;
    }
    s.job = job;
    ModsGreyed(page, s);
    return true;
}

std::wstring ModsHome(const ModsState& s) { return L"--home " + Quote(s.home); }

void ModsList(HWND page, ModsState& s) {
    s.listed = true;
    s.rows.clear();
    ModsStart(page, s, kModList, L"list " + ModsHome(s) + L" --json");
}

void ModsAdd(HWND page, ModsState& s) {
    wchar_t buf[MAX_PATH * 2] = {};
    GetDlgItemTextW(page, IDC_MOD_PATH, buf, static_cast<int>(std::size(buf)));
    std::wstring path = buf;
    const size_t b = path.find_first_not_of(L" \t\""), e = path.find_last_not_of(L" \t\"");
    path = b == std::wstring::npos ? std::wstring() : path.substr(b, e - b + 1);
    if (path.empty()) {
        ModStatus(page, L"Type or select the path of a .zip file first.");
        return;
    }
    if (!IsFile(path)) {
        ModStatus(page, L"No file is at that path.");
        return;
    }
    s.build_next.clear();
    s.added_kind.clear();
    ModStatus(page, L"Checking the mod...");
    ModLog(page, L"Checking " + path + L"...");
    ModsStart(page, s, kModAdd, L"add " + Quote(path) + L" " + ModsHome(s) + L" --json");
}

void ModsBrowse(HWND page, ModsState& s) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = page;
    ofn.lpstrFilter = L"Mods (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select a mod (a .zip with a mod.toml and data files)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    SetDlgItemTextW(page, IDC_MOD_PATH, path);
    ModsAdd(page, s);
}

void ModsRemove(HWND page, ModsState& s) {
    const LRESULT i = SendDlgItemMessageW(page, IDC_MOD_LIST, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR || static_cast<size_t>(i) >= s.rows.size()) return;
    const ModRow& r = s.rows[static_cast<size_t>(i)];
    ModLog(page, L"Removing " + Widen(r.name) + L"...");
    ModsStart(page, s, kModRemove, L"remove " + Quote(Widen(r.id)) + L" " + ModsHome(s));
}

void ModsOnLine(HWND page, ModsState& s, Job job, const std::wstring& wline) {
    const std::string line = ConfigToUtf8(wline);
    if (job == kModList && IsJsonObject(line)) {
        s.rows.push_back({JsonString(line, "id"), JsonString(line, "name"), JsonString(line, "version"),
                          JsonString(line, "kind"), JsonString(line, "base"), JsonString(line, "state"),
                          JsonString(line, "needs"), JsonString(line, "problem")});
        return;
    }
    if (job == kModAdd && IsJsonObject(line)) {
        if (JsonTrue(line, "ok")) {
            const std::string replaced = JsonString(line, "replaced");
            ModLog(page, L"Added: " + Widen(JsonString(line, "name")) + L" " + Widen(JsonString(line, "version")) +
                             (replaced.empty() ? std::wstring() : L" (replaces " + Widen(replaced) + L")") + L".");
            s.build_next = JsonString(line, "id");
            s.added_kind = JsonString(line, "kind");
            if (JsonString(line, "state") == "needs") {
                ModLog(page, L"It needs " + Widen(JsonString(line, "needs")) +
                                 L" text. This install does not have it. Add a disc in that language on Data, Sources. "
                                 L"Then the mod builds.");
                s.build_next.clear();
            }
        } else {
            ModLog(page, L"Not added: " + Widen(JsonString(line, "problem")));
        }
        return;
    }
    if (line.compare(0, 6, "error ") == 0) {
        ModLog(page, L"Error: " + Widen(line.substr(6)));
        return;
    }
    if (!wline.empty()) ModLog(page, wline);
}

void ModsOnDone(HWND page, ModsState& s, Job job, DWORD code) {
    CloseHandle(s.process);
    s.process = nullptr;
    s.job = kNone;
    switch (job) {
    case kModList:
        ModsRender(page, s);
        if (code != 0) ModLog(page, L"Cannot read the mod list (tools/mods.py exit " + std::to_wstring(code) + L").");
        if (s.rows.empty()) ModStatus(page, L"No mods are installed.");   // else what the last step said stays
        if (s.changed) {
            s.changed = false;
            SendMessageW(GetParent(page), kMsgModsChanged, 0, 0);
        }
        return;
    case kModAdd:
        if (code == 0) {
            s.changed = true;
            SetDlgItemTextW(page, IDC_MOD_PATH, L"");
            // installed, built or not: Settings' Row above the text box shows (the owner's call, 2026-10-22)
            if (s.added_kind == "reading-row") SendMessageW(GetParent(page), kMsgModsReadingRowAdded, 0, 0);
            if (!s.build_next.empty()) {
                ModStatus(page, L"Building the mod...");
                ModsStart(page, s, kModBuild, L"build " + Quote(Widen(s.build_next)) + L" " + ModsHome(s));
                return;
            }
            ModStatus(page, L"Added.");
        } else {
            ModStatus(page, L"Nothing was added. See the log below.");
        }
        break;
    case kModBuild:
        ModStatus(page, code == 0   ? L"The mod is ready. Select it in Settings, Row above the text."
                        : code == 2 ? L"No install is built yet. The mod builds with the install (Data, Sources)."
                                    : L"The mod is installed but not built. See the log below.");
        break;
    case kModRemove:
        s.changed = true;
        ModStatus(page, code == 0 ? L"Removed." : L"Not removed. See the log below.");
        break;
    default:
        break;
    }
    ModsList(page, s);
}

void ModsDraw(const DRAWITEMSTRUCT& d) {
    const bool selected = (d.itemState & ODS_SELECTED) != 0;
    FillRect(d.hDC, &d.rcItem, GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    if (d.itemID == static_cast<UINT>(-1)) return;
    const LRESULT n = SendMessageW(d.hwndItem, LB_GETTEXTLEN, d.itemID, 0);
    if (n < 0) return;
    std::wstring text(static_cast<size_t>(n) + 1, L'\0');
    SendMessageW(d.hwndItem, LB_GETTEXT, d.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(n));
    SetBkMode(d.hDC, TRANSPARENT);
    SetTextColor(d.hDC, GetSysColor(d.itemData ? COLOR_GRAYTEXT : selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    // the columns: name, version, kind, state - at fixed shares of the list's width
    const int w = d.rcItem.right - d.rcItem.left;
    INT tabs[3] = {w * 30 / 100, w * 40 / 100, w * 57 / 100};
    const int x = d.rcItem.left + 2;
    TabbedTextOutW(d.hDC, x, d.rcItem.top + 1, text.c_str(), static_cast<int>(text.size()), 3, tabs, x);
    if ((d.itemState & ODS_FOCUS) && !(d.itemState & ODS_NOFOCUSRECT)) DrawFocusRect(d.hDC, &d.rcItem);
}

INT_PTR CALLBACK ModsProc(HWND page, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* s = new ModsState;
        SetWindowLongPtrW(page, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        s->rt = FindRuntime();
        s->home = HomeDir();
        if (s->rt.error.empty()) {
            s->script = s->rt.script.substr(0, s->rt.script.find_last_of(L"\\/")) + L"\\mods.py";
            if (!IsFile(s->script))
                s->rt.error = L"The mod tool is missing (no mods.py next to " + s->rt.script +
                              L"). Extract all files of the release, not only the launcher.";
        }
        if (!s->rt.error.empty()) {
            ModStatus(page, L"You cannot add mods: the importer cannot run.");
            ModLog(page, s->rt.error);
        }
        ModsGreyed(page, *s);
        return FALSE;   // the window decides the focus
    }
    case WM_SHOWWINDOW: {
        ModsState* s = GetMods(page);
        if (wp && s && !s->listed && s->rt.error.empty()) ModsList(page, *s);
        break;
    }
    case kMsgLine: {
        std::wstring* line = reinterpret_cast<std::wstring*>(lp);
        if (ModsState* s = GetMods(page)) ModsOnLine(page, *s, static_cast<Job>(wp), *line);
        delete line;
        return TRUE;
    }
    case kMsgDone:
        if (ModsState* s = GetMods(page)) ModsOnDone(page, *s, static_cast<Job>(wp), static_cast<DWORD>(lp));
        return TRUE;
    case kMsgModsLanguage:
        if (ModsState* s = GetMods(page)) {
            s->language = *reinterpret_cast<const std::string*>(lp);
            ModsRender(page, *s);
        }
        return TRUE;
    case WM_MEASUREITEM: {
        // sent before WM_INITDIALOG: the dialog's font gives the row height
        auto* m = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (m->CtlID != IDC_MOD_LIST) break;
        const HDC dc = GetDC(page);
        const HFONT font = reinterpret_cast<HFONT>(SendMessageW(page, WM_GETFONT, 0, 0));
        const HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        if (old) SelectObject(dc, old);
        ReleaseDC(page, dc);
        m->itemHeight = static_cast<UINT>(tm.tmHeight + 2);
        SetWindowLongPtrW(page, DWLP_MSGRESULT, TRUE);
        return TRUE;
    }
    case WM_DRAWITEM: {
        auto* d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (d->CtlID != IDC_MOD_LIST) break;
        ModsDraw(*d);
        return TRUE;
    }
    case WM_COMMAND: {
        ModsState* s = GetMods(page);
        switch (LOWORD(wp)) {
        case IDC_MOD_ADD:
            if (s) ModsAdd(page, *s);
            return TRUE;
        case IDC_MOD_BROWSE:
            if (s) ModsBrowse(page, *s);
            return TRUE;
        case IDC_MOD_REMOVE:
            if (s) ModsRemove(page, *s);
            return TRUE;
        case IDC_MOD_LIST:
            if (s && HIWORD(wp) == LBN_SELCHANGE) ModsGreyed(page, *s);
            return TRUE;
        case IDOK:
        case IDCANCEL:   // Enter and Escape mean what they mean on the window
            SendMessageW(GetParent(page), msg, wp, lp);
            return TRUE;
        default:
            break;
        }
        break;
    }
    case WM_DESTROY:
        // the window is closing with a child running: it is ended (mods.py
        // writes a mod or a layer whole, into a new folder, then renames)
        if (ModsState* s = GetMods(page); s && s->process) {
            TerminateProcess(s->process, 1);
            CloseHandle(s->process);
            s->process = nullptr;
        }
        break;
    case WM_NCDESTROY:
        delete GetMods(page);
        SetWindowLongPtrW(page, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return FALSE;
}

}  // namespace

bool SourcesDialogRun(HWND owner, Config& cfg, bool first_run, bool rebuild) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);
    State s;
    s.cfg = &cfg;
    s.first_run = first_run;
    s.rebuild = rebuild;
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

StaleCheck SourcesCheckStale() {
    StaleCheck check;
    const Runtime rt = FindRuntime();
    const std::wstring home = HomeDir(), cache = HomeCacheDefault();
    if (!rt.error.empty() || home.empty() || cache.empty()) return check;   // nothing to judge, or nothing to judge it by
    const std::wstring stamp = StaleStamp(rt), remembered = cache + L"\\stale-checked.txt";
    if (!stamp.empty() && ReadSmall(remembered) == stamp) return check;    // judged fresh by this launcher and importer
    std::string out;
    DWORD code = 1;
    if (!RunCapture(rt, L"stale --home " + Quote(home), home, 120000, out, code)) {
        check.error = L"The importer cannot check this install (it did not finish).";
        return check;
    }
    // `stale <step>\t<reason>` a piece, then `stale N` (exit 1); `fresh` (0); `none` (2).
    size_t at = 0;
    while (at < out.size()) {
        size_t end = out.find('\n', at);
        if (end == std::string::npos) end = out.size();
        std::string line = out.substr(at, end - at);
        at = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t tab = line.find('\t');
        if (line.compare(0, 6, "stale ") == 0 && tab != std::string::npos)
            check.pieces.push_back(PieceName(line.substr(6, tab - 6)) + L" (" + ConfigFromUtf8(line.substr(tab + 1)) +
                                   L")");
    }
    if (code == 0) {
        if (!stamp.empty()) {
            if (FILE* f = _wfopen(remembered.c_str(), L"wb")) {
                const std::string bytes = ConfigToUtf8(stamp);
                std::fwrite(bytes.data(), 1, bytes.size(), f);
                std::fclose(f);
            }
        }
    } else if (code == 1 && !check.pieces.empty()) {
        check.stale = true;
    } else if (code != 2) {
        check.error = L"The importer cannot check this install (exit " + std::to_wstring(code) +
                      L"):\n" + ConfigFromUtf8(out.substr(0, 600));
    }
    return check;
}

INT_PTR CALLBACK ModsPageProc(HWND page, UINT msg, WPARAM wp, LPARAM lp) { return ModsProc(page, msg, wp, lp); }

bool SourcesAskRebuild(const StaleCheck& check) {
    std::wstring text =
        L"An older release built this install. This release makes these parts differently:\n\n";
    size_t shown = 0;
    for (const std::wstring& piece : check.pieces) {
        if (++shown > 12) {
            text += L"  ... and " + std::to_wstring(check.pieces.size() - 12) + L" more\n";
            break;
        }
        text += L"  - " + piece + L"\n";
    }
    text += L"\nBuild it again now from your copies of the game? The importer keeps the parts that did not change, "
            L"so this is usually faster than the first build.\n\nNo: the game does not start, and the install does not change.";
    return MessageBoxW(nullptr, text.c_str(), L"Breath of Fire III - the install is out of date",
                       MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES;
}

}  // namespace bof3x
