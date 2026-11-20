# One launcher for Windows and Linux: what it would take

**Status:** IN PROGRESS (2026-11-20: stage P0 is in `main`, the Linux
round's group C7; stage P1, the frame, and stage P2, the settings pages,
are built on the local branch `launcher/p1-frame`, sections 9 and 10; stage
P3, Controls, on the local branch `launcher/p3-controls`, section 11, with
D427 for the owner; stages P4 to P6 are not started. Sections 1 to 8 are the staging of 2026-11-19, measured
at `main` `8cd45199`.)
An estimate and a staging for the owner's question "a launcher that works on
either Windows or Linux". It is tier 3 of [`linux-round.md`](linux-round.md)
(section 5, stand-in 3; section 8) and IDEAS I69. Stage P0 below is also
group C7 of that round's staging and is needed for any native Linux build.
Of the owner's four calls in section 7, calls 1 and 4 are answered; calls 2
and 2 are answered; call 3 is open.

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

## 11. Stage P3 as run (2026-11-20, branch `launcher/p3-controls`)

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

**D427, for the owner.** Both captures store Pause as 0x45 and NumLock as
0xC5: the drawn one keeps the Win32 numbers, since D427 is open and only
proposes the swap. What the game then reads, from the code: a game button
reads the key bytes in DirectInput's numbering (`key_sdl.h`
`FillKeyState`), where Pause is 0xC5 and NumLock 0x45, so a game button
bound to Pause in either launcher is worked by NumLock, and the reverse; a
function key reads the key message (`FnKeyOfMessage`, and
`FnMessageBitsOfDik` off Windows), which gives Pause 0x45 and NumLock 0xC5,
so a function key bound to Pause works with Pause. The choices: keep both
as they are (the function keys right, the game buttons crossed); swap at
the capture as D427 proposes (the game buttons right, and the function keys
then crossed unless `FnKeyOfMessage` swaps too); or store per page. Each
says what an ini that holds one of the two already means.

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
