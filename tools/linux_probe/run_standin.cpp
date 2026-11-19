// A stand-in for the Linux build, for tools/linux_run.py and the harness's
// --host linux (docs/linux-round.md, stages LC and LD, row C8). It is not the
// game and plays nothing: it keeps the few promises the harness relies on, so
// that input_run.py --host linux can be shown end to end before the ELF links.
//
//   g++ -std=c++20 -Wall -Wextra -O1 tools/linux_probe/run_standin.cpp -o <dir>/standin -lpthread
//
// What it does, in order:
//   * prints its arguments, its working folder and every BOF3X_ / SDL_
//     variable to stdout, and writes the same to <exe>.log - the engine's
//     log is the executable's path with the extension .log (src/hook/log.cpp);
//   * with BOF3X_INPUT: reads the recipe and counts its frames roughly
//     (`wait N` is N, `shot NAME [N]` is N or 30, any other step 1). At each
//     `shot` it writes a 2 x 2 BMP to BOF3X_SHOT_DIR/NAME.bmp and logs the
//     engine's shot line; with BOF3X_SHOT_WAIT the shot is frozen on the
//     named semaphore /bof3x_shot_<pid> (src/os/posix/named_event.cpp) for
//     up to 3 s, and the log says whether the driver released it. At a
//     `quit` step it logs the engine's `done` and exits with
//     BOF3X_STANDIN_EXIT (default 0). At the recipe's end it logs `done` and
//     waits to be ended, as the game does;
//   * with BOF3X_STATEHASH: writes that file (a line per shot and the end);
//   * without BOF3X_INPUT: exits with BOF3X_STANDIN_EXIT.
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern char** environ;

namespace {

FILE* g_log = nullptr;
FILE* g_hash = nullptr;

void Both(const std::string& line) {
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
    if (g_log) {
        std::fprintf(g_log, "%s\n", line.c_str());
        std::fflush(g_log);
    }
}

std::string SelfLog() {
    char self[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) return "standin.log";
    self[n] = 0;
    std::string p = self;
    const size_t slash = p.rfind('/');
    const size_t dot = p.rfind('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) p.resize(dot);
    return p + ".log";
}

int ExitCode() {
    const char* v = std::getenv("BOF3X_STANDIN_EXIT");
    return v ? std::atoi(v) : 0;
}

bool WriteBmp(const std::string& path) {
    // 2 x 2, 24 bits, rows padded to 4 bytes: 54 bytes of header, 16 of pixels.
    unsigned char h[70] = {'B', 'M'};
    auto put32 = [&](int at, unsigned v) {
        for (int i = 0; i < 4; ++i) h[at + i] = static_cast<unsigned char>(v >> (8 * i));
    };
    put32(2, 70);
    put32(10, 54);
    put32(14, 40);
    put32(18, 2);
    put32(22, 2);
    h[26] = 1;
    h[28] = 24;
    put32(34, 16);
    for (int i = 54; i < 70; ++i) h[i] = 0x80;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(h, 1, sizeof h, f) == sizeof h;
    return std::fclose(f) == 0 && ok;
}

bool WaitRelease(sem_t* s, unsigned ms) {
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += ms / 1000;
    t.tv_nsec += static_cast<long>(ms % 1000) * 1000000L;
    if (t.tv_nsec >= 1000000000L) {
        t.tv_nsec -= 1000000000L;
        ++t.tv_sec;
    }
    while (sem_timedwait(s, &t) != 0)
        if (errno != EINTR) return false;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    g_log = std::fopen(SelfLog().c_str(), "w");
    Both("standin      pid " + std::to_string(getpid()));
    for (int i = 0; i < argc; ++i) Both("standin      argv[" + std::to_string(i) + "] " + argv[i]);
    char cwd[PATH_MAX];
    Both(std::string("standin      cwd ") + (getcwd(cwd, sizeof cwd) ? cwd : "?"));
    std::vector<std::string> vars;
    for (char** e = environ; *e; ++e)
        if (!std::strncmp(*e, "BOF3X_", 6) || !std::strncmp(*e, "SDL_", 4)) vars.emplace_back(*e);
    std::sort(vars.begin(), vars.end());
    for (const auto& v : vars) Both("standin      env " + v);

    if (const char* sh = std::getenv("BOF3X_STATEHASH")) {
        g_hash = std::fopen(sh, "w");
        Both(std::string("standin      state hash to ") + sh + (g_hash ? "" : " NOT opened"));
    }
    const char* input = std::getenv("BOF3X_INPUT");
    if (!input) {
        Both("standin      no BOF3X_INPUT; exit " + std::to_string(ExitCode()));
        return ExitCode();
    }
    std::ifstream recipe(input);
    if (!recipe) {
        Both(std::string("FATAL standin: cannot open ") + input);
        return 2;
    }
    const char* dir = std::getenv("BOF3X_SHOT_DIR");
    const bool wait = std::getenv("BOF3X_SHOT_WAIT") != nullptr;
    sem_t* release = nullptr;
    const std::string sem_name = "/bof3x_shot_" + std::to_string(getpid());
    if (wait) {
        sem_unlink(sem_name.c_str());
        release = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0600, 0u);
        if (release == SEM_FAILED) {
            Both("FATAL standin: sem_open " + sem_name + " failed");
            return 2;
        }
    }
    unsigned frame = 0;
    std::string line;
    while (std::getline(recipe, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream in(line);
        std::string op;
        if (!(in >> op) || op[0] == '#') continue;
        if (op == "wait") {
            unsigned n = 0;
            in >> n;
            frame += n;
        } else if (op == "shot") {
            std::string name;
            unsigned n = 30;
            in >> name >> n;
            if (dir) {
                const std::string path = std::string(dir) + "/" + name + ".bmp";
                Both("input       shot " + name + (WriteBmp(path) ? " saved to " : " NOT saved: ") + path);
            }
            if (g_hash) std::fprintf(g_hash, "shot %s %u\n", name.c_str(), frame);
            frame += n;
            if (release) {
                Both("input       shot " + name + " recipe frame " + std::to_string(frame) + " frozen");
                const bool got = WaitRelease(release, 3000);
                Both("standin      shot " + name + (got ? " released by the driver" : ": no release in 3000 ms; going on"));
            } else {
                Both("input       shot " + name + " recipe frame " + std::to_string(frame));
            }
        } else if (op == "quit") {
            Both("input       done at recipe frame " + std::to_string(frame) + " (Frame_Counter " +
                 std::to_string(frame) + "); the game quit as the recipe's quit step expected");
            if (g_hash) std::fprintf(g_hash, "quit %u\n", frame);
            if (g_hash) std::fclose(g_hash);
            if (release) sem_unlink(sem_name.c_str());
            Both("standin      exit " + std::to_string(ExitCode()));
            return ExitCode();
        } else {
            frame += 1;
        }
    }
    if (g_hash) {
        std::fprintf(g_hash, "end %u\n", frame);
        std::fclose(g_hash);
        g_hash = nullptr;
    }
    Both("input       done at recipe frame " + std::to_string(frame) + " (Frame_Counter " + std::to_string(frame) +
         "); the pad is the player's again");
    for (;;) pause();   // the game runs on after its recipe; the driver ends it
}
