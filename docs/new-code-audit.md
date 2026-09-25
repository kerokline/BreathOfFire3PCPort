# Audit of the code new to the game: suspected bugs and open questions

**Status:** IN PROGRESS (2026-09-25; everything below was found by reading the
code, and nothing has been run against the game)

On 2026-09-25 the 31 source files with no Capcom counterpart got a readability
audit: the render backend and looks, the launcher and input, the hook layer,
the language and text features, and the cheats. The comment fixes went in
then (commit "Readability audit of the code new to the game"). This file holds
what the audit could not settle without `BOF3.exe`. It is written for a session
that has the game files.

**How to use it.** Each item says where, what the code does, and how to check
it in game. Close an item by writing what was found under it, and a ledger
entry if the fix changes what the player sees (`CLAUDE.md` rule 2). When
nothing is left open, mark the file STABLE. Line numbers are as of the audit
commit, so search for the function name if they have moved.

## A. Bugs confirmed by reading the code

Each was read to the point where the failure follows from the code. None has
been seen to happen. All eight were re-read against the code and fixed on
2026-09-25 (branch `phase-3/audit-a-fixes`); the headless self-test passes
(exit 0, 1,448 injects). What each still owes in game is under it.

### A1. A released surface's slot can be reused while a draw still needs it

`src/render/render_shim.cpp`, `MakeSurface` (~848) and `Surface_Release`
(~309); `src/render/render_d3d11.cpp`, `Bind`.

`Surface_Release` snapshots a surface that has draws pending, then frees its
pixels. `MakeSurface` reuses any slot with `refs == 0`, `gpu == nullptr` and
`pixels == nullptr`, and `memset`s it. `gpu` is created only at `Bind`, during
the present. So a surface that was created, drawn and released within a frame
not yet presented looks free. A `CreateSurface` in the same frame then takes
the slot. At the present, `Bind` converts the pending draw's snapshot through
the *new* surface's width, height, pitch and format (`Convert(s,
version->pixels)`). A larger surface reads past the snapshot in the frame
arena, and a different format misreads it.

- **Reach:** needs create, draw, release and create again within one frame
  that is not presented. That is the frame-skip-across-an-area-change shape
  of the 2026-09-24 fix (docs/render-backend.md, "Released surfaces with
  pending draws").
- **Check:** log in `MakeSurface` when it reuses a slot whose `TexVersion` is
  still referenced by the current frame's commands. Then play area changes
  with the window unfocused or the title bar held, the way D4 was reproduced
  (docs/known-defects.md).
- **Likely fix:** do not reuse a slot that is referenced from the current
  frame. Either keep a `pending_draws`-style count that survives the release,
  or reuse slots only in `SweepReleased`.
- **Fixed 2026-09-25:** a release that takes a snapshot sets
  `Surface::snapshot_held`; `MakeSurface` skips such a slot and `ResetFrame`
  clears the flag (docs/render-backend.md). **Owed:** the area-change
  frame-skip runs above, which should finish as before.

### A2. The Japanese overlay switches the F9 pause lines to English

`src/game/dat_load.cpp` ~166: `case 4` calls `PauseText_Apply()` for every
kind-4 chunk. `tools/loc_build.py` (~1234) gives the Japanese overlay a kind-4
chunk too. DIV-0038 means the English lines for the English overlay, and
DIV-0056 says the exe's own strings stay Chinese under Japanese. So with
`BOF3X_LANG=ja`, F9 would show the ASCII English lines, drawn through the
Japanese glyph table.

- **Check:** `BOF3X_LANG=ja`, press F9 in game and on the title.
- **Decision for the owner:** Chinese lines (gate `PauseText_Apply` on the
  language, and amend DIV-0038 and DIV-0056 to say so), or Japanese lines (new
  data, a new ledger entry).
- **Decided and fixed 2026-09-25:** a translation per language (the owner).
  French and German had the same bug. The lines moved out of the DLL into
  the overlay: `loc_build.py` `PAUSE_LINES`, chunk kind 14, DIV-0038 and
  DIV-0056 amended. **Owed:** rebuild the four overlays (`loc_build.py all`
  per disc - until then the English overlay shows Capcom's lines), then F9 in
  game and on the title under each of en, fr, de and ja, with captures for
  the owner to judge the fit (and C1).

### A3. A binding to the 0 key never shows in the Controls dialog

`src/launcher/config_dialog.cpp` ~409: an unnamed key is detected by
`KeyName(dik)[0] != '0'`, because unnamed scancodes are spelled `0x..`. But
the 0 key (DIK `0x0B`) is named `"0"` (`src/input/bindings.cpp`, the key
table). A saved binding on 0 goes to `kept`: it stays active but never shows
in a cell.

- **Check:** bind 0 in `bof3x.ini`, open Controls.
- **Fix:** test for the `0x` prefix, not the first character.
- **Fixed 2026-09-25.** **Owed:** the check above.

### A4. A recording loses its last run

`src/hook/input_script.cpp`, `Record` / `FlushRun`: a run of the same pad
word is written when the word changes or at a shot (F12). Nothing flushes the
run that is open when the process ends. `DLL_PROCESS_DETACH` in `dllmain.cpp`
does not call into the recorder. So every `BOF3X_RECORD` file is missing its
last hold or wait.

- **Check:** record a short route that ends on a held button, then read the
  last line.
- **Fix:** flush on detach, or at the recorder's own end.
- **Fixed 2026-09-25:** `InputScript_Stop` at `DLL_PROCESS_DETACH` writes
  the open run and closes the file. A process ended by `Fatal` gets no detach
  and still loses it. **Owed:** the check above.

### A5. The call tracer's file names can run one character past the path buffer

`src/hook/calltrace.cpp` ~356: the length check allows for `calltrace.tsv`
(14 wide characters with the terminator). `calldetail.tsv`, `callframes.tsv`
and `callcounts.tsv` are 15, and are copied into the same
`wchar_t path[MAX_PATH]`.

- **Reach:** only with the DLL's path within a character of `MAX_PATH`.
- **Fix:** check against the longest name.
- **Fixed 2026-09-25.**

### A6. A hand-edited SatPixie value in bof3x.ini stops the game

`src/launcher/config.cpp` ~114 passes `satpixie.*` values from the ini through
to `BOF3X_SATPIXIE` unchecked. `src/render/satpixie.cpp` (~256) treats a value
outside the preset's range as Fatal, e.g. gamma outside 1.8..2.6. `ConfigLoad`'s
own rule is that a settings file is never something to fail on.

- **Check:** `satpixie.gamma=3` in `bof3x.ini`, with the SatPixie look.
- **Fix:** clamp or drop out-of-range values in the launcher. The dialog
  cannot produce them, since its trackbar sliders are confined to a range;
  only a hand edit can.
- **Fixed 2026-09-25:** `ConfigLoad` drops a value outside the preset's
  range, as it drops any value it does not know. So does NaN, which passed
  the DLL's own range test too (every comparison with it is false).
  **Owed:** the check above, which should start with the default gamma.

### A7. `Channel` shifts by a wrapped count for masks under 4 bits

`src/render/render_shim.cpp` `Channel` (~139): `bits - (8 - bits) > 0` is
unsigned, so for `bits < 4` it wraps to true, and the shift `2 * bits - 8`
wraps to a huge count, which is undefined. `render_d3d11.cpp`'s copy uses
`2 * bits > 8`, so the two now differ.

- **Reach:** latent. `MakeSurface` gives 16-bit surfaces 5-6-5 masks and
  `Channel` is used only for red, green and blue. It would bite if a 1-, 2- or
  3-bit colour mask ever arrived.
- **Fix:** the d3d11 copy's form.
- **Fixed 2026-09-25.**

### A8. Clearing a colour key does not mark the surface for re-upload

`src/render/render_shim.cpp` `Surface_SetColorKey` (~437): setting a key calls
`BeforeWrite`, but clearing one (`key == nullptr`) only clears
`has_color_key`. The GPU copy keeps the keyed alpha until something else
dirties the surface, and pending draws recorded with the key lose it.

- **Check:** whether the game ever clears a key on a surface it keeps drawing
  (log in `Surface_SetColorKey`).
- **Wider than read:** a snapshot did not keep the key either, so a key
  *changed* mid-frame re-keyed the draws already pending.
- **Fixed 2026-09-25:** `SetColorKey` is a write whether it sets or clears,
  and each snapshot carries the key it was taken under
  (docs/render-backend.md). **Owed:** nothing specific; the batch's captures
  should look as before.

## B. Reported by the audit, not yet checked

Each is the audit agent's reading. Confirm it or strike it.

- **B1** `render_shim.cpp` ~567: after 16 distinct unknown render states, each
  new one is logged on every call, not once.
- **B2** `render_shim.cpp` `IsSurface` (~116), used by `Blt` / `BltFast`: a
  released surface still passes, and `CopyRect` would read null pixels.
- **B3** `config_dialog.cpp` ~87: `(i + n) % n` divides by zero if a combo is
  ever empty when the pad cycles it. All combos are filled today.
- **B4** `CaptureKeyHook`: Escape cancels a capture, so Escape can never be
  bound from the dialog, though the default table binds it. The dialog's text
  says "Escape cancels", so this may be intended.
- **B5** `launcher.cpp` ~145: `Fatal` during `DllMain` (outside self-test)
  leaves exit code 3. The launcher's `module == 0` test then passes, and it
  goes on to `ResumeThread` a dead process. The DLL has already shown its own
  box, so the effect is a misleading second message.
- **B6** `input_script.cpp` ~608: a `BOF3X_RECORD` or `BOF3X_INPUT` path
  longer than `MAX_PATH` is silently ignored rather than Fatal.
- **B7** `crt.cpp` `CrtWanted` (~202): nothing calls it, and it is wrong for
  `satpixie`. It also leaves `text` unterminated for a value of 16 bytes or
  more. Candidate for deletion. `MakeSized` (~215) is outside the anonymous
  namespace, so it is an external `render::MakeSized`.
- **B8** `crash.cpp` ~122: any `ExceptionInformation[0]` of 2 or more prints
  as "executing". Only 8 (DEP) means that.
- **B9** `menu_verbs.cpp` ~131: `MenuVerbs_Inject` re-aims the label call for
  any `BOF3X_LANG`, "original" included. `YesNoLayout_Inject` excludes
  "original". Probably harmless, since the offset is zero with no table
  loaded, but the two disagree.
- **B10** `text_pairs.cpp` ~42: a two-byte lead followed by NUL can report a
  pair that the expansion loop never expands. The only effect is an unneeded
  copy.

## C. Open questions

- **C1 - the pause lines' width.** `pause_text.cpp` says the English lines
  are "kept under about 37 characters", because 8 units a character must fit
  the 320-unit screen. 320 / 8 is 40, and the four lines are 31 to 35
  characters (`kLines`). The owner's recollection (2026-09-25) is that the
  real limit is nearer 21 Latin letters or 14 glyphs, so 37 does not sound
  right. **Check in game:** F9 with `BOF3X_LANG=en`. Measure where the box
  or the screen cuts a line, and whether all four fit. Then correct the
  comment, and the lines if they overflow (DIV-0038).
  (2026-09-25: the comment went with the lines' move to `loc_build.py`,
  which now refuses a line wider than 320 units and prints the widest - 280
  for English. The in-game check still decides whether 320 is the real
  limit.)
- **C2 - `IDC_RESOLUTION`.** `src/launcher/resource.h:10` defines it, and
  nothing uses it. The owner: probably left over from the scale dropdown,
  which the window's snap-to logic replaced (DIV-0042). Remove it when next in
  `launcher.rc`.
- **C3 - flat shading on a triangle fan. Closed 2026-09-25.**
  `render_shim.cpp` said a fan's flat colour comes from the shared vertex 0;
  Direct3D's rule may take vertex i + 1. It does not matter: the game draws
  no fans. BOF3.exe has 21 `DrawPrimitive` calls (`push 0x1C4`, the TLVERTEX
  format, each followed by `push type`), and their types are 5 (10 calls),
  3 (6), 4 (4) and 1 (1, `0x5A22E8`) - never 6. The one other call through
  the device's `+0x70` slot, `0x5A563B`, passes two arguments, so it is not
  `DrawPrimitive`. Our ported handlers pass 3, 4 and 5; the game's "fan"
  effect `MagicFx_DrawFan` is separate triangles before it reaches Direct3D.
  So a flat-shaded fan is now Fatal ("not built") instead of drawn by an
  unchecked rule; Gouraud fans are unchanged. No ledger entry: nothing drawn
  changes.
- **C4 - `kZNearest`.** `render_shim.cpp` ~580: 1/4096 as the smallest depth
  `Gte_PrimDepths4_10B` hands the handlers. This matches DIV-0044, but nobody
  has checked it against the binary.
- **C5 - SatPixie's defaults.** `blur_x = blur_y = 0` make both blur passes
  a no-op. Are those really the preset's values? And the feedback textures are
  created with no initial data, so the first frame after init or a resize
  reads whatever they hold. Is that intended?
- **C6 - the crash reporter during `DllMain`.** The Reporter thread
  `Crash_Start` creates probably cannot run until `DllMain` returns (the
  loader lock). So a fault inside an `InjectAll` fuzz would wait out the 20 s
  and produce no report. This is reasoning about Windows, not tested. Check:
  `BOF3X_CRASH_TEST` during start-up.

## D. Settled during the audit

- **The modules injected after `Widescreen_Inject`** (`inject_all.cpp`
  ~209-231): WorldMap, BattleDraw, BattleWindowDraw, BattleWindows,
  BattleFlow, BattleSetup, BattleMisc, BattleDamage, BattleItems,
  BattleSprites, InventoryOps. The rule the order protects is that a module's
  fuzz clones original bytes, so it must run before any widescreen patch lands
  inside them. Each of these modules re-aims every call from its clones to a
  recording stand-in, so only a patch *inside* a cloned body matters.
  Widescreen patches 18 places: four `fcomp` operands in `0x510780`, and the
  14 slide bounds `0x596926`..`0x59C136`. A scan of every base and size pair
  in the eleven fuzz files found **no overlap** (2026-09-25). The scan was a
  regex over the source, not a run. **Confirm in game:** run
  `BOF3X_SELFTEST_ONLY` with `BOF3X_WIDE` on and off, and compare. Every
  self-test should report the same.
- **`char_names.cpp`'s garbled line** is now explained and fixed. `0x669736`
  is `Char_WhelpSlot` (`symbols.toml`), a byte holding 7. New Game copies the
  whelp's default record into the slot whose number that byte holds. The same
  comment said a mismatch means "nothing is written"; the code stops the game
  (Fatal) instead, and the comment now says so.
