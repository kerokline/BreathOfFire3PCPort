// The drawn launcher's tools (docs/launcher-portable.md, stage P4):
// tools/importer.py and tools/mods.py, run as children on the Python the
// release carries or the system's, through the seam's process call
// (os/process.h). The Sources page and the Mods page each run one child at a
// time. A child's stdout and stderr come back merged on one pipe, read on a
// thread of its own (os/thread.h), and handed to the frame loop a line at a
// time; the window is woken for each line (ui::WakeFrame), so a page whose
// tool is running costs no frames while the tool says nothing.
//
// What sources_dialog.cpp did for the Win32 pages, less the windows: its
// FindRuntime, its JSON readers (IsJsonObject, JsonString, JsonStrings,
// JsonTrue) as they are, its StartChild and ReadChild (here ToolRun), and its
// RunCapture (ToolCapture), with the same variables for the child.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bof3x::ui {

// ---- Where the importer is ----------------------------------------------------

struct Runtime {
    std::wstring python, script;
    std::wstring error;   // empty: found
};

// The release's runtime beside the launcher first: its own Python and the
// importer with the fixtures it reads (tools/make_release.py). Else a source
// checkout's tools/importer.py - the launcher sits in build/, so up to three
// folders up - on BOF3X_PYTHON or the Python on PATH. BOF3X_PYTHON wins over
// the bundled interpreter too, for testing a release against another.
// Windows: runtime\python\python.exe and python.exe; elsewhere
// runtime/python/bin/python3 and python3. The Linux tarball holds no Python:
// there runtime/python is a venv the player makes, and when it is not there
// the release's importer runs on python3 from PATH.
Runtime FindRuntime();

// The folder of this program, no separator at its end; empty when the system
// does not say.
std::wstring LauncherDir();

// A program by its name, as SearchPathW found one for the Win32 launcher:
// the launcher's folder, the working folder, then each folder of PATH. Empty:
// none. (SearchPathW also looks in Windows' own folders before PATH; this
// does not.)
std::wstring FindProgram(const std::wstring& name);

// A part of a path written with backslashes, with this system's separator.
std::wstring ToolSep(const wchar_t* part);

// ---- A flat JSON object, as the importer prints one ---------------------------

bool IsJsonObject(const std::string& line);
// A string value, unescaped to UTF-8; empty when absent or null.
std::string JsonString(const std::string& line, const char* key);
// A list of strings (["a", "b"]), each unescaped as JsonString does; empty
// when absent, null or not a list.
std::vector<std::string> JsonStrings(const std::string& line, const char* key);
bool JsonTrue(const std::string& line, const char* key);

// ---- A child ---------------------------------------------------------------

using ToolEnv = std::vector<std::pair<std::wstring, std::wstring>>;

// One tool at a time: `script args` on the runtime's Python, with no window,
// in `directory`, with `env` over this process's environment (the child's
// alone). From Start until its end is taken (Ended) it is busy (ToolsBusy).
class ToolRun {
public:
    ToolRun();
    ~ToolRun();
    ToolRun(const ToolRun&) = delete;
    ToolRun& operator=(const ToolRun&) = delete;

    // False with `error` (a sentence for the log) when it could not start.
    bool Start(const Runtime& rt, const std::wstring& script, const std::vector<std::wstring>& args,
               const std::wstring& directory, const ToolEnv& env, std::wstring* error);
    // Started and its end not yet taken.
    bool Busy() const;
    // The next line the child wrote, UTF-8, its line end taken off. False: none waiting.
    bool NextLine(std::string* line);
    // True once, after the child has ended and every line of it was taken:
    // its exit code. The child is then closed and the runner is free.
    bool Ended(std::uint32_t* code);
    // Ends the child at once (exit code 1); its end comes through Ended.
    void Stop();

    struct Shared;

private:
    std::shared_ptr<Shared> s_;
};

// Whether any runner is busy: a check's `wait` step runs frames until none is.
bool ToolsBusy();
// Ends every child still running, for the window's end (the Win32 Mods page's
// WM_DESTROY): no child is left behind the launcher. The count ended.
int ToolsEnd();
// A check prints each child's start (its process id and arguments) on stdout.
void ToolsSayStarts(bool on);

// Runs `script args` to its end with no window and returns its merged output
// and exit code; false when it could not start or ran past `timeout_ms`
// (then ended). For the one short command the launcher waits on before the
// window (`stale`), as the Win32 RunCapture.
bool ToolCapture(const Runtime& rt, const std::vector<std::wstring>& args, const std::wstring& directory,
                 std::uint32_t timeout_ms, std::string* out, std::uint32_t* code);

}  // namespace bof3x::ui
