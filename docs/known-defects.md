# Known defects of the port, as observed

**Status:** IN PROGRESS (2026-09-25 — eighty-five entries, D1..D88 with D19, D20 and D29 unused; D1 fixed by DIV-0010 (confirmed off a capture 2026-09-21); D2 fixed by DIV-0039; D3 moot since DIV-0031 / DIV-0035 (recurs only under BOF3X_ORIGINAL); D4 fixed by DIV-0004 and confirmed in game; D5 fixed by DIV-0022 and DIV-0047; D6, D7, D9, D11 and D12..D16 latent; D8 and D10 unchecked in game; D17 fixed by DIV-0025 and D26 by DIV-0028 (both confirmed in game 2026-09-23); D18, D21..D25, D27, D28 and D30..D40 latent (D38 a candidate); D41 fixed in the backend by DIV-0044 (the owner's look owed); D42 a port change, kept; D43..D57 latent, from the seventh round (D43 and D51 candidates; D44, D47, D53, D56 PC only); D58 fixed under an overlay language by DIV-0051; D59..D88 latent, from the eighth round's reading, 2026-09-25 (D86 and D87 candidates; D59, D66 and D68 abort in ours where the original would crash))

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
owner has not yet looked at the menu numerals with it. **Since then:** the
owner judged the in-menu A/B capture, 2026-09-21: "numerals look good"
(DIVERGENCE.md DIV-0010, [`input-script.md`](input-script.md) §5).

**Not established:** the same for the `POLY_FT4` handlers, which use the same
table; the A/B run.

## D2 — The window title is mojibake on a non-Chinese system locale

**Seen:** owner's screenshots, 2026-09-19. The title is GBK bytes passed to
`CreateWindowExA`, so it is decoded in the system ANSI code page. Original
behaviour by construction — our code does not touch window creation.
**Since 2026-09-23 that is no longer so:** WinMain is ours (DIV-0032) and the
title is English whatever the locale — **fixed as DIV-0039**.

## D3 — Fullscreen fallback and resolution handling

Carried from [`STATUS.md`](STATUS.md) step 0: reported, **not yet reproduced
or recorded.** The exclusive-fullscreen `SetDisplayMode(640, 480, 16)` for FMV
does work on this machine. **Moot since 2026-09-23:** no exclusive mode is
set anywhere - the display's by DIV-0031, the FMV's by DIV-0035 - and the
set-up's windowed-to-fullscreen fallback is gone with `Display_Setup`. It
can only recur under `BOF3X_ORIGINAL=Display_Setup` or `=Fmv_Play`.

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

**Measured on demand, 2026-09-21** (DIV-0022's `BOF3X_TICK_BASE` starts the
game's clock at any value, so the original pacing code can be put in any
band): steady state after 30 s, **31.25** at 2^28, **15.62** at 2^29 (the real
clock that day), and at 2^30 **91.7** logic frames a second - the spin never
waits, and DIV-0004's drain fired, which it does only after an unrendered
frame. So past 12.4 days the game fast-forwards, and by the code draws
nothing; the screen itself was not looked at.

**Fixed for players, short term: DIV-0022** (2026-09-21). The game's
`GetTickCount` import now counts from the game's start, so the float sees
small numbers again: **30.00** logic frames a second, and the owner confirmed
the speed recovered in game (2026-09-22). One unbroken session
still drifts through the bands (31.25 past 9.3 hours, half speed past 6.2
days); the complete fix, the deadline in a double, is
[`IDEAS.md`](IDEAS.md) I16.

Original by construction — nothing of ours is in WinMain's loop. Logic is
still a pure function of the frame count, so the oracle is unaffected; only
wall-clock speed changes. A fix (keep the deadline in an integer or a double)
is a divergence and wants a ledger entry.

## D6 — An attachment handle with bit 7 names the wrong object (latent)

**Found:** reading `Sprite_ObjectByHandle` `0x57C0A0` for the takeover queue,
2026-09-20; its PSX twin and the shipped data checked 2026-09-21. Nobody has
seen it on screen, and by the data below nobody can: **latent, in Capcom's
code on both platforms, reached by no shipped script.**

**The code.** A movement-script command, `F8 07 h s`, attaches the current
sprite object to another: it stores the byte `h` at the object's `+0x18`
(PC `0x5792A0`, PSX `FUN_801ad768` in `GAME.EMI`). Two readers turn the
handle into an object number through `0x57C0A0`: `0x5192A0`, which puts the
object where the other one is, and `Sprite_InheritDrawKey` `0x589770`, which
gives it the other's draw key. Without bit 7 the handle means extra object
`h & 0x3F` (number `+ 30`). **With bit 7 it is meant as "the n-th object of
type `0x0A`"**, n = `h & 0x3F`: the function walks the 30 objects counting
type-`0x0A` ones and stops at the n-th - and then returns `n`, not the slot it
stopped at (the index is in `dl`, the result in `al`). The PlayStation's
`0x8015BEE4` does the same by a different route: it returns the count of
matches, which equals `n` (sibling's recompiled output, 2026-09-21). So a
bit-7 handle names slot `n`, which is the object meant only while the
type-`0x0A` objects fill the first slots; otherwise the attached object
follows, and draws with, the wrong one.

**Why it never shows.** Every handle is a literal byte in an area's movement
scripts, and on the PC those are compiled into `BOF3.exe`'s `.data` with the
rest of the area section (the `DAT`s drop it, [`DAT_CONTAINER.md`](DAT_CONTAINER.md)
§2). The interpreter (`0x576B50`, PSX `0x801A9D38`) has three callers on both
builds. Two of the PC's read the area descriptor table `0x667590`, indexed
by `Game_AreaNumber`, directly (arrays `+0x10` and `+0x1C`); the third reads
a pointer kept at `+0x130` of a struct, which on the PSX is filled from the
descriptor's `+0x18` array by script opcode `0x86` (the PC's store is
unread). A scan of every byte of `.data` for `F8 07 xx`: 235
triples, **one** with bit 7 - at `0x61A4C3`, inside the block at `0x61A42C`
that area 100's descriptor `+0x08` names (one 8-byte entry, `0x8000002A` and
that pointer). `+0x08` is what a newly spawned object is given
(PSX `FUN_801a78c8` passes entry `[n]` to `0x8015B67C`), and the block is
repeating records of a count and signed 16-bit offsets: the `F8` is the high
byte of one 16-bit field (the record before has `0x079F` in the same place),
not a command. Nothing else in the exe points into the block.
The `DAT` hits are audio, images, geometry and the event-script block, none
of which the interpreter reads. The other triples carry handles `0x00` 137
times, `0x01` 58, `0x02` 28, `0x03` 7 - the four extra objects - and four
more are the pointers `0x5E07F8`, `0x6207F8`, `0x6407F8`, `0x6507F8` in
pointer tables (`python tools/movement_scan.py --all`). [`movement-script.md`](movement-script.md) has the method.

**The exact list, 2026-09-22.** With the op-length table found
(`MoveScript_OpLengths` `0x6639FC`, byte for byte the PSX's), `python
tools/movement_scan.py --decode` follows every script's control flow from
its start: 3,633 starts in 171 areas, now including the descriptor's `+0x18`
array, which op `86` hands to the third caller. It reaches **230
attachments - handle `00` x137, `01` x58, `02` x28, `03` x7 - and none with
bit 7**; they are exactly the raw scan's real-handle triples, and no path
reaches the bit-7 byte. ([`movement-script.md`](movement-script.md) §4.)

**Left open, and narrowed:** ten array entries in four areas (4, 9, 16, 17)
point into `0x6758E0..0x675960`, which is zero in the file. A read-only watch
through three visits to area 4 in the attract cycle, 2026-09-22, saw it stay
zero: placeholders, not scripts written later (a zero script cannot run; the
flow pass loops on `00`). Areas 9, 16 and 17 are not in the attract cycle
and have not been watched.

**Kept as Capcom had it** by the takeover (`src/game/sprite_find.cpp`,
2026-09-21). Returning the slot found instead would be a divergence; the
start-up fuzz shows it is observable (1,308 of 16,384 calls differ), but no
shipped data would ever show it, so there is nothing to decide unless new
content uses bit-7 handles.

Both readers of the handle are ours since 2026-09-22 - `Field_ObjectFollow`
`0x5192A0` and `Sprite_InheritDrawKey` `0x589770` - and both faithful.

Op `84` is a second reader of handles (2026-09-22): `MoveCmd_HandlePosition`
`0x578DC0` tests bit 7 and counts type-`0x0A` objects the same way. No
shipped script uses op `84` (`movement_scan.py --decode`), so it adds nothing
to D6 today; new content using it would meet the same search.

## D7 — The movement script's length table says `C1` is 5 bytes; it is 4 (latent)

**Found:** reading the `0xC0` group for the takeover, 2026-09-22. Nobody has
seen it; by the data below, nobody can in the shipped game. **Latent, in
Capcom's data on both platforms.**

**The defect.** `MoveScript_FindLabel` `0x579450` finds a label by walking
the script from its start, stepping over each op by its length in
`MoveScript_OpLengths` `0x6639FC`. That table gives `C1` 5 bytes. The op's
handler (`MoveScript_GroupC`, PC `0x578010`; PSX `FUN_801abb00`, whose `C1`
and `C2` share the same `+3`) advances by 3 and the step by 1: `C1` is 4
bytes as executed, as `C2` is - the table has `C2` right. The table is the
PSX's byte for byte ([`movement-script.md`](movement-script.md) §4), so the
error is Capcom's. A label search that crosses a `C1` lands one byte into
the op after it, and from there reads the script out of step - it may find
the wrong `0A`, none, or run through padding and never return.

**Evidence that 4 is what the scripts mean.** `python
tools/movement_scan.py --decode`, which follows control flow, reaches 106
`C1`s. Stepping them by 5 left 90 paths running into a length-0 byte and 137
ops past the next script's start; stepping them by 4 leaves 31 and 36 - the
29 paths that ran on from an op `20` were all out of step after a `C1`.

**Why it never shows.** The decoder resolves every label twice, with the
table's length and with the executed one: **no shipped label search crosses
a `C1` and ends differently.** Kept as Capcom had it by the takeover
(`src/game/move_groups.cpp`): the label search is not ours yet, and fixing
the table would be a divergence with nothing in the shipped data to show it.
New content with a `C1` before a label would meet it.

## D8 — The scenario fade swaps red and blue (unchecked in game)

**Found:** reading `ClutStrip_FadeTo` `0x56C0A0` for the takeover of the
attract demo's scenario, 2026-09-22 ([`field-modes.md`](field-modes.md) §7).
**Configuration: Capcom's on both platforms** — the PSX twin shifts the same
way. Nobody has looked at it in game yet.

**The defect.** The fade builds each of 16 colours by taking the source's
channels from bit 0 upward and shifting each one in from the bottom, so the
source's bits 0-4 (red) land in 10-14 (blue) and 10-14 land in 0-4.
`ClutStrip_Restore` `0x56C110` copies straight back. Unless the sixteen
colours happen to be grey (R = B), whatever draws with that CLUT is red/blue
swapped for the length of the fade and snaps back when it ends.

**What is not established:** which sixteen colours these are in the demo, what
draws with them, and whether the swap is visible at the speed the fade runs.
Captures of scene 3's and scene 4's fades would say. The fuzz would catch a
change here (control C31), so ours fades exactly as Capcom's does.

## D9 — The event script's switch reads its condition index signed and unmasked (latent)

**Found:** reading `EventScript_Switch` `0x579B00` for the takeover,
2026-09-22 ([`event-script.md`](event-script.md) §5). **Latent, Capcom's on
both platforms.**

**The defect.** The `if` ops mask the condition index to five bits before
indexing `EventScript_Conditions` `0x663B30` (17 entries). The switch does not
mask it at all, and reads it signed. Indices 17..31 run off the end into
`MoveScript_CounterOps` `0x663B74`, then the `E9` handlers, then data words —
called with the script position where those expect other arguments. A negative
index reads whatever lies before the table.

**Why it never shows:** `tools/event_scan.py` decodes all 200 shipped
placement scripts, and none asks for an index outside 0..16. New content
would meet it. Kept as Capcom had it.

## D10 — The zone counter's arithmetic is 8-bit (candidate)

**Found:** reading `Field_ZoneCounterRoll` `0x52FEB0` for the takeover,
2026-09-22 ([`field-event.md`](field-event.md) §4). **Capcom's**; unverified
in game, and what the counter counts is itself a reading, not a measurement.

**The defect.** The counter is a byte: `base + min(Rand() & 0x1F, the zone's
cap)`, then doubled for one accessory and halved for another, all in 8 bits.
Zone 1's base is 120, so a roll of 31 gives 151, and the doubling accessory
turns that into 46 rather than 302 — the opposite of what doubling should do.

**What is not established:** that the counter is the steps to the next random
encounter. Its callers are the field's step code and the two accessories
double and halve it, which is what that reading rests on. A player with the
doubling accessory in a high-rate zone is exactly the case that would meet it,
so this one is worth an in-game check before it is called a defect.

## D11 — A large tint on a bright component gives black, not white (latent)

**Found:** reading `MoveScript_TintFrame` `0x454AD0` for the takeover,
2026-09-22 ([`frame-callees.md`](frame-callees.md) §4). **Latent, Capcom's on
both platforms.**

**The defect.** A tint record recolours a CLUT every frame by adding a signed
offset to each 5-bit component **in 8 bits**, then clamping: below zero to 0,
above 31 to 31. A component of 31 with a tint of `0x61` or more sums past 127,
which as a signed byte is negative — so it clamps to 0 (black) where the
arithmetic means 31 (white).

**Why it never shows:** the ops that step a tint (`C1` / `C2`) and the field
fades move the three tints toward 0 or 31 in small steps, so no shipped script
reaches an offset that large. Kept: the fuzz seeds `0x60`, `0x61`, `0x7F`,
`0x80` and `0x81` exactly to hold the wrap in place (controls 28, 29, 45).

## D12 — The kind-2 glide divides by zero within half a unit of its target (latent)

**Found:** reading `Kind2_Script` `0x573080` for the takeover, 2026-09-22
([`kind2-object.md`](kind2-object.md) §5, K1). **Latent, Capcom's on both
platforms.**

**The defect.** The glide state counts its frames as the signed high word of
twice the distance and divides the distance by them. A distance of
1..`0x7FFF` — under half a unit on the larger axis — makes the count 0, and
the second `idiv` faults. A speed index whose speed byte is 0 — index 0, and
6 and 7 past `Field_MoveSpeeds`' six bytes — faults the first. The PSX has the
same arithmetic with its compiled-in `break` traps (`trap(0x1c00)` in the
decompilation), so it is the source's, not the port's.

**Why it never shows:** whether any shipped kind-2 script glides from within
half a unit, or sets such a speed, was not measured. Ours faults at the same
instruction of the same computation (`Idiv`, an inline `idiv`, not a C++
division); the fuzz seeds the faulting inputs on neither side. A fix is the
owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D13 — A tint record whose CLUT row is 32 or more writes past the CLUT strip (latent)

**Found:** reading `MoveScript_TintFrame` `0x454AD0` for the takeover,
2026-09-22 ([`frame-callees.md`](frame-callees.md) §5, §7). **Latent,
Capcom's.**

**The defect.** The tint copies a CLUT's words from
`Gfx_ClutStrip[(clut / per-row) << 8 + column]` to the target's, and the row
— the quotient — is a whole byte. The strip is 32 rows of 256 words
(`0x2000`, VRAM rows 480..511, inside the DAT arena). A CLUT number whose row
is 32 or more — 32 times the depth's per-row count and up; row 255 at depth
1 — reads and writes up to word `0x10FEE`, `0x1DFE0` bytes of the arena after
the strip, on the original as on ours. On the PSX the same index named a VRAM
row past the strip's; what it did there was not read.

**Why it never shows:** the CLUT numbers the shipped tints and fades use were
not measured. The fuzz seeds the whole reachable span (`0x10FF0` words) and
restores it, on both sides, so ours matches the original there.

## D14 — An attachment's move over 128 frames or more goes the wrong way (latent)

**Found:** reading `MoveCmd_AttachMove` `0x5793B0` for the takeover,
2026-09-22 ([`move-cmds.md`](move-cmds.md) §2, control M1). **Latent,
Capcom's on both platforms.** (Group L of the third round.)

**The defect.** Op `F8 07 h s t` with the context's `+4` set moves the sprite
to where the attachment puts it over `t` frames (0 taken as 1): the velocity
is the distance divided by `t` - but `t` is sign-extended (`movsx`; the PSX
passes `(int)(char)t` to `FUN_801AD0A0`), while `+9`, the frame count the
motion runs for, gets the byte unsigned. So a `t` of `0x80..0xFF` gives a
velocity pointing away from the target, run for 128..255 frames; `0xFF`
divides by -1, which faults on a distance of exactly `0x80000000`.

**Why it never shows:** the 230 attachments the shipped scripts reach
(`tools/movement_scan.py --decode`) use `t` of 0, 1, 4, 8, 16 and 32 only
(counted 2026-09-22). Ours divides the same way; the fuzz seeds `0x7F`,
`0x80`, `0x81`, `0xFE` and `0xFF`. A fix is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D15 — The attachment's move and its follow read a handle differently (latent)

**Found:** reading `MoveCmd_HandlePosition` `0x578DC0` for the takeover,
2026-09-22 ([`move-cmds.md`](move-cmds.md) §2). **Latent, Capcom's on both
platforms.** (Group L of the third round.)

**The defect.** An attached object first moves to its handle's object
(`MoveCmd_AttachMove` through `MoveCmd_HandlePosition`), then follows it
every frame (`Field_ObjectFollow` through `Sprite_ObjectByHandle`). Without
bit 7 the move takes the extra object `handle` - all seven bits - and the
follow takes `handle & 0x3F`: they disagree for `0x40..0x7F` (both are past
the four extra objects there anyway). With bit 7, when the type-`0x0A` object
asked for does not exist, the move goes to `Sprite_Objects[count of type-0x0A
objects]` and the follow to `Sprite_Objects[handle & 0x3F]`. When it does
exist both name slot `handle & 0x3F` - D6. The PSX's `FUN_801ACF30` is the
same, uninitialised fourth dword of its result included.

**Why it never shows:** the shipped attachments use handles 0..3 only
(D6's count), where the two agree.
## D16 — The camera's swing back ends only on one exact distance (latent)

**Found:** reading `Transition_Kind11` `0x4952D0` and `Transition_Kind18`
`0x4954B0` for the takeover, 2026-09-22 ([`mode-flow.md`](mode-flow.md) §2,
§7). **Latent, Capcom's** (group K of the third round).

**The defect.** Transition kinds 11 and 18 turn the camera back and add 50 to
`Camera_Distance` every frame until it **equals** `0x5DC`, compared as 16
bits, and only then end the transition task. Their partners, kinds 10 and 17,
take off 50 on each frame their fade is not yet done (31 frames for kind 10, 63 for kind 17), so a
swing away followed by its swing back lands on `0x5DC` again. Entered with any
other distance, the loop circles: an even difference from `0x5DC` comes round
after up to 32,768 frames (18 minutes at 30 a second), an odd one never, and
the transition's wait word holds whatever waits on it meanwhile. Ours loops
the same way (the fuzz's controls K23 and K25 pin the test and its place).

**Why it never shows:** which scripts start kinds 10, 11, 17 and 18, and with
what distance, was not measured; the attract sequence starts only kinds 0 and
1. Kind 12 puts the distance at `0x5DC` outright, which suggests the
designers kept it there. A fix is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D17 — Glyphs sample on texel boundaries: "wobbly" text under point, soft under bilinear

**Seen:** owner, 2026-09-22, in play with `BOF3X_LANG=en`, `filter=point`,
windowed, renderer 1 (`build/bof3x.ini`): Mogu's "OK, explosives are set!"
against the same line in the sibling's recompiled PSX build - ours "wobbly",
the recomp's even. **Capcom's, by reading** (below); the A/B with
`BOF3X_ORIGINAL=*` has not been made, but nothing of ours is on this path. It
is not English-only: every glyph the port draws goes through it, Chinese
included.

**Measured on the owner's screenshot.** The font texture is clean:
`tools/font_pc.py`'s `glyph_pixels` on `en.FIRST.DAT` gives `O K e x p l`
as exact 2 x 2 blocks, every texel doubled. On screen the same strokes come
out one, two or three pixels wide - the top of the `O` three rows tall, its
left stroke three columns - and the `l` of "explosives", one column of the
texture from top to bottom, narrows from two pixels to one at glyph row 16,
which is where the quad's diagonal crosses that column: the two triangles of
the quad round differently. Our own English captures under the default
bilinear filter (`analysis/shots/ab24_attract_en_ours/a07.png`) show the same
fault as every stroke pixel paired with a half-bright one.

**Cause, read 2026-09-22.** `Text_EmitGlyph` builds glyphs as primitive code
`0x6C` (`Gpu_SetCode6C`); the draw `0x59EE50` sends `0x6C` (`(code & 0xFC) -
0x20` = `0x4C`, byte table `0x59F440` -> entry 17 of `0x59F3D8`, the call at
`0x59F1B4`) to **`0x5A2900`**, beside the three `SPRT` handlers of D1. It
fills the four `D3DTLVERTEX`s at `0x7CA958` with:

- `sx, sy` = the primitive's x, y times the scale `0x7C9F4C` / `0x7C9F48`
  (2.0), no half-pixel offset - every edge on a whole pixel;
- `tu, tv` = the byte `u`, `v` times 2, times `0x5C4618` (a double,
  1/32) - the glyph's 24 texels in a 32 x 32 surface
  (`0x5A2BC0` fetches it by glyph and CLUT), no half-texel offset.

A 12-unit quad is 24 pixels showing 24 texels, 1:1. Direct3D 6 puts a pixel's
sample point at its integer coordinate, so **every pixel samples exactly on the
edge between two texels**: under point filtering which texel wins is float
rounding in the interpolator, and differs per triangle; under bilinear every
pixel is a 50 / 50 blend of two texels. The sprite handlers do not have this -
their coordinate table `0x7CA9E0` is `(i + 0.512) / 256`, a deliberate
half-texel inset (D1) - the glyph handler never got one.

The software renderer's glyph path (`0x5A4900`, first jump table, the call at
`0x59EFC9`) copies a 24 x 24 glyph to the back buffer directly when the quad
is 24 x 24 and so should not show this; not checked.

**Fixed as DIV-0025 (2026-09-22, group N; confirmed in game by the owner 2026-09-23, commit `e42ac23`)** - [`glyph-draw.md`](glyph-draw.md) §6. The proposal as it stood:

**Fix proposed (owner, 2026-09-22: "stage this as part of the next wave"):**
sample texel centres, `(2u + 0.5) / 32` on both axes, keeping the 1:1
scale - a [`DIVERGENCE.md`](DIVERGENCE.md) entry when built. Taking
`0x5A2900` (`0x5A2900..0x5A2BB3`) over is the way in: straight-line code,
two COM calls (device `+0x58` render state `0x1B`, `+0x70` DrawPrimitive)
and five callees (`0x59FBA0`, `0x5A2BC0`, `0x437CC0` a bare `ret`,
`0x59FCA0`, `0x59FD80`), fuzzable on the vertex block at `0x7CA958` against a clone with
stand-ins for `0x5A2BC0` and the device.

## D18 — The glyph texture cache overruns into the vertex block when full (latent)

**Found:** reading `Font_GlyphTexture` `0x5A2BC0` for the takeover,
2026-09-22 ([`glyph-draw.md`](glyph-draw.md) §3). **Latent, Capcom's**
(group N of the fourth round). Unmeasured in game.

**The defect.** The glyph textures live in a 128-entry cache
(`Font_TexCache` `0x7C9F50`, `0x14` bytes an entry), and an entry is free for
reuse only when it has not been used this frame (the draw clears the flags
after each `EndScene`). When all 128 are used in one frame and the glyph
asked for is none of them, the lookup's index is left at 128, one past the
table - and the table ends 8 bytes before `D3d_Vertices` `0x7CA958`, so entry
128 is those 8 bytes and the start of vertex 0. Under Direct3D it then calls
`SetTexture(0, ...)` with the bits of vertex 0's `sy` as a texture pointer,
and writes its in-use word over the low half of vertex 0's `sz` (the handler
has already filled the vertices, so the glyph is drawn at z 0.98829 instead
of 0.99). A float's bits handed to `SetTexture` as an interface pointer is
most likely a crash. Under the software surfaces (flag bit 0) there is no
`SetTexture`; the software glyph path `0x5A4900` gets the index 128 back, and
what it does with it was not read.

**Why it may never show:** it needs 128 distinct (glyph, CLUT) pairs in one
frame. A dense Chinese screen (an item or skill list) is the likeliest
place; no capture has counted them.

**Ours does the same** (the addresses are the original's arithmetic); the
fuzz seeds a full cache in a quarter of its rounds, and a control that
clamps the index to 127 is refused. A fix - evict an entry, or draw without
caching - is the owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.
## D21 — Ammonia sets a living member's HP to 1, then is refused (latent)

**Found:** reading `ItemUse_Revive` `0x496F50` for the takeover, 2026-09-22
([`item-use.md`](item-use.md) section 2). **Latent, Capcom's** (group P of
the fourth round); the PSX twin `0x801E81F0` (START.EMI) does the same, in
the same order.

**The defect.** The handler of item table entry 13 (Ammonia, consumable
`0x0E` by the US disc's table) stores 1 into the record's HP word *first*
and only then calls `Char_ClearStatus(id, 0x4000, battle)`. On a member
without the `0x4000` bit the clear answers 0, the handler answers 3 ("no
effect"), and the field menu plays the refusal sound and keeps the item -
but the member's HP is now 1. Ours keeps the order (control P26, D21
"fixed", is refused in 1,000 rounds of 2,000).

**Why it never shows:** the field menu's gate `0x57D9A0` offers a consumable
only when bit 0 of its flags is set, and Ammonia's flags byte is `0x46`
(every field heal has bit 0: `0xC7`, `0xD7`); so the menu never calls the
handler. Only a caller that skips that gate would reach it - none is known
(`ItemUse_Dispatch` has two callers, both in `0x58AAB0` after the gate).
The gate is by design (owner, 2026-09-22): Ammonia is a battle item, and a
fallen member is revived on leaving combat, so there is nothing for it to do
on the field. A fix would test the status first; it is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry if ever wanted.

## D22 — The system choice dispatch is unbounded: ids 0x90 and up call the stack (latent)

**Found:** reading `MsgBox_SystemChoice` `0x498A30` for group P,
2026-09-22 ([`item-use.md`](item-use.md) section 5). **Latent, Capcom's**;
the PSX `0x80152DB4` indexes the same way.

**The defect.** For a message-box choice id of `0x80` or more,
`MsgBox_ChoiceCommit` / `MsgBox_MenuCommit` call `0x498A30`, which stores
sixteen handler addresses on its stack (ids `0x80..0x8F`) and calls
`[esp + 4 * id - 0x200]` with no bound. Id `0x90` calls the word above the
table - `0x498A30`'s own return address, so the commit's tail runs twice
and the sub-state advances by two; `0x91` and up call further stack words
(the commit's return address into the state dispatcher, then saved
registers and the caller's frame), which unbalances the stack.

**Why it never shows:** which ids the scripts give code `0x14` was not
measured; only a script with a choice id of `0x90` or more reaches it. It is
why `0x498A30` stays Capcom's: no faithful C++ reproduces a call through an
arbitrary stack word, and bounding it is a divergence.

**Since round eight** (group DB, 2026-09-25, [`mode_states.md`](mode_states.md)
§2): `0x498A30` is ours. Ids `0x80..0x8F` call their handler; `0x90` from
either commit's tail is reproduced exactly (the tail run twice); `0x90` from
any other caller, `0x91` and up, and below `0x80` abort through
`bof3::Fatal` instead of corrupting the message box. The ids the scripts
use are still unmeasured.

## D23 — A stack of 99 Faerie Tiaras loses one when used (latent)

**Found:** reading `ItemUse_FaerieTiara` `0x4975F0` and `Inventory_Add`
`0x590BB0`, 2026-09-22 ([`item-use.md`](item-use.md) section 3). **By
reading, Capcom's** (PSX `0x801E8B20` the same); not observed.

**The defect.** The tiara's handler gives the item back -
`Inventory_Add(0, 0x57, 1)` - and answers 0, after which the field menu
takes one from the stack it was used from. `Inventory_Add` caps a stack at
99 (it stores 99 and answers 0 when the sum passes it), so from 99 the add
changes nothing and the menu's decrement leaves 98; from 98 and below the
count is unchanged. **Why it barely matters:** whether a stack of 99 tiaras
can be had in play is not known here (game facts are the owner's), and the
loss is one, once. **It cannot happen in play** (owner, 2026-09-22): there
is only one Faerie Tiara in the game, and having it is what lets the party
into the fairy rings on the world map; the owner thinks it cannot be used
from the menu at all. So the stack is at most 1, and the 99 cap is never
met.
## D24 — A BGM file that does not open restarts the previous track, looping (latent)

**Found:** reading `Music_LoadFile` `0x587A20` and `Music_Play` `0x587AE0`
for the takeover, 2026-09-22 ([`sound.md`](sound.md) §2). **Latent,
Capcom's (PC only - the PSX streams from the disc).** (Group Q of the fourth
round.)

**The defect.** `Music_Play(track)` stores `track` as the one playing and,
unless the file already loaded is that track's, calls `Music_LoadFile`, which
sets `Music_FileLoops = 1`, tries `BGM\NNN.DAT` and then `BGM\NNNN.DAT`, and
when neither opens returns -1 having changed nothing else. `Music_Play` does
not look at the result: it zeroes the volume and starts `Music_File` - still
the *previous* track's file - from the top, with `Music_FileLoops` now 1, so
a one-shot track comes back looping. `Music_LoadedTrack` still names the old
track, so the next `Music_Play` of the old track loads nothing and restarts
it. With no file ever loaded, `Music_Start(0, 0, 1)` copies nothing and the
decoder has nothing to open: D25.

**Why it never shows:** every track the game asks for has a file in the
shipped `BGM` (166 files; the plain name failing for the nine `N` tracks is
the designed test, [`asset-loading-path.md`](asset-loading-path.md) §1a). It
shows with a damaged or incomplete install, or a track number past the set.
Ours keeps it. A fix (skip the start when the load fails) is the owner's
call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D25 — A track the decoder cannot open plays a fragment of the last one, for ever (latent)

**Found:** reading `Music_Start` `0x5A6CC0`, `Music_CreateBuffer` `0x5A6E60`
and `Music_Pump` `0x5A7230`, 2026-09-22 ([`sound.md`](sound.md) §2).
**Latent, Capcom's.**

**The defect.** `Music_Start` stores `Music_OpenDecoder`'s result without a
test, then makes the streaming buffer anyway. `Music_CreateBuffer` fills the
buffer's first half through `Music_Decode`, which does nothing without a
decoder - so the half is whatever `Music_Staging` (`0x7CC378`, 0x12000
bytes) still holds: the last half-buffer of the previous track, about 0.42 s.
The buffer is played looping, and `Music_Pump` never refills it (it returns
at once without a decoder), so that fragment and the buffer's untouched
second half repeat until the next `Music_Play`. The decoder that did get made
but not opened is also never destroyed (a leak).

**Why it never shows:** the shipped files all decode. It needs a file that is
not MP3 the decoder accepts - or D24's case with no previous file, where the
staging block is still all zero and the result is silence.

## D26 — Music fades count spins of the frame wait, not frames: they are near instant

**Found:** reading `Sound_Tick` `0x587C70` and its one caller, 2026-09-22
([`sound.md`](sound.md) §3). **Capcom's, PC only; every fade in the game is
affected. Not yet heard by the owner** - it is the first thing on the live
check's list.

**The defect.** The fades (`Music_FadeIn` / `FadeOut` / `FadeOutStop`, the
event ops `B5`, `B9`..`BB`, `Music_Play`'s fade in) take a count the PSX
counts in frames (op `B4 tt 08` is an 8-frame fade in). `Sound_Tick` steps
the volume once per call and ends the fade when the count runs out - and its
only caller is WinMain's wait for the next frame, `0x4FCEBC`..`0x4FCEDC`,
which calls it on every spin of the loop (`call Sound_Tick; call
timeGetTime; compare; loop`). In `hidden_b` (a traced run, so slow) that was
6,710,895 calls in 16,128 frames, about 416 a frame; the 23 fades started
there took 502 steps in all. So an 8-frame fade is over within a few
hundredths of one frame: music starts at full volume and fade-outs are cuts,
at any machine speed faster than one spin a frame. A fade-out-and-stop
still stops the music; only the ramp is lost.

**The PlayStation fades audibly** (owner, 2026-09-22: "I do remember the
music fading in and out"; an imperceptible fade on the PC "would be a
divergence from the psx game"). So if the batch's listen confirms the cut,
the PC has lost something the release had.

**Fixed as DIV-0028** (2026-09-22, owner's request; confirmed in game 2026-09-23: "the fade sounds great"). The fix, as proposed: step the fade once per logic frame (a counter
the frame loop already keeps), a [`DIVERGENCE.md`](DIVERGENCE.md) entry.
The takeover keeps the per-spin step.

## D27 — The cell-texture cache's victim is the cell count when no entry is free (latent)

**Found:** reading `D3d_CellTexture` `0x5A3160` for the takeover, 2026-09-22
([`d3d-draw.md`](d3d-draw.md) §3). **Latent, Capcom's** (group R of the fourth
round). Unmeasured in game.

**The defect.** The cell sprites' textures (code `0x84`, the field's sprites)
live in a 128-entry cache, `D3d_CellTexCache` `0x7CAE38`, `0x28` bytes an entry.
On a miss the lookup evicts the entry not used this frame with the smallest use
counter below `0xFFFF`. It keeps that index in the stack slot of its own `count`
argument (`[esp + 0x1C]` at `0x5A31D9` and `0x5A324B`), so when no entry
qualifies - all 128 used this frame, or every unused one with its counter at
`0xFFFF` - the index it builds into, marks and hands to `SetTexture` is `count`,
the number of cells in the sprite being drawn. A sprite of `n` cells evicts
entry `n`: when all are in use, one whose texture earlier draws of the same
frame were already given. A count of 128 or more writes the whole entry
(`D3d_BuildCellTexture`), the counter and the in-use word past the table, up to
`0x7CAE38 + 0xFFFF * 0x28`, and passes whatever is at `+0x24` there to
`SetTexture`.

**Why it may never show:** it needs 128 distinct cell sprites in one frame (the
counters only wrap after 65,536 uses, so the second condition is rarer still).

**Ours does the same**; the fuzz seeds both conditions, and a control that
starts the victim at 0 is refused. A fix - pick any entry, or draw without
caching - is the owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D28 — Cell sprites map texel centre to texel centre: the last row and column get half their pixels (latent)

**Found:** reading `D3d_DrawCellSprite` `0x5A2EB0`, 2026-09-22
([`d3d-draw.md`](d3d-draw.md) §3, §6). **Capcom's, by reading**; not yet seen
or looked for in a capture.

**The arithmetic.** The texture coordinates of a cell sprite run from
`0.5 / W` to `(used - 0.5) / W` (`0x5A3051..0x5A3113`: `fld 0.5; fdiv`, `fild
used; fsub 0.5; fdiv`, the constant `0x5C41D8`), across a quad as wide as the
sprite's extent. At its own size that spreads `used - 1` texels over `used`
texels' width. At 2x with point sampling (Direct3D 6 samples at each pixel's
integer coordinate), pixel `k` samples `u = 0.5 + k (used - 1) / (2 used)`;
for `used` = 16, 24 and 32 every texel gets 2 pixels except one in the middle
(3) and the last (1). It is D1's far-edge slip - which DIV-0010 fixed for the
`SPRT` handlers - on the field's sprites, whose one builder is `Sprite_Draw`
`0x5935B0` (`Gpu_SetCode84` at `0x59366A`). Under the default bilinear filter it
is a soft one-texel squeeze instead.

**Proposed, not built:** DIV-0010's rule - keep the near value and move the far
one so the last pixel samples the last texel's centre: far = `0.5 + (used - 1)
N / (N - 1)` texels for an `N`-pixel span (at 2x, 15.984 for 16 texels, 31.992
for 32 - every texel exactly two pixels). Exact only at the sprite's natural
size. What a capture would show: the bottom row and the right-hand column of a
standing character (the left-hand column when it faces the other way, x
flipped) one screen pixel thin under `BOF3X_FILTER=point`.

## D30 — A page texture whose Lock fails leaks its surface and draws untextured (latent)

**Found:** reading `D3d_BuildPageTexture` `0x5A0080` and
`D3d_RefreshPageTexture` `0x5A0510` for the takeover, 2026-09-23
([`tex-page.md`](tex-page.md) §2, §3). **Latent, Capcom's** (group T of the
fifth round). Unmeasured in game. The number may need renumbering at the merge
if another group of the round took D30 too.

**The defect.** The build makes the entry's surfaces first - a texture surface
and its `IDirect3DTexture2` into the `Gfx_TexCache` entry's `+0x10` / `+0x14`
(`0x5A02EA`), or a plain surface into `+0x10` (`0x5A0161`) - and only then
locks the staging surface (`0x5A0350`, `0x5A03C1`) or the new surface
(`0x5A01A2`, `0x5A021E`). When that `Lock` fails it returns 0 at once
(`0x5A0357`, `0x5A03C8`, `0x5A01A9`, `0x5A0225`): the entry's state byte is
never set, so the entry stays free with the new surface and texture in it,
never released - the next build of that page takes the same entry and
overwrites both pointers (`Gfx_InvalidateTextures` `0x59E700` releases only up
to the first free entry, so it never reaches them). And `D3d_BindTexture`
hands the 0 to `SetTexture`, so the primitive draws untextured this frame.
Under the software surfaces the same 0 is also the index of slot 0, so the
caller `0x5A3CC0` cannot tell a failure from a build into slot 0 (what it
then does with the entry is unread). A failed `CreateSurface` returns 0 the
same way, leaking nothing.

The refresh sets the state byte to 1 **before** its `Lock` (`0x5A0559`); a
failed `Lock` returns with the texels and the palette generation unchanged
(`0x5A05E9`, `0x5A0645`, `0x5A06EE`, `0x5A0773`). A palettized entry is
caught again - `Gfx_TexCacheFind` compares the generation and sets state 2
next time - but a direct-colour (15-bit) entry stays stale until its VRAM is
written again.

**Why it may never show:** every `Lock` is `DDLOCK_WAIT` on a system-memory
surface (`Dd_CreatePlainSurface` caps `0x840` / `0x1800`, both
`DDSCAPS_SYSTEMMEMORY`), which fails only on `DDERR_SURFACELOST` - a lost
display mode, e.g. a fullscreen alt-tab - or out of memory. One leak of 128 KB
or 256 KB per failure.

**Ours does the same**; the fuzz fails the `Lock` (and the `CreateSurface`, and
the texture's `QueryInterface`) in a third of its rounds, and the controls
"a failed Lock returns the slot" and "a failed Lock marks the entry" are
refused ([`tex-page.md`](tex-page.md) §6). A fix - release the surfaces on
the failure path, and have the refresh set its state only on success - is the
owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D31 — A cell texture whose texture surface cannot be made ends the scene for the frame, and its entry looks built (latent)

**Found:** reading `D3d_BuildCellTexture` `0x5A32B0` for the takeover,
2026-09-23 ([`tex-cells.md`](tex-cells.md) §3). **Latent, Capcom's** (group U
of the fifth round). Unmeasured in game. The number may need renumbering at
the merge if another group of the round took D31 too.

**The defect.** On the Direct3D path the build ends the device's scene
(`EndScene`, `0x5A3696`) before it makes the texture, and begins it again
(`BeginScene`, `0x5A377D`) only at the very end. When
`Dd_CreateTextureSurface` fails (`0x5A36DF`, `je 0x5A3780`) it returns
between the two: the scene the draw walk `Gfx_DrawOTag` began stays ended for
the rest of the frame, so every primitive after this cell sprite is drawn
outside a scene (Direct3D 6 refuses `DrawPrimitive` there) and the walk's own
`EndScene` fails. And the entry is already filled - `D3d_FreeCellTexture`
emptied it, then the extent, used size, CLUT, count, checksum and generation
were stored (`0x5A355F..0x5A35EC`) - with no surface or texture (`+0x20` 0,
unless DirectDraw wrote something there on failure, `+0x24` 0). The first
dword is non-zero, so `D3d_CellTexture` counts it as built: the next draw of
the same cells hits it (count, CLUT and checksum match) and binds texture 0
- the sprite untextured - until the entry is evicted. If its CLUT row's
generation moves first, `D3d_RefreshCellTexture` Blts into `+0x20` without
testing it (`0x5A3A23..0x5A3A43`): a call through a null pointer.

The software path has the same shape without the scene: a failed
`Dd_CreatePlainSurface` (`0x5A362F`) leaves the filled entry with no surface
and `+4` / `+6` 0. A failed `Lock` of the staging surface returns before the
entry is touched (`0x5A336F`), so the victim keeps its old texture and
`D3d_CellTexture` binds that for this draw - the wrong cells for one frame.

**Why it may never show:** `Dd_CreateTextureSurface` asks for a managed
texture (caps2 `DDSCAPS2_TEXTUREMANAGE`) no larger than the device's maximum
(`D3d_FitTextureSize`); it fails on out of memory or a lost device.

**Ours does the same**; the fuzz fails `CreateSurface` in a sixth of its
rounds, and the control that calls `BeginScene` on that path - the fix - is
refused ([`tex-cells.md`](tex-cells.md) §6). A fix - `BeginScene` and an
emptied entry on the failure path, and a refresh that tests `+0x20` - is the
owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.

## D32 — The cell builders write sprite pieces wherever their offsets say, past the staging surface (latent)

**Found:** reading `D3d_BuildCellTexture` `0x5A32B0` and
`D3d_RefreshCellTexture` `0x5A37D0`, 2026-09-23 ([`tex-cells.md`](tex-cells.md)
§3). **Latent, Capcom's.** Unmeasured: whether any sprite of the game has a
piece that far out is not known.

**The defect.** Each SpriteCell record is unpacked to `lpSurface + (x + 160)
* bytes-per-pixel + (y + 128) * lPitch` of the locked 320 x 256 staging
surface (`0x5A3434..0x5A3458`), x the record's s16 and y its s8, the piece `w`
and `h` texels (8..120 each). Nothing bounds it. A piece with `x + 160 + w`
beyond 320 runs into the next row; one with `y + 128 + h` beyond 256 - any
piece more than `128 - h` below the sprite's origin, which an s8 allows -
writes past the surface's last row into whatever DirectDraw put after it in
system memory; one with `x < -160` writes before the row. And the extent kept
from those offsets becomes the source rectangle of the `Blt` into the texture,
which DirectDraw refuses when it leaves the surface - the texture keeps
whatever it had.

**Why it may never show:** the pieces are a sprite's frame layout
(`Sprite_Draw` `0x5935B0`, `SpriteCell_Add`); a sprite whose pieces fit inside
320 x 256 around its origin never reaches it. None of the attract sequence's
has been measured.

**Ours does the same**; the fuzz keeps its pieces inside the stage except in
one round in sixteen, where they spill across rows and past row 256 (inside
the fake's buffer), and compares both. A fix - clip each piece to the stage,
or refuse the sprite - is the owner's call and a
## D33 — A jump's speed index can count down to a division by zero (latent)

**Found:** reading `Field_JumpSetUp` `0x534610` and `Field_JumpCheckHeight`
`0x535F50` for the takeover, 2026-09-23 ([`event-objs.md`](event-objs.md)
sections 2 and 5). **Latent, Capcom's** (group V2 of the sixth round); the
PSX twins `0x801C3530` / `0x801C5C40` do the same (`break 7` on the zero
divisor).

**The defect.** `Field_JumpSetUp` divides the jump's frames (0x10, or 0x20
for directions 2 and 6) by `Field_MoveSpeeds[Field_State +0x128]` - a whole
byte into a table of six, `0, 1, 2, 4, 8, 16`, with zeros at 6 and 7 after
it - and then the rise by those frames, both with `idiv`. Index 0, 6 or 7
faults on the first division; a speed above 0x20 (index 8 reads 64) leaves
0 frames and faults on the second. `Field_JumpCheckHeight` lowers the index
by one, with no bound, whenever the ground at the landing point is 0x80 or
more above the object, and starts the jump again. The landing point does not
depend on the speed (the distance is 16 steps whatever it is), so the same
ledge is found too high again at the next index.

**Why it may never show:** each check lowers the index once, and whether
anything puts it back between attempts is unread - `Field_LeaderStart`
`0x52D920` sets 3 on the leader's state 0, and the unreached `0x535FE0` sets
`+0x70 + 2`. Three too-high landings without a reset in between would reach
index 0. Not seen in any run.

**Ours does the same** (both divisions are an inline `idiv`; the fuzz draws
only the speeds that divide, since a fault would end the start-up test).
Bounding the index is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry.
## D34 — Inventory_Add searches the 32-byte key-item list 128 long (latent)

**Found:** reading `Inventory_Add` `0x590BB0`, 2026-09-23
([`char-stats.md`](char-stats.md) section 4). **By reading, the PC's own**:
the PSX `Inventory_Add` `0x80165AA4` has no category-4 branch at all.

**The defect.** For category 4 the PC adds the item to the first id byte of
0 in the list `0x656B00[4]` = `0x904554`, and the loop runs 128 bytes, as
for the other categories. The key-item list is 32 bytes (`KeyItem_Has`
`0x5918E0` and `Inventory_CountUsed` `0x591A80` both stop there; the list
`0x590C90` fills starts at `0x904574`, right after it). With all 32 key-item
bytes set, a 33rd key item is written into that next list's first zero byte
and the answer is 1. **Why it may never show:** whether the game ever holds
32 key items at once is the owner's to say; the PSX's key items are 16
records (`NameTable_KeyItems`), so probably not.

**Ours does the same**; the fuzz searches category 4 with full lists and
the control "stacks searched in category 4 too" is refused (by a fault, the
null count list - see D35 - with a counting twin).

## D35 — Inventory_Count of a key item that is held reads address 0 + i (latent)

**Found:** reading `Inventory_Count` `0x5919B0`, 2026-09-23
([`char-stats.md`](char-stats.md) section 4). **By reading, Capcom's** on the
PC; the PSX pointer tables are filled at run time and were not read.

**The defect.** With `equipped` 0 the function looks the item up in the id
list `0x656B00[category]` and answers the byte at the same index of the
count list `0x656B14[category]`. For category 4 that count pointer is 0, so a
key item that is in the list reads address `i` (0..31) - an access
violation. **Why it may never show:** nothing is known to ask for a key
item's count; the 40 call sites are unread. **Ours does the same** (a
volatile read of the same address), and the fuzz does not ask it.

## D36 — Menu_DrawIcon's CLUT table has 21 entries and no bound (latent)

**Found:** reading `Menu_DrawIcon` `0x5903F0`, 2026-09-23
([`char-stats.md`](char-stats.md) section 5). **By reading, Capcom's on both
platforms**: the PSX `0x80164EC8` builds the same 21-byte table on its stack
and indexes it the same way.

**The defect.** The icon's CLUT row comes from a 21-byte table on the stack,
indexed by the icon's low byte without a bound: icon 21 and up takes a byte
of the frame instead - three stack bytes never written (21..23), the return
address, the first argument's slot holding a float temporary, the other
arguments, the last argument's slot holding `y + h`, then the caller's
frame. Icons 20 and up already draw the one fixed cell, so only the palette
would be wrong. **Why it may never show:** the icons of the seven callers
come from a loop bound or a table (`0x6672AC`) not read here.

**Ours does the same** for icons 24 and up (read from the same stack, the
detour being a `jmp`; the fuzz compares 24..91), and not for 21..23, which
are undefined in the original.

## D37 — A sprite's far texture edge is read past the coordinate table when u + w passes 256 (latent)

**Found:** reading the three Direct3D sprite handlers for the takeover,
2026-09-23 ([`sprt-draw.md`](sprt-draw.md) §2). **Latent, Capcom's** (group D
of the sixth round). Unmeasured in game.

**The defect.** `D3d_DrawSprt` `0x5A2300` (SPRT) takes a sprite's far texture
edge from `[0x7CA9DC + 4 * (u + w)]` - `D3d_TexCoords[u + w - 1]` - with `u`
the primitive's byte and `w` its u16 width, added unchecked; `D3d_DrawSprt8`
and `D3d_DrawSprt16` read `D3d_TexCoords[u + 7]` and `[u + 15]` the same way;
v and h likewise. The table has 256 entries. A sprite whose texels run past
column 255 of its page (u + w above 256, u above 248 for SPRT_8, above 240
for SPRT_16) reads its far coordinate from what follows the table: the
dwords from `0x7CADE0` (the third holds `D3d_AfterDrawRequest` `0x7CADEA`),
then from `u + w - 1` = 278 on `D3d_CellTexCache` `0x7CAE38` - a cache
entry's words read as a float - and, for a u16 w of thousands, anything up
to `0x80ADD4` (all inside `.data`, so never a fault). A `u + w` of 0 reads
the dword before the table (`0x7CA9DC`, the gap after `D3d_Vertices`). The
far edge is then an arbitrary float, and the sprite's texture is stretched
or squeezed to it.

**Why it may never show:** a PSX texture page is 256 texels wide and the
PlayStation wraps u within it, so a sprite crossing column 255 would already
have looked wrong on the console; the game's data probably never asks for
one. None of the attract sequence's has been measured.

**Ours does the same** with DIV-0010 off (`BOF3X_ORIGINAL=SpriteFarEdge`):
the fuzz has `u + w` past 256 in two of every three of SPRT's rounds, many
of them inside the 768 entries from the table on, which it randomises, and
compares ours with Capcom's byte for byte; a control that reads one entry
short is refused. With DIV-0010 on, the index reads our table
(`g_far`), which has an entry for every `u + w` a `u8 + u16` can make: past
256 it continues the line, `(j - 1/30 + 0.012) / 256`, a coordinate past the
page's edge - so the sprite takes texels as the device's texture addressing
wraps or clamps them, not garbage. That is a side effect of DIV-0010, not a
fix of this: a fix (wrap `u + w` at 256 as the PSX did, or clamp) is the
owner's call and a [`DIVERGENCE.md`](DIVERGENCE.md) entry.
## D38 — A save that fails its checksum is loaded into the live game block anyway (candidate, latent)

**Found:** reading `LoadMenu_Read` `0x5883C0` for the sixth round's group X,
2026-09-23 ([`save-menu.md`](save-menu.md) section 2). **Latent, Capcom's**;
the PSX loads from the memory card by other code, not compared.

**The defect.** The load menu reads the chosen `BISLPS0<n>.DAT` into
`Save_Staging`, zeroes the checksum word (`+0x70`) and then copies the
`0x10B0`-byte block into the live game block `0x9039E0` **while** summing it;
only after the copy does it compare the sum with the stored checksum
(`0x588462`). On a mismatch it plays the refusal sound, shows "could not
load" (error 2) and goes back to choosing a slot - with the game block
already overwritten by the rejected file, and `Field_ScriptFlags` set from
its byte `+0x74D`. A failed *read* (`Save_ReadFile` -1) copies nothing.

**Why it may never show:** from there the player can only load another save
(which overwrites the block again) or cancel to the title and start a New
Game. `TitleFlow_NewGame` `0x5880E0` resets the character records
(`NewGame_InitCharacters`), half of `Cond_Flags` (the first dword of each
8-byte record) and the area; whatever else of the block it does not reset -
the inventory, the story flags from `Cond_Flags + 0xA0`, the gold, by their
offsets unread here - would come from the corrupt file. Unchecked in game:
it needs a save file with a wrong checksum.

**Ours does the same** (control "the checksum compared as dwords" and
"`Field_ScriptFlags` only on a good sum" are refused,
[`save-menu.md`](save-menu.md) section 4). A fix - sum `Save_Staging` first
and copy only on a match - is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry.
## D39 — The menu box's middle takes its bottom texture row from h, not y + h (latent, unseen)

**Read, not seen:** group Y of the sixth round, 2026-09-23
([`menu-windows.md`](menu-windows.md) section 2). `Menu_DrawBox` `0x57CF60`
draws the menu box as three or four `POLY_FT4`s of one 16 x 16 tile repeated
through a texture window. Each quad's `v` coordinates are the screen `y` and
`y + h` as bytes - so the tile's rows line up with the screen - for the left
end (`0x57D065` stores `(h + y)` low byte at `+0x35` / `+0x45`) and the right
end (`0x57D2C8` reloads that byte from the argument slot where `0x57D075`
kept it). The middle quad(s) take their bottom `v` from `[esp + 0x40]`,
which there is `h`'s own slot (`0x57D155` and `0x57D1C8`): `h`'s low byte,
not `(y + h)`'s. The middle's texture is therefore stretched or squeezed by
`y` mod 16 rows against the ends whenever `y` is not a multiple of 16.

**Why it may never show:** if the tile's rows are uniform, or every caller's
`y` is 16-aligned after its `+ 3`, nothing differs. Not checked against the
PlayStation's `0x801AF3F0` (an overlay, unread) nor in game.

**Ours does the same**; the control "the middle's v from y + h" is refused
in 1,883 rounds of 2,000.

## D40 — Four bounds the menu draws never check (latent)

**Read, not seen:** group Y, 2026-09-23 ([`menu-windows.md`](menu-windows.md)
section 3). None is known to be reached by any caller:

- `Menu_DrawPanel` `0x575830`: the bottom edge's counter is a byte compared
  with `w + 5`, so a `w` of 251 or more never ends (callers pass constants of
  9 and so).
- `Menu_DrawScrollBar` `0x57DD10`: `idiv` by `2 * total` and by `total`, so a
  total of 0 is a divide fault (its callers pass 0x20, 0x80 or a list's size).
- `Menu_DrawItemList` `0x5759C0`: its row counter is a byte compared with
  `moving + 9`; `Menu_ListScroll` only answers 0 or 1, so it is safe as
  shipped. The list takes its item and count arrays from the category once
  but tests the category again per row, and key items (category 4) have no
  count array (`0x656B24` is 0): a category changed to non-4 while drawing
  would read through the null pointer. Nothing changes it mid-draw.
- `Menu_DrawBackdrop` `0x575690`: `kind` indexes four CLUT words on its own
  stack with no bound; 4 and up read its stack frame and its caller's. Config
  keeps the byte `0x903A5B` in 0..3; only a corrupted save reaches it. Seen
  poked (`tools/recipes/backdrop_kinds.txt`): Capcom's code draws no backdrop
  at 4..8, 16, 64 and 255, the menu on black. **Ours draws none**
  (DIV-0030); the other three are kept as the original has them.
## D41 — A primitive corner at depth 0 is never drawn: the world map's compass needle (PC only, fixed by DIV-0044)

**Found:** the owner's recorded world-map route, 2026-09-23
([`world-map.md`](world-map.md) §3). Seen in the all-original capture (no
needle in the dial, `analysis/shots/worldmap_orig/f01260.png`) and read.

**The defect.** `0x408530`, called once a frame by the world map's frame
function `0x404390`, draws the compass needle: `Gte_PushMatrix`, the map's
rotation matrix `0x929EC8` through `Gte_RotMatrix` and `Gte_SetRotMatrix`, a
zero translation, then `Gpu_SetPolyG4` over the four corners at `0x9037A0` -
(-10, 0, 0), (0, -4, 0), (0, 4, 0), (10, 0, 0), a diamond - projected by
`Gte_RotTransPers4` and given depths by `Gte_PrimDepths4_10B`, moved into the
dial by `(0x9E - a, 0x76 - b)`, coloured red, purple, purple, blue, and
committed. The depths come out 1/4096, 0, 0, 0 (`BOF3X_DRAWLOG_RGB=800080`,
DIV-0044's verification). The port's handler `D3d_DrawPolyG4` sets `rhw = 0.1
/ z`, infinite for three corners, and Capcom's Direct3D 6 device draws
nothing: the PC port never shows the needle. The PlayStation draws it (the
owner's screenshots of both releases: it turns with the map). **PC only**,
in the port's PSX-to-Direct3D translation. Fixed in the backend by DIV-0044,
not in the port's code, so the primitive is Capcom's byte for byte.

**Seen beside it, not read:** on the PlayStation the dial is translucent over
the map; the PC port draws it opaque. The dial is one of the two sprites
`0x404620` draws through `0x404560`, which has a semi-transparent path
(`Gpu_SetSemiTrans`) - which path the dial takes, and why it differs, is for
the takeover ([`world-map.md`](world-map.md) §4). *Read since: D42.*

## D42 — The world map's dial is opaque: the port clears its semi-transparency bit (port change, kept)

**Found:** the owner's screenshots of the PlayStation releases beside the PC
port, 2026-09-23; read on the takeover of the world map's frame, 2026-09-24
([`world-map-hud.md`](world-map-hud.md) §5.1).

**The defect.** The world map's sprite draw `WorldMap_DrawSprite` `0x404560`
(the port's; the PSX twin is `0x801F39D8` in the map overlay `0x801F2C00`
off the JP disc) draws every sprite of the dial page - the dial, the three
legend labels, the key glyphs, the region box - through one `SPRT`. The PSX
calls `SetSemiTrans(prim, 1)` for all of them; the port calls
`Gpu_SetSemiTrans(prim, index != 0)`, so the dial, index 0, is the one sprite
drawn with the bit clear. **A port change**, one operand, and deliberate:
the PlayStation blends a textured semi-transparent primitive per texel (only
a texel whose CLUT entry has STP set is blended - on the JP dial 1,054 of
5,376 texels, the glass over the map; the rim and markings are opaque; the
legend has no STP texel and draws opaque), while the port's texture path
drops STP (`Gfx_ConvertRow`'s palettes force every non-zero cell opaque) and
its Direct3D blend is per primitive (`D3d_PrimColor` alpha `0x80`,
`SRCALPHA / INVSRCALPHA`). With the PSX's operand the port would draw the
whole dial, rim and all, at 50 %; the porting house cleared the bit instead.
The same loss runs the other way for the legend: 50 % on the PC, opaque on
the PSX.

**Kept.** Ours is byte faithful to the port (the flip is negative control P1,
refused in every opaque-path round). The PlayStation's picture needs STP
carried into the 8-bit page's palette as alpha and the sprite handlers'
blend keyed on it - a texture-path change that would make every
semi-transparent 8-bit sprite in the game blend per texel as on the PSX -
after which `SetSemiTrans(prim, 1)` here is a one-line divergence. Owed: the
owner's eye, if that path is built.
## D43 — The battle skill list's cursor row and its raised row disagree once scrolled (candidate, latent)

**Found:** reading `BattleMenu_DrawSkillList` `0x59D200` for group BJ of the
seventh round, 2026-09-24 ([`battle_draw.md`](battle_draw.md)). Read, not
seen; the PSX side not compared.

**The defect.** The list colours a row as the cursor's when its on-screen
index equals the cursor byte `+0xD` (`0x59D2D6`), but raises a row when top
+ index equals `+0xD` (`0x59D31B`). With the list scrolled the two pick
different rows. Whether the window task's step keeps the two in agreement
is unread (its tables `0x66B54C` / `0x66B560`). Ours keeps it; control S1
of that group refuses the fix.

## D44 — `BattleWin_DrawCell16` takes its x and y as unsigned (PC only, latent)

**Found:** group BD, 2026-09-24 ([`battle_window_draw.md`](battle_window_draw.md)).
Read, not seen. Every other helper of the battle windows reads the
coordinates signed; this one reads them unsigned, so a negative coordinate
lands 65,536 pixels away instead of just off the edge. The PSX twin has one
reading for both. Kept.

## D45 — `BattleWin_FirstOfKind` treats every window 5..12 as an enemy's (latent)

**Found:** group BD, 2026-09-24 ([`battle_window_draw.md`](battle_window_draw.md)).
Read, not seen. A party actor in one of those windows is looked up before
the enemy records (`0x93B674` for actor 0), which could hide an enemy's
name. The PSX does the same. Kept.

## D46 — Five unchecked indices in the battle windows (latent)

**Found:** group BC, 2026-09-24 ([`battle_windows.md`](battle_windows.md)).
Read, not seen; kept as the original has them:

- `BattleWin_DrawCommandIcon` `0x4434C0`: the colour table has no bound; a
  command index of 7 or more reads the function's own stack frame (the PSX
  the same; ours reads the same bytes, the fuzz checks 8..43).
- `Window_DispatchKind` `0x597A30`: the handler table on its stack has no
  bound; an index of 3 would call the function's own return address.
- `BattleObj_RunState` `0x4411E0`: the 27-entry state table has no bound.
- `BattleWin_DrawCommandLabel` `0x4439A0`: the command index has no bound.
- `Text_GlyphCount` `0x597F40`: reads past the end of a string whose
  double-byte lead byte sits just before the terminator.

## D47 — A repeated drop in one battle also appends an item 0 (PC only, latent)

**Found:** group BB, 2026-09-24 ([`battle_flow.md`](battle_flow.md) §5),
`Battle_RollDrops` `0x437580` against the PSX `0x801E525C`. Read, not
seen. On the PSX a drop that matches an entry already in the list adds to
its count and stops; the port keeps searching, then also appends the zeroed
item word as an item 0 with count 1. Needs two drops of the same item in
one battle. Kept.

What the result screen does with it (group CD, 2026-09-25,
[`battle_result.md`](battle_result.md) §5): `BattleResult_Setup` counts it
(the drop window grows by 13 for every second entry, the sort puts the word
0 first); the drop window's draw `0x5984B0` skips a word of 0 (`0x5984F4`)
but places every entry by its index, so the real drops shift one place and
**the first slot is left empty**; `Inventory_Add` returns 0 for item 0 and
adds nothing. Visible - an empty first slot, possibly one extra row - and
harmless to the inventory.

## D48 — `BattleStep_Expire4000` reads an enemy's counter from the party array (latent)

**Found:** group BA, 2026-09-24 ([`battle_setup.md`](battle_setup.md) §6).
Read, not seen. The step reads each enemy's flag-0x4000 counter from the
party array at the enemy's actor index (`0x803266` + 0x14C each) and then
zeroes the enemy's own counter at `+0x122`, the one `Battle_TickCounters`
counts up. The PSX does the same, so it shipped on both. Kept.

## D49 — An enemy's AP heal is checked against its max HP (latent)

**Found:** group BE, 2026-09-24 ([`battle_damage.md`](battle_damage.md)),
`Effect_ApplyResult` `0x44B9F0`. Read, not seen. The heal is compared
against max HP and then clamped to max AP, so an enemy's AP can end above
its maximum. The PSX the same. Kept.

## D50 — Level 99 reads past the turn-order tables (latent)

**Found:** group BE, 2026-09-24 ([`battle_damage.md`](battle_damage.md)),
`Battle_LevelClass` `0x445640` and its users. Read, not seen. Level 99
lands in level class 6, one past the tables: a level-99 party member's
bonus becomes 516 %, and a level-99 enemy's random offset comes from the
damage variance table. The PSX the same. Kept. (Whether a level of 99 is
reachable in play is the owner's to say.)

## D51 — An enemy's charge replaces its attack instead of adding to it (candidate)

**Found:** group BE, 2026-09-24 ([`battle_damage.md`](battle_damage.md) §2),
`Battle_CalcDamage` `0x445CF0`. Read, not seen. A charged party member's
attack is added to; a charged enemy's is replaced by charge × ATK / 2, so a
charge of 2 does nothing for an enemy. The PSX the same; whether it was
intended is unknown. Kept.
## D52 — The sparkle effect's phase table has no bound (latent)

**Found:** group BH, 2026-09-24 ([`battle_items.md`](battle_items.md)).
Read, not seen. `0x4B9000`, the sparkle's type-0 update (Capcom's; not
taken, for this reason), calls its phase handler through a three-entry
table on its own stack with no bound: a phase of 3 or more calls its own
return address or the caller's stack. `Sparkle_Types` `0x65AE28` has one
real entry, so a non-zero type would jump into data; the one writer stores
0. Neither can be reproduced; a bounds check would be a divergence (as
D22's `MsgBox_SystemChoice`). Kept.

**Since round eight** (group CJ, 2026-09-25, [`magic_fx_reached.md`](magic_fx_reached.md)
§3): `0x4B9000` is ours as `Sparkle_Update`, taken with the stack-table
abort (D59); its phases stay 0..2 in every writer read.

## D53 — `SndStream_Stop` tests an uninitialised local when `GetStatus` fails (latent)

**Found:** group BH, 2026-09-24 ([`battle_items.md`](battle_items.md)),
`SndStream_Stop` `0x5A71C0`. Read, not seen. PC only (the PSX streams from
the disc). Kept.

## D54 — The battle's pop-up writers do not check for a full record pool (latent)

**Found:** group BG, 2026-09-24 ([`battle_sprites.md`](battle_sprites.md) §7).
Read, not seen. `Battle_SetDamagePopup` `0x453DA0` and `Battle_SetHitPopup`
`0x454410` take a record from `BattleTask_Create` `0x435180` (48 records of
0x84 bytes at `0x93A000`, 0xFF when none is free) and never test for 0xFF,
so a write would land past `.data`. Kept.

## D55 — Eight different enemy kinds walk `Battle_OpenEnemyNames` past its list (latent)

**Found:** group BG, 2026-09-24 ([`battle_sprites.md`](battle_sprites.md) §7),
`Battle_OpenEnemyNames` `0x494A80`. Read, not seen. With eight distinct
enemy kinds alive the walk runs past its eight-entry list into the stack;
the PSX does the same. Ours reads the same words, since its entry stack
pointer is the original's. Kept. Also there: the enemy data id is indexed
by 16 bits in `Battle_SetupEnemy` `0x494320` but by 8 in
`Battle_CopyEnemyData` `0x4946C0`; `ClutMap_Mark` `0x454DF0`'s release
writes over the last scratch cell when the owner is not found; and
`Sprite_SetClutStp` `0x4551A0` divides by zero for a CLUT kind of 5 or more.
## D56 — `Encounter_OnScreen` lost the PSX's lower bounds (PC only, latent)

**Found:** group BI, 2026-09-24 ([`inventory_ops.md`](inventory_ops.md)),
`Encounter_OnScreen` `0x5928F0`. Read, not seen. The PSX compares the
projected corners as unsigned shorts against 320 and 240, which rejects a
corner left of or above the screen as well; the port compares floats
against 320 and 240 only, so such a corner passes. Kept faithful; a fix
would be a ledger entry.

## D57 — The encounter placement's small faults, as the PSX has them (latent)

**Found:** group BI, 2026-09-24 ([`inventory_ops.md`](inventory_ops.md)).
Read, not seen; every one the PSX's too, kept:

- `Encounter_PlaceParty` `0x5920E0` checks the recentred path from the
  centre's z cell twice, (Z, Z), and when none of the six retries passes
  the centre still moves to the sixth.
- `Encounter_AimCamera` `0x592A30` divides by the number of things averaged
  with no guard.
- `Sprite_ShadeLower` `0x534880` wraps above 127.
- `Encounter_RollInitiative` `0x532550`'s "all noticed" result also needs
  the two slots past a three-member party to roll 50 or less.

## D58 — The Config panel's keyboard column is hard-coded to the default keys (PC only)

**Found:** reading the Controller sub-panel, 2026-09-24
([`controls.md`](controls.md) §2). `0x461C00` draws, beside each action's
PlayStation icon, a one-byte string per pad bit - `V` `C` `Z` `X` `S` `A` at
`0x653818..0x65382C` - which are the *default* key table's letters. The
live table (`Key_Table`, `BOF3.CFG` lines 3+) is never read, so a rebound
keyboard leaves the panel showing keys that do nothing. The PlayStation has
no such column. **Ours:** gone under DIV-0051 (2026-09-24, the one icon
column) - fixed under an overlay language only; with `BOF3X_LANG=original`
the column is still drawn as the original draws it. The live bindings are
the launcher's Controls dialog.

## D59 — Dispatch indices are never checked (latent; ours aborts past a stack table)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, first by group CA ([`battle_phases.md`](battle_phases.md) §1,
§5) and then by nearly every group of the eighth round. One entry for the
class; the groups' own sections carry the tables in full.

**Established:** a state, step or kind byte is used as a table index with no
bound, in the two forms the battle engine and the field use. The `.data`
tables are read in place by ours as by the original, so an index past one
reads the next table's entries (or whatever follows) on both sides. The
stack-built tables (`mov [esp + 4 i], imm32` then `call [esp + index * 4]`)
call the dispatcher's own return address and then the caller's frame past
their end; **ours aborts there with `bof3::Fatal`** (CLAUDE.md rule 4,
battle_flow's precedent), a difference only on an index the original would
crash on. No index past a table was seen or found reachable with the
game's own values.

- **Stack tables (ours aborts):**
  - CA: `BattleStart_Dispatch` `0x42E470` and `BattleIntro_Dispatch`
    `0x42E730` ([`battle_phases.md`](battle_phases.md) §1).
  - CE: the six stack tables of the battle effect tasks (19, 110, 5, 2, 5
    and 1 entries), `BattleFx_Dispatch` `0x4352A0`'s 19 and
    `BattleMagicFx_Dispatch` `0x435350`'s 110 among them
    ([`battle_fx_tasks.md`](battle_fx_tasks.md) §3).
  - CJ: `FxDiscFan_Task` `0x4C4FC0`, `Steal_Task` `0x4B54B0`,
    `StealClone_Task` `0x4B5830`, `Sparkle_Task` `0x4B8D70` and
    `Sparkle_Update` `0x4B9000` (D52's, now taken with the abort)
    ([`magic_fx_reached.md`](magic_fx_reached.md) §3, §8).
  - CL: `BattleWin_Run` `0x596FA0`'s eight kinds and its kinds' state
    tables. The member gauge's table has no idle entry, so state 3 of kind 5
    calls `0x596FF2`, the middle of `BattleWin_Run` (`add esp, 0x20; ret`
    on the wrong frame); what would reach state 3 there needs a target byte
    a slot of 0..2 never has ([`battle_win_states.md`](battle_win_states.md) §4).
  - CM: `Window_DispatchKind` `0x597A30` (D46's), `BattleWin_BannerRun`
    `0x597C70`, `BattleWin_MessageRun` `0x597D50` and
    `Window_Handler4Kinds` `0x597F60` ([`window_kinds.md`](window_kinds.md) §1).
- **`.data` tables (ours reads the same entry):**
  - CA: `Battle_InputSteps` `0x64AE28`, `Battle_MenuSteps` `0x64AE54`,
    `Battle_CommitSteps` `0x64AE74`.
  - CB: the seven action tables `BattleAction_Steps` `0x64AE80` ..
    `BattleAction_AfterSteps` `0x64AF20` ([`battle_actions.md`](battle_actions.md) §1).
  - CC: the phase 4 and 5 stubs `0x4302B0` and `0x4311E0` and their tables
    `BattleRoundEnd_Steps` `0x64AF2C` .. `BattleEnd_ResultPages` `0x64AFAC`
    ([`battle_turn_steps.md`](battle_turn_steps.md) §3).
  - CD: `BattleResult_LevelUpStep` `0x431B60` and `BattleResult_RewardStep`
    `0x431D50` over `0x64AFC0` and `0x64AFC8` (0..255 all land inside
    `.data`) ([`battle_result.md`](battle_result.md) §1).
  - CE: `Magic_Rows`, by `Sprite_Current +5`.
  - CF: the enemy op tables, `BattleEnemy_States` `0x64B084` ..
    `EnemyOp_DeathSubs` `0x64B234`; past the last, `0x64B258` on is more
    tables, then bytes ([`enemy_ai_ops.md`](enemy_ai_ops.md) §5).
  - CG: `BattleObj_SwingSubs` `0x64E074`, `BattleObj_CastSubs` `0x64E0E4`,
    `BattleObj_CastDoneSubs` `0x64E118`, `BattleObj_State12Subs` `0x64E134`
    and the state tables beside them, as D46's `BattleObj_RunState`
    (the group counts it no defect of its own;
    [`battle_obj_states.md`](battle_obj_states.md) §1).
  - CH: the five stubs over `BattleTarget_Steps` `0x64E3EC` ..
    `BattleItem_TargetSteps` `0x64E428` ([`battle_actor_copies.md`](battle_actor_copies.md) §1).
  - CI: `BattleAttackCmd_Dispatch` `0x447FD0`, `BattleItemCmd_Dispatch` and
    their state tables `0x64E44C` .. `0x64E48C` ([`battle_menu_states.md`](battle_menu_states.md) §5).
  - CJ: `FxRing_Phases` `0x65B5B8`, `StealClone_Types` `0x65AC28`,
    `FxDim_Phases` `0x65C3A0` (the last's fifth dword is 0: D66).
  - DC: `Field_LeaderStates` `0x660918`, `Field_LeaderControlSteps`
    `0x660954` and each state's own table, a whole byte each
    ([`event_leader.md`](event_leader.md) §1, §3); `Field_ActionBySet`
    `0x6609D0` by `0x90412C & 0x7F` ([`field_hidden.md`](field_hidden.md) §9).
  - DE: `PartyAction5_Form0States` `0x65FBC0` and `PartyAction5_ByForm`
    `0x51F1B0`'s `0x65FC18` ([`field_hidden.md`](field_hidden.md) §2).
  - DG: `ShopTrade_States` `0x664118` .. `ShopSell_SellSteps` `0x66417C` and
    `TitleTask_Modes` `0x667294` ([`shop_states2.md`](shop_states2.md) §2, §5).
  - DH: the eight dispatches of the field menu and the list draws, among
    them `FieldMenu_States` (by the byte `0x929F00`), `FieldMenu_TopBarSteps`
    and `MenuList_Kinds` ([`menu_lists.md`](menu_lists.md) §2).
  - DI: `Window_Handler7KindTable` `0x66B1F8` by the kind +2 and every step
    table by +3, `0x66B244` .. `0x66B560` ([`menu_draw_helpers.md`](menu_draw_helpers.md) §1).

The PSX twins were not compared for the class; where a group read one it
says so in its doc.

**Status:** latent. A bound on a `.data` table would be a divergence (the
original survives an index one past, landing on a real but wrong handler).

## D60 — An allocator's "none free" answer is never tested (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, groups CA ([`battle_phases.md`](battle_phases.md) §5), CC
([`battle_turn_steps.md`](battle_turn_steps.md) §3, §4), CF
([`enemy_ai_ops.md`](enemy_ai_ops.md) §5) and CJ
([`magic_fx_reached.md`](magic_fx_reached.md) §8). The same class as D54
(the pop-up writers) and battle_flow's magic starters.

**Established:**

- `BattleIntro_OpenWindows` `0x42E770` does not test `Window_Alloc`'s
  `0xFF`. With window `0x14` already taken at a battle's start it writes
  +2, +3, x and y into "window 255", `0x80553C + 2..+7`, inside the script
  message pool `MessagePools` (`0x803580..0x807580`). What lies there, and
  whether window `0x14` can be taken then, was not read.
- `BattleEnd_StartMemberTask` stores the member at `0x93A080 + 0x84 n` with
  `n` = `BattleTask_Create`'s al untested; with all 48 slots taken that is
  `0x9423FC`, past the image's end `0x93F000`. At start-up (the self-test's
  `VirtualQuery`, 2026-09-25) `0x940000..` is reserved but not committed, so
  the store would fault unless something commits that range first.
- `FxDiscFan_Start` `0x4C5020` (six creates) and `Steal_Start` `0x4B54F0`
  likewise: the owner goes to `0x9423FC` and, for the steal, a 0x80-byte
  copy to `0x94237C`.
- `EnemyOp_HighlightOn` stores `Sprite_SetTint`'s `0xFF` (all 32 records
  taken) in `+7` untested; `EnemyOp_HighlightPulse` then writes the pulse
  into `0x7E12F2..0x7E12F4` (`0x7E0700` + 12 x 255, past the records) and
  calls `Tint_Release(0xFF)`, which reads the record there. The PSX twin
  was not read.

Ours computes the same addresses in every case (the CA fuzz seeds the
`0xFF`).

**Status:** latent: 48 live tasks, 32 live tints or a taken window `0x14` at
those moments were not seen.

## D61 — `BattleAction_AbilityCommit`'s target block has no upper bound (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CB ([`battle_actions.md`](battle_actions.md) §3).

**Established:** for the ids 0x24, 0x25 and 0x8C the new target is
`0x42F9D0`'s answer, and `0x42F880` fills the target block with
`cmp al, 2; ja` to the enemy path and no further test (`Battle_BeginAction`
stops at 10). `0x42F9D0` answers a side - 0x40, 0x80 or 0xC0 - when the
picked id's record byte +0x10 (`0x65C4D8` + 24 id) has bit 4; of the ids in
its two tables (`0x64AEC8`, `0x64AEE8`) `0x5C`, `0x5D`, `0x60`, `0x61`,
`0x62`, `0x65` and `0x66` do, and for a member actor the answer is 0x40. The
enemy path then reads the s8 at `0x93BA52` + 0x128 * (0x40 - 3) =
`0x9400DA`, past `.data` (`0x93D6EC`) and the image (`0x93F000`), and
stores pointers to `0x93FFE8` / `0x9400EC` in `0x904B4C` / `0x904B50`.
Whether that faults depends on what is mapped there at run time (not looked
at) and on whether the three ids are used in play (not known; the owner's
to say). The PSX twin was not read. Ours computes the same addresses
(control M5 plants the bound and is refused).

**Status:** latent.

## D62 — The "turned away" store writes a member's object for any actor (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CB ([`battle_actions.md`](battle_actions.md) §3).

**Established:** `AbilityCheck` and `ItemCheck` store +1 = 2 and +2 = 0 at
ObjTrio + 0x14C * actor without testing for a member, so an enemy actor
would write inside whatever follows ObjTrio (`0x803124`..). Whether an enemy
reaches kinds 4 and 5 was not settled (`0x435AB0` stores a two-bit kind
first; the rest of it was not read). Ours does the same.

**Status:** latent.

## D63 — The round's end leaves flag 0x8000 on the third member and the eighth enemy (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CC ([`battle_turn_steps.md`](battle_turn_steps.md) §4).

**Established:** `BattleRoundEnd_CheckFaster` clears flag 0x8000 (+0x134 /
+0x114, the mark `Battle_MarkFasterSide` sets on the side that outpaces)
with `ecx = 2` over the members from `0x802E74` and `ecx = 7` over the
enemies from `0x93BA74`: members 0 and 1, enemies 0..6. The actor loops
beside it (`Battle_TickCounters`, `Battle_MarkFasterSide`) run 0..2 and
3..10. **The PSX twin `0x801D4B08` has the same bounds** (`sltiu 2` from 0,
`sltiu 0xA` from 3), so the PC inherited it. A third member or eighth enemy
once marked keeps the mark into every later round. What reads flag 0x8000
was not read, so whether that gives an extra action is open. Ours keeps
it; controls F7 and F8 plant the full loops and are refused.

**Status:** latent (its effect unknown).

## D64 — Unchecked data indices beside the dispatches (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, groups CE, CF, CI, CJ, CL, DA, DE, DH and DI (each named below).
Every one is read by ours from the same memory as the original's; none was
seen out of range.

**Established:**

- CE ([`battle_fx_tasks.md`](battle_fx_tasks.md) §3): the enemy index
  `actor - 3` of the damage popup and the actor watch is not checked, and
  the popup's party offset table `0x64DF70` is indexed by the owner's
  `+0x2C * 4 + +8` unchecked. CL ([`battle_win_states.md`](battle_win_states.md)
  §2) reads the same table, `BattleWin_MemberTargetOffsets`, by the
  character id and the side byte, unbounded (11 rows of four pairs to
  `0x64DFC7`).
- CF ([`enemy_ai_ops.md`](enemy_ai_ops.md) §5): `EnemyOp_Wait` indexes the
  0x84-byte battle-task slots by the actor byte `+5`; an actor of 0x74 or
  more would read past `.data` and fault.
- CI ([`battle_menu_states.md`](battle_menu_states.md) §5): the item list's
  category is not checked before `0x656B00[category]`; left and right keep
  it in 0..3, and at 4 the commit would write through `0x656B14[4]`, which
  is 0.
- CJ ([`magic_fx_reached.md`](magic_fx_reached.md) §8): `Steal_RateTable`
  `0x65AC20`'s row is the enemy's `+0xAA`, unbounded (8 rows); a row past 7
  reads `StealClone_Types`' dwords as rates.
- DA ([`worldmap_area.md`](worldmap_area.md) §1, §4):
  `WorldMap33_PlaceMessage` checks neither its row nor the s8
  `Cond_ByteFA`; `WorldMap33_DrawDrift` `0x4048E0` has rows of
  `WorldMap33_DriftUV` for `b` 2 and 3 only, so `b` 0, 1 and 4 read the
  bytes either side.
- DE ([`field_hidden.md`](field_hidden.md) §2): `PartyAction5_Form0Begin`
  `0x51E930` keeps an odd facing `+8` and indexes `Field_DirectionSteps`
  with it; `+8` of 8 or more reads past the 8 rows. `PartyAction5_Form0Resolve`
  `0x51EAF0` the same.
- DH ([`menu_lists.md`](menu_lists.md) §9): `FieldMenu_TopBarInput`
  `0x589B70` indexes the title ids by the cursor as a signed byte; a cursor
  of `0x80` or more (a corrupted state block) reads before the table.
- DI ([`menu_draw_helpers.md`](menu_draw_helpers.md) §1): the cursor box's
  width +0xA (words past the two of `ShopWin_CursorBoxWidths` are the low
  halves of `ShopWin_MemberStatsSteps`' pointers) and the party slot +0xA
  (into `0x904062`, then `0x66972C`).

**Status:** latent.

## D65 — A battle item is spent when its command is chosen (latent, unverified)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CI ([`battle_menu_states.md`](battle_menu_states.md) §5).

**Established:** the item is spent when the command is chosen, not when it
is used. What happens to it if the turn never comes (the battle ends first)
was not read by CI; group CC's `BattleRoundEnd_NextRound` and battle-end
steps call `Battle_ReturnQueuedItem` ([`battle_turn_steps.md`](battle_turn_steps.md)
§3), which may be the answer, but the two were not put together. Whether
the PSX does the same was not checked.

**Status:** latent; possibly not a defect at all once the return path is
read.

## D66 — Dispatch tables that hold a null entry (latent; ours aborts in three)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, groups CJ ([`magic_fx_reached.md`](magic_fx_reached.md) §8), CM
([`window_kinds.md`](window_kinds.md) §4, §7), DA
([`worldmap_area.md`](worldmap_area.md) §5) and DG
([`shop_states2.md`](shop_states2.md) §2).

**Established:** each of these calls address 0 in the original when its
index reaches the null:

- `FxDim_Phases` `0x65C3A0`: four entries, then a 0 - a phase of 4 is a call
  to null (CJ). CF's `EnemyOp_Steps` `0x64B1A0` is likewise followed by a
  null at `0x64B1D0` ([`enemy_ai_ops.md`](enemy_ai_ops.md) §2).
- `Window_Handler4Kinds` `0x597F60`: slots 2 and 3 of its stack table are
  stored from `eax` (0), so a record-handler-4 window of kind 2 or 3 calls
  address 0; no writer of such a kind is known (CM). **Ours aborts loudly.**
- `WorldMap_Records` record 6 (area 104) holds nulls at `+4`, `+8` and
  `+0x14`; kinds 0xE and 0x16 (`0x462B00`, `0x462B20`, not taken) would call
  one. Ours reads the table in place, as the original (DA).
- `TitleTask_Modes` `0x667294` [2]: a `Game_Mode` of 2 while task 0 runs
  the title. **Ours aborts** (rule 4) (DG).

**Status:** latent.

## D67 — The steal does not check that its target is an enemy (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CJ ([`magic_fx_reached.md`](magic_fx_reached.md) §8).

**Established:** a target of 0..2 reads (and on a theft writes) "enemy"
fields below the enemy records - in the task slots, or at `0x93B8E0` for 2.
Whether the game ever lets the player steal from an ally is not known (a
game fact for the owner). Ours does the same.

**Status:** latent.

## D68 — Divides with no zero test: the battle gauges' max HP and the animated cell's period (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, groups CL ([`battle_win_states.md`](battle_win_states.md) §4),
CM ([`window_kinds.md`](window_kinds.md) §6, §7) and DD
([`map_field_objects.md`](map_field_objects.md) §5).

**Established:**

- `BattleWin_EnemyGaugeOpen` `0x597400` and `BattleWin_MemberGaugeOpen`
  `0x5976D0` divide by the HP top with `idiv` unchecked (the AP top is
  checked): a top HP of 0 opening a gauge window raises a divide fault.
  **Ours aborts with a message instead** (CL).
- The enemy HP gauge's `Window_HpGaugeTrack` `0x597A80` and its drain state
  divide by the max HP without a test; a max of 0 raises #DE. Never seeded
  (CM).
- `MapCell_DrawAnimated` `0x5712E0` divides by its period byte unchecked; a
  period of 0 is a divide fault on both sides (DD).

**Status:** latent.

## D69 — An odd arm growth on the battle cross wraps (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CL ([`battle_win_states.md`](battle_win_states.md) §4).

**Established:** `BattleWin_CrossFrame` `0x597160` lowers an unselected
arm's growth by 2 while it is not 0, so an odd growth goes 1 -> 0xFF (then
0xFD ..). Growths are raised by 2 from 0 and lowered by 1 only in
`BattleWin_CrossGrow` `0x5970C0`, which runs until arm 0's is 0 - so an odd
value in arms 1..6 after the grow is possible when they started unequal.
Ours keeps it.

**Status:** latent.

## D70 — The member's target banner does not put its place back (latent, does not show)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CL ([`battle_win_states.md`](battle_win_states.md) §4).

**Established:** the enemy's target banner (`BattleWin_EnemyTargetFrame`
`0x597510`) restores the window's home place through
`Window_RestoreAndBack`; the member's (`BattleWin_MemberTargetFrame`
`0x5978B0`) does not. State 1 of kind 5 draws nothing, so it does not show.
Ours keeps it.

**Status:** latent.

## D71 — `BattleWin_BannerSlideOut` clears the last-visited banner's mask bit (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CM ([`window_kinds.md`](window_kinds.md) §7).

**Established:** `BattleWin_BannerSlideOut` `0x597D10` clears the mask bits
of the banner entry `0x93B8C0` points at - whichever entry
`BattleBanner_Dispatch` visited last - not of the entry this window shows
(record +0xA). With more than one banner live the wrong kind's bit may be
cleared. Whether it matters depends on who reads `0x904AE9` after the window
task runs in a frame (unread). The combat route did not reach the slide-out
(fuzz only).

**Status:** latent.

## D72 — The banner and message slides test for equality and can miss their stop (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CM ([`window_kinds.md`](window_kinds.md) §7).

**Established:** the four slides - `BattleWin_BannerSlideIn` `0x597CF0`,
`BattleWin_BannerSlideOut` `0x597D10`, `BattleWin_MessageSlideIn`
`0x597DC0`, `BattleWin_MessageSlideOut` `0x597E60` - compare y with 0x12 or
0xFFEA for *equality*, stepping 8. A window placed at a y not congruent to
0x12 modulo 8 never arrives and wraps round the 16-bit word forever. The
creators are not read.

**Status:** latent.

## D73 — Off a world map, `WorldMap_RecordIndex`'s 11 runs another table's handlers (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DA ([`worldmap_area.md`](worldmap_area.md) §5).

**Established:** `WorldMap_RecordIndex` `0x462A90` answers 11 when no record
of `WorldMap_Records` `0x653910` matches the area. The three kind handlers
then read the dwords after the eleventh record, which are the table
`0x653A44` of `0x462BA0` (`0x462BC0`, `0x462BF0`, `0x462E70`, ...). So an
effect of kind 0, 0x58 or 0x18/2..3 run outside a world map calls
`0x462BA0`'s state entries. Which effects reach that is unread. Ours reads
the same tables in place.

**Status:** latent.

## D74 — Searches with no end test (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, groups DA ([`worldmap_area.md`](worldmap_area.md) §7, §10), DC
([`event_leader.md`](event_leader.md) §3, §6) and DD
([`map_field_objects.md`](map_field_objects.md) §5).

**Established:**

- `WorldMap33_PlateShow`'s search for the place has no bound: a kind-1 cell
  (0xA1) at a place not among `WorldMap33_PlateAnims`' six would read on
  through `.data` (DA).
- `Field_ExitFromCell`'s search has no end test: a map whose `0xA0` cell has
  no record in the area's list walks on through memory - a hang or a fault
  (DC).
- `MapCell_DrawAnimated` `0x5712E0` scans its thresholds without a bound
  (DD).

Ours walks the same memory in each; the fuzzes always plant a match.

**Status:** latent: each needs area data the shipped game was not found to
have (not scanned).

## D75 — Area 29's "none kept" exit is unreachable with its table (latent, dead)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DA ([`worldmap_area.md`](worldmap_area.md) §1, §10).

**Established:** area 29's placement keeps one of the first eight
`Sprite_Objects` records by a weighted roll over `Area29_Weights`
`0x5EE268`. The weights sum to exactly 64, so the loop's "none kept" exit
(index 8) is never taken with the shipped table. It matters only if the
table changes (a data edit in this project).

**Status:** latent (dead with the shipped data).

## D76 — The drift layer draws when the leader is near in x *or* z (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DA ([`worldmap_area.md`](worldmap_area.md) §4).

**Established:** `WorldMap33_DrawDrift` `0x4048E0` draws only when the leader
is within 25 cells of the layer's position in x **or** in z. Whether an AND
was meant is open. What the layer looks like is unread; the PSX twin was
not read.

**Status:** latent; possibly intended.

## D77 — `Field_PassageOpen`'s kind-2 path reads a stack buffer it never initialises (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DC ([`event_leader.md`](event_leader.md) §3, §6 items 1
and 2, merged here: one cause).

**Established:** the kind-2 path reads the word `+0x88` of a stack buffer it
never initialises and opens that script message (`& 0xFFF`) unless it is
0xFFFF. Unless the event script always writes `+0x88` through
`Field_ActiveMember`, a kind-2 passage can open a message chosen by stack
garbage. The same path leaves `Field_ActiveMember` pointing at the dead
buffer, so whatever next reads it before it is set again reads a dead stack
frame. Which scripts write it, and the pointer's other users, were not read;
no kind-2 passage is on the routes as far as was checked.

**Status:** latent.

## D78 — A blocked spot zeroes a member's `+9` (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DC ([`event_leader.md`](event_leader.md) §3, §6).

**Established:** `Field_SpotFree` zeroes `Sprite_Current +9` and restores it
only when the spot is free; the swap calls it for each member, so a member
whose spot is blocked loses its `+9`. What `+9` holds for a member at that
point was not established. Ours keeps it.

**Status:** latent.

## D79 — `EventScript_SkipSwitch` hangs on F2, F3 or FB..FF (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DD ([`map_field_objects.md`](map_field_objects.md) §5).

**Established:** `EventScript_SkipSwitch` `0x579CF0`'s jump table
`EventScript_SkipSwitchCases` `0x579D40` sends F2 and F3 back to the byte
they are on, and `cmp ecx, 0xA; ja` sends FB..FF there too; the loop never
advances. A switch body being skipped with one of those at an op position
freezes the game task. The PSX `0x801A584C` was not read; which scripts
have them is unread.

**Status:** latent.

## D80 — `MapCell_DrawUprights` indexes its tables past their nine kinds (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DD ([`map_field_objects.md`](map_field_objects.md) §5).

**Established:** in `MapCell_DrawUprights` `0x570210`, kinds with a low
nibble of 0xA..0xC (and 0x3A..0x3C) read counts 0 and draw nothing;
0xD..0xF (and 0x3D, 0x3E) read `MapCell_UprightOffsetsX`' bytes 9, 0xFF and
0xBE as counts and would draw 9, 255 or 190 pairs from offsets well past
the tables. Whether any area's cell runs use those kinds is unread (a scan
of the area files would answer it).

**Status:** latent.

## D81 — `MapCell_PatchThenStep` applies its patch twice (latent, may be deliberate)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DD ([`map_field_objects.md`](map_field_objects.md) §5).

**Established:** `MapCell_PatchThenStep` `0x571090` (kind 0x24) rewrites the
record's kind to 0x25 and then applies patch-list entry
`(record & 0xFFFF) + AreaMap_PatchBase` through `AreaMap_ApplyPatch`;
`MapCell_PatchThenStop` `0x5710D0` (kind 0x25) rewrites it to 0x23 (a bare
`ret`) and applies the same patch. So an 0x24 record applies its patch on
two draws of the cell, and never after. Harmless if the patch is
idempotent; may be deliberate.

**Status:** latent.

## D82 — A zenny cell dug with every effect object busy is cleared for nothing (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DE ([`field_hidden.md`](field_hidden.md) §7).

**Established:** in `Field_CellPickup` `0x51EBD0`'s `0xF2` path the zenny
roll happens only once `Effect_FindFree` answers a slot. With all 20
objects taken the path skips the roll and the `+0xB` store but still clears
the cell (`0x5728D0`) and answers 1, so the chance is lost for good. The
`0xF8` item path asks for no effect and cannot lose its item this way. No
route reaches 20 live effect objects at a dig.

**Status:** latent.

## D83 — The encounter row weights are summed in a byte (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DE ([`field_hidden.md`](field_hidden.md) §7).

**Established:** `Encounter_PickRow` `0x592570` picks a row by
`(Rand & 0xF) + 1` against a byte running sum. Weights over 255 in total
wrap, and a later row can then be picked for a roll the earlier rows should
have covered. The rows come from the area's data; whether any area's
weights sum past 16, let alone 255, was not checked.

**Status:** latent.

## D84 — A shop count step of one plays its sound twice (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DG ([`shop_states2.md`](shop_states2.md) §5).

**Established:** `ShopTrade_BuyCount` and `ShopTrade_SellCount` compare the
count with the one they started with after the one-step clamp *and again*
after the ten-step clamp, so a single up or down plays the cursor sound
(`0x100` / `0x101`) twice in the frame. Two identical effects started
together are unlikely to be heard as two.

**Status:** latent.

## D85 — `FieldMenu_Open` copies the party by `Party_Count` without a bound (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DH ([`menu_lists.md`](menu_lists.md) §9).

**Established:** `FieldMenu_Open` `0x589990` copies one party pair per
`Party_Count` without a bound: a count above 3 writes the saved reserve's
bytes over the saved party's (the two lists are three bytes apart). The
count is at most 3 in play.

**Status:** latent.

## D86 — The field menu's screen title is centred for 12-pixel glyphs (candidate under an overlay language)

**Seen:** not seen as such; found by reading the code while taking it over,
2026-09-25, group DH ([`menu_lists.md`](menu_lists.md) §3, §9). A candidate
for the owner's "the screen title sits left of centre"
([`menu-screens.md`](menu-screens.md) section 3 item 3).

**Established:** `MenuList_TitleBox` `0x599FA0` draws the top bar's title
(the system text of `FieldMenu_TitleIds` `0x6672E4`, by the cursor) in a box
`0x48` wide, starting the text at x + `0x25` - 6 n, n the `Text_CharCount`
of the string. That is right for the Chinese it was written for and wrong
for any narrower font - the case [`dialogue-localisation.md`](dialogue-localisation.md)
section 6 item 1a predicts. Ours draws what the original draws; nothing
re-centred.

**Status:** candidate (not a fault of the original's own text).

## D87 — `ShopWin_MoneySlideUp` does not slide (candidate)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DI ([`menu_draw_helpers.md`](menu_draw_helpers.md) §5).

**Established:** `ShopWin_MoneySlideUp` `0x59B390` is `0x59A3A0` (the
slide-up to -0x14) byte for byte but for the branch: `jle` where `0x59A3A0`
has `jge`. So when the money box is sent up, from any y above -4 it jumps to
-0x14 in one frame and stops; from -4 or less it moves up 0x10 a frame and
the step never ends (the word wraps round to positive, where it snaps).
Which case the shop shows, and whether the player can see the difference,
is not checked (the owner's eye after the merge). Ours keeps it; negative
control 14 plants the fix and is refused.

**Status:** candidate.

## D88 — `BattleMenuWin_ItemListSlideRight` tests 0x53 and stores 0x52 (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group DI ([`menu_draw_helpers.md`](menu_draw_helpers.md) §5).

**Established:** `BattleMenuWin_ItemListSlideRight` `0x59CB60` tests 0x53
and stores 0x52, so an x landing exactly on 0x53 sits there a frame and goes
on to 0x73, then 0x52; the window rests at 0x52. `0x59CB90` (step 3, in no
group) has the same pair. Harmless; ours keeps it.

**Status:** latent.
