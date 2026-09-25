#include "game/lang_layout.h"

#include <windows.h>

#include <cstring>

bool Lang_FullWidth() {
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n == 0 || n >= sizeof lang) return false;
    return std::strcmp(lang, "ja") == 0 || std::strcmp(lang, "zh") == 0;
}
