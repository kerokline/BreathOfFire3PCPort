#pragma once

// DIVERGENCE DIV-0056: which layout a language overlay wants. The layout
// patches of DIV-0015, -0017, -0018, -0026, -0027 and -0051 fit Latin text,
// 8 units a character; Japanese, like the shipped Chinese, is full-width at
// 12, which is what the original layout was made for. The five official
// languages are a closed set (owner, 2026-09-24): en, fr, de are Latin;
// ja and zh are full-width. BOF3X_LANG is a BCP 47 tag since 2026-10-08
// (fixtures.toml's `tag` per build: en-US, en-150, fr-FR, de-DE, ja-JP,
// zh-CN); what goes by language goes by the tag's primary subtag.

// True when BOF3X_LANG's primary subtag names a full-width language (ja, zh:
// `ja-JP`, `zh-CN`; the bare codes are retired, DIV-0005). Everything else, "original" and unset
// included, answers false - so the Latin paths behave exactly as they did
// before this existed.
bool Lang_FullWidth();

// True when BOF3X_LANG names an overlay to lay out as Latin text: set, not
// "original", and not full-width. The one test behind every Latin layout
// patch (DIV-0015..0018, -0026, -0027, -0051, -0058..0061), so a new rule
// for it changes one place.
bool Lang_Latin();
