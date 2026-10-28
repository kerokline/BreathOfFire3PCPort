# The ledger-fields round: staging (IDEAS I33, early decision 1)

**Status:** STAGES A..C DONE (2026-10-28, on `ledger-fields/round`, unpushed;
stage D not briefed. The "as run" sections of 2, 3 and 4 say what landed.)

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
- **`Languages:`** (added 2026-10-28, the owner's call: "I'd rather the line
  allow a list than use value sets") the languages under which the entry's
  new behaviour acts, so that the launcher's language pick selects the
  entries that belong to it: `any` alone (the entry does not depend on the
  language chosen), or a comma list of distinct tags from the engine's list
  (`tools/language_tags.py` `TAGS`: `en-US`, `en-150`, `fr-FR`, `de-DE`,
  `ja-JP`) and `original` (the port's own text, `BOF3X_LANG` unset). No
  class names: a Latin-gated entry lists the four Latin tags, so a German- or
  French-only entry can be said. The code decides the list, not the prose;
  a layer built from a disc the install may lack is still listed. `Switch:
  lang` requires a list; a list means the language pick governs the entry
  (on under those languages, inert under the others, whatever a preset
  says), and its `Default: on` reads `on - under its languages`.
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

### LF0 as run (2026-10-28, from `main` `fec0624c`)

**What landed.** `DIVERGENCE.md`'s Entry format has the five lines and a
section, "The switch fields", with the grammar and one example per value.
`tools/ledger_switches.py` writes `analysis/ledger_switches.tsv` (a
`.gitignore` exception beside `data_census.tsv`) and prints the counts below;
`--check` (in `checks.yml`) fails when its readers stop finding a kind of
name, or when the exe host's `RefuseHostedSwitches` and the launcher's
`HostedOnlyAsked` disagree. `tools/ledger_check.py` reads the five fields:
malformed is an error now, missing a counted note until `STRICT_FIELDS`
(stage C). DIV-0176 (`Switch: SunderLoop`, `Player: cheap`) and DIV-0183
(`Switch: Paralyzer_Start`, `Player: costly`) are filled; every run of the
checker tests its field checks on those two with one line changed (37
cases).

**For the groups.**

- The TSV has two kinds of row. `switch` rows: every name, its class, the
  `Player:` value it gives alone, every `file:line` that reads it. `entry`
  rows: the names the entry's `Reversible?` gives with their classes, the
  best `Player:` value among them, and a note (tier, kind, a long prose
  field, a prose that starts "No" but names a switch). The entry rows are a
  starting point read from prose, not the answer: `Switch:` is the narrowest
  set of names that turns the entry off, which the group picks.
- `Switch:` forms: a bare `BOF3X_ORIGINAL` name, `ini:<key>=<value>`,
  `env:<BOF3X_NAME>=<value>` (added here: section 1 has no form for an
  environment name the launcher has no key for), `lang`, `none`. Section 1's
  example `ini:layering=0` is not a key: the launcher has no `layering=`
  (`ConfigLoad`, `src/launcher/config.cpp:164..279`), so DIV-0071's
  would be `env:BOF3X_LAYERING=0` (LF2's call).
- `Player:` must equal what the `Switch:` names give (the worst of them), or
  be `never`; `never` for a Forced entry and for `Switch: none`.
- The classes are six, not four: `env` (an environment name both hosts read
  that the launcher does not set: `yes`) and `instrument` (the input
  recorder's, the input script's, the state hash's and the WAV dump's
  hooks on the input latch: no name switches them off) join `code`, `detour`, `launcher`, `hosted-only`.
- DIV-0169 already has a `- **State:**` line, on the DLL state it keeps
  (2026-10-23). The checker skips it (`FIELD_CLASH`) and fails once it is
  renamed; LF4 renames it.

**Section 0 again, by the tool**, on `fec0624c` (184 entries, DIV-0184
added since the first cut):

| What | First cut (`e84aaf6d`) | The tool |
|---|---|---|
| Tier: Sensible / Intent / Forced / Extension | 114 / 53 / 10 / 7 | 114 / 54 / 9 / 7 |
| `Reversible?` naming `BOF3X_ORIGINAL=` | 118 entries | 119 entries (145 mentions) |
| `Reversible?` naming `BOF3X_LANG` / the language alone | 37 mentions / 15 | 36 entries / 15 |
| `Reversible?` that starts "No" | 6 entries | 4 (DIV-0044, 0082, 0095, 0141) |

The Forced count: the first cut's 10 is 9 entries and the template's
`Forced | Intent | ...` line, whose first word it read; its row sums to 184
on 183 entries. Intent's one more is DIV-0184. The "No" count is by field
start; DIV-0044 and DIV-0082 start "No switch of its own" and name a detour,
so they are counted by that name below.

| How the entry is switched today (by its narrowest switch) | First cut | The tool | `Player:` it gives |
|---|---|---|---|
| a name our code reads (`PatchBytes`, `RetargetCall`) | 63 | 55 | cheap |
| a detour name only | 49 | 52 | costly |
| a launcher / ini / environment key | 44 | 24 | yes |
| the language alone | 15 | 15 | yes |
| nothing: "No", "Not a switch", "Nothing to reverse" | 6 | 5 | never |
| a code name and a detour name (alternatives) | 6 | 15 | the code name's |
| no switch name the tool can read (build-time, data, files) | - | 18 | by hand |

By the best switch each `Reversible?` names: **74 entries `yes`, 52
`cheap`, 35 `costly`, 5 `never`, 18 by hand.** Ten `Reversible?` fields are
longer than 600 characters (amendments, verification); their names include
diagnostics, and the group reads them (DIV-0160's best is `BOF3X_STPVIEW`,
a magenta debug view, `src/render/render_d3d11.cpp:859`; its switch is
`StpBlend`). Three entries name a hosted-only switch (`BOF3X_MP3=original`:
DIV-0091, 0093, 0094). No `Reversible?` names a switch `src/` does not read.

Where the first cut differs, and why (it was not kept, so this is read from
its own description, not re-run): **no `WantsOriginal` branch exists in
ours** - `WantsOriginal` is called only inside `src/hook/detour.cpp`
(`Inject`, `RetargetCall`, `PatchBytes`), so "a name our own code reads" is
exactly the `PatchBytes` / `RetargetCall` names (83, wrappers such as
`config_text.cpp`'s `Patch` traced); the tool puts an entry with both a
code name and a detour name in the alternatives row (15) rather than in
`code`; and the first cut's 44 "something else" held the 18 entries with no
runtime switch at all (a re-encoded file, a build option, the importer's
sources), which are now their own row. Names in all: 83 `code`, 10,088
`detour` (every `Inject`), 4 `instrument`, 47 `launcher` (26 ini keys, 20
environment names the launcher sets, `lang`), 200 `env`, 4 `hosted-only`.
`BOF3X_OFF` is not read by `src/` yet; when it is (stage D), `cheap` becomes
`yes` and the checker says so.

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

### Stage B as run (2026-10-28, off the round branch at `7a6bb918`)

Four Opus agents, one worktree each under `C:\Users\kerok\bof3x-runs\lf1027`,
launched together after LF0 merged and merged one at a time (LF3, LF4, LF2,
LF1) with `ledger_check.py` after each; every merge was clean, each group's
diff additive field lines in its own hunks plus the `Reversible?` corrections
below. LF4 also took DIV-0184, added since the table above. Reports:
`lf1027\LF<n>\REPORT.md`. No game ran, no source changed.

**The 184 entries, by field** (counted over the merged ledger; the audit of
section 4 may move some):

| Field | Values |
|---|---|
| `Switch:` | a bare name 93, `ini:` 19, `env:` 16, `lang` 20, `none` 27, more than one name 9 |
| `Default:` | on 155, off 29 |
| `Player:` | yes 55, cheap 67, costly 26, never 36 |
| `Toward:` | `psx` 87, `psp` 13, `psx, psp` 3, `none` 81, `pc` 0 (90 list `psx`, 16 list `psp`) |
| `State:` | active 182, withdrawn 2 (DIV-0037, DIV-0067) |

Against LF0's by-best-switch count (74 / 52 / 35 / 5 / 18 by hand): the
groups' narrowest-switch rule moved entries both ways - a `PatchBytes` flag
found beside a detour made eleven cheap (LF2: DIV-0058..0061, 0065, 0070,
0075, 0076, 0078, 0083, 0084), and the rule that a wider switch stays in
prose made the language-only, build-time and file-decided entries `never`
(LF3's 17 are the install's sources, DIV-0096..0106, 0114, 0115, 0130,
0131, plus our own exe, the icon and the suites page).

**`Reversible?` lines corrected** (the code showed the prose wrong; one
line each, the case in the group's report): DIV-0006 (`MsgBox_DrawChar`
no longer turns it off: our stepper calls it itself, `src/game/msgbox.cpp`),
DIV-0012 (`Gfx_FilterPoint` patches Capcom's set-up only; the switch is
`filter=`), DIV-0087 (an empty `cache=` is the home's own cache,
`src/launcher/config.cpp` `ConfigApplyEnvironment`), DIV-0150 (the ini's
`music=seq` is mapped to a source by `ConfigMusicSource`; only the
variable set before the launcher gives the old order). DIV-0169's older
`- **State:**` line (the DLL state it keeps) is now `Kept state:` and the
checker's exception for it is gone; the Status line counts DIV-0037
withdrawn beside DIV-0067.

**Rules the groups applied differently** (handed to the audit, section 4):
`Default:` for `lang`-only entries (LF1 `off`, LF2 `on`); the switch for an
entry that acts only under a language overlay (LF3 the detour, `costly`;
LF1 `lang`, `yes`; LF4 `none`, `never`); a code or detour name narrower
than a launcher box that also turns off a neighbour (DIV-0049, 0133/0139,
0071/0151, 0155..0157, 0160). The checker's note "Toward lists psx and the
PSX line starts Yes" fires on 28 entries where "Yes" means the new
behaviour is the PlayStation's; the audit says whether the test or the
lines change.

**`Toward: none` for want of a read** (the twin settles them later, the
owner's rule of 2026-10-05): DIV-0004, 0027 (LF1); 0058, 0059, 0063, 0071
(LF2); 0121, 0127 (LF3); 0144, 0180 (LF4). `psp` is left off every `psx`
entry without a PSP read; the ones the groups name as worth one: DIV-0064,
0080.

**For the owner, from the groups** (no number assigned): DIV-0036's switch
is `ini:display=windowed, env:BOF3X_SCALE=2` because the launcher sets
`BOF3X_SCALE` only when `scale=` is not 2 (`src/launcher/config.cpp:529`),
so `scale=2` alone does not fix the factor on a scaled desktop (the audit
verifies); DIV-0021 writes `0000` where the PlayStation stores the sign
extension of m[2][2], so it matches the console only for a non-negative
element - a behaviour change if taken, not this round's; DIV-0003's
original path is a crash, so `never - one-way` may fit it better than
`costly`. The `costly` entries with their sizes are in each report (LF1's
0031 and 0032, the renderer and the window, are the ones that cannot be
grown in practice).

## 4. Stage C: the audit, then the strict rule

**LF-audit** (one agent, read-only, after the four merge): every
`Player: never` and `costly` re-derived from the code; every `Toward: psx`
against the entry's evidence; a sample of ten `cheap` and ten `yes` per
group checked line by line. Findings to the coordinator, fixes on one
branch. Then the checker turns strict (a missing or malformed field is an
error), and the counts go into this doc and `STATUS.md`: how many entries a
player can switch today, how many cheaply, how many at cost.

### Stage C as run (2026-10-28, off the round branch at `c011e323`)

**The audit** (one Opus agent, read-only; `lf1027\LFaudit\REPORT.md`, 434
lines): every `never` and `costly` (62 entries) re-derived from the code
before the group reports were opened, every `Toward: psx` (90) against its
entry's evidence, 43 `cheap` and 42 `yes` line by line, every `ini:` /
`env:` `Default:` against a table of the launcher's defaults built from
`ConfigLoad` and `ConfigApplyEnvironment`. Twelve entries disagreed, no
`Toward:` value did, and no `Default:` contradicted the launcher. The four
rules the groups had applied differently, as the audit recommended and the
coordinator took them (each now a sentence in the Entry format):

- **A, `Default:` for `lang`-only entries is `on`:** the field reads the
  player's choices and the install's contents as they are, so an entry that
  acts only under an overlay, a wide mode or a cache is `on` (LF2's
  reading; LF1's ten went `off` to `on`). Read the other way, "Recommended"
  would change the player's language, which a disc-only install cannot set
  to `original` (`src/launcher/config.h:67`, `config.cpp:940`).
- **B, `Switch:` is the narrowest runtime name that turns the whole entry
  off**, `lang` counted as a runtime name, the best class winning a tie, a
  build-time handle never a switch. DIV-0009, 0013, 0141, 0148 went `none`
  / `never` to `lang` / `yes`; the five overlay entries whose own detour is
  narrower than the language (DIV-0051, 0118..0120, 0128) stay `costly`;
  DIV-0090 stays `none`.
- **C, the same rule against a launcher box:** DIV-0049's `Fmv_WndProc` and
  the shared names (`ButtonSigma`, `BOF3X_LAYERING`, `ruby=`, `DirectOpaque`)
  stand. DIV-0145 and 0147 were wrong: F5, F6 and F7 are read only in our
  `Game_WndProc` (`src/game/win_main.cpp:443, 483, 487`), its own `Inject`
  (`:919`), so they are `Game_WndProc` / `costly` like DIV-0040 and 0042;
  DIV-0048 takes the same name (F1, `:505`; its "one detour" clause was
  wrong, `:918..919`). DIV-0086 takes `ini:opt=` as DIV-0110..0113 do.
- **D, the "starts Yes" note is dropped:** all 22 fired falsely, `Toward:
  psx` right in each. The "Also in the PSX version?" line answers for the
  original behaviour; the Entry format now says so, and the 22 lines are
  not reworded. The "Default off and Player never" note fires only for
  `State: active`.

Judgement calls confirmed as written: DIV-0041 `psp`, DIV-0150 `psx`,
DIV-0160 `psx, psp` (a PSP capture of the entry's own case now counts as
`psp` evidence, since DIV-0184, the format's example, rests on the same
capture), DIV-0107, 0094 and 0105 `never`; DIV-0003 stays `costly`, its
original path a crash like DIV-0183's. DIV-0040's prose gained an
amendment (F7 quick-loads since DIV-0145, F11 writes a frame).

**One defect, numbered D395** ([`known-defects.md`](known-defects.md); ours,
the launcher's, open, not fixed in this round): `scale=2` in `bof3x.ini` on
a scaled desktop gives k = 3 at 150 % and 4 at 200 %, because the launcher
sends `BOF3X_SCALE` only when the value is not 2 (`src/launcher/config.cpp:529`),
unset the game takes k from the DPI (`src/game/display_setup.cpp:346..368`),
and `ConfigSave` always writes `scale=` back (`config.cpp:342`), so the ini
can neither pin k = 2 nor say "follow the display". DIV-0036 is amended to
say so; its switch `ini:display=windowed, env:BOF3X_SCALE=2` is right for
the code as it is.

**The fix branch** (`lf1027\LFfix\REPORT.md`): the audit's 28 entry lines
and 7 tool and format lines applied, every citation checked against the
source (the DIV-0108 clause cites `psp_code.cpp:150`, the audit's `:147..149`
corrected), the TSV regenerated (14 probe rows' line numbers). Then
**`STRICT_FIELDS = True`**: a missing or malformed field is an error; the
self-test grew a field-removal form (39 cases). `ledger_check.py` on the
round's tip: 184 entries, 0 errors, 4 notes (all five fields on 184 of 184;
the self-test's case count; the 29 data-only and tooling entries not cited
in `src/`, as before; the impl count).

**The ledger after the round** (184 entries, the counts `STATUS.md` carries):

| Field | Values |
|---|---|
| `Switch:` | a bare name 95, `ini:` 19, `env:` 16, `lang` 24, `none` 21, more than one name 9 |
| `Default:` | on 165, off 19 |
| `Player:` | **yes 59, cheap 67, costly 28, never 30** |
| `Toward:` | `psx` 87, `psp` 13, `psx, psp` 3, `none` 81 (90 list `psx`, 16 list `psp`, none `pc`) |
| `Languages:` | `any` 137; a list 47: the five tags 19, the Latin four 20, `ja-JP` 4, `fr-FR, de-DE` 1, `fr-FR` 1, `en-150` 1, `original` 1 (added after stage C, below) |
| `State:` | active 182, withdrawn 2 (DIV-0037, DIV-0067) |

So: 59 entries a player can switch today, 67 that stage D's `BOF3X_OFF`
makes a player's at no cost beyond the switch, 28 whose original path is
Capcom's alone (the owner's call 4: left, listed in the group reports with
their sizes), 30 that no preset turns off. The "PlayStation" preset would
turn on 90 entries; no entry lists `pc`, so that preset turns nothing off
by the field alone. Ten `Toward: none` rest on a read not done (the stage
B list above) and want the twin.

**The 28 `costly` entries, listed** (the owner's call 4, affirmed
2026-10-28: left, listed, a later round takes the ones a player asks for).
The size is the group's or the audit's estimate of growing the original
path in ours; "small" is one gate or flag around a path ours already has.

| Entry | Tier | `Switch:` | Growing the original path |
|---|---|---|---|
| DIV-0002 | Intent | `Save_WriteFile` | small: one `Save_ListFiles()` call to gate |
| DIV-0003 | Intent | `File_OpenWrite` | small, but the original path is a crash: not worth growing |
| DIV-0004 | Intent | `Gfx_BeginFrame` | small: gate the upload drain |
| DIV-0031 | Sensible | `Display_Setup` | very large: Capcom's DirectDraw / Direct3D 3 renderer; in practice cannot be grown |
| DIV-0032 | Sensible | `Game_WinMain, Game_WndProc, Cursor_Sync, Display_WindowMoved, Display_DeviceName` | large: Capcom's window and loop |
| DIV-0034 | Intent | `Game_WinMain` | small: gate the frame-debt clamp |
| DIV-0035 | Sensible | `Fmv_Play` | medium: Capcom's display mode-set around the FMV player |
| DIV-0039 | Sensible | `Game_WinMain` | small: choose Capcom's GBK strings over ours |
| DIV-0040 | Sensible | `Game_WndProc` | large: F7 device cycling and F11 overlays, which our back end lacks |
| DIV-0042 | Sensible | `Game_WndProc` | small to medium: refuse `WM_SIZING` in our window procedure |
| DIV-0044 | Intent | `Display_Setup` | small: drop the depth-0 primitive in `render_shim.cpp` |
| DIV-0048 | Sensible | `Game_WndProc` | small: F1 is read only in our window procedure |
| DIV-0049 | Sensible | `Fmv_WndProc` | small: the pause / resume pair already sits behind `WinMain_Background()` |
| DIV-0050 | Sensible | `DInput_Init, Pad_Read, DInput_Shutdown` | large: the whole DirectInput joystick path; probably not wanted |
| DIV-0051 | Sensible | `ConfigController, Config_DrawControllerCell, ConfigJapaneseIcons` | moderate: the original glyph cell of `Config_DrawControllerCell` |
| DIV-0066 | Sensible | `Fmv_Play` | small: the pad poll in our `Fmv_Play` pump |
| DIV-0082 | Intent | `Battle_CopyEnemyData` | small: one byte store |
| DIV-0091 | Sensible | `env:BOF3X_MP3=original` | hosted-only by design: Capcom's MP3 decoder is not in `bof3x.exe`'s image |
| DIV-0093 | Sensible | `env:BOF3X_MP3=original` | small on its own (one constant, `kCapcomEosBytes`, already in `src/audio/mp3.cpp`), but the switch is DIV-0091's |
| DIV-0118 | Sensible | `BattleExtra_EquipOpen` | small: one flag to copy from `0x669CE8` again |
| DIV-0119 | Sensible | `Boss26Fx_DrawCount` | small: a flag forcing our no-overlay draw |
| DIV-0120 | Sensible | `CommuHiLo_DrawChoice` | small: as DIV-0119 |
| DIV-0127 | Intent | `EffectKind18Sub61_DrawOverlay, Scena17_DrawLetterbox` | small: ours already emits Capcom's packets for the fuzz; a code name around that path |
| DIV-0128 | Intent | `Identify_DrawElements` | small: a flag forcing our `original`-language glyph path |
| DIV-0129 | Intent | `Sound_StreamDone` | small for the engine half (one flag); the jingles are the install's sources, no runtime switch |
| DIV-0145 | Extension | `Game_WndProc` | small: gate the F6 / F7 reads in our window procedure |
| DIV-0147 | Extension | `Game_WndProc` | small: gate the F5 read in our window procedure |
| DIV-0183 | Intent | `Paralyzer_Start` | the original path is a null read, a crash: not worth growing |

**Left for stage D** (section 5, not briefed): `BOF3X_OFF` read beside
`BOF3X_ORIGINAL` for the 67 `cheap` names; the `Reversible?` prose of
DIV-0141, 0145, 0147 and 0148, which still says "not a switch" against
their new `Switch:` values, to be reworded when the switch exists; an empty
`ini:opt=` means "the key set to empty", not unset (DIV-0110..0113's
switch); the lang-only entries, which a preset must not turn off without
saying it changes the language (answered by the `Languages:` field, below).

### Languages as run (2026-10-28, off the round branch at `281d597c`)

**The owner's call (2026-10-28):** a sixth field that says which languages
an entry acts under, so that the launcher's language pick selects the
entries that belong to that language and a preset never turns one on or off
against it; explicit tags, no class names, so that a German- or French-only
entry can be said. One Opus agent (`lf1027\LFlang\REPORT.md`) added the
field to the Entry format, `tools/ledger_check.py` and every entry.

**The rules** (each a sentence of the Entry format's "The switch fields",
each a check under the strict rule):

1. `any` alone, or distinct tags from `tools/language_tags.py` `TAGS` and
   `original`; the checker imports the list, so the ledger cannot name a tag
   the engine does not know.
2. `Switch: lang` requires a list; a list means the language pick governs
   the entry - on under those languages, inert under the others, whatever a
   preset says.
3. Such an entry's `Default: on` reads `on - under its languages` (the
   checker refuses another clause); `off` stays for one that waits for its
   own switch under its languages too (DIV-0155..0157, the reading row).
4. Stage D's registry carries the field (section 5, amended).

The self-test grew ten cases (49 in all), among them an unknown tag (`xx-XX`,
and the retired bare `en`) and `Switch: lang` with `Languages: any`, both
errors.

**The counts** (184 entries): `any` 137, a list 47. Per tag: `en-US` 39,
`en-150` 40, `fr-FR` 41, `de-DE` 40, `ja-JP` 23, `original` 1. The 24
`Switch: lang` entries all carry a list. The lists:

| Languages | Entries |
|---|---|
| the five tags | DIV-0005..0008, 0013, 0014, 0016, 0038, 0051, 0053, 0055, 0064, 0069, 0088, 0090, 0097, 0117, 0128, 0156 |
| `en-US, en-150, fr-FR, de-DE` | DIV-0009, 0015, 0017..0020, 0026, 0027, 0029, 0052, 0058..0061, 0083, 0084, 0118..0120, 0148 |
| `ja-JP` | DIV-0056, 0057, 0155, 0157 |
| `fr-FR, de-DE` | DIV-0054 |
| `fr-FR` | DIV-0098 |
| `en-150` | DIV-0141 |
| `original` | DIV-0102 |

**The cases decided** (each with its evidence in the report):

- **A chunk a Japanese disc does not build is Latin-only.** `loc_build.py`
  builds kinds 7..12 (Config, verbs, default names, merchant, battle labels
  and messages), label groups other than 15 and 24, and kind 17 (the staff
  roll) for Latin discs alone (`build_all`), so DIV-0015, 0018..0020, 0052,
  0118..0120 and 0148 are the Latin four, though none tests `Lang_Latin`.
- **An entry whose chunk every layer carries is the five tags**, even where
  its title says English: DIV-0006 (every font has kind 4), 0008 (the
  Japanese names too), 0013 (the US and JP white rows), 0014 (the Japanese
  title sheet is the US one), 0016 (the guard follows every table), 0038
  (kind 14 in all four languages), 0064 (a Japanese layer carries groups 15
  and 24), 0069 (kind 16 in every layer since D279), 0088
  (`build_village_sheet` runs on every disc; a disc-only Japanese layer's
  sheet is its disc's).
- **Two halves under different gates take the union:** DIV-0051 (the icon
  column under `Lang_Latin`, the Japanese icons under `ja-JP`), DIV-0128
  (`Lang_Latin() || Lang_Japanese()`).
- **A mechanism every language passes through is `any`**, though it serves
  the layers: DIV-0086 (the optional layers, walked under any language and
  `original`; only its text layers are language-bound), 0089 (the cache),
  0070 and 0144 (the shout's and the fishing banner's empty glyphs, armed
  unconditionally), 0133..0140 (the buttons follow the Config under every
  language; the language picks only the glyph family), 0145 (the quicksave's
  refusal across languages), DIV-0039 (the window title, "whatever
  `BOF3X_LANG` says").
- **An install's sources are not a language:** DIV-0080 (`area4-walls`, an
  optional layer from a Western disc), 0101, 0103..0107, 0114, 0115, 0130,
  0131, 0134 are `any`.
- **A disc-only language layer is its tags:** DIV-0090 and 0097 the five
  (`original` is refused on such an install), DIV-0098 `fr-FR`; DIV-0102,
  the refusal itself, is `original`, the field's one use of that value.
- **The layout switch of DIV-0056 is `ja-JP`:** under a Latin language
  nothing changed when `Lang_FullWidth` was added; under `ja-JP` the Latin
  layouts are withheld. DIV-0054 is `fr-FR, de-DE` (the accented cells);
  its rule 2, the PC's slot count, runs for every build but changes only
  where the tables disagree (15 French and 11 German areas).
- **The reading row:** DIV-0155 and 0157 `ja-JP` (the furigana builder reads
  Japanese text only, `tools/mods.py:89`), DIV-0156 the five (every ordered
  pair, under the main language); all three keep `Default: off`.

## 5. Stage D: the player's switch and the presets (after the owner's calls)

Not to be briefed until section 6's calls 2..4 are answered; the shape:

- **One switch both hosts read** - a variable such as `BOF3X_OFF=<names>`
  and its ini key - for every `Player: cheap` entry: the code-read names of
  section 0 read through it as well as through `BOF3X_ORIGINAL`, so the
  hosted A/B meaning is unchanged. `bof3x.exe` refuses a name it does not
  know, loudly (rule 4), not silently.
- **A generated registry** from the ledger's fields (`data/divergence.toml`
  or a generated header): id, switch, default, player, toward, languages,
  tier. The launcher's presets read it; the in-game log names the preset in
  force. **Amended 2026-10-28 (the `Languages:` field, section 4):** the
  presets' logic skips an entry whose `Languages:` excludes the language
  chosen, and treats a `Switch: lang` entry as part of the language: no
  preset turns one on or off against the player's pick.
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

**Also in stage D (the owner, 2026-10-28):** D395 - the launcher sends
`BOF3X_SCALE` only when `scale=` is not 2 and always writes the key back,
so the ini can neither pin k = 2 nor say "follow the display"
([`known-defects.md`](known-defects.md)); the presets' launcher work takes
the fix with it, DIV-0036's switch and prose updated to match.

### D0 as run (2026-10-28, off `ledger-fields/stage-d` at `de7aeb3d`)

One Opus agent, worktree `C:\Users\kerok\bof3x-runs\lfd1028\wt_d0`,
branch `ledger-fields/d0`; report `lfd1028\d0\REPORT.md`. The player's
switch, the launcher's key, the tools and the ledger; not the Configure
page, the registry or the presets (D1, D2).

**The coordinator's decisions, as built:**

1. **Both hosts read `BOF3X_OFF`** (`src/hook/detour.cpp`): `PatchBytes`
   and a non-instrument `RetargetCall` ask `CodeOff(name)`, which answers
   `BOF3X_ORIGINAL` (unchanged, `WantsOriginal`), else `BOF3X_OFF`
   (`WantsOff`, the same `NameListed`), else on; the `OFF` log line names
   the variable. A name in both is `BOF3X_ORIGINAL`'s. `Inject` stops on a
   non-instrument detour named in `BOF3X_OFF` (bare or `-name`; `*` does
   not reach it) with a `Fatal` that names it and says the original
   function is the hosted build's `BOF3X_ORIGINAL`. `InjectReport` counts
   the code names asked for and refuses every name in `BOF3X_OFF` (bare or
   after `-`) that is not a code name; a code name the start-up did not ask
   for is logged as not active (step 6, below). `RefuseHostedSwitches` is
   unchanged. *My choices:* a separate
   `WantsOff` beside `WantsOriginal` (the class is fixed by the call, so an
   argument bought nothing); `BOF3X_OFF` read once into a static buffer
   (injection is single-threaded); a `-name` must be a code name too, since
   an exclusion that matches nothing is as silent as a misspelt name; an
   instrument hook's name is refused by the end-of-injection check (the
   input recorder's and the state hash's `RetargetCall`s run after
   `InjectReport`, in `InputScript_Start`, so they are never "asked for").
2. **The launcher's `off=`** (`src/launcher/config.cpp`): kept as written,
   written back as read with a three-line ASD-STE100 comment, sent as
   `BOF3X_OFF` to either host when the environment has none. Not added to
   `HostedOnlyAsked`. `tools/launcher_host/config_tests.cpp` `TestOff`: the
   round trip, both hosts, the environment winning, the empty default (332
   checks, 0 failures).
3. **The ledger.** `ledger_switches.py`: `code` gives `yes`; `--check`
   fails if `src/` stops reading `BOF3X_OFF` or the launcher `off=`.
   `ledger_check.py`: `cheap` refused with "was retired by stage D
   (2026-10-28) ... write yes"; DIV-0176 is a `yes` fixture now, the cases
   that used it as `cheap` rewritten (52 cases, 49 before).
   `DIVERGENCE.md`: the Entry format's `Player:` line and table (the
   `cheap` row folded into `yes`, with the retirement note), the 67 entries
   `Player: yes - BOF3X_OFF=<names> (ini off=)` and a last `Reversible?`
   line "A player turns it off with `BOF3X_OFF=<names>` (the ini's
   `off=`)." (`lfd1028\d0\edit_ledger.py`; DIV-0022's "No config toggle."
   dropped, the sentence made it false); DIV-0141 and 0148 say the
   language is the switch, DIV-0145 and 0147 the hosted build's
   `BOF3X_ORIGINAL=Game_WndProc`. **`Player:` now yes 126, costly 28,
   never 30.** `analysis/ledger_switches.tsv` regenerated.
4. **`launcher-settings.md`** section 3's table row and section 3.2.
5. **The proof** (`lfd1028\d0\proof.sh`, logs beside it; every run
   `BOF3X_SELFTEST_ONLY=1 build/bof3x-launcher.exe --game <main>/bof3
   --no-config`, no `bof3x.ini` in `build/`): the exe host plain, exit 0;
   `BOF3X_SHADOW='*'` (the hosted fuzz), exit 0; the exe host with
   `BOF3X_OFF=SunderLoop`, exit 0, `patch OFF SunderLoop ... (BOF3X_OFF)`;
   `BOF3X_OFF='*'`, exit 0, 58 off; `BOF3X_OFF=NoSuchName`, exit 3, the
   `FATAL` names it; `BOF3X_OFF=Paralyzer_Start`, exit 3, the `FATAL`
   names the hosted build's `BOF3X_ORIGINAL`. Beyond the brief, hosted:
   `BOF3X_SHADOW='*' BOF3X_OFF=SunderLoop` exit 0 (off by `BOF3X_OFF`);
   `BOF3X_ORIGINAL=SunderLoop BOF3X_OFF=SunderLoop,Game_Clock` exit 0
   (SunderLoop logged as `BOF3X_ORIGINAL`'s, Game_Clock as `BOF3X_OFF`'s);
   `BOF3X_ORIGINAL=SunderLoop BOF3X_OFF=Paralyzer_Start` exit 3. The
   state-hash proof against the hosted build is the coordinator's.

**Finding, fixed as step 6 (the coordinator's call, 2026-10-28):** 23 of
the 83 code names are asked for only under a setting, so the first build's
end-of-injection check refused them in a start without it. A plain exe-host self-test (`--no-config`)
asks for 60 code names, 58 under `BOF3X_OFF=*` (two are asked for only
when an earlier one is on). Not asked in a plain start: the language
layouts (`BattleBannerCentre`, `BattleListTitleCentre`,
`BattleResultExpLayout`, `BoardYesNoLayout`, `ConfigController`,
`ConfigJapaneseIcons`, `ConfigText`, `FormationNameCentre`,
`MasterAskLayout`, `MasterPupilLabel`, `MenuTitleCentre`, `MenuVerbs`,
`PauseText`, `SaveNameInset`, `ShopAskRow`, `ShopYesNoLayout`,
`TitleMenu_Widths`, `TradeConfirmLayout`, `TradeLeaveLayout`,
`YesNoLayout`), `Widescreen`, `Gfx_FilterPoint`, `Cheat_StealAlways`. So
an `off=YesNoLayout` written under a Latin language stopped the game once
the player picked the original text.

6. **Step 6, the fix.** `tools/ledger_switches.py` now also writes
   `src/hook/code_names.h`: a constexpr array of the 83 class-`code` names,
   sorted in `strcmp` order, generated, not typed. Its `--check` (already in
   `checks.yml`) fails, naming the names, when the checked-in header is not
   what `src/` gives (tried: one name taken out of the header, error; put
   back, 0 errors). `detour.cpp` `KnownCodeName` searches it. At
   `InjectReport` a name in `BOF3X_OFF` that is a known code name but was
   not asked for is logged once - "BOF3X_OFF: YesNoLayout is not active in
   this start (its change is off with these settings: the language, the
   wide mode, the filter or a cheat), so it has nothing to turn off" - and
   the game runs; a name that is not a code name stays a `Fatal`, as does a
   detour's. *My choices:* the file is `code_names.h`, not `.gen.h`, since
   in this repo `*.gen.h` are build-tree outputs (`symbols.gen.h`) and this
   one is checked in, as D1's `ledger_registry.h` is; the log names the
   possible causes, not the one cause, because the engine does not record
   why a module skipped its `PatchBytes`; and `CodeOff` stops the game if a
   `PatchBytes` / `RetargetCall` asks for a name the header lacks (a stale
   header in a local build that CI's `--check` has not seen yet). Proof,
   exe host: `BOF3X_OFF=YesNoLayout` exit 0, the log line above;
   `BOF3X_OFF=NoSuchName` exit 3, "BOF3X_OFF names "NoSuchName", which is
   not a code name"; plain 0, `*` 0 (58 off), `Paralyzer_Start` 3;
   `BOF3X_LANG=en-US BOF3X_OFF=YesNoLayout,ConfigText` exit 0, 77 code
   names asked for, both off by `BOF3X_OFF`; the hosted fuzz
   (`BOF3X_SHADOW='*'`) exit 0.

### D1 as run (2026-10-28, off the stage branch at `de7aeb3d`)

**What landed.** `tools/ledger_registry.py` reads the ledger through
`tools/ledger_switches.py`'s own `ledger_entries` and `field` (imported) and
writes `src/launcher/ledger_registry.h`, checked in: a `constexpr` table,
`bof3x::ledger::kEntries` (184) and `kSwitches` (181), of plain structs -
id, title, `Tier`, `State` (with `superseded_by`), the `Switch:` names each
with its `SwitchForm` (`Name`, `Ini`, `Env`, `Lang`; `Switch: none` is a
count of 0), `default_on`, `Player`, a `Toward` bit set and a `Languages`
bit set (the `tools/language_tags.py` order, then `original`; `kLangAny`
alone for `any`). No allocation, no parser. `Key()` and `Value()` are spans
of the switch's own text, so the header holds no `"BOF3X_..."` literal that
`ledger_switches.py` would count as a reader of that name (the tool refuses
to write one; the inventory's TSV is the same with the header present or
not). The header ends in a `static_assert` that the table is whole, and
`src/launcher/config_dialog.cpp` includes it with a second one, so the
launcher's build compiles it (and `-std=c++17 -fsyntax-only` on the header
alone passes). `--check` regenerates to memory and fails with a diff
summary; it is a step of `checks.yml` beside `ledger_switches.py --check`.
It exits 0 on this tree and 1 on a scratch ledger with DIV-0176's
`Default:` turned to `off` (`--ledger <path>`). It reads `Player:` as the
ledger has it, so after D0's `cheap` to `yes` the header is regenerated with
no change to the tool. A Windows checkout's CRLF is normalised before the
comparison (`core.autocrlf` is on here; CI's Linux checkout is LF).

**The presets, as a reference** (`python tools/ledger_registry.py presets
[--language <tag>|original|all] [--tsv <path>]`; the launcher's code is
D2's and must agree with it). The rules as the coordinator set them, the
owner's calls of 2026-10-27/28 read literally, recorded verbatim:

- A preset only ever turns entries **off** by applying their `Switch:`
  names; it never turns an entry on (an `ini:` / `env:` switch gives only
  the off value, so "on" is not derivable in general). An entry a preset
  would want on that is `Default: off` is listed under the preset as "also
  available, switch it yourself" instead.
- Only `Player: yes` entries (after D0 that includes every former `cheap`;
  until then treat `cheap` as `yes` too, and say so) with `Default: on` and
  `State: active` can be turned off. `costly`, `never` (Forced), withdrawn
  and superseded entries are never touched; the `costly` ones are listed as
  "this build cannot" (the owner's call 4).
- *Recommended*: nothing (the defaults).
- *As shipped*: every such entry off.
- *Repaired*: every such Sensible and Extension entry off; Intent and Forced
  stay.
- *PlayStation* (**the owner's call of 2026-10-28**, replacing the
  coordinator's reading): every such entry whose `Toward:` lists a release
  and does not list `psx` off - today the `psp`-only ones, `pc` when one
  exists. `Toward: none` means the entry is related to no release, a generic
  repair rather than a versioned one, so every `none` entry, like every
  `psx` entry, stays at its default. *Also available* stays: the
  `Default: off` entries that list `psx`. (The reading it replaced turned
  off every entry that does not list `psx`, `none` included; it was dropped
  because `none` mixes shared defects, console-less platform mechanics and
  unmeasured cases, so it turned back on two crash fixes and the port's
  window handling - the flag below.)
- Language rule (section 4 "Languages as run", rule 2): an entry whose
  `Languages:` is not `any` is governed by the language pick: skip it when
  the chosen language is not listed, and never apply a `Switch: lang` entry's
  switch (a preset never changes the language). `original` is a language for
  this purpose.
- An entry whose switch names are more than one applies them all.
- The install rule (an `ini:` off value the install cannot take, such as
  `music=pc` on a disc-only install) is the launcher's, not yours: print the
  switch and let D2 decide at launch.

**The order the tool applies them** (D1's reading; `classify` in the tool
is the one place it is written): an entry the preset neither wants off nor
wants on at `Default: off` is not its concern; then not active (left alone);
then a `Languages:` list that omits the language (*skipped by language*,
before every other test, since the entry is inert there); then a wanted-on
`Default: off` entry (*also available*: Repaired wants Intent and Forced
on, PlayStation every `psx` entry; Recommended and As shipped want none on);
then `Default: off` (left alone, already off, so a `Default: off` entry is
never counted *cannot*); then a `lang` switch name (*lang-governed*); then
`Player: never` (left alone); then `costly` (*cannot*); else *off*, by every
name of its `Switch:`. The printout counts the five actions and, apart, the
three kinds left alone (never, already off, not active), so that a preset
that wants every entry off (As shipped) accounts for all 184.

**The counts** (184 entries; the 67 `Player: cheap` read as `yes`):

| Language | Preset | off | skipped by language | lang-governed | cannot | also available | left alone: never / already off / not active |
|---|---|---|---|---|---|---|---|
| `original` | Recommended | 0 | 0 | 0 | 0 | 0 | 0 / 0 / 0 |
| `original` | As shipped | 74 | 46 | 0 | 23 | 0 | 25 / 14 / 2 |
| `original` | Repaired | 32 | 45 | 0 | 14 | 0 | 14 / 14 / 2 |
| `original` | PlayStation | 1 | 0 | 0 | 0 | 1 | 4 / 8 / 0 |
| `en-US` | Recommended | 0 | 0 | 0 | 0 | 0 | 0 / 0 / 0 |
| `en-US` | As shipped | 85 | 8 | 20 | 28 | 0 | 26 / 15 / 2 |
| `en-US` | Repaired | 43 | 8 | 20 | 18 | 0 | 15 / 15 / 2 |
| `en-US` | PlayStation | 1 | 0 | 0 | 0 | 1 | 4 / 8 / 0 |

Of the *off* rows, the `cheap` ones (a player's only once `BOF3X_OFF` is
read): `original` 56 / 24 / 0 (As shipped / Repaired / PlayStation),
`en-US` 67 / 35 / 0. PlayStation's one *off* is DIV-0123 (`psp` alone,
`env:BOF3X_WIDE_STAGING=0`), the same under every language; its one *also
available* is DIV-0012, the point filter.

**For the owner, from D1, answered 2026-10-28.** D1 flagged that the
coordinator's PlayStation reading turned off every switchable `Toward: none`
entry, and that `none` held a defect every release shares (DIV-0121,
0153), platform mechanics a console has nothing to match (DIV-0022, 0033,
0036, which applies `display=windowed`, 0062, 0078, 0089, 0101, 0146) and
behaviour not measured (DIV-0071, 0144, 0180 - a crash fix - and, under the
Latin four, 0027, 0058, 0059). The owner's answer: `none` is related to no
release, so the PlayStation preset leaves it alone (the rule above).

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

**Answered 2026-10-28 (the owner), on section 4's "as run":** the `env:`
form LF0 added to `Switch:` is accepted; the "starts Yes" note's removal
is accepted; D395 (`scale=2` cannot pin k = 2) is fixed in stage D; call 4
affirmed ("leave them, list them, and let a later round take the ones a
player asks for" - the 28 are listed in section 4); and the language
question of rules A and B is settled by the `Languages:` field, a list of
explicit tags and no class names (section 4, "Languages as run").

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
