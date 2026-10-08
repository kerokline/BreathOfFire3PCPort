#include "game/lang_layout.h"

#include <windows.h>

#include <cstring>

bool Lang_FullWidth() {
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n == 0 || n >= sizeof lang) return false;
    // The primary subtag of a BCP 47 tag: `ja` of `ja-JP`, `zh` of `zh-CN`.
    // A bare `ja` has no `-` and is its own primary subtag.
    const char* dash = std::strchr(lang, '-');
    const size_t primary = dash ? static_cast<size_t>(dash - lang) : std::strlen(lang);
    return primary == 2 && (std::strncmp(lang, "ja", 2) == 0 || std::strncmp(lang, "zh", 2) == 0);
}

bool Lang_Latin() {
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n == 0 || n >= sizeof lang || std::strcmp(lang, "original") == 0) return false;
    return !Lang_FullWidth();
}
