#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os message box on Linux: stderr, and SDL3's message box where the build
// links SDL's video (BOF3X_OS_SDL). See os/dialog.h.
#include "os/dialog.h"
#include "os/os.h"

#include <cstdio>

#ifdef BOF3X_OS_SDL
#include <SDL3/SDL.h>
#endif

namespace bof3::os {

void ShowMessage(const char* title, const char* text, bool error, bool) {
    std::fprintf(stderr, "%s: %s\n", title, text);
#ifdef BOF3X_OS_SDL
    SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, title, text, nullptr);
#else
    (void)error;
#endif
}

}  // namespace bof3::os
