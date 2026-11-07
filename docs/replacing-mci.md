# Replacing MCI with a bundled decoder

**Status:** BUILT 2026-11-07 as DIV-0205 (section 8); sections 1..7 are the design note of 2026-09-19, kept as read

What it would actually take to stop playing FMV through Windows' MCI/VFW stack
and decode it ourselves. Not scheduled: filed as [`IDEAS.md`](IDEAS.md) I7, LOW.
The compatibility problem this would have solved is already solved more cheaply
by [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0001; this note exists so the option is
costed rather than re-argued from scratch later.

**Since then (noted 2026-09-24):** `Fmv_Play` `0x59E360` is ours with MCI
kept - the videos play into the window at an integer scale (DIV-0035) and
keep playing when the window is not in front (DIV-0049) - so the exclusive
640 x 480 mode-set that §6 says presentation work has to remove (and
[`windowed-mode.md`](windowed-mode.md) calls the most fragile operation in
the program) no longer happens.
The decoder costed here is still not built; I7 stays on the list.

**Built (2026-11-07):** section 8. The 64-bit build showed no movie (D407):
Windows' Cinepak decompressor is 32-bit only (Video 1's is not), so the
decoder that was "worth doing eventually" became the only way to a picture
there.

Named `replacing-mci.md` rather than `replacingMCI.md` to match the repo's
kebab-case convention for investigation notes ([`README.md`](README.md)).

## 1. The surface is one function

Every MCI command in `BOF3.exe` is issued by **`0x59E360`**. There are exactly
two call sites, both in `0x4FCB00`, playing `CAPCOM.AVI` then `LOGOS.AVI` back
to back at startup.

**Evidence:** `tools/pe_xref.py` over the five MCI command-format strings
(`0x66B674`, `0x66B654`, `0x66B62C`, `0x66B608`, `0x66B5E8`, `0x66B5D8`,
`0x66B644`) — every one has exactly one reference and all of them are inside
`0x59E360`. The filename literals `CAPCOM.AVI` (`0x65DAB8`) and `LOGOS.AVI`
(`0x65DA6C`) resolve to `0x4FCB00`, at `0x4FCD0F` and `0x4FCD26` (2026-09-19).

Signature, from the stack frame (`sub esp,0x6C` plus three pushed registers, so
arguments begin at `esp+0x7C`):

```c
void Fmv_Play(const char *filename, HWND hwnd, int fullscreen);
```

That is the entire replacement surface. Not a subsystem — one function, three
arguments, two callers.

## 2. What it does, in order

Disassembled 2026-09-19 (`tools/pe_disasm.py 0x59E360`, `0x59E4F0`, `0x59E570`).

1. `sprintf(buf, "open avivideo!%s alias vfw", filename)`, then `mciSendStringA`.
   **On failure it retries** as `"open avivideo!%s%s alias vfw"` with a prefix
   from `0x5A7370` — a one-instruction accessor returning `0x66BC2C`, a `.data`
   string initialised to `"C:\"`. That is the CD-ROM root (`GetDriveTypeA` is
   imported), so the fallback is "look for the video on the disc". If the second
   open also fails, the function returns having done nothing.
2. If `fullscreen` is set, calls `0x59E4F0`, which:
   - `SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP)`, saving the old style to
     `0x6BE1C4`;
   - **`DirectDrawCreate(NULL, &g_FmvDDraw, NULL)`** — a *second, separate*
     DirectDraw object at `0x6BE1CC`, distinct from the game's;
   - `SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)` (vtable
     `+0x50`, flags `0x11`);
   - **`SetDisplayMode(640, 480, 16)`** (vtable `+0x54`);
   - `SetWindowPos(..., 640, 480, SWP_SHOWWINDOW)`.

   Any failure returns 0, and `Fmv_Play` then sends `close vfw wait` and gives up.
3. `SetWindowLongA(hwnd, GWL_WNDPROC, 0x59E570)` — **subclasses the window**,
   keeping the old procedure in a local.
4. `window vfw handle %d`, then `put vfw destination at 0 0 640 480`. The
   destination rect is a fixed literal; there is no scaling logic.
5. `play vfw window from 0 notify`, and sets the playing flag `0x6BE1C8 = 1`.
6. **A private, blocking, modal message loop**: `GetMessageA` /
   `TranslateMessageA` / `DispatchMessageA` until `0x6BE1C8` clears. `GetMessage`,
   not `PeekMessage` — it blocks, and the game's own loop does not run.
7. `stop vfw wait`, `close vfw wait`, restore the original WndProc.
8. If the FMV DirectDraw object exists: `Release()` it (vtable `+0x8`),
   `SetWindowPos` back to the dimensions in `0x939A30`/`0x939A34`, and restore
   the saved window style from `0x6BE1C4`.

The installed window procedure `0x59E570` is small and handles four messages:

| Message | Behaviour |
|---|---|
| `WM_KEYDOWN` (0x100), `WM_LBUTTONDOWN` (0x201) | clear `0x6BE1C8` — this is skip |
| `WM_ACTIVATEAPP` (0x1C), wParam != 0 | `SetFocus(hwnd)` then `mciSendStringA("resume vfw")` |
| `WM_DESTROY` (0x2) | handled |
| anything else | falls through to the saved procedure |

## 3. The contract a replacement must honour

This is the useful output of the disassembly, and it is smaller than expected:

- **Blocking and modal.** The caller expects `Fmv_Play` to return when the video
  is over or skipped. There is no frame-loop integration to design, no state
  machine to thread through the game's update, no partial-frame delivery.
- **No compositing with the game's renderer.** FMV runs on its *own* DirectDraw
  object in its own exclusive-fullscreen mode; the game's presentation is not
  live during playback. A replacement does not have to interoperate with the
  renderer at all — it has to borrow the window and give it back.
- **Fixed 640x480.** Both the display mode and the destination rect are literals.
- **Skip on key or click**, resume on app re-activation.
- **Leave the window as you found it** — style and position are saved and
  restored, and a replacement must do the same or the game returns to a
  `WS_POPUP` window.
- **Failure is non-fatal.** Every failure path returns quietly and the game
  proceeds. Worth preserving: it is why a missing video has never crashed anyone.

## 4. What building it entails

1. **Decode and blit.** A decoder producing RGB frames, and a blit into `hwnd`
   at 640x480. `StretchDIBits` is sufficient and is roughly what MCI was doing.
2. **Pacing and A/V sync.** The real work, but self-contained: present on a
   clock, feed PCM to DirectSound (already initialised by the game). ~7.5 s and
   ~8 s of content, 15 fps, one audio track between them.
3. **Message pump and skip semantics.** Reuse the existing shape — keep the
   subclass, keep the flag, keep key/click to skip, keep `WM_ACTIVATEAPP`.
4. **Re-encode both videos again** to whatever the bundled decoder reads. This
   supersedes DIV-0001, which the ledger handles explicitly (`SUPERSEDED by
   DIV-NNNN`).
5. **Decide what to do about the exclusive-fullscreen mode-set** (§6).

## 5. Codec and licensing

The decoder choice is a licensing decision before it is a technical one, because
vendoring copyleft would end the commercial path
([`LICENSING.md`](LICENSING.md) §4, `CLAUDE.md` rule 5).

| Option | Source licence | Patents |
|---|---|---|
| `pl_mpeg` (MPEG-1, single header) | MIT — **verify before adopting** | MPEG-1 expired |
| `libvpx` (VP8/VP9) | BSD | royalty-free |
| `dav1d` (AV1) | BSD | royalty-free |
| FFmpeg libavcodec | LGPL, or GPL depending on configuration | varies by codec |
| H.264 | — | **MPEG-LA patent licensing; a real cost** |

`pl_mpeg` is the natural fit: one file, permissive, and MPEG-1 at 640x480x15fps
is far more than these two videos need. FFmpeg's LGPL configuration is
permissible dynamically linked but is a large dependency for two logo videos, and
its GPL configuration is disqualifying. **H.264 is the trap** — codec source
licensing and patent licensing are separate questions, and the second lands
directly on [`LICENSING.md`](LICENSING.md) §3.

## 6. Recommendation: worth understanding, not worth doing yet

The compatibility argument is **gone** — DIV-0001 already restored the video
through the port's own path for the cost of one ffmpeg command. What remains:

- *Higher-resolution FMV* — **no**. There is no higher-resolution source. The
  assets are 640x480 and that is all there ever was; upscaled logos do not
  justify a bundled decoder and a patent review.
- *Dropping the last OS multimedia dependency* — **yes, eventually.** MCI/VFW is
  a 1992 API being kept alive by compatibility shims, and it will rot like the
  rest.

The thing that sets the timing is §2 step 2: **FMV performs an exclusive
fullscreen `SetDisplayMode(640, 480, 16)` on a DirectDraw object of its own.**
That mode-set is exactly what presentation-layer work has to remove, so the
natural time to replace MCI is *with* that work ([`IDEAS.md`](IDEAS.md) I8) —
the blit target and the mode-set are being rebuilt anyway, the marginal cost
over doing the renderer alone is small, and doing it earlier means integrating
twice.

It is **not** urgent on compatibility grounds: the game launches, reaches the
start screens and plays the intro level on this machine (owner, 2026-09-19), so
the legacy display path works today, mode-set included.

## 7. Open questions

- Does the game ever call `Fmv_Play` with `fullscreen = 0`? Both known call sites
  are in `0x4FCB00` and their argument was not traced. If a windowed path exists,
  it is the cheaper thing to keep.
- What does `0x4FCB00` do around the two calls — is it the startup sequence, and
  is FMV skippable by configuration already?
- `0x6BE1C8` (playing flag), `0x6BE1C4` (saved style), `0x6BE1CC` (FMV
  DirectDraw) look like one small contiguous state block. Worth checking against
  the global-block model in [`SHARED_SOURCE.md`](SHARED_SOURCE.md), though the
  PSX has no counterpart — this code is porting-house work, not Capcom's.

## 8. As built (2026-11-07, DIV-0205)

What was built, against sections 3..7.

**The shape.** `Fmv_Play` (`src/game/fmv_play.cpp`) reads the file whole -
the name, then the CD root and the name, the original's two opens - and
hands the bytes to `video::Movie` (`src/video/movie.cpp`), which parses the
AVI (`avi_reader.cpp`) and picks the decoder by the stream's handler:
`cinepak.cpp` for `cvid` (`capcom.avi`, DIV-0001's re-encode and DIV-0099's
disc conversion alike), `msvideo1.cpp` for `MSVC` / `CRAM` (`logos.avi`).
Each decoder's header carries the format as implemented; both were written
from the public descriptions (the MultimediaWiki's pages and the vendors'
notes), nothing vendored, nothing copyleft (section 5's table was for codecs
that would have needed a re-encode; the shipped files' own formats are from
1991-92 and read in about 450 lines together).

**The contract (section 3), item by item:**
- *Blocking and modal:* the loop is the same `PeekMessage` / pad poll /
  `MsgWaitForMultipleObjects` loop as DIV-0066's, with the wait cut to the
  time until the next frame is due. It returns when the movie ends or is
  skipped.
- *No compositing with the renderer:* none - and none possible: both movies
  play before `Display_Setup` makes the Direct3D 11 device. The frames go to
  the window's client by `StretchDIBits` (GDI, as MCI's own draw was), into
  DIV-0035's rectangle: a whole multiple copies pixels (`COLORONCOLOR`), a
  4:3 fit averages them (`HALFTONE`).
- *Fixed 640 x 480:* the file's own size is used; both are 640 x 480.
- *Skip on key or click,* and the pad (DIV-0066): `Fmv_WndProc`'s cases are
  unchanged. *Resume on re-activation:* `WM_ACTIVATEAPP` pauses and resumes
  our clock and the audio stream in place of `pause vfw` / `resume vfw`,
  under DIV-0049's rule (nothing in DIV-0033's background mode). `WM_PAINT`
  redraws the current frame; MCI's window did that itself.
- *Leave the window as you found it:* the subclass is removed at the end;
  nothing else is touched (DIV-0035 already made no mode-set).
- *Failure is non-fatal:* a file that cannot be opened, read or parsed, or
  whose codec is neither, logs once per name and returns - `LogNotOpened`,
  the disc-only install's case. A decode fault mid-movie holds the last good
  frame to the end, logged.
- *The two call sites* (`Game_WinMain`, `src/game/win_main.cpp`) are
  untouched; `WM_QUIT` is still put back for WinMain's loop.

**Pacing and sound (section 4, items 2 and 3).** The frame due is
`floor(elapsed * dwRate / dwScale)` on `QueryPerformanceCounter`, every
frame up to it decoded (each is coded over the last), only the due one
drawn; an empty chunk (`logos.avi`'s 60 of 120) repeats the frame. The PCM
stream, when the file has one, is put whole into an SDL3 audio stream opened
for the movie (DIV-0201's library, already linked on both widths; the
game's own sound output, DirectSound or SDL, does not exist yet) and started
with the clock; the device plays it at its own pace, 7.5 s with no
correction, and the stream is destroyed at the end. The movie ends when the
longer of the two streams has run. The WAV dump (`BOF3X_WAV_DIR`) does not
hear it: it is not the game's mix.

**No re-encode (item 4).** The files play as they are; DIV-0001 stands (its
"still ships with Windows" amended), DIV-0099's conversion target is still
Cinepak, and the importer is unchanged.

**The mode-set (item 5, section 6):** gone since DIV-0035; nothing to do.

**Section 7's questions.** `fullscreen` is `Cfg_Fullscreen` at both calls
(`win_main.cpp`), ignored since DIV-0035. `Game_WinMain` plays the two back
to back right after the window is shown, before the display set-up; nothing
in the configuration skips them. The state block (`0x6BE1C4..CC`) is
Capcom's PC code with no PSX counterpart; ours uses `Fmv_Playing` alone and
keeps the rest untouched.

**Verification (2026-11-07).**
- The decoders against ffmpeg 9.0.1's decode of the same files outside the
  tree, frame by frame (`python tools/fmv_compare.py MOVIE.avi`, which runs
  `build/bof3x-fmvdump.exe` and ffmpeg with `-fps_mode passthrough`, rgb24
  for Cinepak and rgb555le for 16-bit Video 1):

  | File | Frames | Result | Audio |
  |---|---|---|---|
  | `capcom.avi` (DIV-0001, 15/s) | 112 | 112 bit-exact | 664,440 bytes identical |
  | the disc-built `CAPCOM.AVI` (DIV-0099, 30/s) | 231 | 231 bit-exact | 1,345,344 bytes identical |
  | `logos.avi` (14.984/s) | 120 (60 coded) | 60 bit-exact, 60 repeats | none |

  The first Cinepak build differed by one in the green channel of about 5 %
  of the pixels of 110 frames (PSNR 53..64 dB): the chroma halved by an
  arithmetic shift where the reference truncates toward zero. A screen grab
  would have passed it; the decode-level check is the one to keep.
- The gates, 2026-11-07, root `C:\Users\kerok\bof3x-runs\fmv1107` (`run_gates2.sh`
  and its `.out`, `proof\`): both widths build; the exe host's self-test
  exits 0 on both; the hosted suite with `BOF3X_SHADOW='*'` exits 0 (17
  minutes); `whelpBoss.txt` to `end: done` on the 64-bit exe host and on the
  hosted 32-bit build, both movies opened and decoded on each (the log's
  `DIV-0205` lines; the recorded route's pad words and the owner's own pad,
  live at the time, skipped them early by DIV-0066, as a player's press
  would); a launch of the 64-bit build outside the harness played
  `capcom.avi` to a screen grab of the logo assembling at 3 s and 6 s
  (`proof\x64_capcom_3s.png`, `_6s`; game-derived, not in the tree). The
  state hash of `whelpBoss` on the hosted 32-bit build under the `orig4`
  conditions against the `orig4` pair: the same eleven signature pages the
  64-bit round's accepted check of the same route shows (`x641101\proof\
  check_whelpBoss_x64.txt`: `0x5FD000` from tick 0, `0x929000`, `0x92A000`,
  `0x905000`, `0x7E0000`, `0x803000`, `0x903000`, `0x904000`, `0x7CA000`,
  `0x7C9000`, `0x6BD000`) plus `0x7DE000` (`Snd_BufferDesc`, `Snd_Device`,
  the sound layer's heap pointers - HANDOFF item 15's skip-list candidate),
  which the same build run with `BOF3X_ORIGINAL=Fmv_Play` (Capcom's MCI
  player, the A/B) also shows against the pair, on other ticks. That A/B,
  ours against MCI on one build (`statehash.py diff`): 10 pages, `0x7DE000`
  from tick 883 and, for 16..21 ticks around 9089..9162, five
  `Gfx_VramShadow` pages and `0x937000` - the same pages the MCI run and the
  64-bit round's 32-bit run show against the references and ours does not,
  so the run-to-run class, not the decoder's. Nothing a movie touches
  differs after the movies.
- **Not yet:** the owner's eye and ear on either width; a pause and resume
  under `background=0`; a client that is not a whole multiple (the HALFTONE
  path), seen only in the decoder's own output.
