# Known defects of the port, as observed

**Status:** IN PROGRESS (2026-10-23 evening, fix wave 1023 and the second twin-diff step: D348 the menu box's unsigned x and y, the port's, fixed by DIV-0165; D349..D352 from the US-against-JP region census, Capcom's JP-build defects the US build changed: D349 the battle start's uncleared pick flag, fixed by DIV-0163; D350 the Scena16 caption's missing wait, open, held for timing; D351 the gene name's missing NUL, fixed by DIV-0166; D352 the window reset's five fields, open; D335 and D342 found moot, D336 / D338 / D341 / D337 / D346 fixed by DIV-0161 / 0162 / 0164 / 0167 / 0168, D343 corrected and open; 2026-10-23, later, the field, menu, shop, start and exe twin-diff against the US PlayStation's matched twins, 955 takeovers: D340 the fishing panel's marker and range bar 3 and 19 px right of the JP disc's, D341 two `+0x70` writes keeping bytes the PlayStation zeroes, D342 an object kind answered as the pointer itself, D343 the inn night without the PlayStation's pause and resume, D344 the save confirm's skipped message step, D345 three empty positional-sound hooks, D346 `Music_Play(0xFF)` unguarded, D347 the port's ten-frame fade where the PlayStation cuts, all the port's and latent or unheard; D34 gains its remove side, D339 this round's pairs; 2026-10-23, the battle twin-diff against the US PlayStation's matched twins: D335 the scaled damage's full width, D336 two unsigned coordinate conversions, D337 the effect preload's missing voice key-off, D338 a dword read of a word, all the port's and latent; D339 four propagated pairs unrelated and two citations swapped, tooling; 2026-10-23: D334 semi-transparent textured primitives blending every texel, so the rooftop rope draws dim, Capcom's port, fixed by DIV-0160 (the bit-15 round); D333 a 15-bit direct-colour page drawing nothing, so Amalgam's melt was still missing after DIV-0142, Capcom's (issue #111), fixed by DIV-0159; 2026-10-22: D332 the second-language re-break running a glyph-written speaker name into the speech, ours (group OS), fixed by DIV-0156's amendment (group NR: rows never re-flowed); D331 `setup` dropping an existing install's saves, ours, fixed; the reading-row round's fold-in: D326 the nue save's party names under `ja-JP`, the recipe save's, open; D327 the next-page arrow on a one-pair page, the original's rule under DIV-0155, left for the owner's eye; D328 the furigana readings unproofread, open, the readings follow-on; D329 recorded routes needing a press per page a layer splits and D330 the stale `orig3` state-hash pairs, tooling, open; 2026-10-21, the wide-ring round's fold-in: D292 narrowed to kind 0x8D's first frames by DIV-0041's wide-ring amendment, the owner accepts the remainder; D300 fixed by DIV-0151; D301's sibling case in areas 53 and 192 fixed by DIV-0071's third amendment; 2026-10-19, the quicksave round: D322 the save list's unbounded table, Capcom's, fixed by DIV-0146; 2026-10-18 evening: D279 fixed by DIV-0144 and DIV-0069's amendment, kind 16 and the fishing label built for `ja-JP` (D280's part); 2026-10-18, the install-gap round's fold-in: D319 the German disc-only equipment panel's slot icons, ours, fixed by DIV-0101's amendment; D320 the lost fight's music missing on every PSX-only and PSP-only install, ours, open, through D24; D286's French and German remainder fixed by DIV-0069's amendment and D289 by DIV-0064's, the owner's confirmation pending; D279 gains a second path; 2026-10-18: D318, the bedroom's song through Ryu's dream in Teepo's bed, the port's, fixed by DIV-0143, issue #88; 2026-10-17 night: D317, Amalgam's melt and the other captures into VRAM, the port's, fixed by DIV-0142, issue #84; D316 the event battles' track half on disc-built images, ours, fixed by DIV-0101's amendment, issue #83; 2026-10-17, the button unification round's fold-in — three hundred and eleven entries, D1..D315 with D19, D20, D29 and D309 unused; D314 and D315 numbered 2026-10-17 from the round's docs: PSP-EU's ram text naming the wrong button on its own disc and PSP-EU's well testing the West's buttons, Capcom's, the second changed by DIV-0139; in the same fold-in D272 fixed by DIV-0140, D274 by DIV-0137 (not seen), D275 by DIV-0135, D273 found to be D307 (fixed by DIV-0130) and D246 by DIV-0141; D312 the Controller panel's crash on a PC install with no layer, ours, and D313 the disc-only fishing records, ours, both fixed 2026-10-17; D302..D311 numbered 2026-10-14..17 as found (D302 fixed by DIV-0127, D303 by DIV-0101's amendment, D304 and D306 fixed, D305 refused since 2026-10-16, D307 fixed by DIV-0130, D308 by DIV-0132, D310 and D311 fixed 2026-10-17); 2026-10-14, the ultrawide round's fold-in — two hundred and ninety-eight entries, D1..D301 with D19, D20 and D29 unused; D290..D301 numbered 2026-10-14 from the round's docs: D290 the 16:9 off-screen flag and D291 the windows' late free, ours, fixed by DIV-0041's amendments; D292 the far cameras past the cell ring (I37), D293 the breath mote and D294 the column's sound left at Capcom's bounds, D295 the band's patch cells (the owner: leave for now) and D296 the slide-ins seen in the 21:9 band, ours, open; D297 the grab's four CPU readers, the port's, open; D298 the eight recipes without a save and D299 the band census's blind spots, ours, tooling; D300 the faerie through the table, Capcom's, the owner's call; D301 the camp's fire under Garr, ours, fixed by DIV-0071's amendment; in the same fold-in D215 and D243 restored by DIV-0126, D242 rewritten (the gene crystal is Capcom's order, the owner accepts it; FY's fix was the bulb's filament), D283 fixed by DIV-0041's random split and D285 measured in part; D266..D289 numbered 2026-10-13 from the away day's docs: D266 the Western title mark's width, D267 the Meteor Strike chip's lifts, D268 `AREA111`'s segment over a twin row, D278 the Identify panel's question marks and D282 the Config values (fixed 2026-10-17), the exe image's gaps or map (D278's rows found, not built); D269 the French and German `ITEM` / `EXP`, the language build's, fixed by DIV-0117; D272 the Western dialogue's button names, D279 the Japanese fishing banner's NUL glyph (a crash on PSP-JP, one run in five), D280 the exe's strings under `ja-JP` and D289 group 16's one-record name shift, the language build's (D280 the owner's call); D273 and D274 the Western disc-only world-map and dock legends, ours; D275 the PC's static world-map key art, the port's, D58's class; D276 the all-calls tracer's stall, the tracer's; D277 the US title's copyright line on the Japanese layout, the owner's call; D283 the wide picture's `Rand` (fixed 2026-10-13), D284 area 23's sky and D285 the battle flash phases in wide, ours; D286 the fishing equip headings on the disc-only installs and D287 the launcher's clipped lines, ours; D288 Ryu's corrupted LZSS flag byte, the port's data, the owner's call; D270 the `stallion.txt` crash, the port's (an F12 save in an encounter field), fixed by DIV-0121; D271 the all-Capcom `Rand` parting and D281 the Japan disc's closer world-map camera unclassed (D281 the owner's hypothesis: Capcom's own later change); D254 and D255 seen on PSX-US too, D255 now a hang (the owner's answer required), D264 seen on PSX-US before its fix; D258..D264 fixed by DIV-0101's amendments and D265 with D247 on 2026-10-13; D251..D257 from the PSP-only round's end batch and visual findings: D251 the battle framing, D252 the equip icons and number formats and D253 the areas 16 / 45 crash fixed by DIV-0101's amendment; D254 the load screen's `"%X"` and D255 the inn's missing jingle on a PSX-only install the owner's calls; D256 the PSP title's framing, the owner's look; D257 the PAL banks, the owner's call since 2026-10-08; D56 fixed by DIV-0116 (2026-10-13); D244..D250 from the PSP-only round, numbered 2026-10-12 from its group docs: our importer's, map's, language build's and launcher's, none Capcom's (D246 fixed 2026-10-17 on the owner's decision; D247 fixed 2026-10-13; D249 narrowed by `main`'s DIV-0102 - the launcher refuses an unheld tag on a disc-only install - and kept for the engine alone and PC installs; D250's Language-box note checked: DIV-0102 fixes it); D242 the dragon gene clipped (Capcom's order, accepted 2026-10-14) and D243 the lighthouse's light rays before the flash not drawn (fixed by DIV-0126), the owner's route `eldritchGene.txt`; D240 the MP3 decoder's stale count at the end of a stream (mirrored by DIV-0091, fixed by DIV-0093 on 2026-10-11); D241 the owner's report that battle music runs on after a fight, not looked at; D1 fixed by DIV-0010 (confirmed off a capture 2026-09-21); D2 fixed by DIV-0039; D3 moot since DIV-0031 / DIV-0035 (recurs only under BOF3X_ORIGINAL); D4 fixed by DIV-0004 and confirmed in game; D5 fixed by DIV-0022 and DIV-0047; D6, D7, D9, D11 and D12..D16 latent; D8 and D10 unchecked in game; D17 fixed by DIV-0025 and D26 by DIV-0028 (both confirmed in game 2026-09-23; D17 recurred at scale 4 and DIV-0025 was amended 2026-09-27, confirmed in game the same day); D18, D21..D25, D27, D28 and D30..D40 latent (D38 a candidate); D41 fixed in the backend by DIV-0044 (the owner's look owed); D42 a port change, kept; D43..D57 latent, from the seventh round (D43 and D51 candidates; D44, D47, D53, D56 PC only); D58 fixed under an overlay language by DIV-0051; D59..D88 latent, from the eighth round's reading, 2026-09-25 (D86 fixed by DIV-0058 on 2026-09-27, D87 a candidate; D59, D66 and D68 abort in ours where the original would crash); D89..D132 latent, from the ninth round's spell overlays and scheduler, 2026-09-25..27 (D102, D103 and D129 candidates; D89's stack tables, D91, D92 in part, D94 in part, D97 in part, D100 in part, D104, D106, D107 in part and D132 abort in ours where the original would crash or run wild; the rest faithful); D133..D161 latent, from the tenth round's chapter banks and area blocks, 2026-09-27..28 (D133 in most groups, D135 in some, D136's `Area_Descriptors` reads, D137 and D150 abort in ours where the original would run past, fault or crash; D138 and D145 PC only; the rest faithful); D162..D173 latent, from the eleventh round's boss band, 2026-09-28 (D166 a candidate, D167 a question for the owner, D168 owes a PSX comparison; D162, D163, D164 and D170 abort in ours where the original would jump wild, write past the pool or near address 0; the rest faithful); D174 fixed 2026-09-29; D175..D196 latent, from the twelfth round's battle engine and field remainder, numbered 2026-10-01 from the fourteen group docs (D181 a crash candidate, D184, D187 and D194 the owner's questions; D175 in most groups, D176 in eleven of twelve places, D177, D183, D186, D190 and parts of D178 and D196 abort in ours where the original would jump wild, write past the pool, fault, hang or read off the stack; D192 passes zeros for the original's frame; the rest faithful); D197 fixed by DIV-0070; D198 the port's, seen at scale 4, not fixed; D199 fixed by DIV-0071; D200..D238 latent, from the thirteenth round's effect engine, numbered 2026-10-03 from the thirty-seven group docs (D200..D214 the round's common classes, one entry each; D221, D222, D226 and D231 for the owner's eye, D224 and D225 the owner's questions; D200, D206 in part, D207, D208 in part, D216, D218, D220, D222, D225, D229, D230 and D233 abort in ours where the original would jump wild, write past the pool, fault, hang or read a stray pointer; D227 and D228 the original's, replaced in ours by DIV-0068 and DIV-0072; D226 and D231 PC only; the rest faithful))
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
  `SharedScratch` included.
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

**Found:** reading `Field_EncounterThresholdRoll` `0x52FEB0` for the takeover,
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

**Reached since the disc-only installs (2026-10-18):** a PSX-only or PSP-only
install has no song 166 (the lost fight's music), so every lost fight that
asks for it meets this path: D320. The cause there is ours (the importer
writes `000..165`); this entry stays Capcom's latent mechanism.

**Reached by the PC's own data too (2026-10-21):** "every track the game asks
for has a file" does not hold. The five event battles whose files carry no
music have the track word 0xFF, and `GameMode5_Load` asks for song 255, which
has no file: the scene's song started again from its top. DIV-0149 skips the
call there, as the PlayStation names no song (the review of PR #86).

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

**The remove side (read 2026-10-23 against the US twin).** `Inventory_Remove`
`0x591B60` has the same PC-only category-4 branch (`cmp dl, 4` at `0x591BA0`):
a key item's id byte is cleared, the answer is 1 and no count is touched. The
PlayStation's `0x801665A0` (US; JP `0x80166F30`, every masked word identical)
has no category test: it checks and decrements the count list for every
category and clears the id at a count of zero. With the null count pointer of
D35 that path would fault on the PC, so the branch is the port's repair of
its own table layout, not a defect of play; it is here so the add and remove
sides are read together.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP `0x800AFBC8` (US `0x800B2C90`, BATTLE.EMI#15) colours
the row whose on-screen index equals `+0xD` (`0x800AFCB8..CC8`) and raises
the row whose top + index equals `+0xD` or `+0xC` (`0x800AFD04..D20`), as
ours. Whether the window's step keeps the two in agreement is not read.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP `0x801DC00C` / US `0x801DC044` (BATTLE.EMI#3): the
party's charge is added onto ATK (`addu $v0, $v0, $v1; sh` at `0x801DC128`)
and the enemy's stored alone (`sra $v0, $a3, 1; sh` at `0x801DC1BC`, no
add). Whether that was intended is still unknown.
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
## D56 — `Encounter_OnScreen` lost the PSX's lower bounds (PC only; fixed by DIV-0116)

**Found:** group BI, 2026-09-24 ([`inventory_ops.md`](inventory_ops.md)),
`Encounter_OnScreen` `0x5928F0`. Read, not seen. The PSX compares the
projected corners as unsigned shorts against 320 and 240, which rejects a
corner left of or above the screen as well; the port compares floats
against 320 and 240 only, so such a corner passes. Kept faithful; a fix
would be a ledger entry.

**2026-10-12, the PSP-only round** ([`visual-findings-1012.md`](visual-findings-1012.md)
1.3, 1.4): the owner's "combatants shifted left" was first read as this, and
is not - it was D251, fixed. D56 stays latent, PC only (the psx-twin rule: a
port repair). **A repair is proposed, not built - the owner's call:** the
PSX's test (`FUN_801C805C`: the projected corner as unsigned shorts against
319 / 239), so the left and top edges refuse too. Its cost: every encounter
placement can change from the first battle on, so every battle route and
state-hash reference after it is re-recorded; the fuzz compares the
original's bounds, so the new test would ride behind a flag set after the
self-test (DIV-0065's pattern). A ledger entry if taken.

**2026-10-13: fixed by DIV-0116** (the owner's call that morning;
[`d56-encounter-bounds.md`](d56-encounter-bounds.md)). The twin read again:
GAME.EMI `0x801C805C`, `lhu` then `sltiu 0x140` / `sltiu 0xF0` at
`0x801C80D4` / `0x801C80E8`. Ours refuses a corner below 0 behind a flag set
after the self-test; `BOF3X_ORIGINAL=EncounterOnScreenLowerBounds` keeps the
port's bounds. None of nine battle routes reaches such a placement (the A/B
and the state hash, the doc's section 3); a crafted run shows it
(`analysis/shots/d56_before`, `d56_after`).

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

## D63 — A third member (or eighth enemy) marked as faster joins every later extra round of the battle (latent, Capcom's)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-25, group CC ([`battle_turn_steps.md`](battle_turn_steps.md) §4).

**Established:** `BattleRoundEnd_CheckFaster` clears flag 0x8000 (+0x134 /
+0x114, the mark `Battle_MarkFasterSide` sets on the side that outpaces)
with `ecx = 2` over the members from `0x802E74` and `ecx = 7` over the
enemies from `0x93BA74`: members 0 and 1, enemies 0..6. The actor loops
beside it (`Battle_TickCounters`, `Battle_MarkFasterSide`) run 0..2 and
3..10. **The PSX twin `0x801D4B08` has the same bounds** (`sltiu 2` from 0,
`sltiu 0xA` from 3), so the PC inherited it. A third member or eighth enemy
once marked keeps the mark into every later round.

**What the mark does** (read 2026-10-21):

- `Battle_MarkFasterSide` `0x4453C0` is its only setter. It marks a member whose
  agility (`+0xA8`) is at least twice the standing enemies' average and at least
  their highest (`0x4455C0`), and an enemy that outpaces the party the same way
  (`0x445600`). It answers 1 only when some member qualified in that call.
- On that answer the round's end starts an extra round (`0x904B7A = 1`, unless
  the initiative byte `0x904AE4` is 2).
- Only the two order builders read the mark, and only in an extra round:
  `Battle_BuildEntryOrder` `0x444F40` (who is asked for a command) and
  `Battle_BuildTurnOrder` `0x4450E0` (who acts) admit just the marked
  (`battle_damage.cpp`'s `Gated`).
- `Battle_Init` and the encounter set-up zero it, so nothing outlives the battle.

So a third member marked once is asked for a command, and acts, in every later
extra round of that battle, even after it no longer outpaces anyone: say the slow
enemies have died, raising the average, while member 1 still qualifies. An extra
round still needs some member to qualify in that round. An eighth enemy behaves
the same, in an eight-enemy fight only. If the third member always outpaces
anyway, nothing shows.

The JP twin agrees: setter `0x801DB020`, `ori 0x8000` at `0x801DB144` (members
to `sltiu 3`) and `0x801DB274` (enemies to `sltiu 0xB`); turn order `0x801DAAB4`
gated on the extra-round byte. So it is Capcom's. Fixing it would be an Intent
entry. Ours keeps it; controls F7 and F8 plant the full loops and are refused.

A live check, not yet run:

1. Put a member whose agility is just over twice the enemies' average in slot 3.
2. Sample `0x80310C`, `0x904B7A` and the orders `0x904AB6` / `0x904ACC` across
   two extra rounds.
3. Lower slot 3's agility at `0x803080` after the first extra round. It should
   still be entered in the second.

**Status:** latent, reachable in normal play. Capcom's.

**Fixed 2026-10-21** (the owner, "for consistency"): DIV-0154 clears the mark
from all three members and all eight enemies.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP MAGIC065.EMI#0 `0x801EEC90` takes the target byte,
subtracts 3 with no test (`addiu $v1, $v1, -3` at `0x801EEE54`) and indexes
the enemy records with it for the speed and rate reads and both theft
writes, as ours. Whether the steal can ever target an ally stays a game fact.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP `0x801E29CC` / US `0x801E2B14` (SHOP.EMI#0, entry 1 of
the money window's step table `0x801E5DCC`): y steps by `-0x10`, then
`slti $v0, $v0, -0x13` snaps to `-0x14` and ends the step when y is `-0x13`
or above - the same test as ours.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes, every one - Capcom's. Each wait has the same exact compare or flag
test and no frame limit on the JP disc: Pentagram's `+0xB == 6` / `== 0x80`
(MAGIC113 `0x801EF35C` / `0x801EF3A0`), the 0xFF and 0 waits (MAGIC018
`0x801EEF7C`.., MAGIC017 `0x801EEF78`), the state-6 reaction (MAGIC008
`0x801F1474`), the one-call script loop (MAGIC064 `0x801F0CBC`), Drain's
equality (MAGIC079 `0x801EFD2C`), Wall of Fire's bit 7 (MAGIC144
`0x801EF11C`; MAGIC092 carries its own copy where the PC shares one body).

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** The unchecked read is Capcom's, the ordinary-battle crash is the port's.
JP BOSS050.EMI#16 `0x800C1A30` reads the current enemy's cue pointer
(`+0xE8` there) with no test, as ours - but that code is loaded only with
BOSS050, at `0x800C1800`, and BOSS050's own set-up (`0x800C1888`) writes the
pointer first. The battle engine's row table (BATTLE.EMI#15 `0x800B3910`)
gives row 123 no MAGIC file, only the fixed address `0x800C19A8` in the boss
window; in any other fight the PlayStation jumps to whatever that window
holds (not read). The PC links every boss overlay resident, so row 123 always
reaches `Paralyzer_Start` with a pointer no ordinary set-up wrote: the null
read is the port's, made by the merge. TCRF's PlayStation crash fits the
jump into an unloaded window rather than this read (a reading, not measured).

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes for the waits, read in JP BOSS054.EMI#16 (row 128's fixed address
`0x800C19C4` in the boss window; `_Drop` `0x800C1BA8`: the create unchecked,
`_WaitTarget` `0x800C1C18`: the raw target byte, no side mask, the state-6
wait unbounded, `_WaitCaster` `0x800C1B40` the same). But as D102: on the
PlayStation the code is loaded only in BOSS054's fight, so TCRF's hacked
cast elsewhere jumps into whatever the boss window holds - the freeze TCRF
saw there need not be any of these waits. On the PC the code is resident.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP MAGIC104.EMI#3: whirl i's delay is `2i` (`0x801EED90`,
whirl 0's 0), the delay decrements before it tests (`0x801EF190`, so 0 runs
255 frames), the end writes its index through the owner pointer (`0x801EF35C`,
the burst the same at `0x801F03A8`), and the parent moves on at 7
(`0x801EEE8C`), as ours.

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
  Character_IndexById[k]` in the call tables, chapters 6, 8 and 14
  (`Scena08_Scene1` / `11` `0x551590` / `0x553050`, `Scena14_Run2` / `3`
  `0x565240` / `0x565910` step 0x11 / 9 by `0x669734`), `Scena01_Scene06`
  `0x53AC30` step 0xF (a byte of 8 or more clears a bit in `Cond_Flags`
  `0x903F90`), `Scena02_StripMember` `0x542080` (ten bytes at `0x903AEE +
  0xA4 x member`; its caller passes 3 and 4), `Party_AddToLists` `0x591CC0`
  (an id of 24 or more reads past `Character_IndexById`'s 24 and ORs 3
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
  `Area108_FlagIfEffectState5` `0x416EC0` (`Character_IndexById`, 24
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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP BOSS025.EMI#13 `0x800C2184` saves member 0's HP and AP
with `sb` (to `0x800C32D4` / `0x800C32D8`) and its end hook `0x800C2634`
restores them with `lbu` then `sh`: a value above 255 comes back mod 256 on
the PlayStation too. Whether member 0 can have that much in this fight is a
game fact, not read.

## D167 — Set-ups 8..10's exit hooks pose every actor from enemy 0's words (latent, intent open)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-28, group BSB ([`boss_sb.md`](boss_sb.md) §6).

**Established:** `Boss08_Exit`..`Boss10_Exit` copy enemy 0's pose words
(`0x93B9B8` / `0x93B9BA`) onto actors 0, 1 and 2 alike, where set-ups 4..6
pose each actor from its own enemy. Kept as read. Whether that is intended
(enemy 0 may be each fight's only enemy) is the formation data's, not read -
**a question for the owner**, not a defect until answered.

**Status:** latent, intent open.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's, for the code. JP BOSS008.EMI#16's three exits (`0x800C2028`,
`0x800C2114`, `0x800C2210`; a fourth set-up's `0x800C230C` too) each pose one
actor (0, 1, 2, 3) from enemy 0's two words at `0x801EB5F8` / `0x801EB5FA`,
never indexed by the actor. Whether enemy 0 is each of these fights' only
enemy is the formation data's (area 27 rows 7, 6, 5), still open.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's, and the port lost nothing. JP BOSS035 / BOSS047.EMI#16
`0x800C1994` sets `+0xEC = 0x800C1D10`, twelve zero bytes of the overlay that
no instruction writes (the only immediate in `0x1D10..0x1D1F` is that store),
while the overlay's other two kinds use `0x800C1D04` (`00 00 01 01 04 04 02
02 ...`). The suspicion above that the link dropped overlay data is wrong.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes for the code. US `0x801E4C7C` / JP `0x801E4BCC` (BATTLE.EMI#3) does
`lw +0xE8; lhu 2(...)` unchecked in any event battle, and the set-up
`0x801E320C` / `0x801E3184` stores only the animation table at `+0xEC`. The
overlay image holds the eight enemy records as zeros, so a non-boss caster
reads the halfword at `0x00000002`: on the PlayStation a plain low-RAM read
that plays whatever cue id lies there, on the PC an access violation. The
crash is the platform's consequence of Capcom's unchecked read.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. US `0x800A5F80` / JP `0x800A2EE8` (BATTLE.EMI#15, words
identical) waits on window 18 and then clears 17, 19, 16 and 19 again -
window 19 twice, 18 never - where its twin `0x800A5244` / `0x800A21AC`
clears 17, 18, 16, 19 (MSVC dropped the duplicate store on the PC). A source
slip, 19 for 18. The function passes its guard only when window 18's bit 0
is already clear, so what the missing store leaves is the byte's other bits;
whether any can be set there is not answered by these two functions.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Partly. The reserve flags run past their pairs on the PlayStation too (JP
`0x801DE2FC`, US `0x801DE200`, SHOP.EMI#0, words identical), but the reserve
holds four there, not three: the count is a separate byte 12 past the
reserve's start (JP `0x801E6184` against the reserve `0x801E6178`), not its
fourth. The three-entry limit and the count clobber are the port's data
layout. On the PlayStation a fifth entry and beyond lie under the flag
bytes, written after the reserve, so entries 5..8 are overwritten by the
flags; the reserve flags land on odd bytes and never on the count.

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

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes, all three - Capcom's. GAME.EMI#0, US `0x801B8D18` / JP `0x801B9D10`
passes 3 to the raised facing-5 path's third probe; `0x801B79F8` /
`0x801B89F0` and `0x801B7EF0` / `0x801B8EE8` test facings 7 and 3;
`0x801B8A9C` / `0x801B9A94`'s fall-back probes one plain step, as ours.

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

## D200 — Round thirteen's dispatchers and table indexes, unchecked: 257 dispatchers and the indexes beside them, in the effect engine (latent; ours aborts past them)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, by every round-thirteen effect group (their docs'
defects sections, named below). D59's, D133's, D162's and D175's class in
the effect engine; one entry for the class, the groups' docs carry the
tables.

**Established, the dispatchers:** `jmp [table + 4 * byte]` (or `call`) with
no compare, the byte a record's `+1` (a kind's state), `+2` (a sub-state, or
kind 0x18's sub-kind's state), `+3` (a step) or a sub-record's `+1`. The
tables lie end to end, so a byte past one runs the next table's entries or
data. Every group found that the writers of the bytes **in its band** keep
them inside their tables; the writers outside it the docs name are kind
0x66's states 3..5 (set from outside, E2G), kind 0x73's `+1` (the spawner's:
area 132 writes 0 and 1, E3C), kind 0x8D's state 2 (E4C), kind 0x94's state
3 (the spawner's, E4D), `Scena07_TakeEffect49`'s argument stored into `+1`
(its callers pass 0..9, E2D) and `Cond_ByteFE` for sub-kind 0x61's run (the
areas' scripts', E6D). Kind 0x69's dispatcher calls through a stack table of
three: past it, its own saved registers and return address (E3B). The one
case where the band's own code steps a byte past its table is D225. The
257 dispatchers, as the round's `symbols.toml` evidence strings record them
(a search of the round's `[[func]]` rows for `jmp [` / `call [` beside
"unbounded", "unchecked" or "no compare"; where a doc counts more - E2F's
eight tables and `EffectKind52_MoveSparks`' call by a spark's `+1`, E6C's
seven, E1E's other stages, E1B's animation scripts past their end - the
doc's table is the fuller):

- E1A ([`effect_1a.md`](effect_1a.md) §6, 13): `EffectKind01_Run` `0x462BA0`,
  `EffectKind07_Run` `0x462E50`, `EffectKind08_Run` `0x463040`,
  `EffectKind09_Run` `0x463440`, `EffectKind0B_Run` `0x463EE0`,
  `EffectKind02_Run` `0x464660`, `EffectKind03_Run` `0x464F20`,
  `EffectKind05_Run` `0x465310`, `EffectKind0A_Run` `0x4658C0`,
  `EffectKind0C_Run` `0x465A60`, `EffectKind0D_Run` `0x465F10`,
  `EffectKind0F_Run` `0x466080`, `EffectKind1A_Run` `0x4667A0`.
- E1B ([`effect_1b.md`](effect_1b.md) §7, 7): `EffectKind0F_Child0`
  `0x467520`, `EffectKind0F_Child1` `0x4677F0`, `EffectKind0F_Child2`
  `0x467A60`, `EffectKind0F_Child3` `0x467D60`, `EffectKind0F_Child4`
  `0x468020`, `EffectKind0F_Child5` `0x468320`, `EffectKind14_Run` `0x46A5E0`.
- E1C ([`effect_1c.md`](effect_1c.md) §7, 7): `EffectKind3C_Run` `0x46A930`,
  `EffectKind1C_Run` `0x46D890`, `EffectKind1D_Run` `0x46DD50`,
  `EffectKind1E_Run` `0x46E200`, `EffectKind1E_ShardsDraw` `0x46E3B0`,
  `EffectKind1F_Run` `0x46EE90`, `EffectKind20_Run` `0x46EF70`.
- E1D ([`effect_1d.md`](effect_1d.md) §7, 7): `EffectKind21_Run` `0x46F2B0`,
  `EffectKind22_Run` `0x46F450`, `EffectKind23_Run` `0x46F790`,
  `EffectKind24_Run` `0x46F820`, `EffectKind25_Run` `0x46F900`,
  `EffectKind26_Run` `0x46FE60`, `EffectKind27_Run` `0x46FEE0`.
- E1E ([`effect_1e.md`](effect_1e.md) §10, 1): `LeaderPanel_S2` `0x528F50`.
- E1F ([`effect_1f.md`](effect_1f.md) §6, 2): `ChoiceMenu_Run` `0x52A6C0`,
  `ExtraSlots_Step` `0x52A6D0`.
- E1G ([`effect_1g.md`](effect_1g.md) §6, 1): `ItemTrade_Leave` `0x5940F0`.
- E2A ([`effect_2a.md`](effect_2a.md) §7, 7): `EffectKind28_Run` `0x470300`,
  `EffectKind29_Run` `0x4709E0`, `EffectKind2A_Run` `0x4710A0`,
  `EffectKind2B_Run` `0x471930`, `EffectKind2C_Run` `0x471EA0`,
  `EffectKind2E_Run` `0x472060`, `EffectKind2D_Run` `0x472420`.
- E2B ([`effect_2b.md`](effect_2b.md) §7, 8): `EffectKind2F_Run` `0x4731A0`,
  `EffectKind33_Run` `0x4734F0`, `EffectKind35_Run` `0x4737A0`,
  `EffectKind35_ShardsDraw` `0x473850`, `EffectKind38_Run` `0x473DD0`,
  `EffectKind39_Run` `0x473FA0`, `EffectKind3B_Run` `0x4741E0`,
  `EffectKind3D_Run` `0x474940`.
- E2C ([`effect_2c.md`](effect_2c.md) §7, 7): `EffectKind3E_Run` `0x474F40`,
  `EffectKind3F_Run` `0x475090`, `EffectKind40_Run` `0x475760`,
  `EffectKind42_Run` `0x475CA0`, `EffectKind43_Run` `0x475CF0`,
  `EffectKind6B_Run` `0x475DE0`, `EffectKind44_Run` `0x476680`.
- E2D ([`effect_2d.md`](effect_2d.md) §7, 10): `EffectKind45_Run` `0x4771B0`,
  `EffectKind46_Run` `0x477BA0`, `EffectKind47_Run` `0x477E10`,
  `EffectKind48_Run` `0x4781B0`, `EffectKind49_Run` `0x478550`,
  `EffectKind49_V0Run` `0x478570`, `EffectKind49_V1Run` `0x478600`,
  `EffectKind49_V2Run` `0x478680`, `EffectKind49_V3Run` `0x478880`,
  `EffectKind49_V4Run` `0x4789B0`.
- E2E ([`effect_2e.md`](effect_2e.md) §7, 10): `EffectKind48_State8_Run`
  `0x478B30`, `EffectKind48_State9_Run` `0x478C30`, `EffectKind48_State10_Run`
  `0x478D30`, `EffectKind48_State11_Run` `0x478E40`,
  `EffectKind48_State12_Run` `0x478EF0`, `EffectKind4A_Run` `0x47A950`,
  `EffectKind4B_Run` `0x47AA50`, `EffectKind4C_Run` `0x47ABA0`,
  `EffectKind4D_Run` `0x47B2B0`, `EffectKind4E_Run` `0x47B630`.
- E2F ([`effect_2f.md`](effect_2f.md) §7, 7): `EffectKind4F_Run` `0x47B970`,
  `EffectKind50_Run` `0x47BB70`, `EffectKind51_Run` `0x47BC10`,
  `EffectKind52_Run` `0x47C090`, `EffectKind52_Sparks` `0x47C0B0`,
  `EffectKind53_Run` `0x47D600`, `EffectKind56_Run` `0x47D910`.
- E2G ([`effect_2g.md`](effect_2g.md) §7, 11): `EffectKind66_Run` `0x47DBE0`,
  `EffectKind18Sub20_Run` `0x47DFD0`, `EffectKind15_Run` `0x47E2D0`,
  `EffectKind54_Run` `0x47E680`, `EffectKind55_Run` `0x47EA90`,
  `EffectKind57_Run` `0x47ECA0`, `EffectKind5A_Run` `0x47EEA0`,
  `EffectKind5B_Run` `0x47EF40`, `EffectKind5D_Run` `0x47F2B0`,
  `EffectKind5E_Run` `0x47F5D0`, `EffectKind5F_Run` `0x47FD80`.
- E3A ([`effect_3a.md`](effect_3a.md) §7, 5): `EffectKind60_Run` `0x4801F0`,
  `EffectKind61_Run` `0x480590`, `EffectKind62_Run` `0x480B70`,
  `EffectKind64_Run` `0x480D40`, `EffectKind68_Run` `0x481150`.
- E3B ([`effect_3b.md`](effect_3b.md) §7, 10): `EffectKind63_Run` `0x4823D0`,
  `EffectKind65_Run` `0x4828B0`, `EffectKind67_Run` `0x482A00`,
  `EffectKind69_Run` `0x482A80`, `EffectKind69_PartRun` `0x482BD0`,
  `EffectKind69_Part0` `0x482BF0`, `EffectKind69_Part1` `0x482DF0`,
  `EffectKind69_Part2` `0x483540`, `EffectKind6C_Run` `0x483BA0`,
  `EffectKind6C_DrawSparks` `0x483D10`.
- E3C ([`effect_3c.md`](effect_3c.md) §7, 9): `EffectKind6E_Run` `0x484050`,
  `EffectKind6D_Run` `0x4842A0`, `EffectKind6F_Run` `0x4845F0`,
  `EffectKind72_Run` `0x484F50`, `EffectKind73_Run` `0x4852F0`,
  `EffectKind73_RunA` `0x485310`, `EffectKind73_RunB` `0x4853B0`,
  `EffectKind74_Run` `0x485440`, `EffectKind75_Run` `0x485BA0`.
- E3D ([`effect_3d.md`](effect_3d.md) §7, 11): `EffectKind7B_Run` `0x485EC0`,
  `EffectKind7C_Run` `0x486020`, `EffectKind77_Run` `0x486180`,
  `EffectKind77_Count` `0x4861A0`, `EffectKind77_Resume` `0x486280`,
  `EffectKind78_Run` `0x486360`, `EffectKind7D_Run` `0x486640`,
  `EffectKind7F_Run` `0x486F10`, `EffectKind80_Run` `0x487270`,
  `EffectKind81_Run` `0x487890`, `EffectKind82_Run` `0x487C10`.
- E4A ([`effect_4a.md`](effect_4a.md) §7, 5): `EffectKind83_Run` `0x488220`,
  `EffectKind84_Run` `0x4887E0`, `EffectKind85_Run` `0x4889E0`,
  `EffectKind86_Run` `0x488BE0`, `EffectKind87_Run` `0x488F60`.
- E4B ([`effect_4b.md`](effect_4b.md) §7, 8): `EffectKind88_Run` `0x4896A0`,
  `EffectKind89_Run` `0x489BE0`, `EffectKind9D_Run` `0x489D70`,
  `EffectKind9F_Run` `0x48A350`, `EffectKindA4_Run` `0x48A4C0`,
  `EffectKind8A_Run` `0x48A5F0`, `EffectKind8B_Run` `0x48AB30`,
  `EffectKind8C_Run` `0x48AC30`.
- E4C ([`effect_4c.md`](effect_4c.md) §7, 6): `EffectKind8D_Run` `0x48B200`,
  `EffectKind8E_Run` `0x48B300`, `EffectKind8F_Run` `0x48B930`,
  `EffectKind90_Run` `0x48BF00`, `EffectKind93_Run` `0x48BFB0`,
  `EffectKind99_Run` `0x48C0E0`.
- E4D ([`effect_4d.md`](effect_4d.md) §7, 7): `EffectKind91_Run` `0x48C990`,
  `EffectKind94_Run` `0x48CB30`, `EffectKind95_Run` `0x48CDD0`,
  `EffectKind96_Run` `0x48CF00`, `EffectKind97_Run` `0x48D070`,
  `EffectKind98_Run` `0x48D290`, `EffectKind9A_Run` `0x48DDE0`.
- E4E ([`effect_4e.md`](effect_4e.md) §7, 10): `EffectKind9B_Run` `0x48DF90`,
  `EffectKind9C_Run` `0x48E320`, `EffectKind9E_Run` `0x48F070`,
  `EffectKindA0_Run` `0x48FC20`, `EffectKindA1_Run` `0x490A60`,
  `EffectKindA2_Run` `0x4910D0`, `EffectKindA3_Run` `0x4912F0`,
  `EffectKindA7_Run` `0x491AA0`, `EffectKindA8_Run` `0x491BA0`,
  `EffectKindA9_Run` `0x491C60`.
- E4F ([`effect_4f.md`](effect_4f.md) §7, 10): `EffectKindAA_Run` `0x491D70`,
  `EffectKindAB_Run` `0x492510`, `EffectKindAC_Run` `0x4925A0`,
  `EffectKindAD_Run` `0x492660`, `EffectKindAE_Run` `0x492760`,
  `EffectKindAF_Run` `0x492980`, `EffectKindB0_Run` `0x492A50`,
  `EffectKindB1_Run` `0x493430`, `EffectKindB9_Run` `0x493550`,
  `EffectKindBA_Run` `0x493E10`.
- E5A ([`effect_5a.md`](effect_5a.md) §7, 8): `EffectKind18_04_Run`
  `0x4FD470`, `EffectKind18_05_Run` `0x4FD7E0`, `EffectKind18_1A_Run`
  `0x4FDBC0`, `EffectKind18_1F_Run` `0x4FDDD0`, `EffectKind18_06_Run`
  `0x4FDF20`, `EffectKind18_07_Run` `0x4FE330`, `EffectKind18_08_Run`
  `0x4FE750`, `EffectKind18_0A_Run` `0x4FEF50`.
- E5B ([`effect_5b.md`](effect_5b.md) §7, 7): `EffectKind18Sub0D_Run`
  `0x4FF320`, `EffectKind18Sub0B_Run` `0x4FF970`, `EffectKind18Sub0C_Run`
  `0x4FFBB0`, `EffectKind18Sub0E_Run` `0x4FFE40`, `EffectKind18Sub13_Run`
  `0x500300`, `EffectKind18Sub0F_Run` `0x500930`, `EffectKind18Sub4F_Run`
  `0x5011A0`.
- E5C ([`effect_5c.md`](effect_5c.md) §7, 9): `EffectKind18Sub50_Run`
  `0x501500`, `EffectKind18Sub10_Run` `0x501BA0`, `EffectKind18Sub56_Run`
  `0x502020`, `EffectKind18Sub57_Run` `0x502470`, `EffectKind18Sub58_Run`
  `0x502580`, `EffectKind18Sub11_Run` `0x502810`, `EffectKind18Sub12_Run`
  `0x502B30`, `EffectKind18Sub15_Run` `0x502E10`, `EffectKind18Sub17_Run`
  `0x503660`.
- E5D ([`effect_5d.md`](effect_5d.md) §7, 8): `EffectKind18Sub18_Run`
  `0x504430`, `EffectKind18Sub43_Run` `0x504F70`, `EffectKind18Sub1C_Run`
  `0x505100`, `EffectKind18Sub1D_Run` `0x505540`, `EffectKind18Sub14_Run`
  `0x5059A0`, `EffectKind18Sub1E_Run` `0x505AB0`, `EffectKind18Sub21_Run`
  `0x505E20`, `EffectKind18Sub22_Run` `0x5064A0`.
- E5E ([`effect_5e.md`](effect_5e.md) §7, 6): `EffectKind18Sub23_Run`
  `0x506A10`, `EffectKind18Sub24_Run` `0x506BD0`, `EffectKind18Sub3F_Run`
  `0x507620`, `EffectKind18Sub25_Run` `0x507D90`, `EffectKind18Sub26_Run`
  `0x5083B0`, `EffectKind18Sub39_Run` `0x508A80`.
- E5F ([`effect_5f.md`](effect_5f.md) §7, 8): `EffectKind18Sub27_Run`
  `0x508CC0`, `EffectKind18Sub28_Run` `0x509290`, `EffectKind18Sub42_Run`
  `0x509690`, `EffectKind18Sub29_Run` `0x509C90`, `EffectKind18Sub48_Run`
  `0x50A2D0`, `EffectKind18Sub49_Run` `0x50A3C0`, `EffectKind18Sub2A_Run`
  `0x50A760`, `EffectKind18Sub3C_Run` `0x50ACB0`.
- E5G ([`effect_5g.md`](effect_5g.md) §7, 4): `EffectKind18Sub2B_Run`
  `0x50AF90`, `EffectKind18Sub2C_Run` `0x50B520`, `EffectKind18Sub3A_Run`
  `0x50BB90`, `EffectKind18Sub4A_Run` `0x50BFD0`.
- E6A ([`effect_6a.md`](effect_6a.md) §7, 7): `EffectKind18Sub2D_Run`
  `0x50C140`, `EffectKind18Sub3E_Run` `0x50C6E0`, `EffectKind18Sub2E_Run`
  `0x50CA40`, `EffectKind18Sub2F_Run` `0x50CFE0`, `EffectKind18Sub30_Run`
  `0x50D330`, `EffectKind18Sub31_Run` `0x50D8D0`, `EffectKind18Sub32_Run`
  `0x50DE70`.
- E6B ([`effect_6b.md`](effect_6b.md) §8, 9): `EffectKind18Sub33_Run`
  `0x50E400`, `EffectKind18Sub34_Run` `0x50E750`, `EffectKind18Sub37_Run`
  `0x50EAA0`, `EffectKind18Sub35_Run` `0x50EB80`, `EffectKind18Sub38_Run`
  `0x50F110`, `EffectKind18Sub3B_Run` `0x50F230`, `EffectKind18Sub40_Run`
  `0x50FA00`, `EffectKind18Sub41_Run` `0x50FD30`, `EffectKind18Sub44_Run`
  `0x510C20`.
- E6C ([`effect_6c.md`](effect_6c.md) §7, 6): `EffectKind18Sub45_Run`
  `0x511DD0`, `EffectKind18Sub51_Run` `0x512350`, `EffectKind18Sub55_Run`
  `0x5128B0`, `EffectKind18Sub59_Run` `0x512B20`, `EffectKind18Sub5A_Run`
  `0x513860`, `EffectKind18Sub5B_Run` `0x513CD0`.
- E6D ([`effect_6d.md`](effect_6d.md) §7, 6): `EffectKind18Sub5C_Run`
  `0x514270`, `EffectKind18Sub5E_Run` `0x514690`, `EffectKind18Sub5D_Run`
  `0x514A70`, `EffectKind18Sub61_Run` `0x515000`, `EffectKind18Sub64_Run`
  `0x516480`, `EffectKind18Sub65_Run` `0x5169F0`.
**Established, the indexes** by a record byte, a count or an argument with no
bound (the original reads, or writes, what follows):

- E1A (§6): `WorldMap_RecordIndex` past 11 and a record's handler of 0
  (D229); the extra indexes `+0x18`, `+0xC` of 4 or more; `0x939A1C` of 30 or
  more; kind 5's `+6` past `0x653B44`'s four rows; kind 0xD's `+6` past its
  six sound words; kind 0xF's text past thirteen (D230), a row or column past
  `0x653C04`, a line byte of 3 or more in `EffectKind0F_LineNext` `0x466460`
  and `_LineFade` `0x466750` (safe only because `_LineType` sets the wait for
  a line other than 0xFF and `_LineScroll` releases the record for 0xFF).
- E1B (§7): `Panel_DrawEdgeQuad`'s `which` past 5;
  `EffectKind0F_DrawMessageList` `0x468560`'s title id past 6 and line index
  past 0x36; an accessory id past `NameTable_Accessories`' 52 (`_DrawItemsA`
  / `_DrawItemsB`); `EffectKind92_Follow` `0x46A3E0`'s sprite index past 30 (a
  write past `Sprite_Objects`). "None is reached from the states' own
  starts."
- E1C (§7): `EffectKind36_Run` `0x46A850`'s frame index past
  `EffectKind36_Frames`' four; `EffectKind70_Run` `0x46D780` with more than
  three members (the bit `1 << i` with the count masked to five bits: members
  3..7 write past `ObjTrio`'s three records - only when `Field_MemberCount` is
  above three and the spawner set those bits).
- E1E (§10 L1, L3): `0x939A1C` at 0xFF ("none", stage 1's step `0x5289C0`)
  indexes `Sprite_Objects` in stage 4's steps, reading and writing
  `0x7E91DC..` (whether play reaches stage 4 with 0xFF is not measured);
  `0x9040EC + kind` by the sprite's `+6` (no bound known; ours reads as the
  original).
- E1F (§6 L3, reads): `UiSprite_SetMode` `0x52CF60` into ten records,
  `UiSprite_Draw` `0x52CFE0` into 74 (every constant index at the 131 call
  sites is inside), `FieldPanel_KindPoints` `0x52CE60` by a kind byte,
  `ExtraSlots_PickItem` `0x52A8F0` by `+0x36` / `+0x3A` and by an id - in place
  in the image or the save block, the same on both sides.
- E1G (§6 L3, reads): the record numbers at `0x66AD10 + 10 * row + k` and the
  record at `0x66AB58 + 8 * number` - a pick past the row count or an entry
  byte 0xFF reads past the table's 55 (inside `.data`); kept unchecked as
  FE2's `TradeRecord` is.
- E2E (§7): `EffectKind48_State7_Swell` `0x478A40`'s two eight-byte tables by
  `+9` (0..5 in play); `EffectSphere_Draw` `0x47A780` trusts its quads' vertex
  numbers (only `EffectSphere_Build` `0x47A560` writes them, below 0x1E2).
- E2G (§7): kind 0x54's `+0xB` into the thirty `Sprite_Objects` (area 8 stores
  the active member's index, in range only while that member is a field
  object of the pool); kind 0x5B's `+0xB` into area 75's two slots.
- E3A (§6): kind 0x61's `+7` past `ObjTrio`'s three; the frame byte past the
  two frames; a mote's `+1` past `EffectKind68_MoteStates`' three.
- E3B (§7 item 4): `EffectKind63_End` `0x482650` and `_RefreshSprites`
  `0x4826B0` walk `ObjTrio` by `Field_MemberCount` (writes past three; the
  count is at most three in play).
- E3C (§7): kind 0x6D's block `0x92C780 + 0x500 * +6` (no spawner found to say
  how many blocks there are; the next pool, `0x92D1DC`, starts inside the
  third), its shape `+4` into eight rows, kind 0x6F's colour `+6` into one row,
  `Party_MemberAt`'s answer into `ObjTrio`.
- E3D (§7): kind 0x7D's `+6`, which area 170's case 40 stores as the message
  word's low byte less 0xB: `EffectKind7D_Input` `0x486690` writes the turn to
  `0x675DC8 + +6` (a byte below 0xB or above 0xD writes past the three dials)
  and `EffectKind7D_Start` `0x486660` takes that address unchecked.
- E4A (§7): kind 0x86's `+3` / `+4` (from `Sprite_FindFree`, 0xFF tested in
  `EffectKind86_Spawn` `0x488C00` only) and kind 0x85's `+6` from
  `Area144_SpawnEffect85` `0x421090` (`(Field_ActiveMember - Sprite_Objects) /
  0xA4`, unchecked): past 29 the writes land in the draw records at
  `0x7E01C0`.
- E4B (§6, §7): its indexes, all kept in range by the band's own code.
- E4D (§7): kind 0x96's index (kept 0..7 by its own wrap); `Field_MemberCount`
  into `ObjTrio`.
- E5A (§7): `+0x36` into sub-kind 5's rows (5), sub-kind 6's flags, rectangles
  and part runs (3), `+0x3A` into the levels (4), `+0xB` into sub-kind 7's
  doors (3), `+9` into `_Topple`'s (0x29) and `_Crash`'s (0xE) tables, `+2`
  into `_Pattern`'s heights and the `Cond_ByteFE` values, the door variant
  into `0x65DF2C` (32); `Gfx_ClutStripCopy16` `0x4FDB70` past its strip
  (`EffectKind18_05_FadeIn` `0x4FD910`'s slot goes negative past `+9` 44; the
  state resets `+9` past 0x1E). "In play each byte comes from the same tables
  or a bounded count."
- E5B (§7): sub-kind 0x0D's glow by the column `+0x36` and its frames by
  `+9 >> 1` / `+9 / 3`, sub-kind 0x0C's flag and VRAM column by `+0x36`,
  sub-kind 0x13's by `+0xB`, the gates' by the variant `+0x36` (8) and `+0x3A`
  (4: `0x65E034`, below).
- E5C (§7): sub-kind 0x50's variant from the record's x byte into two cells
  and its `+0xB` into four flag bytes (`0x65E034`); sub-kind 0x57's `+0xB`
  into eight points; `Gfx_BufferIndex` into layer 15's two last pointers.
- E5D (§7): sub-kinds 0x14 / 0x1E by the x cell word `+0x36` (two places),
  0x22 by it (four), 0x19 by `+0xB` (two); E5C's `EffectKind18Sub17_DrawPatch`
  `0x503FA0` by its argument (three) and `EffectKind18Sub17_CopyFrame`
  `0x5043B0` by its (ten); `EffectKind18Sub22_SetMap` `0x506640`'s corner row
  by a running count that the four rectangles (7, 11, 11, 11 cells) keep
  inside its eleven.
- E5E (§6, §7): `EffectKind18Sub24_TrailsOut` `0x506ED0` with `+9` below 8
  would draw from before the polyline (not reached).
- E5F (§6): its list.
- E5G, E6A, E6B, E6D (§7, §7, §8, §7): the variant index is the spawn's x
  cell, s16, into tables with room for four / five (E5G), 16 / 10 / 4 / 2
  (E6A), two or four (E6B), two (E6D) - past them the next table's bytes, for
  some kinds a state table's code pointers, are read as a cell; E5G's 0x36
  and E6B's 0x54 index four bytes by `+2` before their own `& 3` wrap, from
  what the spawn left; E5G's draw side into `0x65EA74`; E6A's 0x2D signs by
  `+0xA`; E6B's 0x41 `+9` into its curve (room 0x1C, kept below 0x18) and the
  draw's variant into two rows; E6D's 0x5C textures by `+0xB` and signs by
  the side, the tracks' ring by a head byte `EffectKind18Sub61_Reset`
  `0x515100` never writes.
- E6C (§7): sub-kind 0x51's variant `+0x36` into five rectangles; `DrawRing`'s
  half into two uv pairs; `SetTiles`' `step % 3 + 3 bank` into six stack bytes
  (a negative step reads the frame); `DrawPart`'s part into three ranges; the
  sky's tints by `(w - 0x390) / 3` (past 0x3BF the twinkle offsets;
  `Area189_StepArrive`'s wrap at 0x3C0 keeps it from happening);
  `MapView_ItemAt`'s answer and the link word `& 0xFFF` into `DrawItems`
  (1,024 items, 2,048 under DIV-0062).

**Readings that disagree** (each for one read of the code to settle; nothing
is changed here):

- **`EffectKind14_Op` `0x653F78` and `EffectKind36_Frames` `0x653F88`.** FC1
  ([`field_c1.md`](field_c1.md) §1.7; `symbols.toml`, status hypothesis)
  counts the event op 32 bytes, to `EffectKind3C_States` `0x653F98`. E1C
  ([`effect_1c.md`](effect_1c.md)'s opening table; its `symbols.toml` row) reads
  `0x653F88..0x653F97` as kind 0x36's four s16 pairs, which leaves the op 16
  bytes. How much of the op `EventOp_6x` reads would decide it.
- **`EffectKind0F_States` `0x653C28` and `EffectKind1A_States` `0x653C5C`.**
  E1A ([`effect_1a.md`](effect_1a.md) §3, §10; `symbols.toml`) counts kind
  0xF's table 13 and kind 0x1A's 17 from `0x653C5C`, its entries 13..16 being
  E1B's states. E1B ([`effect_1b.md`](effect_1b.md) §1, §3) reads kind 0xF's
  dispatcher `0x466080` through a 41-entry run from `0x653C28` - its states
  26..40 E1B's, entries 30..40 the same cells as `EffectKind0F_Children`
  `0x653CA0` and `EffectKind0F_Child0Steps` `0x653CB8`. The cells
  `0x653C90..0x653C9F` are kind 0x1A's entries 13..16 by the one reading and
  kind 0xF's 26..29 by the other; "past the table" for kind 0xF is entry 13
  by E1A's count and 41 by E1B's.
- **`0x654468`** (the round doc's wave-two debt). E2C
  ([`effect_2c.md`](effect_2c.md) §3) found `band_rows.py` filing `0x474F60` /
  `0x474FC0` as entries 22 and 23 of kind 0x35's run `0x654410` and reads
  `0x654468` as kind 0x3E's (`EffectKind3E_States`, 2). Checked against E2B
  for this entry: E2B's doc (its opening section, before §1) and its `[[data]]` rows count
  `EffectKind35_States` 3 (to `EffectKind35_ShardStates` `0x65441C`) and name
  `0x654468` as kind 0x3E's - **the two docs agree**; the disagreement was the
  tool's run.
- **`0x65E034`** (four bytes, named by neither group). E5B
  ([`effect_5b.md`](effect_5b.md) §1, §3) reads it by `+0x3A` in
  `EffectKind18Sub0F_Start` `0x500950`; E5C ([`effect_5c.md`](effect_5c.md)
  §3) by `+0xB` in sub-kind 0x50's states (the wall's texture flag), and
  names E5B's `0x50096F` and `0x5011E0` as readers too. Both count four
  bytes; which index bytes stay below 4 is each group's reading.

**What ours does:** aborts with a message past each table and each index
above (the round-nine rule), except the reads E1E L3, E1F L3 and E1G L3,
which ours makes in place as the original does, and E5C's
`Gfx_BufferIndex`, which ours indexes as read.

**Status:** latent; ours aborts where the original would jump wild or read
or write past a table. No group found a writer that leaves its table other
than those named above and D225.

## D201 — The effect kinds' working pools overlap from `EffectKind30_Shards` `0x92BF80`: one copy per kind, no owner (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03: described as a defect by E1C, E2A, E2B, E2E, E2F, E3A,
E3B, E3D and E4C (their docs' §7), and as layout by E2C, E2D, E3C, E4B, E4D
and E4F (their §1, §3 or §4). The round doc's wave-two debt (one entry, and
the symbol's extent).

**Established:** a large part of the effect engine keeps its particles,
shards, trails, rings and debris outside `Effect_Objects`, in the memory
from `0x92BF80`, each kind with its own stride and its own "free" test, and
one copy per **kind**, not per record. Nothing in the code checks that
another kind is using it: two such kinds alive at once (or, for E2E's kind
0x4A, two records of one kind) write over each other's records and counts.
The docs' examples: a drop's frames word is every fifth speck's in-use byte
(E2A); kind 0x4B's debris 31 lies over kind 0x48's ring (E2E); a kind-0x50
speck in use over kind 0x52's trail is drawn from trail data and moves it
(E2F); kind 0x93's start clears kind 0x90's shards outright (E4C); kind
0x44's clear leaves another kind's bytes to be drawn as sparks (D232); kind
0x45's trace reads kind 0x49's glow record (D233). **Reach, as the docs say
it:** E2A - no two of these kinds are spawned together by anything found;
E3A - chapter 10's run 2 spawns kinds 0x68, 0x61, 0x62, 0x69 and 0x64 on its
count and timers, whether two are alive at once not measured; E2E, E4C - not
traced; E2F - the owner's to say.

The layouts, by doc:

- E1C (§7): kinds 0x1C / 0x1D's specks (0x14 apart), kind 0x1E's eight
  shards (0x28), kind 0x1F's debris (0x2C) and, from `0x92C0C0`, kind 0x1E's
  pieces.
- E2A (§1, §4, §7): specks (0x80 of 0x14), sparks (0x28) and drops (0x18); the
  centre `0x92D380` and kind 0x2C's ground points `0x92D390` after them.
- E2B (§7): kind 0x2F's three trails (to `0x92C448`), kind 0x35's sixteen
  shards, 55 pieces and 55 face copies (to `0x92CFC0`), kind 0x3B's three
  spirals (to `0x92CDB4`), kind 0x3D's sixteen rings (to `0x92C100`).
- E2C (§1): kind 0x40's 32 shards of 0x18 and its disc (`0x92C280`,
  `0x92C28C`), kind 0x43's 256 sparks of 0x1C, kind 0x44's ring record
  (`0x92BF80`, through `EffectKind44_RingCell`) and its eight sparks of 0xA0
  (`0x92C07C`), kind 0x6B's read-back pixels; its particles at `0x92EC80`
  (D217).
- E2D (§1, §7): kind 0x45's 36-sample history from `0x92BF80`, kind 0x49's
  glow record `0x92C060`, sixteen sparks of 0x18, 64 dust records at
  `0x92D1C8`.
- E2E (its opening table, §4, §7): kind 0x4A's trail `0x92BF80..0x92CC83`, kind 0x4B's debris
  `0x92CC84..0x92D203`, kind 0x48's spiral `0x92C4A4..0x92D1C3`, ring
  `0x92D1C8..0x92D1D9`, burst `0x92C060..0x92C49F` and sparks
  `0x92BF80..0x92C05F`; the sphere's light to `0x931174`.
- E2F (§7): kind 0x50's 64 specks of 0x28 (to `0x92C980`), kind 0x52's eight
  sparks of 0x1C (to `0x92C060`) and its trail `0x92C060..0x92C4A0`.
- E3A (§3, §7): kind 0x62's 24 rays of 0x38 (to `0x92C4C0`), kind 0x64's
  eight sparks of 0x18 (to `0x92C040`) and sixteen shards of 0x2C from
  `0x92C040` (to `0x92C300`; kind 0x64 does not overlap itself), kind 0x68's
  sixteen motes of 0x1C (to `0x92C140`), kind 0x61's read-back pixels from
  `0x92BF80`; its particles from `0x92DF80` (D217).
- E3B (§1, §7): kind 0x6C's sixteen sparks of 0x28, reached through the
  cursor `0x67626C`.
- E3C (§1, §3): kind 0x6E's sixteen shards of 0x28 (through the same cursor
  `0x67626C`), kind 0x72's 32 debris of 0x2C, kind 0x73's 32 sparks of 0x18
  (cursor `0x676274`); kind 0x6D's blocks at `0x92C780`, which E1C's kind
  0x1E's debris shares.
- E3D (§1, §4, §7): kind 0x80's trail (0x440 bytes), kind 0x81's 256 drops
  (0x1800 bytes from `0x92BF80`) and its sources at `0x92D780`.
- E4B (its opening table, §1): kind 0x87's six panes of 0x1C, kind 0x88's 32 particles of 0x2C.
- E4C (its opening table, §4, §7): kind 0x8E's two pools over `0x92BF80..0x92E580` (128 chips
  of 0x28; 128 dots of 0x24 at `0x92D380`, over E2A's centre); kinds 0x8F,
  0x90 and 0x93 in the first 0x500 bytes.
- E4D (§1, §3): kind 0x97's eight sparks of 0x18 and its 32 debris of 0x2C at
  `0x92C040`.
- E4F (§1): kind 0xAB's 256 drops of 0x18 (kind 0x81's pool) and eight
  sources at `0x92D780`; kind 0xB9's eight sparks of 0x18 (also called by kind
  0x64's states 4..7); kind 0xB0's bars.

The functions that lay, clear, find or walk records there, as their
`symbols.toml` evidence names the buffer (a search of the round's `[[func]]`
rows for `EffectKind30_Shards` or an address in `0x92C000..0x92EFFF`; the
states that only call them are not listed):

- E1C ([`effect_1c.md`](effect_1c.md), 7): EffectKind1C_MoveShards `0x46DD00`,
  EffectShards_Clear `0x46E120`, EffectKind1E_ShardsInit `0x46E2E0`,
  EffectKind1E_SplitModel `0x46E6E0`, EffectKind1E_DebrisInit `0x46EA60`,
  EffectKind1F_Start `0x46EEB0`, EffectKind1F_Debris `0x46EEE0`.
- E2A ([`effect_2a.md`](effect_2a.md), 4): EffectSpecks_Spawn `0x471D10`,
  EffectKind2C_Start `0x471EC0`, EffectSparks_Clear `0x4723E0`,
  EffectDrops_Clear `0x472D70`.
- E2B ([`effect_2b.md`](effect_2b.md), 4): EffectKind2F_Start `0x4731C0`,
  EffectKind35_SplitModel `0x473B60`, EffectKind3B_InitSpirals `0x474770`,
  EffectKind3D_RingsClear `0x474C30`.
- E2C ([`effect_2c.md`](effect_2c.md), 9): EffectKind40_ShardsClear
  `0x4757F0`, EffectKind40_ShardDraw `0x4758D0`, EffectKind40_DrawDisc
  `0x475A90`, EffectKind43_Setup `0x476230`, EffectKind6B_Store `0x475EC0`,
  EffectKind6B_Scatter `0x475F20`, EffectKind44_Start `0x4766A0`,
  EffectKind44_SparksClear `0x476D00`, EffectKind44_SparksStep `0x476D20`.
- E2D ([`effect_2d.md`](effect_2d.md), 8): EffectKind45_Start `0x4771D0`,
  EffectKind45_PushSample `0x4777A0`, EffectKind45_DrawTrace `0x477820`,
  EffectKind48_FindFreeSpark `0x478320`, EffectKind49_V2Launch `0x4786A0`,
  EffectKind49_V2Fly `0x478720`, EffectKind49_V2Rise `0x4787F0`,
  EffectKind49_V2Sink `0x478840`.
- E2E ([`effect_2e.md`](effect_2e.md), 9): EffectKind48_State7_Start
  `0x4789D0`, EffectKind48_State8_Start `0x478B50`, EffectKind48_State9_Emit
  `0x478CC0`, EffectKind48_State12_Start `0x478F10`, EffectSphere_Build
  `0x47A560`, EffectSphere_Draw `0x47A780`, EffectKind4A_Follow `0x47A9C0`,
  EffectKind4A_TrailInit `0x47AC80`, EffectKind4B_Start `0x47AA70`.
- E2F ([`effect_2f.md`](effect_2f.md), 8): EffectKind52_Trail `0x47C160`,
  EffectKind52_TrailStart `0x47C190`, EffectKind52_TrailFade `0x47C320`,
  EffectKind50_ClearSpecks `0x47C350`, EffectKind52_ClearSparks `0x47CF00`,
  EffectSpark_FindFree `0x47CF20`, EffectKind52_TrailUpdate `0x47D040`,
  EffectKind52_TrailDraw `0x47D220`.
- E3A ([`effect_3a.md`](effect_3a.md), 6): EffectKind61_Store `0x480730`,
  EffectKind61_Scatter `0x4807A0`, EffectKind62_ClearRays `0x4812B0`,
  EffectKind64_ClearSparks `0x482050`, EffectKind64_DrawShards `0x482360`,
  EffectKind68_ClearMotes `0x481B80`.
- E3B ([`effect_3b.md`](effect_3b.md), 1): EffectKind6C_ScatterSparks
  `0x483C10`.
- E3C ([`effect_3c.md`](effect_3c.md), 4): EffectKind6E_FindShard `0x4841C0`,
  EffectKind6D_ClearParticles `0x4842F0`, EffectKind72_Start `0x484F70`,
  EffectKind73_FindSpark `0x485810`.
- E3D ([`effect_3d.md`](effect_3d.md), 6): EffectKind80_Start `0x487290`,
  EffectKind80_Fade `0x4873A0`, EffectKind80_TrailStep `0x4873E0`,
  EffectKind81_ClearDrops `0x487920`, EffectKind81_PlaceSources `0x487AB0`,
  EffectKind81_FindFreeDrop `0x487BF0`.
- E4B ([`effect_4b.md`](effect_4b.md), 3): EffectKind87_Setup `0x489030`,
  EffectKind87_StepPanes `0x489220`, EffectKind88_ClearParticles `0x4897A0`.
- E4C ([`effect_4c.md`](effect_4c.md), 7): EffectKind8E_MoveChips `0x48B4A0`,
  EffectKind8E_ClearChips `0x48B640`, EffectKind8E_MoveDots `0x48B730`,
  EffectKind8E_ClearDots `0x48B850`, EffectKind8F_ClearShards `0x48BC40`,
  EffectKind90_FindShard `0x48C220`, EffectKind90_ClearShards `0x48C240`.
- E4D ([`effect_4d.md`](effect_4d.md), 3): EffectKind97_WaitCue `0x48D100`,
  EffectKind97_EmitSpark `0x48D650`, EffectKind97_DrawDebris `0x48D980`.
- E4F ([`effect_4f.md`](effect_4f.md), 5): EffectKindAB_MoveDrops `0x492AF0`,
  EffectKindAB_PlaceSources `0x492C80`, EffectKindB0_ClearBars `0x493330`,
  EffectKindB9_SpawnSpark `0x493B50`, EffectKindB9_DrawShards `0x493BF0`.
**The extent, readings that differ.** `symbols.toml` gives
`EffectKind30_Shards` **1,344 bytes** (FC2's 24 records of 0x38, to
`0x92C4C0`; `EffectKind30_Sparks`, 260 bytes, after it), and E3D calls the
symbol 0x540; the harness's standard region is **0x644** (both symbols; E2A,
E2E, E2F, E3C and E4C cite it); E2B: kinds 0x35 and 0x3B use past the
symbol, **to `0x92CFC0`** (the round doc's debt, "the symbol's count"); E2C:
one buffer from `0x92BF80` **running on to the particles at `0x92EC80`**; E3D
to `0x92D8C0`; E4C to `0x92E580`; E3A's particles to `0x941F80` (D217). No doc
reads what the image places after the buffer beyond the cells named. One
reading of where the PlayStation's pool ends would settle the symbol's count.

**The cursor `0x67626C`** (the round doc's wave-three debt): written by E3B's
kind 0x6C (`EffectKind6C_ScatterSparks` `0x483C10` sets it to the buffer and
reads it again for every access) and E3C's kind 0x6E (`EffectKind6E_Emit`
`0x484090`, `EffectKind6E_DrawShards` `0x4841E0`; E3C lists E3B's writers at
`0x483C10`, `0x483D30..`, `0x483FA0`, `0x484000`), over the same sixteen
records of 0x28. E3B calls it the **spark** cursor ([`effect_3b.md`](effect_3b.md)
§1, §3), E3C the **shard** cursor ([`effect_3c.md`](effect_3c.md) §3), and each
left it unnamed for the other or the coordinator. The docs agree on the cell
and the records; they differ in the name only. The two kinds share the
records as well as the cursor - an instance of this entry.

**What ours does:** reproduces; each group's fuzz keeps the regions it
touches (their §4).

**Status:** latent, faithful; whether two users are ever alive at once is
measured by no group.

## D202 — Effects whose last state never releases the record (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E2A, E2B, E2D, E2F, E2G, E3B, E4B, E4C, E4D, E4E,
E4F, E5G, E6B and E6C (their docs' §7, E6B's §8).

**Established:** the kind's own states never step to a release; the record
lives until something outside frees it, and the docs did not trace what does
(E4E: "the area's tail, an area change").

- E2A: kind 0x2E's `EffectKind2E_Burst` `0x4720F0` (state 2) never moves
  `+1`: once its counter reaches 3 it throws up to sixteen sparks every frame,
  as many as the pool has free. Whether a script releases it was not traced.
- E2B: a kind-0x3D ring whose state is past 1 stays in use for ever, undrawn,
  and keeps `EffectKind3D_RingsStep` `0x474C80` answering 1, so kind 0x3D never
  releases - nothing writes such a state.
- E2D: kind 0x46's state 4, `EffectKind46_Hold` `0x477D10`, draws the flash
  and the redraw every frame; its spawner (chapter 7) waits on the counter and
  something else must free the record. The redraw leaves every live sprite's
  and the leader's `+0x29` at 5.
- E2F: kinds 0x51 and 0x56 wait on the chapters' counters (`0x903848` = 0xB,
  `0x90384B` = 0xA then 0xE); spawned where no event script sets them, they
  wait for ever ("by design, not a fault").
- E2G: kind 0x57's `EffectKind57_Show` `0x47ECC0` steps to its release only
  outside area 2; there the ring is drawn until something else frees it.
- E3B: `EffectKind67_SpawnKind13` `0x482A20` never leaves its state: one
  kind-0x13 record a frame for as long as the kind-0x67 record lives (until
  the pool is full, then one whenever a record frees). Whether that is meant
  is the owner's question.
- E4B: kind 0x89's `EffectKind89_Fade` `0x489C90` sets `+1` 3, a `BareRet`;
  kinds 0x9D and 0x9F have a state 3 no code of the band sets, and kind 0x89's
  state 2 is likewise set from outside.
- E4C: kind 0x8D's state 1 is never left by the band's code (something
  outside writes 2).
- E4D: kind 0x96's `EffectKind96_Pulse` `0x48CF50` never advances nor
  releases (its quad, 320 x 320, overhangs the frame by 80 rows, clipped); no
  spawner of it was found.
- E4E: kind 0x9B never leaves state 2: the ring stays until something else
  frees the record ("noted for whoever traces area 100").
- E4F: kind 0xB1 (`EffectKindB1_Run` `0x493430`): no state of its table
  releases the record.
- E5G: sub-kind 0x36 (`EffectKind18Sub36_Pulse` `0x50B480`): no state releases
  or advances past itself.
- E6B: sub-kinds 0x3D (`EffectKind18Sub3D_Ripple` `0x50F820`) and 0x54
  (`EffectKind18Sub54_Pulse` `0x50F780`): no state releases (0x3D's corner
  adds are D211's).
- E6C: sub-kind 0x53 (`EffectKind18Sub53_Run` `0x512660`, one state, no
  release); sub-kind 0x5A's cycles; sub-kind 0x44's states (E6B's
  dispatcher).

**What ours does:** reproduces.

**Status:** latent, faithful; each is harmless while what spawns the kind
also frees it, which no doc traced.

## D203 — Draws skipped while a state waits (latent, faithful; the owner's eye)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E2G, E5B, E5C, E5E, E5F, E5G, E6A, E6B and E6D
(their docs' §7, E6B's §8). Most of the docs say "as read, perhaps
deliberate".

**Established:** a state that waits returns before its draw, so the thing it
shows is not drawn while it waits:

- E2G: sub-kind 0x20's `EffectKind18Sub20_WaitSet` `0x47E040` and
  `_WaitClear` `0x47E0B0` (sub-states 1 and 3): the strips are drawn only
  while they move. Whether something else draws them at rest was not read.
- E5B: the gates' `_Open` (sub-kind 0x0F's state 3, `EffectKind18Sub0F_Open`
  `0x500D20`): while a gate stands open its quad is not drawn at all.
- E5C: sub-kinds 0x11's and 0x12's state 1 draws its tiles only on the frame
  it moves on (the map cells stay 0x10 until the flag).
- E5E: `EffectKind18Sub25_WaitAway` `0x508670` (sub-state 3 of both lids): the
  open lid is not drawn while the leader leaves - E5F counts the same function
  as sub-kind 0x2A's state 3 (the far wait), where 0x27 and 0x29 draw through
  theirs.
- E5F: sub-kind 0x42's `EffectKind18Sub42_WaitFlag` `0x509740` and
  `_WaitFlagBack` `0x509980` return before the draw when their test fails.
- E5G: sub-kind 0x2B's waiting-open state, where 0x2C's draws.
- E6A: sub-kind 0x3E draws nothing in any state (it slides and sounds).
- E6B: sub-kinds 0x37's and 0x38's places draw nothing in their first frame;
  0x40's `EffectKind18Sub40_WaitOne` `0x50FA50` and 0x41's
  `EffectKind18Sub41_WaitOne` `0x50FD60` draw nothing until their cue; 0x41's
  `EffectKind18Sub41_Hold` `0x50FE20` stops drawing for good once `+9` reaches
  0x17 (the record lives on to `+0xA` 0x31).
- E6D: sub-kind 0x5C draws nothing while waiting shut or open.

**What ours does:** reproduces.

**Status:** latent, faithful; whether any of these is a visible gap in play
is for the owner's eye.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's, every bullet. A JP area-overlay twin was found for each
(each area carries only the sub-kinds it uses, with its own state table) and
each skips its draw while it waits: E2G AREA135#14 `0x801F3B28` / `0x801F3BF4`
(no `jal` to the draw `0x801F3CA8`), E5B AREA118 `0x801F3590` (leaf), E5C
AREA095 `0x801F2D3C` / AREA096 `0x801F2D04` (the tiles drawn only on the
frame it moves on), E5E/F AREA147 `0x801F2FB0` (leaf), E5F AREA140 `0x801F33FC`
/ `0x801F37C4` (return before the draw), E5G AREA171 `0x801F2F5C` (leaf), E6A
AREA157 `0x801F3484..0x801F3864` (nothing drawn in any of five states), E6B
AREA165 `0x801F345C` / `0x801F34C8`, AREA166 `0x801F345C`, AREA105 `0x801F2C94`,
AREA106 `0x801F2CB4` / `0x801F2DDC` (hold: `+9 == 0x17` stores 0, never drawn
again), E6D AREA173#14 `0x801F3C9C` / `0x801F3E4C`. Whether a gap shows is
still the owner's eye.

## D204 — A released record still written or drawn from (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E2G, E3C, E4A, E4B, E4F, E5D and E5F (their docs'
§7).

**Established:** `Effect_Release` clears bytes `+0..+4` only; these go on
after it:

- E2G: kind 0x5B's `EffectKind5B_Fade` `0x47EFF0` releases at `+0x5D` 0, then
  steps `+6` and draws the beam once more from the freed record.
- E3C: on the frame kind 0x6E's record is released its dispatcher still
  tail-jumps to the shard pass (`EffectKind6E_DrawShards` `0x4841E0`) from the
  freed record (the rest of kind 0x6E's last state is D205's).
- E4A: kind 0x84's three states call `Effect_Release` on `0x903849` = 0xFE and
  carry on - the line check, `Rand`, the writes of `+1` / `+9` land in the
  freed record, which the next `Effect_FindFree` may hand out with them set;
  kind 0x85's `EffectKind85_Wait` `0x488B70` frees the record at counter 0x35
  and still calls the tile and `EffectKind85_ShowObjects` `0x488B90` with the
  freed record's `+6` and colour.
- E4B: kind 0x88 draws a particle on the frame it dies (the draw follows the
  `+3` clear).
- E4F: `EffectKindB0_StepBars` `0x493370` draws a bar once more after putting
  it out of use ("probably intended (the last frame of the bar)").
- E5D: sub-kind 0x14's `EffectKind18Sub14_Start` `0x5059C0` steps `+2` to 1 on
  the record it has just released; its `EffectKind18Sub14_Fade` `0x505BB0`
  releases by a call and still draws the tiles with the freed record's `+9`.
- E5F: `EffectKind18Sub3C_Rise` `0x50AD30` draws after releasing its record.

**What ours does:** reproduces.

**Status:** latent, faithful; harmless unless another spawner takes the
record in the same frame, which no doc measured.

## D205 — Draws committed twice in one frame (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, groups E3C, E5B and E5C (their docs' §7).

**Established:**

- E3C: kind 0x6E's dispatcher `0x484050` calls the state and then always
  tail-jumps to the shard pass; state 3, `EffectKind6E_Fade` `0x484150`, runs
  the pass itself first (to learn whether any shard is live). So in state 3
  every live shard rises, grows, darkens and ages twice a frame and is drawn
  twice. "Possibly intended (a faster fade)."
- E5B: sub-kind 0x0E's `EffectKind18Sub0E_Raise` `0x5000B0` and the gates'
  `_Raise` (`EffectKind18Sub0F_Raise` `0x500E00`) call `_Wait`, which draws
  the gate, and when it did not re-trigger draw it again.
- E5C: `EffectKind18Sub10_SlideIn` `0x501DD0` calls state 1 (which draws the
  panel) and draws it again unless state 1 moved on.

**What ours does:** reproduces.

**Status:** latent, faithful; two identical quads that frame (twice the
blend where the quad is semi-transparent - not measured).

## D206 — `Effect_FindFree`'s "none" untested or stepped past (latent; ours aborts on the writes)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E1A, E1B, E1D, E3A, E3B and E5C. D54's, D163's and
D176's class in the effect pool; the two writes of their own are D218 and
D220, the arms' stale cell D234.

**Established:** `Effect_FindFree` answers 0xFF when all twenty records are
busy.

- E1B (§7): `EffectKind0F_ListOpen` `0x4672F0` (the call at `0x46736A`),
  `EffectKind0F_Child2Gauge` `0x467B10` (`0x467B8B`) and
  `EffectKind0F_DrawMessageList` `0x468560` (`0x468626`) write the answer's
  record without testing for 0xFF: with all 20 busy the original writes a
  kind-0x1A record at `0x7E91E0`, past the pool. "Ordinary play needs 20 live
  effects at that moment; not seen."
- E1A (§6): an answer past the twenty, wherever the band uses one.
- E3A (§7, §1's table): `EffectKind61_Capture` `0x4805B0` with no free record
  still steps on (`+1` up and nothing more), so the store reads back whatever
  VRAM `(0x340, 0x100)` held and the scatter makes particles of it.
- E3B: `EffectKind69_Spawn` (D218). E5C: `EffectKind18Sub17_WaitCue` (D220).
  E1D: `EffectKind22_SpawnArms` keeps a stale cell for a missing arm (D234).
- Not a defect, for comparison: E5C's `EffectKind18Sub57_Spawn` `0x5024C0`
  skips the spawn on none (`effect_5c.md` §7), and FC1's / E2F's
  `EffectKind4F_Start` gives the first sprite back and retries
  (`effect_2f.md` §7).

**What ours does:** aborts with a message where the original would write past
the pool (E1B's three, E1A); E3A's capture steps on as the original does.

**Status:** latent; reachable only with the pool full at that frame, which
no group measured.

## D207 — Divisions by a value that may be 0 in the effect engine (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E1A, E1F, E2A, E2B, E5C, E5D and E6C. D177's class;
EGT's `EffectGte_ProjectSize`, which most of the engine calls, has its own
entry (D216).

**Established:** an `idiv` with no test of the divisor:

- `Sprite_UpdateScreenScaled` `0x52CD50` divides by `+0x60 - 2 * s16 +0x3E`
  when `+0x60` is below 0, zero when the two meet; kind 5's last two states
  call it ([`effect_1a.md`](effect_1a.md) §6 item 7,
  [`effect_1f.md`](effect_1f.md) §6 L2: "whether any sprite reaches it is not
  measured").
- `EffectSpecks_Spawn` `0x471D10` tests `+0x2E` for 0, calls `Rand`, and reads
  the word again for the divide: harmless in the game (`Sprite_Current` does
  not change during the call); ours aborts where the two reads differ and the
  second is 0 ([`effect_2a.md`](effect_2a.md) §7).
- `EffectKind3B_DrawSpiral` `0x474410` divides by `Math_Cos(0x80)`, a constant
  the compiler did not fold; never 0 in play; ours also aborts on the one
  quotient `idiv` cannot hold ([`effect_2b.md`](effect_2b.md) §7).
- `EffectKind18Sub58_DrawColumn` `0x502670` by `8 - n`: its callers pass `+9`
  (0..7), 7, and `+9 >> 2` (2..7 while `_Fade` runs from 0x1F down to 8):
  "unreachable in ordinary play" ([`effect_5c.md`](effect_5c.md) §7).
- `EffectKind18Sub1C_Draw` `0x5052D0` and `EffectKind18Sub1D_Draw` `0x5057D0`
  by `8 - rise`: never 8 in play, but a record reaching sub-state 2 or 4 with
  `+9` 8 or 32..35 faults ([`effect_5d.md`](effect_5d.md) §7).
- Sub-kind 0x59's `DrawRing` (`EffectKind18Sub59_DrawRing` `0x512D30`): its
  glows divide by `Camera_Distance + 0x1194`, zero at a distance of -0x1194;
  "whether play reaches that distance is not measured; no recorded route
  shows sub-kind 0x59" ([`effect_6c.md`](effect_6c.md) §7).
- Not a defect: E5B's `_DrawGlow` tests the depth first.

**What ours does:** aborts with a message naming the function where the
original would fault (an integer divide by zero, no handler).

**Status:** latent; ours aborts where the original would end the process.

## D208 — Loops whose bound play does not check: a count past a byte, a walk with no end, a column past the wrap (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E1C, E1E, E2F, E3A, E3C and E4C (their docs' §7,
E1E's §10, E3A's §6). Kind 0x45's segment hang is D233's.

**Established:**

- E1C: a negative face count (`Sprite_ObjectsExtra[0]`'s `+0x54` byte 0x80 or
  above) is `movsx`'d in kind 0x1E's model draw and the loop runs
  `0xFF80..0xFFFF` times, reading far past the model. No reach is
  established; ours aborts.
- E1E L2: `LeaderPanel_S4Count` `0x529AE0` compares the byte `+9` with the
  picked record's word `+0x9C`: at 0x100 or more the step never ends;
  `LeaderPanel_S4Best` `0x5298F0` compares the word with a byte best and stores
  its low byte. What ours does here the doc does not say.
- E2F: `EffectKind51_DrawColumn` `0x47C870` - 65 angles and a byte row counter:
  a half-height above 0x20 reads past `EffectKind51_Angles`, one above 0x80
  never ends; the kind's states keep it within 0..0x20.
  `EffectKind56_Markers` `0x47D980`'s walk never ends when the ground `+0x14`
  lies in the lowest 2^24 of the signed range; ours aborts.
- E3A (§6): a frame of w or h above 0xFF in `EffectKind61_Scatter` `0x4807A0`
  never ends (the row and column are bytes); the two frames are 32 x 48 and
  64 x 64, neither reaches it; ours aborts.
- E3C, E4C: `EffectKind74_Draw` `0x485570` and its twin `EffectKind93_Draw`
  `0x48C550` step 0x100000 at a time from the foot to the top + 0x1000000
  with signed compares; the states keep the top within 0x8000000 of the
  foot (at most 0x90 steps), but a foot near the signed limit would run the
  loop past the wrap (up to 4,096 steps of 0x48 bytes).

**What ours does:** aborts where the docs say so (E1C, E2F's walk, E3A);
reproduces the columns.

**Status:** latent; none is reached by the values the kinds' own states
write, as the docs read them.

## D209 — Quads laid out past a packet cursor that did not move (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, groups E4E, E5C and E6C (their docs' §7).

**Established:** a draw that builds several primitives at fixed offsets from
the cursor as it stood before them, without reading it again: when
`Gfx_CommitPrim`'s room test fails (the pool nearly full; the commit stops
0x54 short of the end and does not move the cursor) the later ones are
written past the pool.

- `EffectKind9C_DrawBeamSides` `0x48EBB0` copies its first quad (`rep movsd`)
  to the first quad's address + 0x44 regardless: up to 0x34 bytes past the
  pool ([`effect_4e.md`](effect_4e.md) §7).
- `EffectKind18Sub15_Draw` `0x502E60` builds its four `POLY_G4`s 0x44 apart:
  up to 0x11C bytes past a cursor within 0x54 of the end, about 0xC8 past the
  pool ([`effect_5c.md`](effect_5c.md) §7).
- The sky, `EffectKind18Sub44_Draw` `0x510EB0`: its four G4 bands at the
  cursor + `0x44 m`, up to 0x110 bytes past the pool's room margin, unlinked
  ([`effect_6c.md`](effect_6c.md) §1.2, §7).
- Kind 0xA0's trail overwrites the last packet the same way (D222).

**What ours does:** writes the same bytes (E4E and E6C say so; E5C's doc does
not say).

**Status:** latent, faithful; harmless unless the packet pool is that full.

## D210 — `Sprite_Current` or `Field_State` left on a member or the leader (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E2G, E3D, E4B and E5D (their docs' §7).

**Established:**

- E2G: kind 0x15's `EffectKind15_WaitPose4` `0x47E370` and `_WaitPose8`
  `0x47E5B0` point `Field_State` and `Sprite_Current` at `ObjTrio` record 1
  before testing its animation and return without putting `Sprite_Current`
  back while the animation has not run through; `Field_State` is left on the
  member on every path of the kind.
- E3D: kind 0x82's `EffectKind82_Wait` `0x487C50` and every check leave
  `Field_State` on the leader.
- E4B: kind 0x8B's `EffectKind8B_Pose` `0x48ABC0` makes the leader
  `Sprite_Current` before its test and puts the record back only when the test
  passes.
- E5D: `EffectKind18Sub1D_Hurt` `0x505690` leaves `Field_State` on the party
  member it pointed it at (`Sprite_Current` is put back).

`Effect_RunObjects` sets `Sprite_Current` afresh for each record, so the next
effect is not affected; whatever runs after the last record of the frame sees
the member or the leader (E2G, E4B).

**What ours does:** the same (E2G; E4B's control 77 is refused by it).

**Status:** latent, faithful; what reads `Sprite_Current` or `Field_State`
after the effect pass was not traced.

## D211 — Writes into the area's map with no bound against the map, and corner bytes that accumulate (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, groups E3D, E5A, E5B, E5C, E5D, E6B and E6D (their docs' §7,
E6B's §8).

**Established:**

- E3D: `EffectKind7D_SetMap` `0x486D60` rewrites a dword of the area block by
  `AreaMap_Header`'s arithmetic over a cell word the map file supplies - right
  for area 170's map, anywhere for another (DIV-0062's site).
- E5A: `EffectKind18_06_SetMap` `0x4FE040` writes height bytes at
  `AreaMap_Header + 4 * AreaMap_HeightBase` with no bound on the base word -
  in play the area's own rows, "not a defect by itself".
- E5B: the gates write `AreaMap_Bytes + z * width + x` (and the neighbour)
  from the record's words; sub-kind 0x0E writes the fixed cells (1, 21) and
  (1, 22) whatever the map's size.
- E5C: sub-kind 0x15 rewrites rows `r - 17 .. r + 13` of columns 0x2D..0x30 of
  the corner heights round `Field_Kind2Z`'s row every frame, with no test
  against the map's height.
- E5D: `EffectKind18Sub22_SetMap` `0x506640` writes `AreaMap_Bytes`,
  `AreaMap_Corners` and the height bytes by its rectangles' cells and the
  area's width, unchecked, "as the game's other map writers are".
- E6B: sub-kind 0x3D (`EffectKind18Sub3D_Ripple` `0x50F820`) adds to the map's
  corner bytes every frame `Cond_ByteFE` is set (byte adds that wrap): the
  corners drift by the running sum, which the sine differences keep
  oscillating rather than growing.
- E6D: sub-kind 0x62 (`EffectKind18Sub62_Ripple` `0x516090`) adds the
  difference of two sine steps to the corner bytes each frame, wrapping at
  256, and never restores them.

**What ours does:** reproduces (no bound either).

**Status:** latent, faithful; right for the areas that spawn these, as far
as the docs read; a spawn near a map's edge or in another area writes outside
its grid.

## D212 — The waiting states' absolute value leaves `0x80000000` negative (latent, unreachable)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, groups E5G, E6A, E6B and E6D (their docs' §7, E6B's §8).

**Established:** the near / far tests of kind 0x18's panel sub-kinds take
`|d|` as `cdq; xor; sub`, which leaves `0x80000000` negative, so a leader
exactly 0x8000 cells away counts as near. "Unreachable on a map (cells are
bytes)."

**What ours does:** computes it the same way.

**Status:** latent, unreachable.

## D213 — Harmless leftovers: words handed on unwritten, stale register halves, argument slots reused, scratch written to cells others own (latent, harmless)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E1A, E2A, E2B, E2E, E2F, E3C, E4B, E4C, E4D, E4E,
E5A, E5C, E5E, E5G, E6B, E6C and E6D. D101's kind.

**Established:** none reaches a result the game reads, as each doc read it:

- Half-written sizes handed to `EffectGte_ProjectSize`, whose second quotient
  no caller reads: `EffectSparks_Draw` `0x472240` (E2A), `EffectKind3B_DrawGlow`
  `0x4747D0` and `EffectKind3D_DrawFan` `0x474AC0` (E2B; ours writes 0),
  `EffectKind4A_DrawGlow` `0x47AF10` (its own argument slot as the size pair,
  E2E), `EffectKind50_SpeckQuad` `0x47C4A0` (E2F), `EffectKind97_DrawSpark`
  `0x48D6F0` (E4D: ours hands the first word in both, which draws the same;
  "no `DIVERGENCE.md` entry is needed for a value nobody reads").
- Vertices whose fourth word, and flag pointers, are never written:
  `EffectKind53_Beam` `0x47D650` (E2F), `EffectKind9D_Project` `0x48A2A0`
  (E4B: also a fourth argument the port's `Gte_RotTransPers` does not take),
  E5A's `_DrawPanel` callers, `EffectKind18_08_DrawTilted` `0x4FEC20` and
  `EffectKind18_0A_DrawDoor` `0x4FF150` (DIV-0023's pad; no GTE function of
  ours reads it).
- Stale upper bytes: E1A's coordinates pushed with `Sprite_Current`'s or a
  callee's high half for `0x469750`, `0x468AC0`, `0x469210`, `0x468C50`,
  `0x468F00`, `FieldPanel_*` and `Text_DrawAt`, and kind 0xC's `flip` a stale
  stack word (§6 item 4; E1B §2 reads the callees: they take the low bits);
  `EffectKind52_TrailDraw` `0x47D220`'s shade dword (E2F);
  `EffectKind18_0A_DrawDoor`'s dy (`mov dl` over `Prim_SetTexture`'s `edx`,
  E5A).
- Argument slots reused as locals or flag outs: `EffectKind18_04_DrawPanel`
  `0x4FD6A0` and `EffectKind18_08_DrawTilted` (E5A), `EffectKind18Sub50_DrawWall`
  `0x501850` (E5C), kind 0x8E's `_DrawFan` (E4C), `EffectKind18Sub40_Draw`
  `0x50FAE0` (E6B), E3C's `EffectKind6F_PushParty` `0x4849A0` (its whole slot to
  `Member_SetState2_8`; the upper bytes are 0).
- Answers and writes nothing reads: E3C's `_AlongZ` / `_AlongX` call `BareRet`
  with the line's points pushed, and kind 0x6D's start clears the block it then
  sets up in full; kind 0x8E's `_Fall` ignores `MoveCmd_TestFB`'s answer (E4C);
  `EffectKindA0_StepSparks` `0x4906A0`'s answer is read by no caller and kind
  0xA0's state 8 lowers a glow size no state draws (E4E); E5G's
  `EffectKind18Sub2C_Draw` `0x50B8B0` writes the four corners' heights before
  the ground overwrites them.
- Scratch written to cells another subsystem owns: `EffectKind18Sub24_DrawTrail`
  `0x5073D0` hands `Gte_RotTransPers3` the global `MapView_ScreenXY` as its
  third out (E5E); sub-kind 0x45's ring (`EffectKind18Sub45_DrawRing`
  `0x512040`) overwrites `MapView_ScreenXY` and `0x903828` with its fixed point
  every call, and `EffectKind18Sub44_DrawStars` `0x511740` stores each star's y
  into the cursor's primitive before testing it (E6C); sub-kind 0x5C's draw
  (`EffectKind18Sub5C_DrawPanel` `0x5144F0`) stores its two base floats into
  `MapView_ScreenXY`, which `MapView_Build` owns - harmless if the map view
  rewrites it before reading, "not measured" (E6D).

**What ours does:** the same, except where noted (E2B's 0, E4D's first word,
the pads ours does not read).

**Status:** latent, harmless by reading.

## D214 — Bytes read before anything wrote them: fields a kind never sets, pools drawn before they are set up (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, groups E1D, E2D, E3A, E3C, E4D and E4E (their docs' §7).

**Established:**

- E1D: kinds 0x21 (arms), 0x24 (rays) and 0x26 (bands) are given `+0` and `+5`
  but not `+1`, so they start at state 0 only because `Effect_Release` cleared
  `+1` (FC2's finding for kind 0x34, D196).
- E2D: kind 0x45's state 0 leaves the offset `+0x36` unset; without flag 0x25
  the samples are computed with whatever the record held.
- E3A: the particles' `+0xC` is never written: `EffectKind61_Scatter`
  `0x4807A0` writes `+2`, `+4`, `+8`, and `EffectKind61_Twinkle` `0x480A50`
  copies `+4..+0xF` into the `TILE_1`, so the tile's depth is an earlier
  effect's bytes; `EffectKind64_DrawShards` `0x482360` draws all sixteen shards
  with no in-use test (state 3 has always started them, by the table's
  order).
- E3C: kind 0x6F's `EffectKind6F_AlongZ` `0x484670` (and `_AlongX` `0x4847D0`)
  reads the elevation's coordinate before writing it, so the first frame's
  ground comes from what the spawn left there.
- E4D: the debris pass draws all 32 every frame whether set up or not
  (`EffectKind97_Fade` `0x48D220` draws them after `_Burst`; nothing clears the
  pool).
- E4E: kind 0xA0's shards 16..31 are drawn from state 4 on, before state 6's
  end sets them up (state 3 zeroes only their speed and shade words): black
  triangles collapsed onto a stale point, "not visible if the renderer drops
  degenerate triangles".

**What ours does:** reads and copies the same bytes.

**Status:** latent, faithful; what shows depends on what an earlier effect
left in the record or the pool.

## D215 — `EffectGte_SetDiagonalOne` writes 1 on the diagonal where the PlayStation twin writes 0x1000 (the port's; seen in play as D243 and restored by DIV-0126, 2026-10-14)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group EGT ([`effect_gte.md`](effect_gte.md) §5; the round doc's
stage-A debt, [`takeover-queue-round13.md`](takeover-queue-round13.md)
section 11).

**Established:** `EffectGte_SetDiagonalOne` `0x494180` stores 1 on the
matrix's diagonal. Its PlayStation twin `0x801B0F5C` begins `addiu v0, zero,
0x1000` and stores that (read in the sibling's generated overlay code,
`overlays_static_0170.c`) - the identity in the GTE's 4.12 fixed point. The
PC's 1 is 1/4096 of it, and its seven callers (E1C's `0x46EA80`, Capcom's
`0x46F570`, E2E's `0x47B070`, E3A's `0x482240`, E3C's `0x4851E0`, E4D's
`0x48D860`, E4E's `0x4906F0`) premultiply it by `Gte_RotMatrixX` / `Y` / `Z`
(`MulMatrix0`, each element `>> 12`), so the matrix they build collapses to
elements of 0, 1 and -1: whatever those effects draw with it would be
squashed onto their translation. What it was on the PlayStation (0x1000) and
what the PC does (1) stay separately answerable here; what the game should
do is the owner's.

**What ours does:** reproduces the 1s (control C22 refuses 0x1000).

**Reach, as the doc says it:** no recorded route reaches any of the seven
callers (`effect_gte.md` §9); what the effects look like on the PC is for
the owner's eye, and a fix would be a divergence.

**Status:** a port defect against the PlayStation, **seen in play
2026-10-14** as D243 (the lighthouse's rays, kind 0x4B, drawn as points) and
**restored by DIV-0126** ([`lighthouse-rays.md`](lighthouse-rays.md)): ours
writes 0x1000 once the `DiagonalIdentity` switch is armed, after the fuzz,
which still compares Capcom's 1s; `BOF3X_ORIGINAL=DiagonalIdentity` gives the
1s back. The six other callers' effects (kinds 0x1E, 0x1F, 0x21, 0x64, 0x72,
0x97, 0xA0) turn by a true rotation now and are not seen in play: their look
is the owner's when a route reaches one.

## D216 — `EffectGte_ProjectSize` divides by an unchecked depth and keeps 16 bits of the quotient (latent; ours aborts at depth 0)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group EGT ([`effect_gte.md`](effect_gte.md) §5); the round doc's
stage-A debt (section 11).

**Established:** `EffectGte_ProjectSize` `0x4941E0` divides `size * 1000` by
the camera-space depth with no test: a point at depth exactly 0 faults
(`idiv`, no handler, the process ends). It keeps the quotient's low 16 bits:
near the eye the quotient passes 0x7FFF and wraps (size 0x40 at depth 1:
64,000, stored as -1,536). The doc's "ranges, not defects" (the focus words
and the point's x and z wrapping after their shifts, the height's fraction
dropped) are reproduced and not counted here. Most of the engine calls it:
E2E ([`effect_2e.md`](effect_2e.md) §7, `EffectKind4A_DrawGlow` at the trail's
head) and E4E ([`effect_4e.md`](effect_4e.md) §7, `EffectKindA0_DrawGlow`
`0x4901D0`) point at this entry for their depth; the half-written sizes
handed to it are D213's. FC2 met the divide first (D177).

**What ours does:** aborts with a `Fatal` naming the point at depth 0;
reproduces the 16-bit wrap (control C33 refuses a saturating version). A
fuzz that calls it for real keeps the point off the eye's plane (the round
doc, section 11).

**Reach, as the doc says it:** no recorded route enters `0x4941E0`; whether
play ever puts a point on the eye's plane is not established.

**Status:** latent; ours aborts where the original would end the process,
and wraps as it does.

## D217 — Kinds 0x61 and 0x6B make a particle of every opaque pixel, with no bound: past 2,048 (0x61) or 1,881 (0x6B) they overwrite `Sprite_Current` (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29 (E2C, [`effect_2c.md`](effect_2c.md) §7) and 2026-10-03 (E3A,
[`effect_3a.md`](effect_3a.md) §7, which names E2C's kind as the same shape).

**Established:** both read a member's sprite back from VRAM and turn each
pixel that is not 0 into a particle of 0x14:

- **Kind 0x61**, `EffectKind61_Scatter` `0x4807A0`: particles at `0x92DF80 +
  0x14 n`, n up to w x h. The second frame (the member's `+0x148` not 0) is
  64 x 64, up to 4,096 particles (to `0x941F80`); the 2,049th lands on
  `0x937F80` - its `+4` float on `Gfx_CurrentEnv` `0x937F84`, its `+8` on
  `Sprite_Current` `0x937F88`, then `Frame_Counter` - and the next state's
  `Sprite_Current` access goes astray. The first frame (32 x 48, 1,536)
  cannot reach it.
- **Kind 0x6B**, `EffectKind6B_Scatter` `0x475F20`: a 0x50 x 0x48 rectangle
  (5,760 pixels), particles at `0x92EC80 + 0x14 n`; the 1,882nd lands on
  `0x937F84` (`Gfx_CurrentEnv`, then `Sprite_Current`, `Frame_Counter`,
  `MapView_CellItems`, the desktop cells at `0x939A2C..`): more than 1,881
  opaque pixels overwrite `Sprite_Current`'s bytes 1..3 with a count and a
  colour. What the PlayStation reserved there is not read; on the PC the
  unnamed room before `0x937F84` holds 1,881.

**What ours does:** writes as the original (E3A says so; E2C's doc does not
say otherwise).

**Reach, as the docs say it:** E3A - whether a member's captured sprite has
more than 2,048 opaque pixels in its 64 x 64 is not measured; E2C - chapter
10's run 13 captures sprite record 2, its pixel count is the sprite's, not
measured.

**Status:** latent, faithful; a crash candidate if a captured sprite is
opaque enough.

## D218 — `EffectKind69_Spawn` writes nine records without testing `Effect_FindFree` for none (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E3B ([`effect_3b.md`](effect_3b.md) §7 item 1). D206's
class, an entry of its own as its own spawner.

**Established:** `EffectKind69_Spawn` `0x482AB0` takes nine records from
`Effect_FindFree` and never tests the answer: with fewer than nine free
records the original takes 0xFF as a record and writes kind 0x69's bytes at
`Effect_Objects + 0xFF * 0x80` = `0x7E9160`, outside the pool (whatever lies
there).

**What ours does:** aborts with a message.

**Reach, as the doc says it:** depends on how many effects are live when
chapter 10's scene spawns the kind (`Scena10_Run2` spawns several kinds
around it); not traced.

**Status:** latent; ours aborts where the original would write past the
pool.

## D219 — `EffectKind69_Parent` is one cell for every kind-0x69 spawner (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E3B ([`effect_3b.md`](effect_3b.md) §7 item 2).

**Established:** every part reads its parent through `EffectKind69_Parent`
`0x676268`. A second kind-0x69 spawner while the first's parts live retargets
all of them: the first never counts to nine (it waits for ever) and the
second's `+0xB` is raised by both sets - a test for exactly 9 that the count
can step past (four parts end in the same frame).

**What ours does:** keeps the one cell, as the original.

**Reach:** not stated in the doc beyond the shape (two spawners at once).

**Status:** latent, faithful.

## D220 — Sub-kind 0x17's spawn writes a record it did not find (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E5C ([`effect_5c.md`](effect_5c.md) §7). D206's class, an
entry of its own.

**Established:** `EffectKind18Sub17_WaitCue` `0x5036D0` takes
`Effect_FindFree`'s answer `& 0xFF` and writes `+0`, `+5`, `+1`, `+2` of that
record untested: with all twenty records live (0xFF) the four bytes land
0x7F80 past `Effect_Objects`, at `0x7E9160`, `0x7E9165`, `0x7E9161`,
`0x7E9162` - inside display buffer 0's packet pool (`0x7E1C00..0x7F1BFF`).
The game's nearest answer would be to skip the spawn, as
`EffectKind18Sub57_Spawn` does.

**What ours does:** aborts there.

**Reach, as the doc says it:** only if the effect pool is full on the frame
the counter `0x903848` becomes 0xC; not measured.

**Status:** latent; ours aborts where the original would write into a
packet pool.

## D221 — Kind 0x90's tag `0x676294` is never reset: the 256th record takes tag 0 and owns every free shard (latent, faithful; the owner's eye)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E4C ([`effect_4c.md`](effect_4c.md) §7); listed for the
owner's eye in the round doc's wave-four debts
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 15).

**Established:** `EffectKind90_Start` `0x48BF20` hands each kind-0x90 record
the next value of the dword `0x676294` as its tag (`+6`, the low byte), and
`EffectKind90_MoveShards` `0x48C260` / `EffectKind90_EmitOne` `0x48C1C0` treat
a shard whose `+0` equals the tag as the record's. Nothing resets the dword
(a scan of the image: only `_Start` names it), so the 256th kind-0x90 record
of a session gets tag 0 - and every **free** shard (`+0` 0) is then its own:
it moves and draws all of them (stale positions, whatever the pool last
held), and its own emits are written with `+0` 0, free records the next emit
may take. The pool is also cleared only before the first record of the
session (the dword is 0 only then). The doc's nearest sensible value: skip
tag 0, and clear the pool when no kind-0x90 record is live; "the fix and
its ledger entry are the owner's".

**What ours does:** as the original (no divergence was entered for it).

**Reach, as the doc says it:** needs 256 kind-0x90 records in one session
(area 134's handlers 12 and 13 spawn one each); not measured.

**Status:** latent, faithful; a candidate for the owner.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** The code yes, the reach no. JP AREA134.EMI#13 `0x801F4214` (Start),
`0x801F48B0`, `0x801F4938`, `0x801F4974`, `0x801F49A0` match ours to the
instruction: the tag only increments, the pool clears only at tag 0, a free
shard's 0 matches tag 0. But the PlayStation's tag dword `0x801F5818` sits in
area 134's own overlay (0 in the file), so it restarts on each load of the
area; the PC's `0x676294` is a static of the one image and lives for the
session. The 256-record wrap needs 256 kind-0x90 records within one load on
the PlayStation and across the session on the PC: the session-long count is
the port's ([`psx-overlay-reload-resets-state-pc-keeps`]).

## D222 — Kind 0xA0's trail dots have no count: with the leader west of x 0x2F8000 the first frame of state 6 draws until x wraps (latent; ours aborts past 0x400 dots; the owner's eye)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E4E ([`effect_4e.md`](effect_4e.md) §6, §7); listed for
the owner's eye in the round doc's wave-four debts (section 15).

**Established:** `EffectKindA0_DrawTrail` `0x490390`'s dots start at `to +
(Frame_Counter & 0xF) * pull` and step `pull << 4` while x is above `from`'s;
nothing counts them (kind 0x64's same trail stops at eight). State 4 sets the
pull to `(0x2F8000 - x) sar 7` from the glow's point, the leader's. With the
leader **west of x 0x2F8000** (the pull positive), the first frame of state 6
draws with `from` and `to` the same point: with `Frame_Counter & 0xF` not 0
the first dot is east of `from` and every step moves it further east, so the
loop runs until x passes 0x7FFFFFFF - far past `Gfx_CommitPrim`'s pool, which
stops advancing 0x54 short of its end, so the dots overwrite the last packet
until the loop ends (D209's shape). From the second frame the head is held at
0x2F8000 and the loop ends at once; with the leader east of it the dots run
back to the head and stop; a pull of 0 draws none. The doc's nearest
sensible answer would be E3A's count of eight; "the fix and its ledger entry
are the owner's word".

**What ours does:** aborts past 0x400 dots (§6).

**Reach, as the doc says it:** "ordinary play draws about nine" (§6); where
chapter 15's run 4 has the leader when it spawns kind 0xA0 is not measured
(§7).

**Status:** latent; ours aborts where the original would draw on for a
frame; a candidate for the owner.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP SCE15EF1.EMI#0 `0x801D1678` loops the dots on
`from.x < q.x` with no count, from `to + (Frame_Counter & 0xF) * pull` by
`pull << 4`; state 4 (`0x801D0F10`) sets the pull `(0x2F8000 - x) sra 7` as
ours. The PlayStation's packet commit (`0x8014E494`) refuses to advance past
its pool's end, so a long run re-writes its last packet; ours aborts past
0x400 dots where the PlayStation draws on.

## D223 — `EffectKindAD_DrawArc`'s second vertex depth is a y (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E4F ([`effect_4f.md`](effect_4f.md) §7).

**Established:** in `EffectKindAD_DrawArc` `0x492DC0` each `POLY_G4`'s `+0x20`
(vertex 1's depth) is loaded from b's projection's `+4`, its y (`mov eax, [esp
+ 0x2c]` / `mov ecx, [esp + 0x2c]`, the same cell twice), where vertex 3's
(`+0x40`) is its depth `+8`. The nearest sensible value is the depth.

**What ours does:** copies the y as the original (control 8 plants the depth
and is refused).

**Reach, as the doc says it:** what the renderer does with the depth of a
semi-transparent quad (its sort, its perspective) decides whether it shows;
nothing measured it.

**Status:** latent, faithful.

## D224 — `EffectKind18Sub31_Draw`'s compare that never sets the bit 0x30's draw sets (latent, faithful; the owner's question)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E6A ([`effect_6a.md`](effect_6a.md) §7); the round doc's
wave-six debts (section 17).

**Established:** `EffectKind18Sub31_Draw` `0x50DC20`'s texture word ORs in
`(+8 == 0) < 0x10` (`sete cl; cmp ecx, 0x10; setl dl`), always 1, into a word
whose bit 0 is already set - so the word is `+8 << 21 | 0x150010F` whatever
`+8` is. Its siblings put `(+8 == 0)` at bit 16 (`EffectKind18Sub30_Draw`
`0x50D680`) or `+8` at bits 16 and 21 (`EffectKind18Sub2E_Draw` `0x50CD90`);
the source perhaps meant `<< 0x10` (a shift typed as a compare). For `+8` set
the draw is the same either way; for `+8` clear 0x30's draw sets bit 16 and
0x31's does not.

**What ours does:** kept as read.

**Status:** latent, faithful; whether 0x31 was meant to look like 0x30 is the
owner's question, "not a defect ours can settle".

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. The ground-quad sub-kinds each live in one JP area overlay
with their own draw (the texture word `0x150010F` in code only in AREA158 and
AREA160..165): AREA162's draw, 0x31's by elimination, is `+8 << 21 |
0x150010F` with no bit 16 (`0x801F33C4..E8`), where AREA161's 0x30 ORs in
`0x10000` when `+8` is clear. GCC folded the same always-true compare; the
difference from 0x30 is in Capcom's build. Whether 0x31 was meant to match
0x30 stays the owner's question.

## D225 — `EffectKind18Sub5C_WaitFar` can step `+2` past its table (latent; ours aborts; the owner's word for a fix)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E6D ([`effect_6d.md`](effect_6d.md) §7); the round doc's
wave-six debts (section 17). D200's shape, reached by the original's own
code (as D179 was).

**Established:** `EffectKind18Sub5C_WaitFar` `0x514410` steps `+2` once when
`Cond_ByteFE` is 1 and once more when the leader is away: both in one frame
take it from 3 to 5, and the next frame's dispatch (`EffectKind18Sub5C_Run`
`0x514270`) jumps through `0x65F388` - the variant cells read as a code
pointer. The game's nearest answer would be 4 (`EffectKind18Sub5C_Close`).

**What ours does:** steps it the same and aborts in the dispatcher.

**Reach, as the doc says it:** only if the area's script sets `Cond_ByteFE`
to 1 while the leader is more than three cells from the open panels; not
measured.

**Status:** latent; ours aborts where the original would jump through data;
the fix is the owner's word.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes - Capcom's. JP AREA173.EMI#14 `0x801F3E4C` steps `+2` once for
Cond_ByteFE 1 and again for the leader away, 3 to 5 in one frame, and the
dispatch `0x801F3B94` jumps through the five-entry table `0x801F52F0` with no
bound: entry 5 is data (`0x46116030`) that the PlayStation `jalr`s to; ours
aborts there.

## D226 — Sub-kind 0x59's twelve shade bytes in `.data` are never restored: a second showing in one session starts faded (latent, PC only; the owner: probably fine)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E6C ([`effect_6c.md`](effect_6c.md) §7); traced to its area
by the coordinator ([`takeover-queue-round13.md`](takeover-queue-round13.md)
section 17; [`USER_CHECKS.md`](USER_CHECKS.md) item 8).

**Established:** `EffectKind18Sub59_Fade` `0x512C30` writes the twelve bytes
`0x65EF0C..0x65EF17` of the image (initially 0x80 each) down to 0, and
nothing in the executable writes them back (a scan of `.text` for the
addresses finds only the band's reads and the fade's writes). On the
PlayStation the twin's table lived in an AREA overlay, reloaded with the
area; on the PC it is resident, so a second showing of sub-kind 0x59 in one
session starts faded: `_Fade` sees `0x65EF14` already 0 and moves straight
on, the ring drawn dark. Where it runs (section 17): effect records of kind
0x18 are placed by the event script's op `E0`; the one op with kind 0x18 and
sub-kind 0x59 in the executable's data is at `0x648F97`, inside the script
that area 196's descriptor `0x6490A8` names first, and the PlayStation twin
of the ring draw is in area 196's overlay alone. Kind 0x87's
`EffectKind87_Setup` also rewrites `.data` (rows 8..13 of `0x654DA0`), but
idempotently ([`effect_4b.md`](effect_4b.md) §7; D238).

**What ours does:** as the original (no ledger entry from the group).

**Reach, the owner's word** ([`USER_CHECKS.md`](USER_CHECKS.md) item 8,
2026-10-03): area 196 is a one-time scene - **"probably fine, to be verified
in game"**.

**Status:** latent, PC only (the PlayStation reloaded the bytes); open until
the owner's check in game. The wave's lesson stands beside it: a function
that writes its own `.data` table in a kind that came from an AREA overlay is
a defect of the port to look for (section 17).

## D227 — `EffectKind64_DrawGlow` reads its rim vertices' depth from a stack word it never writes (the original's; ours differs by DIV-0068)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E3A ([`effect_3a.md`](effect_3a.md) §2, §7); entered in
the ledger with the wave (round doc section 14).

**Established (what the original does):** `EffectKind64_DrawGlow` `0x481740`
draws a fan of 32 semi-transparent `POLY_G3`; the rim vertices' x and y are
computed into stack locals, but their depth is read from `esp + 0x28`
(`0x481836`, `0x48189D`), a dword the function never writes - whatever the
stack held, which the renderer divides by.

**What ours does:** differs by [DIV-0068](DIVERGENCE.md): ours writes the
centre's depth to the rim vertices, as the sibling discs
`EffectKind64_DrawSpark` and `EffectKind68_DrawMote` write to all three.
`BOF3X_ORIGINAL=EffectKind64_DrawGlow` runs Capcom's, its stale depth
included.

**Reach:** not seen live; no recorded route reaches kind 0x64 (DIV-0068).

**Status:** the original's defect; replaced in ours by DIV-0068 (the owner
kept it as written, 2026-10-03).

## D228 — `EffectKind18Sub4B_Run` builds its far corners' x from a stack word it never writes (the original's; ours differs by DIV-0072)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E5F ([`effect_5f.md`](effect_5f.md) §7); entered in the
ledger by the coordinator (round doc section 16).

**Established (what the original does):** in `EffectKind18Sub4B_Run`
`0x50A510`'s quad loop x0 is kept in `ebp`; the near corners add the quad's
offset (stored over x0's home `[esp + 0x24]`), then `ebp` is reloaded from
`[esp + 0x20]`, a local no instruction of the function writes, and the far
corners' x (v1, v3) are built from it; from the second quad on all four
corners' x come from it. The original's far corners and its second and third
quads sit wherever the caller's stale stack puts them.

**What ours does:** differs by [DIV-0072](DIVERGENCE.md): ours uses x0 for
that word (each quad symmetric about its column).
`BOF3X_ORIGINAL=EffectKind18Sub4B_Run` runs Capcom's.

**Reach:** not seen live; no recorded route reaches sub-kinds 0x4B / 0x4C
(DIV-0072).

**Status:** the original's defect; replaced in ours by DIV-0072 (kept as
written, 2026-10-03).

## D229 — Kinds 0xE and 0x16 jump to address 0 in area 104, and read other kinds' states off a world map (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E1A ([`effect_1a.md`](effect_1a.md) §6 item 1). D73's
reading for kinds 0 / 0x58 / 0x18, extended.

**Established:** `EffectKind0E_WorldMap` `0x462B00` and
`EffectKind16_WorldMap` `0x462B20` jump through the world-map record
`WorldMap_RecordIndex` `0x462A90` picks. Record 6 (area 104) of
`WorldMap_Records` holds 0 at `+4` and `+8`: an effect of kind 0xE or 0x16
there jumps to address 0. Off a world map the index is 11, which reads
`EffectKind01_States[1]` (`EffectKind01_Draw`) and `EffectKind07_States[0]`
(`EffectKind07_Start`).

**What ours does:** aborts with a message on a handler of 0 and on an index
past 11 (§6).

**Reach, as the doc says it:** which scenes spawn kinds 0xE / 0x16 is not
read.

**Status:** latent; ours aborts where the original would crash or run
another kind's state.

## D230 — Kind 0xF's rows 7 and 8 name texts 13 and 14, past the table's thirteen (latent; ours aborts)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E1A ([`effect_1a.md`](effect_1a.md) §6 item 2).

**Established:** a record of kind 0xF with `+6` 7 or 8 reads its "text" past
`0x653B98`'s thirteen records - from the pen bytes and the row table as a
pointer (`EffectKind0F_Choose` `0x466120`, `_Title` `0x4661B0`, `_LineStart`
`0x4662B0`). The rows exist, so either the spawner never writes 7 or 8 or the
game reads garbage there.

**What ours does:** aborts on a text past thirteen (§6).

**Reach, as the doc says it:** which `+6` the spawner writes is not read.

**Status:** latent; ours aborts where the original would read a stray
pointer.

## D231 — Kind 0x1E's back-face test reads the halves of a float (latent, PC only; the owner's eye)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E1C ([`effect_1c.md`](effect_1c.md) §7).

**Established:** `EffectKind1E_Winding` `0x46EE20`, called by
`EffectKind1E_DrawModel` `0x46EC20`, is the PlayStation's screen-space
winding test on s16 `(x, y)` pairs, as the PSX GTE wrote them. On the PC
`Gte_RotTransPers4` writes each screen point as two floats (x at `+8`, y at
`+0xC`), so the pairs it is handed are the low and high 16 bits of each
vertex's x float: their sign is noise in the mantissa, and which of the
model's outlines are drawn does not follow their facing. The PC has the
float version beside it - `0x4941B0` (the same cross product on float
vertices, three other callers) - which this caller does not use.

**What ours does:** reproduces (a fix would be a divergence).

**Reach:** not stated in the doc; what the model looks like on the PC is for
the owner's eye.

**Status:** latent, PC only by reading (the PlayStation's pairs were s16).

## D232 — `EffectKind44_SparksClear` clears one spark of eight (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E2C ([`effect_2c.md`](effect_2c.md) §7, §9).

**Established:** `EffectKind44_SparksClear` `0x476D00` sets the cursor
`0x6761B8` to the first spark record and clears the in-use byte the cursor
points at eight times, never moving it. Sparks 1..7 keep whatever their bytes
held: the records lie in the shared buffer from `0x92BF80` (D201: kind 0x43's
spark records, kind 0x6B's read-back pixels, kind 0x40's shards and disc), so
a kind-0x44 ring started after one of those kinds ran may step and draw up to
seven stale "sparks" - their life byte up to 0xFF frames, their speed and
colour whatever lay there. Kind 0x44's first state and its fade-in both call
it.

**What ours does:** kept as read.

**Reach, as the doc says it:** area 78 / 80's choice (§9); whether stale
bytes are left there in play depends on what ran before - not measured.

**Status:** latent, faithful.

## D233 — Kind 0x45's trace reads 72 points from a 36-point history, and a stale point can hang its segment draw (latent; ours aborts at the hang)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E2D ([`effect_2d.md`](effect_2d.md) §7).

**Established:** `EffectKind45_PushSample` `0x4777A0` keeps 36 samples (it
moves 35 dwords), while `EffectKind45_DrawTrace` `0x477820` hands
`EffectKind45_DrawPoints` `0x4778A0` a count of 0x48, so the 36 dwords after
the history (`0x92C010..0x92C09F`, inside `EffectKind30_Shards`' block, where
kind 0x49's glow record `0x92C060` lies - D201) are read as points and
plotted, or joined to a neighbour, when their x falls in the panel's clip
columns; `EffectKind45_Start` `0x4771D0` clears 32 samples of the 36.
`EffectKind45_DrawSegment` `0x477940` counts its steps in a byte against a
16-bit distance: past 0xFF on the stepping axis it never returns. The trace's
own samples stay close (the amplitude is capped at 0x18), but a segment from
one of the stale points to a sample can be longer.

**What ours does:** reads the same points; aborts with a message at a
segment that would never end.

**Reach:** not stated in the doc (kind 0x45 is spawned by `Scena06_Run13`,
its `symbols.toml` row; fuzz only).

**Status:** latent; ours aborts where the original would hang.

## D234 — Kind 0x22 waits on arms through cells the spawn did not write, and on a "done" byte that outlives them (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E1D ([`effect_1d.md`](effect_1d.md) §7).

**Established:** when `Effect_FindFree` answers 0xFF for an arm (the pool of
20 full), `EffectKind22_SpawnArms` `0x46F490` leaves the dword `+0xC + 4 i` as
the record's last use left it - another kind's data, 0, or a record address
from an earlier kind-0x22 life - and `EffectKind22_WaitArms` `0x46F530` reads
byte `+6` through it: a small value faults, a stale address answers for a
record that may be anything now. So a full pool can hang the effect (the byte
0, for ever), end it early, or crash. And `Effect_Release` clears `+0..+4`
only, so a folded arm's `+6` = 1 stays until the record is reused: if another
spawner takes it first and writes `+6` (kind 0x22 itself writes 0), the parent
waits on a record that is no longer its arm.

**What ours does:** dereferences the cell as the original does.

**Reach:** a full pool at the spawn (D206); not measured.

**Status:** latent, faithful; a crash candidate with a full pool.

## D235 — Kinds 0x82 and 0x83's wait: 6 does nothing where 0xFF ends it, the "reached" exit is overwritten, and 0x83's copy tests record 0 (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, groups E3D ([`effect_3d.md`](effect_3d.md) §7) and E4A
([`effect_4a.md`](effect_4a.md) §7).

**Established:**

- `EffectKind82_Wait` `0x487C50` maps `0x903849` 3, 4, 5 to three, four, five
  pushes and only 0xFF to the release; the byte table holds one more index
  (3) at 0xFF and none at 6. A script that writes 6 to end the push leaves the
  record waiting. Whether 6 is ever written was not traced (the writers of
  `0x903849` are the event scripts').
- The same function sets `+1` twice: when record 0 has reached the leader it
  writes 0x16 (0x17 when `0x90384A` is 0x80), then the switch overwrites `+1`
  for 3, 4, 5 and releases for 0xFF - the "reached" exit is lost whenever a
  push count is pending. (It also leaves `Field_State` on the leader, D210.)
- `EffectKind83_Wait` `0x488240` is kind 0x82's code copied whole: every other
  state of kind 0x83 pushes and checks `Sprite_Objects` record 1, but this one
  still compares record 0's x with the leader's for its 0x16 exit (and
  `_Finish`, shared, stores record 0's distance). With both kinds alive the
  0x16 exit follows record 0.

**What ours does:** reproduces.

**Status:** latent, faithful.

## D236 — Kind 0x49's variant 3 writes through a pointer only variant 4 sets (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29, group E2D ([`effect_2d.md`](effect_2d.md) §7).

**Established:** `EffectKind49_V3Burst` `0x478900` adds to the word `+0x10`
behind `0x6761D0`, which only E2E's `0x4789D0` (variant 4's state 0) sets, to
`0x92D1C8`; the image's initial value is not an address of the game.

**What ours does:** writes through the same cell.

**Reach, as the doc says it:** chapter 7 spawns variants 3 and 4 together, so
variant 4 runs first in practice; a variant 3 alone would write through the
stale value.

**Status:** latent, faithful.

## D237 — Sub-kind 0x4F's variant 0 is drawn along x and marks and tests cells along z (latent, faithful)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-10-03, group E5B ([`effect_5b.md`](effect_5b.md) §7).

**Established:** `EffectKind18Sub4F_Start` `0x5011C0` sets `+8` = (v == 0) as
sub-kind 0x0F's does, so variant 0's gate is drawn along x; but 0x4F's cell
writes ((x, z), (x, z + 1)) and its leader tests always take the along-z
shape. A variant-0 gate of this sub-kind would mark and test cells across
the quad it draws.

**What ours does:** reproduces.

**Reach, as the doc says it:** whether any area spawns 0x4F with variant 0 is
not known.

**Status:** latent, faithful.

## D238 — Small slips in round thirteen's effect code, each read once (latent)

**Seen:** not seen in play; found by reading the code while taking it over,
2026-09-29..10-03, by the round-thirteen groups (their docs' defects
sections). Kept in each; none is known to show.

**Established:**

- `EffectKind05_Bounce` `0x4656C0` at a height of exactly 0 clears `+0xB`
  (the jump at `0x4657A0` lands on the `jle` at `0x4657DA`) where a height
  above 0 sets it: the bounce is re-armed only by an exact landing - "possibly
  intended". Kind 0xA (`EffectKind0A_Start` `0x4658E0`) copies record 0
  whatever record 0 is (kind 5's sprite only when kind 5 took the first free
  record) (E1A).
- The steps of kind 0xF's sub-kinds read "the record before `+0xA`": with
  `+0xA` 0 and `+9` not 0 up to four bytes in front of the script (the high
  bytes of the step table's last pointer); a record's own start never gets
  there, a record left mid-step does - ours reads the same bytes rather than
  abort. `EffectKind0F_DrawCountHeader` `0x468A40` writes 0x20 over a byte it
  has just found to be 0x20. `EffectKind0F_Child3Grid` `0x467E10` takes the lit
  mark's string at `0x66A32C + mark * 8` and the others' at `mark * 4` (with
  the image's marks the lit one reads entry 0 or 2 of three) (E1B).
- `EffectKind1C_Start` `0x46D8B0` overwrites the placement
  `Area10_SpawnEffect1C` gives it with (0x600000, 0x110000) - "observed, not
  necessarily a defect"; `EffectKind1E_ShardNext2` `0x46E6C0` and `_ShardNext3`
  `0x46E6D0` are unreachable; kind 0x20's `+0x64` floors at 8 only in `_Fall`
  (E1C).
- `EffectKind24_Shrink` `0x46F8B0` stores 0 to `+0x12` (already 0) when `+0x14`
  goes below 0 and leaves `+0x14` negative (no effect: the next state
  releases; control 38); the rays are drawn from the
  leader (`0x46FAE0` reads `ObjTrio`), not from the kind-0x23 record that
  spawned them - "the owner's to say"; `EffectKind26_DrawBand` `0x46FFB0`'s
  outer shade wraps past t = 0x500, which its one caller never passes (E1D).
- `ExtraSlots_PickItem` `0x52A8F0`'s two slots are not symmetric: the kind-0xB
  slot takes the id under the cursor even when it is already the slot's
  (`Inventory_Remove` then `Inventory_Add` of the same id), where the kind-0xA
  slot checks first, and neither checks `Inventory_Remove`'s answer; whether
  a remove can fail from this list is not measured (E1F L4).
- `ItemTrade_LeaveAsk` `0x594100` with confirm and cancel in one frame runs
  both tests and the screen opens again; whether both can be pressed in one
  frame was not traced; ours keeps it (E1G L1).
- Kind 0x28's push leaves the record's point at its last cell; kind 0x2A's sets
  a member's `+8` (5 or 1) whether or not its `+1` lets `Member_SetState2_8`
  run; `EffectKind2E_Burst` sets a dword `+0xC` nothing of the kind reads
  (E2A).
- `EffectKind3B_Rise` `0x474240` steps `+0xA` three times a frame, once per
  spiral, and resets only the spiral it is on to shade 0x80 when it caps: the
  three spirals' shades are not stepped alike - "for the owner's eye". Kind
  0x33's state 3 is dead (state 2 releases first). `EffectKind2F_DrawTrail`
  `0x473360`'s clamp catches only the first line's first end (E2B).
- `EffectKind43_SparksRun` `0x4762D0`: a spark whose phase is 5 or more is
  drawn every frame and never taken out of use; only kind 0x43's states write
  the `+6` it derives from (0, 1, 2), so not reachable from them (E2C).
- Kind 0x47's lookup (`EffectKind47_Start` `0x477E30`) reads a sixth triple
  past the five at `0x654564` when the model's animation byte is none of the
  keys - zeros in the image: animation 0 from 0 (E2D).
- `EffectAngle_Mean` `0x479970` does not wrap its answer: two angles in the
  upper half and 0x800 or more apart give up to 0x13FF; its callers were not
  read for a mask (E2E).
- `EffectKind4F_Start` `0x47B990` keeps FC1's `EffectKind3C_Start` behaviour:
  when the second sprite is not found the first is given back but `+3` keeps
  its index and the next frame tries again (E2F).
- Kind 0x76 (`EffectKind76_Shade` `0x485CB0`) draws colour 0 under abr 2
  (subtract), nothing on the PlayStation's rule - what the port's renderer
  makes of it was not looked at; kind 0x78 restores record 2's turn only in
  `EffectKind78_End` `0x4864A0`, so a record released early leaves it turned
  (E3D).
- `EffectKind82_Count18` `0x488150` can never reach 0xF in play
  (`EffectKind82_Start` and `_Again` set `+2` 0 on every pass): its "0xF or
  more, back to 0xD" exit is dead; kind 0x84's entry 4 (`Effect_StateRelease`)
  is unreachable (no state stores 4) (E4A).
- Kind 0x88's states 1 and 2 wait for extra object 0's x to equal `0x3C8000` /
  `0x408000` exactly - an object that steps past never moves the kind on;
  `EffectKind87_Setup` `0x489030` recomputes rows 8..13 of `0x654DA0` in
  `.data` every time - idempotent while rows 0..7 are constant (E4B; compare
  D226).
- Kind 0x8D sets the party records' byte `+0x48` and never puts it back
  (whoever reads it is outside the band); kind 0x8F's stage byte is one cell
  for every kind-0x8F record, harmless as each computes it before reading it
  (E4C).
- Kind 0x91 writes `Field_Request` 2 itself after `Msg_OpenScript`; kind 0x95's
  two states test different party bytes (`0x80310F`, record 2's `+0x137`, in
  `EffectKind95_Clock` `0x48CE10`; `0x802E77`, record 0's, in `_Finish`
  `0x48CEA0`) - "as read, perhaps deliberate" (E4D).
- Drawn 320 wide or centred on 320 x 240 under DIV-0041's wide picture:
  `EffectKindAF_DrawScreen` `0x4932E0`'s tile (E4F), sub-kind 0x15's strip,
  scrolled quads and gradients (E5C), `EffectKind18Sub21_DrawSpiral`
  `0x505E60` (E5D) - for DIV-0041's survey (the round doc's section 18 item
  5), not defects of the original.
- E5D's `_DrawColumn`'s second argument is dead and `_DrawTiles`' never read
  (the callers pass 0x80 / `(0x1F - +9) * 4` and 0) (E5D).
- `EffectKind18Sub3F_ShadeClut` `0x507C40` does not clamp: a level above 0x80
  carries a channel into the next (its callers never pass above 0x80); the
  spin's angle mask `& 0xFFD` (`0x506FC0`, `0x5070E0`, `0x507AC0`, `0x507B20`)
  also clears bit 1 (E5E).
- `EffectKind18Sub42_WaitFlag` `0x509740` compares the flag's whole byte with
  `+8` where `_WaitFlagBack` `0x509980` compares `(flag == 0)`: the same pair
  of tests only while `+8` is 0 or 1 (the placements set it);
  `EffectKind18Sub42_Mark` `0x509850` textures item 0 when the draw-item pool
  is empty (the word stays 0, the next frame tries again);
  `EffectKind18Sub49_WaitCue` `0x50A3E0` can step `+2` twice in one frame
  (E5F).
- Sub-kind 0x3E's far test (`EffectKind18Sub3E_WaitFar` `0x50C930`) crosses
  the centre at 0x8000 where every other sub-kind's has 0x20000; 0x2D's place
  (`EffectKind18Sub2D_Place` `0x50C160`) reads two flags from the spawn's z
  cell's low byte and then overwrites `+0x3A`, which 0x2E..0x32 and 0x3E test
  whole - both "as read" (E6A).
- 0x38's place (`EffectKind18Sub38_Place` `0x50F130`) tests the leader's z
  first whatever `+8` says, where every other place and wait picks the axis by
  `+8` - "perhaps a slip" (E6B).
- 0x64's `EffectKind18Sub64_Watch` `0x516560` can step `+2` down below 2
  (`Game_Mode` 5 and `Game_Step` not, in state 2), which `_WaitBattle` steps
  back - "as read, perhaps deliberate" (E6D).

**What ours does:** reproduces each (E1B's script read reads where it could
abort).

**Status:** latent, harmless by reading or the owner's to judge where a doc
says so.

**PlayStation (read 2026-10-23, the twin read of
[`psx-twin-check.md`](psx-twin-check.md)):** Yes for most, read 2026-10-23 against the JP overlays (the US where a
matched twin exists): E1A's both items (AREA030#4 `0x801D2644`, `0x801D29B8`),
E1B's grid stride (`0x801D66A0`: the PlayStation passes string addresses
where the PC reads pointers, the same 2:1 ratio), E1C's placement (SCENA01
`0x801F73FC`), E1D's rays from the leader (SCENA03 `0x801F88FC`, a fixed
point at `0x80145EC0`), E1F (`0x801DD050`), E1G (AREA030#5 `0x800F5BE8`),
E2B's Rise (AREA103 `0x801F2CB0`), E2C (AREA120 `0x801F36B0`), E2D (AREA085
`0x801F30E0`, the sixth triple's bytes zero there too), E2E (SCENA07
`0x801F8F40`), E3D's colour-0 subtract (AREA188 `0x801F2C3C`, the
PlayStation's own), E5E's no clamp and the `0xFFD` masks (AREA082
`0x801F5388` and four sites), E5F's flag pair (AREA140 `0x801F33FC` /
`0x801F37C4`), E6A's 0x3E far test (AREA157 `0x801F3760`), E6B (AREA166
`0x801F345C`), E6D (AREA120 `0x801F3B84`). **One is the port's:**
`EffectKind0F_DrawCountHeader` `0x468A40` - the PlayStation (JP AREA030#4
`0x801D7BAC`, US `0x801D7B64`) writes `0xFF`, its message font's space, over
a leading `0x20`; the PC writes `0x20`, which does nothing (the US also moved
the coordinates to `0x11C`, `0xFA`; the JP's are the PC's). Not read: E1B's
out-of-step read, E1C's reachability items, E1D's Shrink and DrawBand, E2A,
E2B's kind 0x33 and trail clamp, E2F, E3D's kind 0x78, E4A, E4B, E4C, E4D,
E5D, E5F's Mark and WaitCue, E6A's 0x2D place. The DIV-0041 item is not a
PlayStation question.

## D239 — A map cell's side faces read their texture words by which sides exist, so a cell created at another moment is textured differently (fixed by DIV-0085, seen by the owner; the sea bridge's "waterfall")

**Found:** the owner, 2026-10-06, playing area 41's sea bridge under the wide
picture (DIV-0041): tall columns of sea-like texels hanging from the deck's
east edge to the bottom of the screen, "the waterfall". **Read 2026-10-07**
(the session-`a8d0ee80` scratchpad, `vis/`, with `BOF3X_DRAWORDER`, the
draw-order log's new byte dump of tall quads, and item traces in
`map_layers.cpp`). **Capcom's, PC and PSX by the code; shown only by the
wide picture here.**

**The mechanism.** `MapView_CellTextures` `0x56F9B0` reads a cell's
texture words from the map in a fixed walk: the cell's own, then the `+0x8E`
(south) side's **if that side item exists**, then the `+0x7E` (east)
side's. So the east side's word is the map's third dword when the cell has a
south side and the *second* when it has not. The sides are allocated once,
when the cell's draw item is created (`MapView_Build`), by comparing corner
heights at that moment; and the bridge's sky effect
(`EffectKind18Sub15_Draw`) rewrites the deck columns' corner heights every
frame, a hump that follows the party. So whether a deck cell gets a south
side depends on the frame it is created, which depends on the terrain cull:
with the original's `[-50, 370]` the deck cells of column 48 were created
without a south side and their 125-px east faces (down to the sea) read
`0x12800100`, a 64-texel-tall rectangle; with the wide cull they were
created earlier, got a south side, and the east faces read `0x0180001E`, a
16 x 16 sea tile stretched over 125 px - the streaks. The bytes of the
faces in the two runs differ only in u, v, CLUT and page; the positions are
identical (`wide_bytes100.bof3x.log`, `wide_bytes0.bof3x.log`).

**Ruled out on the way:** the draw-item pool and the draw table (no
diagnostic fires), the renderer's two texture caches (no miss or fallback
at the frame), the packet pool (a quarter full), DIV-0077 / DIV-0079, the
VRAM shadow's page 0, a double allocation (a live bitmap on the pool found
none in the scene), a stale side word at allocation (none).

**The survey, 2026-10-08: authored own, south, east - the code's walk is
right; the deck's edge has no word for a south side.** `tools/side_survey.py`
(`--dat` or `--disc`) on the PC's 200 area blocks (`AREA*.DAT`, the kind-0
chunk of tag `0xC8000`), and the same on the Japanese disc's 200 (the
section for `0x80104000`): every total identical, and `AREA060` byte for
byte. The method: each cell's sides from the corner heights as loaded, by
`MapView_Build`'s two tests; its tile's run of dwords (to the next index
any cell uses); each word decoded as `Prim_SetTexture` decodes it.

- **Side sets against runs** (the map's last row and column left out):
  single-sided S 32,791 cells (run 2 or more: 32,408), E 32,091 (31,714),
  two-sided 7,911 (run 3: 7,832; run 2: 7; run 1: 72). Runs longer than
  the static sides are common (cells with no static side: 156,589 of run 2,
  108,640 of run 3) - words for sides the static heights do not make.
- **Faces are shaded by their facing.** A single-sided cell's word 2 is
  unambiguous: south faces carry shade `0x78` / `0x70` / `0x80`, east faces
  `0x50` / `0x60`. On the two-sided cells, word 2 looks like a south word
  and word 3 like an east word: 7,144 cells for own, S, E against 259 for
  own, E, S (429 ties), each word scored by how often single-sided S and E
  words carry its signature (source, shade, turn).
- **Shape against drop agrees.** A side word's vertical texels are twice
  its drop in corner units on 90 % of single-sided cells. Under own, S, E
  the two-sided cells' words fit as well (92 %), under own, E, S worse
  (75 %); per cell with unequal drops, 2,408 to 13.
- **The bridge is `AREA060.DAT`** (70 x 100; the docs' "area 41" names
  it otherwise - which numbering is unresolved here). The deck is columns
  45..48, rows 1..98, flat at corner height 32 in the file over sea at 0.
  Columns 45..47 have tiles of two words, the second **0**: a south face
  authored as word 0, for the steps the sky effect's hump makes between
  deck rows. Column 48, the deck's east edge, has two words too: its own
  `0x1286000C` and the **east** face's `0x12800100` (the 64-texel
  rectangle). It has no word for a south face. The dword after its run is
  the next tile's own word, `0x0180001E`, a 16 x 16 sea grid cell. So a
  column-48 cell that the hump gives a south side at creation hands its east
  word to the south step and draws the 32-unit east cliff with the sea tile:
  the streaks, exactly as measured live.
- **The PSX twin is the same code** (`SLPS_009.90`, capstone):
  `FUN_80153B8C` allocates `+0x46` (the PC's `+0x7E`) when `next[0] <
  own[1]` or `next[2] < own[3]` and `+0x4E` (`+0x8E`) when `below[0] <
  own[2]` or `below[1] < own[3]`, only for a new item (`0x80153F3C..
  0x80153FFC`); `FUN_80154D50` reads `+0x4E`'s word first, then `+0x46`'s,
  a zero one releasing `+0x46` (`0x80154E1C..0x80154EC8`). Capcom's on
  both machines; when it shows depends on when the cull creates a deck
  cell under the hump.

**Why the wide view (2026-10-08, offline on `AREA060`'s block):** the sky
effect writes rows `r - 17 .. r + 13` of the deck round the party's row `r`
and restores nothing outside them, so a party walking north leaves every
row from `r + 13` back to where it started as a one-unit step (the slope's
last row, 36 / 37, written again one row on each frame). A walk from row 70
to 50 leaves column 48's rows 63..83 with a south side under the build's
test. The cell keeps whatever it was created with: the original's cull
creates the edge's cells elsewhere and the wide margin (DIV-0041,
`BOF3X_WIDE_TERRAIN` 100 against 0, the two logs above) on the steps -
inferred from the geometry, not yet measured: a creation line against the
party's row in one `BOF3X_DRAWORDER` log would show it.

**Fixed by DIV-0085** (2026-10-08, the owner's request; `BOF3X_SIDE_DUP`, on
by default): a cell textured with both sides whose file heights give it one
draws both with its one side word. The owner, 2026-10-08, the bridge wide,
walked several times: no streaks, the edges unremarkable.

**Where DIV-0085 can act (2026-10-08, offline, all 200 blocks).** It needs a
cell the file gives one side, whose tile has no third word (54,842 cells in
198 areas), *and* code that moves heights during play so the cell is
created with both. The writers of `AreaMap_Corners` in ours and where they
run:

- `EffectKind18Sub15_Draw` (the sky effect): `AREA060`'s deck, columns
  45..48. Column 48's 97 edge cells are the only ones it can reach. Seen.
- `EffectKind18Sub3D_Ripple`: area 47 (`Area47_SpawnEffect3D`, handler 1),
  35 x 35 cells about the leader while `Cond_ByteFE` is set; the party
  starts at (18, 36). `AREA047` has 221 qualifying cells (136 east-only, 85
  south-only), 206 of them faces with a drop of 8 or more. The two long
  cliffs are x 40, z 1..28 and x 47, z 29..57 (east faces, drops up to 80);
  the rest are short runs at x 12..36 near the start and a row at z 13, x
  89..106. The sibling's docs name area 47 ウィンじろ がいへき (Wyndia
  castle's outer wall); the owner (2026-10-08): likely the jump off the
  wall and the float down, one way only. The two long cliffs would be the
  wall. One way does not protect it as it largely does the bridge: the
  ripple moves every cell of its block every frame (each row its own
  phase, a cell's two edges by different amounts), so cells entering the
  view on a single pass are created mid-ripple; the faces it adds are a
  few units high against the wall's 80. Not yet seen.
- `AreaMap_ApplyPatch`'s height records: `AREA094`, `103`, `128`, `140` -
  never give a one-sided cell both (every combination of entries checked).
- `EffectKind18Sub62_Ripple`, `EffectKind18Sub22_SetMap` (four raised or
  lowered places), `BossMap_SetCorners` (one boss's map), MAGIC102 Quake (a
  16 x 14 block heaved in a battle): where they run is not read here.

What the change looks like where it acts: a short face carrying the same
texture as the cell's tall one beside it; what the original showed there
was the next tile's texture stretched over the face. `BOF3X_SIDE_ZERO=1` (an experiment, off by
default) releases a side whose word is 0 and does not touch this.

## D240 — At the end of an MP3 stream the decoder leaves the last frame's byte count in place, so `Music_Decode` steps over 1,152 samples it never wrote at every rewind (Capcom's; mirrored by DIV-0091; fixed by DIV-0093, 2026-10-11)

**Found:** 2026-10-10, by the comparison of DIV-0091 ([`mp3-decoder.md`](mp3-decoder.md)
section 2), on every one of the 177 MP3 files: Capcom's `Mp3_Decode`
`0x5AFC40` returns `0xFFFFFDFE` at the end of the stream, writes nothing to
the output (a 0xA5 fill is untouched), and leaves 4,608 - the last frame's
count - at `[[decoder + 0x14] + 8]`. **Configuration: Capcom's decoder
driven in-process by ours, identically to the game's own calls.**

**The mechanism.** `Music_Decode` `0x5A6F30` (`symbols.toml`'s evidence:
"the frame's bytes are read from the decoder after each call and counted
whatever the call returned, the end of stream included"; ours is the same)
adds the count to `filled` after the end-of-stream call too. On a looping
track it then seeks to 0 and goes on filling from `filled`: the 4,608 bytes
before it are whatever `Music_Staging` held from its previous fill - the
music 0x12000 bytes (0.418 s) earlier - and they play for 26 ms at the seam.
On a once-only track the zero fill starts after them: 26 ms of stale music
precede the silence. DIV-0081's measured loops (39 tracks) never reach the
end of a stream, so they are untouched; every other looping track and every
once-only one has it.

**The PlayStation twin:** none - the PSX has no MP3 path.

**The fix** is `kEosBytes = 0` in `src/audio/mp3.cpp` and the self-test's
expectation; a ledger entry, since it changes what is heard. **Done
2026-10-11 as DIV-0093** at the owner's word ("zeros, to match our
implementation in other stale-byte decisions"): 0 stored at the end of the
stream, the self-test exit 0, the comparison's shape test expecting the one
planned difference. The owner heard the battle theme and the fanfare loop
unchanged the same day (both are table loops, which never reach the end).
Recurs only under `BOF3X_MP3=original`.

## D241 — Battle music runs on after a fight ends and the area's never resumes; the title music through the load screen: a stalled fade (ours, DIV-0028 amended) and a dead resume (the port's, DIV-0092) - both fixed 2026-10-10 night, the owner's ear owed

**Found:** the owner, 2026-10-10 evening, in play: "our game doesn't
'end' songs where I expect (compared to psx). The battle music will
continue after battle ends, and the area music won't resume." Catalogued
for a fix; **configuration unknown** - which build, whether the cache's
music (DIV-0087, `BOF3X_CACHE` / `music=`) or the MP3s was playing, and
whether `BOF3X_ORIGINAL='*'` shows the same.

**Where to start.** The PlayStation twin first (the twin rule): the PSX
ends a fight by `Music_Play`ing the area's song (`0x80162610`, the resident
SEP's sub-song) - read what the PC's `Battle_End` path calls and whether
`Music_Play` of the area's track is asked for and refused (`Music_LoadFile`
opening neither name is D24; a `Music_Play` during a stopping fade is
DIV-0028's ignored call) or never asked. If the cache's music was playing:
`music_seq::Release` / `Begin` on the track change, and the 2026-10-10
review's fix for a song change into a smaller bank. A route: the owner to
record a fight from a save with `BOF3X_RECORD`, or any battle route of
`tools/recipes/` replayed with the music log (`DIV-0087` lines, and the
`Music_Play` call trace).

**Cause (2026-10-10 night) - ours, not the port's and not the sequenced
engine's.** The owner's route `tools/recipes/musicStopCheck.txt` (from the
`eldritchGene` save: the title into the load screen, two areas, a fight and
back) replayed three ways with a new `BOF3X_MUSIC_LOG=1` line per music
command (`analysis/shots/musicstop_1010/`): ours with the cache's music and
ours with the MP3s both show `Music_FadeOut(16)` at frame 467,
`Music_FadeOutStop(10)` at frame 483 and then **no `Music_Stop` until the
next `Music_Play`** at frame 913 (every `Music_Stop` in both logs is the
release inside `Music_Start`); the fight's end and the fanfare's end the
same. Capcom's code on the same route stopped the title music on time (the
owner's ear). DIV-0028's once-a-frame step watched the float frame deadline
`0x6BC628` for a new frame, and DIV-0047's WinMain (2026-09-24) never writes
it: every fade took one step and stalled, so no stopping fade reached its
`Music_Stop`. Fixed in `Sound_Tick`: the step keys on `Frame_Counter`
(DIV-0028's amendment). The 2026-09-23 confirmation of DIV-0028 in game
predates DIV-0047 by a day. The second half of the report - the area's
music not resuming - was not reproduced on this route: the game asked for
the area's track (`Music_Play(93)` at frame 3841) and it played; whether
the owner's fight was a different case is still theirs to say.

**The second half, found later the same night (DIV-0092):** the resume
`GameMode5_Leave` asks for is `Music_Play(Music_Track)`, and by then the
fanfare's stopping fade has marked the track `0xFF`, so Capcom's PC never
resumed the field's music after a fight at all - the PSX restarts the area's
song from its table. The owner heard Capcom's run stay silent after the
fight. The same route showed it: no `Music_Play` between the
fanfare's stop (frame 2804) and the next fight (3189). Ours now plays the
area's track `0x904CD0` there, and on a load of a save whose track byte is
`0xFF` (the owner's F12 save `eldritchGene`, which loaded silent under both
codes). The owner's ear is owed on both.

## D242 — The dragon gene, the crystal at the lighthouse, is cut at its lower left by the wall in front (Capcom's order; the owner accepts the original look, 2026-10-14)

**Found:** the owner, 2026-10-10 evening, on their route
`tools/recipes/eldritchGene.txt` (the `eldritchGene` save), reported as the
gene clipped by the pedestal's floor. Read twice in the ultrawide round:

- **Group FY** ([`faerie-over-table.md`](faerie-over-table.md) 5-7, 2026-10-13)
  took the model at the pedestal (object `0x802148`) for the gene and found
  DIV-0071 had moved it out of its layer, over the globe's semi-transparent
  facets. That model is **the lighthouse bulb's filament**, not the gene. It
  was ours and is fixed by DIV-0071's 2026-10-13 amendment (a model keeps its
  layer); the owner likes the filament's look as fixed.
- **Group CF** ([`campfire-and-crystal.md`](campfire-and-crystal.md) 6,
  2026-10-14) read the gene itself: the purple-blue crystal at the upper left
  at the start of the route (recipe frames about 330..700, `AREA077`).

**Established (CF):** the crystal is a plain sprite of cells in layer 29,
standing at the centre of its floor cell, not moved by DIV-0071. Its art
reaches below its ground point (composed from VRAM: the opaque extent 30 x 32
px about the point), and the top and face of the raised wall cell (109, 14),
two rows nearer, are drawn in layer 32 over its lower left: covered, not
short. **Capcom's:** 0 of 5,624 px differ in the crystal's box between ours
(DIV-0071 on and off) and Capcom's code; the area's map data is
byte-identical to the Japanese disc's; the PlayStation twins order the same
way (FY's reading); the owner's let's play of the original shows it clipped
the same way.

**Status:** Capcom's order. **The owner accepts the original look for now
(2026-10-14).** A candidate Intent change (a sprite drawn centred on its
point and reaching below it, drawn after a raised cell of the next two rows
no higher than its point) is in `campfire-and-crystal.md` 6, not built; it is
D300's class from the other side, and a census of such sprites should come
first.

## D243 — The lighthouse's light rays before the big screen flash are not drawn (the port's, D215; fixed by DIV-0126, 2026-10-14)

**Found:** the owner, 2026-10-10 evening, on the same route: before the
scene's big screen flash the PlayStation draws light rays coming out of the
lighthouse; here nothing is drawn before the flash. **Configuration
unknown**: not yet run with `BOF3X_ORIGINAL='*'`.

**Where to start.** `--original '*'` on the route decides whether Capcom's
PC port drew the rays at all (then the effect is a kind of ours that stops
short - round thirteen's effect engine, [`takeover-queue-round13.md`](takeover-queue-round13.md)
and the `effect_*.md` notes) or never did (a port omission
against the PlayStation twin, to be read from the PSX's scene script and
effect kind, and a DIV to restore). The flash is in the `_ab` copy's
window between `f02400` and `f02700`; `recipe_shots.py --from 2400 --to
2700 --every 10` gives the close shots.

**Established (2026-10-14, group LR, [`lighthouse-rays.md`](lighthouse-rays.md)):**
the rays are effect kind 0x4B: 32 additive `POLY_G3` wedges from the globe,
each turned by a random rotation built on `EffectGte_SetDiagonalOne`'s
matrix. Capcom's PC writes 1 on its diagonal where the PlayStation writes
0x1000 (D215), the rotation collapses to 0 / 1 / -1, and every wedge is drawn
as a point on the globe (`BOF3X_DRAWORDER`: boxes a tenth of a pixel).
Capcom's PC never drew them; everything else on the path is the same on both
machines.

**Status:** fixed by DIV-0126; the rays are in every capture from recipe
frame 882 to 1014. **The owner approved the rays (2026-10-14).**

## D244 — `install --base` from a cache with an incomplete `base/` writes the missing containers empty or partial and exits 0 (ours, the importer; warned, not refused)

**Found:** the PSP-only round's group PN, 2026-10-12
([`psp-install.md`](psp-install.md) 1), on a PSP-EU cache built before SI's
stand-ins: `install --base --cache cache_eu1 --game g_psp` wrote 742
containers, 223 of them empty, and exited 0. 222 had no `base/` file at all
(`FIRST.DAT`, `BATTLE.DAT`, `START.DAT`, 118 `AREA`s, 40 `BOSS`es, the 8
`RYUD` and 37 arena files - not text), and `install_base`'s line called them
"written empty for the language layer to fill", which is true only of
`AFLDKWA` on a PSX cache. A start that opens one of them would fail.

**Established:** PN added a warning for exactly that case (a container whose
`base` layer the cache lacks; `tools/importer.py` `install_base`, the line
`base: WARNING - N of the M containers have no base/ file in this cache`)
and left the PSX output unchanged; it warns rather than refuses so that a
partial PSP install could be self-tested before SI merged. Checked at the
round tip `88817a07`: still a warning. With SI's stand-ins and MB's banks a
PSP disc alone builds every `base/` container (MS measured 741), so a cache
built by the round's tools no longer reaches it; a cache built before them,
or one a failed build left short, still does. **Where to start:** refuse
`--base` when any container's `base` layer is missing (PN's proposal, "the
safer default once SI's stand-ins make a PSP base complete"), naming the
first few, with no flag to override.

## D245 — `SimoonDust_Offsets` is misplaced in both PSP maps of `.data` (ours, the PSP exe map; not fixed)

**Found:** the PSP-only round's group IM, 2026-10-12
([`psp-image.md`](psp-image.md) 1, `exe_tables.py measure` and the scratch
`relplaced.py`): on a PSP-built image `SimoonDust_Offsets` differs from the
PC's in 18 bytes on both PSP-JP and PSP-EU (PSX-JP: 0). The map's
`BOOT.BIN` run reaches seven words that `BOOT.BIN`'s relocations mark as
`.text` pointers - a jump table - where the PC holds numbers. Not the PSP's
content: the map's fault. It is also 12 of the 124 bytes by which the
PSP-JP and PSP-EU images differ (`psp-image.md` 6).

**Established:** reported, not fixed. The map is `exe_twins.py`'s; splitting
PSP runs at relocated words would also unplace about 600 coincidentally
equal bytes (`psp-image.md` 8). What reads `SimoonDust_Offsets` on a
PSP-only install (the effect named in it) was not run. **Where to start:**
make `exe_twins.py` stop a PSP run at a word the ELF's relocations mark,
and keep the bytes that a second, unrelocated window still places.

## D246 — A PSP-EU `en-150` layer has no Config screen text: its labels and controller names draw blank (ours, the language build; fixed by DIV-0141, 2026-10-17)

**Found:** the PSP-only round's group LG, 2026-10-12
([`loc-build-psp.md`](loc-build-psp.md) 3.3, 6, 8 item 1). The PC draws the
PlayStation's Config screen (6 labels, 17 options, 6 controller names); the
PSP's own Config screen has other rows, so `loc_build.py` builds no Config
chunk from PSP-EU. On a PSP-only English install the 12 addresses
(`0x669F0C`..`0x669F3C`, the row labels; `0x66A338`..`0x66A360`, the
controller names) stay zero, and the 17 option records are not counted. Not
a crash: the disc-only round's class B, read through ours, the bytes zero.

**Established:** not played. **The owner's call, deferred**
(`psp-only-round.md` 7.1 item 7): the owner will look on an emulator at what
the PSP originally drew. The choices LG offered: leave it blank; take the
words that match from the PSP's screen and leave the rest blank; or draw the
PC's Config rows from a built-in English set of ours.

**Fixed 2026-10-17 by DIVERGENCE DIV-0141** (the owner's decision, the same day: fill it with the
words, none more than a one-word name of a function;
[`loc-build-psp.md`](loc-build-psp.md) 3.3). `convert_config_psp` reads the
PSP's own screen out of `BOOT.BIN`: its row tables (the PlayStation's with
the Sound row taken out) locate fifteen option records before them and the
six controller records after them; four row labels sit in `.data` before
the records and two (Msg Speed, Autorun) are built on the label routine's
stack, read off its `ori` / `sb` pairs. 25 of the PC's 29 slots take the
PSP's word; the four the PSP's screen has no word for - the Sound label and
its Stereo / Mono, and the Controller label (the PSP's row for that panel is
"Button Config.") - take ours, `CONFIG_BUILTIN_*` in `loc_build.py`, the
Sound row's records at the Autorun row's count and x. The resulting kind-7
chunk is **byte-identical** to the PSX-EU layer's (and the PSX-US
layer's), so the PSX-EU words and ours are the same bytes in those four
slots. Every other layer unchanged (`loc_build.py all --discs`, the three
disc-only caches, before and after). **Seen headless** on
`tools/recipes/sigma_config.txt` (`abc_run.py`, `--wide 0`, harness root
`C:\Users\kerok\b_d246`, runs `pre_sigma_config` / `post_sigma_config`):
on the PSP-EU-only install the rows and the Controller panel's names blank
before and filled after, and its three Config shots and three timed Config
frames after are pixel-equal to the PSX-US install's; the PSX-US and PC +
`en-US` installs 32 of 32 frames pixel-equal before and after. Not seen by
the owner.

## D247 — A PSP-EU `en-150` layer has no village words (label group 14): four labels draw blank (ours, the language build; fixed 2026-10-13 with D265)

**Found:** the PSP-only round's group LG, 2026-10-12
([`loc-build-psp.md`](loc-build-psp.md) 3.2, 6, 8 item 2): label group 14
is not built from PSP-EU, so its 4 addresses (`0x669E10`, `0x669F08`,
`0x669F60`, `0x669F68`) stay zero on a PSP-only English install: the faerie
village board panel's culture label, the ranked lists' heading word, and
the hi-lo game's money and stake titles draw blank.

**Established:** read from the build's counts, not played. LG's label rules
were fitted to PSP-EU, the one Latin PSP image held; the other groups each
equal the PSX-EU layer's. **Where to start:** where the four words sit in
`BOOT.BIN` (group 14 on the PSX is DIV-0064's English), and whether a rule
of section 3.2 reaches them.

**Fixed 2026-10-13** ([`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 3): the stake title
is the first word after the pointer table that ends before CommuDraw's category table (`0x652DEC`),
and the other four were where LG found them; the PSP-EU group's payload is byte-identical to the
PSX-EU layer's. Seen: the culture label (D265); the heading word and the hi-lo titles on no route yet.
DIV-0090's PSP amendment amended.

## D248 — A disc-only Japanese layer leaves 177 language addresses zero (ours, the language build; not the PSP's)

**Found:** the PSP-only round's group LG, 2026-10-12
([`loc-build-psp.md`](loc-build-psp.md) 6, 8 item 3), counting the
language addresses that stay zero on a disc-built image: PSP-JP `ja-JP` and
PSX-JP `ja-JP` both leave **177** (US and EU: 8). Not new in this round: the
Japanese layer has never built the exe's strings (the verbs, labels, battle
text, Config) but the names. Over the PC install those slots show the port's
Chinese; on a disc-only Japanese install they are blank. The disc-only
round counted US only.

**Established:** counted with LG's scratch `zero_count.py` over the
disc-only round's per-address table; not played. **Where to start:** for
whoever takes the Japanese exe strings - the PSX-JP boot EXE and `BOOT.BIN`
carry the Japanese text the PC's `.data` replaced with Chinese.

## D249 — A language the install has no layer for starts without objection (ours, the engine and the launcher)

**Found:** the PSP-only round's group PN, 2026-10-12
([`psp-install.md`](psp-install.md) 4): the exe host's headless self-test
from a PSP-only install with `BOF3X_LANG=en-150`, when neither the cache nor
the install held an `en-150` layer, logged `DIV-0005: language overlays
DAT\en-150.*.DAT` and `language en-150: the install's` and ran to exit 0;
the game would show a language-less install's text slots.

**Established:** checked at the round tip `88817a07`: `DatLoad_Inject`
(`src/game/dat_load.cpp`) logs the language and asks `dat_cache::HasLayer`
only to choose between the cache's layer and the install's; nothing refuses
a tag with neither. Since LG a PSP disc builds `en-150` and `ja-JP`, so the
round's own installs have their layer; a hand-set `BOF3X_LANG` or a cache
built without `--lang` still reaches it. `main`'s DIV-0102 (a disc-only
install refuses `original` and any language not held, in the launcher and
with a Fatal in `DatLoad_Inject`) covers the same ground once `psp/round` is
rebased onto `main`; whether its test catches this case (a Latin tag with no
`DAT\<tag>.*` and no `loc\<tag>`) is to be checked then, and this entry
closed or kept.

**Re-checked after the merge of `main` (2026-10-12, `psp/main-merge`) -
kept, narrowed.**
- **Closed through the launcher on a disc-only install** (no `BOF3.exe`, no
  cache `loc\zh-CN`): `ConfigResolveLanguage` refuses a tag the install does
  not hold. Measured headless on a US disc-only install with
  `BOF3X_LANG=fr-FR`: exit 1, "The languages it holds: en-US."
- **Still open** in two places:
  - the engine alone: `RefuseOriginalWithoutChinese` (`dat_load.cpp`)
    refuses only an unset or `original` language, and returns at once for
    any tag, so `bof3x.exe` started without the launcher with an unheld tag
    still runs;
  - a PC install: `ConfigResolveLanguage` returns true unchanged when
    `ConfigOriginalAvailable` holds, so `language=` naming a layer the
    install lacks still starts with the Chinese slots and no overlay.

**Where to start:** the same test in `DatLoad_Inject` for a non-empty tag
(no `DAT\<tag>.*` and no cache `loc\<tag>`: a Fatal naming the held
ones), and in `ConfigResolveLanguage` before its PC-install early return.

## D250 — On a PSP-only install the launcher's Music box still offers "The PC's MP3s always" (ours, the launcher)

**Found:** the PSP-only round's coordinator, 2026-10-12, reading the
launcher's page for a PSP-only cache; checked at the round tip `88817a07`
(`src/launcher/config_dialog.cpp` `Populate` and `CacheNote`). The Music box
always lists "The PC's MP3s always", and the cache note says "No cache: the
PC's MP3s play." or "No base\bgm in it: the PC's MP3s play." whatever the
install. A PSP-only (or PSX disc-only) install has no MP3s: `BGM\` is empty
(`disc-only-install.md` 2), so that choice plays no music, and the notes
promise music that is not there. With a cache holding `base\bgm` the note
reads "Holds base\bgm: the disc's songs play where it has them.", which is
right.

**Established:** read from the code, not driven. **Where to start:** the
rule DIV-0102 uses for the Chinese (`ConfigOriginalAvailable`: `BOF3.exe` in
the game directory) says whether the PC's MP3s are there; without it, grey
the MP3 entry with the reason and word the notes as the songs the cache has
or none. The round's list also named the Language box, which at the round tip
offers and defaults to "Chinese (PC script)" on such an install (`Populate`
selects entry 0 when `language=` names no layer); `main`'s DIV-0102 already
fixes that (the box drops the Chinese and defaults to the first language
held when the install has neither `BOF3.exe` nor `loc\zh-CN`), so it has no
entry of its own. **Checked after the merge (2026-10-12):** it holds.
- `Populate` offers the Chinese only when `ConfigOriginalAvailable` holds,
  and otherwise selects the first held language.
- The launcher with no language set on a disc-only install starts with the
  first held ("language en-US, the first it holds (DIV-0102)", measured
  headless).

The Music box's MP3 entry and the cache notes are this entry's alone and
unchanged.

## D251 — A disc-built image's battle opened framed wrong: the view centre not stepped, the enemy name box at the top left, a low-level member's ability bonus dropped (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the owner, 2026-10-12, watching the end batch's live disc-only
installs ("combatants shifted left", an enemy partly off screen); EB's
defect 3 ([`psp-end-batch.md`](psp-end-batch.md): combat frame 480, the
party lower right, the enemy off the left edge, its name box at the top
left, on both the PSP-EU and the PSX-EU disc-only installs, not on the PC's).

**Cause** ([`visual-findings-1012.md`](visual-findings-1012.md) 1.4): five
tables zero on every disc-built image - `GameMode5_Turn`'s approach pairs
`0x656AA4` (the battle's view centre was not stepped toward the enemies),
`Battle_OpenEnemyNames`' column places `0x656A34` (the name box at the
top-left corner), the turn order's percent row `0x64E328` (the party's
ability bonus 0 at level classes 0 and 1), the wake chances `0x64E3E8` (a
sleeper never woke by the roll) and the result tally's steps `0x64ADD0`.
Not D56, not the enemy records, not the formation table.

**Fixed 2026-10-12** by VF's `twin` fills (DIV-0101 amended): every byte
read from the disc's own overlay and equal to `BOF3.exe`'s. AB's harness
after it: combat 81 of 87 frames equal to the PC's (the rest the load
screen's `"%X"`, D254, and the title, DIV-0114). Not seen by the owner since.

## D252 — A disc-built image's equip slot icons all one glyph, item counts as stray glyphs, the dragon list's AP costs 0 and the save menu's numbers without their formats (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the owner, 2026-10-12 (the battle equip screen's slot icons; the
save menu's numbers and "space buffer"); EB's defects 4 and 5
([`psp-end-batch.md`](psp-end-batch.md): the icons all flames in
cutsceneAndNue's equip window; the `x 2` and `5/128` drawn as stray glyphs,
the dragon list's AP costs all 0), on both disc-only installs.

**Cause** ([`visual-findings-1012.md`](visual-findings-1012.md) 2, 3, 8):
zero on every disc-built image - the equipment window's six slot icons and
categories `0x66B5B4` and the BATE screen's copy `0x64AE18` (every slot drew
icon 0 and named its item from category 0); the menus' formats (`"%3d/   "` /
`"   /%3d"`, `"%8d"`, `"%3d/%3d"`, the no-next-level text, the play-time
format and its colon `"  :"`, whose leading spaces are Capcom's, the PSX's
`START.EMI#8` byte for byte); the item row's count format, `"AP%2d"`,
`"%3dZ"`, `"*%2d"` and the result's `"%d"`.

**Fixed 2026-10-12** by VF's `twin` fills (DIV-0101 amended), every byte
`BOF3.exe`'s. Not filled, no disc carries them: the save slot number's
`"%X"` (D254) and the empty slot's text `0x669CCC`. "You gain 3 EXP!" (the
`"%d"`) not yet seen in a run.

## D253 — Areas 16 and 45 crashed on entry on every disc-only install (ours, the exe image's gaps; fixed by DIV-0101's amendment, `wmrec`)

**Found:** EB, 2026-10-12 ([`psp-end-batch.md`](psp-end-batch.md) defect 2):
the routes caughFish (area 16) and masterAndManillo (area 45) stopped on
both disc-only installs at area entry, `MoveCmd_AttachOffset`
(`field_objects.cpp` 265) reading the null `+0x0C` vector table of the
area's descriptor (`CRASH 0: reading 0x00000000`).

**Cause** ([`visual-findings-1012.md`](visual-findings-1012.md) 6.1, 8):
`WorldMap_Records`' area words (`+0x18` of eleven 0x1C-byte records,
`0x653910`) were zero on every disc build, so `WorldMap_RecordIndex` never
found a world map and answered 11, the "none" index, whose dwords are
`EffectKind01_States`: the world map ran effect kind 1, whose draw calls
`MoveCmd_AttachOffset` with a descriptor whose `+0x0C` is null in
`BOF3.exe` too (`llvm-symbolizer`: `MoveCmd_AttachOffset` <-
`EffectKind01_Draw` <- `Field_Frame` <- `Field_WaitTransition` <-
`GameMode_Enter`).

**Fixed 2026-10-12** by `wmrec` (the words from `GAME.EMI#0`'s table of the
same shape; DIV-0101 amended). AB after it: both routes run to their end on
PSX-EU. masterAndManillo's world-map entry (transition 1 where the PC takes
11, the walk poses and area list) fixed after that run and **not re-run**;
the PSP-EU side not run. **Re-run 2026-10-13 night**
([`visual-findings-1012.md`](visual-findings-1012.md) 10): both routes run to
their last frame with no crash on PC-EU, PSX-EU, PSP-EU, PC-US, PSX-US and two
copies of the owner's play setup; masterAndManillo no longer parts at tick
~322. The owner's scratch installs `C:\b3g` / `C:\b3p` were built before
`wmrec` and want rebuilding.

## D254 — The save and load screens' slot numbers on a disc-only install: `"%X"` is missing, so each slot draws nothing or the previous slot's tail (ours, a gap no disc fills; fixed 2026-10-16, the engine's own string)

**Found:** AB's harness, 2026-10-12 ([`visual-findings-1012.md`](visual-findings-1012.md)
9; [`abc-harness.md`](abc-harness.md)): on the PSX-EU disc-only install the
load screen draws `) 16` and a stray glyph after "Ryu" where the PC draws
the slot number.

**Established:** all three are the missing `"%X"` at `0x6639D4`, a gap on
every disc-built image (no disc carries the string): `Crt_sprintf` writes
only a NUL, and `Text_DrawFont8` - Capcom's quirk - stops on the byte
*after* a NUL, so it draws the stale tail of the previous slot's text
(`dumps/*.174.bin`: `0x904BA0` "2\0" on the PC, "\0" then "3.." on PSX-EU).
**The owner's call:** leave it, or give the engine its own format string
(an `exe_own` region, recommended by VF; a two-byte constant of ours, not
Capcom's table).

**Also PSX-US** (2026-10-13, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md)
6.1): on every route's load screen PSX-US draws each slot's number blank and
each empty slot as `) 16`, the same as PSX-EU; the US disc carries no `"%X"`
to twin either. Still the owner's call.

**The owner's report, 2026-10-16:** on a PSX-only install the empty save
slots show odd characters, and the same characters show behind slots with
a save's name drawn over them.

**Established 2026-10-16** (PSX-US, this fix's own harness root `C:/Users/kerok/d254`;
the routes `load_list` with the `nue` save, and `cutsceneAndNue`'s save
menu): one cause for both. `Menu_DrawSaveSlot` (`field_o.cpp`) formats the
slot number through `"%X"` and draws it at `(x + 9, y + 5)` **for every
slot, before it looks at the summary**; on a disc-built image `0x6639D4` is
zero, so that draw is the buffer's stale text - `) 57` on each empty slot
(the tail of slot 0's time `00:57`) and a stray glyph after `Ryu` on the
named one, the number's square blank on all of them. Not the empty slot's
own text: `0x669CCC` stays zero on a PSX-only install
([`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4), so it draws
nothing - the US release's blank slot. Not the en-US layer: with only the
format string filled (below) every stray glyph is gone.

**Fixed 2026-10-16:** `"%X"` is the engine's own (`exe_own.cpp`'s
`SlotNumber`, region `0x6639D4..0x6639D8`,
[`exe-engine-own.md`](exe-engine-own.md) 2), filled over every disc-built
image and checked against `BOF3.exe` at every start where the file is there
(DIV-0101 amended). After it, on PSX-US: the slots numbered `0`, `1`, `2` as
the PC draws them, the empty slots blank, the named slot `Ryu` alone.
Captures (the harness root, outside the repo; before = a build without the
region, its log `exe_own: 18 regions`, after `19 regions`):
`runs/d254_before_load_list` and `runs/d254_after_load_list` (`load_list.png`),
`runs/d254_before_cutsceneAndNue` and `runs/d254_after_cutsceneAndNue` (the
save menu, frames 7020..7380). The PC install with a Western layer still
draws the port's Chinese empty-record word there (HANDOFF's staged
localisation PR).

## D255 — On a PSX-only install the inn's and the camp's screen stay black for good: the jingle `OYASUMI` is not there, and the wait hangs (ours, the install's missing jingles; fixed by DIV-0129)

**Found:** AB's harness, 2026-10-12, the shop route on the PSX-EU disc-only
install, ticks 2880..3150 ([`visual-findings-1012.md`](visual-findings-1012.md)
9): the inn's black screen holds longer than on the PC's.

**Established:** not data in `.data`: the inn opens `SND\OYASUMI.DAT` (the
kind-0 jingle, the port's MP3 of the disc's `S_XA00.STR`), which a PSX
disc-only install lacks - the 11 jingles are the PC's MP3s and the PSP's,
not a PSX disc's (`disc-only-install.md` 2) - and the log has `open ...
FAILED` (`runs/vf2_shop/psx-eu/bof3x.log`); the wait runs on until
something else ends it. A PSP-only install has the jingles (DIV-0106). Not
seen by the owner. **Where to start:** the owner's open call on the jingles
for a PSX-only install (`sound-import.md` 7: the disc's XA stream as the
jingle), or the failed stream made to end the wait at once (a DIV).

**Also PSX-US** (2026-10-13, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md)
6.2): `shop` opens `SND\OYASUMI.DAT` between recipe frames 2640 and 2670,
`open ... FAILED`; from 2880 the PC is back in the inn while PSX-US stays
black to the route's last frame, 3150.

**A hang, not a longer wait** (2026-10-13, [`eye-batch-1013.md`](eye-batch-1013.md)
3 item 1, Q7): on the PSX-EU-only install the inn's black screen never ends -
106 of 106 frames black from 2670 to 6720, the last 3,600 frames with no
input, `Frame_Counter` still counting, no `CRASH` - and the camp's the same
from 2610 to the route's end. By the code (read, not measured):
`Sound_LoadStream` `0x587910` returns at the failed open with the stream kind
0, and the wait needs `Sound_StreamDone` `0x587A00` = `Music_IsPlaying() ^ 1`,
so it waits for the area's song, which the failed load never stopped. Twenty
more source files call `Sound_StreamDone`; any wait on a jingle a PSX-only
install lacks would hang the same way. So one of the owner's two answers is
now **required** for the inn and the camp to be leavable on a PSX-only
install. The listen pair is [`wav-dump.md`](wav-dump.md) 4c.

**Fixed** by DIV-0129 (2026-10-16, the owner's "both" of 2026-10-15): a
failed stream open ends `Sound_StreamDone`'s wait at once, and a PSX-only
build makes the 11 jingles from the disc's `S_XA00.STR` by the player's
ffmpeg. `shop` on PSX-US (`C:\Users\kerok\abd255`, rebuilt from the US disc
by this branch's importer): `OYASUMI` opens from the cache, black 2670..2850,
back in the inn at 2880 (the PC's frame in `abc-sweep-1013-us.md` 6.2); on
an install without the jingles the open fails, the log says DIV-0129 once,
and the inn is back at 2820. `campingFishing` on both reaches the fishing
spot for the first time on a PSX-only install, which shows D303 and D304.

## D256 — A PSP-only install's title is the PSP's fishing-version logo, its tiles misaligned in our 4:3 frame (ours; fixed 2026-10-17, DIV-0131)

**Found:** EB, 2026-10-12 ([`psp-end-batch.md`](psp-end-batch.md) defect 6),
on the PSP-EU-only install's title.

**Established:** the PSP's own title page and its `Title_Sprites` entry 10
(a source rectangle into that page) are kept as the PSP's in a PSP-only
base (DIV-0103's amendment; `visual-findings-1012.md` 4.2); whether the
PSP's title art reads right through the PSP's rectangles in our 4:3 frame
was not rendered. Whether the logo is "the fishing version's" is EB's
reading of the capture, not checked. **Next:** the owner's look at a PSP-EU
title capture; then take the PSP's title framing apart (which rectangles
the PSP's 384-wide view needs) or keep a PlayStation title on a PSP base.

**Fixed 2026-10-17** (DIV-0131, [`psp-title-framing.md`](psp-title-framing.md)):
rendered at last, the logo tore at the "F", its right wing was cut flat, a
"FISHING ver." caption and a second TM showed, the same at every width: the
PlayStation rectangles cutting the PSP's pages, whose pieces lie elsewhere.
A PSP base now cuts them where they lie. A PlayStation title on a PSP base is
not possible on a PSP-only install (no disc there carries one).

## D257 — A European PSX disc alone builds 733 of 741 containers: eight area banks are the PAL swap (the owner's open call)

**Found:** EB, 2026-10-12 ([`psp-end-batch.md`](psp-end-batch.md) V1 and
defect 7): `importer.py build` from the European PSX disc alone with
`--lang en-150` writes 733 containers; `AREA100`, `112`, `135`, `138`,
`147`, `168`, `170` and `188` have no source (the coordinator's `ref_eu`
the same).

**Established:** known since 2026-10-08 ([`sound-import.md`](sound-import.md)
2 and 7, [`region-diff.md`](region-diff.md) 8.4): a PAL disc's bank for those
eight areas is a sample swap, built faithfully but not to the recipe's hash,
so it is refused. What a European disc-only install does in those areas
(their bank absent from the container) was not run. Recorded here because no entry tracked it. **The
owner's call** (`sound-import.md` 7 item 1): take the PAL banks for a PAL
disc-only install (a ledger entry), or not.

## D258 — No trees in the field or on the world map on a PSP-built install (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the areas 16 / 45 re-run, 2026-10-13 night
([`visual-findings-1012.md`](visual-findings-1012.md) 10): masterAndManillo,
PC against PSP-EU, area 61 drew no trees from tick 3720 to the end (about a
quarter of every frame), and caughFish's world map at tick 420 none either.
The harness's `psp-area-art` rule had hidden the field half in mode 2.

**Cause:** four of `MapCell_DrawUprights` `0x570210`'s tables
(`MapCell_UprightCounts` `0x66313C`, `_V01`, `_U02`, `_Clut`) were zero on
both PSP images, so no upright quad was drawn; `BOOT.BIN` packs the nine
tables without the PC's padding and the map placed only the five that fill
their slot. Not the area's data: `AREA061.DAT`'s chunk hashes are equal in
all three caches.

**Fixed 2026-10-13** by four `twin` rows from `BOOT.BIN`'s packed copy; the
re-run's area 61 has its trees (173 to 113 differing frames, PC against
PSP-EU).

## D259 — No area's cell hook ran on a PSP-EU install (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** beside D258, the same night: `Area_CellHooks`' 28 area words
`0x662F28` were zero on the PSP-EU image, so the switch cells of areas 44,
49, 52, 77, 86 and others did nothing. PSP-JP had them through `psxmap`, which
reads PSX-JP's `GAME.EMI` address; PSP-EU's holds the table 0x1000 earlier.
Not on either areas 16 / 45 route; seen since on `fairyVillage`: the PSP-EU side never showed the
population board at tick 810 before the fix and shows it at 840 after ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3).

**Fixed 2026-10-13** by `cellhk`, the words from `GAME.EMI#0` (`wmrec`'s
finder). **Open:** other `psxmap` runs at PSX-JP's addresses may miss on
PSP-EU for the same reason - a class to sweep.

## D260 — A PSP-EU install crashes in the faerie village: the board's help line reaches the glyph trap (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the A/B/C sweep, 2026-10-13 night ([`abc-sweep-1012.md`](abc-sweep-1012.md)
3.1): route `fairyVillage` on the PSP-EU-only install, at about recipe frame
1704 in area 177: `CRASH 0: exception 0xC0000096 at bof3x.dll+0x3429A`,
`ecx 0000FFDE` - Capcom's glyph trap in `Text_DrawString`
(`text_draw.cpp` 38) <- `Text_DrawAt` <- `CommuBoard_DrawPanel`
(`rest_4b.cpp` 1197), drawing the board's help line from the message pools.
The PSP side had parted from the others before it (the population-change
board never appeared at tick 810). It recurs after D258's fix. Not seen on
PC-EU or PSX-EU.

**Where to start:** the glyph index -0x22 points at a text byte 0x04 (an
inference from the register); the PSP-EU language layer's village messages
and the image's village tables against the PSX-EU ones. An agent is on it
(2026-10-13 night).

**Fixed 2026-10-13** ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): the board's help
word `0x675F7A` was 0, so the help line drew message 0 of area 177's script pool, a dialogue message
starting with control byte `0x04`. `CommuBoard_PickList` `0x457DD0` / `CommuBoard_PickHelp` read the
help words at `0x652AF0`, whose first four bytes were a gap on all seven disc builds (the map placed
the other 16 from `COMMU01.EMI#7` and missed these behind the PC's code-pointer words). Filled by a
`twin` row from `ETC/COMMU01.EMI`; PSX-EU had the gap too and escaped only because D261 parted its
route first; the PC is not affected. The drift before the crash (no population board at tick 810) was
D259. After: `fairyVillage` runs to its last frame (4719) on all three `eu` installs, no `CRASH`.

## D261 — On a PSX-EU disc-only install the village board's slots draw empty: the area-limit table is a gap (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** measured in [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4
(the area-limit bytes, a game value with no chunk kind); its cost seen by the
sweep ([`abc-sweep-1012.md`](abc-sweep-1012.md) 3.2): the table at
`0x6536AC..0x6536BB` is zero in the PSX-EU image, so the board's slots draw
empty from tick 1350 of `fairyVillage` and the route parts from the PC's at
3600. PSP-EU maps it from `BOOT.BIN`, equal to the PC's.

**Where to start:** whether a PSX overlay that reads it carries it verbatim
(a `twin` row); if none does, it stays a gap and the owner's call.

**Fixed 2026-10-13** ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): the table is carried
verbatim by `ETC/COMMU00.EMI#0` and by section 14 of each village overlay (`AREA175..185.EMI`) on all
seven discs; only the PSX-JP and PSX-EU maps missed it. A `twin` row for those two builds. After: the
board draws its two house slots at tick 1350 and the route no longer parts at 3600.

## D262 — The field Equip screen draws one flame icon on every row on both disc-only installs (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the sweep, 2026-10-13 night ([`abc-sweep-1012.md`](abc-sweep-1012.md)
3.3): `menu_screens` frame `t001440` and its `equip` shot, PSX-EU and PSP-EU.
`Equip_SlotIcons` `0x663660` equals the PC's on the PSX-EU image, so the
screen reads another table, not yet located. Corrects
[`visual-findings-1012.md`](visual-findings-1012.md) 2.2, which took that
table for the whole field menu.

**Fixed 2026-10-13** ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): the screen is
`Menu_DrawEquipCompare` `0x574EC0`, which reads its icons at `0x66386C` (a gap on every build), not
`0x663660`. `STATUS.EMI#0` holds the six bytes twice; its MIPS code loads the second copy from the
compare panel's five readers, so a `twin` row takes that copy. After: the equip region of `t001440`
from 1,470 differing pixels to 0 on every pair.

## D263 — The gene window's "AP" label is blank on the disc-only installs (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the sweep, 2026-10-13 night ([`abc-sweep-1012.md`](abc-sweep-1012.md)
3.4): `dragonTransform`, 101 frames, the region at 200,76-224,88, blank on
PSX-EU (PSP-EU likely too: B-C does not differ there). The string or table
not yet traced; the `"AP%2d"` format `0x66AF8C` was filled in section 8 of
`visual-findings-1012.md`, so it is another.

**Fixed 2026-10-13** ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): `GeneWin_DrawGrid`
draws the label from `0x66AF38`, a gap on every build; a `twin` row from `BATTLE.EMI#15`, where the
label and the 14 bytes after it are one run on every disc. After: the label region from 616 differing
pixels to 0.

## D264 — On the disc-only installs the faerie village's stats are zero: black stat bars, food 34 where the PC shows 84, the simulation's traits zero (ours, the exe image's gaps; fixed by DIV-0101's amendment)

**Found:** the D260..D263 fixes' run, 2026-10-13 ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md)
3; already listed as a gap in [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4): the faerie
records at `0x653210 + 20*r` are zero on both disc images, so `fairyVillage` draws black stat bars and the
village simulates with zero traits - it plays differently, not only looks different.

**Where to start:** `COMMU00`'s nine-byte records carry the values in another shape; filling the PC's
layout needs a new rule (not a `twin`), every byte checked against `BOF3.exe`'s.

**Fixed 2026-10-13** ([`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 2): a new rule shape
in `tools/psp_exe.py`, `faerie`, copies each record's four stats from `ETC/COMMU00.EMI#0`'s sixty 9-byte
records (the stats at +5), found by all sixty in order; "236 bytes, 0 not BOF3.exe's" on all seven discs.
`fairyVillage` (`C:\Users\kerok\ab\runs\vil2_fairyVillage`): food 84, the bars and the faeries'
pop-ups equal to the PC's on both disc-only installs; DIV-0101 amended.

**Also PSX-US** (2026-10-13, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 2 (c), 3.4): the us
sweep, built from `d0d9162a` before the `faerie` rule, saw the same black stat bars on PSX-US at
1710. The rule's row is "0 not BOF3.exe's" on all seven discs, PSX-US included; not re-run on the us
set since.

## D265 — On a PSP-EU install the "Culture" label beside the village board's counter is blank (ours, the language build; fixed with D247)

**Found:** the same run, 2026-10-13 ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): PSP-EU
only; not traced. The board's `"%d/%d"` page counter (`0x6531F4`, engine-own) is a separate, known gap.

**Fixed 2026-10-13** ([`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 3): it is D247 seen in
play - label group 14 was not built from PSP-EU, and `Culture` is its first slot. `convert_labels_psp`
builds the group now; `fairyVillage` draws `Culture` on all three `eu` installs.

## D266 — A Western disc-built image draws the title's mark zero wide: `Title_Sprites` entry 1's width is a gap (ours, the exe image's gaps; fixed 2026-10-20 by DIV-0101's `titlemark` rule)

**Found:** the disc-data inventory, 2026-10-13
([`disc-data-inventory.md`](disc-data-inventory.md) 5 item 1); the policy
branch's note the same day that it is still open.

**Established:** the width byte `0x65385E` (`Title_Sprites+0xE`, entry 1) is
unplaced in the US, EU-English, FR and DE images and recorded as a
`game-value` gap (`recipes/exe-gaps.tsv`); the disc holds 12 there and the PC
10, so no segment matches, and the image keeps 0. A Western disc-only install
therefore draws no mark at all where its disc draws ™. It is what the
harness's `pc-title-mark` difference measures against PSX-EU, and DIV-0114's
"New behaviour" (as corrected) says so. **Where to start:** a `twin` row
reading the width from the disc's own `GAME.EMI#1`, as D260..D263's rows
were made.

**Fixed 2026-10-20** (the owner's call: "read the 12"): `tools/psp_exe.py`'s
`titlemark` rule finds `Title_Sprites` in `GAME.EMI#1` by entries 0..6 and
takes the disc's own width, 12 on all four Western discs (DIV-0101's
amendment of that date). A US disc-only install built with it draws the TM
whole; `exe-gaps.tsv` no longer lists the row.

## D267 — The Meteor Strike chip's four lifts are zero on every disc-built image (ours, the exe image's map; fixed 2026-10-17)

**Found:** the disc-data inventory, 2026-10-13
([`disc-data-inventory.md`](disc-data-inventory.md) 5 item 2).

**Established:** `MeteorStrikeChip_Lifts` `0x65C374` (four dwords, read by
`MeteorStrikeChip_Launch` / `_Rise`): bytes `+0..+0xA` are a recorded gap on
all seven builds, and `+0xB..+0xF` are placed by the `BATTLE.EMI#15`
segment, which starts inside the table and puts 0 at `+0xE` where the PC has
a non-zero byte. So every lift is 0 on a disc-built image. The table is
whole on the JP disc (`MAGIC226.EMI#3`, `0x801F0C68`), where two `psxmap`
rows already take the PSPs' neighbours. Not seen in play. **Where to
start:** a `twin` row from there for every build, with the segment's start
trimmed (a `twin` fills only unowned bytes).

**Fixed 2026-10-17** (`install/recipe-version`): a `twin` row from
`MAGIC226.EMI#3` on all seven builds, and `psp_exe.fill` lets a `twin` take
back a byte a map segment of *another* file placed with a different value
(the segment is generated, `exe_maps/`, so not trimmed by hand). The lifts
equal `BOF3.exe`'s on every image (`tools/exe_scan.py`); DIV-0101 amended.
Not seen in play.

## D268 — A `twin` row at `0x61FBD0` loses three bytes to `AREA111`'s map segment (ours, the exe image's map; fixed 2026-10-17)

**Found:** the disc-data inventory, 2026-10-13
([`disc-data-inventory.md`](disc-data-inventory.md) 5 item 3).

**Established:** the `twin` row `0x61FBD0..0x61FCA8` from `AREA112.EMI#13`
(psx-jp, psx-us, psx-eu-en) is defeated at its first three bytes by the
`AREA111.EMI#13` segment (`0x61FB5C..0x61FBD4`), which runs past
`Area111_GridStart` and owns them first: the image holds `AREA111`'s zeros
where the PC and `AREA112`'s section hold three non-zero bytes, on all five
PSX builds. Not seen in play. With D267 it is the class the inventory names:
a segment whose end or start lands in a neighbour's data; E1's list of bytes
every disc places differently from the PC is the check for it. **Where to
start:** trim the segment, or let a `twin` row override a segment's
overreach.

**Fixed 2026-10-17** by the second: `psp_exe.fill`'s other-file rule (D267).
The three bytes are `AREA112`'s on the five PSX builds; `exe-layout.tsv`'s
stand-in number at `0x61FBD0` is gone. Taking *every* byte of another file's
segment instead moved 13 of the PSP's places and left more PC bytes zero, so
only a byte that differs is taken (measured).

## D269 — The French and German layers drew the English `ITEM` on the Identify panel, and the German the English `EXP` (ours, the language build; fixed by DIV-0117)

**Found:** reading `MAGIC060.EMI` on every disc, 2026-10-13
([`exe-chinese-remaining.md`](exe-chinese-remaining.md) 4.1;
[`labels-1013.md`](labels-1013.md) 1.3).

**Established:** since DIV-0064 group 15 (2026-10-10) every Latin layer drew
the PC's own `ITEM` (`0x65AAB4`) over the items and the PC's `EXP`
(`0x65AAB0`), where the French disc has `OBJET`, the German `OBJEKT` and
`EP`. **Fixed 2026-10-13 by DIV-0117:** group 15 carries each disc's words;
the fr-FR and de-DE captures draw `OBJET`, `OBJEKT` and `EP`
([`labels-1013.md`](labels-1013.md) 1.4).

## D270 — `stallion.txt` crashed twice at `--speed 8` in `Area_ZoneAtIn`, reading address 0 (the port's: Capcom's PC code crashes the same; fixed by DIV-0121; the recipe was on the wrong save)

**Found:** the WAV agent's harvest, 2026-10-13
([`wav-dump.md`](wav-dump.md) 5).

**Established:** `exception 0xC0000005 at bof3x.dll+0xCA3F3`, reading 0, area
`0x1D`, mode 2; symbolized `Area_ZoneAtIn` (`src/game/window_task.cpp` 227)
under `Area_PickMusic` (194) under `Field_ChangeArea` (137). At `--speed 1`
the same recipe from the same slot ran 10,310 frames into area 29 without a
crash. The recipe has no `# save` header (slot 0 as the owner's install held
it), so the route is not the owner's as recorded. **Where to start:** run it
again at speed 8 under `BOF3X_ORIGINAL=Area_ZoneAtIn` (or `*`) to class it,
then read what null zone pointer a fast area change leaves.

**Classed and fixed (2026-10-13):** not a speed effect - at speed 1 the
earlier run timed out at frame 10,310, before the crash's frame 10,726. The
recipe had no `# save` header, and the owner's slot 0 was the `combat` save
(byte for byte): an F12 save in area 29, an encounter field. At frame 10,726
the leader steps onto the map's edge, which leads to the area the party came
from - `0x802290`, which a loaded game has as `0xFFFF` (`GameMode_Start`).
`Field_ChangeArea(0xFFFF, 0, 0, 4)` (the census's dump), and `Area_PickMusic`
reads entry `0xFFFF` of the 200-entry zone-list table `0x668D80`: `0x6A8D7C`,
zero in `.bss`. Capcom's six functions (`--original Field_LeaderStepLands,
Field_ChangeArea,Area_PickMusic,Area_ZoneAtIn,Area_Enter,GameMode_Start`)
crash at the same frame at `BOF3.exe+0x195475`, `Rand` equal throughout. The
PSX twins have the same unbounded lookup (DIV-0121), but only the PC's F12
can save in such a field. **DIV-0121**: such an edge holds. `stallion.txt`
now names `# save stallion` (area 67) and plays to its end through the
Stallion fight. Scratch runs: session a2b7d970's scratchpad `runs/`.

**Met again by the ultrawide census (2026-10-13):** EB-1 hit the same crash
at recipe frame 10,681 at every width, narrow included
([`ultrawide-census-1.md`](ultrawide-census-1.md) 7) - not the width; the
census ran before DIV-0121.

## D271 — The all-Capcom side of `nueRandomEncounter.txt` parts from ours at frame 3,661 (unclassed; one run)

**Found:** the WAV agent's 4a pair, 2026-10-13
([`wav-dump.md`](wav-dump.md) 4a, 5).

**Established:** with `--original '*,-Game_Clock,-Game_WndProc'` the route's
`Rand` count is 112 against ours' 111 at frame 3,661, inside the sunset
cutscene, and the random fights fall elsewhere. Ours and the
`AreaTrackFallback` side agree frame for frame. One run; no state-hash pair
has played `cutsceneAndNue` this far. Possibly related to the regression
signal on `cutsceneAndNue` ([`d56-encounter-bounds.md`](d56-encounter-bounds.md)
6) - not checked. **Where to start:** repeat the pair; then `randlog` and a
`BOF3X_STATEHASH_DUMP` at the parting frame.

## D272 — The Western layers' dialogue names the US release's buttons over the PC's code (ours, the language build; fixed by DIV-0140, 2026-10-17)

**Found:** the key-icon census, 2026-10-13
([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.6, 9).

**Established:** `loc_build.py` copies the disc's icon glyphs into the text,
so nine areas' dialogue (32 mentions under en-US) names the US layout's
buttons, while the code is Japan's: area 75 says cross where the code tests
circle `0x20`; the fishing help lines (areas 30, 89, 129) say cross where
stage 1's cast and `LeaderPanel_S2Steer` test circle; the crane (135) the
same. Wrong whatever the save's mapping where the code is hard-coded. Read,
not played. **The owner's call:** that doc's section 10 (translate the
text's icons, or pair them with a US layout preset).

**Fixed 2026-10-17** by DIVERGENCE DIV-0140 (the owner: translate; the button
round's DT-1 and DT-2, [`button-dialogue.md`](button-dialogue.md)): the layer
carries a button code before each mention and the draw shows the button the
Config has put that role on. Under a PC install with a Western layer at
Japan's defaults all 32 Western mentions now name Japan's button, the one
the code tests (the resolver over every held text, that doc 4.5, 4.6). No
capture: no recorded route opens these messages.

## D273 — A Western disc-only install's world-map legend draws two icons on top of each other (ours, the disc-only art; fixed by DIV-0130 as D307, 2026-10-17)

**Found:** the key-icon census, 2026-10-13
([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.2, 9): the disc's
baked default (US) icon and the live glyph from Japan's words in one place
(`02_worldmap_legend_psx-eu.png`, `vf2_caughFish` t000630). **Where to
start:** that doc's design (section 5) decides which of the two stays.

**Fixed 2026-10-17:** the same defect as D307, reported again by a player
(GitHub issue #78) and fixed by DIVERGENCE DIV-0130 (the importer repaints
the disc's baked icons). The button round's DIV-0135 leaves that repaint as
it is; at the identity Config the disc legends are frame-equal to the
round's references ([`button-routes.md`](button-routes.md) 9.1).

## D274 — A Western disc-only install's dock legend names the US buttons over Japan's code (ours, the disc-only art; fixed by DIV-0137, 2026-10-17, not seen)

**Found:** the key-icon census, 2026-10-13
([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.4, 9): the disc's
legend says cross for OK where `EffectKind5E_Pick` places on circle. No
route reaches the dock. Related: [`disc-data-inventory.md`](disc-data-inventory.md)
4.2 (Q5; its first icon corrected to circle).

**Fixed 2026-10-17** by DIVERGENCE DIV-0137 (the button round's IC-D,
[`rest_3e.md`](rest_3e.md) 11): each row of the legend shows `Sigma` of its
test's bit, and a Western disc-only install now starts at the West's Config
(DIV-0134), so its page's own icons are the buttons the code tests
(DIV-0139). Headless only (the dock self-test, 2,167 plans, 0 mismatches);
**not seen**: RC's placed save `dock128` waits for the owner's recording.

## D275 — The PC's world-map legend names keys statically: wrong once a key is rebound (the port's, D58's class; PC only; fixed by DIV-0135, 2026-10-17)

**Found:** the key-icon census, 2026-10-13
([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.2, 9): the port's own
legend art reads `SPACE` / `X` / `ENTER` whatever the keyboard mapping
(the harness's `pc-map-legend` row, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md)
5). **The owner's call:** that doc's section 10 items 3 and 4.

**Fixed 2026-10-17** by DIVERGENCE DIV-0135 (the owner: the glyphs alone,
[`button-unification-round.md`](button-unification-round.md) 5.1): the
key names are painted over and the buttons the Config holds drawn as
glyphs - the PlayStation's shapes where a PSX disc is among the sources,
the port's stand-ins otherwise. Keyboard key names are a later round's.
Captured on `pc` and `pc-enus`, identity and swapped
([`button-routes.md`](button-routes.md) 9).

## D276 — The all-calls tracer stalls a hosted run before any recipe frame (ours, the tracer; not fixed)

**Found:** the entry-list replace, 2026-10-13
([`hidden-start-scan.md`](hidden-start-scan.md) 8).

**Established:** `input_run.py ... --env BOF3X_CALLTRACE=<list> --env
BOF3X_CALLTRACE_MODE=all` from an ini-less launcher copy arms the list (374
or 367 entries) and then times out with no recipe frame played and
`bof3x.calltrace.tsv` holding only its header; the log's last line is
`retarget ON InputScript`. Both the new and the `_pre1013` list; the same
command without `BOF3X_CALLTRACE` reaches `done`. Consequence:
`pc_hidden_reached.json` is still the 2026-09-21 reach, and the scan's 199
new hidden starts are untraced. When the tracer last worked under the
current DLL was not measured. **Where to start:** bisect the DLL for the
first build that stalls.

## D277 — On a US disc-only install the title's copyright line is drawn on the Japanese layout (ours, our code against the US disc's own layout; the owner's call)

**Found:** the us sweep, 2026-10-13
([`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 3.1): PSX-US draws `1997`
running into `ALL` on one line.

**Established:** `Title_DrawSetA` `0x462820` (ours, `title_states.cpp`)
places entries 8 and 9 the Japanese way; the US image's `Title_Sprites` is
the US disc's (entry 8 160 wide, entry 9 at v 0x30, 20 entries), and the US
disc's own code draws three pieces on two lines, including entry 0x13, which
ours never draws. Not a gap and not a twin: data and code together. **The
owner's call** (a DIV either way): draw the US layout on a US-built image, or
leave it.

## D278 — The Identify panel's question marks are blank on the Western disc-only installs (ours, the exe image's gaps; fixed 2026-10-17, the engine's own marks)

**Found:** the labels round's disc-only runs, 2026-10-13
([`labels-1013.md`](labels-1013.md) 6.2): US and French disc-only installs,
frame 900 of `identify.txt`.

**Established:** the US image leaves `0x65AAA8..0x65AAD0` zero (a
`game-value` gap): every disc's `MAGIC060` block holds the same strings in
another layout, so the byte map places none of it. The marks would take two
`twin` rows, `0x65AAC4` <- US `0x801F0B88` (4 bytes) and `0x65AAC8` <-
`0x801F0B8C` (8). Not built.

**Fixed 2026-10-17** (`install/recipe-version`): those rows would not have
matched - the Western `MAGIC060` holds other bytes there, and the census
(`install-gap-census.md` 4) found the marks blank on all seven builds. The
twelve bytes `0x65AAC4..0x65AAD0` (one `?`, then a row of eight drawn by
count) are the engine's own (`exe_own.cpp` `IdentifyMarks`, DIV-0101 amended),
equal to `BOF3.exe`'s. Not seen in play yet.

## D279 — On a disc-only Japanese install the fishing banner draws a NUL as glyph `0xFFDA`; on PSP-JP it crashed one run in five, on PSX-JP every run (2026-10-18) (ours, the language build, reaching Capcom's scroller; fixed 2026-10-18 by DIV-0144 and DIV-0069's amendment)

**Found:** the jp sweep, 2026-10-13
([`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.1): `caughFish`, PSP-JP,
`CRASH 0: exception 0xC0000005` in `Font_UnpackGlyph`, glyph `0xFFDA`.

**Established:** a disc-built image maps the banner's strings
`0x669FC0..0x66A088` as `language`, and `loc_build.py` builds kind 16 (the
fishing text, DIV-0069) for Latin discs only, so every line is empty;
`EffectKind0F_LineType` `0x466310` draws the character under its cursor
before counting the string, so it draws the NUL - a cell 18.9 MB past the
glyph table: garbage on every fishing visit (PSX-JP and all five PSP-JP
runs), an access violation when the read lands on unmapped memory. The PC
never has an empty line. **Where to start:** a Japanese kind-16 layer from
the Japan disc (D280's scope) or the scroller declining an empty line (a
code DIV) - [`owner-review.md`](owner-review.md).

**A second path (2026-10-18, group CW of the install-gap round,
[`install-gap-live-rows.md`](install-gap-live-rows.md) 3.2):** the banner's
button label, the one-character string behind `0x66A2FC` that
`EffectKind0F_LineNext` draws through `EffectKind0F_DrawGlyph`, is a `language`
slot no `ja-JP` layer writes either (kind 8 is built for Latin donors only). So
under `ja-JP` its NUL becomes the same glyph `0xFFDA` (`0 - 0x26`,
`effect_1b.cpp`'s one-byte branch), beside the lines' own. The read watch saw
the range read on psx-jp and psp-jp (`Menu_Verbs`,
[`install-gap-reads.md`](install-gap-reads.md) 3). A fix of D279 should cover
both: a Japanese layer that writes the label as well as the lines, or the
one-byte draw declining a NUL.

**2026-10-18 evening, psx-jp crashes too, every run** (the install-gap round's
live end batch, [`install-gap-round.md`](install-gap-round.md) 6): `caughFish`
on a PSX-JP disc-only install built from `install-gap/round` ended in a FATAL
both times it ran (after recipe frames 1,800 and 960): an access violation at
`bof3x.dll+0x145328`, the stack `Font_UnpackGlyph` <- `Font_BuildGlyphTexture`
<- `Font_GlyphTexture` <- `D3d_DrawGlyph` <- `Gfx_DrawOTag` <- `Gfx_Present`,
the banner box empty in the frame before. No change of the round touches
these rows; whether the round's base build crashes as often was not run. Until
D279 is fixed (the owner's call, owner-review 2026-10-13), a Japanese
disc-only install cannot fish.

**Fixed 2026-10-18 (branch `install-gap/d279`), both ways, as the owner
decided:**
- **DIV-0144**, the guard: `EffectKind0F_LineType` draws no cursor glyph over
  an empty line, and `EffectKind0F_DrawGlyph` draws nothing for a one-byte
  code below `0x26` (a NUL, or a Japanese line's space). Self-test
  `BOF3X_SHADOW=fishing_empty`, 13 checks, 0 mismatches.
- **DIV-0069 amended**, the words: `loc_build.py convert_fishing_ja` builds
  kind 16 tags 1 (the thirteen lines), 2 (the tabs) and the new tag 4 (the
  three button labels) for `ja-JP` from the Japanese disc's fishing module,
  found by `EffectKind0F_LineNext`'s and `_DrawToggles`' twins. Encoded for
  the `ja-JP` font, the counts read as characters. Both psx-jp's and
  psp-jp's caches carry tags 1, 4 and 2; the exe host logs `13 of 13 fishing
  lines`, `3 of 3 fishing labels (D279)` and `3 of 3 fishing tabs`.

**The live proof** (2026-10-18 evening, root `C:\Users\kerok\g_d279`; caches
and installs built by `abc_run.py prepare` with this branch's tools; the
launcher built into a fresh `lb2\` after Defender took two copies, below).
`abc_run.py run --route caughFish` played each run to the recipe's last frame
(3,889), `end: done`, with no `FATAL` or `CRASH` line in any `bof3x.log`:

| Install | Runs | Crash | What the frames show |
|---|---|---|---|
| psx-jp | 3 (`psxjp1..3`) | none (before: 2 of 2) | the banner typed in Japanese with the red circle label (t1200), the tabs in Japanese over the equip panels (t870); the three runs byte-equal at t870 and t1200 |
| psp-jp | 4 (`pspjp1..4`) | none (before: 1 of 5) | the same words; the four runs byte-equal at t1200 |
| pc-enus | 1, and 1 with both DIV-0144 switches off (`input_run.py --original FishingEmptyLine,FishingLowGlyph`) | none | 129 of 129 timed frames pixel-equal between the two: the guard changes nothing on the PC |
| psx-us | 1 | none | the English banner and label, the layer's four kind-16 chunks logged as before |

Captures: `C:\Users\kerok\g_d279\shots` (`psx-jp_run1_*`, `psx-jp_run3_*`,
`psp-jp_run1_*`, `psx-us_banner_t001200.png`, `pc-enus_banner_t001200.png`).
The same switches-off run on psx-us did not start the game twice (no
`bof3x.log`, no frames; stopped by its PIDs after 10 and 7 minutes), so the
Latin disc-only A/B was not taken. The psx-us run itself is unchanged in
kind: the same `13 of 13` lines, `3 of 3` tabs and `4 of 4` headings, and the
Latin overlays' `loc_build.py all` reports are as before.

Headless, from ini-less launcher copies against a PC copy carrying this
branch's overlays: `BOF3X_SHADOW='*'` under `ja-JP` (`g_d279\L1`) and under
`original` (`lb2`), each `self-test only: done`, `inject: 10087 ours`, 1,086
`MISMATCHES` lines, every one 0, no `FATAL`. **Defender** quarantined
`L1\`'s two launchers at 19:47:58, as the `ja-JP` run started (the run had
already been handed to the job and finished; its log is complete), and
the harness's `L\` launcher at 20:00:21 (one run refused to start, `WinError
225`). Ids 1116 / 1117, `Trojan:Win32/Bearfoos.A!ml`. `lb2\`, a fresh
`cmake -B` build of the branch, was not touched and ran everything else.

## D280 — Under `ja-JP` the exe's own strings are the port's Chinese on the PC and blank on the Japanese disc-only installs (ours, the language build; the owner's call)

**Found:** the jp sweep, 2026-10-13
([`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.2); the eye batch's Q6, the
Config grid in Chinese under ja-JP ([`eye-batch-1013.md`](eye-batch-1013.md)
3 item 8).

**Established:** `loc_build.py` builds kinds 7..12 for Latin donors only, and
kinds 15 and 16 from Latin anchors, so a `ja-JP` layer carries none of: the
Config screen's labels and options, the menu verbs, list titles, stat labels,
battle commands, the skill-type title, the gene tabs, New Game's default names
(the whelp has no name), the zenny unit, the fishing banner (D279) and the
village board's names. The PC draws the port's Chinese for each; PSX-JP and
PSP-JP draw nothing. **The owner's call:** a `ja-JP` layer carrying kinds
7..16 from the Japan disc (the PC's Chinese becomes Japanese; DIV-0015 /
0018 / 0019 / 0020 / 0064 / 0069 extended).

**Kind 16 and the fishing label done for `ja-JP`** (2026-10-18, D279's fix,
DIV-0069 amended): the banner's thirteen lines, its three tabs and its three
button labels (tag 4, drawn in place of `0x66A2FC`'s string by the banner
only) come from the Japan disc on psx-jp, psp-jp and the PC under `ja-JP`.
Kinds 7..15 are still open. What that work learned, for the rest:
- **Find each table by its reader on the Japanese disc.** `loc_build.py`'s
  `lui_sites` (an `addiu` / `lw` / `lbu` and the `lui` that set its base)
  found the fishing module's tables by the twins' draws alone. The Latin
  anchors are not needed, and the Japanese layouts differ from the
  Western ones anyway.
- **A Japanese record's count can be characters, not bytes** (a `0x12` /
  `0x13` / `0x15` lead byte's code counts one). The fishing lines' are;
  `ja_chars` reads such a count.
- **A Japanese string need not end in a NUL**: the labels are 3-byte
  entries (a two-byte code, then the pen) and the tabs fixed 4-byte slots
  drawn by count.
- **`0x66A2FC` is shared with the Config cell `0x461C00`** (its six entries
  as circle, cross, triangle, square, L1, R1). The fishing labels were kept
  in buffers of ours so as not to change the Config. A Japanese Config
  would want the same icons, and the disc's codes are `0x15 0x08..0x0B`
  for circle, cross, triangle and square (`JA_ICON`); L1 and R1 have no
  code there.
- **The Japanese space `0xFF` becomes the PC's one-byte `0x20`**. Any draw
  that makes `c - 0x26` of a one-byte code (the banner's `DrawGlyph` did;
  DIV-0144 now declines it) draws a cell past the font from it. A layer of
  kinds 7..15 should check each draw it feeds for that branch.
- **PSP-JP's EMI modules are PSX-JP's byte for byte**, and its `BOOT.BIN`
  data holds the same words, packed. So a PSX-JP reader serves PSP-JP
  through the UMD's EMIs; checking each word against `BOOT.BIN`, as
  `convert_fishing_ja` does, shows they are the PSP's own.

## D281 — On the Japan disc alone the world map's camera sits closer from the load, and the routes part (unclassed: the disc's own, or ours)

**Found:** the jp sweep, 2026-10-13
([`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.3): areas 33, 65, 45 and
30's world map; deterministic over two runs.

**Established:** PSX-JP draws the world map about a third larger, centred on
the party, where the PC and PSP-JP draw the same wider view; the state hash
A-B differs from the world map's load (tick 1095) on `0x905000`'s camera page
and four others, A-C on none. No base chunk or `.data` byte named for the
world-map camera differs on PSX-JP alone. The European disc drew the wide
view (2026-10-12). **The owner's hypothesis** (2026-10-13): they do not
remember the Japanese release's map being closer but it would be subtle;
since the PC and the PSP both draw the wide view, the zoom may be a change
Capcom made after the first Japanese release. **The check:** the PSX twin -
JP against US `GAME.EMI`'s world-map camera set-up - and a raw dump of
`0x905000`'s page at tick 1095, PSX-JP against PSP-JP.

## D282 — The Config screen's option values are blank on every disc-built image (ours, the exe image's gaps; fixed 2026-10-17, DIV-0101 amended)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 2): PSX-EU and PSX-US draw
the labels but no option values.

**Established:** the row tables beside the records, `0x653808` (row to first
record) and `0x653810` (row to count), are zero in the PSX-EU, PSX-US and
PSP-EU images. `recipes/exe-gaps.tsv` classes the whole range
`0x6536F8..0x653830` `language`, but the language layer writes only the 17
records (`0x6536F8..0x653808`), not the two tables. **Where to start:**
`config_text.cpp` says the tables are byte-identical to the PlayStation's,
so a `twin` fill from the disc is the likely shape (DIV-0101's class). The
PSP-EU Config text itself is D246.

**Reported by a player** on a PSX-only install, 2026-10-17 (GitHub issue
#77). **Fixed** the same day by a `twin` row from `ETC/START.EMI` (all seven
discs hold the 13 bytes there; the padding after them differs by disc);
DIV-0101's amendment has the measurement and the live check (PSX-US, the
title's Config draws every row's values).

## D283 — The wide picture changes the random sequence (ours, DIV-0041's side effect; fixed by DIV-0041's random split, 2026-10-13)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 3).

**Established:** `randlog` narrow against wide parts on the attract at
recipe frame 4437 and on whelpBoss at 285 (wide calls `Rand` 8 more times a
frame); the other five routes are equal. Area 2's steam vent, outside the
narrow view and inside the wide band, is drawn and makes puffs. In whelpBoss
the fight then goes another way. DIV-0041 is written as the picture only.
**The owner's call:** allow it (and say so in DIV-0041), or keep the band's
emitters off `Rand`.

**The owner's call (2026-10-13): split the random.** The cause
([`ultrawide-census-2.md`](ultrawide-census-2.md) 3.1,
[`ultrawide-rand.md`](ultrawide-rand.md)): DIV-0041's lowered cell inset hands
`MapCell_DrawRising` `0x570660` cells only the wide walk reaches, and it calls
`Rand` eight times a cell; `BOF3X_WIDE_INSET=0` brought `whelpBoss`, `shop`
and the world map back to narrow. The steam vent's puffs come from that one
random consumer by elimination; the vent was not traced to its cell.

**Status:** fixed by DIV-0041's 2026-10-13 amendment (the map walk's random
split: band cells draw from a generator of ours, the original's cells keep
`Rand`). With it and the other two splits (D290, D291) the eleven hash routes
keep narrow's `Rand` count on every frame at 16:9 and 21:9
([`ultrawide-census-3.md`](ultrawide-census-3.md) 0). A second logic path
from the band, the area patch cells, is D295.

## D284 — Area 23's sunset sky does not change colour in the wide picture (ours, the wide picture; not traced)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 4): narrow, the sky turns
from blue to orange between 390 and 660; wide, it stays blue across the whole
width. Not traced to its function.

## D285 — Battle sprites' flash phases differ in the wide picture, and between two wide runs (ours, the wide picture; not traced)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 5): hit-flash silhouettes and
a boss's tint differ at the same tick, once the command cross (gameover
`t000810`). The narrow run repeats exactly; two wide runs differ from each
other at 1830..1890 only. Not traced.

**What the ultrawide round measured (2026-10-13..14), and what it did not:**
the wide modes then changed the game's state on every hashed route - the
off-screen flag (D290), the band cells' `Rand` (D283) and the windows' late
free (D291) - and three routes parted, a fight among them going another way
(`cutsceneAndNue`'s Nue fight showed a different hit at 21:9;
[`ultrawide-census-1.md`](ultrawide-census-1.md) 5). With the three fixed, the
eleven hash routes keep narrow's `Rand` and game pages at 16:9 and 21:9
([`ultrawide-census-3.md`](ultrawide-census-3.md) 0,
[`ultrawide-census-4.md`](ultrawide-census-4.md) 0). The `gameover` route of
this entry was in the census (`--speed 8`) but was not hashed, and its flash
phases were not looked at again. So the random drift is a **likely** cause,
not a shown one, and it would not explain two wide runs differing from each
other.

**Status:** open; a `gameover` hash at 1 and 2 against narrow, with its flash
frames compared, would settle it.

## D286 — The fishing equip screen's headings are blank on the disc-only installs (ours; fixed by DIV-0101's amendment, FR and DE by DIV-0069's, 2026-10-18)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 6): `EQUIP`, `ROD` and
`GUIDE` drawn on the PC, blank on PSX-EU and PSP-EU (718 pixels). Whether a
gap of the image or of the language build is not known.

**Established 2026-10-16** ([`fishing-rule-pages.md`](fishing-rule-pages.md),
with D303): a gap of the disc-built image. `ROD` .. `GUIDE` are four
fixed-width fields at `0x653EA0` (`EffectKind0F_PanelTitles`), zero on every
disc-built image: each disc's fishing module holds them after its tabs, which
the PC lacks there, so the map's segment stopped short. Now a `twin` row of
`WORLD00/AREA030.EMI#4` on PSX-JP, US, EU and both PSPs; seen drawn on PSX-US
(`campingFishing` 3360). **Still open on FR and DE**: those discs re-lay the
words at other widths, so the PC's fields have no copy there; drawing them
needs the module's widths in ours (DIV-0069's way with the tabs).

**FR and DE fixed 2026-10-18** (group LA of the install-gap round,
[`install-gap-labels.md`](install-gap-labels.md) 1): DIV-0069 amended. The
language layer built from each disc carries the module's four headings at the
disc's own counts (chunk kind 16 tag 3, read off the module's draws), and our
draws centre each on the original's centre by its real width. The image's
bytes stay zero on an FR or DE disc-built image; the layer every disc-only
install builds from its disc supplies the words. The read watch had seen the
fields read on psx-fr and psx-de (`caughFish`, `masterAndManillo`;
[`install-gap-reads.md`](install-gap-reads.md) 5 item 7). Captured on both
disc-only installs (`campingFishing`, harness root `C:\Users\kerok\g_la`);
the owner's eye owed.

## D287 — The launcher page clips two lines (ours, the launcher)

**Found:** the eye batch, 2026-10-13
([`eye-batch-1013.md`](eye-batch-1013.md) 3 item 7, Q2): the note under
`Snap the picture...` is cut on every cache; the Data suite's reason runs out
of its box with the owner's `pc-plus-us` cache.

## D288 — Ryu's form image 10 decodes out of step on the PC: one LZSS flag bit corrupted in the port's data (the port's data, PC only; the owner's call)

**Found:** the arena render, 2026-10-13
([`arena-render-1013.md`](arena-render-1013.md) 0 item 1, 3); the inventory's
P7 / Q3 ([`disc-data-inventory.md`](disc-data-inventory.md) 6).

**Established:** the byte at `+0x7ACE` of each `RYUD` is a flag byte of
image 10's LZSS stream; the port's value turns one match into a literal, so
the stream decodes out of step from texel (59, 6): 332 of 2,048 texels wrong,
14 outside the palette, the stream 39 bytes short of its record. The one
frame that draws the image shows stray pixels along its parts' lower edges.
Every disc's stream is whole. A corruption, not an edit. **The owner's
call** (recommended: take the disc's; on a PC-only install clear bit 3 by a
structural rule): [`arena-render-1013.md`](arena-render-1013.md) 6.

## D289 — Label group 16 writes faerie r's name into record r + 1: faerie 0 is nameless and every other shows its neighbour's name (ours, the language build and `labels.cpp`; fixed 2026-10-18 by DIV-0064's amendment, the owner's confirmation pending)

**Found:** [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4's defect
(2026-10-12), left open by [`abc-sweep-1013-village.md`](abc-sweep-1013-village.md)
5; never numbered.

**Established:** the PC's faerie record is the name first (`0x653200 + 20 r`,
the stats at `+0x10`); `convert_labels` pairs the disc's record r with the
stats at `0x653210 + 20 r`, and `labels.cpp` writes name r at `0x653214 + 20
r` - record r + 1's name field. Skipping the "unnamed sixtieth record" is what
keeps the write off the area-limit table `0x6536B0`. Under every Latin layer,
the PC path included. Whether the owner's fairyVillage route shows shifted
names was not looked at. **Where to start:** group 16's slots at `0x653200 +
20 r` and the name check at + 0 in `loc_build.py` - a `src/` change and a
change of the PC path's output, so a ledger amendment and the owner's word.

**Fixed 2026-10-18** (group LA of the install-gap round,
[`install-gap-labels.md`](install-gap-labels.md) 3): DIV-0064 amended. Group
16's sixty slots are `0x653200 + 20 r`, and `loc_build.py` sends all sixty
names (the "unnamed sixtieth record" was record 59's stats read as a name).
Every faerie shows its own name under a Latin layer, the PC path included.
Because that changes the PC install's output, **the owner's confirmation is
pending** (labels doc 8, [`owner-review.md`](owner-review.md) "2026-10-18, the
install-gap round"). Not seen in play: the `fairyVillage` route starts a new
game on a German install. An overlay built before the fix sends record 59
empty, and that record then keeps the shipped name.

## D290 — Under 16:9 the sprite cull's off-screen flag moved with the picture, so nine logic readers decided at the wide edge (ours, DIV-0041 since 2026-09-30; fixed by DIV-0041's flag split, 2026-10-13)

**Found:** group UC of the ultrawide round, by reading, 2026-10-13
([`ultrawide-culls.md`](ultrawide-culls.md) 2); measured by EB-1
([`ultrawide-census-1.md`](ultrawide-census-1.md) 5).

**Established:** `Sprite_Draw` `0x5935B0` sets bit 7 of an object's byte 0
when it culls it, and nine functions of game logic read the bit (a follower's
catch-up, a scene waiting for an actor to leave, a boss walking until off
screen, four effects, a fish's sound), plus the two free-record scans that
test the whole byte. DIV-0041's object culls of 2026-09-30 moved that cull out
by the columns, so under `BOF3X_WIDE=1` the readers decided at `[-117, 437]`
where the original decides at `[-64, 384]`. Unledgered until the round. On
`combat` the census read object 2's byte 0 as `0x81` narrow and `0x01` at
21:9. Ours; not in the PSX (no wide view), and the PSP kept the cull at
`[-64, 384]` for its 32 columns.

**Status:** fixed by DIV-0041's 2026-10-13 amendment (the flag split:
drawn at the wide bound, bit 7 at Capcom's at every width;
[`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) 1). `combat`'s byte is
`0x81` at 21:9 again ([`ultrawide-census-2.md`](ultrawide-census-2.md) 0).

## D291 — In the wide modes the menu and shop windows were freed or held late, and recorded routes parted (ours, DIV-0041's slide bounds since 2026-09-23; fixed by the faster slide, 2026-10-13)

**Found:** EB-2, the state hash after the flag split, 2026-10-13
([`ultrawide-census-2.md`](ultrawide-census-2.md) 3.2).

**Established:** DIV-0041 moved the fourteen slide-off bounds out by the
columns, so a window counted as active 2 frames longer at 16:9 (53 / 0x20)
and 4 at 21:9, and the battle and menu code waits on the record before it
acts on the next input. `BOF3X_ORIGINAL=Widescreen` (the 18 patched values
reverted) kept `dragonTransform` (16:9 and 21:9) and `cutsceneAndNue` (21:9)
on the narrow route; the window records that differ are `MenuSlide_RightOff`
`0x59A5E0` and `MenuSlide_LeftOff` `0x59A580` / `MenuSlide_LeftOff170`
`0x596920`. Ours.

**Status:** fixed by DIV-0041's 2026-10-13 amendment (the faster slide:
Capcom's bound for every reader, the box drawn to the wide bound on the same
frame; [`ultrawide-slides.md`](ultrawide-slides.md)). Both routes keep
narrow's `Rand` and game pages at both widths
([`ultrawide-census-3.md`](ultrawide-census-3.md) 0).

## D292 — Effect kind 0x8D's camera pull reaches past the field's cell ring at its first frames: a notch at the picture's edges (ours, the wide picture; by the model, not seen)

**Found:** group UC, by the ring's projection model, 2026-10-13
([`ultrawide-culls.md`](ultrawide-culls.md) 3).

**Established:** at the field camera (distance 0) Capcom's 28-column ring
covers 560 columns once DIV-0041's inset reaches 0. Past `Camera_Distance`
about 2,200 it did not: three areas' scenes at 2,176..2,560, kind 0x8D's
pull and off-axis yaws over low ground needed more ring columns than it
has. **Narrowed 2026-10-21 by DIV-0041's wide-ring amendment**
([`wide-ring.md`](wide-ring.md)): the wide modes draw the PSP's 33 x 58
ring, which by the model covers up to about 3,200 (ground 256 below) to
3,700 (level ground) at 21:9 - every persistent distance our source writes.
What remains is **kind 0x8D's first frames**: its pull starts at 7,296 and
falls by 42 a frame to 1,024, needing 17.5 half-columns at 16:9 and 23 at
21:9 against the grown ring's 16.5 - by the model's table, the first 15 or
so frames at 16:9 and about 80..100 at 21:9. No route reaches kind 0x8D; not
seen. The real far-camera scenes crafted saves reach (area 167's init,
chapter 1's scene 3, area 149's chapter-15 camera) were already covered by
Capcom's ring but for one corner pixel at 21:9, which the grown ring fills
([`wide-ring.md`](wide-ring.md) 8). Ours (the PSX has no wide view; the
PSP's own 33 x 58 leaves the same frames).

**Status:** open, narrowed; the owner accepts the remainder (the wide-ring
round's call 1, 2026-10-21). A ring sized for 7,296 (about 50 columns) is the
fix if it is ever seen.

## D293 — A breath mote is not drawn in the wide bands (ours by design: its draw consumes `Rand`)

**Found:** group UC, by reading (row 13 of [`ultrawide-culls.md`](ultrawide-culls.md)
1b), kept by UC2 ([`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) 3),
2026-10-13.

**Established:** `BreathMote_Fly` `0x4DECA0` culls at Capcom's `(0, 320)`
and gates `BreathMote_DrawRing`, which calls `Rand`, the CRT generator battle
damage shares. Widening the cull would change the random stream, so it was
left: a mote stops at the 320 view's edge at 16:9 and at 21:9. Of UC's four
Capcom culls inside the 16:9 view, the ground sprite and the glow were
widened (DIV-0041's 2026-10-13 amendment); this and D294 were not. Not seen
in play.

**Status:** left by design - the owner, 2026-10-15: leave (the logic stays
Capcom's). A wide mote would need a ring drawn without drawing a number (for
example the last frame's radius kept for a mote in the band); reopen only if
one is seen.

## D294 — A column's start-up sound is silent while the column is in a wide band (ours by design: the cull gates sound state)

**Found:** group UC (row 11 of [`ultrawide-culls.md`](ultrawide-culls.md) 1b),
kept by UC2 ([`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) 3),
2026-10-13.

**Established:** `EffectKind18Sub1C_OnScreen` `0x505480` plays
`Sound_PlayEffect(0x200)` only inside Capcom's `[-20, 340]`; the call writes
`Sound_Channels` and stops whatever plays on its channel. Left at Capcom's
bound, so a column visible in either wide band starts silently, as the
original decides for the narrow view. Not seen in play.

**Status:** left by design - the owner, 2026-10-15: leave (the sound channel
state stays Capcom's). Hearing the band is one rule (`x` at `-20 - c`,
`340 + c`) and a DIV of its own; reopen only if it is noticed.

## D295 — A band cell can apply its area patch early (ours, DIV-0041's lowered inset; measured; the owner: leave for now)

**Found:** group RN, by reading, 2026-10-13 ([`ultrawide-rand.md`](ultrawide-rand.md)
3.3); measured by RN's second part ([`ultrawide-patch-cells.md`](ultrawide-patch-cells.md)).

**Established:** `MapCell_PatchThenStep` / `_PatchThenStop` (kinds `0x24` /
`0x25`) apply an area patch list through `AreaMap_ApplyPatch` when the walk
reaches their cell; the wide walk reaches band cells the original's does not.
No area file holds a `0x24` or `0x25` record: all 670 patch records are kind
`0x23`, rewritten at run time by `MoveCmd_TestFB` / `TestFC`. 638 patch
textures only; 32 records in 13 areas write ground heights or map bytes that
logic reads. Of those, 15 are by inference never in the band, 11 depend on
where the party stands when a scene or gadget fires them, 6 (area 140) have no
identified trigger. A band cell applies the same values earlier, and only when
the cell is 8 to 10 ring columns from the focus at 16:9 (8 to 13 at 21:9). No
recipe reaches any of the 13 areas. Not seen.

**Status:** open; the owner (2026-10-14): leave it for now. The scenes
worth a look are `ultrawide-patch-cells.md` 4. A fix would have a band cell
skip its `0x24` / `0x25` record until the original's walk arrives.

## D296 — At 21:9 a menu box sliding in is seen whole in the band on its first frames (ours, the wide picture)

**Found:** EB-1's census, 2026-10-13 ([`ultrawide-census-1.md`](ultrawide-census-1.md)
6.3), and the faster slide's reading ([`ultrawide-slides.md`](ultrawide-slides.md)
6).

**Established:** the slide-ins start where the original starts them, just
past the 4:3 edge, which is inside the 21:9 band: on `sortScreens` (frames
1650..1800) a sliding window is seen whole in the band at its first and last
frames. Their timing is Capcom's at 21:9. A slide back in from a parked spot
starts from Capcom's x since the faster slide (DIV-0041's 2026-10-13
amendment), so its first frame is inside the band too. Not seen in play.

**Status:** open. The slide-in states are ours and can take the faster
slide's hook with their own bound as the start; logic would not change.

## D297 — Four effect states read the frame-buffer grab back on the CPU and still see the empty VRAM shadow (the port's; not fixed)

**Found:** group DP, by reading, 2026-10-13 ([`depress-wobble.md`](depress-wobble.md)
5, 6.1).

**Established:** DIV-0125 grabs the picture on the GPU for direct-colour page
textures. Four effect states copy the grabbed rectangle into particle buffers
on the CPU (`Gfx_StoreImage`): `EffectKind6B_States[1]` `0x475EC0`,
`EffectKind61_States[1]` `0x480730`, `EffectKindA1_ReadBack` `0x490BB0` and
`EffectKindA3_ReadBack` `0x491410`. They read the shadow as Capcom's port
does, empty where a grab went, so what they draw from it has no picture.
Not seen in play (no recipe is known to reach them).

**Status:** open. It needs a GPU-to-CPU readback, a larger step than the
grab. **2026-10-17:** the rectangle these four read is the one their own
state 0 fills by `+0x24` bit 3, the PSX's draw into VRAM, which the PC lacked
(D317). Since DIV-0142 that capture is drawn into the shadow, so by reading
they find the captured sprite there; not seen in play.

## D298 — Eight recorded routes have no `# save` header, so they load whatever slot 0 holds and play off their route (ours, the recipes)

**Found:** EB-1, 2026-10-13 ([`ultrawide-census-1.md`](ultrawide-census-1.md)
1).

**Established:** `bridgeWalk`, `combatGroup`, `fairyVillage`, `identify`,
`ninaWalkBehindBlock`, `sortScreens`, `tarmanLogic` and
`vaccinationAndFireScene` carry no `# save` line (`stallion` has had one since
DIV-0121, D270). In the census every one of them loaded only area 29, so their
census rows are area 29's, not their routes'. Census 3 ran scratch copies with
headers added: six take the save of their own name; `combatGroup` and
`sortScreens` have none of their own and were run from `combat`, a guess
([`ultrawide-census-3.md`](ultrawide-census-3.md) 1). The harness refuses
such a recipe unless `--save` is given.

**Status:** open for the six headers (mechanical). The owner's two answered
2026-10-15: `combatGroup` carries `# save combat` (assumed), and `sortScreens`
`# save vaccinationAndFireScene` - a stand-in, since none of the three saves
holding its 11,957 zenny matches its first run's 08:20 or its Camp step;
the sort menus replay the same, the frames from about 1770 on do not
([`owner-review.md`](owner-review.md)).

## D299 — The band census does not see type-`0x0A` sprites and calls the world map and the fishing spot scenes (ours, the instrument)

**Found:** group BA, by design ([`band-census.md`](band-census.md) 5), and
EB-1 in use ([`ultrawide-census-1.md`](ultrawide-census-1.md) 2.2), 2026-10-13.

**Established:** `BOF3X_BANDLOG` hooks `Sprite_UpdateScreen` only: type-`0x0A`
sprites and the four extra party objects (`Sprite_UpdateScreenA`,
`Party_ExtraScreens` in `field_frame.cpp`), overlay sprites, sprites refused
by a full draw list, effects and map cells are not logged. Its void channel
(`BOF3X_BANDLOG_VOID=1`) is not built and is Fatal. Its scene mask (the four
tests the game makes before the leader may move) is set on the world map
(`0C`) and the fishing spot (`10`), and by the attract demo (`02`), not only in
scenes; the 21:9 hide (DIV-0124) keys on a field scene besides the mask for
that reason.

**Status:** open; two small follow-ups, unowned: the hook line in
`field_frame.cpp` and the void channel at the renderer seam.

## D300 — A faerie in the village is drawn through a table: cut by the next cell's top (Capcom's; fixed by DIV-0151, 2026-10-21)

**Found:** the owner, on `dolphinBoss.txt`, 2026-10-13; read by group FY
([`faerie-over-table.md`](faerie-over-table.md) 1-4).

**Established:** in area 175 (recipe frames about 960..2,850) a faerie placed
on a table is drawn with the table's plank over part of it. The object sits at
floor height inside a raised cell, so the next cell's top, in its layer's
second list, is drawn after it. The same picture with `BOF3X_LAYERING=0` and
with Capcom's code; the PlayStation twins of the three functions that decide
the order (`FUN_8014D184`, `FUN_80153B8C`, `0x80156844`) order it the same
way; the placing data are the disc's. Capcom's.

**The PlayStation look (the owner, 2026-10-15):** a let's-play frame of the
PlayStation release ("...now that the light's on, El?") shows the table's
front plank over the lower half of the faerie standing on it - the same cut.
Capcom's on both platforms.

**Status:** fixed by DIV-0151 (2026-10-21, the wide-ring round's side
group FY, [`faerie-fix.md`](faerie-fix.md)) for this scene only, on the
owner's decision of 2026-10-15: area 175's table faerie (the object
running the area's movement script 1) is drawn one layer later, past its
table's tops, with the table's sides drawn again after it over its
shadow. The DIV carries the note to broaden it to `faerie-over-table.md`
4's rule if more examples of the class turn up (D242 would be the
second). The owner approved it off the before/after sheets (2026-10-21);
not yet seen in play.

## D301 — The camp's fire is drawn under Garr in the cutscene (ours, DIV-0071; fixed by DIV-0071's 2026-10-14 and 2026-10-21 amendments)

**Found:** the owner, on `vaccinationAndFireScene.txt`, 2026-10-14; read by
group CF ([`campfire-and-crystal.md`](campfire-and-crystal.md) 1-5).

**Established:** in `AREA053` (recipe frames about 1,260..2,970) the fire, a
`MapCell_DrawFrames` record (the same record as the field camp's in
`AREA090`), is anchored three cells from Garr. DIV-0071 looked for cell
records only two cells out, so it moved him three layers later, over the
flames. With `BOF3X_LAYERING=0`, and with Capcom's code, the fire is drawn over
him. Ours, not Capcom's.

**Status:** fixed by DIV-0071's 2026-10-14 amendment for the scene
reported (`vaccinationAndFireScene.txt`; a cell record whose quads in the
last pass reach the sprite stops the move; the owner saw it in a replay and
approved, 2026-10-14), and by its 2026-10-21 amendment
([`garr-fire.md`](garr-fire.md)) for the sibling case in two more camps
where Garr sits beside the same fire record with his arm and weapon over
it: `AREA053` in chapter 6 (`cs_53.txt`) and `AREA192` (`cs_192.txt`, the
camp the owner saw). Those two were drawn wrong by `main`'s build since
DIV-0071 (the owner's "Garr over fire", 2026-10-21). The body box the
10-14 test used did not reach the fire; the sprite's own drawn box does.
Both are now pixel-identical to Capcom's draw pass; the wider reach was
accepted on the owner's PlayStation check (2026-10-21). Seen on captures;
not yet in play.

## D302 — The ending's sky shows a line where it meets the dunes, and down its last column (the port's; fixed by DIV-0127)

**Found:** the owner, on the first renders of the ending's desert scene for
the launcher background, 2026-10-14 ([`ending-backdrop.md`](ending-backdrop.md) 3).

**Established:** `EffectKind18Sub61_DrawOverlay` `0x5151A0` draws the sky as
pieces 0 and 1 of the table `0x65F470`, 48 rows and 64 columns with a `dv` of
48 and (piece 1) a `du` of 64, where the port's own rule (piece 0's `du` of
255 for 256 columns, pieces 4 and 5, every full-frame fill) is w - 1 and
h - 1. Through the port's texel-centre table `D3d_TexCoords` (`(i + 0.512) /
256`) the last pixel row samples texel row 64 and the last column of piece 1
texel column 64: the row under the strip and the column past it, index 0 of
the page, a different blue. At scale 2 that is a one-screen-pixel line along
game row 48 across the width and down column 319 of the sky. The pieces of
the dunes overshoot too, into their own picture's next row and column, which
shows nothing. By the reading, the PlayStation GPU's whole-texel stepping
would show rows 16..63 for the same table; not measured on the disc.

**Fixed** by DIV-0127: the two pieces' `du` and `dv` are w - 1 and h - 1
once the fills are armed; the fuzz and `BOF3X_ORIGINAL` keep Capcom's
packets.

## D303 — The fishing spot's Rule pages stack their text on a PSX-only install (ours; fixed by DIV-0101's amendment)

**Found:** `campingFishing` on the PSX-US disc-only install, 2026-10-16 (the
first PSX-only run to reach the fishing spot, D255 having hung the camp
before it; the owner saw it live). Recipe frames 3690..3750, the `Rule` tab
(`How to Fish`): the PC (`pc-en150`) and PSP-EU draw page `Fundamentals`
with three lines of controls; PSX-US draws the title `Type of animal` and
the page's entries one over another down the panel and past its bottom
edge (`Type of animal`, `That floats and is easily caught`, `Av. Length:
20CM` repeated). The page counter reads `1/6` on the PC and `0` on PSP-EU
and PSX-US. Not caused by a sound stream: the same with and without the
jingles (`runs/d255b_campingFishing`). **Where to start:** whether the
pages' text comes from the en-US layer or the disc's own fishing overlay,
and what the counter's `0` reads (a gap of the disc-built image would read
0); D286 (the equip screen's blank headings) is the same screen family.

**Fixed** (2026-10-16, `bb38afbf`, [`fishing-rule-pages.md`](fishing-rule-pages.md)):
three runs of the disc-built exe image were zero on every disc-only install -
the Rule pages' title and row message ids (`0x653DF4`), the equip headings
(`0x653EA0`, D286) and the counter's `"%d/6"` with the data page's total
(`0x653EB4`). Zero ids drew pool message 0 (the fish list's first entry) down
the panel; the empty format left a stale `0`. Each disc's fishing module
(`WORLD00/AREA030.EMI#4`) holds all three where its own code reads them: two
catalogued tables (`fishing_rule_titles` / `fishing_rule_lines`, each build's
own layout) and two `twin` rows. PSX-US `campingFishing` now shows
`Fundamentals`, its three lines and `1/6`; the other builds not played.

## D304 — Multi-second single-frame freezes on PSX-only runs, at a different place each run (the machine's saturated SSD, met by a log write on the game's thread; ours fixed 2026-10-16)

**Found:** 2026-10-16, the D255 runs; the owner heard it as doubled sound
effects and music dropping out (DirectSound replays its buffer while the
frame is held). `DIV-0034 frame deadline N ms behind` in `bof3x.log`, one
frame held for seconds, on the same recipe each time: `campingFishing`
PSX-US with jingles, none (first run) and 10,456 ms at frame 940 (second);
without jingles, 3.7 / 4.2 / 3.9 s at frames 4873..4875 and 2.0 / 2.2 s at
5005..5006; `shop` on a copied install, 13,277 ms at frame 64. The EU PC
run of `campingFishing` earlier that day (`abv`): none. The track requests
are the PC's, in the same order. A deterministic route that freezes in a
different place each run points outside the game's logic (the machine:
other sessions' work, a scan of freshly copied caches); not shown. **Where
to start:** the same recipe on the PC and a PSX-only install back to back,
with the owner's eye on Task Manager, and whether a freeze lines up with a
file open or the sequencer.

**Established (2026-10-16 evening, the D304 session):**

- **Not PSX-only, not new.** Of 392 harness runs under `~/ab*/runs` since
  2026-10-12, 113 holds of 1 s or more, on PC, PSX and PSP installs alike.
  Games running at the same moment in different roots held *together*, for
  the same length to within 0.4%: `d255_shop` 13,277 ms and `abv` `wbh1`
  13,294 ms (both launched 16:24:17-19); on 2026-10-13 about 07:5x three
  roots each held 11.4 s, then 2.97 s, then 1.47 s. A machine-wide cause.
- **Alone, none.** PC (en-150) and PSX-US `campingFishing` back to back with
  nothing else running: no hold in either (`abd255/runs/d304a_*`).
- **Disk saturation reproduces it.** PSX-US `campingFishing` with a second
  process writing as fast as the SSD takes it (9 GB in 164 s, 16 MB
  buffered writes, nothing flushed): nine holds, 1.6 to 33.6 s, each the
  length of a stall in the writer's own writes to within 0.3 s (26,009 /
  26,258 ms, 13,672 / 13,794, 33,569 / 33,879, 30,445 / 30,582)
  (`d304/runs/d304b_*`). The owner saw the SSD at 100% in Task Manager at
  the time and earlier that day. Saturation is what the harness itself
  makes: importer builds (ffmpeg), installs copied, 8 MB BMPs a second of
  game time per run and their PNG conversion, several sessions at once.
- **Where the game waits.** `BOF3X_STALLWATCH=1000` (below) sampled the
  main thread in each hold inside `WriteFile`, called from `bof3::Log`
  (`log.cpp`) - a line to `bof3x.log`. A scripted run logs a line every
  frame (`randlog`); once the cache manager's dirty pages pass its limit,
  Windows throttles every buffered writer until the disk catches up, and
  the frame loop was one. DirectSound then loops its buffer: the doubled
  effects and the music dropping out the owner heard.

**Fixed (ours):** `bof3x.log` is written by a thread of its own
(`src/hook/log.cpp`): `Log` appends to a 4 MB queue (no heap - the crash
reporter logs through it) and the writer writes it outside the lock.
Before the writer runs (DllMain, the loader lock) and with the queue full,
the caller writes itself, in order. `LogFlush`, `Fatal` and `LogClose`
drain on the calling thread, and a chunk whose writer `ExitProcess` killed
is written again (a repeated part over a lost one). Same load, fixed DLL:
**no hold** (`d304c`, 11 GB with writer stalls up to 20.8 s; `d304d`). All
5,274 `randlog` lines are there and equal to the unfixed run's. Both self-tests
pass (exe host; hosted with `BOF3X_SHADOW='*'`). No ledger entry: the log is ours, not the game's
behaviour.

**What still waits on the disk:** the harness's own timed shots
(`BOF3X_SHOT_EVERY`). `render::SaveOnFiber` writes each 8 MB BMP on the
game's frame, so under saturation the picture and sound stop for seconds
at every 30th recipe frame. The game clock is paused for the write, so
nothing is logged and the frames taken are unchanged. Harness-only; off
for a player. A player's game still reads the cache synchronously on an
area load, as Capcom's did.

**`BOF3X_STALLWATCH=<ms>`** (`src/hook/stallwatch.h`): when no logic frame
has run for that long, a watcher thread writes the main thread's eip and
stack return-address candidates as module+offset (llvm-symbolizer with
`bof3x.dll`'s ImageBase `0x10000000` added) to `bof3x.stall.log` beside the
DLL, with the local time, again every second while the hold lasts.
Instrumentation; off unless set. Recipe `shot` freezes show as about 1 s
holds in `Freeze`; ignore them.

## D305 — A PSX-US install built on 2026-10-12 crashes loading area 16 under the 2026-10-16 engine (an old cache; refused since 2026-10-16)

**Found:** 2026-10-16, `campingFishing` on a copy of `~/ab`'s `g_psxus` /
`c_psxus` (built by the importer of 2026-10-12): `0xC0000005` reading
`0x00000000` at `bof3x.dll+0x78EAE` at frame 368, area 0x10, just after
`AREA016.DAT` and its `loc\en-US` layer open (dump
`C:\Users\kerok\abd255\L\bof3x.crash-28636-0.dmp`). The same route passed
area 16 on an install rebuilt that day, so an old cache meeting a newer
engine is the first guess, not established. **Where to start:**
symbolise `+0x78EAE`; then whether the engine should refuse a cache older
than it reads, rather than crash.

**Cause** (2026-10-16, `8b5caf5b`): the cache was built at 17:34 on
2026-10-12, before the importer's `wmrec` rule (`cfc307a6`) that evening, so
`WorldMap_Records`' eleven area words (`0x653910 + 0x1C k + 0x18`) are gaps
(zero); `WorldMap_RecordIndex` answers the "none" record, whose effect kind 1
calls `MoveCmd_AttachOffset` with area 16's null vector table (the dump:
`MoveCmd_AttachOffset+0x8e` from `EffectKind01_Draw`). The area 16/45 crash
of `visual-findings-1012.md` 8, met again through an old cache. **Refused**:
`ExeImage_Source` stops on an image whose `data.toml` still classes any of
those words a gap, with a message to rebuild the install
([`exe-import-engine.md`](exe-import-engine.md)); no game behaviour changes.

## D306 — A character beside a bush flickers in front of it and behind it (ours, DIV-0071's 2026-10-14 amendment; fixed 2026-10-17)

**Found:** GitHub issue #79 (a player, a PSX-only install, 2026-10-17: the
rear sprite "flickers between the foreground and background", the scene
where the party robs Bunyan); the owner saw it on the PC install too, and
caught Rei's right side drawn through the bush and behind it in two snips.
Recorded as `tools/recipes/yraallRoad.txt` with its save.

**Established:** with `BOF3X_LAYERING=0` the box round Rei and the bush is
steady over frames 3300..3720 (0 of 421 frames flip); with the rule on, 100
flip, one pass in three. `BOF3X_LAYERING_LOG`: the sprite is moved past layer
18 on the passes where the bush's record run (`MapCell_DrawUprights`) is
judged to have drawn nothing over her body. A dump of the kept quads: on
those passes all six have every point on the trunk's axis. Capcom's handler
commits each quad pair before writing its outer vertices; DIV-0071's
recorder read the points at the commit, so it saw what the packet pool held
there before. **Fixed** by reading the run's primitives when the run ends
(DIVERGENCE DIV-0071, amended 2026-10-17). Not Capcom's and not the PSX-only
install's: the same on every install with the rule on.

## D307 — A Western disc install's world-map legend shows two button icons in each row (ours; fixed by DIV-0130, 2026-10-17)

**Found:** [`key-icons-by-mapping.md`](key-icons-by-mapping.md) 9 (2026-10-13,
`vf2_caughFish` t000630 on PSX-EU); reported by a player on a PSX-only
install as GitHub issue #78 ("Button legend shows everything all at once").

**Established:** the disc's dial page bakes the release's default buttons
into the legend - dim in the frame, lit in labels 1 and 2 - and the PC's code
draws the glyph of the save's words (Japan's) over the same place: a circle
over the baked cross, a cross over the baked triangle. A first repaint of the
labels alone changed nothing on screen: the frame (sprite 0) carries its own
copy, drawn under everything. **Fixed** by repainting both copies in the
importer (DIVERGENCE DIV-0130); verified on a rebuilt PSX-US install.

## D310 — Six draw formats and marks are empty on a disc-only install: area 75's counters, area 104's countdown, the faerie board's rows, a level-up's ability names, the play-time colon (ours, the exe image's gaps; fixed 2026-10-17, the engine's own strings)

**Found:** the install-gap census, 2026-10-17
([`install-gap-census.md`](install-gap-census.md) 4.1), by reading each
reader against the gap list; not seen in play.

**Established:** each is a format string or a one-glyph string Capcom's code
reads in place, zero on a disc-built image, so the draw is empty:
`Area75_DrawCounters`' `0x60ABF0` (the five PSX builds; the PSPs place it),
`Area104_Kind6ACountdown`'s `0x61BC74`, `CommuBoard_DrawRows`' dash and three
formats `0x653084..0x653098`, `BattleResultWin_DrawLevelUp`'s and
`EffectKind5E_DrawMenu`'s `%s` `0x654900` (the four Western PSX builds; Japan's
and the PSPs' `BATL_END.EMI` twin has it), and `Menu_DrawPlayTime`'s separator
`0x6637E0` (all seven). The same class as D254. No disc holds any of them
where the byte map or a search can place it.

**Fixed 2026-10-17:** the engine's own strings (`exe_own.cpp`, the
`game-format` class; DIV-0101 amended), each equal to `BOF3.exe`'s bytes. Not
seen in play yet.

## D311 — The faerie village's hi-lo game pays nothing on a disc-only install (ours, the exe image's gaps; fixed 2026-10-17, a twin)

**Found:** the install-gap census, 2026-10-17
([`install-gap-census.md`](install-gap-census.md) 4): `CommuHiLo_DealSteps+0x14`
`0x652CF4..0x652D04`, the u16 payout by turn that `CommuHiLo_Payout` reads, a
`disc-unplaced` gap on all seven builds; not seen in play.

**Fixed 2026-10-17:** a `twin` row from `ETC/COMMU02.EMI` section 8, which
holds the 16 bytes verbatim on all seven discs (`COMMU02B.EMI` section 7 a
copy); DIV-0101 amended. Not played yet.

## D312 — The Config's Controller panel crashes the game on a PC install with no language layer (ours, round 14's R4F; fixed 2026-10-17)

**Found:** the button round's RC group, 2026-10-17, opening the Controller
panel on the `pc` install kind (the PC's `DAT\` and `BOF3.exe`, no
`BOF3X_LANG`). `0xC0000096` in `Text_DrawString` (bof3x.dll+0x3514A) called
from `Config_DrawControllerCell`, `ecx 0xA65`. It happens from the title's
Config with a harness cache, without one, and with no `BOF3X_*` variable.

**Established** (2026-10-17, reading the code and the capstone read of
`0x461C00..0x461E07`): `ecx 0xA65` is `kIconCircle`, the first of DIV-0051's
six icon glyphs, which `loc_build.py` builds into a Latin layer's font. The
Chinese font ends before it, and `Text_DrawString`'s privileged instruction
is Capcom's own trap for a glyph past the table. Our
`Config_DrawControllerCell` (`config_text.cpp`) drew only the icons, which was
safe while the only way in was the jmp `ConfigText_Inject` puts at `0x461C00`
under a Latin overlay (`Lang_Latin`). Without one, Capcom's cell ran. Round
14's R4F (commit `1750a40d`, 2026-10-05) took `Config_DrawControllerPanel`
`0x461A50`, and ours calls `Config_DrawControllerCell` by name
(`rest_4f.cpp`), so it reaches our icon draw whether or not the jmp went in.
**A regression of ours, from `1750a40d`.** The rest of the panel's no-layer
path was already the original's: the box width, the second cell, the frame
and the names' width and edge are values of ours (`ConfigText_Operands`) that
keep the original's numbers unless `ConfigText_Inject`'s patches go in, which
is under a Latin overlay only. `ConfigText_Apply` writes the label, option and
controller-name text only when a layer's chunk arrives.

**Fixed 2026-10-17:** the cell draws DIV-0051's icons only while its Inject is
on (`bof3::IsEnabled(0x461C00)`: a Latin overlay, not under `BOF3X_ORIGINAL`).
Otherwise it draws the original's path in full, as the 2001 port did. That is
the port's glyph from `0x66A2FC[i]` at (x + 4, y) in colours 2, 1, 6, 5, 0, 0,
and the default table's letter at (x - 0x10, y) in colour 0 (D58's column).
With none of the six bits set, select `0x100` draws the original's three-cell
SELECT label. Our earlier function never drew that label, under a layer or
without one. The rebinding cannot give a line the select bit
(`and ecx, 0xFC`), so only a save could set it. It is still not drawn under
DIV-0051, as before. R4F's fuzz now copies `0x461C00` and compares ours with it
whenever the address is still Capcom's (no Latin overlay). Under a layer the
row is left out and the log says so. The no-layer path restores the
original's behaviour, so it needs no DIVERGENCE entry. DIV-0051 is unchanged.

**Other draws of layer-only glyphs, checked:** `fishing_text.cpp`'s
`kLabelCodes` (DIV-0069, the same icon cells) are reached only through
`FishingText_Label`, and every caller in `effect_1a.cpp` / `effect_1b.cpp`
tests `FishingText_On()`, which is `Lang_Latin()`. `ConfigText_DrawSelected`
(the UI-to-dialogue glyph swap) is reached only through call sites
`ConfigText_Inject` re-aims under a Latin overlay, and through
`ConfigText_Operands().big_draw` / `name_draw`, which stay `Text_DrawAt`
without one. No other code names glyph `0xA65..0xA6A`.

## D313 — A disc-only install's fishing banner labels every line circle and waits about eight seconds between lines (ours, the disc-built exe image's gaps; fixed 2026-10-17)

**Found:** the button round's IC-F group, 2026-10-17
([`fishing-text.md`](fishing-text.md) 8.6), from RC's white reel label on
`psx-us` and `psp-eu`.

**Established** (2026-10-17, [`fishing-text.md`](fishing-text.md) 8.8): the
fishing banner's 13 line records `0x653B98` (`EffectKind0D_States+0x20`;
label byte, pause byte and two zero bytes at `+4..+7`) and the label pens
`0x653C00..0x653C02` were `gap` / `game-value` rows of `recipes/exe-gaps.tsv`
on all seven disc builds, zero in `data.bin`. So on every disc-only install:

- every line's label byte was 0. The six no-label lines (`0xFF` on the PC)
  flipped in and faded out a circle, and the quit and gear lines named
  circle;
- every pause was 0, where the PC's is 12 (text 12: 1). `LineNext`
  `0x466460` decrements the pause before it tests it, so each line waited
  256 frames, not 12, before the next one spawned. Measured on the dense
  fishing route: two reel lines, then about 255 frames of empty banner;
- every pen was 0, so labels were white.

The disc carries all of it. Each fishing module (`AREA030.EMI` section 4;
`BOOT.BIN` on a PSP disc) holds the records as 12 bytes whose `+8..+11` equal
the PC's `+4..+7` on all 13 records of all seven builds. Three label entries
follow, each with a pen at `+2`, in the release's own buttons: Japan's 2, 1,
6; the PlayStation West's cross 1, triangle 6, square 5; PSP-EU's cross 1,
circle 2, square 5. A defect of ours, the disc-only image's. The 2001 port
and the PlayStation both had the values.

**Fixed 2026-10-17** (`fix/fishing-records-disc`): `tools/psp_exe.py` rules
`fishrec` (the four bytes, record for record) and `fishpen` (the three pens
after the records), every disc build. Recipes regenerated:
`exe-gaps.tsv` loses exactly those 91 rows. The pens are each release's own,
so `src/game/fishing_text.cpp` now reads a label in the terms of the
release the running image was built from (`ExeImage_MappedBuild`):
the line byte's button is `M_R` of Japan's and the label is
`SigmaFrom(R, it)` (DIV-0136's amendment). An unmoved label draws as that release's disc does.
This amends DIV-0136, IC-F's entry ([`fishing-text.md`](fishing-text.md) 8.5).
Live on `psx-us`: the lines follow each other as on the PC, no-label lines
have none, the reel label is a red circle at Japan's words, a blue cross
under the swap, and a blue cross under the West's own defaults, as the US
disc draws it.

## D308 — The fishing song goes silent while the fishing panel is open (the port's; fixed by DIV-0132, 2026-10-17)

**Found:** the owner, 2026-10-17, listening to `caughFish.txt` on a PSX-only
install: the music "pausing during the menu", playing on in Manillo's shop.

**Established:** `LeaderPanel_S9Begin` `0x52A440` fades the music to volume 0
(`Music_FadeOut(0x10)`) and `LeaderPanel_S9Menu` `0x52A4A0` back to 127 on
cancel; the PlayStation's twins duck it by 20 sequence-volume units over 16
VSyncs (`SsSepSetDecrescendo` / `SsSepSetCrescendo`). Measured with the WAV
dump: silent over frames 866..966 on the PC install's MP3s and on PSX-US
alike, so the port's, not the disc path's. **Fixed** by DIVERGENCE DIV-0132.
The same rewrite (a duck turned into a fade to nothing) may sit at other call
sites - `GameMode3_Enter` / `GameMode3_Leave` (PSX `0x80198620` /
`0x801986EC` decrescendo 0x14 too, behind a condition not yet read) and the
movement script's `BA` / `BB` ops, whose PSX operands the PC drops - not
surveyed.

## D314 — PSP-EU's ram text names the wrong button on its own disc (Capcom's, PSP-EU only; drawn right by DIV-0140)

**Found:** the button round's HT group, 2026-10-17, answering the round's
open question 4 ([`button-tests.md`](button-tests.md) 5).

**Established** (a capstone read of both PSP `BOOT.BIN`s, addresses and
immediates only): the ram's push (area 121) reads Held against word 1, the
Move line's word, on both PSP builds - PSP-JP `0x24ECAC` reads `0x0FF9D2`,
PSP-EU `0x24EAE8` reads `0x1004E2` - as the PC's `Area121_PushObject` does.
So the ram takes whatever button Move is on: triangle at PSP-EU's defaults
(`S` of square). PSP-EU's text there names circle, `P` of square - the
West's terms. The text is wrong on the PSP-EU disc itself, against its own
code at its own defaults. Read, not played.

**In ours:** DIV-0140 translates PSP-EU's areas 68 and 121 by `P` (the
owner's answer 3), so the mention draws as `σ(square)`, the Move line's
button now - the word the code reads. The PSP-EU disc's own text is left
as it is; whether to record more than this is the owner's call (that doc 5).

## D315 — PSP-EU's well tests the West's buttons (Capcom's, PSP-EU only; changed by DIV-0139)

**Found:** the button round's HT group, 2026-10-17
([`button-tests.md`](button-tests.md) 5).

**Established** (the same read): the well's seven exact compares (area 68;
PSP-JP `0x052788..`, PSP-EU `0x05257C..`) are `== 0x40` x3 then `== 0x20`
x4 on PSP-JP (cross, then circle) and `== 0x10` x3 then `== 0x40` x4 on
PSP-EU (triangle, then cross). That is `P` of Japan's, the PlayStation
West's map, where PSP-EU's own map `S` would give circle and cross. The
rest of PSP-EU's code read follows `S` (the chop, the dock). The well's
text is consistent with its code, so this is a release quirk of PSP-EU, not
a text defect.

**In ours:** DIV-0139 tests `σ(cross)` / `σ(circle)` at the well on every
install, so on a PSP-EU-only install, at PSP-EU's defaults, the well takes
circle and cross where the disc's own code takes triangle and cross - the
unified flow, with the icons the text draws (DIV-0140, answer 3).

## D316 — Every boss and event battle plays track 0 on a disc-only install (ours, the exe image's gaps; fixed 2026-10-17, by rule)

**Reported:** issue #83 (2026-10-17, a PSP-EU-only install: the miners' fight
at the start plays song 000 where the PC keeps the scene's escape music), and
a comment there from a second player: wrong music "anytime the boss music is
played". Listed first among the census's P1 items
([`install-gap-census.md`](install-gap-census.md) 8 item 2), as the owner's
"track half" call.

**Established:** `GameMode5_Load` (`0x495FD0`, `src/game/mode_rest.cpp`)
plays `Music_Play(track)` with the track word of the event battle's
(file, track) pair, `EventBattle_Records+0xE2` (`0x64DECE`, stride 4, 41
words). A disc-built image filled only the file half (`pairs`, DIV-0101's
first amendment); the track half was a `game-value` gap on all seven builds,
so every event battle asked for song 0, and one whose file has no music of
its own (the PC's 0xFF; see DIV-0149 for what that word does) started song 0 over it.

**Where the disc has it:** nowhere as a number. The PSX's `GameMode5_Load`
(`0x80198C94`) loads the battle file and names no song: the boss file's own
music plays. Measured over the 41 event-battle files of all seven discs
(2026-10-17): each file either carries a whole copy of one `BIN/BGM` file's
bank header, SEP and bank body (35 of 41, the `BGMBAT00`..`06` set) or no SEP
at all (5 boss files and none other), and the PC's track word is that BGM
file's sub-song 0 in the disc's own song table - 151..157 - or 0xFF for the
five, 41 of 41 on both PSP discs. On the five PSX discs `BGMBAT02` is a byte
copy of `BGMBAT00`, so one boss file (the PC's 153) is equally 151 by its
bytes: the two songs render identically from a PSX disc's music.

**Fixed 2026-10-17:** `pairs` (`tools/psp_exe.py` `event_track`) fills the
track half by that rule: the music sections of the battle file matched to the
song table's BGM files, the lowest song of a byte-identical set, 0xFF with no
SEP; a file matching more than one distinct BGM file refuses the build.
`recipes/exe-psp.tsv`'s seven `pairs` rows fill 164 bytes, every byte
`BOF3.exe`'s but that one tie on the PSX builds (151 for 153); DIV-0101
amended. Measured: the `whelpBoss` route on a scratch PSP-EU-only install under
`BOF3X_MUSIC_LOG=1` (en-150; the route, recorded in en-US, drifts after the
fight): before, frame 718 loads `BOSS007` and plays `Music_Play(0, 10)`,
`bgm\000`; after `importer.py setup` rebuilt the image, the same frame plays
`Music_Play(151, 10)`, `bgm\151` (bank `BGMBAT00`), and the start-up log's
41 `EventBattle_Records` gap lines are gone. Not heard.

**Amended 2026-10-21 (the owner):** the tie takes the highest song of the
set, not the lowest, so `BOSS033` (event battle 25) plays 153, `BGMBAT02`, on
every build: every byte of the seven `pairs` rows is now `BOF3.exe`'s. On a
PSX base with the PSP's music layer it hears `BGMBAT02`'s recording, no longer
`BGMBAT00`'s.

## D317 — A sprite meant to be drawn into VRAM stays on the screen, and the page it was for stays empty: Amalgam's death has no melt (the port's; fixed 2026-10-17 by DIV-0142)

**Reported:** issue #84 (2026-10-17): "The Amalgam boss does a cool
stretching death animation normally, and here he just scoots back and
freezes until it disappears after a second." The reporter was on a disc-only
install. The cause is in the code, not the data, so it should show on every
kind of install, and with Capcom's own exe too (not checked in play).

**Established** (disassembly of the PSX JP disc and `BOF3.exe`, 2026-10-17):
the PlayStation's sprite draw `0x8014C3C8`, the twin of `Sprite_Draw`
`0x5935B0`, tests bit 3 of `+0x24` after the cull (`0x8014C538`). Set, it
puts a `DR_AREA` (`0x8017CB5C`) of (0x340, 0x100, 0xC0, 0x100) into the
object's ordering-table slot and moves the point: `+0x2E` += 0x340, `+0x30`
+= 0x10 + 240 x (`0x80143D44` ^ 1). Against the draw offset (0, 240 x
buffer), the frame lands at VRAM (0x340 + x, 0x100 + y), and the moved point
culls the object on the next frame. Capcom's PC reads `+0x24` once, for bit
7 (`0x5935C5`), and its renderer never writes the picture into the VRAM
shadow. So:
- `BossAmalgam_DeathStart` `0x439790` clears (0x340, 0x100), moves the
  sprite to (0x32, 0x41) and sets `+0x24 |= 0x88`;
- `_DeathMelt` `0x439830` draws `BossAmalgam_DrawSprite` (a SPRT) and
  `BossAmalgam_DrawStreak` (a POLY_FT4) from that page for 43 frames, and
  they draw nothing;
- the sprite meanwhile goes on being drawn on the screen at (50, 65) in its
  last pose until `_DeathEnd` removes it.

Amalgam's code is the same on both machines (`BOSS012.EMI`'s `0x800C1978`
and `0x800C1A24`).

Other setters of the bit (by reading, none seen in play):
- with `|= 0x88`: Mikba's death `BossMikba_DeathOpen` `0x43DD30`, magic_s14's
  four images, and the capture states of effect kinds 0x6B (`0x475E00`), 0x61
  (`0x4805B0`) and 0xA1 / 0xA3 (`0x490A80`, `0x491310`);
- with `|= 8` alone: kind 0x9D's field object (`0x489D90`).

D297's four CPU readers are the read-back states that follow those effect
captures. They find a picture in the shadow now, by reading; not seen.

**Fixed 2026-10-17:** DIV-0142. **Not enough on its own (2026-10-23, issue
#111):** in play the melt still drew nothing, through D333. DIV-0142 and
DIV-0159 together bring it back.

## D318 — The bedroom's song plays through Ryu's dream in Teepo's bed, where the PlayStation is silent (the port's; fixed 2026-10-18 by DIV-0143)

**Reported:** issue #88 (2026-10-18), on a PSP-EU-only install: the music
from the bedroom scene ("014 Healing") keeps playing through Ryu's dream
after Rei rescues him, where the original is quiet. The cause is in the
code, so it shows on every install, with Capcom's own data too.

**Measured** (2026-10-18, the `whelpBoss` route under `BOF3X_MUSIC_LOG=1`,
the same on the PC install, PSX-EU-only and PSP-EU-only):

- `Scena01_Scene02` (`0x53A380`, step 1, the bedroom, f9127) stops the
  song, plays track 14 and sets `Field_ScriptFlags` bit 0x80, which tells
  area changes to keep the current track.
- Step 2 (f10821) runs `Music_FadeOutStop(10)` and in the same frame
  `Field_ChangeArea(0xA, ...)`. `Area_PickMusic` (`0x5953B0`) copies the
  current track, still 14 while the fade runs, into `0x904CD0`.
- The fade ends at f10831 and marks `0x904131` 0xFF. At f10839
  `Area_Enter` (`0x594E60`) plays the kept 14, and Healing restarts for
  the whole dream, until Ryu wakes (f12708, track 7).

**The PlayStation** (the Japanese disc; disassembly, then the sibling
recomp's headless runtime from this route's save put on a card with
`save_convert.py pc2psx`):

- Step 2's stop is `0x8015DDF4`, straight to `SsSepStop`: immediate, and
  it leaves the current-track byte `0x80145025` at 14.
- The bit-0x80 copy (`0x801A1388`) keeps 14, and the area's play
  `0x80162514` skips a track equal to the current one (`0x80162580`).
- Live: at VSync 64,359 the area's play ran (its count 8 to 9) and the
  music play `0x80162610` did not (its count stayed 4). The SPU had 0
  active voices from then to 65,766, and only sound effects after, until
  the wake's track 7 (VSync 67,863).

So the dream is silent there, and the difference is the PC's fade-out-stop
clearing the current track before the replay.

**Fixed 2026-10-18:** DIV-0143.

## D319 — On a German disc-only install the equipment panel's six slot icons are drawn from zeros (ours, the exe image's map; fixed 2026-10-18 by DIV-0101's amendment)

**Found:** group LA of the install-gap round, by reading, 2026-10-18
([`install-gap-labels.md`](install-gap-labels.md) 2.2). The read watch had seen
the row read in the shop on psx-de (`Menu_DrawEquipPanel`,
[`install-gap-reads.md`](install-gap-reads.md) 5 item 6). Not captured.

**Established:** the German gap `0x663660..0x663677` is two rows. `0x663668..`
are the panel's labels (DIV-0064's group 21). `0x663660..0x663668` are not
text: they are the eight bytes `Menu_DrawEquipPanel` `0x573A80` reads as its
six slot icons (`mov ebp, 0x663660` at `0x573B79`). The German disc holds them
verbatim, but the map's segment stopped there: the bytes after them are German
words, and eight bytes are under the 16-byte window. So on a German disc-built
image the panel drew its six slot icons from zeros.

**Fixed 2026-10-18** (group PT, [`install-gap-twins.md`](install-gap-twins.md)
6.2): a `twin` row on psx-de from `ETC/BATE.EMI`'s second copy (`TWINS`' sixth
field 1). The reader decides between the disc's five copies: that copy has the
three readers of the PC's panel, in its order, and it is where the US and EU
maps take the same range from. The bytes equal `BOF3.exe`'s ("0 not
BOF3.exe's"). `exe-gaps.tsv`'s German row becomes `0x663668..0x663677`,
`language`. Not seen in play.

## D320 — On every PSX-only and PSP-only install a lost fight restarts the fight's own music instead of the loss tune: song 166 is missing (ours, the importer; open)

**Found:** group RD of the install-gap round, by reading, 2026-10-18
([`install-gap-readers.md`](install-gap-readers.md) 3). Not run, not heard.

**Established:** `BattleEnd_LossBanner` `0x431550` (`BattleEnd_LossSteps[0]`)
calls `Music_FadeOutStop(10)` and then `Music_Play(0xA6, 10)` unless bit
`0x40` of `0x904AE5` is set; its counterpart `BattleEnd_WinBegin` plays 165,
the fanfare, under the same test. A capstone scan of the 152 calls to
`Music_Play` and `Music_LoadFile` finds no other constant 166 (callers that
pass a track from data were not searched). A PC install has `BGM\166.DAT`. A
PSX-only install (all five) has `base/bgm/000..165` and an empty `BGM\`, and a
PSP-only one the same 166 files (`ppms.py`). So `music_seq::LoadFile(166)`
finds no song, `Music_LoadFile` opens neither name, and `Music_Play` starts the
previous track's file anyway - D24's path. The fight's own music, just faded,
restarts from the top and loops where the PC plays the loss tune.

**What the PlayStation plays there:** the PSX twins (`BATTLE.EMI` section 3
and its copies; PSX-JP `0x801D6D20` for the win, `0x801D6E1C` for the loss)
play the resident SEP's sub-song 1 for a win and 2 for a loss, after the same
bit test. Sub-song 2 of every battle bundle has the shape of `BGM082`'s sub 2,
table song 86. The PC's `166.DAT` is about as long as `086.DAT`, and the
owner's names call 166 the alternate of 86 (`data/music-names.tsv`).
**Hypothesis:** 166 is the battle bundles' sub 2 through the fight's bank. A
synth render of a bundle's sub 2 under `BGMBAT00`'s bank against `166.DAT`
(`tools/bgm/synth_check.py`'s method) would settle it.

**Where to start:** the importer writes `base/bgm/166.DAT` from a battle
bundle's sub 2 under `BGMBAT00`'s bank, as it writes 165 from sub 1
(`tools/seq.py`; `ppms.py` for a PSP source), once the render matches. A ledger
entry either way: what the PC asks for, what the disc plays, which an install
gets. It is a song, not a row of the exe image, so not a `TWINS` row; whether
it plays through the fight's own bank is the same open choice as 165's.

## D321 — On a PSP-JP-only install the first item list stops the game: `ShoulderGlyphs` finds `0x66A2FC[4]` empty (ours, DIV-0138 meeting D280; fixed 2026-10-18 by DIV-0138's amendment, the owner's call)

**Found:** the read watch's re-run on the merged install-gap round, 2026-10-18
([`install-gap-reads.md`](install-gap-reads.md) 9). Five of psp-jp's eleven
routes end in `FATAL: ShoulderGlyphs: 0x66A2FC[4] is not a one-byte glyph
string` at the first item list (combat frame 515, shop 935, cutsceneAndNue
4,443, dragonTransform 849, menu_screens 1,159); the same with the watch off.
RW's first runs (2026-10-18 morning) ended the same way, unreported there.

**Established:** `GlyphCode` (`src/game/shoulder_glyphs.cpp`, DIV-0138), from
`Menu_DrawPiece`, draws the item lists' L1 / R1 corner. A PSP base has no L1 /
R1 page art, so without a Latin layer it falls back to the port's one-byte
stand-in strings through `0x66A2FC`, which sit in `Menu_Verbs`' tail
(`0x66A2E0..`): zero under `ja-JP`, since no Japanese layer writes kind 8
(D280). The module refuses the empty string loudly, as rule 4 wants. psx-jp
is not affected (its base has the art), nor psp-eu (a Latin layer). D279's
fix (DIV-0144) built the circle / cross / triangle labels for `ja-JP` from the
Japan disc's fishing module; **L1 and R1 exist in no module** there, so there
is no disc source for these two.

**The owner's call:** what a Japanese PSP-only install draws in the item
lists' corner - (a) nothing (the corner left blank, a code DIV-0138 line),
(b) the Latin icons DIV-0051 already builds (`ICONS_AT`), drawn under `ja-JP`
too, or (c) part of the `ja-JP` layer D280 decides. Until then a PSP-JP-only
install cannot open an item list.

**Decided 2026-10-18 (the owner):** on a Japanese-only install, draw just a
capital L and R in the corner; the Japanese font has capital Latin letters
that fit the space.

**Fixed 2026-10-18 (branch `install-gap/d321`), DIV-0138 amended.** On a base
without the L1 / R1 art and under `ja-JP`, `GlyphCode` draws L1 and R1 as the
`ja-JP` font's capital `L` and `R` and no longer reads `0x66A2FC[4]` / `[5]`.
The face buttons keep their paths. psx-jp keeps the art, the Latin installs
keep DIV-0051's icons, and any other empty stand-in still stops the game.

**Corrected 2026-10-21.** Two statements above did not hold. "psx-jp is not
affected" was true only at the default Config: a face button in a shoulder
slot read an empty stand-in on psx-jp and psp-jp alike (the review of PR #91,
2026-10-21). And the Japan disc does have L1 / R1 glyphs: the font atlas's
8 x 8 strip holds "L1 L2 R1 R2 START SELECT" on every disc, outside the 12 px
cells the text codes reach. DIV-0138's 2026-10-21 amendment draws the
Japanese corner wholly from the disc: the face buttons the sheet's `0x15
08..0B`, L1 / R1 those small labels (DIV-0051's amendment, the owner's call
over the capital L and R).

- **The codes, by rule.** The `ja-JP` layer's font puts the Japan disc's
  single-byte sheet whole from `JA_AT` = `0x993`, cell `b` at `JA_AT + b`
  (`loc_build.py` `ja_cell_of`), and the Japanese script's `0x41..0x5A` are
  `A..Z` (`JA_SHARED`; the sibling's `docs/TEXT_ENGINE.md`, settled
  2026-09-09). So `L` is glyph `0x993 + 0x4C` = `0x9DF` and `R` is
  `0x993 + 0x52` = `0x9E5`, two-byte codes as every `ja-JP` string encodes
  them. `JaLetterGlyph` computes them; no table is copied.
- **The glyphs, measured** from the player's caches (`g_d321\scripts\letters.py`
  over `c_pspjp` and `c_psxjp`, the kind-3 table in `loc\ja-JP\dat\FIRST.DAT`,
  3,570 glyphs). Both glyphs are a capital L and R. They are half-width in
  the 24 x 24 cell: ink columns 4..17 (L) and 2..17 (R), rows 2..23 with the
  drop shadow. `Text_DrawSmall` draws a glyph into 8 x 8, so they take
  columns 1..6 and 0..6 of the corner box's 8 (`kBoxW`), and rows 0..7. For
  comparison, DIV-0051's Latin `L1` / `R1` icons on psp-eu (`0xA69` /
  `0xA6A`) fill all 24 x 24. The one-byte slots `0x26` / `0x2C` hold the same
  pixels on these disc-only tables (`JA_SHARED`), but the two-byte codes are
  the `ja-JP` sheet's on every install, the PC's too.
- **A guard.** The draw checks the glyph against the table loaded
  (`TextDraw_GlyphCount`, new) and stops loudly if the font has no `ja-JP`
  sheet.

**Checks** (root `C:\Users\kerok\g_d321`; caches and installs built by
`abc_run.py prepare` with this branch's tools; the fix in a fresh build
directory `lb2\`, the base `9f9edccd` in `lb0\`):

- The build: `cmake --build build`, no error.
- `BOF3X_SHADOW=shoulder_glyphs`, hosted from `lb2\` against a PC copy, under
  `original` and under `ja-JP` with `c_pspjp` as the cache: `720 plans
  rastered ... 24 glyph codes (6 under ja-JP letters, D321) ... 0
  MISMATCHES`, `self-test only: done`. The second logs `a PSP base under
  ja-JP ... L1 / R1 the font's capital L / R (D321)`.
- The exe host's self-test on the psp-jp disc-only install (`g_pspjp`,
  `BOF3X_EXEIMAGE` and `BOF3X_CACHE` `c_pspjp`, `ja-JP`): `self-test only:
  done`, no `FATAL`.
- `BOF3X_SHADOW='*'` hosted from an ini-less `lb3\` against the PC copy,
  under `original`: `inject: 10087 ours`, 1,086 `MISMATCHES` lines, every
  count 0, no `FATAL`, `self-test only: done`.
- Defender logged no 1116 / 1117 event during the work; no launcher copy was
  taken.

**Live** (`abc_run.py run`, one game at a time; every result read from the
run's `_run.txt` and `bof3x.log`):

| Install | Route | Before (RW2, 9.4) | Now |
|---|---|---|---|
| psp-jp | combat | `FATAL` at 515 | `recipe done` at 2,620, no `FATAL` |
| psp-jp | shop | `FATAL` at 935 | `recipe done` at 3,157, no `FATAL` |
| psp-jp | cutsceneAndNue | `FATAL` at 4,443 | `recipe done` at 7,421, no `FATAL` |
| psp-jp | dragonTransform | `FATAL` at 849 | `recipe done` at 4,329, no `FATAL` |
| psp-jp | menu_screens | `FATAL` at 1,159 | `recipe done` at 1,770, no `FATAL` |
| psx-jp | menu_screens, fix against base | - | both `recipe done`; 60 of 60 timed frames and the four named shots pixel-equal |
| psp-eu | menu_screens, fix against base | - | both `recipe done`; 60 of 60 timed frames and the four named shots pixel-equal |

Captures in `C:\Users\kerok\g_d321\shots`: each psp-jp route's first item
list with the corner enlarged four times (`pspjp_combat_*_t000540`,
`pspjp_shop_*_t000960`, `pspjp_cutsceneAndNue_*_t004470`,
`pspjp_dragonTransform_*_t000870`, `pspjp_menu_items*`), and the controls'
`items` shot from both builds (`psxjp_menu_*`, `psxjpbase_menu_*`,
`pspeu_menu_*`, `pspeubase_menu_*`). The psp-jp lists show `L` above the
scroll bar and `R` below it. psp-eu shows DIV-0051's `L1` / `R1`, as before.

**Left open:** the face buttons on psp-jp still read the stand-ins
`0x66A2FC[0..3]`, which are empty there too. They reach the corner only when
the Config moves a face button onto L1 or R1, and then the list stops the game
the same way. That case was not run. D280's `ja-JP` layer, or a ledger line
drawing the Japanese icons `0x9F6..0x9F9` (`button_text.h`) there, would close
it.

## D322 — Sixteen saves and one more `BISLPS??.DAT` overrun the save list's table (Capcom's, the PC port's code; fixed 2026-10-19 by DIV-0146)

**Found:** the quicksave round's staging, by reading, 2026-10-17
([`save-files.md`](save-files.md) 2, [`quicksave-round.md`](quicksave-round.md)
1); measured by group QM's self-test, 2026-10-19
([`quicksave.md`](quicksave.md) QM.6).

**Established:** `Save_ListFiles` `0x4548B0` stores every match of
`BISLPS??.DAT`, `0x18` bytes apart from `Save_Directory` `0x929F40`, with no
bound: a seventeenth file writes past the table's `0x180` bytes towards
`Save_Staging` `0x92A0E0`, and each name is copied whole (`strlen + 1`) into
a `0x14`-byte field. A player reaches it with sixteen saves and one more
matching file (a copy, or since DIV-0145 any quicksave). The search handle
is never closed either (a leak, not fixed).

**Fixed:** DIV-0146's bound under `SaveLists`: at most sixteen entries, a
seventeenth match or an over-long name logged and not stored. Self-test:
twenty matches, sixteen stored, the guard after the table unwritten.
`BOF3X_ORIGINAL=SaveLists` brings the overrun back.

## D323 — A Western disc-only install's staff roll is a patchwork of two releases' credits (ours, the exe image's map; fixed 2026-10-20 by DIV-0148)

**Found:** 2026-10-20, rendering the ending for the owner's call on the
roll ([`exe-gap-round.md`](exe-gap-round.md) 3): the chapter-jump save
`ending17` run to "Fin" with a shot every 300 frames, on the PC install and
on a US disc-only install (scratch `C:\Users\kerok\bof3x-runs\away1020`;
`analysis/shots/ending-roll-1020/`, local).

**Established:** the roll's text and steps are `Scena17_EndSteps` and the
strings after it (`0x66219C..0x662AEA`, read by `Scena17_DrawLine`). The PC's
table equals the Japan disc's `SCENARIO/SCENA17.EMI#0` byte for byte. The
Western discs re-lay it: the job titles reworded ("Engine Design" for
"System Planner"), the Japanese naming contest and the "Helper / Bug Checker
/ And Thanks For All" lines gone, "Assistants", "Testing" and "Cooperation"
added, and on the US disc a localization staff block. The region is
byte-identical on the European, French and German discs and different on the
US disc: the roll differs by release, not by language. The PC port shows the
Japanese roll under every language (en-US against the original: 82 of 84
frames pixel-equal, the other two the fade into "Fin").

The map places the stretches of the US roll that happen to match the PC's
and the rest stays a gap (zero), so a US disc-only install draws a mix:
"Engine Design" beside "Sound Program", "Battle" with no "Planner",
"Packag", an empty frame where the PC has the naming contest, "K I" for
"K IWAO". The European builds have the same shape.

**Where to start:** the owner's call between the PC's (Japan's) roll on
every install and each release's own; for the second, the Western table is
one contiguous run in `SCENA17.EMI#0` at the same offset on every Western
disc, and the question is whether `Scena17_DrawLine` and the step table read
it through offsets the PC's code fixes (the exe-gap round, section 3).

**Fixed 2026-10-20** (the owner: "let western installs use their own roll",
then "let's have the roll track with localization"): DIV-0148. A layer built
from a US or PAL disc carries its release's roll as kind 17
(`tools/exe_roll.py`, `tools/loc_build.py`) and `Scena17_ScrollRoll` reads its
lines from there, on a PC install and a disc-only one alike; the US and EU
disc-only installs and a PC install under en-US roll their release's credits
to "Presented by CAPCOM" and "Fin".

## D324 — Enemy sound cues on programs 3..7 take their pitch from the wrong tone (Capcom's; the PC plays the intended pitch)

**Seen:** not seen in play; read 2026-10-21 (`sound-import.md` 10.1).

**Established:** `SE_CueSetup_Bank*` looks a program's tone block up in a four-entry table,
`{0, 0x200, 0x400, 0}` (JP `0x80182B58`, the tone-offset table straight after it). A cue naming
program 3 or above lands inside program 0, `program - 4` tones up. The handler takes the key
note, fine tune, volume and pan from that slot. The volume and pan are overwritten for these
panned cues (`SsUtSetDetVVol`, `0x17FF`). The sample, envelope and centre note come from the
right tone, through libsnd's own addressing.

On the JP disc 2,000 bank-6 records name programs 3..7, and 290 of them (566 voice words, 68
files) play a sound. 298 of those words differ in pitch from the intended tone:

| Pitch change | Words | Examples |
|---|---|---|
| about ±50 cents | 290 | most of the affected sounds |
| +2.5 semitones | 4 | `ENEMY027` cue `0x60C` (Roach), `ENEMY052` `0x60C` (FireSlug) |
| −26 semitones | 2 | `ENEMY027` `0x60D` |

The same table, with the same 28 references, is in every boot EXE (JP, US, EU, FR, DE), so it is
Capcom's. The PC's WAVs are pitched from the named tone (`tools/vag.py` matches them 901/901), so
the port never had it. Renders of the worst cues, PC against PSX, are in
`C:\Users\kerok\bof3x-runs\cue_pitch\wav`.

**Status:** Capcom's, absent from the PC. Nothing to fix. If the effects move onto the SPU model,
reproducing it would need its own entry.

## D325 — The Holy Mantle brings encounters sooner in area 83 (Capcom's)

**Seen:** the owner's long-held suspicion that the Holy Mantle "seemed to not work right";
found by reading the code 2026-10-21.

**Established:** `Field_EncounterThresholdRoll` `0x52FEB0` doubles the encounter threshold for
accessory `0x15` in a byte (`shl bl, 1`). Zone 1 (base 120, limit 60) is the only zone whose
threshold can pass 127. Its 120..151 wrap to 0..46: seven rolls in ten land on 2..46 steps
instead of 120 or more, and one in 32 on 0, no encounters until the next roll. Only area 83
(いせき2) lists zone 1 in its own records (`Area_StateRecords` `0x668D80`, x 17..78, z 45..98).

`Area_ZoneAt` has no end test, so area 82's leader, on a cell its two records miss, would read
on into area 83's list. Whether such a cell is reachable is not checked. The JP twin `0x801B4CC4`
wraps the same way.

**Fixed 2026-10-21** (the owner): DIV-0153 caps the doubled value at 255.

## D326 — Under `ja-JP` the nue route's save draws its party names as wrong glyphs (the recipe save's; not the reading row's)

**Seen:** 2026-10-22, the reading-row round's group RR, on its contact sheets
of the `cutsceneAndNue` route under `BOF3X_LANG=ja-JP`
(`C:\Users\kerok\bof3x-runs\rr_rr\shots\sheets\furigana_4x3.png` and its
16:9 twin, outside the tree): a speaker's name inserted by `0x04 nn` draws as
a run of unrelated glyphs.

**Established:** the plain messages of the same route show it too, with no
reading-row layer, so it is not DIV-0155's. The save the recipe loads holds
names written under another language; the box draws the record's bytes as
they are, through the Japanese font. A save started under `ja-JP` would hold
names in its own codes; that was not run.

**Status:** open, a property of the recipe's save, not fixed. A capture
meant for the owner's eye under `ja-JP` wants a save named under `ja-JP`.

## D327 — On a one-pair page the next-page arrow sits on the text row's lower edge (the original's rule, met by DIV-0155's layout)

**Seen:** 2026-10-22, group RR, frame 2 of the furigana sheet: a one-pair page
whose text row runs long puts the arrow on that row's lower edge.

**Established:** `MsgBox_State3Arrow` places the arrow at the last row's pen
y + 12, the original's rule, kept. A one-pair page's text row is text 1, at
pen y 9 ([`reading-row.md`](reading-row.md) 3.3), 9 units below where a plain
page's first row sits, so the arrow lands against it; with two pairs the last
text row is at 28, the plain page's third row, and the arrow lands where the
original's does.

**Status:** left as the original's rule, for the owner's eye in play. A fix is
a placement rule for annotated pages (for example the arrow at the two-pair
position on every annotated page), which would amend DIV-0155.

## D332 — Under a second-language layer a three-row page re-broken into two runs a speaker's name into the speech and can fill a row to the frame line (ours, DIV-0156's re-break; fixed 2026-10-22, group NR)

**Seen:** 2026-10-22, group OS, the opening scene under `ruby-en-US-ja-JP`
(`opening_en-US_over_ja-JP.png`, stops s02 and s03, in
`C:\Users\kerok\bof3x-runs\rr_os\sheets\`): pages that in Japanese are a name row and two text
rows show the name and the first text row on one row, and on one page that
row reaches the box's right frame line.

**Established:** the speaker's name there is written as glyphs, not as a
`04 nn` insert, so `SecondComposer.rebreak_page`'s rule that keeps an
insert-only name row ([`reading-row-second.md`](reading-row-second.md) 2,
rule 4) does not see it, and the three rows are re-broken into two of up to
192 units - which a row of sixteen 12-unit kana fills exactly, the last
glyph against the frame of a box whose inner width is those 192 units. Both
are the rule as built, not the stop's: the stop only made these pages
composed (they were plain as effect messages before).

**Status:** fixed 2026-10-22 by group NR, on the owner's call ("in the
original script, the name gets its own row - just go ahead and do a page
break instead of fitting it onto one page"): the second mode no longer
re-breaks rows at all - every authored row is kept and a three-row page is
two pages ([`reading-row-second.md`](reading-row-second.md) 2, rule 4;
DIV-0156's amendment). The name row is its own row again and no composed
text row is wider than its authored row (`check` stops the build
otherwise; 0 in all 20 pairs). Seen fixed at the same stops:
`opening_en-US_over_ja-JP_rows_kept.png` in
`C:\Users\kerok\bof3x-runs\rr_nr\sheets\` (s02..s05, s08..s09).

## D328 — The furigana layer's readings are not proofread

**Seen:** 2026-10-22, group RR, on the shrine frames of the nue route: 道祖神
is laid as どうそ over 道祖 and かみ over 神 (the dictionary's split), where
the word is read as one, どうそじん.

**Established:** the readings are SudachiPy's (`sudachidict_core`) with
`data/furigana/readings.toml`'s overrides first
([`reading-row-builder.md`](reading-row-builder.md) 3); nobody has proofread
them, here or in the sibling (2026-09-12). `--review FILE` writes every
reading placed with its context, `--ambiguous FILE` the surfaces the lexicon
reads two ways. Measured on the owner's request: Sudachi's core and full
dictionaries differ in 253 of 27,976 composed messages, nearly all compound
width ([`owner-review.md`](owner-review.md), 2026-10-22).

**Status:** open; the readings follow-on group the owner decided on
2026-10-22 (the lexicon, a fugashi + UniDic `--compare`, and an agent pass
over the review list, each disagreement adjudicated into `readings.toml`)
is the fix path ([`HANDOFF.md`](HANDOFF.md)). Wrong readings are text, not
behaviour: each correction is a sidecar rule, re-issued in the mod zip
(DIV-0157).

## D329 — A route recorded before the reading row needs one more press for each page a layer splits (tooling)

**Seen:** 2026-10-22, group RR: `tools/recipes/dolphinBoss.txt` under
`ruby-furigana-ja-JP` does not reach the dolphin fight. Its save opens on the
world map; the Fairy Tear prompt's first message is one the builder splits
(a page added), so the recording's third press lands on the yes/no list as a
cursor move, the box is answered "no", and the route wanders the field to its
end (`C:\Users\kerok\bof3x-runs\rr_rr\shots\f_dolphin_w0`, `_w1`).

**Established:** not a renderer fault: an added page is DIV-0155's (and
DIV-0156's) accepted cost of a split, one confirm press. With one press added
after the recorded third press (`until` the yes/no list is up, `wait 20`,
`hold circle 5`, then the recording's own `wait 74` and the rest unchanged)
the route runs to its end, recipe frame 14,980, the coast scene's long
dialogue and the event battle with readings at 4:3 and 16:9. That copy is
`C:\Users\kerok\bof3x-runs\r_ruby\rr\recipes\dolphin_fix.txt`, outside the
tree. Every route recorded before the reading row is in the same position
under a layer that splits one of its pages.

**Status:** open, by design. `tools/recipes/` stays as recorded: the routes,
the state hash and the self-tests run **without** a reading-row layer, and a
recording made with one would not run without it. A route under a layer
wants its own recording, or the fixed copy's press added by hand
([`HANDOFF.md`](HANDOFF.md) traps).

## D331 — `setup` on an existing install dropped its saves (ours; fixed 2026-10-22)

**Seen:** 2026-10-22, the owner's first install of the reading-row round:
the launcher listed no saves. The thirteen `BISLPS0?.DAT` sat in the home
folder itself, where the layout before 2026-10-15 played from, and the
day's `setup` had laid the game out in `H\game\` without them.

**Established:** `player_sources.run_setup` builds `game.new` from the
cache and the PC folder (whose saves are deliberately not copied,
[`install-sources.md`](install-sources.md) 3), then `swap` renames the
previous `H\game\` to `game.old` and removes it - so a re-run on an
install that had been played lost every save and quicksave in it, and the
older layout's saves one folder up were never seen.

**Status:** fixed 2026-10-22: `carry_saves` runs before the swap - the
previous game folder's `BISLPS??.DAT` copied into the new one, the home
folder's moved down, nothing overwritten, each line in `setup.log`.
Checked on a temp home (two carried, one moved, one left). The owner's
saves were copied into `H\game\` by hand the same day.

## D330 — The `orig3` state-hash pairs are stale at the reading-row round's base (tooling, not the round's)

**Seen:** 2026-10-22, group RR's state hash on the ten routes with no
reading-row layer (`BOF3X_OPT=none BOF3X_LAYERING=0`, PR #105's regenerated
recipes, the tip's skip list): nine of the ten fail against
`cn_<route>_orig3` / `_orig3b`. Runs and checks:
`C:\Users\kerok\bof3x-runs\rr_rr\statehash\`.

**Established:** the base tip (`localization/second-script` at `1fb979cf`,
without a line of the round's renderer, built from `git archive`) fails the
same nine routes, with the same page counts on eight of them (whelpBoss one
page more), and shop besides (the sound page and four VRAM pages at ticks
770..775, where ours is identical in two runs).
The differences arrived before the round, with changes already ledgered and
with the regenerated recipes. Ours against the base directly
(`statehash.py diff`): every page that differs also differs between two runs
of one build - the sound page `0x7DE000`, `0x6C3000`
(`D3d_BackMaterialHandle`...), the VRAM shadow and `0x937000` at ticks 2..4,
and shop's own second base run wandering on 13 pages. So the round changes
no game state with no layer (DIV-0155's identity), and the references no
longer measure the tip.

**Status:** fixed 2026-10-23: the pairs re-recorded as `cn_<route>_orig4` /
`_orig4b` at `main` `b13eb30a`, ours at that tip differing only by ledgered
changes. [`state-hash.md`](state-hash.md) 6 carries the dated line.

## D333 — A 15-bit direct-colour page draws nothing: every texel is discarded, so Amalgam's melt is still missing after DIV-0142 (Capcom's; fixed 2026-10-23 by DIV-0159)

**Reported:** issue #111 (2026-10-23, after v0.1.0-alpha.5): "Just beat
him again and he didn't stretch like he was supposed to." Seen the same
day on the owner's route `tools/recipes/ropeAndAmalgam.txt` (save
`ropeAndAmalgam`, recipe frames 7660..7720): the boss is gone on the frame
of DIV-0142's capture and nothing is drawn after it.

**Established** (2026-10-22..23: the route with temporary logging, then
`Tex_Convert16` read, then a run of Capcom's code):
- The capture runs and the page holds the picture: on every melt frame
  about 19,500 non-zero cells under (0x340, 0x100). The melt binds page
  0x1D in colour mode 2 every frame, and the texture is built after the
  capture.
- `Tex_Convert16` (`0x5A9D2F`) packs red, green and blue through
  `Gfx_PixelFormat`'s shifts and masks and never ORs its opaque bits
  (dword 7, `0x7DED7C`). `Gfx_ConvertRow` (`0x59EBB0`; `or edx, [0x7DED7C]`
  at `0x59EC77`) does, for palettes. Those are the binary's only reads of
  `0x7DED7C` but one, in `0x5AA624`, which nothing calls (capstone).
- The page format is A1R5G5B5, opaque bits `0x8000`, under the port's
  backend and under Capcom's own set-up alike (`Display_Setup` `0x5A5160`
  with DirectX 6; its ranking prefers that format,
  [`display-setup.md`](display-setup.md) 6). So every direct-colour texel
  has alpha 0, and the colour-keyed draw discards all of them.
- Capcom's build does the same: with Capcom's set-up,
  `D3d_BuildPageTexture`, `Tex_Convert16` and all twelve `BossAmalgam_*`
  functions, and DIV-0142's capture filling the page, the boss vanishes and
  no melt is drawn (`sheet_capmix.png` in
  `C:\Users\kerok\bof3x-runs\amalgam_111_orig\`). In Capcom's own build this
  hid behind D317's empty page.

Other direct-colour drawers (a read-only sweep, 2026-10-23): Mikba's death
(`BossMikba_DrawQuad` `0x43E290`), Weretiger (`Weretiger_DrawSprite`
`0x4B5240`) and effect kind 0x9D (`EffectKind9D_DrawPulse` `0x48A010`,
`_DrawGlow` `0x48A160`) draw nothing for the same reason; no recorded
route reaches them. Kind 0x51's column and ring, Depress's vortex and the
ending's frozen frame (`Scena17_DrawFade`) take their texels from a
DIV-0125 grab, which the shader samples ahead of the texture, so they draw.
Kind 0x9D's page is not cleared before its draw (`EffectKind9D_Start` sets
`+0x24 |= 8` only), so after the fix it draws whatever that page holds in
the field. By reading the PSX would too; not seen.

**Fixed 2026-10-23:** DIV-0159.

## D334 — A semi-transparent textured primitive blends every texel: the rooftop rope draws at a quarter of its brightness (Capcom's port; fixed 2026-10-23 by DIV-0160)

**Seen:** 2026-10-23 on the owner's route `tools/recipes/ropeAndAmalgam.txt` (save `ropeAndAmalgam`, recipe
frames ~264..383): the rope over the McNeil rooftop's black sky (area 28) draws at about a quarter of its
PlayStation brightness, median (20, 16, 6) against the twin's (72, 56, 24).

**Established** ([`stp-census.md`](stp-census.md); HANDOFF item 00):
- On the PlayStation a semi-transparent textured primitive blends only the texels whose colour has bit 15 set;
  a texel with it clear draws opaque. The rope's 12 quads are `0x2E` (blend 3, background plus a quarter of the
  texel) on both machines, and its CLUT `78C3` has bit 15 clear in every entry (the twin's runtime), so the
  PlayStation draws it opaque.
- The port drops bit 15 when it converts a colour to a texel (`Gfx_ConvertRow` `0x59EBB0`, `Tex_Convert16`
  `0x5A9D2F`) and blends the whole primitive by its tpage (`D3d_SetBlend` `0x59FCA0`).
- The census over five routes: besides the rope, only the page-15 text palettes (`7800`, `7807`, additive)
  are blended where the PlayStation draws opaque - 34,858 of 123,467 semi-transparent textured draws; the
  twin's savestates have bit 15 clear there too. No primitive mixes the two kinds of texel.

**Fixed 2026-10-23:** DIV-0160, the bit-15 round ([`stp-blend.md`](stp-blend.md)): the textures carry bit 15
and a blended draw blends only its bit-15 texels. The rope's pixels go from a median of (20, 16, 6) to
(81, 62, 25), against the twin's (72, 56, 24); the page-15 text draws in its palette colour. The owner, the
same day, on the text: "much closer to original now to my eye". They had taken the brighter text, seen on
their first run of the Chinese port, for fallout of its text-memory expansion.

## D335 — `Battle_ScaleDamage` answers its full width where the PlayStation keeps a signed 16-bit result (the port's; latent)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`BATTLE.EMI#3` `0x801DCD50` (JP `0x801DCD18`, every masked instruction word
identical) against the PC's `0x446430`.

**Established:** the PlayStation ends the scaled damage with `sll 8; sra 16`:
the answer is the low sixteen bits of `damage >> 8`, sign-extended. The PC
ends with `sar eax, 8` (`0x44652C`) and answers the whole value; ours does
the same. The two agree below 32,768. Above it the PlayStation's answer wraps
negative where the PC's stays positive. The one caller is
`Battle_BaseDamage` `0x4462B0` (the only `call 0x446430`, at `0x4463CF`),
which answers the value whole to `Battle_CalcDamage` `0x445CF0` (its two
calls, `0x445E1B` and `0x44603D`; nothing else in `.text` calls either).

**Moot (fix wave 1023, group BT, 2026-10-23):** every reader downstream
uses only the low sixteen bits, which the two answers share. In
`Battle_CalcDamage`: the weapon doubling (`add edi, edi`) keeps them; the hit
functions `0x446110` / `0x4461B0` answer the amount as `mov ax, word ptr
[esp+4]` (`0x4461A8`) or 0; the status tests are `test di, di`; the flag-bit-1
roll reads `movsx eax, di` (`0x446013`); the second `Battle_BaseDamage` call
is read as `movsx eax, ax` (`0x446042`); and from step 7 the amount is a word
clamped to +-9,999. So no damage, hit or miss can differ, and no repair was
built (DIV-0161 went to D336 instead). Whether an attack can reach 32,768 is
moot with it.

**Status:** moot; the port's; not repaired (nothing a player could see).

## D336 — Two battle draws take their coordinates unsigned: a negative x or y lands near 65,536 (the port's; latent)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`0x801DA078` and `0x800B1F04` (JP `0x801DA040` and `0x800AEF04` by the map,
words identical) against the PC's `0x444C40` (the battle window cell) and
`0x599A00` (`GeneWin_DrawCursorBox`).

**Established:** the PlayStation stores the x, y (and the box's width and
height) as signed halfwords. The PC masks each with `0xFFFF` before `fild`
(`0x599A6E..0x599A81`, and the same shape in `0x444C40`), so a negative
coordinate becomes a float near 65,536 and the primitive falls off-screen
where the PlayStation draws it clipped. The cell's callers (`0x442FA0`,
`0x443F60`) are not known to pass a negative coordinate. The cursor box is
drawn by `GeneWin_DrawList` (`0x5990E0`) at the window's x + 5, and that draw
runs on every frame of the gene list's slide-in and slide-out (`0x599020`,
`0x599050`, x to -0xA5): if the box is up then (step 6, not sub-step 5), the
PC drops it while x + 5 is below 0, where the PlayStation slides it out with
the list. Not seen.

**Status:** fixed by DIV-0161 (`BOF3X_ORIGINAL=BattleDrawSigned` for the
original), fix wave 1023; the port's.

## D337 — The action effect's preload does not key off voices 16..23, where the PlayStation does (the port's sound layer; not heard; fixed by DIV-0167 for the looping voices)

**Seen:** not heard; found 2026-10-23 by reading the US PlayStation's
`0x801D3E80` (JP `0x801D3E60`, words identical) against the PC's `0x42FD20`
(`BattleAction_EffectPreload`).

**Established:** before it resolves the ability or item row and starts the
effect's load, the PlayStation calls `SsUtKeyOffV` on voices 16..23, cutting
the effect voices of the previous action. The PC's function has no such step
(its sound is the port's DirectSound layer), and nothing in ours cuts those
voices there either. Whether the previous action's sound can still be
playing at that moment, and so runs on under the next effect's, is not
measured.

**The PC's equivalent** (2026-10-23, group SN of the fix wave): the
PlayStation's loop is `SsUtKeyOffV` (`0x8016F10C`) for `s0 = 0x10..0x17`
(JP `0x801D3E68..0x801D3E84`; US `0x801D3E88..`, the same words but the
call's target `0x8016E794`), and voices 16..23 are every voice `SE_Play`
keys. The PC's cue words keep the voice in bits 8-12, the shipped banks name
only 16..23 (`sound.md` section 1), and `Sound_PlayEffect` files voice `v`
under channel `v` (less 23 from 23 on): channels 16..22 and 0. A channel
holds a buffer only for a looping voice; a one-shot's buffer is the bank
entry's and is not remembered anywhere a stop could find it.

**Status:** fixed by DIV-0167 (switch `EffectKeyOff`) for what the PC can
reach: the looping voices on those channels are stopped and forgotten at
the PlayStation's point. A one-shot still sounding plays to its end, where
the PlayStation's key-off would release it; left as the PC's. Not heard; a
listen for the owner.

## D338 — `DragonCmd_StoreAsk` reads the prompt's answer as a dword that every writer stores as a word (the port's; latent)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`0x800A632C` (JP `0x800A332C` by the map, words identical) against the PC's
`0x451160`.

**Established:** the PlayStation draws the hand at `answer * 0x22 + 0xD5`
with `answer` a `u16`. The PC reads the dword at `0x904B74` (`0x4511F2`)
while its writers store a word (`0x451042`, `0x4511EA`, and the level-up
banner's `0x431C70`, whose `sprintf` at `0x431C77` reads the dword too, but
masks it with `0xFFFF` at `0x431C7C`, so it reads the word). The hand lands
elsewhere only if the halfword at `0x904B76` is not 0; no instruction in
`.text` names `0x904B74..0x904B77` but the six above (a scan, 2026-10-23), so
it is the `.data` tail's 0.

**Status:** fixed by DIV-0162 (`BOF3X_ORIGINAL=StoreAskWord`), fix wave 1023:
the hand reads the word; the level-up reader needed nothing. The port's;
invisible either way unless `0x904B76` gains a writer.

## D339 — Four propagated PlayStation pairs are unrelated functions, and two twin citations were swapped (tooling; the citations fixed 2026-10-23)

**Seen:** 2026-10-23, the battle twin-diff of 377 PC functions against the
US PlayStation's matched twins.

**Established:**
- `analysis/pairs_propagated.json` pairs `0x433810` with `0x800A83F8` (its
  twin is `0x801E72F4`, handler 0 of `0x801E72A8`), `0x435CF0` with
  `0x801E2948` (a random chooser between two pickers; ours is the picker by
  ability flags), `0x45BFA0` with `0x801D42B4` and `0x52D140` with
  `0x801E10C8` (unrelated bodies). `0x44F030`'s call-anchored twin
  `0x800A2D70` is `Battle_StatusResistRate` `0x44F770`'s; its citation twin
  `0x800A2EF0` is right.
- `rest_4a.cpp` cited `Battle_RandomLiveMember` `0x452EB0` and
  `Battle_RandomLiveEnemy` `0x452F10` with each other's twins; the comments
  now carry the right ones (members JP `0x800A6408`, enemies `0x800A64BC`),
  and DIV-0074's PlayStation line is answered from the right one.

**Status:** tooling; the citations fixed; the pairs file is regenerated
output and keeps its four until the pairing is re-run with a size check.

**The second round (2026-10-23, later; 955 takeovers of the field, menu, shop,
start, scenario, area and boot targets).** Of 151 `mispair` verdicts, 104
were the map's own: a pair found in one JP area overlay, or a citation from
one area's block, was matched to the same address in every area overlay the
decompilation has, where each area keeps a different function.
`region_twin_map.py` now matches a propagated pair to its own overlay only (the
boot image to the executable and GAME.EMI) and an area file's citation to its
area's overlay, and takes the first `PSX 0x...` on a marker's line (527 more
citations mapped). The rest are the pairs file's or ours:
- unrelated bodies: `0x573090` with `0x801C5874`, `0x42EF50` with
  `0x801D2564`, `0x442080` with `0x801E0680`, `0x445640` with `0x801DB510`,
  `0x496B60` with `0x8014ECEC`, `0x42FAF0` with `0x801D39F8`, `0x4363E0`
  with `0x801E36F8`, `0x441550` with `0x801DF478`, `0x532860` with
  `0x801B7988` and `0x801B7E74` (its citation `0x801C0114` is right),
  `0x51AD60` with `0x801BC3B8` (its twin is near `0x801BC000`), `0x56D8B0`
  with `0x801A7C2C`, `0x56C830` with the stub `0x801F8398`, `0x581ED0` with
  `0x801D1B00`, `0x513AD0` / `0x513D20` / `0x513E80` with area bodies;
- neighbours and swaps: `0x5253E0` with `0x801B6BC8` (step 1; ours is step 0,
  `0x801B6AB0`), `0x535490` / `0x535D60` with `0x801C3A18` / `0x801C476C`
  (the four-cell tests; the wide ones are `0x801C3B78` / `0x801C48FC`),
  `0x535830` with `0x801C4040` (the plain way test; ours is `0x801C41D0`),
  `0x589110` / `0x589160` crossed with `0x8014D54C` / `0x8014D5F0`, and
  `0x4680F0` / `0x468210` crossed with `0x801D6B28` / `0x801D6CB0` (JP
  `0x801D6B70` / `0x801D6CF8`) - the crossing that hid D340;
- `0x4DF820` with `0x8015D300` is not a mispair but a dropped call: the PC's
  bare `ret` stands where the PlayStation stops, re-volumes and replays a
  sequence (D345);
- two of our citations were one function off: `0x403310` cited `0x801F32C8`
  (its body is `0x801F3290`) and `0x403330` cited `0x801F3348` (its body is
  `0x801F32C8`); both fixed. `0x5172F0`'s "no PSX twin paired" is answered
  (`0x8019A368`). `0x435C40`, `0x435EF0` and `0x582090` cite battle or
  game addresses that the map can only place in the shop and scenario
  overlays, where another function sits: the map's limit for a citation
  outside an area file, not a wrong citation.
- `0x488EF0`'s pair with area032's `0x801F35B4` is doubtful: the US body
  releases two tints where ours also clears the sprites, and its spawner
  moves x and y where ours moves z, so the area032 effect is a relative of
  kind 0x86, not its twin; no verdict.

## D340 — The fishing panel's cast marker and range bar sit 3 and 19 pixels right of the Japanese disc's (the port's; cosmetic, not seen)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`AREA030.EMI#4` `0x801D6CB0` against the PC's `EffectKind0F_Child4Move`
`0x4680F0` (the pairing had crossed it with `0x468210`; D339).

**Established:** the function ends in two draws, the cast marker
(`0x465D90`, four arguments) and the range bar (`0x465E50`, six). The PC's
words at `0x4681E0` pass `(0x7E, 0x5C, 0, 1)` and `(0x8F, 0x69, 0x20, 0, 1,
0)`; ours passes the same. The Japanese disc (`0x801D6EC8..0x801D6EF8`)
passes `(0x7B, 0x5C, 0, 1)` and `(0x7C, 0x69, 0x20, 0, 1, 0)`: the same
rows, the marker 3 px and the bar 19 px further left. The US disc re-laid the
panel: the bar first at `(0xCE, 0x5C, ...)`, then the marker at `(0xE3, 0x69,
0, 1)`, the two rows swapped. The port's numbers are its own, not either
disc's; the sibling `0x468210` (the cursor panel) and the other children
agree with both discs.

**Status:** latent; the port's; cosmetic. A capture of the fishing panel
beside the Japanese disc's would show whether the port's shift was a
correction for its own art or a slip.

## D341 — Two sprite-record writes to `+0x70` keep bytes `+0x71..+0x73`, where the PlayStation stores a whole word and zeroes them (the port's; latent)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`GAME.EMI#0` `0x801A6C98` (JP `0x801A7CA0`) and the executable's `0x8014DD3C`
(JP `0x8014DC30`), every masked word identical to the JP, against the PC's
`EventOp_Bx` `0x57B130` and `Sprite_SetAnimationBank` `0x589590`.

**Established:** the event op places an extra object and copies byte 2 of
the shared block into the sprite's `+0x70`: the PlayStation stores it as a
32-bit word (`FIELD(s32, tail, 0x70) = byte`), so `+0x71..+0x73` become 0;
the PC stores one byte (`mov byte ptr [eax + 0x70], dl` at `0x57B245`). The
bank setter does the same: the PlayStation stores the bank byte over the
whole `u32`, the PC `and`s the word with `0xFFFFFF00` (`0x58962D`) and `or`s
the byte in, keeping bits 8..31. Ours does as the PC. The two agree unless
something reads `+0x71..+0x73`, or `+0x70` as a word, after either write.
One such reader exists (fix wave 1023, a `.text` operand scan): area 51's
`Area51_MemberAtObject` (`0x409CE6`) pushes the whole dword as
`Party_MemberAt`'s margin; the other 31 dword reads mask it. No PC writer of
non-zero upper bytes was found, so the difference is not expected in play.
A third store keeping the upper bytes, `0x533C64..0x533C87`, is not paired
and stays Capcom's.

**Status:** fixed by DIV-0164 (`BOF3X_ORIGINAL=SpriteReachWord`), 2026-10-23;
latent before; the port's.

## D342 — `MoveScript_ObjectKind` answers the pointer itself for an object outside every array, where the PlayStation answers 0 or 3 (the port's; latent)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
executable `0x8015C148` (JP `0x8015CB38`, every masked word identical)
against the PC's `0x57C840`.

**Established:** both classify `Sprite_Current`: 2 for the leader's record,
1 for the three party records, 0 inside the object pool, 3 inside the second
array. For a pointer in none of them the PlayStation answers `work <
D_80147A58 ? 0 : 3`; the PC falls to `0x57C891` with `eax` still the pointer
(`cmp eax, 0x802000; jb 0x57c891`). Ours does as the PC. Whether any caller
hands it such a pointer is not known. It does not matter (fix wave 1023): all
nineteen call sites (`0x4090E8` .. `0x57C9B4`, every `call 0x57C840` in
`.text`; no pointer to it in the image) compare the answer with 1 or 2 only,
and the pointer, 0 and 3 all fail both; `0x577AFB` stores `al` only on the
equal branch. The JP twin's test is `work < 0x80147A54 ? 0 : 3` with no
range checks; on the PC it would read `< 0x802000`.

**Status:** moot, not repaired (no reader tells 0, 3 and the pointer
apart); the port's.

## D343 — The inn's night neither pauses nor resumes the sound sequences, where the PlayStation pauses four and replays them (the port's sound layer; not heard)

**Seen:** not heard; found 2026-10-23 by reading the US PlayStation's
`SHOP.EMI#0` `0x801D446C` (JP `0x801D447C`, every masked word identical)
against the PC's `InnNight_Restore` `0x57FCB0`.

**Established:** the PlayStation's night sequence pauses four SEP slots at
`0x801D41EC` (US; `SsSepPause`) and, in the restore state, calls
`SsSepReplay(0, i)` for each of the four before the party's restore, the
next state and the message. The PC's `0x57FCB0` has neither the pause nor
the replay (its calls are the title box, the transition test,
`Party_RestoreAll`, the message); its music runs on the stream path. Ours
does as the PC. What is heard through an inn stay on each machine is a game
fact, not measured.

**Read again** (2026-10-23, group SN of the fix wave, the US disc's
`SHOP.EMI#0` disassembled): the pause is night state 0's end - on the
counter's last frame, after `Transition_Start(0)` (`0x8014ECAC`), a loop
over slots 0..3 stores `0x8016B344(0, i)` (a playing test) at `0x801E6100 +
2i` and calls `0x8016B968(0, i)` (`SsSepPause`) for each that plays. Night 2
plays the jingle through `0x801636A0(0, 1)` (the XA stream, beside the
paused sequence), night 3 waits on `0x80163EA0`. The replay, `0x8016BB6C(0,
i)` for each slot recorded as playing, comes **last** in the restore state,
after `Party_RestoreAll(1)` (`0x801D68D0`), the answer byte, the repeat
latch and the message - not before them as the paragraph above has it. So
on the PlayStation the area's song stops as the screen starts to fade and
resumes where it stopped once the fade in has finished.

The PC differs in more than the two calls: its jingle (`OYASUMI`, kind 0)
plays through `Music_Start` on the music's own stream (`Sound_LoadStream`
`0x587910`), which releases the area's stream, so there is nothing left to
pause; `Music_Track` stays the area's track, so a later `Music_Play` of it
does nothing. Yet the town's song is heard again after the night
([`wav-dump.md`](wav-dump.md) 4c, 99 s on `shop.txt`), by a path not read.
A pause / resume repair needs that path first, or the song could start
twice. **Not built** in the fix wave: the next step is
`BOF3X_MUSIC_LOG=1` on `shop.txt` (the route reaches the inn at recipe frame
2647), which names every music call through the night and after.

**Status:** latent; the port's sound layer (D337 and D345 are of the same
class); the owner's ear.

## D344 — `FieldSave_Confirm` skips the PlayStation's two-frame message step before the write (the port's; cosmetic)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
`SHOP.EMI#0` `0x801D50C0` (JP `0x801D50D0`, every masked word identical)
against the PC's `0x57FF80`.

**Established:** on a "yes" the PlayStation goes to step 0x13, which shows
system message 0x99 for two frames (counter 2) and then moves to the old
step + 1, the write. The PC goes to step + 1 at once (`inc al` on `0x929F02`
at `0x57FFE3`); the write step (`FieldSave_Write`, `0x580010`) shows message
0x99 itself, so the message is drawn either way, two frames later and under
a write that, on the PC, is a file write rather than a card write. Ours does
as the PC.

**Status:** latent; the port's; cosmetic.

## D345 — Three positional-sound hooks are empty on the PC: the movement op `B8`, the overlay queue's bit-3 voice update, and `Sound_PlayById`'s level and distance handling (the port's sound layer; not heard)

**Seen:** not heard; found 2026-10-23 by reading the US PlayStation's
`GAME.EMI#0` `0x801A96E4` (JP `0x801AA6E4`) and the executable's `0x8014D4E0`
(JP `0x8014D3D4`) and `0x8015D71C` / `0x8015D7C8` / `0x8015D820` (JP
`0x8015E10C` / `0x8015E1B8` / `0x8015E210`), every masked word identical,
against the PC's `0x577280`, `0x5890E0` and `0x587900`.

**Established:**
- the movement script's `B8` calls `0x8015D300(b1, b2, b3, b3)` on the
  PlayStation - a sequence stopped, its volume set (0 while the duck gate
  `D_80144954` is set), and replayed - and advances the cursor three; the
  PC's `B8` calls `0x4DF820`, a bare `ret`, and advances the same
  ([`movement-script.md`](movement-script.md) 1 named the empty call);
- `Sprite_QueueOverlay` with bit 3 of the sprite's `+0` set calls
  `0x80161454(index)` on the PlayStation, a voice volume and pan update by
  position; the PC jumps to the same bare `ret` (`0x589109`);
- `Sound_PlayById` on the PC plays the effect once (`0x587740`) and ignores
  its other arguments; the PlayStation's three functions set the volume pair
  from a level table, repeat a cue gated by its distance from a task, and
  pack voice numbers into `+0x70`.
Ours does as the PC in all three. These are the port's replacement of the
PlayStation's positional sound by flat effects; which scenes used `B8` is
not known.

**Read again** (2026-10-23, the sound-layer round's group SP, the US disc's
executable and `GAME.EMI#0` disassembled; [`DIVERGENCE.md`](DIVERGENCE.md)
DIV-0171..0173):
- `B8`'s `0x8015D300(b1, b2, b3, b3)` is the music's own start: every group
  is SEP access 0 (US `0x80148FC0`, sixteen zeros), b2 a sequence of the
  loaded song file (the track table US `0x80181EB8`: every track group 0,
  sequence 0..3 of its file), b3 the level - stopped, set (0 under the gate
  `0x80144954`, never found set) and played from its top, no fade.
  `Music_Play`'s twin starts its tracks through it at level 0. Two shipped
  scripts use it (`tools/movement_scan.py`'s decode): `B8 00 00 64` in area
  142's `+0x18` script 2 and area 174's `+0x10` script 35.
- `0x80161454` takes no index: it updates the voices packed in the
  scratchpad work record's (`Sprite_Current`'s) `+0x70` bits 8..27, by the
  screen point under bit 0x1000 of `0x8014625A` (the PC's
  `Field_ScriptFlags2` `0x905BA4`) and by the distance from the kind-2
  object otherwise, panned by screen x unless the options' Mono; a voice
  keyed off is dropped from the word, and bit 3 cleared when none is left.
  The bit and the word are set by `0x8015D71C`, the first of the three, so
  on the PC, whose `Sound_PlayById` does neither, the bit-3 path is never
  taken.
- The PC's `Sound_PlayById` stands for all three: `0x8015D71C` at every call
  site but four (a jal scan of the US disc: 503 calls in 176 overlays),
  `0x8015D7C8` from op `BC` and `SCENA01.EMI`'s three, `0x8015D820` from ops
  `BD` and `D1`.

**Status:** fixed by DIV-0171 (`B8`, switch `MoveB8Restart`), DIV-0172 (the
positional voice, `SpriteVoicePosition`) and DIV-0173 (the level and the
distance gate, `SoundLevelGate`), 2026-10-23; what the PC's records cannot
carry (the `+0x70` word and bit 3, `B8`'s sequences 1..3, the CD mix cells)
is not built, each named in its entry; the port's sound layer; the owner's
ear.

## D346 — `Music_Play(0xFF)` is not refused, where the PlayStation's twins return on 0xFF (the port's; latent; fixed by DIV-0168)

**Seen:** not seen in play; found 2026-10-23 by reading the US PlayStation's
executable `0x80161B24` and `0x80161C20` (JP `0x80162514` / `0x80162610`,
every masked word identical) against the PC's `0x587AE0`.

**Established:** both PlayStation twins test the selection against 0xFF and
do nothing for it. The PC's `0x587AE0` tests only `track == Music_Track`
(`0x587AEC`); for 0xFF while music plays it stores 0xFF as the track and
restarts the previous file through `0x587BA0`. Ours does as the PC. No
caller is known to pass 0xFF; the area pick's 0xFF goes through
`Area_Enter`'s own test.

**The test's width** (2026-10-23, group SN, read on the JP disc's
executable): both twins mask the selection to its low byte first (`andi
0xFF; beq 0xFF` at `0x80162530` and `0x80162630`). The callers that pass a
value rather than a constant: `Area_Enter` (tests 0xFF itself), the scenario
steps' `Music_Play(Music_Track, 8)` (a no-op for 0xFF either way, the track
equals the byte), and the movement script's `B4` / `BE` (`MoveScript_GroupB`
`0x577280`), which pass a script byte unchecked - the one way a 0xFF could
arrive. (`0x587BA0` in the paragraph above is the fade in that follows the
restart; the restart is `Music_Start` of what `Music_File` still holds,
looping, `Music_LoadFile` finding no file 255.)

**Status:** fixed by DIV-0168 (switch `MusicPlayRefusesNone`); latent.

## D347 — The port's stop is a ten-frame fade where the PlayStation cuts (the port's sound layer; the owner's ear)

**Seen:** heard in part - D318's reading named the PlayStation's immediate
stop in Ryu's dream; found as a class 2026-10-23 by reading the US
PlayStation's `AREA026.EMI#13` `0x801F3280` (JP `0x801F3290`, every masked
word identical) against the PC's `Area26_ResetCamera` `0x403310`, and the
area change's stops (`GameMode_Field` case 5, US `0x80197378`;
`GameMode8_Leave`, US `0x801985C8`).

**Established:** where the PlayStation calls `SsSepStop(0, 0)` (through
`0x8015DDF4`, JP; US `0x8015D404`) the port calls `Music_FadeOutStop(0xA)`
(`0x587B40`): the song fades over ten frames and then stops. The area change
is otherwise the same shape - the PlayStation fades through
`SsSepSetDecrescendo` (`0x8015DEE8`; 100 over 16) where the port fades over
16 frames, then both stop after the transition. Earlier readings here
([`mode-tasks.md`](mode-tasks.md) 3, [`field-modes.md`](field-modes.md) 4,
DIV-0092) called `0x8015DDF4` a restart of the area's track from its table;
it is the stop, and those lines are corrected. Ours does as the PC.

**Status:** the port's sound layer; cosmetic; the owner's ear decides whether
the cut is wanted back (a switch would be DIV-0132's shape).

## D348 — `Menu_DrawBox` and `Menu_DrawIcon8` take x and y unsigned: a box slid past the left or top edge draws its frame without its fill (the port's; fixed by DIV-0165)

**Seen:** not seen in play; found 2026-10-23 by the typed-record audit, reading
the PlayStation's panel records (x and y signed, floored at -170 / -150)
against our `WindowRecord`.

**Established:** `Menu_DrawBox` `0x57CF60` and `Menu_DrawIcon8` `0x57D360`
mask x and y with `0xFFFF` before the `fild` (`0x57D009` / `0x57D00F`,
`0x57D3A0` / `0x57D3BB`), where `Menu_DrawPiece` `0x57D860` sign-extends
(`movsx`); the PlayStation's draws (`0x801AF3F0`, `0x801AFA80`) store signed
halfwords and the GPU clips. A box slid off the left (`MenuWin_SlideOutLeft`
`0x59B440`, to x = -150, then `Shop_DrawMemberStats` `0x575430`) or the top
(`GeneWin_ChoiceSlideIn` / `Out` `0x598E10` / `0x598E50`, y through -7) became
a float near 65,536 and fell off-screen, so the frame pieces drew without the
fill on the slide's last frames.

**Status:** the port's; fixed by DIV-0165 (`BOF3X_ORIGINAL=MenuSignedXY`); the
owner confirmed the shops in play (2026-10-23).

## D349 — `Battle_Init` leaves the target-pick flag set from the last battle (Capcom's, fixed in the US build; latent; fixed by DIV-0163)

**Seen:** not seen; found 2026-10-23 by the region census: the US disc's
battle start (`BATTLE.EMI#3` `0x801D1228`) clears `0x801462EF` (JP
`0x801462EB`); the JP disc and the PC (`0x42E4A0`) do not.

**Established:** the PC resets `0x904AA5..0x904B97` a byte at a time but
skips `BattleE4_Picking` `0x904AAF`. The command steps (`0x446FF0`,
`0x447840`, `0x447F40`, `0x450510`) set it while a target is picked and clear
it on confirm or cancel. A battle started with it set would pulse the stale
target's tint (`0x4417A0`) and step the enemy gauge windows (`0x5974C0`) until
the first pick. No PC path leaving it set was found: a battle cannot end
during command entry, and a quickload restores the byte with the rest. The US
case is likely its soft reset, which the PC lacks.

**Status:** Capcom's (the JP build's), fixed in the US build; fixed here by
DIV-0163 (`BOF3X_ORIGINAL=BattleInitPickClear`); expected to change nothing
in normal play.

## D350 — Scena16 scene 4 starts its caption without waiting for message 2 to close (Capcom's, fixed in the US build; open, held for timing)

**Seen:** not seen; found 2026-10-23 by the region census.

**Established:** step 3 of scene 4 (`0x56BCC0`, step 3 at `0x56BD42`, ours
`Scena16_Scene4`) starts the 0x130-frame caption and camera pull when the
script counter reaches 4; the US disc (`SCENA16.EMI#0` `0x801F7CC4`) also waits
for `Field_Request` `0x66C7D8` to leave 2, as step 1 already does; the JP disc
does not. Chapter 16 is the attract demo (scene 4 in area 2), so the attract
recipe and the attract state-hash run pass through it; whether counter 4 can be
raised while message 2 is open was not shown from the scripts, and longer
translated text makes it likelier.

**Status:** Capcom's (the JP build's); open: a repair would be measured on
the attract run first, since a wait that adds frames shifts the recording.

## D351 — Message 0xFA's gene name is not terminated after its 8-byte copy (Capcom's, fixed in the US build; fixed by DIV-0166)

**Seen:** not seen; found 2026-10-23 by the region census. No route finds a
gene.

**Established:** the dragon gene's "found" message copies the name with
`strncpy(0x904CE0, res(0x4172 + n), 8)` at `0x56DED1` and shows message 0xFA;
the US disc (`GAME.EMI#0` `0x801A83DC`) writes a NUL at `buf[8]`, the JP
(`0x801A93E4`) and the PC do not. A name of 8 bytes or more (a translation's)
ran into what follows the buffer.

**Status:** Capcom's (the JP build's), fixed in the US build; fixed here by
DIV-0166 (`BOF3X_ORIGINAL=GeneNameNul`).

## D352 — `Window_ResetAll` clears five fields of each window record where the US build clears all (Capcom's, changed in the US build; open)

**Seen:** not seen; found 2026-10-23 by the region census.

**Established:** `Window_ResetAll` `0x59E330` zeroes five fields of each of
the 22 records at `WindowRecords` `0x803160` (stride 0x24); the US executable
(`0x80158E50`; JP `0x8015990C`) zeroes every field. One field is read stale
after a reset: byte `+0xD` (bit 0, no outline), read by window kinds 1 and 2
in `Window_Grow` / `Window_Shrink` and never written by them, so an item list
or a battle window can leave it set for the next window. Clearing every field
at all 21 reset sites could also clear one meant to survive (a list cursor).

**Status:** Capcom's; open: a repair wants a state-hash comparison over the
routes first.
