# The platform layers: what is left after the game's own code, and how to take it

**Status:** PROPOSED (2026-10-04, the owner's ask during round fourteen; counts from the catalog at `aed35f8`, nothing here read function by function; 2026-10-05 the owner agreed the order of section 4, with the audio before state 2's proof; 2026-10-10 the decoder chosen, minimp3, section 2.4)

Round fourteen ([`takeover-queue-round14.md`](takeover-queue-round14.md))
takes the remainder of the game's own code. What it leaves in `BOF3.exe` is
the layer under the game: the C runtime, the MP3 decoder, what is left of
Capcom's renderer and PSX library shim, the sound and set-up code, and the
executable itself as the thing we are loaded into. No document planned those
as a whole. [`PLAN.md`](PLAN.md) section 5 names the goal (phase 3's
"platform layer", phase 4's cutover) and four entries of
[`IDEAS.md`](IDEAS.md) cover pieces (I3 the lifter, I7 the video path, I8 the
presentation layer, I23 the music). This is the plan that joins them.

## 1. What is left, measured

`tools/remaining_catalog.py` at `aed35f8` (the scratch copy
`catalog_aed35f8.tsv` of round fourteen's staging): 533 starts in the
catalog's parts 0 and 1, which round fourteen's cut left out.

| Layer | Starts | Bytes (catalog) | Range | What the catalog says it is |
|---|--:|--:|---|---|
| The C runtime (MSVC 6, static) | 242 | 41,586 | `0x5B9380..0x5C3660` | `malloc`, `sprintf`, `strncpy`, `rand`, start-up, the per-thread data |
| The MP3 decoder (static) | 200 | 55,534 | `0x5AB7E0..0x5B8DA0` | an MPEG-1 layer III decoder; lineage not identified ([`media-stack-survey.md`](media-stack-survey.md)) |
| Renderer | 44 | 15,712 | `0x455050..0x5AAF93` | Capcom's Direct3D / DirectDraw draw path |
| PSX library layer | 17 | 1,209 | `0x5A6380..0x5A7C70` | the port's stand-ins for PlayStation library calls |
| Platform set-up | 10 | 2,260 | `0x5A5BC0..0x5A6830` | device and window set-up |
| Sound | 8 | 219 | `0x587B80..0x5A7200` | DirectSound glue |
| Windows shell | 7 | 832 | `0x4FC6A0..0x5A9880` | the window procedure and its helpers |
| Not functions | 5 | 4,729 | | data the start lists took for code |

**What that table does not establish, and the first step must:**

- **The labels are by address range.** Round fourteen's R2H found
  `Menu_DrawVerbPair` `0x59E160` filed as "Renderer" - it is a menu draw - and
  took it; nobody has read the rows from `0x59E4F0` on. Some of the 86
  non-runtime, non-decoder starts are game code.
- **Which of them still run under ours.** The presentation layer is ours
  since DIV-0031 ([`render-backend.md`](render-backend.md)), `Fmv_Play` since
  DIV-0035, WinMain and the input path since the early rounds. A function
  of Capcom's renderer that only Capcom's `Gfx_DrawOTag` called is dead when
  ours runs and alive only under `BOF3X_ORIGINAL`. The catalog does not say
  which are which.
- **How many runtime entry points the game calls.** Ours calls a handful by
  address today (`Rand` `0x5B93D2`, `Crt_sprintf`, `Crt_malloc`, `strncpy`
  `0x5B9450`, `_stricmp` `0x5C2B40`); the other two hundred-odd starts are the
  runtime's own insides.

## 2. The layers, and what "taking over" means for each

They are not one kind of work. Only the first two are takeovers in the sense
of rounds one to fourteen.

### 2.1 The small layers: shell, set-up, PSX library, sound (42 starts)

Game-facing glue of the size of one takeover group. **Take them as a group
like any other** - read to the last instruction, fuzz against the original
where a function computes, a recorded stand-in where it only calls Windows.
Those that are already bypassed by ours (the set-up our display code
replaced) are not taken: they are listed as original-only and go at the
cutover.

### 2.2 The renderer's remainder (44 starts, 15.7 KB)

First the read: each start is one of (a) game code mis-filed - a round
fourteen debt, taken as game code; (b) alive under ours - taken, with the
state hash and the picture A/B as its live checks; (c) reached only from
Capcom's draw path - **not taken**, documented, and gone at the cutover. The
packet builders (`Gpu_*`, `Gfx_*`) the game calls are mostly ours already;
what is left is expected to be mostly (c). Expected, not measured.

### 2.3 The C runtime (242 starts) - replaced at its boundary, not decompiled

Decompiling Microsoft's runtime buys nothing and is not ours to publish.
**The unit of work is the entry point the game calls**, each bound to our own
toolchain's runtime or to a small function of ours:

- `rand` is **reimplemented exactly** (the MSVC 6 generator, never seeded:
  [`HANDOFF.md`](HANDOFF.md)'s note on why battles replay). Every recipe and
  the `randlog` depend on its sequence; a fuzz against the original's is
  trivial and total.
- `sprintf` and friends: the formats the game uses are a short list read from
  the call sites; ours handles those and aborts on any other (rule 4).
- `malloc` / `free`: the game's allocations are few (the music file, the
  DAT buffers); pointers they return already differ run to run.
- The string and memory functions bind to the toolchain's.
- Start-up, exit, the per-thread data and the FPU control word (`0x027F`,
  [`SCAFFOLDING.md`](SCAFFOLDING.md)) stay the executable's until the
  cutover and are designed there.

**Exit test:** no call or jump from ours into `0x5B9380..0x5C4000`
(`tools/pe_xref.py` over our own call-by-address constants; the tracer with
only that range armed over every route).

### 2.4 The MP3 decoder (200 starts, 55.5 KB) - not scheduled; its shape waits on I23 (section 5)

`BGM/*.DAT` are bare MPEG streams and the decoder is a third party's, linked
in. [`media-stack-survey.md`](media-stack-survey.md) judged replacing it
"pure risk with no player-visible return", and that stands **while we run
inside `BOF3.exe`**: it works, and it is not Capcom's game. It becomes
necessary only for the cutover (section 3), where no code of the original
may run. Then:

- The seam is small and already ours on the calling side: `Music_OpenDecoder`,
  `Mp3_Decode`, `Mp3_Seek` and the end-of-stream answer `0xFFFFFDFE`
  (`symbols.toml`, the music entries).
- A permissively licensed decoder behind that seam
  ([`LICENSING.md`](LICENSING.md) section 4: no copyleft;
  [`THIRD_PARTY.md`](THIRD_PARTY.md) carries its notice). Which one is a
  decision; none is chosen here. **Chosen 2026-10-10 (the owner): minimp3**,
  `minimp3.h` at `ea99364f`, CC0 - dr_mp3 carries the same decoder and
  fixes no decoding bug it has ([`mp3-decoder-choice.md`](mp3-decoder-choice.md),
  with the rule for our seek: never skip frames with `pcm == NULL`).
- **It is a divergence**: two correct decoders differ in the last bits of
  each sample. The entry says so and gives the measure - PCM against the
  original's, per track, as an error bound, and the loop seam of every
  looping track ([`bgm-comparison.md`](bgm-comparison.md) has the method).
- The state hash is unaffected (the staging buffer is in its skip list); the
  frame's logic sees only `Music_Finished` and the fade.
- The alternative on the table is I23's larger one: music from the disc's
  sequences, which removes the decoder question for players who have the
  disc. Not this plan's to choose.

### 2.5 Video

`Fmv_Play` is ours and plays through MCI into the window (DIV-0035). MCI and
its codecs are Windows', not the executable's, so nothing of `BOF3.exe`
remains in this path; I7 ([`replacing-mci.md`](replacing-mci.md)) is the
costed plan for a bundled decoder and is gated on portability, not on this.

### 2.6 What the import table still names

DirectDraw, Direct3D, DirectSound, DirectInput, MCI, kernel and user calls
made by code that is still Capcom's go away with that code; those made by
ours are ours to keep or to move (the pad is SDL3's already, DIV-0050).
**Not measured:** which imports only original-only code reaches. It falls out
of section 4's first step.

## 3. The executable itself: the cutover

Today the launcher starts `BOF3.exe` suspended and injects `bof3x.dll`
([`SCAFFOLDING.md`](SCAFFOLDING.md)); every function of ours is reached
through a jump planted at Capcom's entry, and the game's data is the
executable's own `.data` and `.rdata` at their linked addresses. Three
states, each a real milestone:

1. **Hosted, all game code ours** - the end of round fourteen. Capcom's code
   still runs for the layers of section 2.
2. **Hosted, no Capcom code runs** - sections 2.1 to 2.4 done. Provable: the
   tracer armed on every start that is not ours, over the attract sequence
   and every route, records nothing after the entry point's hand-off. This
   is the state in which "we own every call" is a measurement.
3. **Our own executable** - a 32-bit process of ours that maps the player's
   `BOF3.exe` data sections at their addresses and never maps its code. The
   image has no relocations (`/FIXED`, base `0x400000`), which is what makes
   this possible without touching a data pointer: our executable reserves the
   range and loads `.rdata` and `.data` there. **Measured 2026-10-08**
   ([`exe-import.md`](exe-import.md) sections 5 and 6): 9,142 `.data` words
   point into `.text` (6,346 targets), and state 3 maps no Capcom code, so
   the engine rewrites those words to its own functions or maps entry stubs
   at Capcom's addresses - with the PC's exe as the source too. **Chosen
   2026-10-10** ([`exe-import-engine.md`](exe-import-engine.md) section 4): the entry-stub page,
   measured - 6,211 of the 6,346 targets are function starts, the smallest
   gap between starts is 11 bytes, 17 functions have no `impl`;
   `BOF3X_EXEIMAGE=<cache>` already lays `base/exe/data.bin` over `.data`
   after a byte check. The data it
   maps is `base/exe/data.bin` from any source, plus `.rdata` (65 addresses
   `src/` reads: since 2026-10-10 44 are the engine's own, `rdata_consts`,
   13 the loader's import slots, 1 the SDK's, 7 nothing to map). **The game's tables stay the
   player's file** - [`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 5's rule
   and [`LICENSING.md`](LICENSING.md) section 3's engine / data split are why
   this is the shape and not a copy of the tables into our source.

After state 3 the pin to x86-32 is the data's layout - 32-bit pointers inside
tables and records at fixed addresses - not any code. Moving to 64 bits or
another system is [`PLAN.md`](PLAN.md)'s phase 4 and I3's lifter question and
is not planned here.

**What the cutover changes for testing.** In states 1 and 2 the original is
one switch away (`BOF3X_ORIGINAL`) and the state hash's reference is two runs
of Capcom's code ([`state-hash.md`](state-hash.md)). In state 3 the original
can still be run as its own process for a reference pair, so the anchor
survives; the day-to-day check becomes build N against build N + 1 in any
language. That wants the state hash address-independent where the game's
tables point into our image (the language overlay's cells, section 6 of the
state hash's doc) - a small piece of work to do before it is needed.

## 4. Order, and the first step

1. **The read pass** (tooling and reading, no takeover): every one of the 86
   non-runtime, non-decoder starts classed as game code / alive under ours /
   original-only, with the evidence; the runtime entry points the game calls,
   listed with their call sites; the imports that only original-only code
   reaches. The reach half is a trace: every start that is not ours armed,
   ours running, the attract sequence and every route. Output: this table
   with real numbers, and round fourteen's debt about the unread "renderer"
   rows closed.
2. **The small layers and the renderer's live remainder** - one round on the
   existing pattern (section 2.1, 2.2). **Done 2026-10-05 night**
   ([`platform-round.md`](platform-round.md)): the 40 and thirteen more the
   catalogue never held (game modes 3..6), 10,065 ours; the software path
   settled original-only. The state hash is its live check.
3. **The runtime boundary** (section 2.3) - `rand` first, because the
   recipes hang on it.
4. **The music investigation** (I23; section 5), which decides what section
   2.4 becomes, and the decoder's replacement or removal that follows from
   it.
5. **State 2 proved** by the tracer - after the audio, since state 2 is
   defined with section 2.4 done and a proof that exempts the decoder's
   range is not the measurement (the owner, 2026-10-05: the order of these
   two reversed from this plan's first writing). Then **state 3** - our own
   executable, which the owner has named the first goal of the phase 4 / 5
   work.

Steps 1 to 3 need no decision the project has not already made. They can
follow round fourteen directly. Step 4 opens with the listening set for the
owner's ear (section 5).

## 5. For the owner

- **Our own executable (state 3): answered 2026-10-04.** The owner: it is
  the first goal of the phase 4 / 5 work, not for what it buys alone but as
  what several other goals pass through - a build made from the player's
  disc or their PC game ([`ASSET_SOURCES.md`](ASSET_SOURCES.md)), 64-bit
  builds, and the like. So it is planned as the route to those, and its
  design choices (how the data sections are mapped, what the loader takes
  from which source) are judged by what they do for them. Not scheduled:
  round fourteen and section 4's steps 1 to 4 come first (state 2's proof, step 5, with them).
- **The decoder: an investigation first (the owner, 2026-10-04).** Before
  any replacement is chosen the owner wants to hear how the PC's MP3s differ
  from the disc's sequenced music, to judge whether two music paths are
  worth keeping as a configuration choice. That is I23, and its method is
  written and unrun ([`bgm-comparison.md`](bgm-comparison.md)): loop points,
  reverb, codec, the track map. So section 2.4 is **not scheduled** and its
  shape waits on that answer - one path or two, and which decoder if any.
  The first step is a listening set: a few tracks rendered both ways, side
  by side, for the owner's ear. **Answered:** two paths (2026-10-06: the
  MP3s kept, their seams fixed as DIV-0081; 2026-10-08: the disc's music
  by default where a disc is held, DIV-0087), so a PC-only player still
  needs a decoder - minimp3 (2026-10-10, [`mp3-decoder-choice.md`](mp3-decoder-choice.md)).
- **`EffectKindA8_DrawBar` and the other owner's calls** are round
  fourteen's, not this plan's.

## 6. Not verified

Everything in section 1's table beyond the counts: which rows are game code,
which are dead under ours, the runtime's called entry points, the decoder's
lineage and seam beyond the names in `symbols.toml`, and whether mapping the
data sections alone is sufficient (the resources section and anything the
start-up code initialises before `WinMain` are unread).
