// A fault in the real game ELF past the exe host's step 23, with the game
// unchanged (docs/linux-round.md, "B2b as run"): an LD_PRELOAD library that
// stands in for SDL_CreateWindow, which the game first calls on its main
// thread inside Game_WinMain (game_window: step 25, so after step 23 and on
// the placed stack 0x60000..0x160000, exe_host_posix.cpp row 3), and there
// faults the way B2B_FAULT names - before any window is made, so the run needs
// no screen:
//
//   read      a read of address 0x10, as BOF3X_CRASH_TEST's fault
//   overflow  a recursion with no end: the main thread's stack runs into the
//             guard page under it, and the catcher reports from its
//             alternate stack
//
// Any other process (the launcher), an unset B2B_FAULT, or a later call:
// SDL's own SDL_CreateWindow. B2B_ENV_OUT=<file>: the game's environment is
// written there first, NUL-separated, for a direct re-run whose exit code the
// caller reads (the launcher does not wait for the game).
//
//   cc -shared -fPIC -O1 -o fault_preload.so tools/linux_probe/fault_preload.c -ldl
//   LD_PRELOAD=$PWD/fault_preload.so B2B_FAULT=overflow ./bof3x-launcher --game <install> --no-config
#define _GNU_SOURCE 1
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct SDL_Window SDL_Window;
typedef SDL_Window* (*CreateWindowFn)(const char*, int, int, uint64_t);

static int Deeper(int depth) {
    volatile char frame[512];
    frame[0] = (char)depth;
    const int below = depth < 0x7FFFFFFF ? Deeper(depth + 1) : 0;   // never 0x7FFFFFFF: the guard comes first
    return below + frame[0];
}

static void SaveEnvironment(const char* path) {
    char buf[1 << 16];
    const int in = open("/proc/self/environ", O_RDONLY);
    const int out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (in >= 0 && out >= 0) {
        ssize_t n;
        while ((n = read(in, buf, sizeof buf)) > 0)
            if (write(out, buf, (size_t)n) != n) break;
    }
    if (in >= 0) close(in);
    if (out >= 0) close(out);
}

SDL_Window* SDL_CreateWindow(const char* title, int w, int h, uint64_t flags) {
    static int calls = 0;
    const char* fault = getenv("B2B_FAULT");
    if (calls++ == 0 && fault && strcmp(program_invocation_short_name, "bof3x") == 0) {
        const char* env_out = getenv("B2B_ENV_OUT");
        if (env_out) SaveEnvironment(env_out);
        if (strcmp(fault, "read") == 0) {
            volatile int* nowhere = (int*)0x10;
            printf("%d\n", *nowhere);
        } else if (strcmp(fault, "overflow") == 0) {
            printf("%d\n", Deeper(0));
        }
    }
    const CreateWindowFn real = (CreateWindowFn)dlsym(RTLD_NEXT, "SDL_CreateWindow");
    return real(title, w, h, flags);
}
