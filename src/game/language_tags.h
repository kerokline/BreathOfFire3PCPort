// The language tags the overlays are built under, and the one rule for which
// optional layer is text of which language. Shared by the launcher, which
// offers the tags and filters BOF3X_OPT by the language it exports, and by the
// DLL, which refuses a text layer under another language at start-up
// (src/game/dat_load.cpp ReadOptLayers) - so the two cannot disagree on what
// a text layer is. Plain C++, no Windows headers.
#pragma once

#include <cstring>

namespace bof3x {

// DIV-0005 (French and German DIV-0054, Japanese DIV-0056): BOF3X_LANG's
// values, the BCP 47 tag of the PlayStation release the text came from
// (fixtures.toml's `tag` per build; docs/importer.md section 5). The launcher's
// kLanguages lists these, in this order, with the dialog's labels.
inline constexpr const char* kLanguageTags[] = {"en-US", "en-150", "fr-FR", "de-DE", "ja-JP"};

// The primary subtag: what a text layer has to share with the language played
// (a layer's names are glyph codes of that language's font, DIV-0008), and
// what Lang_FullWidth reads. "en" for both English tags.
inline bool SamePrimaryLanguage(const char* a, const char* b) {
    const std::size_t n = std::strcspn(a, "-");
    return n == std::strcspn(b, "-") && std::strncmp(a, b, n) == 0;
}

// DIV-0086: the tag a text layer's name ends in - "-<tag>" with <tag> one of
// kLanguageTags (psp-names-en-150, psp-names-ja-JP) - or null for a layer that
// is not text (psp-art, psp-tiles, psp-maps).
inline const char* LayerLanguage(const char* layer) {
    const std::size_t n = std::strlen(layer);
    for (const char* tag : kLanguageTags) {
        const std::size_t t = std::strlen(tag);
        if (n > t + 1 && layer[n - t - 1] == '-' && std::strcmp(layer + n - t, tag) == 0) return layer + n - t;
    }
    return nullptr;
}

}  // namespace bof3x
