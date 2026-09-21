# Known defects of the port, as observed

**Status:** IN PROGRESS (2026-09-19 — five entries; D4 fixed by DIV-0004 and confirmed in game)

Things the 2001 port does wrong on a current machine, written down when seen so
that "we broke this" and "it shipped like this" stay distinguishable
([`DIVERGENCE.md`](DIVERGENCE.md)). An entry here is an observation, not a
decision to fix: a fix is a divergence and gets a ledger entry of its own.

Each entry says **who saw it, in which configuration**, and what has been
established about the cause. "Configuration unknown" means the A/B run with
`BOF3X_ORIGINAL=*` has not been made, so the defect is not yet proven to be
Capcom's rather than ours.

## D1 — Stat numerals lose their bottom rows on the equipment screen

**Seen:** owner, 2026-09-19, windowed 640x480, equipment screen: the large
attack / defence / intelligence / speed numerals ("96", "59", "71") are cut off
two or so pixels short at the bottom. The smaller HP/AP numerals and the clock
on the party screen look intact in the owner's second screenshot (agent's
reading of the image, not confirmed by the owner). Chinese text is unaffected.
**Configuration unknown.**

**Established:** the glyphs are intact in memory. A read-only read of the
1 MiB VRAM shadow `0x6C9F44` from the owner's running game, decoded as 4 bpp,
shows both digit sets complete — bottoms included — in the font page at the
top right of VRAM (shadow x words 960..1023, y 0..~70). So loading is not at
fault (consistent with `LoadDatFile` and `Gfx_LoadImage` comparing
byte-identical, [`asset-loading-path.md`](asset-loading-path.md)); **the
clipping happens at draw time.**

**Cause read, 2026-09-20 - every sprite's far texture edge is one texel
short.** The owner's menu screenshots show it on every menu numeral, not only
Equipment. The menu's numerals are `0x517090` (the 8x8 font: `SPRT_8`, u =
`((c - 0x20) % 32) * 8`, v likewise `/ 32`, page (960, 0)) and `0x516F60` (the
12x12 font, 21 to a row, same page). In the live VRAM shadow the 8 px digits
fill rows 1-7 of their cell and the 12 px ones rows 1-11: the last row of the
sprite is the glyph's bottom stroke. The D3D sprite handlers - `0x5A2300`
(`SPRT`), `0x5A2520` (`SPRT_8`), `0x5A2710` (`SPRT_16`), reached through the
draw `0x59EE50`'s second jump table `0x59F3D8` - take texture coordinates from
a float table at `0x7CA9E0`, read live as `tc[i] = (i + 0.512) / 256`. The
near edge is `tc[u]`; the far edge is **`tc[u + 7]`** (table base `0x7CA9FC`),
`tc[u + 15]` (`0x7CAA1C`) and `tc[u + w - 1]` (`0x7CA9DC`), while the quad is
the full 8, 16 or w pixels wide (`+ 8.0` from `0x5C41CC`, times the scale
`0x7C9F4C` / `0x7C9F48`, both 2.0 here). So w - 1 texels are stretched over w
pixels: at 2x the last texel row gets one screen row of sixteen instead of
two, and the rows before it are unevenly doubled. Glyphs that touch the bottom
of their cell show it; everything drawn as a sprite has it.

**Correction to the paragraph above, same day:** the port filters
bilinearly, and the inset is what keeps a cell's neighbours out - the values
are right and the slip is that the far value is reached one pixel past the
last one drawn. **Fixed as DIV-0010** ([`DIVERGENCE.md`](DIVERGENCE.md)); the
owner has not yet looked at the menu numerals with it.

**Not established:** the same for the `POLY_FT4` handlers, which use the same
table; the A/B run.

## D2 — The window title is mojibake on a non-Chinese system locale

**Seen:** owner's screenshots, 2026-09-19. The title is GBK bytes passed to
`CreateWindowExA`, so it is decoded in the system ANSI code page. Original
behaviour by construction — our code does not touch window creation.

## D3 — Fullscreen fallback and resolution handling

Carried from [`STATUS.md`](STATUS.md) step 0: reported, **not yet reproduced
or recorded.** The exclusive-fullscreen `SetDisplayMode(640, 480, 16)` for FMV
does work on this machine.

## D4 — Crash: the image-unpack buffer overflows into the draw structures

**Seen:** owner, 2026-09-19 14:55, all ten functions ours, windowed. Playing the
converted US save (area 141, solo Ryu): a battle, a save to slot 5, then the
crash. Last lines of `bof3x.log`: `Save_WriteFile(BISLPS05.DAT)`, then
`DAT\PL478.DAT` opened. **The owner was standing still** (2026-09-19): "nothing
should have been reloading except the audio and any idle animations".
**Configuration: ours. Not reproduced, not yet tried with `BOF3X_ORIGINAL=*`.**

**Established, from the Windows event log and the WER dump**
(`%LOCALAPPDATA%\CrashDumps\BOF3.exe.29616.dmp`, local, game-derived — never
commit it):

- Access violation reading `0x00080000` at **`0x59F24F`**, `mov ecx, [esi]` in
  the draw `0x59EE50`'s list walk, called from WinMain's rendered-frame branch
  (return address `0x4FCE74` on the stack). The walk masks each link to 24
  bits (`0x59F249`) and stops at -1.
- The list head it was given, `0x90399C` = draw structure `0x903910` + `0x8C`,
  holds `0x08080000`, and **everything from `0x8E4580` to about `0x904E00` is
  one unbroken run of bytes below `0x20`** — both draw structures
  (`0x903880`, `0x903910`), the upload queue's own arrays (`0x903680`) and
  `DamageScratch` included.
- `0x8E4580` is the base of a bump-allocated scratch buffer: `0x4FD230`, called
  once per logic frame from WinMain, resets the pointer `[0x929EDC]` to it, and
  `0x461FC0` / `0x462070` — the unpackers behind `Gfx_FlushUploadQueue`
  ([`call-trace.md`](call-trace.md) §6) — write bytes of `value & 0x1F` there
  and advance the pointer. In the dump the pointer stands at **`0x904D80`,
  133,120 bytes past the base**, and the upload queue is empty: the flush had
  just run, and the draw that follows it in the same branch read the wreckage.

So: **a fixed-size scratch buffer in Capcom's code, overrun by one flush of
queued image uploads.** Nothing of ours is on that path — but `LoadDatFile`,
which is ours, loaded the file whose sprites were presumably being uploaded,
so the A/B run is still owed before calling it original.

**Mechanism, observed live 2026-09-19** (read-only sampler on the owner's
running game, same spot, ten functions ours; `analysis/crash/focus_watch2.txt`).
The owner recalled switching between this window and the game before the
crash, and repeated it:

| state | upload queue count `0x9035A0` | scratch pointer past `0x8E4580` |
|---|---|---|
| standing still, focused, 20 s + 4 min | never above 1 | at most `0x4000` |
| refocus after 71 s unfocused | 2, 3, 4, 5, 6 within 10 ms, then 0 | `0xA800` |
| refocus after 96 s unfocused | 7 .. 12 within 10 ms, then 0 | `0x13800` |

The game freezes while unfocused and replays the missed logic frames
unrendered on refocus ([`windowed-mode.md`](windowed-mode.md)). Each replayed
frame may queue an upload; nothing drains the queue until the next *rendered*
frame; then one flush unpacks all of it into the scratch buffer. **The enqueue
has no bound**: at `0x416072` it is `queue[count] = ...; count++` with no
comparison (the sites in `0x4A29C0` and `0x5894D0` are unread). Growth here was
about one entry and 6-7 KB per 8 s away, so the ~127 KB to the first globals
above the buffer — and the 20 entries the x-array `0x903680` has before it runs
into the y-array `0x9036A8` — are both reached after roughly **two and a half
minutes unfocused**, at this spot.

**Reproduced 2026-09-19 17:06**, same session, after the owner left the game
unfocused for several minutes (`analysis/crash/drag_watch.txt` caught the
refocus): within 50 ms the queue count ran **1 to 39** — past the 20 slots of
the x-array — the flush then drove the scratch pointer to **`0x35800`
(219 KB) past its base**, the count byte read 31 mid-flush (it lies inside the
overrun, at base + `0x1F020`, so the flush overwrote its own loop bound), and
the process was gone 2.4 s later. The crash reporter's first real catch:
access violation at **`0x59F083`, the same draw `0x59EE50`, same caller
`0x4FCE74`, same list head `0x90399C`**, this time holding 0, read at `[0+7]`;
the area word in the report reads `0x0C14`, i.e. overwritten too. Dump:
`build/bof3x.crash-9304-0.dmp`. Prediction and crash agree; the mechanism is
established. Still owed: the same with `BOF3X_ORIGINAL=*`.

**The window-drag variant, predicted and then confirmed 2026-09-19**
(`analysis/crash/drag_watch2.txt`). With the game *focused throughout*, the
owner held its title bar for 57 s: on release the queue count ran 2 to 6 and
the scratch pointer reached `0xA800` — the same figures as 71 s unfocused.
Windows runs a modal loop while a title bar is held, WinMain's loop does not
run, and the stale deadline is caught up unrendered on release exactly as
after focus loss. So **keeping the game running while unfocused would remove
the focus trigger but not this one**; a drag or resize of two and a half
minutes should crash the same way (not taken that far).

The owner also heard the **music skip and repeat during the drag**, where
losing focus gives silence. That fits the code: the window procedure's
`WM_ACTIVATEAPP` branch (`0x4FC72B`) pauses audio through `0x587C30` when the
app deactivates, but a drag deactivates nothing — the stream simply stops
being refilled, because the pump `0x587C70` is called from the blocked loop
([`call-trace.md`](call-trace.md) §4), and DirectSound replays what is left in
the buffer. Not verified beyond that reading.

**Not established:** the buffer's intended size (the next known global above
it is the queue count byte `0x9035A0`, 127,008 bytes up); whether one frame's
uploads were simply too large, or several logic frames' worth piled up while
rendering was being skipped (the pointer resets per *logic* frame, the queue
drains per *rendered* frame — [`call-trace.md`](call-trace.md) §6); and
whether the PSX original has the same limit.

**The enqueue sites, all read 2026-09-19.** Every writer of the count byte
(`pe_xref.py 0x9035A0`, 15 references in five functions):

- **`0x5894D0`** is the general one: for the sprite object at `[0x937F88]` it
  queues x = texture slot `[obj+0x25]` x 64 (slots above `0x10` wrap to the
  lower half: x - `0x400`, y + `0x100`), y = `[obj+0x26]`, and a record pointer
  from the sprite's frame table, then `count++`. **No bound.** Reached through
  `0x5891F0` (set animation frame — 216 calls in the attract trace, a dozen
  static callers) -> `0x589200`.
- `0x416020` queues one special image and also has no bound. Its record
  pointer is the scratch base `0x8E4580` itself: it builds the record *in* the
  scratch buffer, so the buffer is not purely transient.
- `0x4A29C0` does not add: after calling `0x5891F0` it decrements the count
  and zeroes the slot — "set the frame, cancel the upload".
- `LoadDatFile` zeroes the count for one kind of load (`0x454634`), and
  `Gfx_FlushUploadQueue` zeroes it after draining.

So a bound or an early drain placed in `0x5894D0` covers the pile-up seen
here; `0x416020` is one entry and rare.

**Next:** the `BOF3X_ORIGINAL=*` run (the pause and deadline logic is read —
D5, and above); then choose the fix with the owner — keep running while unfocused
(owner's proposal, 2026-09-19), bound or drain the queue, clamp the catch-up;
each is a ledger entry. Older next steps: try to reproduce from slot 5, then
with `BOF3X_ORIGINAL=*`; read who fills the queue and what bounds it.

**Fix: DIV-0004, 2026-09-19** ([`DIVERGENCE.md`](DIVERGENCE.md)) — our
`Gfx_BeginFrame` drains a non-empty queue at the top of every logic frame.
The all-original reproduction was made first (owner, `BOF3X_ORIGINAL=*`: log
says `0 ours, 10 left original`, crash at `0x59F24F` reading `0x00080000`,
caller `0x4FCE74`; dump `build/bof3x.crash-35980-0.dmp`). Runs since the fix:

- Attract oracle, eleven functions ours: identical to `orig_a.tsv` at all
  7,478 frames; no `DIV-0004` line (the drain never fires when every frame
  renders) and no `CRASH` line.
- **The reproduction no longer crashes** (owner, same save and spot, eleven
  functions ours; `analysis/crash/fix_watch.txt`): **665 s unfocused** — more
  than four times what killed it before — then refocus. The game carried on;
  the sampler never saw the queue above 1 (before the fix: 39) or the scratch
  pointer move far; the log has `DIV-0004: 1 uploads left by an unrendered
  frame, draining` and no `CRASH` line.
- **The title-bar variant, under the fix** (owner, same session;
  `analysis/crash/fix_drag_watch.txt`): the window held and nudged for about
  78 s with the game focused; queue never above 1, scratch at most `0x2800`.
  Before the fix 57 s gave queue 6 and `0xA800`.
- **Frame-hash A/B** ([`call-trace.md`](call-trace.md) §7), `ab2_orig` against
  `ab2_ours`, 2,834 entries armed and the eleven owned functions unarmed:
  calls and hash identical on all 4,484 frames. The tracer slows the game
  enough to skip rendering, so the drain *did* fire in the ours run
  (`DIV-0004` in its log) — and every logic call still matched.

## D5 — Game speed depends on how long Windows has been up

**Found:** reading WinMain's pacing for D4, 2026-09-19. Not something anyone
saw on screen — but it explains a number already on record.

**Established, from the code.** The frame deadline at `0x6BC628` is a
**32-bit float** holding a `GetTickCount` value in milliseconds: written by
`fstp dword` at `0x4FCDC4` (start: now + 33.34, the double at `0x5C4220`) and
advanced once per logic frame at `0x4FCF0F`..`0x4FCF1B` by the double at
`0x5C4218`, **33.334**, again stored with `fst dword`. (The float at
`0x5C4214` is 4294967296.0, subtracted when the deadline passes it — the
49.7-day tick wrap was thought of.) A float has 24 bits of mantissa, so once
the tick count passes 2^24 ms — **4.7 hours of uptime** — it can no longer
hold every millisecond, and the spacing doubles with each doubling of uptime.

**Measured.** On this machine today `GetTickCount` is about 384,566,000
(4.45 days up), where adjacent floats are **32 ms** apart. Adding 33.334 then
rounds to +32 every time: 300 frames advance the deadline 9,600 ms, which is
**31.25 frames per second** — exactly the "31.25 per second by the external
count" that [`attract-mode.md`](attract-mode.md) §5 recorded from a 25-minute
run and left unexplained.

**Predicted from the same arithmetic, not observed:** between 6.2 and 12.4
days of uptime the spacing is 64 ms and +33.334 rounds up to +64 — about
15.6 fps, half speed; beyond 12.4 days the spacing is 128 ms and +33.334
rounds to nothing, so the deadline stops advancing, the loop is always
"late", and the rendered-frame branch (taken only when now < deadline) is
never taken — no drawing, and D4's queue never drained. Below 4.7 hours it is
exact. A fresh boot is the cheap way to check the low end.

**Observed, 2026-09-21: the half-speed band.** Every recorded attract run
on this machine (`analysis/attract/*.runlog`, `done:` lines) ran at 29.4-29.6
logic frames a second through 2026-09-20 13:41, and at 14.2-15.2 from the
first run of 2026-09-21 (12:12) on - original and ours alike, traced or not.
`GetTickCount` read 557,794,953 (6.456 days) that afternoon, so it passed
2^29 ms (6.2 days) at about 11:27 - between the two. The prediction above
was 15.6; the measured 15.2 is that less the loop's own overhead, as 29.6
was of 31.25. Frame hashes and oracles stayed identical throughout, as they
must: only wall-clock speed changed. For a day this was written up as an
unexplained regression of ours ([`HANDOFF.md`](HANDOFF.md)).

**"Uptime" is not how long the PC has been on.** The owner switches the PC
off every night and on every morning - and the clock still read 6.46 days.
Windows' **Fast Startup** (on by default since Windows 8; here
`HiberbootEnabled = 1`, and the System log's Kernel-Boot event 27 says "boot
type 0x1" every morning) makes Shut down hibernate the kernel session, so
power-on resumes it and `GetTickCount`, which counts time spent in
hibernation too, carries on from the last *full* boot (2026-09-15 06:19
here). The unbiased interrupt time, which leaves hibernation out, read 4.72
days. So an ordinary player who shuts down every night reaches the
half-speed band about a week after their last Restart, and the no-drawing
band about two weeks after - this is a defect players meet, not a corner
case. A **Restart** (or Shift + Shut down) resets the clock.

**Next on this machine,** without a Restart: 2^30 ms at about 2026-09-27
16:35 - from then, by the arithmetic, the deadline stops advancing and
nothing is drawn. The fix is the first item of [`HANDOFF.md`](HANDOFF.md)
"Pick up here".

Original by construction — nothing of ours is in WinMain's loop. Logic is
still a pure function of the frame count, so the oracle is unaffected; only
wall-clock speed changes. A fix (keep the deadline in an integer or a double)
is a divergence and wants a ledger entry.
