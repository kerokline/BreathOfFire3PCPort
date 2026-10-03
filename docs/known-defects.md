# Known defects of the port, as observed

**Status:** IN PROGRESS (2026-10-01 — one hundred and ninety-three entries, D1..D196 with D19, D20 and D29 unused; D1 fixed by DIV-0010 (confirmed off a capture 2026-09-21); D2 fixed by DIV-0039; D3 moot since DIV-0031 / DIV-0035 (recurs only under BOF3X_ORIGINAL); D4 fixed by DIV-0004 and confirmed in game; D5 fixed by DIV-0022 and DIV-0047; D6, D7, D9, D11 and D12..D16 latent; D8 and D10 unchecked in game; D17 fixed by DIV-0025 and D26 by DIV-0028 (both confirmed in game 2026-09-23; D17 recurred at scale 4 and DIV-0025 was amended 2026-09-27, confirmed in game the same day); D18, D21..D25, D27, D28 and D30..D40 latent (D38 a candidate); D41 fixed in the backend by DIV-0044 (the owner's look owed); D42 a port change, kept; D43..D57 latent, from the seventh round (D43 and D51 candidates; D44, D47, D53, D56 PC only); D58 fixed under an overlay language by DIV-0051; D59..D88 latent, from the eighth round's reading, 2026-09-25 (D86 fixed by DIV-0058 on 2026-09-27, D87 a candidate; D59, D66 and D68 abort in ours where the original would crash); D89..D132 latent, from the ninth round's spell overlays and scheduler, 2026-09-25..27 (D102, D103 and D129 candidates; D89's stack tables, D91, D92 in part, D94 in part, D97 in part, D100 in part, D104, D106, D107 in part and D132 abort in ours where the original would crash or run wild; the rest faithful); D133..D161 latent, from the tenth round's chapter banks and area blocks, 2026-09-27..28 (D133 in most groups, D135 in some, D136's `Area_Descriptors` reads, D137 and D150 abort in ours where the original would run past, fault or crash; D138 and D145 PC only; the rest faithful); D162..D173 latent, from the eleventh round's boss band, 2026-09-28 (D166 a candidate, D167 a question for the owner, D168 owes a PSX comparison; D162, D163, D164 and D170 abort in ours where the original would jump wild, write past the pool or near address 0; the rest faithful); D174 fixed 2026-09-29; D175..D196 latent, from the twelfth round's battle engine and field remainder, numbered 2026-10-01 from the fourteen group docs (D181 a crash candidate, D184, D187 and D194 the owner's questions; D175 in most groups, D176 in eleven of twelve places, D177, D183, D186, D190 and parts of D178 and D196 abort in ours where the original would jump wild, write past the pool, fault, hang or read off the stack; D192 passes zeros for the original's frame; the rest faithful))

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

The draw is `BattleResultWin_DrawDrops` `0x5984B0`, ours since round twelve
(group BE7, 2026-09-29, [`battle_e7.md`](battle_e7.md) §7): it places each
drop by its index and skips a word of 0 there, unchanged (D196).

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

`Battle_SetApPopup` `0x453EB0`, the AP pop-up's writer, is the same and is
kept the same in ours (group BE6, 2026-09-29; D176, where the other eleven
places of round twelve abort).

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

**Status:** ~~candidate (not a fault of the original's own text).~~
**Fixed as DIV-0058, 2026-09-27**, after the owner showed it again
("Ability", "Tactics" left of centre): under a Latin overlay the title is
centred on the width the pen covers.

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

## D89 — The spell overlays' dispatch indices are never checked (latent; ours aborts past a stack table)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, by every group of the spell round (the forty-three docs
below; the round's summaries in [`takeover-queue-round9.md`](takeover-queue-round9.md)
§7, §11, §12). D59's class, in the overlays behind `Magic_Rows`; one entry
for the class, the groups' docs carry the tables.

**Established:** each task, child and record dispatcher indexes its table by
a phase byte (`+1`, `+2`, `+3`) with no bound. Past a stack-built table the
original calls through its own return address and the caller's frame; past
a `.data` table it runs the next table's entries - on the PC often another
overlay's code, since the overlays are linked into one image (D104 is the
one case found reachable by reading). **Ours aborts past every stack
table** (the precedent, [`magic_fx_reached.md`](magic_fx_reached.md) §3).
For the `.data` tables the docs' wording differs: C2, S11, S19 and S20 say
ours reads them in place as the original does (D59's rule); most other
groups say "ours aborts" of their stack and `.data` tables together. Which
each group's code does for a `.data` index was not re-read for this entry.
No index past a table was seen or found reachable, except Blitz's (D104).

Tables per group, as the docs count them (stack / `.data`):

- L ([`magic_lib.md`](magic_lib.md) §6): the popup tasks' stack tables (a
  phase past 3).
- E ([`magic_engine.md`](magic_engine.md) §6, E3): every dispatcher;
  `HeadCrackerRock_Task` has a one-entry table.
- C1 ([`magic_c1.md`](magic_c1.md) §12): every task's stack table and every
  `.data` dispatcher (not counted).
- C2 ([`magic_c2.md`](magic_c2.md) §8): every `Step`'s stack table, and
  `.data` tables that overlap (`HolocaustBeam_Types` entry 1 is
  `HolocaustBeam_Phases` entry 0; `HolocaustSpark_Phases` entry 3 is
  MAGIC130 code).
- C3 ([`magic_c3.md`](magic_c3.md) §8): four stack tables, the child kinds
  by `+1`, the wash's and the flash's `.data` tables by `+2`.
- S01 4 / 2 (`JumpChild_Task`'s one-entry table is followed by other data);
  S02 6 / 4; S03 7 / 5; S04 6 / 5; S05 6 stack by `+1`, a ten-entry table by
  `+2`, a one-entry `.data` table (past it, MAGIC008's); S06 9 / 2; S07 9 / 8;
  S08 7 / 7; S09 7 / 8; S10 9 / 9; S11 4 / 6; S12 2 / 7; S13 the task's two-
  and the child's five-entry stack tables / 4 (a child `+1` of 1 would run
  `SuddenDeathMote_Kinds[0]`); S14 7 stack, the streak record's by `+1` / 4;
  S15 5 / 10.
- S16 ([`magic_s16.md`](magic_s16.md) §6): the task's (6 or 2), the child's
  and the update's stack tables; `.data` tables that overlap (a child `+1`
  of 1 runs the sparkle update, 2 runs MAGIC075's code).
- S17 4 (`Purify_Task` 3, `Revive_Task` 6, `ReviveMote_Task` 3,
  `Leech_Task` 2) / 5 (`ReviveHalo_Phases` past 3 reads the mote-count
  bytes as a pointer); S18 2 / 8; S19 2 / 8; S20 3 / 13; S21 3 / 7; S22 ten
  in all (a one-entry task table's index 1 reads the next overlay's table);
  S23 every stack table (not counted).
- S24 ([`magic_s24.md`](magic_s24.md) §6): every table (not counted);
  `Fx105_ChildPhases` has one entry (a `+1` of 1 jumps to the words of
  `Fx105_OrbAngles`).
- S25 ([`magic_s25.md`](magic_s25.md) §6): task tables of 2, 2, 4 and 6;
  child tables of 4 and 6; `SpellRagnarok_ChildKinds` (3; phase 3 jumps into
  `SpellRagnarok_SpritePhases`' first entry, MAGIC130's code); sprite, ring
  and spark tables of 5, 4 and 2.
- S26 4 / 7 (back to back: `Magic115_MotePhases` runs on into MAGIC118's);
  S27 every table (`DragonBreathChild_Kinds` has one entry; index 1 reaches
  `DragonBreathBeam_Aim`); S28 3 / 7 (`BreathBeam_Phases` index 1 runs
  `BreathBeam_Aim`); S29 sixteen tables; S30 2 / 11; S31 10 / 8; S32 2 / 5;
  S33 8 / 7; S34 6 / 8; S35 6 / 6; S36 3 / 8; S37 fourteen tables; S38 3 / 13.
- Steal ([`magic_steal.md`](magic_steal.md) §6): the task's stack table -
  already D59's, as CJ found it in Pilfer's copy.

**Status:** latent. Blitz's is the only one found reachable (D104).

## D90 — The spell starts never test `BattleTask_Create`'s "none free" (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, by 34 of the round's groups (below). D60's class.

**Established:** with all 48 battle task slots taken, `BattleTask_Create`
answers `0xFF`, and the creators below write the child's fields (and in
several a 0x80-byte copy of an actor record) into "slot 255",
`0x93A000 + 255 x 0x84` = `0x94237C` (its owner word `+0x80` is the
`0x9423FC` most docs give), past the image's end `0x93F000` - an access
violation unless something is mapped there (D60: at start-up `0x940000..` is
reserved, not committed). **Ours writes the same addresses** in every group;
the fuzzes' recorders answer 0..47, since `0xFF` would fault both sides.
Creators per group, with the docs' call counts:

- L: `MagicFx_BuffPopup` `0x4FB790`. E: `HeadCracker_Drop` `0x43FDC0` (and a
  rock counted that never lands: D103).
- C1: Pentagram's creates (four counted children in phases 1..3, six sprites
  in phase 4; D99). C2: `BoneDance_Cast`, `RottenBreath_Emit`,
  `RottenBreathCloud_Start`, `Holocaust_Start` (`BoneDance_Spawn` tests it).
  C3: `Magic002_Start` `0x499DB0`, `Magic111_Start` `0x4D6140` (a 0x80-byte
  copy, three times).
- S01: both starts. S02: `SuperCombo_SpawnDash`, `_SpawnImages` (4),
  `ElemStrike_Start` (2). S03: `MindSword_Start`, `MindSwordBlade_Burst` (9),
  `Chlorine_Start` (with its copy), `Chlorine_Release`, `Blitz_Start`. S04:
  `Snap_Start`, `Snap_Buff`, `Charge_Start` (5), `AirRaid_Start` (3),
  `FlyingKick_Start` (the images copy 0x80 bytes). S05: all six starts. S06:
  `Magic008_Start`, `Magic008_Apply`, `Magic020_Start` (5), `Magic020_Darken`
  (8). S07: `Bonebreak_Start`, `WarShout_Start` and `_Rally`, `Focus_Start`,
  `Enlighten_Start` and `_Apply`. S08: `Berserk_Start` (9), `Counter_Start`,
  `Ward_Fade`, `EvilEye_Start` (2) (`EvilEyeBeam_Trail` tests it).
- S09: `BoneDart_Start` (2), `ElemBreath_Start` (2), `DreamBreath_Start` (12),
  `Pollen_Start` (10); `BoneDartShadow_Follow` / `_Fade` then index the task
  slots by the dart's unchecked slot. S10: `Ovum_Spawn`, `Lavaburst_Start`
  (8), `Howling_Start` (with a copy), `Ebonfire_Start` (6), `Sacrifice_Start`
  (with a copy), `SacrificeActor_Split` (2). S11: `Sanctuary_Start`,
  `Tornado_Start`. S12: `Identify_Start`, `Identify_WaitOpen`,
  `Celerity_Start`, `Celerity_Apply`. S13: `SuddenDeath_Spawn`. S14:
  `Weretiger_FocusActor`, `_Burst`, `_Copy`, `WeretigerCopy_Spawn` (4),
  `Tsunami_Start`, `Tsunami_Rings`. S15: `Chill_Start` (3),
  `Chill_SpawnMarks`, `Foretell_Start`, `Influence_Start` (6),
  `Influence_SpawnMarks`.
- S20: `Magic087_Start`, `Magic088_Start`, `Magic088_Apply`,
  `Magic092_Start`. S23: the four starts. S26: `Magic114_Launch`,
  `_TrailSpawn` (2), `Magic115_Start`, `Magic115_Orbit`. S27: `Burn_Start`,
  `WhelpBreath_Start`, `WhelpBreath_WaitBeam`, `DragonBreath_Start`. S28:
  `Firebreath_Start` `0x4DD8E0`. S29: `DivineBreath_Start`,
  `ShadowBreath_Start`, `ShadowSeeker_Burst` `0x4E28C0`.
- S31: `DoomBreath_Start`, `Corona_Start` (3), `MainCannonShell_Fly`,
  `ThunderClap_Start` (only `MainCannon_Fire` tests it). S32:
  `WallOfFire_Start` `0x4E91C0`. S33: `Accession_ActorScript`, the
  controller's three, `AccessionOrb_Emit`, `MightyChop_Start`,
  `MightyChop_Throw`. S34: `Magic159_Start`, `TimedBlow_Start` `0x4EE310`.
  S35: `LastResort_Start` (9), `Benediction_Spawn`. S36: `MagicBall_Start`
  (9), `Intimidate_Start` (2), `IntimidateTrail_Fly`, `AuraBreath_Start`;
  `MagicBallOrb_Fly` then reads the slot its `+4` names, so every orb would
  read past the image too. S37: `GeoBreath_Start`, `Combustion_Start`,
  `CombustionSprite_Fall` (8), `CombustionSprite_Flash`. S38:
  `Tempest_Start` `0x4F86C0`, `Magic225_Start` `0x4F8FD0`,
  `MeteorStrike_Start` `0x4F9E60` (`Magic225_Apply` `0x4F9130` tests it,
  after applying the stat: a full table loses the popup, not the buff).

The docs of S16..S19, S21, S22, S24, S25 and S30 name no unchecked
`BattleTask_Create`. The overlays' own pools are D93.

**Status:** latent: 48 live battle tasks at a cast were not seen.

## D91 — Spell effects divide by the live-target count (latent; ours aborts at 0)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups L, S09, S11, S15, S21 and S22 (below).

**Established:** each centres or scales an effect by an `idiv` by the number
of live actors on a side, with no zero test: with every actor of that side
out the original raises an integer-divide fault, a crash. **Ours aborts with
a message** at the same point. **The owner, 2026-09-26: no DIVERGENCE
entry** - the original faults there, so no reachable case behaves better in
it, and normal play is expected never to cast at an empty side
([`takeover-queue-round9.md`](takeover-queue-round9.md) §6).

- `MagicFx_CenterOnSide` `0x4FC0E0` (L, [`magic_lib.md`](magic_lib.md) §6).
- `DreamBreath_TargetHeight` `0x4AB330` (S09, [`magic_s09.md`](magic_s09.md) §8).
- `Tornado_AverageHeight` `0x4B0CB0`, the opposite side (S11,
  [`magic_s11.md`](magic_s11.md) §8).
- `Foretell_Read` `0x4B74C0` (S15, [`magic_s15.md`](magic_s15.md) §8): by
  counts that can be 0 (no present member without `+0x91` bit 0x40, no
  enemy in) and by record words that can be 0 (a counted member's `+0xA0`,
  an enemy's `+0xB0`).
- `Inferno_TargetCentre` `0x4C7320` (S21, [`magic_s21.md`](magic_s21.md) §5).
- `Blizzard_CenterOnTargets` `0x4C9AF0` (S22, [`magic_s22.md`](magic_s22.md) §8).

**Status:** latent. Unmeasured: a trace of a fight where the last enemy
dies to a multi-target spell would settle whether any cast meets it.

## D92 — Spell loops bounded only by a byte or a random walk (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups C2, S02, S07, S08, S12, S19, S28 and S36 (below).

**Established:** a loop or fade whose end depends on a byte reaching a
value, or on a random walk falling below a floor:

- **Never ends:**
  - `EnlightenRays_DrawLines` `0x4A5FC0` (S07, [`magic_s07.md`](magic_s07.md)
    §7): a byte step (`add bl, 8`) compared with the dword first + 0x20; for
    a first step of 0xE0 or more the loop commits lines forever. Its first
    step runs 0..0x30 in play. Kept in ours.
  - `CounterMark_DrawSpokes` `0x4A7220` (S08, [`magic_s08.md`](magic_s08.md)
    §8): the same for a first angle of 0xE0 or more. **Ours aborts.**
  - `HolocaustBeam_Draw` `0x4E3740` (C2, [`magic_c2.md`](magic_c2.md) §8): a
    byte counter against `+0xA` + 1 as a dword; at `+0xA` 0xFF it wraps and
    loops forever. `HolocaustBeam_Grow` stops `+0xA` at 0x10. Kept.
  - `ThunderBolt_Draw` `0x4DFAE0` (S28, [`magic_s28.md`](magic_s28.md) §8):
    the same at `+0xA` 0xFF; its steps keep `+0xA` in 0..0x14. Kept.
  - `ElemStrike_Fade` `0x49BE30` (S02, [`magic_s02.md`](magic_s02.md) §8):
    an odd tint byte steps 1 -> 0xFF and never reaches 0. All 36 bytes of the
    kind table are even (read 2026-09-26). Kept.
  - `MagicBall_DrawSpark` `0x4F2F20` / `_DrawSparkShort` `0x4F3140` (S36,
    [`magic_s36.md`](magic_s36.md) §8): loop until a random walk (a radius
    stepped by `sin(wave) x (Rand & mask) >> 12` over a 32-step wave whose
    sum is about 0) falls below its floor - unbounded in principle, one more
    line drawn a step. Faithful.
- **Wraps, then ends late:**
  - `Barrier_Fade` `0x4C29C0` (S19, [`magic_s19.md`](magic_s19.md) §8):
    tint levels starting at 0 wrap to 0xFF and the fade runs 255 more
    frames; `BattleFx_Brighten` raises them first.
  - `MagicFx_CountDown2Release` `0x4B18B0` (S12, [`magic_s12.md`](magic_s12.md)
    §8): an odd `+9` wraps and ends 128 frames later; Identify's disc leaves
    it even.

`LastResortBeam_DrawSparks` is D109.

**Status:** latent: by reading, the games' own values stay inside in each.

## D93 — Spell pools' "none free" writes record 255 inside the image (latent, silent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27; first S09 ([`magic_s09.md`](magic_s09.md) §8, round nine
§11), then C2, S02, S07, S11, S20, S21, S24, S28 and S29.

**Established:** an overlay's own record pool answers `0xFF` when full, and
these callers write "record 255" untested. Unlike D90 the record lies inside
the image (in `.data`'s zero-filled tail, which some docs call `.bss`), so
the original **does not fault: it silently overwrites** whatever lives
there. Ours writes the same bytes.

- S09: `ElemBreathEmitter_Emit` `0x4AA260` with `ElemBreathMote_Alloc`
  `0x4AABE0`: `0x6861BC` (+0x80, +0xA, +0xB). By reading far fewer than the
  48 records are live at once.
- C2: `RottenBreathCloud_Emit` `0x4BFBD0` and `HolocaustBeam_Emit`
  `0x4E3680`: `0x696534` and `0x6AB0B4`, in other overlays' storage (about
  21 of 48 and 25 of 64 live by reading).
- S02: `SuperCombo_SpawnHits` `0x49B390` with `SuperComboHit_Alloc`:
  `0x67ED3C`; not reached (the pool is emptied first, the count stops at
  0x20).
- S07: `BonebreakChild_Burst` `0x4A3D00` with `BonebreakMote_Alloc`:
  `0x681F3C`; unreachable (12 of 64, the pool cleared first).
- S11: both pools' callers: `0x688AFC`, `0x689D9C`; not reachable as written.
- S20: `Magic087_ChildSpawn` `0x4C3740` and `_ChildRing` `0x4C3870`: pool A's
  record 255 lies inside pool B (`Magic092_ChildSpawn` tests it).
- S21: `Inferno_Start` `0x4C6380` with `FlamePool_Alloc`: past the pool's 32;
  only a second flame effect at the same time could fill it.
- S24: `Fx106_Start` `0x4D24C0` with `Fx106_SparkAlloc`: `0x1FE0` bytes past
  the pool's start, beyond its `0xC00`; the start empties the pool and takes
  exactly 96.
- S28: `Icebreath_Start` `0x4DE980` (90 of 90) and `Thunderbreath_Start`
  `0x4DF580` (32 of 64): cannot fail as used (the other side is D113).
- S29: `DivineBurst_Shrink` `0x4E1110` and `ShadowSeeker_Burst` `0x4E28C0`:
  `0x6A7FAC` and `0x6A3D10`; one cast fills 64 once.

Not defects, noted by the docs: S10's `Lavaburst_PoolAlloc`, S13's
`SuddenDeathMote_Alloc`, S35's and S37's pools are tested by their callers
(S13's skip has a consequence: D123); S16's allocators cannot answer
128..254; S32's eight spark cells are exactly enough.

**Status:** latent; a single cast does not fill any of them by reading.

## D94 — Spell draws divide by a table byte or a record byte with no zero test (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, groups L, S23, S27 and S28.

**Established:**

- The CLUT helpers (L, [`magic_lib.md`](magic_lib.md) §6): the row is
  `index / SpriteClut_Divisors[kind]`, which holds 0 for kinds 5..7 (a
  divide fault), and kinds 8 and up read past the tables; `+0x28` is
  unchecked. What kinds sprites carry is not measured; the doc does not say
  what ours does at a 0.
- `Quake_Heave` `0x4CE500` (S23, [`magic_s23.md`](magic_s23.md) §5): the
  divisor row is `(facing >> 1) * 15` into `Quake_Divisors` (two rows); a
  facing of 4 or 5 reads a 0 byte of padding and `idiv` faults when a cell of
  the block lies inside the area. **Ours stops with a `Fatal` naming the
  facing.** `Quake_FacingOffsets` is read past its four pairs the same way.
- `WhelpBreathBeam_Draw` `0x4DB0D0` (S27, [`magic_s27.md`](magic_s27.md) §8)
  divides by `+0xB` (signed); by reading never 0 while it draws. **Ours
  aborts.**
- `BreathBeam_DrawTextured` `0x4DDCE0` / `_DrawGlow` `0x4DE370` (S28,
  [`magic_s28.md`](magic_s28.md) §8): four `idiv` by `+0xB` a step. MAGIC122's
  steps keep it at 1 or more; the five other overlays that drive
  `BreathBeam_Run` were not read for it. **Ours aborts.**

**Status:** latent.

## D95 — Spell scans with no end test (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S08, S23 and S36. D74's class.

**Established:**

- EvilEye's trail draws (S08, [`magic_s08.md`](magic_s08.md) §8) scan for the
  first point not erased with no bound, relying on a point before the end
  not being 0xFFFF; the first segment's heading reads the point after it
  even when that lies past `+0xA`.
- The ribbons' gap scan (S36, [`magic_s36.md`](magic_s36.md) §8) walks from
  point `+0xB` while the x word is 0xFFFF with no limit; a trail all gaps
  runs on into `Magic213Mote_Pool` and beyond. It only reads. Whether `_Fade`
  can make a trail all gaps before it is freed is not measured.
- Quake's cell walks (S23, [`magic_s23.md`](magic_s23.md) §5): a cell run
  that does not land on its end runs away; the area data decides.

**Status:** latent.

## D96 — Battle actor records indexed by the actor byte, unchecked (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, by most of the round's groups; the one wave five named,
S34's `TimedBlow_Start` ([`magic_s34.md`](magic_s34.md) §8), stands for the
class.

**Established:** an effect indexes the enemy records by the acting actor
byte - 3, or the party records by the actor byte, or either by a child's
byte naming an actor, with no bound. An enemy index above 10 reads past the
eight enemy records; a party actor (0..2) taken as an enemy reads a
"record" 0x128..0x378 bytes below `0x93B960`, among the battle task slots;
an enemy actor taken as a party member reads (and in some writes) past the
three party records. Several copy 0x80 bytes from there into a new task
slot. Ours reads and writes the same addresses.

- `TimedBlow_Start` `0x4EE310` (S34): enemy records by actor - 3, above 10,
  with a 0x80-byte copy.
- **Writes:** `RestoreForm_Start` `0x452620` (E, E4: an enemy actor writes
  +1..+4 past the three members, inside the image); MAGIC064 (S14: reads,
  copies and writes `Weretiger_End`'s +0x134 / +0x142 past the party
  records for an enemy actor); `Accession_*` (S33: D108).
- **Party actor read as an enemy:** S01's row 1 cast by a party member
  (`0x435A70` animates a task-slot "record" as the current enemy);
  `HolocaustBeam_Aim` `0x4E3410` (C2: the beam's near end from
  `0x93B5FC..`); `ElemBreathMote_Start` (S09, the enemy's +0x8C);
  `Chill_Start` `0x4B6710` (S15) and `Corona_Start` `0x4E7450` (S31: the
  0x80-byte copy may overlap the new slot; ours copies dword by dword,
  forward, as `rep movsd` does) - these two reached only in an event battle.
- **Actor - 3 unchecked above 10:** `Magic111_Start` (C3); S01's starts;
  S02's actor and target copies; `Chlorine_Start`, `Blitz_End` (S03); S04's
  images; S05's actor record; `Magic008_Start`, `Magic020_Start` and the
  reactions' state reads (S06); `Bonebreak_Start` (S07); `EvilEyeBeam_Start`
  (S08); `Sacrifice_Start` / `_Wait` (S10); `MainCannonShell_Aim` / `_Fly`
  (S31); `MightyChop_Start` `0x4ED100` (S33, with the overlapping copy).
- **By a child's byte:** S03's bolts by +4; `HowlingChild_Start` / `_End`
  by +0xB (S10); `SanctuaryMote_Wait` by +0xB - 3 (S11);
  `SuddenDeathChild_Start` by +4 - 3 (S13); `InfluenceMark_Start` by +0xB
  (S15); `Magic092_ChildTint` / `_ChildEnd` and `Magic088_Apply` (S20);
  `BenedictionChild_*` by +4 into the party records (S35: below the party
  count `0x904AB0`); `MagicFx_ApplyBuff` and `BuffPopupAt_Start` past 10 (L).

In each the creator writes only in-range values by reading.

**Status:** latent.

## D97 — Spell value tables indexed unchecked (latent, reads)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups L, C1, S03, S04, S06, S08, S11, S12, S14, S17, S19,
S21..S25, S27, S29, S30, S33..S36 and S38. D64's class.

**Established:** a table of angles, offsets, colours, names or counts in
`.data` indexed by a record byte with no bound; past the table the original
reads the next table's bytes as values - a wrong value, not a fault. Ours
reads the same bytes (where a group aborts instead, it is said).

- L: `BuffPopup_Draw`'s icon (9 cells, by the caller's whole kind byte),
  `MagicFx_FormationOffset`'s two indices.
- C1: `PentagramRing_Radii[+0xB]`, the puffs' offset rows `[+0xB]`.
- S03: `ChlorineCloud_Start`'s offsets by +0xB (**ours aborts past 2**).
- S04: `SnapWave_FacingPhase` by the facing, `AirRaidImage_ShadeSteps` by
  +0xB.
- S06: two data tables by +0xB.
- S08: `CounterMark_Place` (the form byte `0x904B89` into the 28-byte
  `CounterMark_FormIndex`, then the 22-pair tables); `WardMote_PushMatrix`'s
  `WardMote_Tilts` by the direction; the trail (+4 x 32 + point).
- S11: `Tornado_FacingPhase` by the facing (four entries).
- S12: `Identify_DrawItem` past the name tables (a wrong name).
- S14: `WeretigerStreak_Start`'s offset pairs by the facing,
  `Weretiger_DrawSprite`'s UV entries; `Item_CopyName` checks neither index
  nor category (0 or above 3 reads the consumable names).
- S17: `ReviveHalo_Spawn`'s count table by +4. S19: the colour tables by
  `+4`. S21: `IceShard_Draw`'s jitter bytes by `+0xB >> 1` (+ k). S22:
  `BlizzardShard_Launch` / `_Grow` / `Blizzard_Start` by +4.
- S23: `FxSpiral_Turns[+4]`, `FxSpiral_Tilts[2 * +8]`,
  `SimoonDust_Offsets[+0xB]`. S24: `Fx105_OrbAngles` / `_OrbColours` by +4.
  S25: `SpellDepress_Start`'s corner (`0x904AAC` xor 2),
  `SpellRagnarok_SparkInit`'s row by +0xB.
- S27: `WhelpBreath_GlowAngles` (4 bytes) by the direction. S29:
  `ShadowSeeker_Colours` by +0xB.
- S30: the party-set index (`0x90412C & 0xFF`, bit 7 kept: a wrong file
  index), `VenomRing_ScreenY` by +0xB, the mote tables by +7;
  `Kaiser_ReloadParty` / `Kaiser_ShowParty` walk the members by
  `Field_MemberCount`, unbounded by the three records (D85's shape).
- S33: `0x904B89` into `0x64ECB0`; `AccessionSpark_Angles` by +4.
- S34: `TransferMote_Colours` by +4 x 4 + +3. S35: `CureMote_Colours` by
  kind x 4 + variant.
- S36: `MagicBallOrb_Shades` by +0xB; `IntimidateTrail_*`'s points by +4 and
  +0xA / +0xB - these **write**, into `Magic213Mote_Pool` and on, for a +4
  past 1.
- S38: `TempestGust_Angles` by the owner's direction (past 3, a code pointer
  of `Magic225Veil_TaskTable`, then the burst colours).

**Status:** latent: each group found its creators writing in-range values.

## D98 — Tint record indices taken from `Sprite_SetTint` unchecked (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, groups C1, S16, S20, S22 and S26.

**Established:** effects keep the byte `Sprite_SetTint` answered and index
`MoveScript_TintRecords` (12-byte records) by it later, unchecked; D60
records that `Sprite_SetTint` answers `0xFF` when no record is free.
`Magic088_Darken` `0x4C46B0` / `_Lighten` `0x4C4700` (S20), `ActorFx_Untint`
`0x4BC1A0` (S16, as MAGIC070's `Sparkle_End`), `Myollnir_Darken` `0x4CBA00`
(S22), MAGIC115's +0xA and MAGIC117's +0xB (S26), and C1's tint record
`+0xB` / `+0xA`. **The docs disagree on the table's size:** S16 and S20
(and D60) count 32 records, so `0xFF` writes 223 records past them; C1
("3,072 bytes") and S26 ("256 records") say every byte lands inside it.
Not re-measured here. Ours indexes the same.

**Status:** latent; the size owes one reading of `Sprite_SetTint`.

## D99 — Spell waits with no limit or on an exact count (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups C1, S05, S06, S14, S18 and S32 (Head Cracker's
waits are D103).

**Established:** each wait ends only when a count or a flag reaches one
value, with no frame limit; a count left short (an unchecked create, D90) or
passed between two runs freezes the effect, and `BattleFx_Finish` then never
ends the spell.

- Pentagram (C1, `Pentagram_Task` `0x4D67F0`, [`magic_c1.md`](magic_c1.md)
  §12): phases 5 and 6 wait for `+0xB` to be exactly 6 and exactly 0x80.
- Rows 5, 6, 54 and 68 wait for `+0xB` to reach 0xFF, rows 53 and 43 for 0
  (S05, [`magic_s05.md`](magic_s05.md) §8).
- `Magic008Blow_ReactTwo` / `_ReactThree` wait while the target's state is
  6, Head Cracker's shape (S06, [`magic_s06.md`](magic_s06.md) §8).
- `WeretigerImage_Play` `0x4B5160` ticks the image's script to its end in
  one call: a script that never ends hangs the frame (S14,
  [`magic_s14.md`](magic_s14.md) §8).
- Drain (S18, `Drain_Task` `0x4BEB50`, [`magic_s18.md`](magic_s18.md) §9):
  types 2 and 3 wait for the owner's child count to *equal* 2 and 3; by the
  counters they see it in a window of a few frames.
- Wall of Fire (S32, `WallOfFireChild_WaitFlame` `0x4E93C0`,
  [`magic_s32.md`](magic_s32.md) §8): the child and both sparks wait for bit 7
  of the child's +0xB, which only the flame sets; if the flame's record
  were not allocated nothing would set it. MAGIC092's child has the same
  shape.

Ours keeps each.

**Status:** latent; none is reached by reading.

## D100 — Stack indices and phases read again across calls (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S12, S17, S18, S19, S26, S27 and S33.

**Established:** an index into a stack array, or a phase, is read once
before a call and again after it; nothing the callees do moves it in the
game, but a callee that did would store outside the array (into the
original's own frame) or dispatch another phase.

- `CeleritySpark_Draw` `0x4B28B0` (S12) and `BuffSpike_Draw` `0x4C0E30`
  (S18): `+9` read again after the GTE calls indexes a four-entry depth
  array. **Ours aborts outside 0..3.**
- `ShieldSpark_DrawCrystal` `0x4C22D0` (S19): the same with `+9`. **Ours
  aborts.** `BarrierRing_Draw` `0x4C2E30` (S19): "stands up" (`+2`) tested
  twice a segment; a change before any standing segment would link an
  uninitialised stack dword. **Ours aborts.**
- `Magic114_DrawTriangle` `0x4D7D00` (S26): the eight keys by +0xB read
  after the projection; 8 or more writes past the ring's frame array.
- `Identify_DrawMember` `0x4B1090` (S12): restores the name byte through the
  target read again after two calls.
- Phases read after two calls: `Leech_Task` `0x4BDAC0` (S17),
  `WhelpBreathSprite_Run` `0x4DB7A0` (S27), `MightyChopBlade_Run`
  `0x4ED330` (S33).

**Status:** latent; unreachable by reading.

## D101 — Uninitialised bits passed as arguments (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S19, S25, S26 and S37.

**Established:** `Shield_Start` `0x4C14F0` passes `Battle_ActorIsOut` a stack
dword whose upper bytes are uninitialised, and `ShieldAura_Fade` `0x4C1940`
passes register garbage above the bytes to `BattleActor_Flash` and
`0x4FB790` (S19); `Magic114_DrawRays` `0x4D91A0` passes `MapView_LinkPrimAt`
a dword with three garbage bytes (S26); `SpellConfuse_ChildFly` `0x4D4080`
passes a VECTOR by value with its pad word unset (S25);
`MagmaBreathRecord_Fly` `0x4F5E40` passes an uninitialised stack word as
`MagicFx_StepTowardPoint`'s fourth argument (S37). Every callee reads only
the bytes that were set (read: `0x4456C0`, `0x4FB6F0`), or never the
argument. Ours passes what the callees read.

**Status:** latent, harmless.

## D102 — Paralyzer (row 123) reads through address 0 in an ordinary battle (candidate)

**Seen:** not seen here; TCRF reports the skill "crashes the game". Found by
reading the code while taking it over, 2026-09-26, group E
([`magic_engine.md`](magic_engine.md) §6, E1;
[`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3).

**Established:** `Paralyzer_Start` `0x43F3E0` (row 123's task
`Paralyzer_Task` `0x43F3B0`, ability id `0x8B`) plays a sound cue from
`word [[0x939AD8] + 0xF8]`: the enemy object `BattleEnemy_RunAll` ran last,
not the caster, and its `+0xF8` cue-table pointer. A byte-pattern scan of
`.text` for every `mov dword [r32 + 0xF8], ...` finds 62 stores, all of cue
tables (`0x64C7A0..0x64DCD4`), all in the event-battle set-ups
(`0x437A55..0x4400D0`); the ordinary enemy set-up (`0x494570`, `0x4946C0`)
never writes `+0xF8`, and every other reader tests the event-battle byte
first. The enemy records lie in `.data`'s zero-filled tail, so `+0xF8` is 0
at start. So in a session where no event battle has yet written that enemy
slot, the first Paralyzer reads a word at address 0: an access violation.
After an event battle it plays that boss's cue instead (nothing clears the
pointer). **Ours reads through the same pointer, unchecked, and faults where
the original does.** Not measured: a block clear through a pointer computed
elsewhere would not show in the scan; the check is one read of `0x93BA58`
(+ 0x128 n) in an ordinary battle, or the owner casting it.

**Status:** candidate (the TCRF crash, explained by reading); fixing it is a
divergence for the owner to choose.

## D103 — Head Cracker's (row 128) freeze is not explained by reading (candidate)

**Seen:** not seen here; TCRF reports the skill "freezes the game" when
hacked into a player's list and cast on an ordinary enemy. Read 2026-09-26,
group E ([`magic_engine.md`](magic_engine.md) §6, E2 and E6;
[`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3).

**Established:** `HeadCracker_Task` `0x43FC80` (ability id `0x80`) has three
unbounded waits:

1. `HeadCracker_WaitCaster` `0x43FD80`: the caster's animation 2 until
   `Sprite_ScriptTickOnce` answers 1 - at the script's end or any jump, so
   not a freeze by reading.
2. `HeadCracker_WaitTarget` `0x43FE00` for the rocks: `+0xA` counts rocks
   created and not landed; `HeadCracker_Drop` `0x43FDC0` does not test
   `BattleTask_Create`'s `0xFF` (D90), so with 48 slots taken it counts a
   rock that never lands - a freeze, if the write past the image does not
   fault first.
3. `HeadCracker_WaitTarget` for the reaction, while the target's `+1` is 6:
   by reading it ends for a state-0 enemy (`EnemyOp_HitEnd`) and for a
   member (`0x441D80`). It does not for an enemy in another state (the boss
   and event tables `EnemyOp_StepsB..F`; D..F not read), or for a target
   byte with a side bit (`0x40` / `0x80`), which `HeadCracker_WaitTarget`,
   `HeadCrackerRock_Start` `0x43FF00` and `Battle_ActorIsOut` index as an
   enemy past the eight records (past the image for `0x40`).

So **TCRF's case (a member caster, a state-0 target) is not explained**; the
candidates are the side-bit target and an enemy in a non-zero state. Ours is
faithful. The check is live: cast it with the cheat (DIV-0045) with a watch
on `0x904B44`, the caster's and target's `+1`, and the task's `+1` / `+0xA`.

**Status:** candidate, cause open.

## D104 — Blitz's bolt can step past its table into Snap's code (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S03 ([`magic_s03.md`](magic_s03.md) §8;
[`takeover-queue-round9.md`](takeover-queue-round9.md) §10).

**Established:** `BlitzBolt_Seek` `0x49E340`, when the bolt's actor is out,
sets `+2` to 11 (`BlitzBolt_Drift`) and goes on to its step and near test;
if the bolt is also within 0x8000 of that actor that frame, the near branch
runs too and moves `+2` on to 12. The next frame `BlitzBolt_Run` `0x49E260`
jumps through `BlitzBolt_Steps[12]` (`0x65A630`, twelve entries), which is
MAGIC013's table's first entry, `0x49EBE0` - `SnapWave_Run` (Snap, a name
read one id down) - which indexes its own five-entry table by the same 12.
On the PlayStation, where each overlay loads alone, the entry past the table
is whatever follows it in Blitz's own file, so the wild jump is likely an
artifact of the port's linking. The likely case is a target knocked out by
an earlier bolt while this one is on its way; not measured. **Ours aborts at
the bad step.** **The owner, 2026-09-26: keep ours as it is, no DIVERGENCE
entry.**

**Status:** latent; ours aborts where the original runs another spell's code.

## D105 — Identify on a party member can mark an arbitrary enemy kind identified (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S12 ([`magic_s12.md`](magic_s12.md) §8).

**Established:** `Identify_Start` `0x4B0D90` runs its roll whatever the
target; for a member (0..2) the roll's enemy record is index -3..-1, the
tail of the battle task slots (`0x93B960 - 3 x 0x128`). Its "level"
(`+0x98`) is whatever a task left there, and if its `+0x8F` is not 0 and the
roll hits, `Identify_MarkSeen` `0x4B0FF0` sets the seen bit of the "kind" at
its `+0x8C`. Whether the skill can target a member is not measured. Faithful
in ours.

**Status:** latent.

## D106 — The matrix pushers turn by uninitialised stack words past direction 3 (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S15 ([`magic_s15.md`](magic_s15.md) §8), S04
([`magic_s04.md`](magic_s04.md) §8), S25 ([`magic_s25.md`](magic_s25.md) §6)
and S31 ([`magic_s31.md`](magic_s31.md) §8).

**Established:** each builds a rotation from a four-entry jump table on the
direction byte; a direction past 3 goes to the common tail with the angle
words never written, so the matrix is built from whatever was on the stack.
`ChillRay_PushMatrix` `0x4B6BC0` (S15: the ray's +8 comes from the owner's
+8 in `Chill_Start`), `SnapWave_PushMatrixA` / `_B` (S04: +8 written only by
`SnapWave_Start`), `SpellConfuse_PushFacingMatrix` `0x4D41B0` (S25) and
`CoronaRay_PushMatrix` `0x4E77D0` (S31). **Ours aborts on a direction past
3.** Whether a live owner's direction can be past 3 is not measured.

**Status:** latent.

## D107 — Null entries in the form table `0x64E9BC` (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S30 ([`magic_s30.md`](magic_s30.md) §8) and S33
([`magic_s33.md`](magic_s33.md) §8; round nine §12).

**Established:** `0x64E9BC` holds 26 pointer pairs indexed by `0x904B89`
(8-byte entries); entries 10, 19 and 20 are null (read 2026-09-26).
`Kaiser_LoadSetFile` `0x4E5090` (S30) reads through the entry unchecked: a
word at address `2 x party set`, an access violation. **Ours aborts with a
message there.** `Accession_LoadFormA` `0x4EB0F0` / `_LoadFormB` `0x4EB2E0`
(S33) dereference the pair at once: a kind of 10, 19 or 20 (or past 25,
past the table) faults - on both sides, by S33's harness note (its seed
keeps to the 23 non-null kinds). Whether `0x904B89` can hold those values
while these spells run is not measured.

**Status:** latent.

## D108 — Accession's path B copies an enemy actor's "record" over party member 0 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S33 ([`magic_s33.md`](magic_s33.md) §8).

**Established:** `Accession_*` index the acting member as a party record
unchecked (0..2 by design); an enemy actor would make them read and write
past party record 4, and `Accession_ApplyB` `0x4EB350` would copy that over
party record 0. Ours does the same.

**Status:** latent.

## D109 — `LastResortBeam_DrawSparks` never ends for a step of 0 or below (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S35 ([`magic_s35.md`](magic_s35.md) §8).

**Established:** `LastResortBeam_DrawSparks` `0x4F0470` loops with a row
that does not advance past `+9` and a shade that does not fall when its step
k is 0 or below. Its only callers pass 2 and 3. Ours keeps it (the fuzz
passes k of 2 or 3, else 1..6).

**Status:** latent, unreachable by reading.

## D110 — `CombustionSprite_Fade` scales one axis twice and the other never (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S37 ([`magic_s37.md`](magic_s37.md) §8).

**Established:** `CombustionSprite_Fade` `0x4F76E0` adds 0x1000 and then
0x800 to the dword `+0x40` and never touches `+0x44`, which every start sets
to the same 0x10000 - read as a copy slip for `+0x44`: the sprite would
stretch in one direction as it fades. Ours keeps it. Unmeasured on screen.

**Status:** latent (visible, if it shows, as a one-way stretch).

## D111 — Two spell shakes leave the camera's first angle at 0xFD56 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S37 ([`magic_s37.md`](magic_s37.md) §8) and S10
([`magic_s10.md`](magic_s10.md) §8).

**Established:** `CombustionSprite_Shake` `0x4F75E0` and
`LavaburstChild_Shake` `0x4AC470` rock `Camera_Angles[0]` relative to its
value and then end by setting it to 0xFD56, whatever it was before the
spell. A battle whose camera angle is not 0xFD56 there would keep the new
one (a jump). Whether any battle's camera differs was not measured. Ours
keeps it.

**Status:** latent.

## D112 — `Magic225_Spawn` plays one sound sixteen times in a frame (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S38 ([`magic_s38.md`](magic_s38.md) §8).

**Established:** `Magic225_Spawn` `0x4F9090` plays sound 0x100 once before
each of its sixteen shards, in one frame. Whether the sound layer merges
them was not measured (D84's shape). Ours keeps it.

**Status:** latent.

## D113 — A second cast of the same overlay clears the first's pool (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S38 ([`magic_s38.md`](magic_s38.md) §8) and S28
([`magic_s28.md`](magic_s28.md) §8).

**Established:** each overlay's record pool is cleared by its own start
(`Tempest_Start` `0x4F86C0`, `Magic225_Start` `0x4F8FD0`,
`MeteorStrike_Start` `0x4F9E60`; `Icebreath_Start` `0x4DE980`,
`Thunderbreath_Start` `0x4DF580`), and the casts share one pool. A second
cast while the first still runs clears the first's records, whose owner
then never sees its count reach 0 - a wait without end (S38). Whether two
casts can overlap in play was not measured. Ours keeps it.

**Status:** latent.

## D114 — Ink Ink's and MAGIC213's children read the target's side late (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group C1 ([`magic_c1.md`](magic_c1.md) §12).

**Established:** `InkInkActor_Start` `0x4EA130` and `Magic213Actor_Start`
`0x4F4C80` find their actor's record from the target byte's 0x40 when the
child starts (after a delay of up to 16 x 7 + 1 frames), not when it was
spawned; if the byte changes in between, a party index is read as an enemy
(inside the task slots) or an enemy index as a party member (past the
three). Whether the target byte changes during an effect is not measured.
Ours keeps it.

**Status:** latent.

## D115 — Pentagram leaves row 26's CLUT with the STP bit set (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group C1 ([`magic_c1.md`](magic_c1.md) §4, §12).

**Established:** `Pentagram_Start` `0x4D6860` sets the STP bit on row 26's
first sixteen CLUT entries; nothing in MAGIC113 puts them back (MAGIC213's
start copies them back from the source, and others may). Not measured in
play. Ours keeps it.

**Status:** latent.

## D116 — Row 27 in battle draws at the field's kind-2 point (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group C1 ([`magic_c1.md`](magic_c1.md) §3, §12).

**Established:** `Magic080_Start` `0x4FC360` places the task at
`Field_Kind2X` / `Field_Kind2Z` (`0x905E64` / `0x905E60`, names that are
hypotheses in `symbols.toml`) with the map's elevation there, whatever those
hold in battle. By design or not was not read. Ours keeps it.

**Status:** latent; possibly intended.

## D117 — A Bone Dance follower would stand at the ground height of (x, x) (latent, unreachable)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group C2 ([`magic_c2.md`](magic_c2.md) §8).

**Established:** `BoneDanceFollow_Start` `0x4AEF00` and `_Step` `0x4AEFE0`
push the dword `+0x34` for both of `AreaMap_Elevation`'s arguments, where
the bone's fall pushes `+0x38` then `+0x34`. No creator in MAGIC057 starts a
follower, so it is unreachable by reading. Ours keeps it; control B44 (the
fix) is refused.

**Status:** latent, unreachable.

## D118 — `Magic002_Start` overwrites its row task's owner pointer (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group C3 ([`magic_c3.md`](magic_c3.md) §8).

**Established:** `Magic002_Start` `0x499DB0` writes the ball's address into
the current slot's `+0x80` (through `0x93B8C4`), which is the row task's own
owner pointer: the owner is lost for the rest of the effect. Nothing after
reads the owner (`Magic002_Wait` reads the field as the ball; `0x43FE80`
does not read it). Ours keeps it.

**Status:** latent, harmless.

## D119 — Dead or overwritten computations in the spell overlays (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups C3, S08, S10, S23, S25 and S29.

**Established:** each computes something that is discarded or never runs;
ours keeps each.

- `Magic002Ball_Draw` `0x499FF0` (C3, [`magic_c3.md`](magic_c3.md) §8):
  `Gpu_GetTPage` gets `(0x3C0, 0x100, 0, 0)`, which looks like libgpu's
  `(tp, abr, x, y)` given as `(x, y, tp, abr)` - no difference, the quads
  are untextured; the turn is read as its low byte, so the ball does not
  turn while it flies; the pole's shade is computed ten times and every
  point's z never read.
- `EvilEyeSpark_Grow` `0x4A9510` / `_Fade` `0x4A9550` (S08): call `Rand` and
  drop its answer; the bit 0 test after it sets flags nothing reads (by the
  shape, a left / right jitter that lost its branch).
- `EbonfireRing_End` `0x4AD300` (S10, [`magic_s10.md`](magic_s10.md) §8) is
  dead code; if it ran it would set the owner's ring count to 0xFF and
  `FxDiscFan_Fade` would never see 0 rings.
- `FxFunnel_Wait` `0x4CCB10` (S23, [`magic_s23.md`](magic_s23.md) §5): the
  turned offset is overwritten by the owner's position, so the heading is
  `Math_Ratan2(0, 0)` every time, a constant (control F4 unrefusable).
- `SpellRagnarok_DrawScreenTint` `0x4D55C0` (S25): a dead store to
  `0x903854`.
- `ShadowBreath_Task` `0x4E19A0` (S29): leaves `ShadowMote_Current` at the
  last live record; nothing else reads it.

**Status:** latent, harmless.

## D120 — `SuperCombo_ReadButton` compares the whole pressed word (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S02 ([`magic_s02.md`](magic_s02.md) §8).

**Established:** `SuperCombo_ReadButton` `0x49A960` compares the whole
`Input_Pressed` word, so the right button pressed together with any other
bit counts as a miss and ends the prompt. Whether that is felt in play
depends on whether `Input_Pressed` holds edges or levels, not measured.
Ours keeps it.

**Status:** latent; the owner's feel to judge.

## D121 — Magic008's reactions cancel an upload they did not check was queued (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S06 ([`magic_s06.md`](magic_s06.md) §8).

**Established:** `Magic008Blow_ReactTwo` `0x4A2E50` / `_ReactThree`
`0x4A3020` take `Gfx_UploadQueueCount` down by one after
`Sprite_SetAnimation` and zero that entry, assuming the call queued one. If
it did not (the enqueuer `0x5894D0` skips a record with +0 bit 1; whether
`Sprite_SetAnimation` always reaches it was not read), an earlier entry is
dropped; at a count of 0 the count wraps to 255 and the three 20-entry
arrays are written 235 entries past their end. Not measured whether a
member's double can reach it. Ours keeps it.

**Status:** latent.

## D122 — `ElemBreathEnemy_Start` ends its parent breath early (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group S09 ([`magic_s09.md`](magic_s09.md) §8).

**Established:** `ElemBreathEnemy_Start` `0x4AA320` leaves `Sprite_Current`
at the breath task (not put back), zeroes the breath task's count +0xB and
sets its +1 to 1 (`BattleFx_Finish`). So, by reading, in that event battle
the breath ends (the done flag, the task freed) the frame its second child
starts, while the emitter and its motes run on, and the emitter's own end
(`MagicFx_EndWithChildren`) later counts down the freed task's +0xB. Not
measured. Ours keeps it.

**Status:** latent.

## D123 — One designated child carries the effect's payload (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups S13 ([`magic_s13.md`](magic_s13.md) §8) and S29
([`magic_s29.md`](magic_s29.md) §7).

**Established:**

- Sudden Death's burst (S13): 92 burst motes plus up to ten orbit motes can
  ask for more than the pool's 96; `SuddenDeathMote_Alloc` `0x4B3E80`
  answers 0xFF and the caller skips the mote (checked). If mote 0 is the one
  skipped, `SuddenDeathBurst_Close` `0x4B3500`'s
  `Battle_SetTargetFlags(..., 0x10)` - made by mote 0 only - is never
  called. Not measured.
- `ShadowSeeker_Burst` `0x4E28C0` (S29) does nothing but step on for a
  seeker whose `+0xB` is not 0; if seeker 0 were freed early (nothing seen
  does it) no orb, glow, motes or target flag would come.

Ours keeps both.

**Status:** latent.

## D124 — A reused record keeps its last tenant's fields (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26..27, groups E ([`magic_engine.md`](magic_engine.md) §6, E5) and
S14 ([`magic_s14.md`](magic_s14.md) §8).

**Established:** `HeadCrackerRock_Start` `0x43FF00`'s rock starts 0x800 above
the height word its slot's previous tenant left (`BattleTask_Create` does
not clear `+0x3E`). `WeretigerStreak_Alloc6` `0x4B4B90` does not set a
record's step `+1`, relying on the previous streak's end
(`WeretigerStreak_Draw` clears +0 and +1); a record left live with another
`+1` would dispatch past the two-entry table (D89). Ours keeps both.

**Status:** latent.

## D125 — `Magic073_CountReacting` reads the wrong enemies' state (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S16 ([`magic_s16.md`](magic_s16.md) §6).

**Established:** `Magic073_CountReacting` `0x4BBCE0` asks
`Battle_ActorIsOut(i + 3)` for i 0..7 and then reads the state byte at
`0x93BCD9 + 0x128 i` - enemy 0's state byte `0x93B961` plus three records.
So enemy i's liveness gates enemy i + 3's reaction state, and for i 5..7 the
byte is past the eight enemy records (`0x93C2B1..`, near the message queue
`0x93C2A0`). MAGIC073's end therefore waits on the wrong enemies'
reactions; whether that ever holds the effect up is not measured (a watch
on `0x904AA8` bit 2 in a live cast). The party half is right. Ours keeps
the original's index; control C2 (the fix) is refused. The PSX's was not
compared.

**Status:** latent.

## D126 — `Magic088_Variant` rewrites the ability id (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S20 ([`magic_s20.md`](magic_s20.md) §7).

**Established:** `Magic088_Variant` `0x4C4830` rewrites the ability id
`0x112` to `0x5A` when the side byte is not 4 - a presentation effect
mutating battle state. Whether it is meant is not known. Ours keeps it.

**Status:** latent; possibly intended.

## D127 — Jolt and Lightning take task slot 0's position when no bolt was made (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S22 ([`magic_s22.md`](magic_s22.md) §8).

**Established:** `Jolt_Start` `0x4C9C90` / `Lightning_Start` `0x4CAAA0` keep
a slot index that starts at 0 and is not checked, so with every target out
(no bolt made) the task takes task slot 0's position. Harmless; ours keeps
it.

**Status:** latent, harmless.

## D128 — `Quake_End` loses a map code 0x28 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S23 ([`magic_s23.md`](magic_s23.md) §5).

**Established:** `Quake_Start` `0x4CE140` turns cell code 0 into 0x28 and
`Quake_End` `0x4CEA20` turns 0x28 back into 0, so a cell prim that was 0x28
before the spell comes back as 0. Whether any battle map has such a cell
was not checked. Ours keeps it.

**Status:** latent.

## D129 — Fx104's whirl 0 outlives its parent and writes into a freed slot (candidate)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S24 ([`magic_s24.md`](magic_s24.md) §6), which calls it a
defect candidate.

**Established:** `Fx104_WhirlEnd` `0x4D0850` and `Fx104_BurstShrink`
`0x4D1320` signal by index, not by count: each writes its own index into the
parent's `+0xB`, and the parent moves on at 7. Whirl i's delay starts at 2i;
whirl 0's at 0, and `Fx104_WhirlDelay` `0x4D0790` decrements before it
tests, so it wraps to 255 frames. Whirl 7 ends first (about 38 frames), the
parent goes on, burst 7 ends the parent at about 170 frames, and whirl 0
draws until about 280 and then writes its `+0xB` through its owner pointer,
into a task slot freed and possibly reused. On screen a lone late whirl (a
guess). The live check: a cast of row 14 with a watch on the task slots.
Ours keeps it.

**Status:** candidate.

## D130 — `BurnFlame_Draw` sorts by x in both x and z (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S27 ([`magic_s27.md`](magic_s27.md) §8).

**Established:** `BurnFlame_Draw` `0x4DA540` sorts each quad at the task
moved by its first point's x in both x and z (the z uses x, not the point's
second coordinate) - by reading a slip in the source. Ours keeps the depth
order it gives.

**Status:** latent.

## D131 — The kind-1 battle task `0x5B` never ends by itself (latent, open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-26, group S28 ([`magic_s28.md`](magic_s28.md) §2, §8).

**Established:** the battle kind-1 task table's entry 91 (`0x5B`) is
`0x4DF820`, `Port_DroppedCall`, the linker's shared empty function: a task
with that handler does nothing each frame and never frees itself.
`tools/magic_rows.py` lists it as reached by MAGIC113 through that entry;
S28 left what MAGIC113 does about it to group C1. C1's doc
([`magic_c1.md`](magic_c1.md) §4, §8) reads `0x4DF820` in `Pentagram_Sprites`
`0x4D6B70` as two direct dropped calls a frame, not as a task created - the
two readings were not put together.

**Status:** latent; whether such a task is ever created is open.

## D132 — `Task_Create` does not test its slot, and the task stacks have no guard (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, round nine's scheduler group ([`task_sched.md`](task_sched.md)
§6; [`takeover-queue-round9.md`](takeover-queue-round9.md) §4).

**Established:** `Task_Create` `0x5A9914` does not test the slot: a slot of
4 writes the fifth "record" over `Task_CurrentOffset` / `Task_SchedulerEsp`
(`0x66C850` / `0x66C854`), and the next landing loads a garbage `esp`. Every
caller passes a constant 0..2. **Ours aborts on a slot past 3.** The task
stacks (0x4000 bytes each) have no guard; kept, it is the layout
([`SCAFFOLDING.md`](SCAFFOLDING.md) §3).

**Status:** latent.

## D133 — The chapter banks' and area blocks' dispatch tables are never checked (latent; ours aborts past most, three chapter groups and one world-map copy read on)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, by nearly every group of the scenario and area rounds
([`takeover-queue-round10.md`](takeover-queue-round10.md) §10, §13, §16,
§19; the group docs below). D59's class in the chapter banks (`Scena*`) and
the area overlays (`Area*`); one entry for the class, the groups' docs carry
the tables.

**Established:** every chapter's frame, run, object and cell dispatcher
indexes its `.data` table by a signed state or run byte, the object's
`+0x86` or the cell search's answer with no bound, and every area's state
machine by `Sprite_Current[4]`, an effect record's `+1` or a phase byte the
same way. Past a table the original jumps through the next table's entries
(the state tables run straight into the run tables in most chapters, D134),
through data (a step table, a descriptor, a 0 dword) or, in a stack-built
table, through its own return address. **What ours does differs by group**,
each citing round nine's rule (round9 doc §6) - the four places where the
docs contradict each other, for one read of the code to settle:

- *Aborts for any index outside the table's own entries, code beyond it or
  not*: SC1 (`Scena01_Run` `0x53A2B0`, `_ObjectHook` `0x53D470`, `_CellHook`
  `0x53DD20`; [`scena_sc1.md`](scena_sc1.md) §3), SC2 (`Scena02_Frame`
  `0x53DDA0`, `_Run` `0x53E3D0`, `_ObjectHook` `0x541D10`; [`scena_sc2.md`](scena_sc2.md)
  §3), SC5, SC6, SC9a, SC9b (which also aborts on a 0 entry: chapter 10's
  runs 8 and 9 and object 10 are nulls, D66's shape), SC12 and SC13 (each
  doc's §2): a movement script storing a run past the table by op `F6`
  "would stop here where the original ran the next table's entry".
- *Aborts only where the word read is not code* (`CodeAt`): SC0
  (`Scena00_Frame` `0x537F20`, `_Run` `0x5384F0`, `_ObjectHook` `0x539A10`;
  a state of 3..14 runs the run handler the original runs,
  [`scena_sc0.md`](scena_sc0.md) §7) and SC15 (every frame, run, object hook
  and `Scena17_EndTask` `0x56D3B0` of chapters 15..19,
  [`scena_sc15.md`](scena_sc15.md) §7).
- *Reads in place, no abort, as the original*: SC3 ([`scena_sc3.md`](scena_sc3.md)
  §7 "ours reads them in place the same"), SC7 ([`scena_sc7.md`](scena_sc7.md)
  §7, silent on ours; `scena_sc7.cpp` has no bound) and SC11
  ([`scena_sc11.md`](scena_sc11.md) §6: `Scena11_ObjectTrigger` `0x55E170`'s
  index 15 is a call to address 0 and 16 and up chapter 12's vtable; and the
  chapter's own triggers 9..13 set `MoveScript_Var7` to 0xA, 0xB and 0xE past
  `Scena11_Runs`' ten entries, so `Scena11_Run` `0x55C360` reads
  `Scena11_Triggers` as runs - the one chapter whose own code writes a
  past-end index). `scena_sc3.cpp`, `scena_sc7.cpp` and `scena_sc11.cpp` hold
  no `Fatal`.
- *The area blocks* abort with a message past each table (the owner's rule,
  no DIVERGENCE entry) in every group but one: AR0A (`0x401A10` /
  `0x401AF0`), AR0C (`Area32_RunA` `0x4038C0` / `_RunB` `0x403960`,
  `Area33_Record08Run` `0x404680`, `_Record04Run` `0x404800`,
  `Area36_EffectRun` `0x404FE0`), AR1A (`Area39_Run` `0x405590`, `Area41_Run`
  `0x4063D0`), AR1B (`Area43_ObjectRun` `0x406F80` and area 45's six), AR1D
  (`Area56_FallRun` `0x40B080`, `Area59_EffectRun` `0x40B4D0`), AR1E (area
  65's six), AR1F (`Area68_GlideRunA` `0x40D070`, `_GlideRunB` `0x40D130`,
  `Area69_GlideRun` `0x40D360`, `Area75_FlyRun` `0x40D920`, `_FollowRun`
  `0x40DA60`, `Area75_PhaseRun` `0x40E1C0`), AR2A (`Area79_ObjectState`
  `0x40F410`), AR2B (areas 87 and 88's six each), AR2C (`Area91_ObjectRun`
  `0x411FE0`: state 6 is the `+0x3C` array's one entry, the dispatcher
  itself - a hang; 7 and up the descriptor's words - a fault), AR2D
  (`Area99_RunDrift` `0x413D50`, `_RunLeap` `0x413DF0`, `Area100_EffectB7Run`
  `0x414490`), AR2E (area 104's four, `Area104_LeaderRun` `0x415020`'s two
  and `Area104_Kind5CStates`' five), AR2F (`Area108_FadeRun` `0x416F10`,
  `Area112_EffectRun` `0x418860`), AR3A (area 115's six and
  `Area116_EffectB8Run` `0x419E20`), AR3B (area 121's eight by its doc, plus
  `Area121_CameraRun` `0x41AA50`, `_LeaderRun` `0x41B9D0`, `_Kind5CRun`
  `0x41C190` by its function rows - the count wants one read of
  `area_w3b.cpp`), AR3D (`Area135_QueueRun` `0x41E110`, `_WalkRun`
  `0x41E260`, `_LiftRun` `0x41E370`, `_SpawnRun` `0x41E420`, each given the
  count of code pointers its original reaches, D134), AR3E
  (`Area140_Effect71Run` `0x41FB70`, `Area142_RunWait` `0x420760`,
  `Area141_RunShadeUp` `0x41FDB0`, `_RunShadeDown` `0x41FCB0`), AR3F
  (`Area143_EffectB3Run` `0x420B00`, `Area146_EffectB4Run` `0x422080`,
  `Area145_RunDrop` `0x421220` - whose own landed state, 2, is past its
  two-entry table: the script's re-calls keep it from running there,
  "not measured in play"), AR3G (area 151's six, `Area148_BeamRun`
  `0x422840`), AR4A (area 152's six), AR4B (`Area172_RunFall` `0x427C10`,
  `_RunSlide` `0x427DF0`, `Area172_EffectA5Run` `0x428200`; D134), AR4D
  (`Area175_GlideRun` `0x429440`), AR4E (`Area189_LeaderRun` `0x42A8B0`,
  `Area191_RunScale` `0x42B680`), AR4F (`Area197_RunShake` `0x42CCA0`,
  `Area198_EffectA6Run` `0x42D4B0`). **AR0B's area 16 reads on** through
  its six tables (`Area16_PlateRun` `0x401D00`, `_HudRun` `0x402080`,
  `_FrameStep` `0x4020B0`, `_BoxStep` `0x402180`, `_Record8Run` `0x4025D0`,
  `_Record4Run` `0x402750`; `area_w0b.cpp` has no `Fatal`,
  [`area_w1b.md`](area_w1b.md) §9 says so) where every other world-map copy
  (45, 65, 87, 88, 104, 115, 121, 151, 152) aborts - the same body, two
  ports.
- *Stack-built tables* (D59's aborting form): `Area174_FadeRun` `0x428F90`
  by the object's `+4` (AR4C), `Area104_DrawGauge` `0x415940` by `bar & 0xFF`
  (AR2E), `Area193_ChoiceMessageState` `0x42C470` by the choice answer (a
  negative answer reads below the stack pointer, 3 the return address's low
  word; AR4F); ours aborts in each.

By reading, no chapter or area writes an index past its own table except
chapter 11's triggers (above) and area 145's drop (above); the movement
script's op `F6` can store any run. **Ours writes the same states**; the
aborts differ only where the original would run past.

**Status:** latent; the three policies are unreconciled.

## D134 — Tables laid back to back in the chapter banks and area blocks (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC0, SC2, SC5, SC6, SC7, SC9a, SC9b, SC11, SC12,
SC13, SC15, SX, SX2, AR0A, AR0B, AR1A, AR1B, AR1E, AR2A, AR2B, AR2C, AR2D,
AR2E, AR3D, AR3E, AR4A, AR4B, AR4C, AR4D, AR4E, AR4F (their docs' defects
sections). What D133's past-end index reaches, and what an unchecked byte
index (D136) reads.

**Established:** the linker laid the tables of one chapter or area end to
end with nothing between them, so a read past one is a read of the next:

- **States into runs:** in most chapters `Scena<NN>_States` (3 entries) is
  followed at once by `Scena<NN>_Runs`, so a state of 3 runs run 0 (`Scena09`,
  `Scena10`, `Scena12`: "the state table's entry 3 is the run table's entry
  0, exactly as chapter 16's"; `Scena00_States` `0x660CE4` / `_Runs`
  `0x660CF0` "one run of words"; `Scena07_States` `0x6611CC` running into
  `_Runs` `0x6611D8`). [`scena_sc1.md`](scena_sc1.md) §2 says chapter 1's
  do "not overlap" of the same adjacency (`0x660D7C` then `0x660D88`) - the
  docs' word "overlap" means adjacency, and SC1's sentence contradicts the
  others' usage, not their layout.
- **Runs and objects into the next table:** `Scena05_Runs` (23) into
  `Scena05_Objects`; `Scena11_Runs` `0x661678` (10) into `Scena11_Triggers`
  `0x6616A0` and those into chapter 12's vtable at `0x6616DC`; `Scena13_Runs`
  `0x6617A8` (9) and `Scena14_Runs` `0x661840` (10) into the next tables;
  `Scena10_Objects` `0x661614` (13) into `Scena11_EffectRolls` `0x661648`;
  `Scena04_ObjectHandlers` `0x661004` (1) into `Scena04_Cells` `0x661008`;
  `Scena09_Runs` `0x661408` (17) into `0x66144C` and `Scena09_Objects`
  `0x661450` (16) into the bytes at `0x661490`.
- **Byte tables into hooks:** `Scena00_StartSteps` `0x660D30` (8) into
  `Scena00_InputAnims` `0x660D38`; `Scena02_EffectX` `0x660E48` (8 s8) into
  `Scena02_Hooks`; `Scena05_MemberBytes` `0x661018` (8) into `Scena05_Hooks`;
  `Scena06_MemberBytes` `0x6610D8` (8) into `Scena06_Hooks`.
- **The staff roll:** `Scena17_RollLines` `0x661A20` holds 351 lines and a
  -1, and `Scena17_ScrollRoll` `0x56D110` reads 367 (the window draws
  fifteen lines after the top one): indices 352..366 are zeros, one more
  line pointer at 356, and at 366 `Scena17_Hooks`' first word, which
  `Scena17_DrawLine` `0x56D1A0` draws as a string - `0x56C130`'s code bytes
  (in this process the detour's `jmp`) at y 309..330, below the screen.
  `Scena17_DrawLine`'s glyph index is unchecked against the 64 widths of
  `Scena17_GlyphWidths` `0x66208C`: any character outside its classes reads a
  width from `Scena17_EndSteps` `0x6620CC` and the strings after it.
- **Area state tables:** area 135's four are contiguous (`Area135_QueueStates`
  `0x62DA0C` (2), `Area135_QueueCells` `0x62DA14`, `_WalkStates` `0x62DA18`
  (2), `_LiftStates` `0x62DA20` (9), `_SpawnStates` `0x62DA44` (2), then a
  zero), so the walk's states 2..12 run the lift's and the spawn's handlers
  and the lift's 9 and 10 the spawn's - 13 and 11 code pointers, which ours
  reaches and aborts past (AR3D, [`area_w3d.md`](area_w3d.md) §1.4, §6);
  area 172's fall table `0x63EF14` (2) runs into its slide table `0x63EF1C`
  (2), that into `Area172_DriftSteps` `0x63EF24` (16 signed bytes), and its
  effect table `0x63EF34` (3) into area 173's data block `0x63EF40` -
  `Area172_FallStep` `0x427CB0` lands at state 2, so `Area172_RunFall`
  `0x427C10` runs the slide's code (kept, ours too) and `Area172_RunSlide`
  `0x427DF0` on such an object reads four drift steps as an address outside
  `.text` (a fault; ours aborts) (AR4B, [`area_w4b.md`](area_w4b.md) §6);
  `Area141_ShadeDownStates` `0x62F618` into `_ShadeUpStates` `0x62F620` into
  `Area141_CellEntries` `0x62F628` (AR3E); `Area32_StatesA` into `_StatesB`
  (AR0C); `Area104_LeaderStates` `0x61BC28` into `_Kind5CStates` `0x61BC30`
  (AR2E); `Area175_GlideStates` `0x6424A4` into `_GlideSteps` `0x6424AC`
  (AR4D); `Area189_LeaderStates` `0x6475A0` into `Area189_StepVectors`
  (`0xFFFFF4B0` first; AR4E); `Area197_ShakeStates` `0x649818` into
  `_ShakeSteps` `0x649820` (AR4F); `Area79_States` into `Area79_SlideSteps`
  (AR2A); `Area99_DriftSteps` after area 99's tables (AR2D).
- **Choice arrays into handler arrays:** area 167's choices `0x63C510` are
  five dwords before its handlers `0x63C524`, so choices 5..15 are handlers
  0..10 (AR4A, [`area_w4a.md`](area_w4a.md) §1); area 148's choices
  `0x635364` start two dwords before its handlers `0x63536C` (AR3G §1);
  `Field_ObjectTriggers` `0x662E1C`'s id 0 is `WorldMap_FieldHooks[11]` =
  `Area97_FlagIfKeyItem5` `0x4139E0`, so an object with trigger id 0 would
  set story flag `0x23` when key item 5 is held (AR2D).
- **Data read as the next table's:** the world-map copies' name set 3 (an
  id in no set) reads `Area<NN>_PlateStates`' code pointers as item ids
  (areas 16, 45, 87, 88, 115: D143); `Area152_PlaceRows` `0x637344`'s row 3
  is `Area152_PlateStates` `0x6373A4`, the message id then half a code
  pointer (AR4A); `Area174_PoseTables` (3) into `Area174_StepDeltas` (AR4C);
  `Area191_TalkMessagesA` / `B` `0x647C24` / `0x647C54` and
  `Area192_TalkMessages` `0x647F68` / `_TalkMessagesF` `0x647F98` /
  `_TalkWho` `0x647FC8`, each read into the next for a "who" in none of its
  keys (AR4E, AR4F); `Area65_Cells` `0x6043F4` (182) into area 65's choice
  table (AR1E); `Area41_GiveMessages` (2) into `Area41_States` (AR1A);
  `Area111`'s 28-byte grid `0x675C00` into areas 112 and 113's descriptor
  records at `0x675C20` (a write, AR2F, D136); the engine's
  `BattleFormation_Offsets` (16 pairs) into `BattleFormation_Anims` and
  `Inventory_Remove`'s list pointers into the consumables' names (SX2, SX;
  D136).
- **The world-map copies' buttons:** `Area45_Buttons` `0x5F76E8` and areas
  87 / 88's are six entries of which "the second legend reads eight"; area
  16's doc gives `Area16_Buttons` `0x5E5FF0` as 6 x 4 with no overread of
  the same code (AR0B against AR1B / AR2B).

**Status:** latent. Each read stays inside the image by reading; the two
that would not (area 172's slide at state 2, area 91's state 7 and up) are
D133's aborts.

## D135 — `Effect_FindFree`'s "none" is stored and used as a slot in the chapters and areas (latent; ours aborts in some, writes the same in others)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC1, SC3, SC7, SC9a, SC9b, SC11, SC13, SC15, SX2,
AR1B, AR2B, AR2C, AR2E, AR3B, AR3D, AR3G, AR4A, AR4C, AR4E (their docs'
defects sections). D60's class in the field; D93 is the spell overlays'
version.

**Established:** `Effect_FindFree` answers 0..19 or `0xFF`. A scene or
handler that keeps the answer in a byte and reads or writes the record it
names without testing `0xFF` reaches "record 255", `Effect_Objects +
0xFF x 0x80` = `0x7E9160`, `0x7F80` bytes past the twenty records (which end
at `0x7E1BE0`) - inside the image's `.bss`, so a silent read or write, not a
fault; kept in an s8 it is record -1, `0x7E1160`, before the pool.

- **Chapter writes:** `Scena01_Scene02` `0x53A380` step 4 clears byte 0 of
  the record slot `0x903850` names (whatever the last writer of that scratch
  cell stored); `Scena07_TakeEffect49` `0x550CE0` answers record 255 when
  none is free and `Scena07_Scene6` `0x54FFE0` writes a byte and nine dwords
  through it (`0x7E9160 + 6..+0x23`, `+0x34..+0x3F`); `Scena11_Scene5`
  `0x55CCF0` step 9 stores the slot into member 0's `+0xB` through
  `Sprite_Current`; `Scena14_Run4` `0x565BF0` step 0x10 writes `+1` of the
  record `Scena14_Slot` `0x6BC73C` names; `Scena15_Run2` `0x568980` step 1
  writes 2 to `0x7E9161` and `Scena15_Run4` `0x5690C0` step 0xB counts that
  byte up when the slot byte `0x6BC743` is `0xFF`. **Ours writes the same
  places** (the fuzzes' recorders answer 0..19).
- **Chapter reads** (waits on the record's live bit): `Scena04_Scene1`
  `0x545340` step 4 (the s8 `0x8034E3`: record -1 before the pool; SC3),
  `Scena07_Scene4` `0x54FB40` and `Scena08_Scene6` / `8` / `10` (`0x552120`,
  `0x5525B0`, `0x552EF0`; SC7), `Scena09_Run11` `0x556C40` step 7 (only a
  failed take in step 6 leaves `Scena09_Slot11` `0x6BC730` so, and step 6
  does not advance on one: unreachable; SC9a), `Scena10_Slot` `0x6BC735` in
  runs 1, 3, 4 and 6 (`0x558600`, `0x558FC0`, `0x559500`, `0x559B20`; four
  waits follow a store that does advance on `0xFF`; SC9b), chapters 13 and
  14's waits on `Scena13_Slot` `0x6BC738` / `Scena14_Slot` (SC13),
  `Scena15_Run1` / `3` / `6` (`0x568650`, `0x568A70`, `0x569F20`; SC15). A
  read only, both sides the same.
- **Area writes, ours aborts:** `Area85_SpawnEffect47A` / `B` index
  `Effect_Objects` by the slot they stored in `+0xB` (AR2B, ours aborts past
  twenty); `Area104_Kind5CStart` `0x4157A0` spawns four effects without
  testing none and `Area104_Tail40` `0x415BE0` tests none but sign-extends
  the slot (AR2E, ours aborts outside 0..19); area 135's tail
  `Area135_Tail18` `0x41E630` writes through the slot `0x675CC0` in six
  states after `Area135_ChoiceStartTail14` `0x41DB40` stored none there (the
  original writes `0x7E9161` in each; AR3D, ours aborts at 20 or more);
  `Area174_EffectState3` `0x4289E0` writes `+1`, `+0x5D`, `+0x5E` of the
  record the object's `+0xB` names after `Area174_Effect9DSub0` `0x4288D0`,
  `_EffectA2State0` / `_State2` `0x428B80` / `0x428D50`, `_EffectA1`
  `0x428C10` or `_EffectA3` `0x429120` stored none (AR4C, ours aborts;
  "which script orders the handlers is not read").
- **Area writes, ours reproduces:** `Area121_Kind5CStart` `0x41C1B0`'s four
  stores land in record 255 with no free slot (AR3B, "S09's breath pool is
  the precedent" - D93's silent case). **The docs contradict each other
  here**: AR3B reproduces the write to `0x7E9160` as "a silent write into
  mapped memory", AR3D and AR4C abort on the same write "past the 20
  records"; the shared body (`Area104_Kind5CStart` `0x4157A0` is the same
  code in area 104's copy, where ours aborts) has two ports.
- **Area reads:** `Area42_TimerTail` `0x406A30` (tail kind 7) states 3 and
  5 test the record `0x9039F5` names after states 2 and 4 stored none there
  - the tail waits on bit 0 of the byte at `Effect_Objects + 0x7F80` (AR1B);
  `Area148_Tail31` `0x422510` state 0xB reads by the same shared byte, which
  every tail kind writes (AR3G); `Area188_WalkWhileZUnder` `0x42A450` reads
  `Area188_ZLimits` `0x647530` (2 bytes) by a sub-kind that
  `Area188_ChoiceByChapter` `0x42A380` sets to `0xFF` - the byte at
  `0x64762F` (AR4E). Area 92's spawns `Area92_SpawnKind4AtMember0`
  `0x412AA0` .. `_SpawnKind1AtMember2` `0x412ED0` store the slot before the
  test, so a full pool leaves `0xFF` in the party record's `+0xB` where
  `Effect_Spawn`'s callers leave the old slot (op `9F` waits on it; AR2C).
- **`Sprite_ObjectAt`'s none, the same shape:** `Area121_PushObject`
  `0x41BC60` indexes `Sprite_ObjectsExtra` by the answer less `0x1E`: with
  the push button held and no object two steps ahead it reads "extra record
  `0xE1`" (`0x80B0A4`, in `.bss` before `Gfx_ClutStripSource`) and, when
  that byte's low three bits equal the leader's facing, sets bit 0 of
  `0x80B124` - **reachable in area 121's leader state 12 whenever a push
  meets no object**, reproduced (AR3B, [`area_w3b.md`](area_w3b.md) §7.1);
  its copy `Area104_ObjectAhead121` `0x4152B0` is dead in area 104 and ours
  aborts there (AR2E). Area 135's tail states `0x1B` / `0x1D` write the field
  object the sub byte names; `0x1D` does not test `0xFF` (AR3D, ours aborts).
- **Tested but consequential:** `Effect_HoldFlag1C` `0x469FE0` with every
  record in use sets nothing, and its callers do their work first
  (`Area49_CellHook` toggles the switch's flag before the call), so a switch
  fired with all twenty records busy has no cool-down and can fire again
  next frame (SX2). `Area152_Record8Spawn` `0x4253C0` (record-8 state 0 of
  all ten world maps) stops on `0xFF` and ours also aborts on 20..254, which
  `Effect_FindFree` never answers (AR4A).
- **Not cases**, though their docs say "unchecked": the spawns of AR2D,
  AR2F, AR3C, AR3E, AR3F, AR4D and AR4F, and SE's `Effect_SpawnAtCell`
  `0x524870`, all test `0xFF` (SE's control C8 refused a second `FindFree`
  on `0xFF`); "unchecked" there means no range test below 20.

**Status:** latent: twenty live effect records at a spawn were not seen.
The AR3B / AR3D policy split is one read to settle.

## D136 — Reads and writes by a signed choice byte, a party list byte or the member count, unchecked (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, by every group of the scenario and area rounds but SC12
(their docs' defects sections). One entry for the class; where a read stays
inside `.data` ours reads the same, where a write would leave what it
indexes ours aborts (each group's doc says which).

**Established:**

- **The choice tables by the s8 answer.** Every `Area<NN>_Choice*` handler
  that picks a message word or a focus pair by the message box's answer
  indexes its table by the signed byte with no bound: a negative answer
  reads before the table, one past its rows the words after (D134). Areas
  0..15's tables, 21, 26, 41 (`Area41_ChoiceGiveItem` `0x406280`), 42, 49,
  53, 57, 67, 68, 77..81 (area 79's choice 3 reads before its bound), 90,
  92, 94, 100, 117, 130, 133, 136, 150, 153, 154, 166, 168, 187, 188, 191,
  192, 193 and chapter 0's `Scena00_StartSteps` `0x660D30` by `Cond_ByteFD`.
  All in `.data`; kept.
- **The effect-kind and argument tables by a party list byte** (a character
  id, 0..7 in play): the `Area<NN>_Spawn*AtMember*` handlers of areas 11,
  26, 49, 57, 67, 68, 77, 92, 94, 117..119, 130, 133..136, 175;
  `Scena03_SpawnAtMember` `0x544830`, `Scena05_SpawnMember` `0x54A1E0`
  (`Scena05_MemberBytes` `0x661018`), `Scena06_LeaderEffect` `0x54E2F0` and
  `Scena06_Run14` `0x54D4F0` (`Scena06_MemberBytes` `0x6610D8`),
  `Scena02_PlaceEffect70` / `68` `0x541B90` / `0x541C50` (`Scena02_EffectX`
  `0x660E48`). Kept.
- **Member records by a whole byte:** `CharacterRecords + 0xA4 x
  MoveScript_EffectState[k]` in the call tables, chapters 6, 8 and 14
  (`Scena08_Scene1` / `11` `0x551590` / `0x553050`, `Scena14_Run2` / `3`
  `0x565240` / `0x565910` step 0x11 / 9 by `0x669734`), `Scena01_Scene06`
  `0x53AC30` step 0xF (a byte of 8 or more clears a bit in `Cond_Flags`
  `0x903F90`), `Scena02_StripMember` `0x542080` (ten bytes at `0x903AEE +
  0xA4 x member`; its caller passes 3 and 4), `Party_AddToLists` `0x591CC0`
  (an id of 24 or more reads past `MoveScript_EffectState`'s 24 and ORs 3
  into a record the byte after names; SE), `Char_LevelUp` `0x498DE0`
  (`CharacterRecords` and `Char_ExpTable`; SX), `Member_SetState2_8`
  `0x57C8A0`, `AbilityList_ForType` `0x591EC0`, `Party_PlaceInFormation`
  `0x532FD0` (a formation past `BattleFormation_Offsets`' 16 pairs; at
  `0x904060` = `0xFF` with two members the group wraps to 0; SX2),
  `Area191_TalkMessage` `0x42BA90`, `Area192_TalkMessage` `0x42C0A0` (the
  level byte of a record past the eight), `Area144_SpawnEffect85`
  `0x421090`.
- **Walks to `Field_MemberCount`, unchecked against `ObjTrio`'s three** (1..3
  in the field): reads in `Area85_FollowMember2`, `Area86_MemberNear`,
  `Area103_FirstListedMember` `0x414540`, `Area108_PlaceScene` `0x4169A0`,
  `Area112_TalkByMember` `0x418400`, `Area112_MemberNearBoxes` `0x4187C0`,
  `Area135_PartyInBox` `0x41EEF0`, `Area148_MemberMessageA` / `B` `0x4223A0`
  / `0x422430`, `Area173_MessageByMemberA` / `B` `0x428450` / `0x4284E0`,
  area 145's member search; **writes** in `Area117_MembersFrame` /
  `Area118_MembersFrame` `0x41A0A0` / `0x41A410` (`+0x128`, `+9`, `+8` past
  the three; a member index of 8 or more is never marked, the mask being an
  8-bit shift), `Area192_RestoreCharacters` `0x42C2D0` (`0xA4` bytes a
  record into the party's `+0x80` blocks), `Scena14_Run7` `0x5670E0` step
  0x17 (`0xA4` bytes into ObjTrio per member after the leader),
  `Scena06_Run08` `0x54C480` step 0 (the count's bytes from `0x904065` to
  `0x939A10`: above 3 the flag bytes after the list), `Scena10_Run7`
  `0x55AB00` step 1 (from ObjTrio `+0x89` to `0x903A10..`; above 8 past what
  `Scena10_TallyMet` `0x559930` reads), `Party_HealJoined` `0x533E50` and
  `Party_PlaceForBattle` `0x532ED0` (ObjTrio records past the three; SX),
  and D150's two. `Party_Join` compares the count with 3 before adding
  (`0x533F2A`); what else writes it was not surveyed.
- **`Area_Descriptors[Game_AreaNumber]`** with no bound at 200:
  `Scena00_Steer` `0x539950` and `Scena00_Run11` `0x5395D0` (a write through
  the block's `+0xC`), `Area77_LeapStep` `0x40F090`, `Area104_Kind5CStart`
  `0x4157A0` (runs only in area `0x68`), `Area121_Kind5CStart` `0x41C1B0`,
  `Area175_OpenScriptMessage` `0x429540`: **ours aborts past 199** in each.
- **A movement-script counter as an effect index:** `Scena01_Scene03`
  `0x53A5E0` step 0x15, `Scena02_Scene09` `0x53F120` step 3 and
  `Scena09_Run6` `0x554D40` steps 0x22 / 0x2C wait on `Effect_Objects[counter
  3]`, the slot the step before stored, which a script counter op between
  moves to any of 256 records; `Scena08_Scene4` `0x551DE0` reads the sprite
  counter 3 names. Reads only.
- **A pointer divided by `0xA4` into a byte:** `Scena08_Scene8` `0x5525B0`
  step 0x1A (from the pointer `0x903804`, a signed division, any pointer;
  step 0x1B then clears `Sprite_Objects[Scena08_Kept]` `0x6BC720`),
  `Area59_Trigger24` `0x40B470`, `Area68_Trigger25` `0x40D300`,
  `Area98_Trigger44` `0x413C20` (counter 3 from the focus object: one of the
  four extra objects or the leader gives an index past the thirty), area
  75's object bytes `0x93C350` / `0x93C351` from `ObjectIndex(Field_ActiveMember)`
  (`Area75_MarkObjectA` / `B` `0x40D820` / `0x40D7F0`; then indexed for
  writes by area 75's handlers), and `Area01_ActiveMemberToVar6` `0x4010D0`
  / `Area08_SpawnEffect54` `0x4015B0`, which divide `Field_ActiveMember -
  Sprite_Objects` by `0xA4` though `Field_ActiveMember` points into
  `Sprite_ObjectsExtra` `0x802000` - not a whole number of records after
  `Sprite_Objects` on the PC (`0x23180 / 0xA4` = 876.5), so the answer is
  `0x6C` plus the slot, not an object index (AR0A: "not checked against the
  PSX twins `0x801F2C88`, `0x801F443C`, where the objects may be
  contiguous").
- **Other unchecked indexes that write** (ours aborts where the write would
  leave what it indexes): `Area16_Record4MarkCell` `0x402770` and
  `Area33_Record04Start` `0x404820` (`Area<NN>_Cells` by `+0xB`, then
  `AreaMap_Bytes[width * z + x]` with no bound; kept, the fuzz seeds the width
  small), `Area44_PushParty` `0x4077F0` (`Field_DirectionSteps`,
  `Sprite_ObjectsExtra`; aborts), `Area111_PlaceMemberInGrid` `0x4178F0`
  (`0x675C00 + x / 2 + z * 4` for an object off the 28-byte grid; aborts),
  `Area111_StepToExtra` `0x417C10` (the extra objects by a dword `+0x18`;
  aborts outside `.data`), `Area111_ArmTailAtLeaderCell` `0x418130` (a leader
  cell of nibble 0 writes extra record `0xFF`'s `+8` at `0x80C364`, a wall
  record 14's - the doc gives `0x8028F0`, the stride gives `0x802900`; kept),
  `Area145_PartyRecord16` `0x4211C0` (`MoveScript_PartyRecords` record i for
  any byte: records 6 and 7 lie over `Cond_ByteFA..0x8034FF`; kept),
  `Area104_BuildMinimap` `0x416020` (the upload queue at
  `Gfx_UploadQueueCount` unchecked against twenty; aborts), `Area42_Init`
  `0x406E90` (by `Area42_Rank` `0x675A00`, 0..2 by both writers),
  `Scena13_Run6` `0x5633F0` step 0x19 (a byte through the pointer
  `0x903804`), `AreaMap_SetHeight` `0x572620` (any s16 x, z; SX2).
- **Engine reads by a whole byte:** `Field_StartEventBattle` `0x4410B0`
  (`EventBattle_Records`, length unknown; SE), `Inventory_Remove` `0x591B60`
  (a category above 4 takes the count list's pointers and the consumables'
  names as lists, D35's neighbourhood; SX), `MapView_FillCells` `0x56FCA0`
  (wraps only at the exact edge: a row above 0x37 or a column above 0x1B
  counts on past the table; SX), `Sprite_FlashClut` `0x534DB0` (a colour of 4
  or more reads its own frame - the return address's halves, then the
  caller's; every caller passes 0..2 or a byte of `Field_FloorDamage`'s
  table; **ours aborts**; SX), `EventOp_0x` `0x57A010` (the count word
  compared signed, `cmp ax, 0x1E; jge`, so `0x8000..0xFFFF` indexes
  `Sprite_Objects` backwards; `EventOp_1x` / `2x` share it; SE),
  `Scena15_Battles` `0x6618C0` by `Scena15_BattleSetup` `0x568510`'s n and the
  effect's `+0xB`, `Area121_PushObject`'s facing byte into
  `Field_DirectionSteps` and `Area121_LeaderControl` `0x41B9F0`'s `Field_State
  +0x148` into `Field_ActorStates` (as `event_ops.cpp`'s leader code),
  `Area148_BeamStart` `0x422860`'s unmasked `+0xB` as a facing and
  `Area148_BeamTurnMembers` `0x422A50`'s `+9 >> 3`, `Area189_StepBegin`
  `0x42ADB0`'s whole facing byte into sixteen vectors (kept below 16 by its
  writers), `Area48_ToggleFlagD` / `_BumpCount` `0x409040` / `0x409080` and
  `Area108_FlagIfEffectState5` `0x416EC0` (`MoveScript_EffectState`, 24
  bytes, by the leader's `+0x89`), `Area41_TintUp` / `_TintFall` `0x406560` /
  `0x4064C0` (`MoveScript_TintRecords` by `Field_State +0x149`, as ops `C1` /
  `C2`), `Area40_DrawGrid` `0x405ED0` (`DrawItems`, `0x24000` bytes, by
  `MapView_ItemAt`'s answer up to `0xFFF`), area 91's CLUT rows by the
  object's bytes (`Area91_State4RevealClut` `0x4123D0`, `_State3Place`
  `0x412310`: past `0x1F` the rows either side, past the strip's end for the
  last), `Area75_FlyHeights` by `+0xB`, `_MoveAnims` by `old * 4 + new`,
  `_Rhythm` by `row * 15 + frames % 15`, `Area77_Leaps` by the script's
  operand and the running area's `+0x10` script table by the script object's
  `+3` (`Area77_LeapStep` `0x40F090`, as the movement-script engine indexes
  them), `Area175_OpenScriptMessage` `0x429540` (the operand by `+3` / `+0xA`,
  then `Area175_ScriptMessages` `0x6424BC` by it), `Area174_TurnRightToScript`
  / `_TurnLeftToScript` / `_FaceAwayFromLeader` `0x428DE0` / `0x428E70` /
  `0x428F00` (`Area174_PoseTables` by a script byte), `Area16_Record8Place`
  `0x4025F0` and `Area33_Record08Start` `0x4046A0` (direction and animation
  tables by `+8`), `Area130_TailGiveItem` `0x41D0E0` (an item by the byte
  `0x903F6A` nothing in its band writes; past 5 gives nothing but still opens
  message `0x49` with `Text_Records`' previous contents), `Area136_ZLimits`
  by the sub-kind, `Area140_CellRects` by the entry's rectangle byte, the
  cell steps `0x66971C` by the direction times 2, effect kind `0xA6`'s `+7`
  and `+6` into `Area198_EffectA6Records` `0x649E00` (3 of 8),
  `Area135_LeaveByExit` `0x41DC60`'s exit, `_QueueStepX` `0x41E1E0`'s count,
  `_SpawnCopy` `0x41E440`'s record. Kept: every one stays in the image.

**Status:** latent.

## D137 — The chapter and area code divides by a byte that can be 0 (latent; ours aborts at 0)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC6, SX, AR2D, AR3D and AR4E. D91's class outside the
spells; ours aborts with a message at each, as there.

**Established:**

- `Scena06_LeapStart` `0x54E7D0` indexes `Field_MoveSpeeds` (6 bytes) by
  the object's `+4`: 6 or 7 read a 0 (the leap ends at once, al 0); 8 reads
  `0x40`, so `16 / speed` frames a step is 0 and the division by frames x
  steps faults. Area 77's handler `0x40F090` passes the movement-script
  object, whose `+4` the leap reads as the speed index. `Scena06_LeapAir`
  `0x54E8E0` divides 16 by the speed again at every new step with no test:
  a speed of 0 there (the object's `+4` changed to 0, 6 or 7 during the
  leap) faults ([`scena_sc6.md`](scena_sc6.md) §6).
- `Camera_EaseAngleFB` `0x57C6B0` divides by the speed x 4 (or x 8) as a
  byte - 0 for a `Field_MoveSpeeds` entry of `0x40` (or `0x20` / `0x40` at
  x 8) or a second argument of 0 - and faults on `-2^31 / -1`; SC0's two
  calls pass 10 and 15. A negative multiplier leaves a negative frame count
  that counts down through 2^32 frames: reproduced ([`scena_sx.md`](scena_sx.md)
  §6).
- `Area95_LeapArc` `0x413750` divides by the running object's `+9` (a timed
  move's frames; 0 faults). `Area99_LeapStart` `0x413E10` divides 16 by
  `Field_MoveSpeeds[3]` and then by four times the quotient (a speed of 0,
  or above 16, faults); `Field_MoveSpeeds[3]` is non-zero in the image and
  no writer of it was found, so area 99's cannot fault in practice
  ([`area_w2d.md`](area_w2d.md) §6).
- `Area135_FallTo` `0x41EBE0` divides by its frame count `+9` = `(|d / 128|
  << 4)` as a byte (`idiv ebx` at `0x41EC18`): 0 whenever the height is
  within 127 of the object's `+0x3E`, or `|d / 128|` is a multiple of 16.
  Handler 10 `Area135_Fall780` `0x41E040` falls to `0x780` and handler 9
  `Area135_FallToFloor` `0x41DF60` to a ground or an object top: **an object
  already at, or within 127 of, that height faults the original with a
  divide error** ([`area_w3d.md`](area_w3d.md) §6) - the one case here the
  geometry alone reaches.
- `Area189_StepBegin` `0x42ADB0` sets `+9` to 8, calls the height helper
  `0x511C10` (which writes only its own arguments) and divides by `+9` read
  again: 8 in the game, 0 only from a stand-in ([`area_w4e.md`](area_w4e.md)
  §6).

**Status:** latent. **The owner, 2026-09-26, for D91: no DIVERGENCE entry**
where the original faults; the same reading was applied here.

## D138 — Eight area inits are a bare `ret` on the PC where the PSX descriptor has one (PC only, latent)

**Seen:** not seen in play; found by reading the descriptor tables while
taking the areas over, 2026-09-27..28, groups AR1D, AR1F, AR2C, AR2F, AR3F,
AR3G and AR4A ([`takeover-queue-round10.md`](takeover-queue-round10.md) §16,
§19). For the owner and the divergence map.

**Established:** the descriptors' `+0x40` (the init) of areas 56, 75, 90,
108, 145, 148, 153 and 154 all point at `0x437CC0`, the linker's shared
`ret` (the entry the chapters' object tables use for "nothing"), where the
sibling's `names/area_records.toml` gives each PSX descriptor an init of its
own: area 56 `0x801F3464` ([`area_w1d.md`](area_w1d.md) §1), area 75
`0x801F4FB4` ([`area_w1f.md`](area_w1f.md) §1; the PSX also lists 8 handlers
where the PC has 13), area 90 `0x801F2C5C` ([`area_w2c.md`](area_w2c.md) §1;
area 90's `+0x3C` is null too), area 108 `0x801F56E0` ([`area_w2f.md`](area_w2f.md)
§1), area 145 `0x801F5324` ([`area_w3f.md`](area_w3f.md) §1, §6), area 148
`0x801F4274` ([`area_w3g.md`](area_w3g.md) §1, §6), areas 153 and 154 both
`0x801F2C5C` ([`area_w4a.md`](area_w4a.md) §1). What each PSX init does was
not read; "either the port moved its work elsewhere or dropped it" (AR3F).
The PSX addresses repeat across areas (`0x801F2C5C` is area 90's and areas
153 / 154's) because each overlay loads at the same base. Nothing in the
bands stands in for them; ours has nothing to inject at a `ret`.

**Status:** latent, PC only; unread on the PSX side. A DIVERGENCE entry
would restore an init the port dropped, once one is read and found to do
something a player sees.

## D139 — Area 198's handler 9 always plays sound effect 0 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group AR4F ([`area_w4f.md`](area_w4f.md) §1, §6).

**Established:** `Area198_SpawnDrops` `0x42D280` (area 198's handler 9, PSX
`0x801F35FC`), every eighth frame with counter 0 above 2 and `+0xB`'s bits
0..2 clear, calls `Sound_PlayEffect` with an argument computed by `and eax,
8; add eax, 0x203; neg eax; sbb eax, eax; inc eax`: the value before `neg`
is never 0, so the result is always 0. What the PSX twin passes and what
`Sound_PlayEffect(0)` does on the PC are not read; ours plays 0 too. For
the owner's ear in area 198.

**Status:** latent.

## D140 — The camera turn steps take the step as an s8, and the turn test does not wrap (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group SX2 ([`scena_sx2.md`](scena_sx2.md) §6).

**Established:** `Camera_TurnStep` `0x57C5A0` and `Camera_TurnStepFB`
`0x57C650` read their step as a signed byte, though the degree wrappers
`Camera_TurnToDegrees` `0x57C550` and `Camera_TurnFBToDegrees` `0x57C600`
pass `x * 4096 / 360` as a dword: a step of 12 degrees or more (136 and up)
turns the other way, and its end test runs the other way too. Every caller
passes 2 or -2 degrees (22 steps). The end test compares `angle & 0xFFF`
with the target as numbers, without wrapping, so a step whose sign points
the long way round (a positive step toward a smaller target) ends the turn
at once and snaps the angle to the target. Reproduced.

**Status:** latent; by reading, no caller reaches either.

## D141 — `Sound_SetCueVolume` of a cue with bank bits 0 reads before `Sound_Banks` (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group SX2 ([`scena_sx2.md`](scena_sx2.md) §6).

**Established:** `Sound_SetCueVolume` `0x587890` indexes `Sound_Banks` by
the cue's bank bits with no test: bank 0 reads `0x384` bytes before
`Sound_Banks` for the cue and its voices and hands whatever non-zero dword
it finds there to `SetVolume` as a buffer; banks 7..15 read past the six
records. Its one caller passes `0x207` / `0x208` (bank 2). Reproduced.

**Status:** latent.

## D142 — `Area53_Trigger42` answers whatever `ScriptFlags_Set40` left in eax (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group AR1D ([`area_w1d.md`](area_w1d.md) §1, §2, §6).

**Established:** object trigger 42, `Area53_Trigger42` `0x40AB40`, calls
`ScriptFlags_Set40` and returns without setting `al`; its caller `0x56E020`
returns that eax to `Field_ObjectTrigger` `0x56D6B0`, which returns it too
(neither reads it; their callers were not read). Capcom's `ScriptFlags_Set40`
leaves `Field_StatusBits | 0x40` in `al`; ours (group C's, declared `void`)
leaves whatever its compiled code does. Ours calls it through a pointer
typed to return `unsigned` and returns that - exactly what the original
returns with either `ScriptFlags_Set40` in place - and the fuzz compares it
(`ret_mask 0xFF`). Its siblings `Area55_Trigger23` `0x40ABF0` and
`Area61_Trigger21` `0x40B5C0` answer al 0.

**Status:** latent; nothing read uses the answer.

## D143 — The world-map copies inherit area 33's searches and reads with no bound (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups AR0B, AR1B, AR1E, AR2B, AR2E, AR3A, AR3B, AR3G and
AR4A (their docs' defects sections). D74's shape in the other ten copies of
one body (areas 16, 45, 65, 87, 88, 104, 115, 121, 151, 152 over their own
tables; [`takeover-queue-round10.md`](takeover-queue-round10.md) §19).

**Established:** each copy's `Area<NN>_PlaceMessage` (`0x401B80`, `0x407B40`,
`0x40B8C0`, `0x40FC60`, `0x410D90`, `0x418BE0`, `0x424A30`, ...) searches
its cell table and its plate animation table with no end test: a leader
cell in no record reads on through `.data` (area 87's `0x611CC0` ends in a
zero record that stops the search only for cell (0, 0); area 88's `0x61271C`
and area 121's `0x6236A0` and 151's `0x636A84` run straight into the
descriptor's data), and a place in no plate entry the same (area 104's
table is one entry and a zero, 121's four, 151's seven, 152's three). A
found cell whose id is in neither name set reads "set 3" from the plate
state table's code pointers as item ids (D134). `Area<NN>_Record4MarkCell`
indexes the cells by `+0xB` and writes `AreaMap_Bytes[width * z + x]` with
no bound, and `_Record8Place` reads its direction and animation tables by
`+8` (D136). The dispatches are D133's (area 16's copy reads on, the rest
abort). The shared bodies `Area152_PlateStart` `0x424BA0` (area 151's plate
state 0) and `Area152_Record8Spawn` `0x4253C0` (record-8 state 0 of all ten)
are one function each in ours. Kept: the fuzz keeps inside the tables, and
the world-map route plays areas 33, 45, 88, 104 and 115 without meeting one.

**Status:** latent. Sharing the five copies of the body in ours once is
[`round-10-cleanup.md`](round-10-cleanup.md) item 1's last bullet.

## D144 — Walks over a map header or a patch chain that never end on a bad entry (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups ARH ([`area_011.md`](area_011.md) §6), AR1A
([`area_w1a.md`](area_w1a.md) §6), AR1C ([`area_w1c.md`](area_w1c.md) §6)
and AR2C ([`area_w2c.md`](area_w2c.md) §6). D74's class; `MoveCmd_TestFB`'s
hazard ([`field-misc.md`](field-misc.md)).

**Established:** `Area11_DimBackdrop` `0x4017C0` and `Area51_TintBackdrop`
`0x409DA0` walk the area's map header as `AreaMap_HeaderPass` does - a zero
dword ends it, byte `+2` is the step in dwords - and never end on an entry
of another kind whose step byte is 0. `Area94_InitPatches` `0x413550` walks
a patch chain with no bound: a chain with no zero dword runs on through
memory (AR2C cites the same function for a different failure than ARH and
AR1C describe: the step-0 case and the no-terminator case are two hazards
of one walk). `Area40_TileLit` `0x405BF0` has `MoveCmd_TestFB`'s shape: a
record step of 0, or one that steps past the run's last dword, walks on for
ever (reads only: a hang, not a fault); it also assumes `MapView_Row` /
`MapView_Column` keep the ring cell inside `MapView_Cells` after one wrap.
Ours walks on as the original; the fuzzes build steps of 1..3 and chains
that end.

**Status:** latent: the shipped headers and chains are well-formed by
reading.

## D145 — The PC keeps an area's `.data` across visits where the PSX reloads the overlay (PC only, latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups AR1C ([`area_w1c.md`](area_w1c.md) §6) and AR2F
([`area_w2f.md`](area_w2f.md) §6).

**Established:** two area blocks write their own `.data`: `Area52_TailBlock`
`0x40A670` sets byte `+1` of the descriptor's first extra-object entry
(`0x5FE049`) to 0 when a timed scene begins and `0xFF` when it ends;
`Area108_TailPlace` `0x417210` moves `Area108_Model`'s flags byte and its 33
records' corners by the scene. On the PSX the overlay is reloaded with the
area, so each visit starts from the disc's bytes; on the PC every overlay is
linked into one image and the value stays until the next write, so a later
entry to area 52 sees the last scene's byte and a second visit to area 108
starts from the last scene's positions. Not measured in play; the fuzzes
seed both.

**Status:** latent, PC only. A DIVERGENCE entry would reset the two on
entry, if a second visit is found to differ.

## D146 — Choices that leave the message word as they found it (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups AR0C ([`area_w0c.md`](area_w0c.md) §6), AR2A
([`area_w2a.md`](area_w2a.md) §6) and AR4D ([`area_w4d.md`](area_w4d.md) §6).

**Established:** `Area37_ChoiceTail2E` `0x4051F0` (choice 21) leaves the
message word for an answer of 3 or more where every other choice there
writes it; `Area79_ChoiceCounter3` `0x40F2E0` leaves it on every path where
the other choices store `0xFFFF` or a word; choice 27 of areas 175..185
(`Area175_ChoiceMessageByAnswer` .. `Area185_ChoiceMessageByAnswer`,
`0x4292C0` .. `0x429D10`, eleven copies) leaves it for an answer outside
0..4. `MsgBox_ChoiceCommit` reads the word after the choice
([`item-use.md`](item-use.md) §5), so it sees the message before the choice.
Faithful; what the commit then does with a stale word is not read.

**Status:** latent.

## D147 — Uninitialised bytes passed on in the chapter banks and area blocks (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups CALLS, SC15, AR1F, AR2F, AR3A, AR3E and AR4A (their
docs' defects sections). D101's class.

**Established:** `ScenaCall_SaveAndLeave` `0x51A300` and
`Scena15_LeaveAllParty7` `0x51AB50` gather the field members' ids in a
4-byte local that is the caller's pushed `ecx` and read three bytes whatever
the count, so with fewer than three members the rest are `ecx`'s bytes -
through the call tables the entry's index (chapter 9's B[6], chapter 10's
B[3], chapter 15's A[0]), so `Party_Remove` `0x534030` is handed 6, 3 or 0;
ours reproduces it through a naked entry. `Area75_DrawCounters` `0x40E5F0`
pushes the colour as a dword whose upper bytes are its own stack
(`Text_DrawString` reads the low byte). `Area108_PlaceScene` `0x4169A0`'s
drop-in argument has the stack in its upper bytes (`Party_DropIn` reads the
byte). `Area116_EffectB8Ring` `0x419E40` hands `Area146_DrawGlowCylinder`
`0x4220D0` a 16-byte block whose fourth dword it never writes; the callee
reads three, and ours passes 0, as `Area100_EffectB7Ring` `0x4144B0` does.
`Area152_Record8Spawn` `0x4253C0` reads its count as a stack dword with
three unwritten bytes (masked to the nibble). `Area140_CellHook` `0x41F9B0`
reuses its argument slots for the member count and a predicted x that no
caller reads; `Area140_StepHook` `0x41F960` reads z's high word as a dword
at `[esp + 0xA]` that runs into the caller's frame; `Area140_FollowAndAct`
`0x41F650` passes x and z in registers whose high halves are the caller's.
The staff roll's draws (`Scena17_DrawLine` `0x56D1A0`) carry s16 x and y in
registers whose upper bits are whatever they held; only the low 16 bits are
read. Ours computes the same low bits and the fuzzes compare only them.

**Status:** latent, harmless by reading.

## D148 — `Scena08_EnterArea` sets the wrong flag in area 0x15 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group SC7 ([`scena_sc7.md`](scena_sc7.md) §7).

**Established:** where run 10's scene is armed in area 0x15 (flags 0xF and
0x13 set, 0x14 not), `Scena08_EnterArea` `0x5510A0` pushes `0x14`, then `dl`
= the low byte of the row pointer, then the row, and calls `Flags_Set`, which
takes two arguments: it sets flag (row pointer & 0xFF) of the row - with
chapter 8's row `0x903FD0` flag 0xD0, bit 0 of `0x903FEA` (chapter 11's row,
its flag 0x10) - and never 0x14. `Scena08_Scene10` `0x552EF0` sets flag 0x14
itself at its end (step 0xC), so a finished scene does not repeat; left
unfinished, it arms again on the next entry. Kept (control E8b shows the fuzz
tells it apart); the PSX twin `0x801FA3E4` (a hypothesis) is not read.

**Status:** latent.

## D149 — `Scena10_Party75` sends every second member away (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group CALLS ([`scena_calls.md`](scena_calls.md) §6).

**Established:** `Scena10_Party75` `0x51A3A0` sends member `i` away while
`i < Field_MemberCount`, rereading the count after each call; `Party_Remove`
`0x534030` lowers the count and moves the later field members down one, so
with three members the first and third leave and the second stays, in the
lists and on the field, when the new party joins. Kept.

**Status:** latent; whether chapter 10 ever reaches it with three members is
not measured (no route plays chapter 10).

## D150 — A member count above 4 smashes the return address of two call-table entries (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group CALLS ([`scena_calls.md`](scena_calls.md) §6, §7).

**Established:** `ScenaCall_SaveAndLeave` `0x51A300` and
`Scena15_LeaveAllParty7` `0x51AB50` copy `Field_MemberCount` member ids into
a 4-byte local (D147): a fifth byte lands on the return address and more on
the caller's frame, and the original returns to what it wrote. Ours does
everything the original does before that return and aborts with a message
(the owner's rule for an unchecked index; the fuzz's seed stops at 4, since
the clone would jump through the smashed return). `Party_Join` compares the
count with 3 before it adds a member (`0x533F2A`); what else writes the
count was not surveyed.

**Status:** latent.

## D151 — `Char_LoseHp` tests the wrong record for low HP (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group SX ([`scena_sx.md`](scena_sx.md) §6).

**Established:** `Char_LoseHp` `0x537480` hurts the member its argument
names but sets the `0x2000` state bit from the HP of the record `Field_State
+0x148` names, and marks `Field_State +0x90`. When `Field_Bit80Tick` hurts a
member other than the one `Field_State` is (its bit-0x80 walk over the party
list), the leader's HP decides. Reproduced; whether it shows is the owner's
to see (poison or floor damage on a party member who is not the leader).

**Status:** latent.

## D152 — `Party_Remove` of an id in no list writes slot `count` (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27, group SX ([`scena_sx.md`](scena_sx.md) §6).

**Established:** `Party_Remove` `0x534030`, handed an id that is in neither
list, writes `0xFF` at the list's slot `count` (the second list's slot 3 is
`0x904068`; the first list's slot 3 is the second list's slot 0), and at a
count of 0 clears `+0` of the `0x14C` bytes before `ObjTrio` (`0x802BF4`)
and passes `Member_ClearState(0xFF)`. D147 and D149 are callers that can
hand it such an id. Reproduced.

**Status:** latent.

## D153 — Hooks and choices that arm a scene without `ScriptFlags_Set40`, or answer 0 after arming (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC2, SC9b, AR2F, AR4B and AR4E (their docs' defects
sections).

**Established:** each is the odd path of a hook whose other paths set the
script flag and answer 1:

- `Scena02_StepHook` `0x5420C0`: area 0x1B's run 0xA from the rectangle at z
  `0x1C8000` (member count 2, flag 0xF clear) and run 0xF at x `0x208000`
  (flag 0x18 clear) set the run and answer 1 without `ScriptFlags_Set40`,
  where every other start sets it ("whether the scenes then run with the
  player's control is not measured").
- `Scena09_StepHook` `0x557270`: area 0x29's rectangle is open on the left
  (its x test is `je` then `jg` where every other rectangle tests a lower
  bound with `jl`), so any x up to `0x408000` with z in `0x270000..0x288000`
  starts run 0xF at step 0x64.
- `Scena10_ArriveHook` `0x55BE70`: the area-0x78 hit starts run 0xD at step
  0xF and then falls to the area-0x80 test instead of returning 1, so the
  caller sees no hook while the run is started.
- `Area111_ArriveHook` `0x417ED0` answers 0 on every path, even when it arms
  the tail.
- `Area170_StepHook` `0x427270` at x `0x218000` writes tail kind 37 and state
  60 without `ScriptFlags_Set40` and answers 0; state 60 itself calls
  `ScriptFlags_Clear40`. `Area172_ChoiceFlag12` `0x428090` calls
  `ScriptFlags_Set40` on answer 0 but arms tail kind 35 only when Cond row
  14's flag `0x12` is set, and otherwise writes `MoveScript_Var7` and leaves
  the flag to whoever clears it next.
- `Area188_ChoiceByChapter` `0x42A380` arms tail kind 10 without
  `ScriptFlags_Set40` past chapter 12, where every other arming there sets
  it; `Area143_Trigger50` `0x420A60` arms tail kind 4 without its state when
  `Cond_ByteFE` is already set, so the state byte is the last tail's. What
  the engine's tail kinds 4 and 10 do with that is not read.

Reproduced in each; what `Area_ArriveHook`'s caller does with a 0 is not
read.

**Status:** latent.

## D154 — Dead branches in the chapter banks and area blocks (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC1, SC0, AR0B, AR1F, AR2C, AR3B and AR3C. D119's
class.

**Established:** chapter 1's object handlers 6..9 (`Scena01_Object06`..`09`
`0x53D550`, `0x53D5D0`, `0x53D650`, `0x53D6E0`) test the facing with `setne
dl; xor edx, 4`, which is 4 or 5 and never 0, so the `je` past the turn is
never taken: the sprite always turns to the leader's facing ^ 4. The "none
chosen" exit of the random placers `Area20_PickFieldObject` `0x402DB0`
(`Area20_Weights` `0x5E6C4C`, not read), `Area72_PlaceRandomObject`
`0x40D4B0`, `Area73_PlaceRandomObject` `0x40D580`, and areas 124 / 125's
inits (`Area124_Weights` `0x6265BC`, `Area125_Weights` `0x6266F8`) is
unreachable with the shipped weights, which sum to 64 against a roll of
`Rand & 0x3F`. `Area91_EffectRings` `0x412590`'s clamp to `0xFF` cannot fire
(the colour is at most `0x20 + 0xC8`). `Area121_Kind5CStart` `0x41C1B0`
tests area `0x68` for the key item, but `0x462B60` never sends area 104 to
this copy. `Scena00_Run5` `0x538B10` uses `Rand % 3` as a shift count, where
a negative `Rand` (the CRT's never is) would shift the byte out.
`Scena00_Run11` `0x5395D0` step 5 sets the timer to 0 and then decrements
it (0xFFFF) when the counter `0x903849` passes 0x1E, so the step changes to
6 only by its other test. Kept.

**Status:** latent, harmless.

## D155 — Steps and tail states with no exit of their own (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC5, SC11, SC13, AR3F, AR4B, AR4D and AR4E.

**Established:**

- Two chapter-11 steps run on into the next step's test, a missing `break`
  by the shape: `Scena11_Scene1` `0x55C370` step 11 with `Field_Kind2Hold`
  set runs step 12's counter test, `Scena11_Scene8` `0x55D700` step 7 with its
  effect live runs step 8's (both only test and set the next step).
  `Scena11_Scene8` step 14 goes on with a full effect pool; `Scena11_Scene4`
  `0x55C980` step 0 takes a new effect every frame its roll is one the switch
  does not act on (every shipped roll is one it acts on).
- `Scena13_Run7` `0x563A20` step 0x16 waits on a timer that never counted
  down in its band.
- `Scena05_Run16` `0x547920` step 0x44 sets flag 0x25 every frame and keeps
  the step; no code in the band stores step 0x44 (it is an entry the
  movement script's step and run ops write, "not a defect on this reading,
  only unexplained").
- `Area145_Tail20` `0x4216D0` state 0xA, with no effect slot free, runs
  story flag `0x2C`, sound `0x200` and `Kind2_Place(0)` again every frame
  until one frees.
- `Area170_Tail37` `0x426C90`: state 51 has no case and state 53 no exit of
  its own (it swaps the held input's top nibble through `Area170_InputSwap`
  `0x63D63C` every frame `Field_Request` is 0); both wait on area 170's
  choice 3 `Area170_ChoiceState52Or55` `0x426BF0`, "reads as intended".
- `Area187_TailMessage2` `0x42A290`'s state 2 is set by nothing in its band;
  `Area186_TailShift` `0x42A000`'s states 2..9 do nothing for ever
  (`Area186_Start` `0x42A1C0` arms only 0 and 0xA); `Area191_Tail53`
  `0x42B7F0`'s state 1 waits on nothing but a re-arming (choice 5 at 0x1E,
  the step hook at 0).

Reproduced in each; what writes the states from outside (a message script's
op, the movement script) is not read.

**Status:** latent.

## D156 — `Area189_DrainHp` revives a record at 0 HP (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group AR4E ([`area_w4e.md`](area_w4e.md) §6).

**Established:** `Area189_DrainHp` `0x42B510`'s floor (an HP at most d
becomes 1) turns an HP of 0 into 1 for each of the seven records it walks.
Faithful; what the game does with a fallen member drained back to 1 HP is
not measured.

**Status:** latent; the one entry here a player could notice in area 189.

## D157 — The item `0x5B` top-up wraps past sixteen (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups AR3G ([`area_w3g.md`](area_w3g.md) §6) and AR4F
([`area_w4f.md`](area_w4f.md) §1).

**Established:** `Area150_ChoiceFill5B` `0x4236D0` and its twin
`Area193_ChoiceFill5B` `0x42C3E0` top the party up to sixteen of item `0x5B`
by `Inventory_Add(0, 0x5B, 0x10 - Inventory_Count(0, 0x5B, 0))` with the
difference taken as a byte: holding more than sixteen makes the count wrap
to 240..255 and hands that to `Inventory_Add`. AR4F's doc lists the body's
shape but not the wrap. Reproduced.

**Status:** latent; whether more than sixteen can be held is not read.

## D158 — Cell hooks compare a nibble with the whole facing byte (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group AR4A ([`area_w4a.md`](area_w4a.md) §7; the same shape in
AR3D's and AR3G's function rows).

**Established:** `Area167_CellHook` `0x4264A0` compares a nibble with the
leader's whole facing byte, so a facing with any of bits 4..7 set never
matches; `Area148_SwitchHook` `0x4227E0` ([`area_w3g.md`](area_w3g.md) §2)
and `Area135_CellHook` `0x41E580` ([`area_w3d.md`](area_w3d.md) §1) do the
same, unlisted by their docs. Reproduced (the fuzz plants such facings);
whether the facing byte ever carries high bits on those cells is not read.

**Status:** latent.

## D159 — `Area136_SpawnLeaderKind1` reads party slot 1 for the leader (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group AR3E ([`area_w3e.md`](area_w3e.md) §6).

**Established:** `Area136_SpawnLeaderKind1` `0x41F2D0` reads party slot 1's
character for the leader's record, where its siblings read the slot of the
record they use; the leader's own slot is `0x904062`. Kept as read; whether
Capcom meant slot 0 is not known (the PSX twin `0x801F30C8` was not
compared).

**Status:** latent, intent open.

## D160 — Draw slips in the area blocks (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups AR2E ([`area_w2e.md`](area_w2e.md) §7) and AR3G
([`area_w3g.md`](area_w3g.md) §6).

**Established:** `Area148_DrawBeamBand` `0x422D90` builds its second quad at
the first's pointer + `0x44`, not at `Gfx_PacketNext`, assuming
`MapView_LinkPrimAt` moved the pointer by exactly the size (ours writes where
the original writes; the fuzz's `Gfx_CommitPrim` stand-in moves the cursor by
another amount one call in five to tell the two apart). `Area104_BuildMinimap`
`0x416020`'s image is 69 rows where its header says `0x44` (68).

**Status:** latent.

## D161 — Small slips in the chapter banks and area blocks, each read once (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-27..28, groups SC6, SX2, AR1A, AR1C, AR2A, AR2F, AR3A and AR4B
(their docs' defects sections). Kept in each; none is known to show.

**Established:**

- `Scena06_Run16` `0x54DED0` gives record 7's equipment byte + 1: a byte of
  `0xFF` gives item 0, which `Inventory_Add` refuses, and the run ends as if
  the inventory were full.
- `KeyItem_Remove(0)` `0x591920` clears the first empty slot and answers 1.
- `Area38_SpawnEffectMember1` / `2` `0x405410` / `0x4054D0` leave
  `Sprite_Current` on party record 1 / 2, so the movement-script op that ran
  the handler then reads the party record's byte `+8` (op `DE`) where it
  would read its own object's (the spawns of areas 49, 57, 68 and 77 do the
  same, unlisted as a defect by their docs).
- `Area49_EffectFrame` `0x409480` and `Area117_MembersFrame` /
  `Area118_MembersFrame` `0x41A0A0` / `0x41A410` leave `Field_State` at the
  last member's record (they put back `Sprite_Current` only).
- `Area52_ShiftBlockXBack` `0x40AA20` ignores `Area52_BlockCell` `0x40A2F0`'s
  answer: with counter 3 at 0 the helper backs the script up by 2 and
  answers 0, and the handler takes the column and moves the object anyway
  (its three sibling callers return).
- `Area79_StateSlide` `0x40F430` writes `Field_State`'s word `+0x12E`,
  whatever object is sliding.
- `Area111_SlideMark` `0x4176B0`'s two map calls mark the start cell `0x10`
  and the stop cell 0, never the cell the object ends in.
- `Area170_Trigger56` `0x4274C0` pushes a fourth word (0) to
  `Inventory_Add`, which takes three (harmless under cdecl).

**Status:** latent, harmless by reading.

## D162 — The boss band's dispatchers, hook tables and stack tables index unchecked (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, every round-eleven group ([`boss_h.md`](boss_h.md) §6,
[`boss_sa.md`](boss_sa.md)..[`boss_sj.md`](boss_sj.md) §6, [`boss_sc.md`](boss_sc.md) §7).
The round-nine and round-ten classes D89 and D133, in the boss band.

**Established:** every kind's per-frame dispatcher (`BossKind_Table`
`0x64B088`'s entries) jumps through its `.data` state table by the enemy's
state byte `+1` (twelve entries; the second-level tables six, four or three)
and every hook (`+0xF4`) through a three-entry table by the low byte of the
engine's word, both unchecked. The tables lie end to end, so a stray state
runs the next table's handler rather than faulting at once. The same shape
in the effect tasks' stack tables (`BossAnglerFx_Run` by `+2`, past 2 the
caller's return address; the Arwan task's by `+1`, past 3 its caller's
frame; `BossMyriaFx_Follow` indexing `BossMyriaFx_Drift` by `+1`; Myria's
effect indexing a ten-byte pose table by the 16-bit word `0x904B7E`, of
which 10 and 11 read the frame's unset bytes and 12 and up the saved
registers - D170) and in `BossMap_SetCorners` (a cell past 1, a value past
2). Nothing in the kinds' own code stores a state byte out of range; the
generic `EnemyOp_*` entries they share set them. Ours aborts with a
message naming the function and the index (the owner's rule for an
unchecked index into a table ours owns); the fuzz's seeds stop at the
table's end.

**Status:** latent; ours aborts where the original would jump wild.

## D163 — `BattleTask_Create`'s "no slot" answer untested in seven boss effects (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups BSA, BSF, BSH, BSI and BSJ (their docs' §6).

**Established:** `BossWeretigr_State4Fx` (BSA), `BossGazer_State4Wait`,
`BossGazerFx_BounceStart` and `BossMyria_SpawnFx` (BSF),
`BossAngler_HookSpawnFx` (BSH), `BossArwan_State4Fx` (BSI) and
`BossDLord_HookFx` (BSJ) write the slot `BattleTask_Create` answers
without a test. With all 48 slots taken it answers 0xFF, and the original
writes `0x84` bytes (or the owner byte, or an enemy object's `0x80` bytes and
six more) at `0x93A000 + 0xFF * 0x84` = `0x9423FC`, past the pool (which ends
at `0x93B8C0`) and past the image. Myria's entrance creates three at once
and every action pick one more; the Arwan, with its task never running,
waits in state 4 step 1 for good. Ours aborts with a `Fatal` at the slot.
Whether a fight can fill the pool is not measured - the BSE and BSG
harness stand-in answering 0xFF (the round doc's section 5) is what the
controls refused on.

**Status:** latent; ours aborts where the original would write past the pool.

## D164 — Exit hooks and spawn writers write through `BossActor_Find`'s null answer (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups BH, BSA, BSB, BSC, BSD, BSH and BSJ (their docs' §6;
[`boss_sc.md`](boss_sc.md) §7).

**Established:** `BossActor_Find` `0x494920` answers null when no field actor
carries the tag. BH's spawn writers `BossActor_ClearBit40`,
`BossActor_CopyFrom` (for `what` 0 and 1) and `BossActor_Clear` use the
answer without a test (22, 9 and 7 units call them, with constant tags where
read: the exit hooks' 0, Boss01's 6 and 7), and the set-ups' own exit hooks
do the same in their own code: `Boss08_Exit`..`Boss10_Exit` (BSB, stores at
`+0x48`, `+0x2A`, `+0x58`, `+0x5A`, `+0x34..+0x3C` - the original faults first
inside `Sprite_SetAnimationBank`, which reads `Sprite_Current`),
`Boss11_Exit` and `Boss16_Exit` (BSC, each preceded by the same tag's
`BossActor_ClearBit40`, so the second write is unreachable), `Boss21_Exit`
(BSD, `+0x2A`), `Boss22_Exit` (BSE, `+0x2A` and `+0x48`, preceded by the
same tag's `BossActor_ClearBit40`, so unreachable like BSC's), `Boss34_Exit`
(BSH; its abort follows two calls that already read `Sprite_Current`, so it is
the preceding `BossActor_ClearBit40` that stops a null first).
`Boss52_Exit` (BSJ) and BSG's `ActorZero` (the exit hooks of set-ups 31 and
32) store the null into `Field_ActiveMember` and `Sprite_Current` and call
`Sprite_SetAnimation` on it, dereferencing nothing themselves, so ours does
the same there. (BSE's and BSG's added, and the address corrected from
`0x4948E0` - `EnemyData_FindByTag` - by the capture review of 2026-09-29.) BSA's end walk
trusts `BossActor_Index(0)` likewise: 0xFF with no actor tagged 0 hands
`MoveCmd_OpE9` `0x7DEF00 + 0xFF * 0xA4` = `0x80925C`, far past the 30 field
objects (kept: the pointer is only passed on). Whether a scene can lack its
actor is each area's data, not read. The one caller outside the band,
`0x494570`, tests the answer. Ours aborts with a message naming the hook
and the tag; the stand-in never answers null, so the path is not fuzzed.

**Status:** latent; ours aborts where the original would write near address 0.

## D165 — Boss code reads by an index or actor it does not check (latent, kept)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups BH, BSA, BSF, BSG, BSH, BSI and BSJ (their docs' §6).
Kept in each: none indexes a table ours owns or writes past one, so a
`Fatal` would end a game the original survives.

**Established:**

- `BossTorast_Colours` is read by the kind unchecked: a kind outside 8..11
  gets neighbouring `.data` (only kinds 8..11 reach these steps, through
  their own tables).
- F6 (Weretigr's turn, BSG) reads the byte pair at `0x64E4F4 + 2 *
  (0x904AAC & 0xFF)` - four pairs, a facing past 3 reads what follows (the
  table is engine code's, shared) - and indexes the enemy objects by
  `target - 3` and `actor - 3`: with a member (0..2) acting, the finish's
  `&= 0xBF` would land in the task slots before the enemy objects. In the
  fight the actor is Weretigr itself.
- The task spawns copy by an actor they do not check: `BossWeretigr_State4Fx`
  (BSA) copies `(0x904B34 - 3) * 0x128` from the enemies' base, not
  `Sprite_Current`'s object; `BossAngler_HookSpawnFx` (BSH) and
  `BossDLord_HookFx` (BSJ) the same by `actor - 3` - with a member acting
  the source is 0x128..0x378 bytes below the enemies, inside the task
  slots, and the task starts as a copy of task memory. `BossMyria_SpawnFx`
  (BSF) copies enemy 0's object and names enemy 0 the owner whichever enemy
  runs it; `BossArwanFx_Start` (BSI) takes its count from enemy 0's `+0xF0`,
  not its owner's. In the game each is presumably enemy 0 or acting on its
  own turn (not measured).
- The ability records are indexed by the whole word `0x904B80`
  (`BossMyria_ActPick`, `_State7Start`, `_State8TickUnless`, `_State8Check`,
  BSF): an id past the table reads the `.data` after it.
- `Boss27_Event` (BSF) reads the actor at `0x904ACB + 0x904AE2` unchecked
  (the turn order one before its cursor); only an actor 0..2 is acted on.
- The enemy-data reads trust `+0xF0` (`BossArwanFx_Start`, BSI;
  `BossDLordFx_Start`, BSJ, as `BattleActor_FxSize` does): an index past the
  area's eight records reads up to `0x8CE1C6`, still inside `.data`.
  `Battle_CopyEnemyData` sets it to the record's slot, so it is 0..7 in play.
- `Boss34_Event` (BSH) indexes the party by the byte `0x675F08`, written only
  by `Boss34_Setup` with 0..2 and 0 at start: when no member matches
  `0x669730` the fight marks member 0.
- `Boss01_Event` (BSA) and BSB's event hooks read the command `[0x904B40]`
  at phase 1 without a test; the action phase sets the pointer before it
  calls the hook (whether always was not read).
- `Boss16_Event` and `Boss16_End` (BSC) name enemy 2 by address (`0x93BBB0`)
  and pose it as Nina's without checking the kind at that slot; row 7 of
  `BOSS013`'s area decides.

**Status:** latent, kept.

## D166 — Set-up 25 saves member 0's HP and AP as bytes and restores them as words (latent; a fix candidate)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSE ([`boss_se.md`](boss_se.md) §6).

**Established:** `Boss25_Setup` stores the low byte of `+0x98` and of `+0x9A`
(`mov al, byte ptr [0x802DD8]` into `0x675F05`) and `Boss25_End` writes them
back zero-extended (`movzx ax, byte ptr [0x675F05]`): a member-0 HP or AP
above 255 comes back as its value mod 256 after the fight. Whether the
fight's member 0 can have that much, and what the PSX did, were not read.
Kept as read; **the owner decides** - saving the words is a two-instruction
change and would be a DIVERGENCE entry.

**Status:** latent; a candidate for a fix.

## D167 — Set-ups 8..10's exit hooks pose every actor from enemy 0's words (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSB ([`boss_sb.md`](boss_sb.md) §6).

**Established:** `Boss08_Exit`..`Boss10_Exit` copy enemy 0's pose words
(`0x93B9B8` / `0x93B9BA`) onto actors 0, 1 and 2 alike, where set-ups 4..6
pose each actor from its own enemy. Kept as read. Whether that is intended
(enemy 0 may be each fight's only enemy) is the formation data's, not read -
**a question for the owner**, not a defect until answered.

**Status:** latent, intent open.

## D168 — Kind 42 (Torch) points its animation table at never-written `.data` (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSH ([`boss_sh.md`](boss_sh.md) §6).

**Established:** the Torch's entry points the enemy's `+0xFC` at `0x675F0C`,
twelve bytes of `.data` that nothing in the exe writes (a scan of the exe for
the address finds only this store) and that are zero in the file - so every
animation byte the generic states read for the Torch is 0. The other six
kinds of the group point at tables in the kinds' `.data` block beside their
state tables. A PSX overlay's data section the port's link did not carry over
is the obvious reading, not proven: the sibling's `BOSS035` image was not
compared. Ours stores the same address.

**Status:** latent; a comparison with the PSX overlay is owed.

## D169 — `BossTorast_DrawRing` draws its ring twice (latent, kept)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BH ([`boss_h.md`](boss_h.md) §6).

**Established:** 64 points at `i * 0x80` are two laps of the 4096-step circle,
and the rim's second corner is point `(i + 1) & 0x1F`, so triangles 32..63 are
triangles 0..31 again, each semi-transparent: the ring is blended twice. Kept
(it is the look the original gives; a lap of 32 would halve the cost and
change the blend).

**Status:** latent, kept.

## D170 — Myria's effect indexes a ten-byte pose table by a 16-bit word (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSJ ([`boss_sj.md`](boss_sj.md) §6).

**Established:** every state's pose is `table[0x904B7E]` with the table ten
bytes of a twelve-byte frame: 10 and 11 read the frame's two unset bytes, 12
and up the saved registers and the caller's stack. Kind 62 (BSF's) writes the
word from a byte argument (`0x440630`) or as 8 or 0; whether it stays below
10 in the fight is kind 62's callers', not read. Ours aborts at 10 (the D162
rule).

**Status:** latent; ours aborts.

## D171 — Kind 30's entry copies enemy data record 0xFF when no record carries tag 0x59 (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSE ([`boss_se.md`](boss_se.md) §6).

**Established:** in fight 0x1A `BossBeyd_Enter` passes
`EnemyData_FindByTag(0x59)` to `Battle_CopyEnemyData` untested; "none" is
0xFF, and the copy reads `0x8C55C8 + 0xFF * 0x8C`, far past the eight records.
Whether the area's data always carries the tag is the data's, not read.

**Status:** latent.

## D172 — Set-up 25's exit copies up to 255 bytes into the party list (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSE ([`boss_se.md`](boss_se.md) §6).

**Established:** the exit copies up to 255 bytes from `0x939A10` into the
party list `0x904065` by the byte `0x939A02`; its writer was not read, so
whether it stays within the list is not known.

**Status:** latent.

## D173 — Small slips in the boss band, each read once (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, groups BSA, BSB, BSC, BSD, BSF, BSG and BSI (their docs' §6;
[`boss_sc.md`](boss_sc.md) §7). Kept in each; none is known to show.

**Established:**

- Kind 3's shift `1 << (+5 & 0x1F)` into a byte (BSB): a slot `+5` of 8 or
  more sets no bit in `0x904AAD`; enemy slots are 0..7.
- `BossAmalgam_Enter` (BSC) drops `0x455290`'s "no slot free" answer (al
  0xFF): with all eight field slots in use the script is simply not started,
  and `BossAmalgam_DeathEnd`'s release finds nothing to free.
- `BossAmalgam_DeathMelt` (BSC) ends on `+0x20 == 0x56` exactly, stepping by 2
  from 0: an odd row (never stored by the code) would melt until the dword
  wraps.
- `BossNina_HookAct` (BSC) decrements `0x904AE2` without a floor, and
  `BossSunder_HookAct`'s 0x2A line on bit 5 sets no bit, so it opens again at
  every action pick while bit 5 stays set. The hit hooks of Balio and Sunder
  floor HP at 1: neither fight ends through a hit; the end hook's win bit
  was not traced to its setter.
- Kinds 21..23 and 26 (BSD) restore HP from `0x939A14..0x939A1B` when their
  flag is set, and only set-ups 18..20's end hooks write those words: a flag
  set without that hook having run in the same process would restore zeros.
  Where the flags are set, and whether the words survive a load, was not read.
- `Boss26Fx_DrawCount` (BSF) prints `0x15 - turn` unclamped; `Boss26_Event`
  sets `0x904AE8` bit 2 at turn 0x15, which presumably ends the fight first.
- Set-up 32's end hook (BSG) and `Boss26_End` (BSE) wrap the pose as a byte:
  a member `+8` above 0xE3 poses from the start of the set.
- `Boss38_Setup` (BSI) leaves `Sprite_Current` at enemy 7's object and relies
  on `MagicFx_CenterOnSide`, which divides by the count of enemies not out:
  with all eight out at set-up the original divides by zero (ours aborts
  there, round nine's `magic_lib.cpp`).
- `BossDodai_Dispatch` (BSD) rewrites the enemy's `+0xFC` every frame from a
  chapter flag before the step reads it, and `BossMyria_State4Wait` (BSF)
  never moves Myria on itself (the slot-5 tasks do) - not defects, noted so
  the next reader does not look for them.

**Status:** latent, harmless by reading.

## D174 — `ConfigText_Inject` laid the Chinese Config screen out for Latin text under `BOF3X_LANG=original` (fixed)

**Seen:** `BOF3X_SHADOW='*'` headless in the main checkout, 2026-09-29:
`FATAL: Config_DrawRowLabel: the call at +0x9F reaches ..., not 0x00516B30:
the site is re-aimed already, cannot clone` - group FC1's fuzz, with the
launcher handing the ini's `language=en` to the game, or with
`BOF3X_LANG=original` set by hand.

**Established:** `ConfigText_Inject` (`src/game/config_text.cpp`) applied
DIV-0015's anchor and DIV-0017's widths whenever `BOF3X_LANG` was set and not
full-width - `original` included, which `DatLoad_Inject` reads as no
overlay. Noted in [`glyph-draw.md`](glyph-draw.md) §8 on 2026-09-22 and left
("not this group's"); only the controller panel's patch made the exception.
So an all-original reference run (`attract_run.py` pins `original`) had the
Latin layout over Chinese text on the Config screen, and FC1's clone of
`0x461800` refused the re-aimed site.

**Fixed 2026-09-29:** the function tests `Lang_Latin()` as every other
layout patch does; the inner `original` test went with it. The fuzz passes
with `BOF3X_LANG=original`. With `language=en` in the ini the FC1 clone still
refuses (rightly: the overlay is on) - the headless run wants the language
off, `BOF3X_LANG=original` in the environment or a scratch ini.

**Status:** fixed.

## D175 — Round twelve's dispatchers index unchecked: the battle engine's step and state tables, the effect kinds', the field core's, the panels' (latent; ours aborts past most, three groups read on)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, by every round-twelve group ([`battle_e1.md`](battle_e1.md) §7,
[`battle_e2.md`](battle_e2.md) §7, [`battle_e3.md`](battle_e3.md) §10,
[`battle_e4.md`](battle_e4.md) §7, [`battle_e5.md`](battle_e5.md) §7,
[`battle_e6.md`](battle_e6.md) §7, [`battle_e7.md`](battle_e7.md) §7,
[`field_c1.md`](field_c1.md) §10, [`field_c2.md`](field_c2.md) §10,
[`field_c3.md`](field_c3.md) §10, [`field_e1.md`](field_e1.md) §7,
[`field_e2.md`](field_e2.md) §6, [`field_o.md`](field_o.md) §10,
[`field_s.md`](field_s.md) §10). D59's, D133's and D162's class in the
resident code no route had entered; one entry for the class, the groups'
docs carry the tables.

**Established:** `jmp [table + 4 * byte]` with no bound, the byte a step
(`0x904AA1..0x904AA4`, a record's `+1`..`+4`) or a window record's `+3`.
The tables lie end to end, so a stray byte runs the next table's entry, or
data past the last: BE1's seven (nested: `0x64ADE0` runs into `0x64ADEC`,
`0x64AEB4` into `BattleAction_AbilitySteps`, `0x64AFD8` into a zero dword);
BE2's four `EnemyOp` tables; BE3's eleven; BE4's four; BE5's six Dragon
dispatchers and `0x44FF00` (past `DragonCmd_StoreSteps` the data dword
`0x00070700`); BE6's three task dispatchers (7 / 2 / 2 entries); BE7's four
stack tables (`0x598DC0`, `0x598F60`, `0x599360`, `0x599470`: past the three
or five entries the caller's frame); FC1's five and the catalogue's five;
FC2's eight (past `EffectKind34_V0States` the area bytes `0x65401C`); FC3's
ten and `Field_ObjectHandlers` by op E7's operand
(`FieldCore_VerticalSteps`' third word is `FieldCore_UpSteps`' first); FE1's
`Field_FormActions` by `0x90412C & 0x7F` (19 entries, then
`Field_EncounterAreas`; the same index reaches `Field_ActionBySet`, D59)
and the jump and content states' bytes (on into the next tables' code to
`0x660A24`, then data); FE2's `Mode11_ObjectFrame` past state 3 and
`Field_ObjectTriggerByKind` past kind 65 (into `Area_CellHook`'s pairs);
FO's op 87 past 1, op E9 past 3 and **op 88's `+4`: 2 or 3 run op 87's
states (visible - the colour ramps the other way), 4 and up data**; FS's
four step dispatchers and `SharedList_Sort` by its argument's byte. In
every group the step bytes' own writers keep them inside their table; what
else writes some of them (a kind-0x34 record's `+1` from a spawner outside
the band, the gene command's input code for BE7's `+3`) was not traced.

**What ours does** differs by group, as D133 found for round ten: BE1..BE7,
FC1, FC2, FC3 and FO abort with a message past each table (FO on reaching
data); FE1, FE2 (`Field_ObjectTriggerByKind`) and FS read the word in place
and abort only on one that is not code (`CodeAt`), so a stray byte that
lands on the next table's code runs it as the original does. BE1 notes
that a `kDispatch` through a `call` is driven as a `kStep` with
`state_cell` ([`battle_e1.md`](battle_e1.md) §13).

**Status:** latent; ours aborts where the original would jump wild, except
in FE1, FE2 and FS, which read on past a table into code the original also
runs.

## D176 — `BattleTask_Create`'s "no slot" answer untested in twelve more places of the battle engine (latent; ours aborts in eleven, writes where the original would in one)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, groups BE2, BE3, BE6 and BE7 (their docs' §7, §10, §7, §7).
D54's and D163's class.

**Established:** the slot byte is used as an index at once: BE2's
`BattleFx_RestoreParty` (the owner at slot 0xFF's `+0x80`, `0x9423FC`) and
`EnemyOp_KnockReturn` (`+0x80`, `+7`, `+0x27` there); BE3's
`BattleObj_HitStepBack`, `_Revive`, `_ReviveByEquip`, `_FallTaskStart`,
`_State10Task` and `_SpecialStart`; BE7's three open states (`0x598FA0`,
`0x599390`, `0x5994A0`: `+9` / `+0xA` of slot 255, 0x83FF bytes past the
pool); and BE6's `Battle_SetApPopup` `0x453EB0` (D54's twin for the AP
pop-up - the entry there names the damage and hit pop-ups). With all 48
slots taken the original writes past `0x93B8C0` and past the image. Ours
aborts with a `Fatal` at the slot in the first eleven; **`Battle_SetApPopup`
is kept as read and writes where the original would** (`battle_e6.cpp`,
its comment), the one of D54's family taken so far that does. Whether a
battle can fill the pool is not measured (D163).

**Status:** latent; ours aborts where the original would write past the pool
in eleven of the twelve, `Battle_SetApPopup` faithful.

## D177 — Divisions by a value play may leave at 0, in nine functions of the battle engine and the field (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, groups BE2, BE4, BE6, FC1, FC3, FE2 and FO (their docs'
defects sections).

**Established:** an `idiv` or `div` by a count, a weight sum or a record's
byte with no test: `Battle_RandomEnemy` by the count of enemies in (every
enemy out but the excluded one) and `Battle_RandomMember` by the byte sum
of the weights (every member out, or weights summing to 0x100), the
restore's gauges by a member's max HP / AP (BE2); `Battle_PartyDefenceMean`
by the party count, `Escape_Roll` by `0x904AB3` (enemies left) and
`0x904AB1` (BE4); `Battle_MemberRollByAction` by the odds (a zero in
`0x64F0FC` / `0x64F104` at the action row's nibble - which nibbles the rows
use was not measured) and `MapCell_DrawGroundSprite` by the record's byte
8 (BE6); `CameraZoom_Start` by `+9`, as `CameraTurn_Start` just before it
(a zoom spawned with 0 frames faults in `CameraTurn_Start` first,
[`frame-callees.md`](frame-callees.md) §3) (FC1); `FieldCore_HopLaunch`'s
`0x20 / Field_MoveSpeeds[f + 0x128]`, whose entry 0 is 0 - `FieldCore_HopBegin`
sets the pace to 3 first, so only a hop entered at its second step from
outside could (FC3); `MapCell_DrawFrames` by a period of 0 (FE2); op E9's
`(a << 15) / (frames * steps)` with the frames 0 (a speed index whose
`Field_MoveSpeeds` byte is above 16) and the Arc and Kind2 steps' `16 /
speed` on a speed of 0 (indexes 0, 6, 7; `Start` answers 0 at once for a
speed of 0, so the second needs the state byte set some other way) (FO).
Met on the way and nobody's: `0x4941E0` divides by the camera-space depth -
a spark at depth 0 faults (FC2). Whether play reaches any zero was not
measured in any group.

**Status:** latent; ours aborts where the original would fault.

## D178 — Reads and writes by an actor, member, enemy, slot or choice byte with no bound, in the battle engine and the field (latent; ours aborts on the writes, reads the same)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, by every round-twelve group (their docs' defects sections).
D136's class in the resident code; one entry, where a read stays inside
`.data` ours reads the same and where a write would leave what it indexes
ours aborts (each group's doc says which).

**Established:**

- *The party count and the entry order* (BE1, BE3, BE4, FE1): the party
  loops index `ObjTrio` by `0x904AB0` and the entry order unchecked - a
  count above 3 writes `+1` / reads `+0x134` past the three records (into
  `WindowRecords` at `0x803160`), an enemy id in the entry order (3..10) is
  read as a member (`Cmd_AutoBattle`); `Party_PlaceAtSlots`,
  `Party_PlacesByList`, `Field_LeaderPlaceOffset` and the pending jump index
  `ObjTrio` by the count and by `0x904EF2` likewise; `Escape_Begin` gives
  back the items of all three records whose command is 5 whatever the
  count; and with no member carrying the leader's id `Party_PlaceAtSlots`
  reads the record after the last (`0x803164..`) for the camera's x, z,
  `Party_PlacesByList` the word past the saved ones.
- *Actor and enemy indices* (BE1, BE2, BE3, BE5): `BattleAction_AbilityNotice`
  reads the acting enemy's `+0x106` at `0x93BA66 + (0x904B34 - 3) * 0x128`
  (a party actor reads the task slots, an actor past 0x30 reads past the
  image and faults - it did, both sides, before the seed kept the actor to
  0..10), then `Ability_Records` by the id; the watch, `BattleFx_PlaceOverOwner`
  and `BattleFx_NextStatusIcon` index the enemies by the owner's `+5 - 3`
  (past 25 past `.data`), the restore steps the party, the task slots and
  the windows by the owner's and `Sprite_Current`'s `+5`; `BattleEnemy_PickAction`
  / `_PickTarget` the enemies by the argument's low byte (past 7;
  `Battle_BeginAction` passes 0..7 from the turn queue); the special
  attack's actor (BE3); `EnemyAI_OtherRowsDone`'s enemy and `Effect_Drain*`'s
  `target - 3` / `actor - 3` (past 10 the objects run off the image).
- *Roster and character bytes* (BE1, BE3, BE5): `Char_LevelUpGain`'s roster
  byte (any byte reads a record past the eight and 99 rows past the table);
  `BattleObj_Fall` zeroes a byte of `CharacterRecords[+0x148]` with the
  character byte unchecked (the revive-by-equipment paths);
  `EnemyAI_CondElement` indexes the ability rows by the whole word
  `0x904B80` and the weapon table by the member's `+0x92`.
- *Slots and categories* (BE4, BE7): `BattleEquip_RemoveSlot` indexes six
  stack pointers by the slot cursor and `BattleEquip_Preview` writes
  `0x675F18 + cursor` (every writer of the cursor keeps it 0..5);
  `Battle_ReturnItem` indexes the list pointers by the category (category
  4's count pointer is 0, a write near address 0); `GeneWin_DrawList` /
  `_DrawList2` by the record's signed `+0x12` (reads), the scroll marks a
  fixed six / twelve rows whatever `+0x14` says.
- *The field's objects and tables* (FC1, FC2, FC3, FE2, FO): the two `Hold`
  states write `Sprite_Objects[+3]`, `[+4]` and `[+0xB]` from the spawner;
  `EffectKind17_Bump`, `EffectKind1B_Hit` and `EffectKind3A_Hit` widen
  `Sprite_ObjectAt`'s answer **signed** (`movsx`) where
  `EffectKind30_WayBlocked` widens it unsigned - an answer of `0x80..0xFE`
  would set a bit below `Sprite_Objects` (the callee answers 0..`0x21` or
  0xFF); kind 0x41 draws `ObjTrio[+0xB]` unchecked (three members); the
  leader's facing indexes the 8-row step tables (`EffectKind30_Push`,
  `EffectKind3A_Start`); `FieldCore_Attached` indexes `Sprite_ObjectsExtra`
  by the dword `+0x18` (four records; a read); `AreaMap_ClearCell` writes
  `AreaMap_Bytes[x + z * width]` for any x, z and reads past `MapView_Cells`
  for a row or column past their wraps; FO's ability title
  `0x663984[+0xB]`, the wheel's 28-byte tables by the kind,
  `CharacterRecords[operand]` in condition 15, the item panel's rows past
  the kept list's 0x80, `Field_MoveSpeeds[+0x84]` in F9 and E9, and
  `Area_Descriptors[Game_AreaNumber]` in op Ax and `EffectKind30_Start`
  (D136's `Area_Descriptors` reads).

**Status:** latent; the writes abort in ours where they would leave the
records (each group's doc names them), the reads are the original's.

## D179 — `EnemyOp_SlideStart` reads the velocity's second component from the place's x and steps the wrong state byte (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE2 ([`battle_e2.md`](battle_e2.md) §7; control C40 there
is the "fix", refused).

**Established:** the function takes the velocity's second component off
`+0x34` (the place's x), not `+0x38`, and steps `+2`, not `+3`. Reached
through `EnemyOp_EnterSubs[1]` (`+2` = 1), it leaves `+2` = 2, and the next
frame's `EnemyOp_EnterDispatch` reads `EnemyOp_EnterSubs[2]` - past the
table's two entries, the next table's first cell (`EnemyOp_ScaleInStart`;
D175's shape, here reached by the original's own code). Whether any enemy
enters with `+2` = 1 was not looked at; the slide-in path is what no
recorded route shows ([`battle_e2.md`](battle_e2.md) §8). Kept.

**Status:** latent, faithful; whether the slide-in is ever taken is the
owner's to see.

## D180 — `BattleFx_NextStatusIcon` never returns when the owner's status has none of the icons and `+0xB` is 16 or more (latent, faithful; its "none" answer is dead)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE2 ([`battle_e2.md`](battle_e2.md) §7).

**Established:** the search runs modulo 16 and stops only on `+0xB` itself;
with none of the 0x58 status bits set and `+0xB` at 16 or more it spins.
`BattleFx_WatchIcon` stores the answer (0..15) in `+0xB`, so only a first
call with `+0xB` at 16 or more can; who sets `+0xB` before (the watch's
states 1 and 2, `0x433550` / `0x433640`) was not read. Its answer 0xFF,
which `BattleFx_WatchIcon` tests to free the slot, never comes: that branch
is dead in the original (the fuzz reaches it through the recorder). Kept.

**Status:** latent, faithful.

## D181 — `EnemyOp_CastCue` follows the current enemy's `+0xF8` as a pointer in any event battle (latent; a crash candidate)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE3 ([`battle_e3.md`](battle_e3.md) §10).

**Established:** with `0x904AAA` (the fight id) set, `EnemyOp_CastCue` reads
`0x939AD8`'s `+0xF8` as a pointer. `EnemyOp_Begin` stores `+0xFC` but not
`+0xF8`, so a non-boss enemy casting in an event battle follows whatever its
`+0xF8` holds - the Paralyzer row's pattern (round nine's doc) and
`EnemyOp_ReceiveAction`'s. Only a fight with an ordinary caster beside the
boss reaches it; which fights have one was not read. Kept.

**Status:** latent; a crash candidate where an event battle has an ordinary
caster.

## D182 — `Escape_Roll` sums the `+0xB8` of objects 3..10, three of them past the enemies (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE4 ([`battle_e4.md`](battle_e4.md) §7).

**Established:** it asks `Battle_ActorIsOut` of actors 3..10 but reads the
word at `0x93BD90 + k * 0x128` - object `actor`, not `actor - 3`: objects
3..10, of which 8..10 lie past the eight enemy objects (`0x93C2A0..`, the
message queue and the memory after it). The enemies' mean the roll
compares with is that of objects 3..7 and three words that are no enemy's,
over the enemies left. Measured by reading only; what the words hold in a
battle is not ([`battle_e4.md`](battle_e4.md) §8). Kept; its two divisors
are D177's.

**Status:** latent, faithful; the escape odds are off by whatever the three
words hold.

## D183 — `EnemyAI_DedupMessages` keeps its messages in an eight-dword stack buffer with no bound (latent; ours aborts at a ninth)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE5 ([`battle_e5.md`](battle_e5.md) §7, §11).

**Established:** a ninth distinct message would be written onto the
function's return address. `EnemyAI_ApplyAction`, the list's one writer in
the band, stops at eight, so the list never holds nine; the fuzz keeps the
count at most 8 as its writer does. Ours aborts at a ninth instead. (Its
`eax` is whatever its loop left; `EnemyAI_TurnCheck` hands it on and its
one caller `0x436908` loads `eax` again at once, so ours is `void`.)

**Status:** latent; ours aborts where the original would overwrite its
return address. Unreachable by the one writer read.

## D184 — `DragonCmd_Slots2Close` waits on window 18 and clears 17, 16 and 19 - window 18 left up (latent; the owner's question)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE5 ([`battle_e5.md`](battle_e5.md) §7).

**Established:** part 5's close waits on window 18's in-use bit and clears
windows 17, 16 and 19; its twin `DragonCmd_SlotsClose` waits on 18 and
clears 17, 18, 16, 19. Kept as read. Whether window 18 can still be up when
part 5 closes was not measured; if it can, the second close leaves it.

**Status:** latent; a question for the owner (does a window linger after
the Dragon command's second slot screen?).

## D185 — `Battle_RecalcStats` clears bytes of enemies 0..2 after a member's rebuild (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group BE6 ([`battle_e6.md`](battle_e6.md) §7, L2).

**Established:** after a member's rebuild the function's tail indexes the
**enemy** objects by the member's own number, so enemy 0..2's `+0xC4` /
`+0xBF` are zeroed when that enemy's `+0x114` has bit 11 / 13. Kept.
Whether the cleared bytes matter in play (what `+0xC4` and `+0xBF` of an
enemy hold at that moment) was not read.

**Status:** latent, faithful.

## D186 — `FieldPanel_DrawTotal` walks the stack for a total of 0xFFFF (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FE1 ([`field_e1.md`](field_e1.md) §7, L1).

**Established:** the last threshold is 0xFFFF and the loop continues while
the total is at or above the threshold, so a total of 0xFFFF passes all
thirteen and goes on comparing the frame's bytes as words (none can exceed
it) until it leaves the stack. The total is `0x52CED0`'s sum of 32 kinds'
points (`0x52CE60`: a record's word when the count reaches its byte `+3`,
else a share of it) - whether ordinary play can reach 0xFFFF depends on the
records' words, not read. Ours aborts with a message.

**Status:** latent; ours aborts where the original would read off the stack.

## D187 — The formation screen's reserve holds three: `PartyForm_Setup` writes a fourth record over its count and the flags past their pairs (latent; the owner's question)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FS ([`field_s.md`](field_s.md) §10, L1 and L2).

**Established:** the reserve's count `0x6BC897` is its own fourth byte:
`PartyForm_Setup` writes the fourth record there, then the count over it; a
fifth lands on `0x6BC898`, which the set-up then sets to 0xFF. The reserve's
flag bytes are written two apart from `0x6BC88D`, one per entry: from the
fourth entry on they land on `0x6BC893`, `0x6BC895` (the second entry,
overwritten with 1) and `0x6BC897` (the count: 1), and the clearing loop
meant for the rest never runs (`cmp esi, eax; jge` with `esi` set to the
count). With four or more records outside the party the list and its
count are garbled. Kept, reproduced.

**Status:** latent; whether ordinary play has four or more outside the party
is the owner's to say - if it does, the formation screen shows the wrong
reserve.

## D188 — `ShopResist_Grant` replaces a record's resistance bits instead of adding one (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FS ([`field_s.md`](field_s.md) §10, L4).

**Established:** `+0x1D` = `1 << bit`, not OR-ed. The member picker refuses a
record with any bit set, so the shop itself never replaces one; another
writer of `+0x1D` was not looked for. Kept.

**Status:** latent, faithful; harmless by the shop's own picker.

## D189 — A call-table-B call in chapter 0 jumps through address 0 (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FE2 ([`field_e2.md`](field_e2.md) §6, L1).

**Established:** `Scenario_CallBTables[0]` is 0, so `Scenario_CallB` in
chapter 0 jumps through `[n * 4]` - an access violation. Kept unchecked, as
`Scenario_CallA` is (the naked jump cannot test and still hand the caller's
arguments on); no chapter-0 code calls it.

**Status:** latent, faithful; unreachable by the chapter's own code.

## D190 — `ItemTrade_PickCount` loops for ever with more than 227 of an item held (latent; ours aborts)

**Seen:** the fuzz, 2026-09-29, group FE2 ([`field_e2.md`](field_e2.md) §6,
L3): the first full run hung, and clang had turned our endless loop into a
silent exit 1 - the reason `Inventory_Count`'s stand-in answers a count.
Not seen in play.

**Established:** the quantity loop lowers an s8 quantity until it plus the
item held is 99 or less; with more than 227 held no s8 value does and the
original loops for ever. Unreachable in play: a stack holds 99 at most and
the equipped count is under 9. Ours aborts.

**Status:** latent; ours aborts where the original would hang. Unreachable
by the inventory's own bounds.

## D191 — `Field_ObjectTriggerByKind` at kind 0 calls the dword before its table, area 97's world-map hook (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FE2 ([`field_e2.md`](field_e2.md) §6, L6).

**Established:** the table is read from 4 bytes below `Field_ObjectTriggers`,
so kind 0 calls `WorldMap_FieldHooks[11]` (`0x4139E0`, area 97's hook) with
(object, flags). Whether an object ever carries kind 0 with bit 6 of `+0x89`
was not traced. Kept; past kind 65 is D175's.

**Status:** latent, faithful; reachable only by an object of kind 0 with the
trigger bit.

## D192 — `Field_PassageTrigger` hands `Field_ObjectTrigger` a record of its own frame's bytes (latent; ours zeroes it)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FE1 ([`field_e1.md`](field_e1.md) §7, L3).

**Established:** of the 0xA4 bytes handed on only `+0x86..+0x89` are
written; `Field_ObjectTrigger` tests `+0x89` bit 6 (the word's high byte,
defined) and hands the record to `0x56E020` or the chapter's hook slot 1,
which may read more of it. Ours zeroes the record ([`field_e1.md`](field_e1.md)
§2) - D101's kind, a deliberate zero in place of the stack's leavings. What
the hooks read of it is FE2's and the chapters' to say.

**Status:** latent; ours passes zeros where the original passes its frame.

## D193 — `EffectKind1B_Trail` copies its look from `Effect_Objects[the leader's +0xB]` (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FC1 ([`field_c1.md`](field_c1.md) §10).

**Established:** the leader record's byte `+0xB` is taken as an effect index,
unchecked. What writes it was not traced; if it is not an effect index when
a shot is fired, the trail copies another record's look and point (a read,
no crash while the byte is below 0xFF). Kept.

**Status:** latent, faithful.

## D194 — Three slips in the field core's slope and arrival steps, read as copy errors (latent, faithful; the owner's question)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FC3 ([`field_c3.md`](field_c3.md) §10; control C133 there
proves ours keeps the first).

**Established:** `FieldCore_TileD0Slope`'s raised facing-5 path passes
direction 3 to its third `MapView_SlopeAt` where the other probes of that
path pass 5 - a copy slip by the look of it. The two arrivals test different
facings: `FieldCore_UpArrive` picks animation 0x3C for facing 7,
`FieldCore_DownArrive` for facing 3. `FieldCore_TileD0Exit`'s fall-back
probes the odd directions one plain step away, where its first pass scales
the step by `+0x70 + 1`. All three kept.

**Status:** latent, faithful; whether any is intent is the owner's to say.

## D195 — The throw and push effects read outside their data: `EffectKind3A_Fly` off the map, `EffectKind30_Push` by an unbounded count, `EffectKind30_ShardsInit` 24 faces regardless (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group FC2 ([`field_c2.md`](field_c2.md) §10).

**Established:** `EffectKind3A_Fly` reads the tile layer at the cell with no
bound (x, z s16 against no dimension): a thrown object leaving the map reads
outside the layer, and a non-zero word there keeps it flying until its 14
frames run out. `EffectKind30_Push` copies `40 x` a signed count byte from
`*(+0x54)` into `0x8C5D80` with no bound (up to 5,080 bytes; the buffer is
used with `0x1800` elsewhere, so it fits), and `EffectKind30_ShardsInit`
then reads 24 faces of it regardless of the count: a model of fewer faces
leaves the rest from whatever the buffer held. Reads, reproduced.

**Status:** latent, faithful.

## D196 — Small slips in round twelve's code, each read once (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, by the round-twelve groups (their docs' defects sections). Kept
in each; none is known to show.

**Established:**

- `BattleExtra_TallyCount` indexes its three tables by a signed byte
  (`movsx`): below 0 it reads `0x64ADD0 - n` and writes `0x675E88 - 4n`;
  `0x929F08` is set to 0 and only counted up (BE1).
- `Char_LevelUpGain`'s EXP compare is signed: an EXP of `0x80000000` or more
  gives level 1. `BattleResult_LevelUpNotice` counts the rows from the OLD
  level's `Char_ExpTable` row (bytes `+6` / `+7` of row `0x904B72`), not the
  new one's; what those bytes are was not read (BE1).
- `Battle_RandomEnemy` takes `Rand`'s answer signed and indexes its frame by
  the remainder: safe only because the CRT's `Rand` answers 0..0x7FFF (BE2).
- `EnemyOp_CastStart` keeps `+9` for an action-4 cast outside an event
  battle, then overwrites or never reads it; `EnemyOp_CastDoneTickUnless`
  increments `+9` every frame until the action ends, read by nothing after
  (BE3).
- `Battle_PickEnemyTarget` keeps the highest `+0x98` as a byte and compares
  each enemy's word with it: a word of 0x100 or more never matches and can
  lower the byte. `BattleEquip_Preview`'s bit is an 8-bit shift: a character
  index of 8 or more always reads "cannot equip". `Battle_OrderPushFront`
  has no floor: at `0x904AE2` = 0 it writes `0x904ACC + 0xFF`.
  `AutoBattle_FillCommands` and `Battle_PrevTarget` compare signed bytes (a
  count or actor of 0x80 or more is negative to them) (BE4).
- The Dragon slot pricers sum the costs in a byte; the three largest of the
  18 cost bytes sum well below 256 (measured off the exe) (BE5).
- The three BMAGIC entry loops end only when their counter meets the length
  byte (1 + 5 n, 1 + 8 n, 3 + 6 n): another length runs on past the record;
  ours aborts first. `DragonForm_Transform` copies `0x904B87` genes into the
  history unbounded (more than three run into the older records). The
  BMAGIC cells hand the GTE vertices whose fourth short is stale stack
  (D101's kind; ours passes 0, the fuzz keeps the six dwords out of the
  compare and puts them back, [`boss_harness.md`](boss_harness.md) `Group::kept`)
  (BE6).
- `BattleResultWin_DrawLevelUp` calls `0x432170` twice per stat shown;
  `BattleResultWin_DrawItem` leaves `dim` on the stack as `Menu_DrawIcon8`'s
  fourth word; `GeneWin_DrawListFrame`'s third and fourth words are never
  read; `BattleEquipWin_Draw` draws every usable item twice (dimmed, then in
  its colour). `BattleResultWin_DrawDrops` `0x5984B0` is the draw behind
  D47's empty first slot (BE7).
- `Config_DrawRowLabel` with a row above 5 reads its own stack frame as a
  string pointer (the caller loops six rows). `EffectKind32_Throw` hands
  `MoveCmd_Move` a stack object with only `+0` and `+4` written, exactly the
  two it reads (FC1).
- `EffectKind34_V0Burst` gives its ten records `+2` = 2 but not `+1`, so
  they run variant 0 only because `Effect_Release` clears bytes 0..4;
  `EffectKind3A_Hit` sets the leader's `+7` even when no record was free for
  the piece (FC2).
- `Field_ContentTake`'s category and id reach `Item_NamePtr` and
  `Inventory_Add` as stack dwords whose upper bytes it never wrote; both read
  the low byte only. `Field_PendingRelease` / `Field_PendingNext` shift by
  the member number mod 32: at 0 "bit n - 1" is bit 31 of a 16-bit word,
  never set - a shape, not a defect (FE1).
- `Field_FloorHurt` past kind 8 reads past its 27-byte stack table into the
  return address (its callers pass a cell code - 0x80 for codes 0x80..0x88
  only; ours aborts). `MapCell_DrawFrames`' threshold scan has no bound and
  a byte `+2` no subrecord lands on never ends the walk, as `MapCell_DrawQuads`',
  `_DrawShaded`'s and `_DrawSpinning`'s; a record of 0 dwords or a run the
  steps overshoot never ends `AreaMap_ClearCell`'s walk (ours aborts) (FE2).
- The icon wheel's sort swaps each coordinate through a byte register: the
  one moved down keeps its low byte sign-extended; the turned spots stay
  within about 48 units, so no shipped kind shows it. `EventScript_SkipIf`
  never passes an FF (or an FD..FF control `EventScript_SkipControl` leaves
  in place): a malformed script hangs, as `EventScript_Run`'s controls do
  ([`event-script.md`](event-script.md)). DIV-0029's note on `ebx`: the
  code pushes `ebx` at `0x5769C7` as a register save and the window
  argument is the `push 0` at `0x5769C8`; what cuts the glyph stays
  unexplained (FO).
- `SharedList_Compact`'s passes and `SharedList_SortCost*`'s sorts are
  quadratic (127 x 127 steps, each swap a call): harmless, noted for the
  fuzz's call counts (FS).

**Status:** latent, harmless by reading; `Field_FloorHurt`, the BMAGIC loops
and `AreaMap_ClearCell`'s walk abort in ours where the original would read
on.

## D197 — A space in a growing shout commits a primitive with a stale glyph word (fixed by DIV-0070)

**Seen:** the owner, 2026-10-02, in play, English overlay: the game crashed
in area `0x63`, message `0x24`, the message box in its grow effect
(`build/bof3x.crash-30104-0.dmp` in the main checkout: `Font_UnpackGlyph`
reading `0x17053EA0`). Capcom's, not ours: `0x4987E0` was still Capcom's
code, called by address from our `MsgBox_Step`.

**Established:** `0x4987E0` (now `MsgBox_EffectDraw`) jumps over the glyph
word `+0x16` and the eight texture bytes for a `0x20` (`0x498819`) and
commits the primitive anyway, so the packet slot's leftover glyph word is
drawn - here `0xC254`, 14 MB past `Font_GlyphData`. The PSX twin does the
same for its word separator `0xFF`. Capcom's Chinese script never puts a
space in a grow span; the English overlays do in 15 places (areas 11, 40,
41, 99), each a stale quad on the original draw and a crash when the word
is far enough out ([`msgbox.md`](msgbox.md) §9).

**Status:** fixed by DIV-0070 (a space commits nothing); recurs under
`BOF3X_ORIGINAL=MsgBox_EffectDraw`. Not yet seen fixed in game.

## D198 — A textured quad's far edge samples the texel past it above scale 1: the fishing menu's stray frame lines

**Seen:** owner, 2026-09-30 (`analysis/shots/owner_catalogue/fishing_equip_menu.webp`),
English, the wide picture at scale 4: a thin vertical line right of the
EQUIP and GUIDE boxes of the fishing equip menu (and right of the ROD / LURE
list), and a short mark under EQUIP's bottom edge. The camping route's frame
3360 (`analysis/shots/camping/f3360.png`, 1704 x 960) shows the same:
measured off it, the line is **two screen pixels wide - half a game pixel** -
at game x 159.75 (narrow), 3.75 units right of the box's frame, olive like
the frame; it runs y 88..118 beside EQUIP, y 150..215 beside GUIDE and
y 105..207 beside the list - **0x1E, 0x40 and 0x68 high: exactly the heights
of the side quads** `Panel_DrawEdgeQuad(x + 0x88, y + 0x18, 0x1E, 1)`,
`(x + 0x88, y + 0x58, 0x40, 1)` (`EffectKind0F_DrawTwinFrame` `0x469490`)
and `(x + 0x80, y + 0x28, 0x68, 4)` (`EffectKind0F_DrawItemFrame`
`0x469630`). **Configuration:** ours, English, wide; not yet seen narrow or
under `BOF3X_ORIGINAL='*'` (the reading below says both show it).

**Cause, read 2026-10-03 (FL of the fix wave).** `Panel_DrawEdgeQuad`
`0x468950` (ours, faithful) builds a `POLY_FT4` the PlayStation way: the
quad `w` units wide (the record's word, 8 for records 0..3 at `0x653E6C`),
texture u from `u` to `u + w` (`0x10 .. 0x18` for record 1). The PlayStation
never samples `u + w`: its rasteriser leaves out the right column. The
port's `D3d_DrawPolyFT4` `0x5A0C40` takes each corner's coordinate from
`D3d_TexCoords` `0x7CA9E0`, `tc[i] = (i + 0.512) / 256`, so the far corner
is `u + w + 0.512` texels; at scale `k` the last screen pixel of the quad
samples `u + w + 0.512 - 1 / k`, past `u + w` once `k` is 2 or more - the
last half game pixel at 2 (the port's own 640 x 480), the last two of four
screen columns at 4. The texel there is the next piece of the frame art
in the page, opaque: the line. The records are the PlayStation's byte for
byte (the US module's at `0x801E2190`), so the data is not at fault; the
port's coordinate table is. The mark under EQUIP is probably the same on a
sprite's bottom (or right) edge (`UiSprite_Draw` `0x52CFE0` builds the same
primitive) - not settled. It is D28's and DIV-0010's family (`SPRT`'s far
edge), on the polygon path, which nobody had read for its far `u` (d3d-draw.md
section 6's last paragraph: "depends on the far `u` the game's builders put
in the primitive").

**Ours does the same** (`D3d_DrawPolyFT4` and `Panel_DrawEdgeQuad` are ours
and fuzzed equal). **Not fixed here:** the cure is renderer-wide - every
`POLY_FT4` whose far edge is `u + w` (DIV-0010's rule for the polygon path:
move a far corner in by `1 / k` texels, or the table's 0.512 to 0.5 with
half a texel in) - and changes every textured quad of the game, so it is
the owner's call and an entry of its own. **What settles it** (for the
coordinator, no route needed beyond `campingFishing.txt` frame 3360):
the same frame with `BOF3X_LANG=original` and with `wide=0` (the line
should stay, it is neither language nor width), and `BOF3X_SCALE=1` (the
line should go).

**Status:** Capcom's (the port's), by reading; seen in ours at scale 4.

## D199 — The world map's nearer cells paint over the party's shadow (the original's, PlayStation too; fixed behind a switch, DIV-0071)

**Fixed 2026-10-03, off by default:** `BOF3X_LAYERING=1` draws a sprite after
the walkable floor under its feet ([`sprite-draw-order.md`](sprite-draw-order.md)
§19). What follows is the defect as found.

**Seen:** the owner, 2026-09-30, in play on the world map. The party sprite's
shadow ellipse ends at a straight or diagonal edge
(`analysis/shots/owner_catalogue/worldmap_shadow_1..3.png`). On open ground,
in their PlayStation screenshot of the Lost Shore, it is whole. **Configuration:
Capcom's.** The sprite's box is pixel-identical between ours and
`BOF3X_ORIGINAL='*'`, including under Capcom's DirectDraw device
(`worldmap_orig`), and the same with the wide picture.

**Established** ([`world-map.md`](world-map.md) §8): the cut moves with the
ground, not with the sprite. It is whole on open ground and cut along hedge,
slope and fence edges. The sibling's renders of the PlayStation code show the
same thing: whole in some places and cut in others, and cut at the same node,
Cedar Woods (`AREA033_f141611`), as the PC's `f01260`. By reading:
- `Sprite_DrawPass` emits each layer's map-cell row (`DrawLayer_Open`) before
  that layer's sprites, so the rows nearer the camera are drawn after the party;
- their quads paint over whatever of the shadow lies below the feet, and
  sometimes a foot (the field's "floor over Ryu's foot",
  [`sprite-draw-order.md`](sprite-draw-order.md) §11).

The slots involved are not measured.

**Status:** the original design's painter's order, not a port defect. Kept.
A world-map-only fix (draw the shadow after the cells in front) is proposed to
the owner in `world-map.md` §8.3. It would be an Intent divergence, with the
number this wave reserved for it.
