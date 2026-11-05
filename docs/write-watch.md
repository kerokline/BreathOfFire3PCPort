# The write watch: who writes a word of the image (`BOF3X_WRITEWATCH`)

**Status:** BUILT (2026-11-05), a diagnostic; tried on one scene. Made for
D226 ([`known-defects.md`](known-defects.md)): on the owner's first meeting with
Manmo, area 196's sand hill shades `0x65EF0C..0x65EF17` were already zeroed,
and only the hill's own fade is known to write them. Static reading found no
other writer, in Capcom's exe or in ours (section 3). Only a write from a
computed address is left, and a scan cannot see one. The CPU can: a debug
register names the instruction.

## 1. Use

```
BOF3X_WRITEWATCH=<hex VA>[:<length>][,<hex VA>[:<length>]...]
```

- Each range is split into aligned four-byte words. Four words at most, one
  for each debug register. A length is 1..16 bytes. A VA that is not on a
  writable page of the image stops the game at start, with a message.
- Each word goes in a write-only data breakpoint on the main thread.
  `src/hook/write_watch.{h,cpp}` loads them after the injects, beside
  `BOF3X_DRAWORDER`, in both hosts. It is refused beside `BOF3X_GAPWATCH`,
  which uses the same registers.
- On a write, the handler logs one `writewatch` line:
  - the word's value before and after the write;
  - the writing instruction (our DLL's as `bof3x.dll+offset`, Capcom's image
    as `exe:VA`);
  - the return addresses into our DLL found on the stack;
  - the logic frame, the recipe frame and the area.
- A writer is logged at its first write and then every 256th, so a fade
  that writes each frame does not flood the log. A new writer is always
  logged.

For D226, set beside an ordinary play session (the launcher passes
`BOF3X_*` variables through):

```
BOF3X_WRITEWATCH=65EF0C:12
```

Then read the log with the writers named:

```bash
python tools/write_watch.py game/bof3x.log
```

`tools/write_watch.py` names `bof3x.dll` offsets with `llvm-symbolizer`
(`tools/gapwatch.py`'s `Symbolizer`). For that, `--dll` must be the build that
wrote the log; the default is `build/bof3x.dll`. It names `exe:` addresses by
the `symbols.toml` function that holds them.

## 2. What it shows on the scene

The crafted save at the start of the area 196 scene (D226's repro,
`C:\Users\kerok\bof3x-runs\manmo1105`, recipe `manmo2.txt`), 16:9:

| Words | Frame | Writer | What |
|---|---|---|---|
| all three | 422, area 196's load | `AreaDataReload_OnLoad` (`area_data_reload.cpp`) | DIV-0182's put-back, `80808080` written over `80808080` |
| all three | from 1413 | `EffectKind18Sub59_Fade` (`effect_6c.cpp:1303`, `:87`) | the hill's fade, two a frame, each byte in turn |

No other writer appeared. A writer that is not one of these two, in a long
session, is the D226 culprit.

## 3. Limits

- **The main thread only.** The game's logic and its task stacks run there.
  A write from another thread (the audio or the window thread) is not
  caught.
- **A write from inside a system call does not trap**, for example a file
  read straight into the word.
- **The callers are a scan, not a walk.** The handler takes the words on the
  stack that point into our DLL just after a call (`crash.cpp`'s filter). An
  old return address left on the stack can appear among them. On the
  scene's put-back, `Music_Play` sits above `Area_Enter`.
- **The registers belong to the thread.** Code that rewrote the main
  thread's debug registers would end the watch without a line. No such code
  is known: `gapwatch.cpp` is the only other user, and the two refuse each
  other.

## 4. Static reading behind it (2026-11-05)

- **Capcom's exe:** every instruction of `BOF3.exe` that names an address in
  `0x65EEF0..0x65EF1F` is the hill's own code (`0x512C41..0x513192`). Its
  only writes are the fade's.
- **Ours:** no literal address in that range appears in `src/` outside
  `effect_6c.cpp`. `kShades59` has four users:
  - the hill's draw, which reads;
  - the hill's fade, which writes, as Capcom's does;
  - the effect's self-test fuzz, which runs only under `BOF3X_SHADOW` and
    leaves `80` x12 for the game;
  - DIV-0182's put-back.

  The state tables on either side are dispatched through bounded counts:
  sub-kind 0x55's three words before the shades, and sub-kind 0x59's six
  after them.
- **Other spawns:** the hill's spawn op, `E0 18 .. .. .. .. 59`, matches only
  in area 196's placement script (`0x648F97`). In the install's `DAT` files
  and the cache it matches seven times, all inside large kind-2 blocks, not
  scripts.
- **Quickload:** a quickload rewrites the whole data span from its file, but
  the owner's game folder holds no quicksave files (`BISLPS1x.DAT`).
