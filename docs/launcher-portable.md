# One launcher for Windows and Linux: what it would take

**Status:** PROPOSED (2026-11-19, measured at `main` `8cd45199`; nothing built).
An estimate and a staging for the owner's question "a launcher that works on
either Windows or Linux". It is tier 3 of [`linux-round.md`](linux-round.md)
(section 5, stand-in 3; section 8) and IDEAS I69. Stage P0 below is also
group C7 of that round's staging and is needed for any native Linux build.
Stages P1 to P6 wait for the owner's four calls in section 7.

Every count was made by the command beside it. What was not measured is in
section 8. The licence remarks in section 4 are a reading, not advice, and
each is to be checked against the library's own text before one is taken in.

## 1. The launcher today, measured

The launcher is `src/launcher/`: 28 source and header files, 13,501 lines
(`find src/launcher -name "*.cpp" -o -name "*.h" | xargs wc -l`), a dialog
resource of 672 lines and a manifest. `launcher.cpp`'s first line calls it
"Windows-only by design". The OS census counts 2,013 sites of the class
`launcher` (`python tools/os_census.py --counts`), against 1,294 live sites
in the rest of the engine; by file the launcher's rows are 2,036 (the table
below), the 23 between the two figures not accounted for.

It is in two halves, and the split is already clean in most places.

| Half | Files | Lines | Census sites | Tested by |
|---|---|---:|---:|---|
| **The model:** what a setting is, how `bof3x.ini` is read and written, the presets, the suites, the environment the game is started with | `config.cpp`, `config.h`, `suites.cpp`, `suites.h`, `ledger_registry.h` | 3,948 | 165 | `config_tests` (`tools/launcher_host`, a host build with no window) |
| **The save editor's format core** | `save/` (ten files) | 2,577 | 0 | `save_tests` (the same host build) |
| **The exe image for a disc-built install** | `exe_image.cpp`, `exe_image.h` | 293 | 30 | the exe host's self-test |
| **The windows:** the title page, Configure and its pages, Sources, the Jukebox, the save editor, the start of the game | `config_dialog.cpp`, `sources_dialog.cpp`, `save_dialog.cpp`, `music_player.cpp`, `launcher.cpp` and their headers, `key_codes_check.cpp`, `resource.h` | 6,683 | 1,841 | `dialog_test.py`, `capture_page.py`, `save_editor_drive.py` (each drives the Win32 dialog through `ctypes`) |

The per-file sites are `python tools/os_census.py --files src/launcher`:
`config_dialog.cpp` 699, `sources_dialog.cpp` 482, `save_dialog.cpp` 225,
`music_player.cpp` 211, `config.cpp` 164, `launcher.cpp` 157.

**So 6,818 lines carry over and 6,683 lines, with the 672 of the resource,
are the part that is Win32.** The model holds no window call: its 165 sites
are wide-text, file and environment calls, which the OS seam already has.

**The windows, counted in `launcher.rc`:** 21 dialog templates and 349
control statements - 136 labels, 105 push buttons, 26 check boxes, 23 group
boxes, 21 radio buttons, 15 combo boxes, 6 edit fields, 5 default buttons, 3
list boxes, 7 sliders, 1 list view, 1 progress bar. By dialog:

| Dialog | Controls | Dialog | Controls |
|---|---:|---|---:|
| The frame (title page and tabs) | 18 | Versions, two pages | 18, 11 |
| Settings | 19 | Mods | 10 |
| Preset | 6 | Extras, two pages | 9, 6 |
| Controls, three pages | 45, 39, 42 | Data | 8 |
| Bind capture | 4 | SatPixie's options | 31 |
| Music | 4 | The Jukebox | 14 |
| Game options, two pages | 12, 9 | Sources | 18 |
| Solo character | 9 | The save editor | 17 |

**What the windows take from Windows beyond plain controls** (`grep -ohE` of
each name over `src/launcher/*.cpp`):

| Need | Today | Sites |
|---|---|---:|
| A message with an OK button | `MessageBoxW` | 17 |
| Start a process and read its output (the game, the developer launcher, the importer, `tools/mods.py`) | `CreateProcessW`, `CreatePipe`, `CreateThread` | 5, 2, 3 |
| Pick a file or a folder | `GetOpenFileNameW`, `SHBrowseForFolderW` | 4, 2 |
| Draw a control ourselves (the banner, greyed rows with a reason, the list's colours) | `WM_DRAWITEM`, `NM_CUSTOMDRAW`, `DrawTextW` | 5, 2, 3 |
| The title page's picture | WIC (`IWICImagingFactory`), `StretchDIBits` | 1, 1 |
| The Jukebox's sound | `waveOut*` | 6 |
| Capture a key for a binding | a `WH_KEYBOARD` hook (`SetWindowsHookExW`) | 1 |
| See a running game (the save editor) | `FindWindowW` | 1 |
| Open a folder or a link | `ShellExecuteW` | 1 |
| The banner's font | Palatino Linotype, "which every Windows install has" ([`launcher-settings.md`](launcher-settings.md) 2a) | - |

**What is already portable in the build.** SDL3 3.4.16 is linked static
into the launcher since 2026-09-24 (`CMakeLists.txt:140`, `:322`): the pad
is read through it and the dialogs take the pad as navigation. That SDL
already has a file and folder dialog (`SDL_dialog.h`), a process call with
pipes (`SDL_process.h`), a PNG reader (`SDL_LoadPNG`, `SDL_surface.h`), text
input events and the audio stream the game plays through (`grep -l` over the
fetched `include/SDL3`).

## 2. What a Linux player needs from it

In the order a player meets them:

1. **Make an install** from their own copy of the game (Sources). Without
   it nothing starts.
2. **Start the game with their settings.** The engine reads `BOF3X_`
   environment switches and never `bof3x.ini`; the launcher turns the file
   into the environment (`ConfigApplyEnvironment`, `config.h:302`) and writes
   the game's own configuration (`ConfigApplyGameCfg`, `:327`). Without the
   launcher a Linux player would set each switch by hand.
3. **Change a setting.**
4. **Bind keys and a pad.**
5. The Jukebox, the save editor, mods.

Rows 1 and 2 need no window. That is stage P0, and the Linux round needs it
before its first release (group C7). Rows 3 to 5 are what this document is
an estimate for.

## 3. The Steam Deck changes the weights

Issue #215 asks for Linux and the Deck. On a Deck the launcher is used with
a pad, on a 1280 x 800 screen, under gamescope, with an on-screen keyboard
for the 6 edit fields. The Win32 dialogs take the pad today because we wrote
that navigation. A toolkit's native look matters less there than a pad
reaching every control and text that is large enough to read.

## 4. The ways, compared

| Way | What it is | Cost | Carries the Win32 code | One source | Verdict |
|---|---|---|---|---|---|
| A | The Windows launcher under Wine writes the file; the native engine reads it ([`linux-round.md`](linux-round.md) 5, stand-in 1) | Small | Yes | No: a Windows program on Linux | A player who wants a native build does not want Wine for its settings. The paths it writes are Wine's (`Z:\...`). Not a product answer |
| B | No window on Linux: the text file and a command line (P0) | Small, and needed anyway | Yes, on Windows | The model, yes | The first native release (call 5 of the round). Poor on a Deck |
| C | A second set of windows for Linux in a Linux toolkit (GTK), the Win32 dialogs kept | About one more set of 21 dialogs, then every later page twice | Yes | No | Rejected: each new option is built and tested twice from then on |
| D | One set of windows in a large portable toolkit (Qt, wxWidgets) | A rewrite of 6,683 lines, a large library to build on llvm-mingw at two widths and on Linux, and to ship | No | Yes | Native controls on both systems. Qt is LGPL and wxWidgets has its own LGPL-with-exception licence: [`LICENSING.md`](LICENSING.md) section 4's friction, and the launcher stops being one file. Pad navigation is ours to write again |
| E | One set of windows drawn by a small immediate-mode library on SDL3's own renderer (Dear ImGui, MIT, with its SDL3 backends) | A rewrite of the windows; one vendored library, no new DLL | No | Yes | **Proposed.** See below |
| F | One set of windows drawn by a widget set of our own on SDL3's renderer | E's rewrite plus the widgets (11 kinds, section 1), text layout, the edit field, scrolling | No | Yes | Vendors only a font rasteriser. The most work; the most control over the look |
| G | The settings inside the game's own menus (IDEAS I33, I65) | Large, and in the game's code | - | Yes | Right for a Deck in the end, but it cannot make an install, since there is no game before one. It complements a launcher; it does not replace one |

**Why E, and what changed since it was turned down.**
[`launcher-settings.md`](launcher-settings.md) section 2 rejected Dear ImGui
for two reasons: it needs its own window and render backend,
and it does not look native. Since then:

- SDL3 is inside the launcher, so the window and the backend are there
  already. The cost that reason named is paid.
- The panel then had twelve controls. It has 349 in 21 dialogs now, and a
  new page arrived on 2026-11-18 (Function keys). The question is no longer one panel.
- On Linux there is no one native look to match, and on a Deck the pad
  matters more.
- The title page is already our own picture and lettering, not stock
  controls. A drawn launcher can carry that look through every page.

What E costs that the dialogs do not: a screen reader no longer sees the
controls; text is drawn from a font we ship (no system font is the same on
both systems, and Palatino Linotype is Windows'); the look must be made, by
the owner's eye, where today Windows gives it.

**Why not F first.** The widget kinds are few, but an edit field with a
caret, selection, the clipboard and an input method, a scrolling list, and text measured and wrapped, are each a small
project. E has them. F stays open as a later replacement behind the same
page code if the owner wants no vendored toolkit: the page code names
widgets, not the library, if P1 writes it that way.

## 5. The stages

Each stage lands on Windows first and is proven there, as the Linux round
does it. The Win32 dialogs stay the player's launcher until P6.

| Stage | What | Lines touched (estimate) | Proof | Owner's eye |
|---|---|---|---|---|
| **P0, the model off Windows** (the Linux round's C7) | `config.cpp`, `suites.cpp`, `exe_image.cpp` on the OS seam; a process-start call in the seam (`src/os/process.h`; there is none); `launcher.cpp` split into the start of the game and the window; a Linux `bof3x-launcher` with no window that reads the file, applies it and starts the game, with `--set key=value` and `--list` | 195 sites changed; about 400 new | `config_tests` on Windows and Linux; the environment both launchers set from one file, compared | None |
| **P1, the frame** | An SDL3 window and renderer, the library vendored and pinned with a `THIRD_PARTY.md` section, a font under a licence that allows shipping it, a theme, the pad and keyboard navigation, the title page from the PNG by `SDL_LoadPNG`, the tab row, a message box and a file picker by SDL's. Built as a second executable beside the player's launcher | about 900 new | Starts on Windows at both widths and in the WSL; a page's picture saved to a file by the launcher itself, so a check needs no screen | **The look:** one capture of the title page and one page, tuned by the owner's words |
| **P2, the settings pages** | Settings, Preset, Game options (two pages), Solo character, Versions (two), Extras (two), Data, Music, SatPixie's options: 142 controls on 12 templates. Every text carried over word for word (it is STE100 already). Every option still listed and greyed by its source, with the reason | about 1,600 new | For each page: the settings file written from the same choices equals the Win32 dialog's, byte for byte; `dialog_test.py`'s checks said again against the model | One pass over the pages on Windows |
| **P3, Controls** | Three pages (126 controls) and the bind capture: a key from SDL's key event in place of the keyboard hook, with the numbers `key_sdl.h` already maps (159 pairs); the pad through `pad_sdl.cpp` as today | about 700 new | The stored binding numbers for every key equal the Win32 capture's (D427's Pause and NumLock cross is decided once, in the ledger) | A rebind, a clear, a double binding, with a pad |
| **P4, Sources and Mods** | The importer and `tools/mods.py` started through the seam's process call, their progress lines read from the pipe, the file and folder pickers by SDL's, the list of sources | about 900 new | `setup` from a disc through the new page on Windows and in the WSL; the cache's manifest equal | One install made through it |
| **P5, the Jukebox and the save editor** | The Jukebox's sound through SDL's audio stream in place of `waveOut` (the decoders are shared code already); the save editor's pages over `save/` (unchanged); "a game is running" asked of the seam, not `FindWindowW` | about 1,200 new | The Jukebox's samples for a song equal the old path's; `save_tests`; `save_editor_drive.py`'s cases against the new window | The owner uses both daily: one session of real use |
| **P6, the cutover** | The new launcher becomes `bof3x-launcher` on both systems; the Win32 dialogs, `launcher.rc` and their three `ctypes` drivers go, or stay one release behind a switch (call 3); the tarball's start script goes; the docs | about 7,300 removed if they go | The release's zips and tarball started on clean machines; Windows Defender's verdict on the new executable | The release |

**New code: about 5,700 lines** against 6,683 and the resource today, with
the vendored library beside it. The page code is expected to be shorter than
the dialog code (no message procedures, no control identifiers, no resource
file) and the frame is new.

**In rounds.** P0 is one group with no eye. P1 is one group and one look.
P2 to P5 are five groups that can run side by side after P1, since each is
its own source files over a model that does not change; they end in one
batched look by the owner. P6 is the coordinator's. That is two waves and
three looks. For scale: the Linux round's waves 1 and 2 were 18 groups, and
ran on 2026-11-17 and 2026-11-18. **The agents' part is a few days; the
length is set by the looks,** because this is the one round where the
deliverable is how something looks, and the owner tunes a layout by eye off
captures.

**P0 alone gives Linux a working release. P0 to P4 give it a launcher a
player can use with no text file. P5 can follow.**

## 6. What can go wrong

- **The look is refused.** It is found at P1, after about 900 lines, and
  before any page is written. That is why P1 has its own look.
- **Text.** A font must be shipped. It must cover every character the pages
  show (song titles from `data/music-names.tsv`, save names, paths outside
  ASCII). Its licence must allow shipping it under ours. Sizes must follow
  the screen's scale on Windows, Wayland and gamescope.
- **Windows Defender.** It quarantined the launcher the day it became a
  windowed program ([`launcher-settings.md`](launcher-settings.md) section
  2). A new windowed executable that starts processes may be judged again.
  The release is signed (`release.yml`); a local build is not.
- **The developer launcher stays 32-bit and stays Win32** in its injection
  half (`launcher.cpp`'s header). It is not a player's program and does not
  go to Linux. The split of P0 must leave it building.
- **Two launchers for a time.** Between P1 and P6 a new option is added to
  both, or the pages drift. Keep that time short: no release between P2 and
  P6 unless call 3 says the old one stays.
- **A page's meaning moves.** The model is not changed by P1 to P5, and each
  page's proof is the file it writes. A change of what a setting does is not
  this work, and would be a ledger entry of its own.
- **The edit fields on a Deck** need the on-screen keyboard, which SDL asks
  for with its text-input call under gamescope. Not tried.

## 7. For the owner

1. **Which way.** Proposed: E (Dear ImGui on SDL3's renderer, vendored,
   MIT), on the reasons of section 4. F if no toolkit may be vendored; D if
   native controls matter more than one file and a small build.
   **Decided (the owner, 2026-11-19): E, Dear ImGui.** Stage P1 starts when
   P0 (the Linux round's group C7) hands back.
2. **The look.** A mock-up from the owner, as the title page had, or the
   first capture of P1 as the starting point for words.
3. **The Win32 dialogs after P6.** Proposed: removed in the release that
   ships the new launcher, since two sets of pages is way C's cost. The
   other answer: kept one release behind a switch, as the keyboard's proof
   tool was.
4. **When.** P0 runs with the Linux round (its C7). Proposed for the rest:
   after that round's first native release, so a Linux player can test the
   pages where they are needed. It does not gate that release.

## 8. Not measured

- That Dear ImGui's SDL3 renderer backend builds on llvm-mingw at both
  widths and with clang on Linux beside SDL3 3.4.16. Not built.
- The size of the executable with it. The launcher was 9 MB when SDL3 went into it
  ([`launcher-settings.md`](launcher-settings.md) section 2); not weighed since.
- Which of the launcher's sites are in code that P0 keeps: the split of
  `launcher.cpp` (157 sites) into its start half and its window half was
  read from its header comment, not cut.
- `config.cpp` on a system whose `wchar_t` is 4 bytes. It holds every path
  and value as `std::wstring`; the seam converts at its edge
  (`os::ToWide`, `os::ToUtf8`), and no test has run it there.
- The new-line figures of section 5. They are an estimate from the counts
  of section 1, not from a prototype. P1 is the first measurement.
- SDL's file dialog on a Deck in game mode, and its text input under
  gamescope.
- A screen reader's use of the launcher today. If a player depends on it,
  ways E and F take it away and way D keeps it.
- The licences of the candidate fonts and of each library in section 4,
  against their own texts.
