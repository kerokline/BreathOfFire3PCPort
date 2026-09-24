# The fourth round's queue: what is left, and the owner's display fixes

**Status:** DONE (2026-09-23) - all five groups merged; oracle, memory dump and
frame hash through `ab25` (the capture A/Bs owed, `analysis/validate_ab25b.sh` -
since run with `ab26`: "every pair identical and none black", HANDOFF round six's batch)

Drawn from the regenerated catalogue (`python tools/attract_catalog.py
analysis/calltrace/hidden_b/bof3x.callcounts.tsv --also ...`: 276 reached
and not ours). **The attract sequence's game logic is nearly exhausted**:
of §4.5 and §4.6's five, `0x576CD0`, `0x577800`, `0x577B80` are case blocks
of functions already ours and `0x56B730` / `0x56B990` are the case labels
group A met ([`attract-remaining.md`](attract-remaining.md) §3); §4.9's two
are a bare `ret` and `Port_DroppedCall`. What is left is the renderer (41),
sound (23), the PSX library layer (10), and the functions no catalogue sees
that the third round named. Counts are `hidden_b`'s; sizes are
`pe_funcs.py`'s and some will be wrong.

## N - the owner's display fixes -> `src/game/glyph_draw.cpp`

The glyph handler **`0x5A2900`** (692) and its texture lookup `0x5A2BC0`
(215; its callee `0x5A2CA0`, 457, builds the texture - take it or give it a
stand-in), taken over faithful, then **D17's fix** as DIV-0025
([`known-defects.md`](known-defects.md) D17: texel centres, `(2u + 0.5) /
32`). This group also builds **the vertex-block fuzz** R will reuse: ours
and a clone each fill the four `D3DTLVERTEX`s at `0x7CA958`, the device at
`0x7CC350` is a fake COM object whose `+0x58` / `+0x70` record their
arguments, and `0x59FBA0` / `0x59FCA0` / `0x59FD80` / `0x5A2BC0` are
recording stand-ins. Name those helpers in `symbols.toml` (R inherits the
names). Then, patches rather than takeovers:
the **Config controller panel** as DIV-0026
([`config-screen.md`](config-screen.md) §8, `0x461AF0`), and **the save
screen's Yes / No** as DIV-0027 - read first, then the owner's layout
([`dialogue-localisation.md`](dialogue-localisation.md) §6 item 8).

## O - the area's entry list and the drop-in party -> `src/game/area_entry.cpp`

`0x5951D0` (0x177, the entry list `GameMode_Enter` walks on input bit 1),
`0x531F90` (0x227, the drop-in party placement, 94 call sites) and what they
call that is not ours: `0x5321C0`, `0x5322D0`. Callers and context in
[`mode-flow.md`](mode-flow.md) §1. Calls ours: `Kind2_Place`, `Flags_Test`,
`Area_ClassifyPending`, `Area_PickMusic`.

## P - the title's hidden cluster -> `src/game/title_tasks.cpp`

*(As built: the cluster was the field menu's item use - `src/game/item_use.cpp`,
fifty-nine functions, [`item-use.md`](item-use.md).)*

The 33 pointer-reached functions at `0x496CC0`..`0x497680`, after
`Title_LoadTask`, that `pe_funcs.py` folded into `0x496AD0`'s 0xBAA bytes
(its own body is 0x88); and group H's two, `0x4981C0` / `0x4983C0`, behind an
indirect call through the area descriptors ([`msgbox.md`](msgbox.md) §4).
First establish the edges - no list has these. Calls not ours: `0x461EB0`,
`0x498A30`, and a family at `0x590660`..`0x590F60`.

## Q - sound -> `src/game/sound.cpp`

§4.12's 23: `Sound_PlayEffect` `0x587740`, `0x587A20`, `Music_Play`
`0x587AE0`, `Music_FadeOutStop` / `FadeIn` / `FadeOut`, the per-loop
`0x587C70` and `0x5A7230` (6.7 million calls each - keep them cheap), and
the `0x5A69C0`..`0x5A70A0` layer under them. `Snd_LoadBank` `0x587CD0` is
ours. DirectSound calls and the MP3 decoder (`0x5ADF00`.., out of scope)
get recording stand-ins. The live check covers sound only by ear: the
owner listens.

## R - the Direct3D draw handlers (after N) -> `src/game/d3d_draw.cpp`

The dispatch `0x59EE50` (208, two jump tables), the handlers `0x5A0C40`
(573, 2.2 million calls), `0x5A14C0`, `0x5A17A0`, `0x5A1D10`, `0x5A20D0`,
`0x5A2EB0` + `0x5A3160`, and the helpers `0x59FBA0` / `0x59FCA0` /
`0x59FFE0` - on N's vertex-block fuzz. `0x5A2300` is DIV-0010's copy and
stays as it is unless R takes the `SPRT` handlers whole.

## Boundaries

None among N, O, P and Q: no unowned callee is shared (a direct-call scan of
each group's roots, 2026-09-22). N and R share the helpers; R starts from
N's merge. Numbers, to keep merges clean: DIV-0025..0027 are N's; new
defects are O's from D18, P's from D21, Q's from D24, R's from D27.

## Left out on purpose

§4.1 the Windows shell, §4.2 the task system, the MP3 decoder and the CRT,
as in [`takeover-queue-round3.md`](takeover-queue-round3.md).
