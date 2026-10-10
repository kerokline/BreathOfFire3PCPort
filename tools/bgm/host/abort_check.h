// abort_check.h - whether a piece of code ends in std::abort, for the host
// tests of the SPU model and the sequencer (IDEAS I35).
//
// POSIX: the case runs in a fork()ed child and the parent reads its signal.
// Windows has no fork, so the test binary runs itself again with
// `--abort-case N` (N counts the Aborts calls from the start of main): the
// child runs the same tests with its output discarded, skips every case
// before N, runs case N and exits 0 if it returns. std::abort's exit status
// under the Windows C runtime is 3; the child turns off the runtime's abort
// message box and error report first. Both test programs call ChildMode first
// in main and Aborts in place of their old fork.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <process.h>
#include <string>
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace abort_check {

inline int g_case = -1;  // the one case this process runs (a re-run child), else -1
inline int g_next = 0;   // Aborts calls so far

// True when this process is a child re-run for one case (Windows only).
inline bool ChildMode(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--abort-case") == 0) {
            g_case = std::atoi(argv[i + 1]);
#ifdef _WIN32
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
            if (!std::freopen(
#ifdef _WIN32
                    "NUL",
#else
                    "/dev/null",
#endif
                    "w", stdout))
                std::_Exit(4);
            return true;
        }
    }
    return false;
}

// Whether fn() aborts. `silence` installs the model's quiet abort hooks.
inline bool Aborts(void (*fn)(), void (*silence)()) {
    const int id = g_next++;
    if (g_case >= 0) {  // a child: only case g_case runs
        if (id != g_case) return false;
        silence();
        fn();
        std::fflush(stdout);
        std::_Exit(0);
    }
    std::fflush(stdout);
#ifdef _WIN32
    char self[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, self, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        std::fprintf(stderr, "abort_check: GetModuleFileNameA failed\n");
        std::abort();
    }
    char num[16];
    std::snprintf(num, sizeof num, "%d", id);
    // _spawnv joins the arguments into a command line unquoted
    const std::string quoted = std::string("\"") + self + "\"";
    const char* args[] = {quoted.c_str(), "--abort-case", num, nullptr};
    const intptr_t rc = _spawnv(_P_WAIT, self, args);
    if (rc == -1) {
        std::fprintf(stderr, "abort_check: cannot run %s\n", self);
        std::abort();
    }
    return rc == 3;
#else
    const pid_t pid = fork();
    if (pid == 0) {
        silence();
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
#endif
}

} // namespace abort_check
