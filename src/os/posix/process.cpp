// bof3::os, the process, on POSIX: setenv, and fork and execve. See os/process.h.

#include "os/process.h"
#include "os/text.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace bof3::os {

bool SetEnv(const char* name, const char* value) noexcept {
    return (value ? setenv(name, value, 1) : unsetenv(name)) == 0;
}

bool SetEnvW(const wchar_t* name, const wchar_t* value) noexcept {
    try {
        const std::string n = ToUtf8(name);
        if (n.empty()) {
            errno = EINVAL;
            return false;
        }
        return value ? setenv(n.c_str(), ToUtf8(value).c_str(), 1) == 0 : unsetenv(n.c_str()) == 0;
    } catch (...) {
        errno = ENOMEM;
        return false;
    }
}

namespace {

// A backslash is a separator here too, as in Path (os/file.h).
std::string Native(const std::wstring& w) {
    std::string s = ToUtf8(w.c_str());
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

// Writes all of `n` bytes, or fails: for the exec's error number.
void WriteAll(int fd, const void* p, std::size_t n) {
    const char* b = static_cast<const char*>(p);
    while (n > 0) {
        const ssize_t w = write(fd, b, n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return;
        b += w;
        n -= static_cast<std::size_t>(w);
    }
}

}  // namespace

bool ChildStart(const ChildSpec& spec, Child* child, std::uint32_t* error) noexcept {
    const auto fail = [error](int e) {
        if (error) *error = static_cast<std::uint32_t>(e);
        return false;
    };
    *child = Child{};
    if (spec.suspended) return fail(EINVAL);   // no suspended start on POSIX (os/process.h)
    // Everything the child needs is made before the fork: after it, only
    // calls that are safe in a child of a threaded process.
    std::string program, directory;
    std::vector<std::string> argv_s, envp_s;
    try {
        program = Native(spec.program);
        directory = spec.directory.empty() ? std::string() : Native(spec.directory);
        if (program.empty()) return fail(ENOENT);
        argv_s.push_back(program);
        for (const std::wstring& a : spec.args) argv_s.push_back(ToUtf8(a.c_str()));
        if (!spec.env.empty()) {
            for (char** e = environ; e && *e; ++e) envp_s.emplace_back(*e);
            for (const auto& [wname, wvalue] : spec.env) {
                const std::string name = ToUtf8(wname.c_str()), value = ToUtf8(wvalue.c_str());
                for (std::size_t i = 0; i < envp_s.size();)
                    if (envp_s[i].compare(0, name.size() + 1, name + "=") == 0)
                        envp_s.erase(envp_s.begin() + static_cast<std::ptrdiff_t>(i));
                    else ++i;
                if (!value.empty()) envp_s.push_back(name + "=" + value);
            }
        }
    } catch (...) {
        return fail(ENOMEM);
    }
    std::vector<char*> argv, envp;
    try {
        for (std::string& s : argv_s) argv.push_back(s.data());
        argv.push_back(nullptr);
        for (std::string& s : envp_s) envp.push_back(s.data());
        envp.push_back(nullptr);
    } catch (...) {
        return fail(ENOMEM);
    }
    char* const* const env = spec.env.empty() ? environ : envp.data();

    int out[2] = {-1, -1}, report[2] = {-1, -1};
    if (spec.capture && pipe2(out, O_CLOEXEC) != 0) return fail(errno);
    if (pipe2(report, O_CLOEXEC) != 0) {
        const int e = errno;
        if (out[0] != -1) close(out[0]), close(out[1]);
        return fail(e);
    }
    int nul = -1;
    if (spec.capture) nul = open("/dev/null", O_RDONLY | O_CLOEXEC);
    const pid_t pid = fork();
    if (pid < 0) {
        const int e = errno;
        for (int fd : {out[0], out[1], report[0], report[1], nul})
            if (fd != -1) close(fd);
        return fail(e);
    }
    if (pid == 0) {
        // The child: only async-signal-safe calls from here to the exec.
        int e = 0;
        if (spec.capture) {
            if ((nul != -1 && dup2(nul, 0) < 0) || dup2(out[1], 1) < 0 || dup2(out[1], 2) < 0) e = errno;
        }
        if (!e && !directory.empty() && chdir(directory.c_str()) != 0) e = errno;
        if (!e) {
            execve(program.c_str(), argv.data(), env);
            e = errno;
        }
        WriteAll(report[1], &e, sizeof e);
        _exit(127);
    }
    close(report[1]);
    if (out[1] != -1) close(out[1]);
    if (nul != -1) close(nul);
    // The exec closes the report pipe (O_CLOEXEC): nothing read is a start.
    int e = 0;
    ssize_t got;
    do got = read(report[0], &e, sizeof e);
    while (got < 0 && errno == EINTR);
    close(report[0]);
    if (got == static_cast<ssize_t>(sizeof e)) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        if (out[0] != -1) close(out[0]);
        return fail(e);
    }
    child->process = pid;
    child->out = out[0];
    child->pid = static_cast<std::uint32_t>(pid);
    return true;
}

bool ChildResume(Child&) noexcept {
    errno = EINVAL;   // never suspended here (ChildStart refuses it)
    return false;
}

std::int64_t ChildRead(Child& child, void* to, std::size_t n) noexcept {
    if (child.out == -1) return -1;
    for (;;) {
        const ssize_t got = read(static_cast<int>(child.out), to, n);
        if (got < 0 && errno == EINTR) continue;
        return got;
    }
}

bool ChildWait(Child& child, std::uint32_t* code) noexcept {
    if (child.process == -1) return false;
    int status = 0;
    pid_t r;
    do r = waitpid(static_cast<pid_t>(child.process), &status, 0);
    while (r < 0 && errno == EINTR);
    if (r < 0) return false;
    child.process = -1;   // reaped: the id is the system's again
    if (code) *code = WIFEXITED(status) ? static_cast<std::uint32_t>(WEXITSTATUS(status))
                    : WIFSIGNALED(status) ? 128u + static_cast<std::uint32_t>(WTERMSIG(status))
                                          : 1u;
    return true;
}

bool ChildEnd(Child& child, std::uint32_t) noexcept {
    return child.process != -1 && kill(static_cast<pid_t>(child.process), SIGKILL) == 0;
}

void ChildClose(Child& child) noexcept {
    if (child.out != -1) close(static_cast<int>(child.out));
    child.out = -1;
    // A child not waited for is left to run; its id is not ours to close.
    child.process = -1;
}

// ChildSpec::own_output needs nothing here: fork keeps this process's 0, 1 and 2 for the child.

bool OutputRedirected() noexcept {
    const auto redirected = [](int fd) {
        struct stat st {};
        if (isatty(fd) || fstat(fd, &st) != 0) return false;
        return S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode);
    };
    return redirected(1) && redirected(2);
}

void AttachParentTerminal() noexcept {}   // a POSIX program has its terminal already

}  // namespace bof3::os
