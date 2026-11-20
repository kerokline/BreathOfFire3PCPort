// bof3::os, what runs on POSIX: the processes under /proc; no window classes.
// See os/running.h.
#include "os/running.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <string>

namespace bof3::os {

namespace {

constexpr std::size_t kCommBytes = 15;   // TASK_COMM_LEN less its NUL: /proc/<id>/comm holds no more

// The text of a small file of /proc, its new line taken off; empty when it cannot be read.
std::string Small(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::string();
    char buf[64];
    const ssize_t n = read(fd, buf, sizeof buf);
    close(fd);
    if (n <= 0) return std::string();
    std::string s(buf, static_cast<std::size_t>(n));
    while (!s.empty() && (s.back() == '\n' || s.back() == '\0')) s.pop_back();
    return s;
}

}  // namespace

int ProgramsRunning(const char* file) noexcept {
    if (!file || !*file || std::strchr(file, '/')) {
        errno = EINVAL;
        return -1;
    }
    try {
        const std::string want(file);
        const std::string comm = want.substr(0, kCommBytes);
        DIR* proc = opendir("/proc");
        if (!proc) return -1;
        int count = 0;
        for (const dirent* d; (d = readdir(proc)) != nullptr;) {
            const char* id = d->d_name;
            if (!*id || std::strspn(id, "0123456789") != std::strlen(id)) continue;
            const std::string dir = std::string("/proc/") + id;
            if (Small(dir + "/comm") != comm) continue;
            if (want.size() > kCommBytes) {
                // The name was cut: the program's own file settles it, when this process may read the link.
                char link[4096];
                const ssize_t n = readlink((dir + "/exe").c_str(), link, sizeof link - 1);
                if (n > 0) {
                    link[n] = '\0';
                    const char* leaf = std::strrchr(link, '/');
                    if (std::strcmp(leaf ? leaf + 1 : link, file) != 0) continue;
                }
            }
            ++count;
        }
        closedir(proc);
        return count;
    } catch (...) {
        errno = ENOMEM;
        return -1;
    }
}

bool WindowClassOpen(const char*) noexcept {
    errno = ENOSYS;
    return false;
}

}  // namespace bof3::os
