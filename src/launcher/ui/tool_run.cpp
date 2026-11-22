// launcher/ui/tool_run.h: the importer and tools/mods.py as children, through
// the seam (os/process.h, os/thread.h, os/event.h). No system call here.
#include "launcher/ui/tool_run.h"

#include "launcher/ui/ui.h"

#include "os/env.h"
#include "os/event.h"
#include "os/process.h"
#include "os/text.h"
#include "os/thread.h"
#include "os/wide_file.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <mutex>

namespace bof3x::ui {

namespace {

constexpr wchar_t kSep = static_cast<wchar_t>(std::filesystem::path::preferred_separator);
// Windows names its programs with .exe and keeps PATH's folders apart with ';'.
constexpr bool kWindowsNames = kSep == L'\\';

bool IsFile(const std::wstring& path) { return !path.empty() && bof3::os::IsFileW(path); }

std::wstring EnvString(const wchar_t* name) {
    const std::uint32_t need = bof3::os::GetEnvW(name, nullptr, 0);
    if (need == 0) return std::wstring();
    std::wstring value(need, L'\0');
    const std::uint32_t n = bof3::os::GetEnvW(name, value.data(), need);
    if (n == 0 || n >= need) return std::wstring();
    value.resize(n);
    return value;
}

std::wstring DirOf(const std::wstring& path) {
    const std::size_t cut = path.find_last_of(L"\\/");
    return cut == std::wstring::npos ? std::wstring() : path.substr(0, cut);
}

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
std::size_t JsonValueAt(const std::string& line, const char* key) {
    const std::string k = std::string("\"") + key + "\"";
    std::size_t at = line.find(k);
    if (at == std::string::npos) return at;
    at += k.size();
    while (at < line.size() && line[at] == ' ') ++at;
    if (at >= line.size() || line[at] != ':') return std::string::npos;
    ++at;
    while (at < line.size() && line[at] == ' ') ++at;
    return at < line.size() ? at : std::string::npos;
}

std::atomic<bool> g_say_starts{false};

}  // namespace

// ---- Where the importer is ----------------------------------------------------

std::wstring ToolSep(const wchar_t* part) {
    std::wstring s(part);
    for (wchar_t& c : s)
        if (c == L'\\') c = kSep;
    return s;
}

std::wstring LauncherDir() {
    std::string self;
    if (!bof3::os::ExecutablePath(&self)) return std::wstring();
    return DirOf(bof3::os::ToWide(self.c_str()));
}

std::wstring FindProgram(const std::wstring& name) {
    std::vector<std::wstring> dirs;
    dirs.push_back(LauncherDir());
    std::error_code ec;
    dirs.push_back(std::filesystem::current_path(ec).wstring());
    const std::wstring path = EnvString(L"PATH");
    const wchar_t between = kWindowsNames ? L';' : L':';
    for (std::size_t from = 0; from <= path.size();) {
        std::size_t end = path.find(between, from);
        if (end == std::wstring::npos) end = path.size();
        std::wstring dir = path.substr(from, end - from);
        if (dir.size() > 1 && dir.front() == L'"' && dir.back() == L'"') dir = dir.substr(1, dir.size() - 2);
        if (!dir.empty()) dirs.push_back(dir);
        from = end + 1;
    }
    for (std::wstring dir : dirs) {
        if (dir.empty()) continue;
        while (dir.size() > 1 && (dir.back() == L'\\' || dir.back() == L'/')) dir.pop_back();
        const std::wstring candidate = dir + kSep + name;
        if (IsFile(candidate)) {
            std::wstring full;
            return bof3::os::FullPathW(candidate, &full) ? full : candidate;
        }
    }
    return std::wstring();
}

Runtime FindRuntime() {
    Runtime rt;
    const std::wstring dir = LauncherDir();
    const std::wstring env_python = EnvString(L"BOF3X_PYTHON");
    const std::wstring bundled_script = dir + ToolSep(L"\\runtime\\tools\\importer.py");
    if (IsFile(bundled_script)) {
        rt.script = bundled_script;
        rt.python = !env_python.empty() ? env_python
                    : kWindowsNames     ? dir + ToolSep(L"\\runtime\\python\\python.exe")
                                        : dir + ToolSep(L"\\runtime\\python\\bin\\python3");
        // The Linux tarball holds no Python (docs/linux-round.md section 6,
        // call 3): runtime/python is the venv the player makes from its
        // README, and without one the system's python3 on PATH, with the
        // packages of requirements.txt installed by the system. Windows'
        // zip always holds runtime\python\python.exe; nothing changes there.
        if (!kWindowsNames && env_python.empty() && !IsFile(rt.python)) rt.python = FindProgram(L"python3");
    } else {
        std::wstring up = dir;
        for (int i = 0; i < 4 && !up.empty(); ++i) {
            if (IsFile(up + ToolSep(L"\\tools\\importer.py"))) {
                rt.script = up + ToolSep(L"\\tools\\importer.py");
                break;
            }
            const std::size_t cut = up.find_last_of(L"\\/");
            if (cut == std::wstring::npos) break;
            up = up.substr(0, cut);
        }
        rt.python = !env_python.empty() ? env_python : FindProgram(kWindowsNames ? L"python.exe" : L"python3");
    }
    if (rt.script.empty())
        rt.error = L"The importer is missing (" + ToolSep(L"runtime\\tools\\importer.py") +
                   L"). Extract all files of the release, not only the launcher.";
    else if (rt.python.empty() || !IsFile(rt.python))
        rt.error = L"The importer cannot find Python (" + (rt.python.empty() ? std::wstring(L"none on PATH") : rt.python) +
                   (kWindowsNames ? L"). Extract all files of the release, not only the launcher."
                                  : L"). Install Python 3 and the packages in requirements.txt. README.txt tells how.");
    return rt;
}

// ---- A flat JSON object -------------------------------------------------------

bool IsJsonObject(const std::string& line) { return !line.empty() && line.front() == '{' && line.back() == '}'; }

std::string JsonString(const std::string& line, const char* key) {
    std::size_t at = JsonValueAt(line, key);
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
            unsigned cp = static_cast<unsigned>(std::strtoul(line.substr(at + 1, 4).c_str(), nullptr, 16));
            at += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && at + 6 < line.size() && line[at + 1] == '\\' && line[at + 2] == 'u') {
                const unsigned lo = static_cast<unsigned>(std::strtoul(line.substr(at + 3, 4).c_str(), nullptr, 16));
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

std::vector<std::string> JsonStrings(const std::string& line, const char* key) {
    std::vector<std::string> out;
    std::size_t at = JsonValueAt(line, key);
    if (at == std::string::npos || line[at] != '[') return out;
    for (++at; at < line.size() && line[at] != ']';) {
        if (line[at] == ' ' || line[at] == ',') {
            ++at;
            continue;
        }
        if (line[at] != '"') break;
        // one element: the line from its quote, read as a value of its own
        const std::string one = "{\"v\": " + line.substr(at) + "}";
        out.push_back(JsonString(one, "v"));
        for (++at; at < line.size() && line[at] != '"'; ++at)
            if (line[at] == '\\') ++at;
        ++at;   // past the closing quote
    }
    return out;
}

bool JsonTrue(const std::string& line, const char* key) {
    const std::size_t at = JsonValueAt(line, key);
    return at != std::string::npos && line.compare(at, 4, "true") == 0;
}

// ---- A child ------------------------------------------------------------------

struct ToolRun::Shared {
    std::mutex lock;
    bof3::os::Child child;   // the reader's: ChildRead and ChildWait
    bof3::os::Child stop;    // a copy for Stop, from another thread
    std::deque<std::string> lines;
    bool ended = false;      // the child has ended (the reader waited for it)
    std::uint32_t code = 1;
    bool busy = false;       // started, its end not taken
};

namespace {

// Every runner's state, for ToolsBusy and ToolsEnd. Made at its first use:
// the pages' runners are globals of other files, made in no set order.
struct Runs {
    std::mutex lock;
    std::vector<ToolRun::Shared*> all;
};
Runs& TheRuns() {
    static Runs runs;
    return runs;
}

void Push(ToolRun::Shared& s, std::string line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    {
        std::lock_guard<std::mutex> hold(s.lock);
        s.lines.push_back(std::move(line));
    }
    WakeFrame();
}

void ReadChild(void* arg) {
    auto* held = static_cast<std::shared_ptr<ToolRun::Shared>*>(arg);
    const std::shared_ptr<ToolRun::Shared> s = *held;
    delete held;
    std::string pending;
    char buf[4096];
    for (std::int64_t got; (got = bof3::os::ChildRead(s->child, buf, sizeof buf)) > 0;) {
        pending.append(buf, static_cast<std::size_t>(got));
        for (std::size_t at; (at = pending.find('\n')) != std::string::npos;) {
            Push(*s, pending.substr(0, at));
            pending.erase(0, at + 1);
        }
    }
    if (!pending.empty()) Push(*s, pending);
    std::uint32_t code = 1;
    if (!bof3::os::ChildWait(s->child, &code)) code = 1;
    {
        std::lock_guard<std::mutex> hold(s->lock);
        s->code = code;
        s->ended = true;
    }
    WakeFrame();
}

bool StartChild(const Runtime& rt, const std::wstring& script, const std::vector<std::wstring>& args,
                const std::wstring& directory, const ToolEnv& env, bof3::os::Child* child, std::wstring* error) {
    bof3::os::ChildSpec spec;
    spec.program = rt.python;
    spec.args.push_back(script);
    spec.args.insert(spec.args.end(), args.begin(), args.end());
    spec.directory = directory;
    spec.env = env;
    spec.capture = true;
    spec.no_window = true;
    std::uint32_t why = 0;
    if (!bof3::os::ChildStart(spec, child, &why)) {
        if (error) *error = L"Cannot start the importer: " + rt.python + L" (error " + std::to_wstring(why) + L").";
        return false;
    }
    if (g_say_starts) {
        std::string line = "bof3x-launcher: check: started pid " + std::to_string(child->pid) + ":";
        for (const std::wstring& a : spec.args) line += " [" + bof3::os::ToUtf8(a.c_str()) + "]";
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    }
    return true;
}

}  // namespace

ToolRun::ToolRun() : s_(std::make_shared<Shared>()) {
    std::lock_guard<std::mutex> hold(TheRuns().lock);
    TheRuns().all.push_back(s_.get());
}

ToolRun::~ToolRun() {
    Stop();
    std::lock_guard<std::mutex> hold(TheRuns().lock);
    TheRuns().all.erase(std::remove(TheRuns().all.begin(), TheRuns().all.end(), s_.get()), TheRuns().all.end());
}

bool ToolRun::Start(const Runtime& rt, const std::wstring& script, const std::vector<std::wstring>& args,
                    const std::wstring& directory, const ToolEnv& env, std::wstring* error) {
    if (Busy()) return false;
    // A new state each run: a reader of an older child still holds its own.
    auto next = std::make_shared<Shared>();
    if (!StartChild(rt, script, args, directory, env, &next->child, error)) return false;
    next->stop = next->child;
    next->busy = true;
    {
        std::lock_guard<std::mutex> hold(TheRuns().lock);
        std::replace(TheRuns().all.begin(), TheRuns().all.end(), s_.get(), next.get());
    }
    s_ = next;
    if (!bof3::os::StartThread(ReadChild, new std::shared_ptr<Shared>(s_))) {
        // No reader: the child is ended and its end given at once.
        bof3::os::ChildEnd(s_->stop, 1);
        bof3::os::ChildWait(s_->child, nullptr);
        std::lock_guard<std::mutex> hold(s_->lock);
        s_->ended = true;
        s_->code = 1;
    }
    return true;
}

bool ToolRun::Busy() const {
    std::lock_guard<std::mutex> hold(s_->lock);
    return s_->busy;
}

bool ToolRun::NextLine(std::string* line) {
    std::lock_guard<std::mutex> hold(s_->lock);
    if (s_->lines.empty()) return false;
    *line = std::move(s_->lines.front());
    s_->lines.pop_front();
    return true;
}

bool ToolRun::Ended(std::uint32_t* code) {
    std::lock_guard<std::mutex> hold(s_->lock);
    if (!s_->busy || !s_->ended || !s_->lines.empty()) return false;
    s_->busy = false;
    if (code) *code = s_->code;
    bof3::os::ChildClose(s_->child);
    return true;
}

void ToolRun::Stop() {
    std::lock_guard<std::mutex> hold(s_->lock);
    // Ended already: its process id may be the system's again (POSIX), so it is left alone.
    if (s_->busy && !s_->ended) bof3::os::ChildEnd(s_->stop, 1);
}

bool ToolsBusy() {
    std::lock_guard<std::mutex> hold(TheRuns().lock);
    for (ToolRun::Shared* s : TheRuns().all) {
        std::lock_guard<std::mutex> one(s->lock);
        if (s->busy) return true;
    }
    return false;
}

int ToolsEnd() {
    int ended = 0;
    std::lock_guard<std::mutex> hold(TheRuns().lock);
    for (ToolRun::Shared* s : TheRuns().all) {
        std::lock_guard<std::mutex> one(s->lock);
        if (s->busy && !s->ended && bof3::os::ChildEnd(s->stop, 1)) ++ended;
    }
    return ended;
}

void ToolsSayStarts(bool on) { g_say_starts = on; }

// ---- To its end ---------------------------------------------------------------

namespace {

struct Capture {
    bof3::os::Child child;
    std::string out;
    std::uint32_t code = 1;
    bof3::os::Event done = nullptr;
};

void ReadCapture(void* arg) {
    auto* c = static_cast<Capture*>(arg);
    char buf[4096];
    for (std::int64_t got; (got = bof3::os::ChildRead(c->child, buf, sizeof buf)) > 0;)
        c->out.append(buf, static_cast<std::size_t>(got));
    if (!bof3::os::ChildWait(c->child, &c->code)) c->code = 1;
    bof3::os::EventSet(c->done);
}

}  // namespace

bool ToolCapture(const Runtime& rt, const std::vector<std::wstring>& args, const std::wstring& directory,
                 std::uint32_t timeout_ms, std::string* out, std::uint32_t* code) {
    Capture c;
    const ToolEnv env = {{L"PYTHONUTF8", L"1"}, {L"PYTHONIOENCODING", L"utf-8"}};
    if (!StartChild(rt, rt.script, args, directory, env, &c.child, nullptr)) return false;
    bof3::os::Child stop = c.child;
    c.done = bof3::os::EventCreate(true, false);
    bool killed = false;
    if (!c.done || !bof3::os::StartThread(ReadCapture, &c)) {
        // No reader thread: read here, with no time limit (as the Win32 RunCapture without its watch).
        if (c.done) bof3::os::EventClose(c.done);
        c.done = nullptr;
        char buf[4096];
        for (std::int64_t got; (got = bof3::os::ChildRead(c.child, buf, sizeof buf)) > 0;)
            c.out.append(buf, static_cast<std::size_t>(got));
        if (!bof3::os::ChildWait(c.child, &c.code)) c.code = 1;
    } else {
        // The reader reads until the child closes its end: the pipe never fills.
        // The time limit ends a child that hangs; its end of the pipe closes with it.
        if (bof3::os::EventWait(c.done, timeout_ms) == bof3::os::WaitResult::Timeout) {
            killed = true;
            bof3::os::ChildEnd(stop, 1);
        }
        bof3::os::EventWait(c.done, bof3::os::kWaitForever);
        bof3::os::EventClose(c.done);
    }
    bof3::os::ChildClose(c.child);
    if (out) *out = c.out;
    if (code) *code = c.code;
    return !killed;
}

}  // namespace bof3x::ui
