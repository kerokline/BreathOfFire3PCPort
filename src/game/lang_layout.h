#pragma once

// DIVERGENCE DIV-0056: which layout a language overlay wants. The layout
// patches of DIV-0015, -0017, -0018, -0026, -0027 and -0051 fit Latin text,
// 8 units a character; Japanese, like the shipped Chinese, is full-width at
// 12, which is what the original layout was made for. The five official
// languages are a closed set (owner, 2026-09-24): en, fr, de are Latin;
// ja and zh are full-width.

// True when BOF3X_LANG names a full-width language (ja, zh). Everything else,
// "original" and unset included, answers false - so the Latin paths behave
// exactly as they did before this existed.
bool Lang_FullWidth();
