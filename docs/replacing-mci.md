# Replacing MCI with a bundled decoder

**Status:** DRAFT — design note, nothing built (verified 2026-09-19)

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
