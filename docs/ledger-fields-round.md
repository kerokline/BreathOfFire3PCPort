# The ledger-fields round: staging (IDEAS I33, early decision 1)

**Status:** STAGED (2026-10-27; not launched. The owner's four calls in
section 6 are answered; LF0 can start.)

The owner's word, 2026-10-27: stage "the ledger kinds work" as a round. The
source is [`IDEAS.md`](IDEAS.md) I33's first early decision - *tag the
ledger's entries by kind, a column or a field, checked by
`tools/ledger_check.py`* - and the product it is for: **presets**, one choice
a player makes ("the PC port as it was", "the PlayStation's behaviour",
"ours as recommended") in place of 183 switches.

The measurement below changes the shape of that job. The kind is already
there (the Tier field). What is missing is that the ledger cannot say, in a
form a tool reads, *what turns an entry off and whether a player can*. In the
player's build the answer today is mostly "nothing can". So the round is
two halves: make the ledger machine-readable (stages A to C, docs and tools,
no behaviour change), then give the player's build one switch it reads and
build the presets on it (stage D, after the owner's calls).

## 0. What the ledger is today, measured 2026-10-27

On `main` `e84aaf6d`, by a script over [`DIVERGENCE.md`](DIVERGENCE.md):

| What | Count |
|---|---|
| Entries | 183 (DIV-0001..0183, DIV-0067 withdrawn) |
| Tier: Sensible / Intent / Forced / Extension | 114 / 53 / 10 / 7 (the template's line excluded) |
| `Reversible?` naming `BOF3X_ORIGINAL=<name>` | 133 lines name it; by entry, 118 |
| `Reversible?` naming `BOF3X_LANG` | 37 mentions, 15 entries whose only switch is the language |
| `Reversible?` that starts "No" | 6 entries (8 lines) |
| `Reversible?` naming something else (`BOF3X_OPT`, `MUSIC`, `LAYERING`, `WIDE_STAGING`, `PRESENT`, an ini key...) | 44 entries |

**The tier already is the kind.** The Tiers table (`DIVERGENCE.md`, "Tiers")
was written for presets - "which is what a player-facing preset ('as
shipped', 'fixed', 'extended') would be built from" - and `ledger_check.py`
already refuses an entry without one. I33's words "fix, enhancement, option"
map onto it: fix = Intent (and Forced, which no preset can undo),
enhancement = Sensible, rule or content change = Extension. "Option" is not
a kind but a property: whether a switch exists. This round adds no fourth
vocabulary; it adds the fields a preset needs that the tier does not hold.

**The switch the ledger names is not one a player has.** `BOF3X_ORIGINAL`
is the A/B switch: in the hosted build (`BOF3.exe` + `bof3x.dll`) it leaves a
named function to Capcom, or skips a named patch. In the player's build
(`bof3x.exe`) it is refused at start-up - `src/hook/exe_host.cpp`
`RefuseHostedSwitches`, "the A/B switch runs Capcom's code, which bof3x.exe
does not map" - and the player's launcher hands any command that sets it to
`bof3x-launcher-dev.exe`, which a release does not ship
([`launcher-settings.md`](launcher-settings.md) 2a). So **118 entries are
reversible on the developer's machine only**. The ones a player can switch
today are the launcher's own boxes (language, music, layering, wide, look,
the optional layers).

**What it would take to give a player each switch.** A first cut by script
(names in `Reversible?` against `symbols.toml` `[[func]]` names and the
string arguments of `WantsOriginal` / `PatchBytes` in `src/`; LF0's tool in
section 2 replaces it):

| How the entry is switched today | Entries | What a player switch needs |
|---|---|---|
| a name our own code reads (`PatchBytes("SunderLoop", ...)`, a `WantsOriginal` branch in ours) | 63 | read the name from a switch both hosts accept: cheap |
| a detour name only: the original behaviour is Capcom's function, which `bof3x.exe` does not map | 49 | the original path written into ours beside the change - real work per entry, or "not reversible in the player's build" |
| a launcher / ini / environment key of its own | 44 | already a player's switch; record it |
| the language alone | 15 | the language box is the switch |
| "No" | 6 | Forced, or a one-way change; record why |
| more than one kind of name | 6 | read by hand |

## 1. The fields

Proposed (the owner's call 1, section 6). Each is one line in every entry,
after `Reversible?`, which stays as the prose it is. The checker reads the
new lines; nobody reads the prose for a preset.

- **`Switch:`** the machine-readable name or names that turn the entry off,
  comma-separated, or `none`. A name is a `BOF3X_ORIGINAL` name as today
  (`SunderLoop`), or `ini:<key>=<value>` for a launcher setting
  (`ini:layering=0`), or `lang` when the language is the switch.
- **`Default:`** `on` or `off` - whether a player who changes nothing gets
  the new behaviour. Most are `on`; a cheat or an opt-in look is `off`.
- **`Player:`** `yes` (a player's build can turn it off today), `cheap` (the
  original behaviour is in our code already; only the switch is missing),
  `costly` (the original is only Capcom's function; ours must grow the old
  path), `never` (Forced, or one-way - the tier or the prose says why).
  Measured from the source by LF0's tool, checked by each group.
- **`Toward:`** a comma list of the releases whose behaviour the new
  behaviour matches - `psx`, `psp`, `pc` - or `none` alone. No value implies
  another: the PSP release is built from the PlayStation's but changed some
  things (I32), so a cascade from `psx` to `psp` would be false exactly where
  it matters. Examples: `psx, psp` restores what both consoles do; `psx`
  alone where the PSP changed it; `psp` alone for one of the PSP's own
  changes; `pc` where the consoles differ and the PC port's way is kept or
  chosen; `none` for behaviour no release had (DIV-0176, Sunder's unused
  loop). Evidence: `psx` from the entry's "Also in the PSX version?" line
  and rationale; `psp` only where it is shown - a PSP twin read, an entry
  of the PSP rounds - and otherwise left off, with the entry listed in the
  group's report. The list never claims more than was measured. The presets
  read it: *PlayStation* turns on every entry that lists `psx`; a later PSP
  preset (I32) every entry that lists `psp`. Two entries that are
  alternatives to each other (one the PlayStation's way, one the PSP's) are
  listed as a pair by the audit.
- **`State:`** `active`, `withdrawn` or `superseded by DIV-NNNN`. Today the
  withdrawn and superseded entries say so in prose; a preset must not try to
  switch them.

`tools/ledger_check.py` refuses a missing or malformed field once stage C
merges, refuses a `Switch:` name that nothing in `src/` or the launcher
reads, and refuses `Player: yes` on a name the player's build cannot reach
(the hosted-only list in `exe_host.cpp`).

## 2. Stage A: LF0, the schema and the tool (one agent, alone, first)

Owns `tools/ledger_check.py`, a new `tools/ledger_switches.py`,
`DIVERGENCE.md`'s "Entry format" section (not the entries), and a section of
this doc ("LF0 as run").

1. The field definitions of section 1 (as the owner settled them) into the
   Entry format, with one example per value.
2. `tools/ledger_switches.py`: every switch name `src/` reads - the string
   arguments of `WantsOriginal`, `PatchBytes`, `RetargetCall`, the `Inject`
   / `BOF3_INJECT` names, every `BOF3X_*` environment name, every ini key
   the launcher writes (`src/launcher/`) - each with its file and line and
   one class: `code` (read by our code, so the player's build could read it),
   `detour` (only skips our function), `launcher`, `hosted-only`. Then for
   every ledger entry: its names from `Reversible?`, their classes, and the
   `Player:` value that follows (the `cheap` / `costly` split of section 0,
   per entry). Output a TSV under `analysis/` and the counts; no game data,
   so it runs in CI.
3. `ledger_check.py`: parse the five fields; in this stage a missing field
   is a note, not an error (the entries are filled in stage B). The
   cross-checks of section 1 written and tested against two hand-filled
   entries (DIV-0176, DIV-0183) as fixtures.
4. Fill those two entries as the worked examples the groups copy.

Done: `ledger_check.py` exits 0 on `main`'s ledger with the notes counted;
the tool's TSV committed with its counts in this doc; the two examples in.

## 3. Stage B: the entries (four groups, in parallel, after LF0 merges)

| Group | Entries | Note |
|---|---|---|
| LF1 | DIV-0001..0046 | the oldest: the platform layer, the first text work, DIV-0021 / 0024 (Forced) |
| LF2 | DIV-0047..0092 | DIV-0064's label groups, the layering fix (0071), the disc-data defaults (0087) |
| LF3 | DIV-0093..0138 | the disc-only and PSP-only rounds, the button round |
| LF4 | DIV-0139..0183 | the localisation rows, the sound layer (0169..0175), the newest |

Each group, for each of its entries: the five fields, from the tool's TSV
and a read of the entry and its code. Where the tool's class and the prose
disagree (a `Reversible?` line naming a switch the code no longer reads, a
`Default:` the launcher contradicts), the group fixes the prose only if the
code shows the fact, and lists the case in its report. `Toward:` cites the
entry's own "Also in the PSX version?" line; where that line says `unknown`,
`Toward:` says `none` and the report lists it: the owner's rule since
2026-10-05 is that the PlayStation twin settles where a behaviour came from,
so those are read against the twin after the round.

Each group edits only its own entries' hunks of `DIVERGENCE.md`, so the
merges are disjoint. No game runs; no source change. A group that finds a
real defect (a switch that does nothing, a default that is wrong) describes
it in its report; the coordinator numbers it (D-numbers collide otherwise).

## 4. Stage C: the audit, then the strict rule

**LF-audit** (one agent, read-only, after the four merge): every
`Player: never` and `costly` re-derived from the code; every `Toward: psx`
against the entry's evidence; a sample of ten `cheap` and ten `yes` per
group checked line by line. Findings to the coordinator, fixes on one
branch. Then the checker turns strict (a missing or malformed field is an
error), and the counts go into this doc and `STATUS.md`: how many entries a
player can switch today, how many cheaply, how many at cost.

## 5. Stage D: the player's switch and the presets (after the owner's calls)

Not to be briefed until section 6's calls 2..4 are answered; the shape:

- **One switch both hosts read** - a variable such as `BOF3X_OFF=<names>`
  and its ini key - for every `Player: cheap` entry: the code-read names of
  section 0 read through it as well as through `BOF3X_ORIGINAL`, so the
  hosted A/B meaning is unchanged. `bof3x.exe` refuses a name it does not
  know, loudly (rule 4), not silently.
- **A generated registry** from the ledger's fields (`data/divergence.toml`
  or a generated header): id, switch, default, player, toward, tier. The
  launcher's presets read it; the in-game log names the preset in force.
- **The presets** in the launcher's Configure page, in ASD-STE100 text (the
  launcher is player-facing text, CLAUDE.md conventions): a box with the
  owner's preset names, and the list of what each changes.
- **The proof** for each `cheap` switch made a player's: the state hash of
  the exe host with `BOF3X_OFF=<name>` against the hosted build with
  `BOF3X_ORIGINAL=<name>`, on a route that reaches the entry, equal page for
  page. One batched run by the coordinator, game runs serialised.
- Each change that a player can now make is not itself a divergence (the
  default is unchanged); the ledger's `Reversible?` lines are updated, and
  DIV entries are added only if a default moves.

The `costly` entries are listed for a later round, each with its size;
none is grown in this one.

## 6. For the owner

1. **The field set** of section 1: the five names and their values. Is
   `Toward:` the right basis for a "PlayStation" preset, or should that
   preset be its own list?
2. **The presets**: which ones, and their names. The proposal: *As
   shipped* (every entry a player can switch, off; Forced stays),
   *Repaired* (Intent and Forced only), *Recommended* (the defaults),
   *PlayStation* (every `Toward: psx` on, every `Toward: pc` off). I32's
   PSP changes would be a fifth, later.
3. **The player's switch**: a new name such as `BOF3X_OFF` beside
   `BOF3X_ORIGINAL`, or one name for both with the exe host taking only the
   code-read ones. The proposal is a new name: `BOF3X_ORIGINAL` keeps
   meaning "Capcom's code", which the player's build has none of.
4. **The 49 `costly` entries**: leave them not switchable in the player's
   build ("As shipped" then says "as near as this build allows" and lists
   them), or grow the original path in ours for some. The proposal: leave
   them, list them, and let a later round take the ones a player asks for.

**Answered 2026-10-27 (the owner):** call 2, the four presets as proposed.
Call 3, "anything sensible, as long as it is well documented": the name is
`BOF3X_OFF` (a comma list of `Switch:` names, the same list syntax as
`BOF3X_ORIGINAL`, `*` and `-name` included) with the ini key `off=`; stage D
documents it in [`launcher-settings.md`](launcher-settings.md) and the
ledger's Entry format. Call 4, leave them: "most of them were not desirable
/ true fixes". Call 1, the five fields as section 1 gives them, with
`Toward:` a list and no cascade (the owner asked how an entry matching both
consoles is tagged; section 1 now says).

## 7. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/ledger_check.py`, `tools/ledger_switches.py`, `analysis/ledger_switches.tsv` | LF0; the audit's fix branch after |
| `DIVERGENCE.md` "Entry format" | LF0 |
| `DIVERGENCE.md` entries | LF1..LF4 by range; the audit's fix branch after |
| this doc's "as run" sections | each group its own; keep both sides |
| `STATUS.md`, `HANDOFF.md` | the coordinator |

Merge order: LF0; LF1..LF4 one at a time with `ledger_check.py` after each;
the audit; the strict rule. Every commit `git commit -s` (rule 7). No game
data in any field or report (rule 1). No D- or DIV-numbers assigned by a
group.

## 8. The brief, per group

The briefs are in `C:\Users\kerok\bof3x-runs\lf1027\` (`brief_common.md`
and one per group). Each names: the goal; the worktree's reset target (the
round branch's tip, not `main`); the files it owns; what to read first
(this doc, `DIVERGENCE.md` "Entry format" and "Tiers", `CLAUDE.md` rules 2,
4 and 7); the checks that mean done; the report's shape. A usage cut
resumes with a message naming the next step; only the report means merge.

## 9. Not in this round

- Growing the original path for `costly` entries.
- I32's PSP game-logic changes as a preset (its own entry).
- The in-game options screen (I33 "Settings in the game"); the presets live
  in the launcher until that exists.
- Mod layers (I33's second early decision).
