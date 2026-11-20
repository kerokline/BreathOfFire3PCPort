# One launcher for Windows and Linux: what it would take

**Status:** IN PROGRESS (2026-11-20: stage P0 is in `main`, the Linux
round's group C7; stages P1 to P5 - the frame, the settings pages,
Controls, Sources and Mods, the Jukebox and the save editor - are on the
branch `launcher/drawn` (pull request 267, a draft), sections 9 to 13;
stage P6, the cutover, is built on the local branch `launcher/p6-cutover`,
section 14: the drawn launcher is `bof3x-launcher` on both systems and the
Win32 dialogs are gone. What is left is the owner's: a hand on the window,
and the release started on clean machines. Sections 1 to 8 are the staging
of 2026-11-19, measured at `main` `8cd45199`.)
An estimate and a staging for the owner's question "a launcher that works on
either Windows or Linux". It is tier 3 of [`linux-round.md`](linux-round.md)
(section 5, stand-in 3; section 8) and IDEAS I69. Stage P0 below is also
group C7 of that round's staging and is needed for any native Linux build.
All four of the owner's calls in section 7 are answered (call 3 on
2026-11-20).

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
   **Open (2026-11-20):** the first captures are made (section 9). With the
   look goes the font: which font ships for the text and which for the
   banner (section 9, "The fonts").
   **Answered (the owner, 2026-11-20):** the dark look is right as a start;
   Droid Sans for the text; TeX Gyre Pagella for the banner.
3. **The Win32 dialogs after P6.** Proposed: removed in the release that
   ships the new launcher, since two sets of pages is way C's cost. The
   other answer: kept one release behind a switch, as the keyboard's proof
   tool was.
   **Answered (the owner, 2026-11-20):** "We can make this the default
   launcher - no need to keep both." The Win32 dialogs go at P6, with no
   switch (section 14).
4. **When.** P0 runs with the Linux round (its C7). Proposed for the rest:
   after that round's first native release, so a Linux player can test the
   pages where they are needed. It does not gate that release.
   **Answered (the owner, 2026-11-20):** now. The owner asked for the work
   to begin before that release.

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

## 9. Stage P1 as run (2026-11-20, branch `launcher/p1-frame`)

**What is built.** A second executable, `bof3x-launcher-ui`, beside the
player's launcher: the title page (the picture, the banner, Play, Configure,
Jukebox, Exit) and Configure's six tabs with their sub-tabs and third rows,
as [`launcher-settings.md`](launcher-settings.md) 2a places them. **Play
starts the game** by the steps of stage P0 (`launcher/start.h`), with
`bof3x.ini` as it is: no page changes a setting yet. It does not make an
install (the Sources page is P4's; it stops with a sentence that names
`bof3x-launcher`) and does not run the Win32 launcher's check for an install
of an older release. The Jukebox and the save editor's button are shown grey
with the reason, and each page of Configure but one says it is not in this
launcher yet. The one page is a **sample of every widget** in the Settings
page's arrangement (`sample_page.cpp`), for the look; it changes no setting
and goes when P2 writes Settings. No release ships the executable
(`tools/make_release.py` names its files).

**The files** (`src/launcher/ui/`, 1,549 lines with `tools/embed_file.py`;
the staging said about 900):

| File | What |
|---|---|
| `ui.h` | The widgets the pages are written against, with no library named: the window, a frame, 11 widgets, a group and an area, the picture and the banner's text, a message, the file and folder pickers. Places are the picture's pixels, as the Win32 window's |
| `ui_imgui.cpp` | `ui.h` drawn by Dear ImGui on SDL3's renderer. The only file that includes Dear ImGui (section 4, "why not F first") |
| `frame.h`, `frame.cpp` | The title page and Configure's rows, with `config_dialog.cpp`'s numbers |
| `sample_page.cpp` | The sample |
| `main.cpp` | The arguments, the start's steps, the loop, `--capture` and `--keys` |
| `assets.h` | The picture and the text font, built in as byte arrays by `tools/embed_file.py` |

**Measured** (each by the command of this section's last paragraph, or the
one beside it):

- **It builds and runs at both Windows widths and on Linux.** 32-bit and
  64-bit llvm-mingw (clang 22.1.8): 24.2 MB and 26.9 MB with debug
  information, against the Win32 launcher's 28.1 MB in the same 32-bit
  build. In the WSL (Ubuntu 26.04, clang 21.1.8), `cmake --preset
  linux-x86_64 -DSDL_X11_XTEST=OFF` and the target `bof3x_launcher_ui`: an
  18.8 MB ELF, linked at the first try. (`-DSDL_X11_XTEST=OFF` is this WSL's
  need: it has no `libxtst-dev`. SDL found X11 there and not Wayland.) So
  section 8's first line is measured for the three builds.
- **A page's picture with no screen.** `--capture FILE.png --page NAME`
  draws the face in a hidden window and writes the frame as SDL read it
  back. The title page from the 64-bit build equals the 32-bit build's byte
  for byte. On Linux it works under X11 (WSLg), under SDL's `offscreen`
  video driver and under its `dummy` one.
- **The keyboard and the pad with no hands.** `--keys LIST` presses keys and
  pad buttons before the capture, and the line on stdout says the face and
  what the window asked. On Windows and in the WSL alike: Enter, Space, the
  pad's cross and Start on the title page ask Play; Escape and the circle
  ask Exit; the arrows and the d-pad move through the buttons, the tabs, the
  sub-tabs and the sample's controls; a grey button is passed over; Escape
  and the circle on Configure go back to the title page; Start on Configure
  asks Play. One difference from the Win32 window, kept: Enter uses the
  control that has the focus, so on Configure it is not Play (the Win32
  window makes Enter Play on either face).
- **Play, through the window on the screen.** `BOF3X_SELFTEST_ONLY=1
  BOF3X_EXEFILE=1 bof3x-launcher-ui --game <main>\bof3 --keys enter` from an
  ini-less launcher copy: the window opened, Enter was Play, the game ran its
  self-tests and the launcher ended with its exit code, 0 (`self-test only:
  done` in `bof3x.log`).
- **The banner's size is the Win32 one's.** At scale 1 its ink ends at
  x = 434; the same four runs measured in Palatino Linotype by Pillow end at
  435.9. Dear ImGui sizes a font by its line and the Win32 launcher by its
  em, so `ui_imgui.cpp` reads each font's `hhea` and `head` tables and the
  pages keep the em numbers.
- **The renderer on Windows must not be SDL's software one.** Timed over
  about 1,000 frames in a hidden window: the software renderer costs 12 to
  15 ms a frame at 640 x 480 and 30 to 44 ms at 1280 x 960; SDL's Direct3D 11
  renderer costs 0.35 to 0.43 ms. So the build turns SDL's Direct3D 11
  renderer on. `SDL_DIRECTX`, which it needs, would also add SDL's
  DirectSound output and DirectInput pad driver to the game (DIV-0201,
  DIV-0050): `CMakeLists.txt` answers SDL's four header questions itself so
  they are not taken in, and stops the configure if SDL's own variables say
  one was. SDL's lists of audio and joystick drivers are the same before and
  after (`disk dummy wasapi`; `hidapi rawinput virtual wgi xinput`), read
  from the configure of this branch and of `main` `c7209fc2` side by side.
  On Linux the renderer is SDL's Vulkan one (already on for DXVK), with the
  software one when there is no display.
- **The software renderer loses long thin lines.** With Dear ImGui's
  textured lines, a group box's edges 300 pixels long were not drawn by
  SDL's software renderer (a box at a whole pixel; the one beside it at a
  half pixel was drawn). `AntiAliasedLinesUseTex` is off for that reason: the
  software renderer is still what a Linux machine with no Vulkan gets.
- **An idle window.** The loop waits for an event when nothing moves.
  Software renderer: 0.14 s of processor time in 5 s idle, 21 MB working
  set. Not measured again with Direct3D 11.
- **What the game's build gains.** SDL is one static library for the game
  and the launchers, so the renderer is now in `bof3x.dll` too, never
  called: 92.6 MB against 90.1 MB with debug information, the same source
  built with `main`'s SDL options (a worktree at `c7209fc2`). Not measured
  stripped. With the new DLL the exe host's self-tests end 0 and
  `attract_run.py --minutes 2` ran 3,596 logic frames in 120 s with SDL's
  window and sound output up. The routes and the state hash were not run:
  no file of the game's own changed.
- **The gates.** `os_census.py --check` and `--boundary`, `width_census.py
  --check`, `data_census.py --check`, `reloc_census.py --check`,
  `reloc_bare.py --check`, `tu_census.py --check --strict`, `ledger_check.py`,
  `ledger_registry.py --check`, and in the WSL `linux_tree.py --check`: ok. Two were not
  ok at first, and why is a rule for P2 to P5: **a widget call must not be
  named as an engine accessor is** (the first name of `ui::Place` was `At`,
  which the typed-data census reads as a raw read of game memory), **and a
  colour is written as red, green and blue, not as one hexadecimal number**
  (two of the look's colours were numbers inside the image's address range,
  and the census took them for addresses; `CLAUDE.md` rule 3).

**The fonts, decided (the owner, 2026-11-20).** The text stays Droid Sans.
The banner is TeX Gyre Pagella, a free face that follows Palatino Linotype
letter for letter: built into the program
([`THIRD_PARTY.md`](THIRD_PARTY.md) section 7), so the banner is the same on
Windows and Linux, and the stand-in below is gone. Seen side by side with
Palatino at scales 1 and 2 before it was taken (`--banner-font FILE` tries
any font file): the banner's ink ends at x = 434 in both, and in the Linux
build too. What stays open is Japanese and Chinese text before P5.

**The fonts as stage P1 left them** (call 2). The text is Droid Sans (Apache 2.0), from Dear
ImGui's own tree: 871 code points, Latin, Greek and Cyrillic, no Japanese
or Chinese (`fontTools`, the font's `cmap`). The banner is Palatino Linotype
read from the system's fonts folder where `WINDIR` names one, and Droid Sans
where it does not: the Linux captures show the banner in Droid Sans. That is
a stand-in until the owner chooses, since a Windows font cannot ship. What
the owner is asked: a font for the banner that may ship (a Palatino-like
face under a free licence, or the banner as a picture made once), and
whether Droid Sans stays the text font. The Jukebox's titles and a save's
names need Japanese before P5. Text is drawn by Dear ImGui's own rasteriser
(stb_truetype), with no hinting: at scale 1 it is softer than the Win32
launcher's ClearType text. FreeType would sharpen it and is one more
library; not tried.

**Not run, not built.**

- **The owner's eye**: the look, on the captures. Nothing of P2 to P5 starts
  on a look that may be refused (section 6).
- A hand on the window: the mouse, a real pad, a real keyboard. Every key
  check above went through `--keys`, which gives Dear ImGui the key and not
  SDL the event.
- The window on a display scaled other than 100 %: `--scale 1.5` and
  `--scale 2` captures are right, and the scale read from the display
  (`SDL_GetDisplayContentScale`) was not seen at another value.
- The message box and the file and folder pickers (`ui::Message`,
  `ui::PickFile`, `ui::PickFolder`): written over SDL's, compiled, and not
  shown once. A refusal of the start shows the message box, so **an
  unattended run of `bof3x-launcher-ui` that can be refused shows a modal
  window** (the same trap as the Win32 launcher's, `HANDOFF.md`).
- A Deck, gamescope, Wayland, the on-screen keyboard.
- Windows Defender's verdict on the new executable outside the excluded
  folders.
- `config_tests` and `dialog_test.py` were not run: no file they test
  changed, and the Win32 launcher's sources are as they were.

**How to run it.** `cmake --build build --target bof3x_launcher_ui`, then
`build/bof3x-launcher-ui.exe --capture out.png --page settings` (pages:
`title`, `settings`, `controls`, `music`, `game`, `mods`, `data`; `--sub N`
and `--sheet N` for a tab's sub-tab and its page; `--scale 2`; `--keys
right,enter`). With no `--capture` it opens the window; it wants
`bof3x.exe` beside it and finds the game as `bof3x-launcher` does.

## 10. Stage P2 as run (2026-11-20, branch `launcher/p1-frame`)

**What is built.** Every settings page of the staging's P2 row, each a port
of the Win32 page's fill and read-back code (`config_dialog.cpp` Populate,
ReadBack and their helpers) less the windows, with the texts word for word:

| Page | File (`src/launcher/ui/`) |
|---|---|
| Settings, with the row above the text and the preset's greying; Preset, with its list | `page_settings.cpp` |
| Game Options: Graphics and audio, Data (the three suites) | `page_versions.cpp` |
| Game Options: Cheats, two pages | `page_cheats.cpp` |
| Game Options: Solo character | `page_solo.cpp` |
| Mods, Extras: Page 1 | `page_extras.cpp` |
| Mods, Extras: Page 2 (New Game's two screens) | `page_newgame.cpp` |
| Music | `page_music.cpp` |
| Data: the cache folder | `page_data.cpp` |
| SatPixie's options, from Settings' Options... | `page_satpixie.cpp` |

`pages.h` is the contract: a page has an Open (fills its boxes from the
settings), a Page (draws, and changes boxes only) and an Apply (writes the
boxes back). The settings change only in `PagesApply`, on Play and on Back,
in `ApplyPages`' order. **The window now saves as the Win32 one does**
([`launcher-settings.md`](launcher-settings.md) 2a): Play applies, saves
`bof3x.ini` and starts the game; Back applies; an Exit after a Back saves and
starts nothing; an Exit or the close box with no Back saves nothing. The
sample page of P1 is gone. 3,162 lines in `src/launcher/ui/` (P1 and P2).

**Not in it, by the staging:** the Controls pages (P3), Mods' Data page and
the Sources page (P4; Data's Sources... button is grey with that sentence),
the Jukebox and the save editor (P5).

**The proof of the staging: the same settings file, byte for byte.**
`tools/launcher_host/ui_compare.py` makes the same choices in both
launchers from one starting `bof3x.ini`, then Back and Exit, and compares
the two files. The drawn side runs with no window (`--check`, below). The
Win32 side is the Win32 launcher's own window, driven by control id as
`dialog_test.py` drives it; it never sends Play, and it looks for a game
started by mistake after each case (none was).
**48 cases, 48 equal**, on the owner's install read and not written (a copy
of its `bof3x.ini`; `BOF3X_HOME` and `--game` name it): an untouched window;
every control of Settings, Preset, Extras, Data, Music, the three suites,
the cheats on both pages with their Defaults, the solo character, New
Game's two boxes; SatPixie's sliders, toggles, mask, Defaults, OK and
Cancel; a choice under a preset that greys Display; a suite chosen and
chosen back; and one case with a choice on eight pages at once.
`python tools/launcher_host/ui_compare.py --list` prints each case's steps.

**Also measured.**

- **An untouched window writes the owner's `bof3x.ini` byte for byte**, with
  every page opened first (`cmp`).
- **Grey controls.** A script step on a grey widget ends the program with
  exit code 1 and "is grey", so greying is a thing a check asks for. Shown
  so: Display, "Keep the game running" and the Music box under the presets
  that set them; a suite's side the cache cannot play (four caches of
  hand-written manifests: none, PlayStation only, a PSP base, a wrong
  policy); the solo character's boxes with the rule off; New Game's boxes
  under Chinese (PC script) and under "As shipped"; Options... without the
  CRT look. Every option stays listed.
- **A multiplier over the cap** (`cheat.exp=37`) is shown as 10 and written
  back as 37 by an untouched page, as the Win32 page.
- **Linux.** The pages build in the WSL and a script over six pages writes
  the expected lines there (no install: the language box holds the PC
  script alone, as the model says for a folder with no language files).
- **Both Windows widths** build. The gates of section 9 pass.

**How a page is checked** (`main.cpp`'s header has it all): `--check` is the
window with no window on the screen and no game; `--ini FILE` and `--game
DIR` are what it reads, `--save FILE` gets the settings where the window
would save them, and `--script` is steps with `;` between them: `open <page>
[<sub> [<sheet>]]`, `set <widget>=<value>`, `press <widget>`, `key <name>`,
`shot <file.png>`. A widget's name is its label, or the name the page gave
it (`wide`, `look`, `graphics-psp`, `exp`, `champion`, `names`...).

**What changed in the look's widgets.** A check box and a radio button are
lower than a button, so rows 11 dialog units apart do not touch.
`ComboGreyed` (entries the player cannot choose), `ReadText` (the preset's
list) and `SliderShowing` (a slider that shows a text for its step) are new.

**Not run, not built, and for the owner's eye.**

- **A hand on any page**: the mouse, a pad, a keyboard. Every check went
  through `--script`.
- The folder picker behind Data's Browse... (SDL's): never shown.
- **SatPixie's options do not fit as the Win32 window lays them out** (252
  dialog units high against 214): the rows are closer and each value shows
  inside its slider. **By the owner's words of 2026-11-20:** "Natural
  vision" and "Overscan crop" lose their words in brackets, the eight check
  boxes are four rows of two, and Defaults, OK and Cancel are a row under
  the boxes. Every control is kept.
- **The Cheats sliders show their number in the bar alone** (the owner,
  2026-11-20): the Win32 page's value text beside the slider is not drawn.
- The two changes above are in the look only: the SatPixie and cheat cases
  of `ui_compare.py` were run again after them, 8 of 8 equal.
- A `solo.champion=` that names no champion is cut off in its closed box,
  as the Win32 box cuts it.
- The comparison's entry numbers are this install's (which languages it
  holds, which row above the text is grey): on another install the table's
  values want a look.
- The Jukebox's battle-chord box (`Pages::chord`) is passed to the suites'
  edit as the Win32 code passes it, and nothing sets it before P5.
- Carried from the Win32 code as it is, each seen in the port: the Music
  box is read back even when a preset greys it; the Data sub-page's member
  note says "see the note above" where that page has no note line; a
  manifest error's sentence runs into the member list with no full stop.

## 11. Stage P3 as run (2026-11-20, branch `launcher/p3-controls`, merged into `launcher/p1-frame`)

**What is built.** The Controls tab's three pages - Directions and
buttons, Shoulders, Start and Select, Function keys: 45, 39 and 42
controls, launcher.rc's 126 - and the bind capture over a cell of any of
them, in `src/launcher/ui/page_controls.cpp`: a port of
`config_dialog.cpp`'s cells, capture, red cells, Defaults and the Controls
part of `ApplyPages`, the texts word for word. The capture is shown in
place of the tabs' page, as SatPixie's options are. The bindings are
written only once a cell, the face-button box or the binding pages'
Defaults was used, and the function keys only once their page was, as in
the Win32 window - where a cancelled capture counts as a use too. The
Settings page's Display box names the window key as the Function keys page
shows it.

**The two inputs** (`ui.h`, "The bind capture"). A key: SDL's key event,
taken in `ui_imgui.cpp` before Dear ImGui's navigation sees it, so Escape,
Enter, Tab and the arrows are keys a player binds; its scancode is made a
cell's number by `capture_key.cpp`, which is `CaptureKeyHook` over
`key_sdl.h`'s table. The pad: `input/pad_sdl.cpp`, as the Win32 capture
reads it, with its layout and its "held when it opened" rule. **Both Dear
ImGui's SDL3 backend and `pad_sdl.cpp` can hold the pad at once**:
`SDL_OpenGamepad` of an open pad returns the same object and counts the
open (`SDL_gamepad.c`, `ref_count`), and each close takes one count, so
neither closes the other's. Two things of `pad_sdl.cpp` do not fit a
window that pumps SDL's events itself, and the drawn launcher calls
neither: `PadSdl_Stop` ends SDL (`SDL_Quit`) and the window with it, and
`PadSdl_Poll` takes the pad's events out of the window's queue, so the pad
would not be seen plugged in. `PadSdl_Find` (new) looks for the pad with
no event; the pad's state comes up to date with the window's own pump.
While the capture runs the pad's navigation is off, so cross and circle are
pad inputs; Dear ImGui still reads the pad (its backend fills the inputs
either way).

**The proof: the stored numbers for every key.** No key was pressed: the
Win32 capture reads the keyboard through a hook and the machine was in use.
`tools/key_host/capture_tests.cpp` walks every key a `WM_KEYDOWN` names
(scan code 1 to 0x7F, extended or not: 254 codes), on a binding cell and on
a Function keys cell, through both captures: the Win32 one as
`CaptureKeyHook`'s code, the drawn one through SDL's own Windows table
(`scancodes_windows.h`, with `WindowsScanCodeToSDLScanCode`'s Pause rule)
and `capture_key.cpp`. **159 codes are answered alike** - each of
`key_sdl.h`'s 159 pairs, and each key's name reads back as its number, so a
check names every key - **94 codes have no SDL scancode** (no key SDL
knows: the Win32 capture would store the code; the drawn one never sees a
key), **and one key differs: Ctrl+Pause (Break, E0 46)**, which SDL reports
as Pause, so the drawn capture stores Pause's number 0x45 where the Win32
one stores 0xC6. 329 checks, 0 failed.

**D427, fixed (the owner, 2026-11-20: "fix both").** As this stage was
built, both captures stored Pause as 0x45 and NumLock as 0xC5, the way
Windows sends them, so a game button bound to Pause was worked by NumLock
while a function key bound to Pause worked with Pause. Now a stored number
is DirectInput's for every key: the Win32 capture and the function keys'
reader cross the pair through one function (`FnKeyOfScan`), and the drawn
capture stores DirectInput's number from SDL's scancode. DIV-0240's
amendment says what an older ini's lines mean. Where the paragraphs of this
section say the pair is crossed or that Ctrl+Pause stores 0x45, read them
as of before the fix: Ctrl+Pause now stores 0xC5 in the drawn capture.
After it: `capture_tests` 329 checks and `key_tests` 133,780, 0 failed; the
game's self-tests end 0; `ui_compare.py` 64 cases, 64 equal with both
launchers rebuilt.

**The comparison tool wants the machine left alone.** With the owner at the
machine, 2 of 64 cases came out unequal once, each with a box toggled that
the case did not name, and both were equal on three more runs. The tool
now sets a Win32 check box and tells its page, where it played a mouse
press; a case that differs is run again before it is believed.

**Through the drawn launcher with no window** (`--check`, the owner's
`bof3x.ini` copied; the script steps `bind-key`, `bind-pad` and `red` are
new): a key and a pad input bound (`key.Up=up` became `key.Z=up`; `pad.lb`
moved from L1 to cross); a Clear; a Cancel, which writes the file back as
read; a double binding - a function key on S and a function pad input on
South: the cells on both pages red, and both lines written - and an ini
whose S does two game buttons, red, which on save keeps the first and
drops the second (and moves a third key of that row to the end), as the
Win32 `BindingsFrom` does; each page's Defaults; the face-button box; the
window key on F3, and the Display box then names F3; F10 and both Alts
refused on a function key and taken on a binding cell; Pause and NumLock
(the cells read "NumLock" and "Pause", D427); an untouched window, all three
pages opened, writing the owner's `bof3x.ini` byte for byte.

**Against the Win32 window** (`ui_compare.py`): 16 new cases - the three
pages untouched, the face-button box three ways, both Defaults, the
capture's Clear on five cells and its Cancel on two (the Win32 capture's
buttons pressed by id; no key and no pad input is sent), and one case
across the Controls pages and Settings: 16 equal. With them the whole
table: **64 cases, 64 equal**; no game started, the install unchanged.

**Linux.** In the WSL (`cmake --preset linux-x86_64 -DSDL_X11_XTEST=OFF`,
the target `bof3x_launcher_ui`) the pages, `capture_key.cpp`,
`key_sdl.cpp` and `pad_sdl.cpp` build and link; under SDL's `offscreen`
driver a script that binds a key, a pad input and the window key writes
`key.Z=up`, `pad.lb=cross` and `fkey.window=F3`. The gates of section 9
that this stage touches (`os_census.py --check` and `--boundary`,
`width_census.py`, `data_census.py`, `reloc_bare.py`): ok.

**Not run, and for the owner's eye.**

- A hand on the capture: a real key, a real pad, the mouse. Every input
  went through the check steps, which hand the capture SDL's scancode or the
  pad input's number; SDL's own key event and `pad_sdl.cpp`'s read of a
  pad were not seen.
- Two keys taken differently from the Win32 hook, read from SDL's source
  and not tried: AltGr, which sends a left Ctrl before the right Alt
  (the Win32 hook takes the left Ctrl; SDL drops it, so the drawn capture
  takes the right Alt), and Print Screen, whose key-down Windows may keep
  (SDL makes a key-down at its key-up; the Win32 hook takes key-downs
  only). A key held down repeats; the Win32 hook would take a repeat of the
  key that opened it, the drawn capture takes none.
- The red is lighter than the Win32 page's 192, 0, 0, which is dim on
  this look's dark buttons.
- `padnav::SetMap`, the Win32 window's pad navigation map from the
  bindings, has no counterpart: the drawn window's navigation is Dear
  ImGui's.

## 12. Stage P4 as run (2026-11-20, branch `launcher/p4-sources`, merged into `launcher/p1-frame`)

**What is built.** The Sources page and the Mods tab's Data page, each a
port of `sources_dialog.cpp` less the windows, with the texts word for
word, and the start's flows that need the Sources page:

| File (`src/launcher/ui/`) | What |
|---|---|
| `tool_run.h`, `tool_run.cpp` | The tools as children through the seam (`os/process.h`): one child to a runner, stdout and stderr on one pipe, read on a thread (`os/thread.h`) and handed to the frame loop a line at a time; the window is woken for each line (`ui::WakeFrame`, an SDL event pushed from the reader). `FindRuntime` and the JSON readers ported as they are; `ToolCapture` is `RunCapture`, its time limit on an event (`os/event.h`); `ToolsEnd` ends every child still running when the window closes |
| `page_sources.cpp` | The Sources page (`IDD_SOURCES`): its list, File... and Folder..., Add, Remove, Locate ffmpeg.exe..., Get ffmpeg, the build's progress bar, status and log, Close and Stop with its question; the first-run, set-up-in-place and rebuild modes; `SourcesCheckStale` and the rebuild's question |
| `page_mods.cpp` | Mods, Data (`IDD_PAGE_MODS`): `mods.py`'s list with each row's check box and grey rows with their reason, Add, Browse..., Remove, the credit and report lines, the log, and the window's side of the six messages |
| `main.cpp` | `launcher.cpp`'s start (lines 418-500) as phases between frames, for the window and for `--check --start` |

**How the window's messages are carried.** `kMsgModsOff`: `ModsOpen`
reads `mods_off=`. `kMsgModsOffChanged`: a box writes `mods_off=` into
the settings at once and marks them to be saved on any way out, as the
Win32 window does. `kMsgModsReadingRowAdded`: Extras' toggle and
`ruby_ui=` on at once. `kMsgModsChanged`: `Pages::mods_stamp` moves and
Settings' row box is made again. `kMsgModsLanguage`: the Mods page reads
Settings' language as the box stands, each frame. `kMsgModsToggle`: the
list's box (`ui::CheckList`'s `toggled`). Both pages take their tool's
lines every frame, shown or not (`SourcesPoll`, `ModsPoll`), as the
Win32 pages take their messages while hidden.

**The start** (`main.cpp`'s header). A first run or a game folder with
no image opens the Sources page before the window; closed with no build,
nothing starts; built, the game folder and the cache are found again and
the settings read again. Then the stale check and its question
(`ui::Ask`): No starts nothing, Yes opens the page with the rebuild.
Data's Sources...: after a build the pages are applied and saved, the
game folder and the settings found and read again, and the window shown
on Data (the Win32 window's `kDialogSourcesChanged`). Locate ffmpeg's
choice is saved whatever the page did. The window on the screen opens
only when a frame is owed, so the stale check runs with no empty window,
as the Win32 launcher runs it before any window.

**Additions to the widgets** (`ui.h`): `Ask`, `OpenLink`, `LabelCut`
(the Win32 end and path ellipsis), `LogText`, `CheckList`, `WakeFrame`,
`Rest`; a check's `TestAnswer`, `TestAsked`, `TestPick`, `TestPicking`.
**Changed:** `PickFile` and `PickFolder` take a title (a default
argument, so P2's callers are unchanged), since the Win32 pickers'
titles are player texts; in a check no picker, question or link is
shown. **`pages.h`:** `Pages::sources_open`, `sources_closed`,
`sources_built`, `mods_stamp`, and the two pages' calls. **`frame.cpp`:**
Mods' Data row has its body; the Sources page is drawn in place of the
title page or the tabs. **`page_data.cpp`:** Sources... opens the page.
**`page_settings.cpp`:** the row box is made again when `mods_stamp`
moves. **`main.cpp`:** `--start`, and the steps `pick`, `answer`, `wait`
and `report`.

**The seam.** One change: a Windows child's argument is now quoted by
the C runtime's rules (`src/os/win32/process.cpp`), because the Win32
page quoted that way and a folder `D:\` given as `"D:\"` reaches the
child as `D:"`. An argument with no backslash before its end and no
double quote is the same text as before, so the game's start does not
change. `tools/os_host` checks `D:\`, doubled backslashes and a double
quote: 606 checks on Windows, 521 in the WSL, 0 failed. Nothing else
was missing: `capture`, `no_window` and `ChildEnd` cover the Win32
code's pipe, `CREATE_NO_WINDOW` and `TerminateProcess`; the time limit
of `RunCapture` is an event's wait in `tool_run.cpp`.

**What was moved to fit the Sources page.** The Win32 window is 400 x
276 dialog units; the page area is about 350 x 214. Every control and
text is kept: the intro is 346 units wide (from 386); the list is 292 x
40 (from 330 x 62), Remove beside it; the path box is 184 wide (from
224), with File..., Folder... and Add at 188, 242 and 296; the ffmpeg
note has two lines (from one line cut with an ellipsis), and Locate and
Get ffmpeg are under it at its right; the progress bar, the status and
the log follow, the log 35 units high (from 46); the install folder line
(230 wide, cut in its middle), Build the install and Close are the last
row. The Mods page fits as `launcher.rc` places it.

**Measured** (each with `--check`; the homes under `D:\bof3x-runs\p4src`,
`BOF3X_HOME` naming each, the working folder an empty one):

- **`setup` from a disc through the new page, Windows.** `--check --start
  --save saved.ini --script "wait 60; press File...; pick <CDImage>\Breath
  of Fire III (USA) (1.1).zip; press Add; wait 1800; report sources"` on
  an empty home: the first run's page opened, the add, the setup and the
  stale check were started (each child's arguments on stdout), the page
  closed when the build was done, the settings were saved, the home's
  game folder was found and the window shown (124 s). The same zip by the
  importer itself into a second home (`importer.py sources --home H add
  ZIP --json`, then `setup --home H --json-progress`, as section 4 of
  [`install-sources.md`](install-sources.md) gives them). **The two
  caches' `manifest.toml` (4,161 lines each) differ in one line,
  `built =`, the build's time.** No line holds a path of either home.
  The files of `cache\` and `game\`: 4,071 and 4,070, every common file
  the same size; the one more is `cache\stale-checked.txt`, which the
  launcher's stale check writes after the build.
- **The same in the WSL.** The same script under SDL's `offscreen`
  driver, the home in the WSL's filesystem, the zip read from `/mnt/c`
  (71 s; this WSL has no ffmpeg; Python 3.14.4). **Its `manifest.toml`
  against the Windows importer's: the `built =` line alone differs.** Its
  files: the same 4,070 and `stale-checked.txt`, every `game\` file and
  every cache file the same size but nine of `cache\build-keys\` (46 bytes
  each), which record the Python that ran (3.14.4 against Anaconda's
  3.13.9) and the hashes of the importer's functions as that Python
  compiles them. Nothing was installed in the WSL.
- **A PC folder set up in place** (`--start --game <main>\bof3`, an empty
  home): the page opened in its set-up mode, listed, added the folder,
  then ran `setup --home H --json-progress --game-in-place <main>\bof3`;
  the build finished (11 s), the settings were saved and the window shown
  with that folder as the game folder.
- **The window's flows.** A first run: above. The stale prompt, on a
  home of this session with one record of `cache\build-keys\snd.json`
  changed: the question named "the sound effects (tools/psx_disc.py
  changed (Disc.__init__))"; `answer no` started nothing and saved
  nothing (the last line says the start ended); `answer yes` opened the
  page in its rebuild mode, ran `setup`, closed the page when done,
  saved, found the install again, ran the stale check again (not stale)
  and showed the window. Data's Sources...: a second disc added (Japan,
  173 s), and Build the install pressed on a later run; each time, after
  Close, the pages were applied and saved, the stale check ran on the new
  cache and the window was shown again, on Data (page 5). Remove: a
  source removed and the list read again. Stop: a folder of images being
  searched, Close (then Stop) pressed, `answer yes`: the child was ended
  and the page closed; no Python process was left.
- **The Mods page** (the US and Japanese home above): no mods ("No mods
  are installed."); *Better Descriptions* added by Browse... and a picked
  path, checked, built (83 s) and shown `[x] ... built: on` with its
  credit line and its report's two sentences in the log; its box used
  (`set mods=box 0`): `built: off`, and `mods_off=descriptions` in the
  saved ini; the language set to Japanese: the row grey, "built; only
  with en-US text". The reading-row case, with the project's own
  `furigana-0.1.1.zip`: added by the path box and Add, built, Extras'
  toggle turned on and Settings' Row above the text box shown at once
  (`ruby_ui=1` saved), the mod chosen there, then removed with Remove.
  In the WSL the page lists an install with no mods.
- **An untouched window writes the owner's `bof3x.ini` byte for byte**
  with every page opened, Mods' Data page listed and the Sources page
  opened, listed and closed (`fc /b`: no differences); the owner's
  install was read (`mods.py list`, `importer.py sources list`) and no
  file of it changed.
- **Against the Win32 window** (`ui_compare.py`): two new cases, the
  Mods page opened and left and the Sources page opened and closed, on
  the owner's install read only (adding or building writes the home, so
  no such case is run there). The Win32 side waits 3 s after its Sources
  window opens, since its Close asks "Stop the importer?" while the list
  is read. With them the whole table: **66 cases, 66 equal**; no game
  started, the install unchanged.
- **The gates.** `os_census.py --check` and `--boundary`,
  `width_census.py --check`, `data_census.py --check`, `reloc_bare.py
  --check`: ok. `data_census` was one site over at first: a helper named
  `W(...)` was read as a raw access, and was renamed. The WSL build of
  `bof3x_launcher_ui` links.

**Not run, and for the owner's eye.**

- **The look of the two pages**, and the Sources page's new arrangement:
  the captures under `D:\bof3x-runs\p4src\logs\` (a build's end, the Mods
  list grey and ticked, Settings' row box) and `first\sources.png` (a
  first run).
- **A hand on either page.** The pickers, the question and the browser
  link were never shown: a check gives their answers. SDL's file dialog
  takes the title on Windows (read in SDL's source); on Linux not seen.
- **The close box during a set-up** ends the child with no question; the
  Win32 page's close box is its Stop and asks. The child ended is Python
  alone, as in the Win32 page: a tool it started (ffmpeg) is not ended.
- **The log box** (`LogText`) is wrapped and follows its end, but the
  keyboard and the pad do not reach it and its text cannot be selected;
  `InputTextMultiline` did not follow its end (a capture showed it).
- **The Mods list's box** is used with the mouse and Space, as the Win32
  list; the pad has no input for it.
- **Programs found on PATH** (`FindProgram`): the launcher's folder, the
  working folder, then `PATH`; `SearchPathW` also looks in Windows' own
  folders before `PATH`. On Linux the runtime is
  `runtime/python/bin/python3` in a release and `python3` on PATH; no
  Linux release carries one yet.
- The window on the screen, and in the WSL the set-up in place and the
  stale prompt, were not run.

**After the merge into `launcher/p1-frame`** (the coordinator's own runs,
2026-11-20): both Windows widths and the Linux build; the game's self-tests
through the drawn launcher's Play end 0 with the seam's new quoting; an
untouched window with every page opened, the Mods list read and the Sources
page opened and closed writes the owner's `bof3x.ini` byte for byte, and
the install's own files keep their times; `ui_compare.py` 66 cases, 66
equal, with both launchers rebuilt; in the WSL the Mods and Sources pages
list the home this stage built there; the gates of section 9 and
`linux_tree.py --check` pass.

## 13. Stage P5 as run (2026-11-20, branches `launcher/p5-jukebox` and `launcher/p5-saves`, merged into `launcher/p1-frame`)

### The Jukebox

**What is built.** The Jukebox (`IDD_MUSIC`, 14 controls) in
`src/launcher/ui/page_jukebox.cpp`: a port of `music_player.cpp`'s lists,
names and dialog procedure less the windows, with the texts word for word.
It is shown in place of the title page from its Jukebox button, as the
Sources page is; its Close, Escape and the pad's circle stop the sound and
go back. The sound is `jukebox_play.cpp`'s: the Win32 player's `Mp3Stream`
and `SeqStream`, and one SDL device stream in place of `waveOut`, refilled
on SDL's audio thread in the Win32 player's blocks of 2048 frames under the
stream's lock. It is the Jukebox's own file that calls SDL's audio, and it
names no operating system. A song that plays once wakes the window when it
ends, so a playing Jukebox draws no frames. The window fits as
`launcher.rc` lays it out (320 x 186 units); nothing is moved. The note and
the status scroll where the Win32 labels cut a long text. The battle-chord
box sets `Pages::chord`, which the Game Options page's Apply writes with
the suites, as the Win32 window's `DialogState::chord`.

**Additions.** `ui.h`: `ComboGreyText`, a combo whose grey entries can
still be chosen. `pages.h`: `jukebox_open` and the page's four calls.
`frame.cpp`: the button, `JukeboxFace`. `main.cpp`: `--samples`, `report
jukebox`, `rest`. A check's Jukebox plays to SDL's dummy driver whatever
the environment says. Build: the page, the player and the decoders in the
source list; `music-names.tsv` copied beside the Linux build too.

**The proof: the samples equal the old path's.**
`tools/jukebox_host/old_pcm.cpp` compiles `music_player.cpp` unchanged and
writes what its streams render in its Output's blocks, with no window and
no `waveOut`. Against the drawn launcher's `--samples` file of the same
song, over 21 to 25 s, seven songs, all byte for byte: a PlayStation song
and a PSP layer's song from the cache, a PC MP3 across its measured loop
and crossfade, a PC MP3 that rewinds, a song that plays once, a disc jingle
(an XA clip) and a PC stream. In the WSL, song 001 from the US disc's
install equals the Windows dump of the same song over 1,083,392 frames.

**Measured with `--check`.** The list (141 rows: 130 songs and 11 streams),
songs started, chosen while playing, the source changed while playing,
stopped; the chord box on writes `opt=area4-walls,psp-chord-one-tone`, and
on then off writes the ini byte for byte; grey streams and Play's sentence
for them; an install with no songs; keys and pad. Captures at scales 1 and
2. `ui_compare.py` has no Jukebox case: the Win32 Jukebox opens `waveOut`
as it opens, so it was not run on the owner's machine, and the chord box is
compared with the Win32 window by no run.

**Not run, and for the owner's ear and eye.** Real sound on any device; a
hand on the page. Play decodes a PC MP3 on the frame's thread, as the Win32
window did under its wait cursor: about 100 ms for the largest. The Win32
window has no Next button, no position display and no pad navigation; the
drawn page has none of the first two, and Dear ImGui's navigation. Carried
from the Win32 code: the chord note says "Off: two tones..." when the box
is ticked too; whether Play and Stop are grey is decided once, when the
page opens.

### The save editor

**What is built.** The save editor (`IDD_SAVEEDIT`, 17 controls) in
`src/launcher/ui/page_saves.cpp`: a port of `save_dialog.cpp` less the
windows, over `launcher/save/` unchanged, with the texts word for word.
Mods' Save Editor... opens it in place of the tabs, and its Close or Escape
goes back. A Win32 message box that waited is a question asked in the next
frames, and the rest of the action runs on its answer; the page is grey
while a question or a picker waits. The window is 430 x 292 dialog units
and is laid out again in the page area, every control kept: the file boxes
are 246 and 194 wide, the sections list 86 x 120, the record box and the
field list 256 wide, the field list 105 high, the value box 170, the
buttons 54.

**"A game is running" is asked of the seam** (`os/running.h`:
`ProgramsRunning`, `WindowClassOpen`): a process of the game's program,
`bof3x.exe` or `bof3x`, and on Windows a window of the class `Bof3`, which
is what `FindWindowW` was asked before (D428). An answer of "cannot tell"
counts as not running, as a failed `FindWindowW` did. No change to the
game.

**Additions.** `ui.h`: `ComboWide`, `EditCombo`, `ColumnList`, `AskNo`,
`PickFileIn`, and `Message` silent in a check (it showed SDL's box in a
hidden window before). `pages.h`: `saves_open`, `SavesBegin`, `SavesPage`,
`SavesText`, `SavesGameMarks`. `main.cpp`: `report saves`, `game-program`,
`game-window`. The Linux build copies `save-names.tsv` beside the launcher.

**Measured.** `save_tests` 1,522 checks, 0 failures.
`tools/launcher_host/save_editor_check.py` says `save_editor_drive.py`'s 33
checks again against the drawn editor with no window, and 64 more (an
edit, the checksum question, refusals, the running game by a stand-in, the
discard and replace questions, the kinds of field, a PC file elsewhere): 97
checks, 0 failures on Windows; 56 of them pass in the WSL. **The same file
from both editors:** the Win32 window on copies of the same input
(`save_editor_drive.py` with a Japanese card and with a US card, and a
field edit, a replace and a damaged save) wrote the same slot and card
files byte for byte. `os_host` 624 checks on Windows and 540 in the WSL, 0
failed. Every save worked on was a copy; the owner's saves keep their
dates.

**Differences from the Win32 editor, for the owner.** The launcher's close
box ends the program even with an unsaved change in the editor (the Win32
editor is a modal window, so its owner cannot be closed). Enter runs Set
only in the value box (the Win32 window: anywhere); in the field list,
Enter chooses a row and Enter again uses it, as a double-click. A warning
shows the information icon, and there is no beep on a refused value. The
new-card picker is SDL's save dialog, which on Windows asks before it names
a card that exists. New sentences, which the Win32 window had no place for:
the picker's title "Select a memory card file, or type the name of a new
one", and four grey reasons ("This section has one record.", "Select a
field.", "Answer the question first.", "Close the file window first."). A
long cell in the field list ends at its column's edge with no "...".

### After both merges (the coordinator's own runs)

Four files conflicted between the two branches (each appended at the same
places); both sides are kept. Then: the 32-bit build whole and the drawn
launcher at 64 bits; the Linux build with both pages and both names files
beside it, where the Jukebox plays song 000 to the dummy driver and the
save editor opens; `save_editor_check.py` again on the merged build, 97
checks, 0 failures; an untouched window with the Jukebox, every page, the
save editor and the Sources page opened writes the owner's `bof3x.ini` byte
for byte, and the owner's saves keep their dates; the game's self-tests
through the drawn launcher's Play end 0; `ui_compare.py` 66 cases, 66
equal, run alone (the two branches' own runs disturbed each other: both
wrote the one launcher copy's `bof3x.ini`, and each saw cases unequal once
and equal when run again); the gates of section 9 and `linux_tree.py
--check`. `src/launcher/ui/` is 8,698 lines.

**What stage P6 finds ready.** Every window of the Win32 launcher is in the
drawn one: the title page, Configure's pages, Controls and the capture,
Sources, Mods, the Jukebox, the save editor. Not yet done anywhere: a hand
on the window (the mouse, a keyboard, a pad), sound heard, a picker, a
question or a message box seen on a screen, a display scaled other than
100 %, a Deck, Windows Defender's verdict, and the release's files beside
the program (the two names files, the fonts' and libraries' notices).

## 14. Stage P6 as run (2026-11-20, branch `launcher/p6-cutover`, cut from `launcher/drawn`)

The owner's call 3 (section 7): "We can make this the default launcher - no
need to keep both." So the Win32 dialogs go in this stage, with no switch.

**What changed.**

- **The drawn launcher is `bof3x-launcher`** on both systems:
  `bof3x-launcher.exe` on Windows at 32 and 64 bits, `bof3x-launcher` on
  Linux. The CMake target `bof3x_launcher_ui` is `bof3x_launcher` (its
  source list `BOF3X_LAUNCHER_SOURCES`), and the name `bof3x-launcher-ui`
  is gone from the program's lines, its usage text and the tools. On
  Windows it has `launcher.rc` again, cut to what a player's program needs:
  the version block, the icon (the first icon resource, which SDL also
  gives the window) and a manifest with the program's identity and
  `asInvoker`. The manifest has no DPI line: SDL declares the window
  per-monitor DPI aware (v2) when its video starts
  (`SDL_windowsvideo.c` `WIN_InitDPIAwareness`, no hint set), and a DPI line
  in the manifest would make that call fail. It has no Common Controls 6
  dependency: no common control is left in either launcher.
- **It takes the Win32 launcher's command line and start** (`main.cpp`):
  `--game`, `BOF3_GAME_DIR`, `--config`, `--no-config`, `--host exe|bof3`,
  `-- <game args>`, with their meanings. Neither `--config` nor
  `--no-config`: `show_launcher` decides. `--no-config`, `show_launcher=0`
  and a self-test start the game with no window (a new phase of the start,
  `kNoWindow`: the settings as read, nothing saved, as the Win32 launcher).
  Under `--no-config` and a self-test there is no Sources page first and no
  stale-install check (`StartSetUpFor` and the check's own condition, as
  before). With `show_launcher=0` and no `--no-config` the Sources page's
  first run and the stale question still come first, as they did in the
  Win32 launcher, whose `want_dialog` was -1 there. The lines it prints come
  from `start.cpp` as before. What `launcher.cpp` did around the start that
  the drawn `main.cpp` did not, each now in `main.cpp`: `AttachParentConsole`
  (now the seam's `AttachParentTerminal`), `RemoveOldIconFiles`, the
  `bof3x.dll is missing` check (on Windows), `HostedOnlyAsked`,
  `ForwardToDevLauncher`, the `BOF3X_IMAGEBASE=...: starting ...` line and
  its "is not a number" refusal, the Win32 launcher's sentence for a missing
  `bof3x.exe`, and the `--host` refusal. `--set` and `--list` came from
  `headless.cpp` with their texts.
- **The hosted build and a moved image are the developer launcher's.** The
  player's launcher hands such a run to `bof3x-launcher-dev.exe` beside it,
  with the same arguments and environment, on its own file or pipe when a
  script gave one, and ends with its exit code. It does this through the
  seam: `ChildSpec::own_output` is new (`os/process.h`; Windows: the
  process's standard handles, inherited, when stdout and stderr both go to
  a file or a pipe; POSIX: nothing to do, the fork keeps them), and so are
  `OutputRedirected` and `AttachParentTerminal`. `tools/os_host` tests
  `own_output` with a relay on a pipe whose child is `os_tests_gui.exe`, the
  same tests built as a windowed program, because a console child finds its
  parent's handles without being given them and a windowed one does not
  (with `own_output` false the check fails, 2 of 632; true, 0 failed). With
  no developer launcher beside it (a release) the refusal is the Win32
  launcher's sentence; on Linux it says that the hosted build is on Windows
  only.
- **A refusal shows a message box only when no script can wait for it.**
  The rule (`main.cpp`, `g_no_box`): no box under `--check`, under
  `--no-config`, under a self-test (`BOF3X_SELFTEST_ONLY`), or when stdout
  and stderr both go to a file or a pipe (`OutputRedirected`). A player's
  start from a double-click or a terminal with no `--no-config` still shows
  the box. A failed hand-over to the developer launcher in a scripted run
  is covered by the last two. The Win32 launcher showed its box under
  `--no-config` too (measured below: its box stopped a run of this stage
  until the process was ended).
- **The developer launcher stays Win32 and 32-bit** (section 6).
  `launcher.cpp` is that launcher alone, 363 lines: the hosted start (the
  injection) and a moved image's fence, the settings read, no window, as
  `--no-config` always; `--config` is refused with a sentence; its other
  refusals keep their texts, and none shows a box. It does not call
  `RemoveOldIconFiles` (the player's launcher does). `key_codes_check.cpp`
  (the build-time check of `src/input/key_codes.h` against the Windows
  SDK's names) is compiled with it, in the 32-bit build. The player's
  launcher imports none of `VirtualAllocEx`, `WriteProcessMemory`,
  `CreateRemoteThread` (`llvm-readobj --coff-imports`, both widths); the
  developer launcher imports all three. `build.yml` did not check it
  before; it does now, in both jobs.
- **Removed** (lines): `config_dialog.cpp` 2,190 and `.h` 27,
  `sources_dialog.cpp` 1,449 and `.h` 89, `save_dialog.cpp` 1,015 and `.h`
  16, `music_player.cpp` 943 and `.h` 25, `resource.h` 221, `headless.cpp`
  142, `launcher.rc`'s 21 dialog templates (660 of its 672 lines),
  `launcher.cpp`'s window half (247 lines out, 59 in), the
  `bof3x_launcher_parts` library and the libraries only the dialogs linked
  (`comctl32`, `comdlg32`, `uxtheme`, `windowscodecs`, `shell32`, `ole32`,
  `winmm`). The tools that drove the Win32 window:
  `tools/launcher_host/dialog_test.py` 926, `capture_page.py` 158,
  `save_editor_drive.py` 341, `ui_compare.py` 622 (its 66 cases are on
  record in sections 10 to 13), `tools/jukebox_host` 113 (its result is in
  section 13). `save_editor_check.py` loses its two cases that ran the Win32
  editor. In all, 52 files: 1,201 lines added, 11,469 removed, the censuses
  included. `src/launcher/` is 17,017 lines of `.cpp` and `.h`.
- **Kept:** `tools/key_host/capture_tests.cpp`, with its own copy of the
  Win32 capture's rule, now said to be the capture as it was, removed
  2026-11-20. The Linux round planned a tarball with a start script for
  this stage to remove; no such tarball or script exists in the tree.
- **The release:** the zip's `bof3x-launcher.exe` is the drawn one, with
  `music-names.tsv` and `save-names.tsv` beside it, and a new `licenses\`
  folder: Dear ImGui's MIT notice from the build's fetched tree, and
  `third_party/droid-sans/` (new: the Apache License 2.0's text and a
  README with the font's name-table notices) and
  `third_party/tex-gyre-pagella/`'s four text files
  ([`THIRD_PARTY.md`](THIRD_PARTY.md) sections 6 and 7, both owed lines
  paid). The release's README says what the folder holds.
- **The censuses**, each by its own tool: `analysis/os_census.tsv` (its
  `--check` failed on the seam's new sites): the launcher's rows 1,802 to
  124, `src/os/win32/process.cpp` 52 to 79. `analysis/width_census.tsv`
  (its `--check` passed and named 8 keys gone) and
  `analysis/ledger_switches.tsv` (passed; the readers of five switches now
  name the files that read them). No ledger line was touched: no gate needs
  a file that `docs/DIVERGENCE.md` cites to exist (`ledger_check.py` 242
  entries, 0 errors, after the removal).

**Measured.**

- **Builds.** `cmake --preset i686 -DFETCHCONTENT_SOURCE_DIR_SDL3=...` and
  `cmake --build build`: whole, no error (the launcher 37,947,392 bytes, the
  developer launcher 9,809,408). `cmake --preset x86_64 ...` and `cmake
  --build build-x86_64`: whole, no error, the launcher an x86-64 image. In
  the WSL (`rsync` to `~/src/bof3x-p6`, `cmake --preset linux-x86_64
  -DSDL_X11_XTEST=OFF`, target `bof3x_launcher`): an ELF
  `bof3x-launcher` with both names files beside it.
- **The two launchers give the game the same start.** As stage P0's C7: the
  old Win32 launcher (`C:\Users\kerok\bof3x-runs\p2cmp\L\bof3x-launcher.exe`,
  built from `launcher/drawn` before this stage) and the new one, each in
  turn in one folder with a stand-in `bof3x.exe` that writes down its
  environment (sorted), command line and working folder and ends with a
  given code, a dummy `bof3x.dll`, and a fresh copy of one `bof3x.ini`
  (`p2cmp\start.ini`), the game folder the tooling install
  (`D:\bof3x-runs\p6cut\startcmp.sh`). Eight cases: the plain ini, a preset
  (`preset=shipped`), `-- -a "two words" D:\x\`, `BOF3X_SELFTEST_ONLY=1`
  with the stand-in's code 5, `show_launcher=0` and no `--no-config`, a
  player's start from a cache image (`BOF3X_CACHE`, no `BOF3X_EXEFILE`),
  `BOF3_GAME_DIR` in place of `--game`, and `BOF3X_IMAGEBASE=0x400000`.
  In every case the exit code, stderr, the stand-in's record (129 to 141
  lines), `BOF3.CFG` and the ini afterwards are byte-identical, and stdout
  is identical but for the pid. The self-test carries code 5 back from
  both. The two refusals were run on the new launcher alone: a bad
  `BOF3X_IMAGEBASE` and an unknown argument end with exit 1 and a line on
  stderr, no box, `BOF3.CFG` and the ini unchanged. A first run of the
  bad-`BOF3X_IMAGEBASE` case on the old launcher showed its modal box under
  `--no-config` and waited until the process (this stage's own) was ended.
  The tooling install's `BOF3.CFG` was put back as it was.
- **The harness runs through it.** A launcher copy as the harness makes one
  (`D:\bof3x-runs\p6cut\harness`: the four files, no ini, no names files;
  a missing names file does not stop a `--no-config` start):
  (a) `BOF3X_SELFTEST_ONLY=1 BOF3X_EXEFILE=1 bof3x-launcher.exe --game
  ...\bof3 --no-config`: exit 0, "self-test only, game process exit code
  0", and `self-test only: done, ending the process` in `bof3x.log`.
  (b) `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=EffectKind5B_Fade` the same way:
  handed to the developer launcher, its lines (the hosted-only line, the
  key table, the pid, exit code 0) on the file, exit 0, the log's
  `self-test only: done`; the same with the output to a pipe; with no
  developer launcher beside it, the refusal and exit 1 with no box, with
  and without `--no-config` (the output on a file and on a pipe). A moved
  image goes to the developer launcher, which refuses the missing
  `bof3x-A00000.exe` with its sentence; `--config` with a hosted switch
  reaches the developer launcher, which refuses it.
  (c) `python tools/attract_run.py --out D:\bof3x-runs\p6cut\attract\a.tsv
  --minutes 2 --game ...\bof3 --no-front --launcher
  D:\bof3x-runs\p6cut\harness\bof3x-launcher.exe`: "3596 logic frames in
  120 s", exit 0.
- **The window, with no screen.** An untouched window with every page, the
  Jukebox, the save editor and the Sources page opened (`--check`, the
  owner's home as `BOF3X_HOME`, `--game ...\game\game`,
  `SDL_AUDIODRIVER=dummy`, `D:\bof3x-runs\p6cut\window_check.sh`) writes a
  copy of the owner's `bof3x.ini` byte for byte (7,659 bytes); the owner's
  saves keep their dates. `save_editor_check.py` on the build: 97 checks,
  0 failures. `tools/key_host`: `key_tests` 133,780 checks and
  `capture_tests` 329, 0 failed. `tools/launcher_host`: `config_tests`
  4,402 checks and `save_tests` 139, 0 failures (its `CMakeLists.txt` names
  no removed file). `tools/os_host`: 632 checks on Windows, 544 in the WSL,
  0 failed. In the WSL the same script but the Sources page (no install
  there) writes the same ini as the Windows build does from the same input
  with no install (both turn `language=en-US` into `original`: the
  window's DIV-0102 rule when no install holds the language).
- **`--set` and `--list`** against `headless.cpp` itself, built from the
  branch before its removal: on Windows `--list` (105 lines), `--set
  filter=linear --set display=fullscreen --list` and the ini it writes are
  byte-identical, and a refused key gives the same sentence; in the WSL the
  same but for the line ends (LF there, CRLF from the Windows build).
- **The hosted build on Linux:** `--host bof3`, `BOF3X_SHADOW` and
  `BOF3X_IMAGEBASE=0xA00000` each end with exit 1 and the Linux sentence.
- **The release zip** (`python tools/make_release.py --version
  v0.1.0-p6test --build build --out D:\bof3x-runs\p6cut\release\...`, not
  published): 1,219 files, 74.6 MB, the game-data scan clean; at its top
  `bof3x-launcher.exe`, `bof3x.dll`, `bof3x.exe`, `music-names.tsv`,
  `save-names.tsv`, `README.txt`, `LICENSE`, `THIRD_PARTY.md`,
  `THIRD_PARTY_RUNTIME.txt`, then `licenses\` (seven files) and
  `runtime\`. The launcher started from the unpacked folder with `--check
  --capture` draws the title page, and a script through the Jukebox, the
  save editor and Data ends 0; nothing is beside it but what the zip holds.
  Its resources: version, manifest, icon.
- **The gates**, as `checks.yml` runs them: `ledger_check`,
  `ledger_switches --check`, `ledger_registry --check`, `data_census
  --check` and `--strict`, `reloc_census --check`, `reloc_bare --check`,
  `width_census --check`, `tu_renames --check`, `tu_census --check
  --strict`, `retire_shells --check --strict`, `os_census --check` and
  `--boundary`, and `linux_tree.py --check` in the WSL: all pass.

**Not run, or not shown.** No window on a screen, no hand, no key or pad, no
picker, question or box seen: everything above is `--check` or a start with
no window. The release's zips started on a clean machine; Windows Defender's
verdict on the new executable (it is the drawn one, 37 MB, unsigned in a
local build); the 64-bit zip; the Linux launcher starting a real game (the
WSL has no Linux game build here). A message box under the new rule on a
player's double-click was not shown (by design of this stage). The start
comparison used `BOF3X_EXEFILE=1` for seven of its eight cases; the eighth is
a player's start from a cache image.

**On the pull request's branch, the coordinator's own runs** (2026-11-20,
`launcher/drawn` moved forward to this stage's commits): the 32-bit and the
64-bit builds whole; the player's launcher at both widths imports none of
`VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread`, and the
developer launcher all three; from a launcher copy of the four files with
no ini, `--no-config` runs the exe host's self-tests to exit 0, a
hosted-only switch is handed to the developer launcher and comes back 0
with its lines on a file and on a pipe, and with no developer launcher, or
an argument that is not known, the launcher prints its sentence and ends 1
with no box; an untouched window with every page, the Jukebox, the save
editor and the Sources page opened writes the owner's `bof3x.ini` byte for
byte; `save_editor_check.py` 97 checks, 0 failures; every gate of
`checks.yml` that reads the tree; in the WSL the launcher builds as
`bof3x-launcher`, `--list` and `--set` work, a page is captured, and
`--host bof3` is refused with the sentence. The pull request's checks were
9 of 9 at stage P5's commits.
