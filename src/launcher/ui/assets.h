// The drawn launcher's built-in files (tools/embed_file.py writes them as
// launcher/ui_assets.gen.cpp at build time; CMakeLists.txt names the files).
#pragma once

namespace bof3x::ui {

// The title page's picture, docs/images/launcherBackground.png: a screenshot
// on the terms of docs/LICENSING.md section 3, as the Win32 launcher's resource.
extern const unsigned char kUiBackgroundPng[];
extern const unsigned kUiBackgroundPngSize;
// The pages' text font (docs/THIRD_PARTY.md section 6).
extern const unsigned char kUiTextFont[];
extern const unsigned kUiTextFontSize;
// The banner's font, TeX Gyre Pagella regular (third_party/tex-gyre-pagella,
// docs/THIRD_PARTY.md section 7): the owner's choice of 2026-11-20 in place of
// Palatino Linotype, which is Windows' and cannot ship.
extern const unsigned char kUiBannerFont[];
extern const unsigned kUiBannerFontSize;

}  // namespace bof3x::ui
