# Known defects of the port, as observed

**Status:** IN PROGRESS (2026-09-23 — twenty-nine entries, D19, D20 and D29 unused; D4 fixed by DIV-0004 and confirmed in game; D5 fixed short term by DIV-0022; D6, D7, D9, D11 and D12..D16 latent; D8 and D10 unchecked in game; D17, the glyph sampling, fixed by DIV-0025 (confirmed in game 2026-09-23); D18, D21..D25, D27, D28, D30, D31 and D32 latent; D26, the music fades, fixed by DIV-0028 (confirmed in game 2026-09-23))

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

**Fixed as DIV-0025 (2026-09-22, group N; not yet seen in game)** - [`glyph-draw.md`](glyph-draw.md) §6. The proposal as it stood:

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
## D-NEW-V2 — A jump's speed index can count down to a division by zero (latent)

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
## D-NEW-W1 — Inventory_Add searches the 32-byte key-item list 128 long (latent)

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
null count list - see D-NEW-W2 - with a counting twin).

## D-NEW-W2 — Inventory_Count of a key item that is held reads address 0 + i (latent)

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

## D-NEW-W3 — Menu_DrawIcon's CLUT table has 21 entries and no bound (latent)

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
