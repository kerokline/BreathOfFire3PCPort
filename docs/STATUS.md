# Status

**Status:** IN PROGRESS (2026-09-23)

Where the project actually is, what is in flight, and what is blocked.
[`PLAN.md`](PLAN.md) says what we intend to do and why; this file says what is
true today. When they disagree, this one is right and `PLAN.md` needs updating.

## Where we are

**Phase 0 is done: our code runs inside the game.** A launcher starts the
player's own `BOF3.exe` suspended and loads `bof3x.dll` into it; a five-byte
detour hands one original function at a time to a reimplementation; and
`symbols.toml` generates the header that makes taking over a function a
four-line change with no edits to its callers
([`SCAFFOLDING.md`](SCAFFOLDING.md)). The exit test passed 2026-09-19 with
`File_Read` `0x5A7470`, under llvm-mingw, in both directions of the A/B switch.
**Seven hundred and eighty-nine functions of ~2,952 recorded - roughly 10,200 real, since `pe_funcs.py` misses every function reached only through a pointer ([`attract-remaining.md`](attract-remaining.md) §3) - are ours** (a hundred and thirty-one from stage 1, three from the text path, below; sixteen on 2026-09-21 through the batch check - oracle, memory dump and frame hash identical ([`sprite-draw-order.md`](sprite-draw-order.md) §12-15); the matrix product among them carries DIV-0021, zeros in a `MATRIX`'s padding; the newest four, later that day, the map-cell handlers `0x570020` and `0x570660` with the condition test and the ground's elevation under them - fuzzed with 17 controls, every in-game call shadowed against a clone over a 7-minute attract run, and through the batch check (§16, `ab18_*`); DIV-0023, zeros in a vertex's padding as in DIV-0021, the owner's call; the newest two, that night, the attachment handle `0x57C0A0` and the inherited draw key `0x589770`, kept faithful to a search Capcom's code discards - latent, no shipped script asks for it ([`known-defects.md`](known-defects.md) D6, [`movement-script.md`](movement-script.md)) - through the batch check with a live shadow in three scenes (§17, `ab19_*`); **the newest twenty-one, 2026-09-22, the whole movement-script interpreter** - the step `0x576B50`, its flow pass `0x576E00` and all eight group handlers - **the field object update that runs it**, `0x517BF0` with its three helpers, the sprite's screen update `0x588F20` with its overlay queue, the attached object's update `0x5192A0` with its offset, and the three counter ops ([`movement-script.md`](movement-script.md) §1-1c) - each fuzzed against a clone with negative controls, and **through the batch check the same day** - captures 4 / 9 / 55 of 4 / 9 / 55 in three scenes, oracle, memory dump, and the frame hash identical on all 10,062 frames ([`movement-script.md`](movement-script.md) §1d, `ab20_*`); **the newest fifty-two, the same day, in five parallel groups** - the field's frame loop, the kind-2 object and the ground's slope, the field objects' kind handlers, the map and draw layers, the title task and the field's mode handler ([`field-frame.md`](field-frame.md), [`kind2-object.md`](kind2-object.md), [`object-kinds.md`](object-kinds.md), [`map-layers.md`](map-layers.md), [`mode-tasks.md`](mode-tasks.md)) - each fuzzed with negative controls, and through one batch check: captures identical in five scenes (the camera turn among them), oracle, memory dump, and the frame hash on all 10,062 frames (`ab21_*`, `ab21b_*`); DIV-0024, a fade past its jump table stops loudly; **the newest one hundred and nineteen, the same day, in seven parallel groups** - the field's mode handlers and the attract demo's scenario 16, the event script's field side, the event script interpreter with the flag helpers, the frame loop's remaining callees, the title's state handlers with the new-game start, the map view's scrolling and reset, and the blocked-ahead test ([`field-modes.md`](field-modes.md), [`field-event.md`](field-event.md), [`event-script.md`](event-script.md), [`frame-callees.md`](frame-callees.md), [`title-states.md`](title-states.md), [`map-scroll.md`](map-scroll.md), [`field-blocked.md`](field-blocked.md)) - 300 negative controls between them (37 / 48 / 46 / 51 / 40 / 50 / 28), six of which were changes that change nothing, and through one batch check: five A/B capture pairs identical, the oracle identical at every compared frame, the memory dump identical in all three regions, and the frame hash identical on all 10,060 frames against an original-vs-original pair (`ab22_*`, `ab22b_*`); no new divergence, and four latent or unchecked defects written down, D8..D11; **the newest one hundred and forty, the same evening, in five more parallel groups (the third round)** - the message box and the last two text pens, the window/task layer with the area change above it, the top-level task flow (boot, the field task, the area entry, all 21 transitions), the movement commands and the party, and sprite animation with the effect pool ([`msgbox.md`](msgbox.md), [`window-task.md`](window-task.md), [`mode-flow.md`](mode-flow.md), [`move-cmds.md`](move-cmds.md), [`sprite-pose.md`](sprite-pose.md)) - each fuzzed with negative controls, and through one batch check (`ab24_*`): five A/B capture pairs identical, the attract in English identical, the oracle identical at every compared frame, the memory dump identical, and the frame hash identical on all 10,063 frames beside an original-vs-original pair, after one fix to the call shape of `Boot_Task`; no new divergence, three latent defects written down, D14..D16; **the newest one hundred and one, that night, in five more groups (the fourth round)** - the area's links and the drop-in party, the field menu's item use (the 33 pointer-reached handlers the queue took for title code), the sound layer, the glyph draw, and the Direct3D draw handlers with the ordering-table walk ([`area-entry.md`](area-entry.md), [`item-use.md`](item-use.md), [`sound.md`](sound.md), [`glyph-draw.md`](glyph-draw.md), [`d3d-draw.md`](d3d-draw.md)) - each fuzzed with negative controls, and through one batch check (`ab25_*`): the oracle identical at every compared frame, the memory dump identical, and the frame hash identical on all 10,062 frames after one fix, to DIV-0028; the capture A/Bs of that batch came back black (the owner's screen cover) and are owed (`analysis/validate_ab25b.sh`); five divergences, DIV-0025..0029 - the glyphs' texel centres (D17), the Config controller panel, the Yes / No layout, the music fades per frame (D26), the save slot's name - every one confirmed in game by the owner, 2026-09-23; D17..D28 written down; **the newest two hundred and twenty-two, 2026-09-23, rounds five and six** - the last of what the attract sequence reaches (the display calls, the page, glyph and cell texture builders on a fake DirectDraw, and the DIV-0010 sprite handlers rewritten with the divergence inside), which **closes stage 1's attract queue at 0 in scope**, and everything the owner's recorded shop route reaches ([`takeover-queue-round6.md`](takeover-queue-round6.md), [`input-script.md`](input-script.md) §5a) - through one batch, `ab26`: the shop A/B 35 of 35, the attract captures all-Capcom against all-ours 55 of 55, oracle and memory dump identical, frame hash identical but for one frame explained as the `sscanf` of a heap address; DIV-0030; D30..D40): `LoadDatFile` `0x454590`, the DAT
container loader every asset passes through (faithful); the whole file layer
`0x5A7370`..`0x5A7510` (eight functions, [`asset-loading-path.md`](asset-loading-path.md)
§1) — seven faithful, and `File_OpenWrite` with a null check the original
lacks (DIV-0003, not yet exercised in game) — `Save_WriteFile`, which
carries the project's first *code* divergence: a fix for saves vanishing from
the save menu ([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0002,
[`save-files.md`](save-files.md)), found, traced, fixed and verified in game
on 2026-09-19 — and `Gfx_BeginFrame` `0x4FD230`, which carries the second: the
first **crash** fixed. Queued image uploads pile up over unrendered frames
(window unfocused, title bar held) until one flush overruns its scratch buffer
into the draw structures; reproduced on all-original code, fixed by draining on
unrendered frames, confirmed in game the same day (DIV-0004,
[`known-defects.md`](known-defects.md) D4). The newest two are faithful:
`Gfx_LoadImage` `0x59EA70`, which writes the PSX-VRAM shadow, and
`Font_SetGlyphData` `0x5A6800` — the first takeover checked in *bytes*, the
1 MiB shadow identical to an all-original run, with a deliberately wrong build
failing the same check ([`asset-loading-path.md`](asset-loading-path.md) §2).
The fourteenth is `Gfx_InvalidateTextures` `0x59E700`, the texture-cache
invalidation and **the first function of the presentation layer**
([`IDEAS.md`](IDEAS.md) I8), faithful down to an off-by-one at every page
edge. Nothing external can see what it does, so it brought a new kind of
check: run a byte-copy of the original beside ours in the same process and
compare, live and under a start-up fuzz ([`SCAFFOLDING.md`](SCAFFOLDING.md)
§2, the shadow check). Three more followed it the same day, the
converted-palette cache: `Gfx_ConvertRow`, `Gfx_LoadImageIfChanged` and
`Gfx_ClutPixels`; then the two rendered-frame flushes, `Gfx_FlushDirtyStrip`
and `Gfx_FlushUploadQueue`, and the unpackers they dispatch to,
`Gfx_UploadPacked5` and `Gfx_UploadLzss`; then `Gfx_ClearImage`,
`Gfx_MoveImage` and `Gfx_MoveCells`; and the texture cache's lookup,
`Gfx_TexCacheFind`, which completed the cache entry's layout. On 2026-09-20
the regenerated takeover queue gave the first nineteen **logic** functions,
all small and all around the field's sprite structures
([`sprite-draw-order.md`](sprite-draw-order.md)): the draw-order pass's two
exchange helpers, the draw-item index pool's alloc and release,
`Prim_SetShade`, `MapView_CellToMap`, `Sprite_FindNearby`, `Field_CopyInput`,
the four functions of the sprite animation script (its format is now known),
`DrawItemPool_ReleaseCell` - at 863,659 calls the hottest function the
attract run has - `AreaMap_ByteAt`, `Sprite_PointInReach`,
`Sprite_RestoreClut`, `DrawLayers_Reset`, `DrawTable_Sort` and
`MapView_SetElevation`. Reading them fixed the **sprite object arrays - 30 + 4
objects of `0xA4` bytes at `0x7DEE80` / `0x802000`**, which 53 functions
reference. All nineteen pass the oracle, the memory dumps and a re-recorded frame
hash. The same day the queue's hottest entries turned out to be one thing:
**the port's own implementation of Sony's libraries**
([`psx-library-layer.md`](psx-library-layer.md)) - libgpu primitive setters
and ordering-table links, `getTPage`, `getClut`, a sine, and a GTE whose
registers are globals. Sixty-eight of its functions are ours, `ApplyMatrix`
at 2.6 million calls a run among them - and, since the same day, the ones that
go through x87. What x87 computes depends on the control word, so that was
measured first: **`0x027F`, 53-bit precision, on every one of 11 million live
calls**, which makes each x87 operation the IEEE double operation and the
functions plain `double` - no `long double`, no emulated rounding. The
perspective division, the depth-cue ramp and the per-vertex depth stores came
first (a live shadow compared 11 million results bit for bit), then the GTE's
transform commands over them, `RTPS` at 4 million calls a cycle among them,
the vector normalisations, and a `NormalColor` whose lit colour the port
overwrites with the unlit one. The draw-order pass's primitive commit and
layer close followed, and then **the pass itself**, `Sprite_DrawPass` - the
first takeover with callees that are not ours, fuzzed with recording
stand-ins in their place ([`sprite-draw-order.md`](sprite-draw-order.md) §9). Two things came out of the integer batch that outlast it:
`-fno-strict-aliasing` is now a project-wide compile option, found necessary
when the optimiser repaired a deliberately wrong build; and the frame hash's
exclusion list was rebuilt (`calltrace.py wallclock --static`) after the
faster traced game exposed wall-clock draw code no slow run had entered -
settled by comparing original against original
([`call-trace.md`](call-trace.md) §6). **Stage 2 began on 2026-09-20 and the game is
playable in English the same day** ([`dialogue-localisation.md`](dialogue-localisation.md)):
overlay `DAT`s built on the player's machine from the player's US disc carry
every area's dialogue, the system pools, the item and ability names and the
US font, and `BOF3X_LANG=en` loads them - five ledger entries, DIV-0005 to
DIV-0009. What was learned on the way: the port's text code IS the glyph
index; English on the PlayStation is monospaced at 8 px, so there was no width
table to port; Capcom made room for English by moving the system pool, and so
did we, because it has one reader against the script's forty-one; and the
Chinese port's name fields are 16 bytes where the US disc's are 12. Three
functions of the text path are ours - `Msg_SystemPtr`, `Text_DrawString`,
`Text_DrawImmediate` - each fuzzed against a clone; the last one's fuzz found
a slip of Capcom's the read had missed. The same evening the owner walked the field menu while
it was sampled read-only ([`menu-screens.md`](menu-screens.md)): its state
machine is mapped, and the first of four defects of the 2001 menu is fixed -
**DIV-0010**, the Direct3D sprite handlers' far texture edge, which cut the
bottom off every menu numeral ([`known-defects.md`](known-defects.md) D1).
That one is not a reimplementation: the log's "115 ours" counts three copies
of Capcom's own handlers with two operands re-aimed, because a drawn surface
cannot be checked yet. **DIV-0011** followed: the Config panel's frame, whose
draw the PC build compiled to an empty function, drawn again from a read of
the PlayStation's, found by searching the owner's disc for the call's
arguments; the reserve list on "change party members" had the same empty
call and got the same frame. The owner's next report, a glow round PC text,
turned out to be two things: bilinear filtering under a low alpha test
(**DIV-0012**, an opt-in `BOF3X_FILTER=point`, the first half of a look
toggle the owner wants - [`IDEAS.md`](IDEAS.md) I15) and a white text palette
the PC team brightened (**DIV-0013**, restored from the disc by the English
overlay). Later the same evening the title menu, which is artwork and not
text, was rebuilt from the disc - NEW GAME, LOAD GAME, and a CONFIG cut from
their letters (**DIV-0014**, [`title-menu.md`](title-menu.md); confirmed in
game by the owner off a recipe capture, 2026-09-21). The frame hash was re-recorded with all of it up to DIV-0013
(`ab15_*`, recorded with DIV-0010 in and before DIV-0011..0013, none of
which touches a traced function): identical over 7,936 frames. Sixteen of the hundred and nine from stage 1 are beyond
the attract sequence's reach and rest on the differential fuzz alone -
`Gfx_UploadLzss`, `Gfx_MoveImage`, `Gfx_MoveCells`, nine of the depth stores,
`Gte_RotTransPers3`, the two `RotAverage`s and `Gte_ScaleMatrix` - as do
`Field_CopyInput`'s button exchange and the two branches of the perspective
division for a vertex behind the near plane, which the attract run never
produces.

What is established:

- The two binaries are **compilations of one C source tree**, and the source's
  *file decomposition* survived into both ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)).
- **Name transfer works**, by two independent techniques — global block deltas
  and constant-table value search — on both a well-documented subsystem and a
  cold one ([`kinship-probe-text-engine.md`](kinship-probe-text-engine.md),
  [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)).
- **Ghidra BSim works cross-ISA** and is a seed generator rather than an oracle
  ([`bsim-evaluation.md`](bsim-evaluation.md)).
- **The `DAT/` container is parsed** — 742 of 742 files, unencrypted, and most
  non-audio sections are byte-identical to the JP disc's `.EMI`s (2,120 of
  2,680, full census; the 560 differences reduce to a handful of causes) ([`DAT_CONTAINER.md`](DAT_CONTAINER.md), `tools/dat.py`). Done
  ahead of step 0 at the owner's direction, 2026-09-19.
- **The media stack is surveyed, and the first divergence has shipped.**
  `BOF3.exe` is DirectDraw + `IDirect3D3`, DirectSound 1, DirectInput 3, MCI/VFW
  for FMV, and a statically-linked MP3 decoder. Exactly one thing was broken —
  `capcom.avi` is Indeo 5, which Windows has not decoded since XP — and it is
  now fixed by re-encoding to Cinepak, the ledger's first entry
  ([`media-stack-survey.md`](media-stack-survey.md),
  [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0001). Audio needs nothing. The FMV path
  is fully read ([`replacing-mci.md`](replacing-mci.md)); the DirectDraw
  presentation layer is the long-term liability, is **not** read yet, and is
  [`IDEAS.md`](IDEAS.md) I8 against phase 3.
- **The asset-loading path is partly read** ([`asset-loading-path.md`](asset-loading-path.md)):
  a 16-slot `FILE*` file layer at `0x5A7370`..`0x5A75F0`, and `LoadDatFile`'s
  four chunk kinds. Kind-0 data lands in **one arena at `0x803580 + tag`** — the
  port's repacked image of PSX RAM from `0x80010000`, which closes
  `DAT_CONTAINER.md`'s open question.
- **There is a regression oracle.** The port is deterministic from launch
  through its attract sequence — identical `Rand` call count, message index and
  area word at every one of 7,478 frames across fresh launches — because the
  seed is fixed at 1, the binary has no `srand`, game logic cannot reach a
  clock, and all of it runs on one thread inside a four-coroutine task system.
  Original-vs-ours already compares identical
  ([`attract-mode.md`](attract-mode.md)). Reach is two field scenes; no battle
  or menu yet.
- **The PC save format is solved** ([`save-interchange.md`](save-interchange.md)):
  the PSX `0x10B0`-byte game block from file offset 0, same checksum rule, same
  field offsets, with the character-record name widened 5→9 bytes and later
  record fields +4. `tools/save_convert.py` converts both ways; round trip is
  byte-identical and the sibling's verifier accepts a PC save. **Both
  converted saves load, play and re-save on PC** (owner, 2026-09-19); PC→PSX
  is still static only.
- 168 functions, 8 global blocks and 119 data items named in
  [`symbols.toml`](../symbols.toml), tiered; 150 functions carry signatures and
  are callable from our code, 122 of them ours (counted 2026-09-21 by
  `gen_symbols.py` and `tomllib`, not from memory).
- **An in-process call tracer and a crash reporter** live in the injected DLL.
  The tracer ([`call-trace.md`](call-trace.md)) gives which functions a run
  reaches (540 of 2,936 in the attract sequence), call counts and edges, a
  per-frame call hash that is identical across launches and passes
  original-vs-ours, and a takeover work queue. The reporter
  ([`crash-reporter.md`](crash-reporter.md)) is always on and caught its first
  real crash the day it was built.
- **Known defects are written down** ([`known-defects.md`](known-defects.md)):
  clipped stat numerals (draw-time, cause unread), the mojibake title, the
  crash above, and a frame deadline kept in a 32-bit float, which makes game
  speed depend on Windows uptime — 31.25 fps at 4.5 days up, as measured,
  and **half speed past 6.2 days, observed 2026-09-21** (every run that day,
  original included; D5). With Fast Startup that "uptime" survives nightly
  shutdowns, so players meet it within a week; past 12.4 days the game runs
  unthrottled and, by the code, draws nothing. **Fixed short term by
  DIV-0022** (the game's clock starts with the game: 30.00 logic frames a
  second; the owner confirmed the speed in game, 2026-09-22); the complete
  fix is [`IDEAS.md`](IDEAS.md) I16.
- **The launcher has a settings dialog** (2026-09-20,
  [`launcher-settings.md`](launcher-settings.md)): language, texture filter,
  display and renderer, in a plain Win32 `DIALOGEX` with nothing vendored.
  Language and filter go to the environment the game inherits, so the DLL did
  not change; display and renderer are written into the game's own `BOF3.CFG`,
  which is the original's input, not a patch — no ledger entry. Resolution is
  shown disabled: 640x480 is welded into the presentation layer
  ([`IDEAS.md`](IDEAS.md) I8). Also established there: the disc's `START.EXE` is
  an autorun shell reached through `WinExec` and the registry, `SETUP.EXE` is
  InstallShield 5, and neither has anything to do with game configuration —
  `BOF3.CFG` is the only config filename in the exe. **Scripted runs now need
  `--no-config`.**
- **The in-game Config screen is translated** (2026-09-20, DIV-0015,
  [`config-screen.md`](config-screen.md)). Its text is in `BOF3.exe`, not in
  any `DAT`: six label addresses built into the row draw, seventeen 16-byte
  option records, six controller names. The US disc's `START.EMI` holds the
  same structures with the same two row tables **byte for byte**, so the
  strings come from the player's own disc through a kind-7 chunk, counts and x
  offsets included - quirks and two unused records and all. **Confirmed in game by the owner, 2026-09-21.**
  The donor's **second** Latin set came with it: the same 100 characters at
  8 x 8, appended at glyph `0xA00` and stored **tripled**, because the 8-unit
  quad scales a whole 24 x 24 glyph down to 16 x 16 rather than cropping one.
  This screen draws with it - and a
  glyph-index guard that now follows the loaded table instead
  of the original's flat `0xA00` (DIV-0016). The row under the cursor is drawn
  in the 8 x 12 dialogue font on the same 8 advance, as the disc does, instead
  of the UI glyph blown up and thrown left (DIV-0017). All of it confirmed in game by the owner, 2026-09-21. The font table is no longer
  capped by anything but that guard, which is in our own `Text_DrawString`.
- **PSX functions pair with PC ones at scale** (2026-09-21,
  [`attract-remaining.md`](attract-remaining.md) §5): the PSX area descriptor
  table has a PC twin at `0x667590` (198 of 200 areas agree), and from its 728
  pairs `tools/psx_pair.py` grows 3,330 through call lists, size-checked
  position, shared tables and callers - 433 of them boot EXE functions - each
  method measured against call edges it was not chosen on. The same day's
  catalogue found that `pe_funcs.py` misses every function reached only
  through a pointer, some 7,300 (§3 there), and that the WndProc is
  `0x4FC6F0`.
- **An agent can walk the game unattended** (2026-09-21,
  [`input-script.md`](input-script.md)): recipes of pad presses played inside
  the game, counted in its own frames and so repeatable from launch, with the
  window captured at each `shot`. Built on two measured facts: the pad word is
  the PlayStation's bit layout, and the input latch runs more often than
  frames, so the recipe keys on `Frame_Counter` `0x937F94`. It reached the
  Config screen, a save's field menu, and the first in-menu A/B of DIV-0010.
- **More of the exe's own text is English** (2026-09-21,
  [`dialogue-localisation.md`](dialogue-localisation.md) §8): the menu's
  button verbs on Config, Items, Ability, Equipment and Tactics (DIV-0018),
  the battle's command labels (DIV-0019), and New Game's default names -
  Ryu ... Peco, the Whelp - and Manillo the fish merchant's (DIV-0020). Each
  is a slot table in `.data` whose US twin sits beside data the PC kept byte
  for byte, which is how the build finds it on the player's disc. All but
  Manillo's are captured in game. Saves keep their names; the owner accepted
  gibberish across a language switch until a language-independent name
  system exists.
- **The first battle is reachable unattended**: a new game plays into the
  opening's scripted fight (field mode 5), and its command cross -
  hold-to-choose - is captured command by command
  ([`input-script.md`](input-script.md) §4). DIV-0010 and DIV-0014 were
  confirmed by the owner off recipe captures the same day.
- Four comparable projects surveyed for what they learned the hard way
  ([`prior-art/`](prior-art/)).

## The immediate order of work

**Direction set by the owner, 2026-09-19** — three stages, in this order:

1. **Replace every function the attract sequence reaches.** It is the part of
   the game with a regression oracle today: 540 of 2,936 functions
   ([`call-trace.md`](call-trace.md)), each testable the day it is taken over,
   with the takeover queue already layered (§9 there). **Closed 2026-09-23**:
   0 in scope - what the attract sequence still runs of Capcom's is the CRT,
   the MP3 decoder, the Windows shell, the task system and the run-once
   platform set-up, all left for [`IDEAS.md`](IDEAS.md) I8 / I12 by the
   owner's choice. The input-reached queue carries it on: routes the owner
   records, each A/B'd and traced ([`HANDOFF.md`](HANDOFF.md) 0000).
   **Ahead of more routes, by the owner's direction of 2026-09-23: the UI
   overhaul** ([`display-overhaul.md`](display-overhaul.md)) - window modes
   without an exclusive mode-set, integer scaling, a shader present pass,
   and widescreen. Its first step, a Direct3D 11 backend behind DirectX 6's
   own objects, runs the game as of 2026-09-23
   ([`render-backend.md`](render-backend.md), DIV-0031); the PSP release's
   16:9 was mined in parallel for how Capcom did it
   ([`psp-widescreen.md`](psp-widescreen.md)). **Its second step landed the
   same afternoon: WinMain, WndProc and the FMV player are ours** (796 ours;
   [`window-modes.md`](window-modes.md)) - a resizable window or a
   borderless one the size of the monitor with no mode-set anywhere
   (DIV-0032), the game running while unfocused with the pads zeroed
   (DIV-0033, the owner's I12), the frame debt clamped (DIV-0034), the FMVs
   through MCI into the window at an integer scale (DIV-0035, the owner's
   choice over a bundled decoder); the oracle and memory dump through the
   batch `wm1` with the window never in front, the frame hash's new
   reference after a review the same evening found why every traced
   all-original run had ended 16 s in (the tracer's single step saved by
   a `pushfd` on Capcom's software-renderer set-up; `wm1b`: original-vs-original identical on all 10,313 frames, original-vs-ours on every logic frame).
   **Step 4, integer scaling, that evening** (DIV-0036), then **the night
   of 2026-09-23** ([`HANDOFF.md`](HANDOFF.md) item 000000): **the
   widescreen survey build** (DIV-0041, [`widescreen.md`](widescreen.md)) -
   426 x 240 behind the launcher's Widescreen box, the view shifted into a
   wider target the PSP's way, the terrain and frame-pass culls widened,
   the backdrop, fades and the menu boxes' slide-outs re-authored from what
   the survey and the owner's play found; the sky gradient `0x571BE0`
   deferred to a recorded route; **the resizable window** (DIV-0042) -
   snap to whole multiples or fit to the height, the target following the
   client between frames, the FMVs obeying, the placement remembered; the
   owner: both modes "work perfect"; and **the SatPixie look** (DIV-0043,
   [`THIRD_PARTY.md`](THIRD_PARTY.md)), the owner's chosen MIT shader
   ported with a sliders dialog. Owed: the oracle and hash once wide, a
   rescale under the older CRT look, the owner's tuning of the look.
   The attract sequence's text boxes are the in-game dialogue engine
   ([`attract-mode.md`](attract-mode.md) §6), so stage 2 inherits a regression
   check from stage 1.
2. **Then the text swap** - [`dialogue-localisation.md`](dialogue-localisation.md):
   overlay `DAT`s and an upscaled font table, both built locally from the
   player's discs. **Begun 2026-09-20: English dialogue draws in the attract
   sequence**, and by the evening the owner was playing it: dialogue,
   narration, menus, item and ability names (DIV-0005..0009); on 2026-09-21
   the menu's buttons, the battle's command labels and New Game's names
   (DIV-0018..0020). Enemy and place names, some labels and headers, and text
   in artwork are still Chinese - so
   that the owner can make headway through the game
   itself — and with that, reach code the attract sequence never runs. What
   this means in detail is the owner's to say; the asset side of selectable
   languages is surveyed below ("A stated goal worth recording now"), and any
   swap is a divergence in the ledger sense.
3. **Then the combat module**, which the attract sequence does not enter at
   all, and which therefore needs stage 2's reach — and an oracle of its own —
   before it can be replaced with the same confidence.

The numbered steps below are the history of how the project got here; this is
what orders new work.

### 0. Verify launch and stability — **passed 2026-09-19, enough to proceed**

Owner-tested on this machine, 2026-09-19: the launcher starts the game, the
intro FMVs play, the start screen works, and play continues **into the first
area**. That is the gate this step existed for — the game runs here — so
**phase 0 proper is unblocked** and is now the next piece of building work.

It also settles what was an open worry: the legacy DirectDraw display path,
including the exclusive-fullscreen `SetDisplayMode(640, 480, 16)` that FMV
performs before the title screen, works on Windows 11 today. Replacing it is
[`IDEAS.md`](IDEAS.md) I8, deliberate phase-3 work, not an emergency.

What this test did **not** cover, carried forward rather than blocking:

- No written baseline of what "working" looks like beyond the first area — no
  battle, menu, save/load or long-session stability check.
- The known defects (fullscreen fallback, resolution handling) have not been
  reproduced and recorded.
- **There is an attract sequence — answered by the owner, 2026-09-19.** Left
  idle, the start screen plays through several areas with story text on screen.
  It is **not FMV**: on the PSX it ran through the overlays and the ordinary
  area code, so it is the real engine being driven, which is exactly the
  property that made TR1X's demos a determinism oracle
  ([`prior-art/tr1x.md`](prior-art/tr1x.md) §2.5). It is "not a full attract
  mode" — no recorded gameplay input is known — so what it can police is area
  load, scripting, text and rendering, not battle. **Not yet established:** what
  drives it (a script, a timer table, recorded input), whether it is
  deterministic run to run, and whether it touches `Rand`. That is
  [`IDEAS.md`](IDEAS.md) I6.

### 1. Read the exe, starting with asset loading

Set 2026-09-19. The `DAT/` work ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)) left
concrete anchors — `LoadDatFile` `0x454590`, the filename table, the `SND\`/`BGM\`
name strings — in a subsystem whose inputs and outputs we can now parse and
diff, which also makes it the natural first replacement target for phase 0.
Steps are in [`HANDOFF.md`](HANDOFF.md). Overlaps phase 0 and does not gate it.

**Save file interchange was priority 1 here until 2026-09-19.** It is now
[`IDEAS.md`](IDEAS.md) I1: still the first visible win to go for, but only once
the game is up and running, since a converted save cannot be verified without
loading it. Step 0 has now cleared that gate; it is available to pick up
whenever a visible win is wanted.

### 2. The overlay corpus

**Demoted 2026-09-18, and worth less than believed.** Investigated in
[`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md):

- The corpus is **121 named overlay functions across 8 of 406 overlays**, not
  the 29,036 repeated across five documents. That figure had no artifact behind
  it. What the overlays really offer is **5,805 statically discovered function
  entries** — structure, not names — and a human role for 216 of 406 overlays.
- Indexing them is **actively risky**, not merely pending: it grows the BSim
  candidate pool ~17x and, done the way the sibling's tooling seeds the boot EXE
  into every overlay program, would flatten BSim's rarity scoring and silently
  degrade the boot-EXE ranks already published.
- The images are already extracted and complete on disk (406/406 verified), and
  the machine time is about an hour. Cheap to do, easy to do wrong.

This is a **PSX-side problem only**: overlays are a 2 MB-RAM technique, and the
PC port is one flat image with every function resident. It matters because the
name corpus is keyed by overlay and a bare PSX address is ambiguous without
knowing which was loaded.

Consequence: this drops below exe reading and phase 0. When it is done, it
must be on a database copy with the nine BSim pairs re-scored before and after.

### 3. Phase 0 proper — **done 2026-09-19**

Launcher, detour layer, generated symbol header, one toolchain. Described in
[`SCAFFOLDING.md`](SCAFFOLDING.md); exit test in its §5. Numbered 3 for history.

What phase 0 deliberately did not build: register-argument thunks for
non-standard MSVC6 conventions (wait for the first real case), a progress
report, the Ghidra round-trip (phase 1), and any CI — there is still no
workflow that compiles `src/`, and the receipt policy below now has something
to bite on.

## A stated goal worth recording now

**Selectable localisations, built from the original releases' own assets.**
Japanese, English, German, French and Chinese all shipped officially. This is a
phase-5 target, not near-term work, but two findings make it worth writing down
while the groundwork is being laid:

- **There is no machinery to inherit.** Each PlayStation language was a separate
  compiled build on its own SKU — no language subdirectories, no language
  strings in any executable, and the regional executables are not
  address-compatible with each other
  ([`regional-builds.md`](../../BreathOfFire3Recomp/docs/regional-builds.md)).
  Language switching is something this project would *build*, and it is
  therefore a divergence in the ledger sense, not a port of existing behaviour.
- **The asset side is enumerable and already surveyed.** Exactly **37 image
  sections** carry language, and 36 of 37 hash as four distinct images across
  the five releases — JP, US+EU sharing, France, Germany. The glyph atlas is a
  32 KB *texture* (not code), duplicated into every module that draws menus;
  plus the ending/kanji font and ~11 areas with text baked into scenery art.
  Every one of those sections is readable from a donor disc.

The design constraint that follows: **the player supplies the discs, the engine
reads the sections.** Shipping extracted glyph atlases would be distributing
Capcom's assets and would break the engine/data split that
[`LICENSING.md`](LICENSING.md) §3 says is not negotiable. Same model as
DevilutionX and OpenRCT2.

[`fixtures.toml`](../fixtures.toml) already carries all five PSX SKUs for this
reason — four as eventual localisation donors, Europe/English catalogued only so
an unrecognised disc can be named rather than guessed at.

## Open decisions

- **How to CI an oracle that needs undistributable game data.** *Decided
  2026-09-18, partially.* Contributors are expected to supply their own copies
  — having both sides locally for side-by-side comparison is the workflow
  anyway, so validation is a formalisation of it rather than new infrastructure.
  The shared contract is hashes, not artifacts: [`fixtures.toml`](../fixtures.toml)
  catalogues known builds and `tools/verify_fixtures.py` checks a local install
  against it, so two people can confirm like-for-like without anyone hosting a
  runner. That also removes the self-hosted-runner question and its fork-PR
  attack surface entirely.

  **Staleness policy, decided 2026-09-18: warn always, fail when the diff
  touches `src/`.** A documentation or tooling change should not be blocked on a
  differential run it cannot affect; a change to the game code should not merge
  on an assertion nobody re-checked. The warning is the part that matters — it
  is what stops the harness dying quietly the way OpenRCT2's did
  ([`prior-art/README.md`](prior-art/README.md) §1). **As of 2026-09-19 `src/`
  exists**, so the "fail" half of this policy is no longer hypothetical, and the
  receipt below is the next piece of infrastructure owed.

  **Receipt shape** (to build when there is something to verify, not before):
  a committed file recording the git SHA it ran against, the date, the
  `fixtures.toml` build ids on both sides, what was covered, the result, and the
  hashes of the vectors used. CI validates structure and freshness only — it
  never needs a byte of game data. Deliberately *not* built yet: there is no
  `src/` and no harness, and speculative verification infrastructure is exactly
  what rots.
- **Size floors for the matcher.** [`bsim-evaluation.md`](bsim-evaluation.md)
  shows tiny wrappers and very large functions are unreliable; nine pairs is too
  few to fit a cutoff.
- **Phase 3 subsystem order.** `PLAN.md` puts the platform layer first for
  portability; two completed projects removed it last
  ([`prior-art/README.md`](prior-art/README.md) §2).

## Outstanding obligations

- **Write findings back to the archival sibling.** We answered its open `0x0C`
  question from the PC side and established that `Rand` does not match between
  the binaries — which constrains *its* differential-testing plans too. The
  cross-reference relationship is reciprocal and we have taken without giving.
- **Contact TheRealBiggs** ([`PLAN.md`](PLAN.md) §8 step 1). Worth doing now
  specifically because there is finally something to offer rather than only
  questions.

## Known gaps in the record

- **No CLA**, and the DCO does not substitute for one
  ([`LICENSING.md`](LICENSING.md) §7).
- **No legal review** of the provenance, which `LICENSING.md` §5 says should
  happen before any commitment to a commercial timeline.
- **The exact MSVC product versions** behind `BOF3.exe`'s Rich header are
  unidentified — the public build-number tables consulted did not cover them
  ([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §2).
- **`MsgBox_Step` returning a single BSim candidate** is unexplained and matters
  before relying on BSim for large battle functions.
