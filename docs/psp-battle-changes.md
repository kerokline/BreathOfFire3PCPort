# The PSP's two battle changes, as switches

**Status:** BUILT (2026-10-12, the PSP-only round's group PB, branch `psp/pb`
off `psp/round` `640d2c57`). Headless self-tests only; **neither change has been
seen in play**. DIV F is drafted at the end, for the coordinator to number.

The PSP's code diff found two places where the PSP's battle code does
something the PlayStation's does not
([`psp-code-diff-battle.md`](psp-code-diff-battle.md), C1 and C2). The owner,
2026-10-12: both "are both worth keeping as a toggle in the launcher psx vs
psp area - leaving them as default for psp-only installs makes sense"
([`psp-only-round.md`](psp-only-round.md) 7.1). This page reads both again
from the PSP's code, says what each does on our side, and describes the
switch, its defaults, and the two launcher members that write it.

One correction to CD1's reading comes first (section 2.2): **C2 does not
change an agility.** The field the PSP writes is the enemy's first item word,
the one a steal takes and a drop rolls. The switch item is named
`myria-item`, not the brief's `myria-agility`.

No game data here or in the code: addresses, offsets, counts, and the PSP's
immediates, which are rules of its code (the owner's stance of 2026-10-10).

## 1. The two changes in one paragraph each

**C1, `duel-loss`.** In event battle 0x1B (set-up 27; the round's notes place
it as the Beyd-Zig duel, chapter 6 run 9, area 92 - not seen here), when the
battle-end byte has bit 0 set and bit 2 clear, the PSP does not end the
battle. It treats the byte as zero, so play goes on to the next action or
round. It writes nothing to the byte. Everywhere else, and with the switch
off, the battle ends on a non-zero end byte, as on the PlayStation and the PC.

**C2, `myria-item`.** When a boss actor is placed as an enemy in area 198
under event battle 0x37 (set-up 55, which our records label Myria,
[`boss_sj.md`](boss_sj.md)), the PSP sets the enemy record's first item word
to 0x317 right after copying the enemy's data. That is the word a steal takes
and the first drop rolls (section 2.2). 0x317 reads as category 3
(accessories), index 0x17. The sibling's `names/items.toml` labels that
accessory "Ivory Charm" (its US name). What the PlayStation's data held there
for Myria was not read.

## 2. What the PSP's code does, read

Every instruction below was read from `BOOT.BIN` with capstone, through
`tools/psp_pair.py`'s ELF loader (`Psp(iso).space`, relocations marked). The
script is `<scratch>/psp_pb/pspdis.py ISO addr:len ...` and its listings are
`jp_c1.txt`, `eu_c1.txt`, `jp_c2.txt`, `eu_c2.txt` and `jp_copy.txt` beside it.
The PSP's battle globals are one block. Its base register is the PC's
`0x904AA0`, so `+0xA` is the event battle byte `0x904AAA` and `+0x48` is the
end byte `0x904AE8`. CD stage A's learned data map pairs the same two
(`0x801462E6` and `0x80146324`; `match.pkl` in the CD scratch).

### 2.1 C1, both functions

**The turn step**, BattleAction_End's twin (PC `0x430010`, PSX `0x801D43BC`;
PSP-JP `0x11DF2C`, PSP-EU `0x11DB9C`). After the hook with 0 (`jalr [+0xCC]`,
taken only when `+0xA` is non-zero), PSP-JP:

```
11dfa4  lbu  a1, 0xa(s1)        ; the event byte, read again after the hook
11dfa8  ori  a2, zero, 0x1b
11dfac  bne  a1, a2, 11dfc8     ; not 0x1B: the original's test
11dfb0  lbu  a0, 0x48(s1)       ; (delay) the end byte
11dfb4  andi a1, a0, 4
11dfb8  bnez a1, 11dfc8         ; bit 2 set: the original's test
11dfbc  andi a1, a0, 1
11dfc0  bnez a1, 11dfd0         ; bit 0 set: on, as if the byte were 0
11dfc8  bnez a0, 11dff4         ; the original's: non-zero ends (phase 4, step 2)
11dfd0  jal  11e27c             ; Battle_ClearActingFlags: the battle goes on
```

PSP-EU has the same sequence at `0x11DC14..0x11DC40`.

**The round start**, BattleRoundEnd_NextRound's twin (PC `0x431090`, PSX
`0x801D653C`; PSP-JP `0x11F2F4`, PSP-EU `0x11EF64`). Inside the event branch,
after the hook with 2 has not answered 0xFF, PSP-JP `0x11F36C..0x11F3A4`
does the same thing: event byte 0x1B, end bit 2 clear and bit 0 set, and it
branches to the not-ended path (`0x11F3A4`, the round flag 0x10 test). Every
other case reaches the original's `bnel a0 ... 0x11f3bc`, the battle's end
(phase 5). PSP-EU has it at `0x11EFDC..0x11F014`. Outside an event battle the
PSP's code is the original's test alone. Since 0x1B is not 0, that equals one
test placed after the hook in both branches, and that is how ours writes it.

Neither function writes the end byte on the new path. The byte stays as it
was, with bit 0 set, for whatever reads it next.

### 2.2 C2, and what `+0x98` is

**The enemy entry**, `0x800A8F4C`'s twin (PSP-JP `0x141414`, PSP-EU
`0x1411E0`). It takes a boss actor (the search `0x141640` matches type 7 and
the tag at `+0x92`), copies its first 0x70 bytes into enemy record `slot`, sets
the state bytes, and calls the enemy data copy (`jal 0x141BA0`, PSX
`0x800A9148`). Then it adds the following (PSP-JP):

```
141570  lui  a1, 0x10
141574  lhu  a1, -0xf0(a1)      ; the area number, a u16 (PSX 0x80143F00, CD's map)
141578  ori  a0, zero, 0xc6     ; 198
14157c  bne  a1, a0, 1415a0
141584  lbu  a0, 0xa(s4)        ; the event byte
141588  ori  a1, zero, 0x37
14158c  bne  a0, a1, 1415a0
141594  ori  a0, zero, 0x317
141598  sh   a0, 0x98(s3)       ; s3 = the enemy record (0x10E730 + slot * 0x128)
1415a0  jal  1423d4             ; Battle_SetEnemyOffset's twin
```

PSP-EU has it at `0x14133C..0x141364`. The area is read at `0x100A20` there,
since the EU image lays its globals out differently.

**Its PC twin is `Battle_PlaceBossActor`** (`0x494570`, `src/game/rest_3g.cpp`).
symbols.toml has no `psx = 0x800A8F4C` row. The pairing is by shape: the
same three calls in the same order (the boss actor search, `0x4946C0`,
`0x494F00`); the same stores (`+1..+4`, `+0x29 = 4`, `+5 = count + 3`,
`+8 = 0x904AAC ^ 2`); and the actor's `+0 |= 0x40` and the count `0x904AB2`
raised after. The PSP's search reads type `+6 == 7` and the tag at `+0x92`,
over 30 objects 0x98 apart (`0x141640`: index x 16 + index x 136). Ours (`BossActor_Find`) reads `+6 == 7` and the
tag at `+0x9E`, over 30 objects 0xA4 apart. The field objects are laid out
differently on the two machines, as the enemy records are below.

**Why `+0x98` is the item word and not agility.** The PSP's enemy record is
not laid out like ours. Its enemy data copy (`0x141BA0`, `jp_copy.txt`) writes
an 8-byte name at `+0x74`. Ours (`Battle_CopyEnemyData`,
`src/game/battle_sprites.cpp`) writes a 12-byte name at `+0x80`. Every field
after the name is 0x10 higher on the PC, read field by field:

| enemy data field (PSP / PC) | PSP record | PC record |
|---|---|---|
| `+0x08..0A` / `+0x0C..0E` (bytes) | `+0x7C..7E` | `+0x8C..8E` |
| `+0x0E` / `+0x12` (u16) | `+0x80` | `+0x90` |
| `+0x10..14` / `+0x14..18` (u16) | `+0x84..88` | `+0x94..98` |
| `+0x18..1F` / `+0x1C..23` | `+0x8C..93` | `+0x9C..A3` |
| `+0x81` / `+0x85` (byte to u16) | `+0x82` | `+0x92` |
| `+0x20` / `+0x24` (u16, twice) | `+0x94` and `+0xC0` | `+0xA4` and `+0xD0` |
| **`+0x2C..2F` / `+0x30..33`** | **`+0x98`, `+0x9A`** | **`+0xA8` (`dst + 0x28`)** |
| `+0x30` / `+0x34` | `+0x9C` | `+0xAC` |
| the stat block copied again | `+0xA0..` from `+0xC0..` | `+0xB0..` from `+0xD0..` |

So the PSP's `+0x98` is our `+0xA8`. On the PC, `+0xA8` is the first of the
enemy's two items, `category << 8 | index`, with its class byte at `+0xAA`.
`Battle_RollDrops` rolls it as a drop (`src/game/battle_flow.cpp`, PSX
`0x801E525C`). Pilfer and Steal take it (`magic_fx_reached.md` 4,
`magic_steal.md` 2). Identify prints it (`magic_s12.md`). The enemy's agility,
which the turn order reads, is `+0xB8` (`battle_damage.cpp`, `kEAgi` on the
`0x93B9E0` base). CD1's "agility" came from the party record's `+0x98`
(`kPAgi`), which is a different structure. The PSP stores a halfword, so the
class byte at its `+0x9A` (our `+0xAA`) is left as the data had it. Whether
Myria's class or steal row lets the item drop or be stolen is a property of
the data, and was not read.

**791 appears nowhere else.** CD1 searched every function on both sides; this
page did not search again.

## 3. What is built

**The engine** (`src/game/psp_code.h`, `.cpp`):
- `psp_code::DuelLossGoesOn(event, end)` and `psp_code::MyriaItem(record,
  area, event)` hold the two rules. The PSP's immediates are named constants
  (`kDuelEvent` 0x1B; `kMyriaArea` 198, `kMyriaEvent` 0x37 and
  `kMyriaItemWord` 0x317, each citing its instruction); `kItemWordAt` is 0xA8.
- `BattleAction_End` (`battle_actions.cpp`) and `BattleRoundEnd_NextRound`
  (`battle_turn_steps.cpp`) test `end != 0 && !DuelLossGoesOn(event, end)`.
  The event byte is read after the hook, as the PSP reads it.
- `Battle_PlaceBossActor` (`rest_3g.cpp`) calls `MyriaItem` between
  `Battle_CopyEnemyData` and `Battle_SetEnemyOffset`, where the PSP stores.
  It reads `Game_AreaNumber` and the event byte after the copy, as the PSP
  does.
- `PspCode_Inject` (`inject_all.cpp`, after `Cheats_Inject` and before the
  battle modules) reads the switch once and logs the result:
  `DIV F       the PSP's battle changes: duel-loss on, myria-item on (the default: the cache's base_policy is psp)`.

**The switch: `BOF3X_PSP`, and `psp=` in `bof3x.ini`.**

| `BOF3X_PSP` | duel-loss, myria-item |
|---|---|
| unset (the default) | **both on** when `<BOF3X_CACHE>\manifest.toml` has `[build] base_policy = "psp"`; **both off** otherwise (no cache, no manifest, no key, or `"psx"`) |
| `none` | both off |
| `duel-loss`, `myria-item`, or both comma-separated | exactly those on |
| anything else, a name twice, or a `base_policy` other than `"psp"` / `"psx"` | Fatal at start-up, naming it |

With both off, the three functions behave as before, byte for byte (section
4). The default reads the manifest itself, not through `dat_cache`, so it
holds under `BOF3X_CACHE_DATA=0` as well. The launcher's `psp=` line is passed
on as `BOF3X_PSP` when it is not empty and the environment does not already
set it (`ConfigApplyEnvironment`). An empty line leaves the DLL its default.

**The launcher's two members** (the data suite, `recipes/suites.toml`):
`psp-duel-loss` ("fight 27: a loss goes on") and `psp-myria-item` ("fight 55:
Myria's item"), `kind = "code"`, `code` naming the item, `from` both PSP
builds, `over_psx = "switch"`. What changed to carry them:
- `tools/suites.py`: the kind `code` and the `over_psx` value `switch`. Each
  goes only with the other; a `code` member needs a `code` that is one of the
  engine's items (`CODES`), placed in exactly one suite; no `why`. The member
  limit is per suite (`MEMBERS_MAX`: data 6, the others 4).
  `MemberInfo` gains a last field, `code`.
- `src/launcher/suites.*`: `SuitesEvaluate` reads a code member as playable
  when the cache's sources hold one of its builds, on either base. On a PSP
  base it is the base's; it notes "off in bof3x.ini's psp=" when the line turns
  it off. Beside a PlayStation base its item joins `codes_on`, the PSP's side
  is enabled by it, and the suite's side reads `psp=` beside `opt=`. With no
  manifest it is greyed ("needs psp-jp or psp-eu"): no installed file can
  stand as evidence of the disc for code. `SuitesPspEdit` writes `psp=` on the
  same terms as `SuitesOptEdit`: untouched means byte for byte; a PSP base
  writes nothing; the PSP's side adds the suite's items, the PlayStation's
  side removes them; the result is in catalogue order; "none" is written when
  a non-empty line empties, and "" stays "". `SuitesPspPlayed` reads the line
  as the DLL does. One consequence: an ini from before this branch that chose
  the PSP's data (the renames in `opt=`, `psp=` absent) beside a PlayStation
  base now reads as a partial data suite, until the PSP's is chosen again,
  which writes both rules.
- `config.*`: `Config::psp`, loaded as written, saved with a comment, put in
  the environment. `config_dialog.cpp`: `in.psp`, `SuitesPspEdit` on Play,
  member lines 5 and 6 at ids `+8` / `+9`, and the partial reason names
  `psp=` too. `launcher.rc` / `resource.h`: the data suite's two new lines at
  y 212 (`1068`, `1069`); its reason line moves to y 222 and is three lines
  high instead of four.

## 4. Measured

| Check | How | Result |
|---|---|---|
| The rules against their statement | `BOF3X_SHADOW=psp_code` (`PspCode_SelfTest`): 4 masks x 256 x 256 (event, end), and 4 x 8 areas x 256 events on a 0x128-byte record | 128 duel cases on, 2 item stores, the other 0x126 bytes untouched, **0 mismatches** |
| C1, the turn step, off | `battle_actions`' shadow, every function with the switch forced off | 23,000 rounds over 23 functions, **0 mismatches** |
| C1, the turn step, on | the same shadow, `BattleAction_End` 4,000 more rounds with the switch on, against the original run with the PSP's test applied after the hook (`StubHookPsp`: the end byte shown as 0, put back at the next stand-in call); half the rounds in event 0x1B, the end byte's low three bits drawn from all eight | the PSP's test applied 268 times, ours differs from the plain original in 268, **0 mismatches** |
| C1, the round start, off / on | `battle_turn_steps`' shadow likewise (`StubHookEventPsp`, after the hook with 2 when it does not answer 0xFF) | 30,000 rounds off, 0 mismatches; 6,000 on: applied 353 times, differs from the plain original in 353, **0 mismatches** |
| C2, off | `rest_3g`'s shadow through the scenario harness with the switch forced off | 124,000 + 4,000 rounds, **0 mismatches** |
| C2, against the original with real callees | `rest_3g` `CheckPspMyriaItem`: a copy of `0x494570` whose three calls go to the real `0x494920` / `0x4946C0` / `0x494F00`, against ours off and on; 4,000 random states (objects, enemy records, the area's enemy data, the battle globals, the area 198 or not, the event 0x37 or not) | 2,010 placed an actor, 658 of them in area 198 under 0x37; off byte for byte; on, the original's state plus the word: **0 mismatches** |
| The controls | one build with three planted faults: C1 tested on the pre-hook event byte; C1 with bit 2 masked off in the round start; C2 stored before the data copy. `BOF3X_SHADOW` set to each module (rest_3g with `BOF3X_R3G_ONLY=Battle_PlaceBossActor`) | **3 of 3 refused**: 185, 355 and 658 mismatches, each a Fatal |
| The switch | headless, `BOF3X_SHADOW=psp_code`, six cases | the PSP-JP cache (`psp_im/c/new_pspjp`, `base_policy "psp"`): both on, "the default". The US cache (`new_us`, `"psx"`): both off. `BOF3X_PSP=duel-loss`: duel-loss only. `=duel-loss,bogus`: Fatal naming "bogus", exit 3. The PSP cache with `=none`: both off. `=myria-item,duel-loss`: both on |
| The hosted `'*'` narrow | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, launcher, `bof3x.exe` and DLL in an ini-less scratch folder, `--game` the PC install, `--no-config` | see the note below |
| The exe host, PSX disc-only | a copy of PN's `g_us` (`install --base` from the coordinator's `ref_us`), `BOF3X_CACHE` = `BOF3X_EXEIMAGE` = `ref_us`, `BOF3X_LANG=en-US` | exit 0, `inject: 10088 ours`, "duel-loss off, myria-item off (the default: the cache's manifest has no base_policy)" |
| The exe host, PSP-only | a copy of PN's `g_psp`, with IM's PSP-EU cache `new_pspeu` (`base_policy "psp"`), `en-150` | exit 0, `inject: 10088 ours`, "duel-loss on, myria-item on (the default: the cache's base_policy is psp)" |
| The launcher host suite | `cmake -S tools/launcher_host ...`, `config_tests.exe` | **137 checks, 0 failures**, `TestCodeMembers` included (PSP-only, PSX-only, PC + PSX + PSP, no cache; `SuitesPspEdit` untouched, either side, partial, a foreign name kept; the `psp=` round trip) |
| `suites.py check`, `importer.py check`, `ledger_check.py` | as named | 0 errors each (ledger: 101 entries; DIV F is not in it yet) |

The `inject: 10088 ours` count matches `disc-only-install.md` 5: no function
was taken over or given up. `PspCode_Inject` is not a detour.

## 5. Known defects and open readings

- **C1 may let a lost duel run on.** By `boss_sf.md`'s reading of
  `Boss27_Event`, its member check sets the end byte to 4 only when the byte
  is 0. Once a loss has set bit 0, only the enemy `+0x92` bit 0x4000 path sets
  bit 2. So with duel-loss on, a party loss in that fight lasts until that
  path fires. Whether that is a scripted end the PSP meant, or a stall, has to
  be judged in play. The PlayStation's twin of `Boss27_Event` is uncaptured,
  so **C1 may be a repair** of a PlayStation fault, or a change the PSP made
  for its own reasons. That is the owner's to judge in play (DIV F's Tier
  clause).
- **Only `Battle_PlaceBossActor` gets C2.** An enemy set up by
  `Battle_SetupEnemy` (`0x494320`) in area 198 under 0x37 does not get the
  item. On the PSP its twin carries no store either (CD1's search for 791).
- **The data suite's `what`** still lists only the data members. A sentence
  for it is under "For the other files", since TG edits that suite's fields.

## 6. Not verified

- **Neither change in play.** Which fight 0x1B is was taken from the round's
  notes (`groups.md` PB). Whether 0x37's enemy in area 198 is Myria was taken
  from our records (`boss_sj.md`). What the item is in play, whether it can be
  stolen or dropped from Myria (her class byte), and what C1 looks like when
  the party falls are all **the owner's**.
- **The PlayStation's own item word for Myria** (its data's `+0x2C`) was not
  read. So "the PSP gives her 0x317 where the PlayStation gave X" is not
  stated.
- **PSP-EU's enemy data copy** was not read field by field. The store is the
  same instruction on the same register in the same function. The 0x10 shift
  is measured on PSP-JP only.
- **The dialog**, by eye or by `dialog_test.py`. It was not run: the brief
  forbids it, because it opens windows. Its `suites()` reads member lines
  `+3..+6`, so the data suite's two new lines are invisible to it but break
  nothing it checks. The layout (lines at y 212, a reason three lines high) is
  unseen.
- **The `'*'` hosted run** is described under "For the other files" if it
  had not finished when this page was written.
- The pad on the new member lines: they are static text, not tab stops.

## For the other files

**DIV F** (one entry, both changes; Tier proposed):

```markdown
### The PSP's two battle rules, behind a switch

- **ID:** DIV-NNNN
- **Date:** 2026-10-12
- **Subsystem:** battle
- **Tier:** Extension - the PSP's own changes to play, taken as options; C1 may
  turn out to be Intent (a repair) once the owner has played the fight
- **Original behaviour:** the PC port and the PlayStation end a battle whenever
  the battle-end byte 0x904AE8 is non-zero, in the turn step
  (BattleAction_End 0x430010, PSX 0x801D43BC) and the round start
  (BattleRoundEnd_NextRound 0x431090, PSX 0x801D653C); and a boss actor placed
  as an enemy (Battle_PlaceBossActor 0x494570, PSX 0x800A8F4C) keeps the
  enemy data's first item word +0xA8 (the drop and steal item). The PSP's
  BOOT.BIN adds, in both releases: (C1) in event battle 0x1B, an end byte with
  bit 0 set and bit 2 clear does not end the battle (PSP-JP 0x11DFA4 and
  0x11F36C; PSP-EU 0x11DC14 and 0x11EFDC); (C2) in area 198 under event battle
  0x37 the placed enemy's item word becomes 0x317 after its data is copied
  (PSP-JP 0x141570..0x141598, PSP-EU 0x14133C..0x141364; the PSP's +0x98 is
  ours +0xA8, its enemy data copy 0x141BA0 read field by field). Read with
  capstone, docs/psp-battle-changes.md section 2.
- **New behaviour:** both rules reproduced in our three functions under one
  switch per rule, BOF3X_PSP / bof3x.ini psp= (duel-loss, myria-item, none).
  Unset, both are on when the cache's manifest says [build] base_policy =
  "psp" (a PSP-only install) and off otherwise. The launcher's data suite
  carries them as two code members, and its choice writes psp=
  (src/game/psp_code.*, recipes/suites.toml).
- **Rationale:** the owner, 2026-10-12: both "worth keeping as a toggle in
  the launcher psx vs psp area - leaving them as default for psp-only
  installs makes sense". A PSP-only install plays as the PSP did; any other
  plays as the PlayStation and the PC did, unless the player chooses the PSP's.
- **Also in the PSX version?** no - the PSX twins have neither test (CD1, the
  constants 0x1B-with-the-end-byte and 791 searched in every function of both
  sides). C1's PSX twin of Boss27_Event is uncaptured, so whether the PSP's C1
  repairs a PSX fault is open (the owner's, in play).
- **Reversible?** yes: BOF3X_PSP=none or psp=none turns both off; with both
  off the three functions are the original's byte for byte (the battle
  modules' shadows, both off and on).
```

**`docs/README.md`**, one row: | [`psp-battle-changes.md`](psp-battle-changes.md) | The PSP-only round's group PB: the PSP's two battle changes (CD1's C1 and C2) read again from `BOOT.BIN` and built as switches - `BOF3X_PSP` / `psp=` (`duel-loss`, `myria-item`), on by default for a PSP-only cache - in `BattleAction_End`, `BattleRoundEnd_NextRound` and `Battle_PlaceBossActor`, with the data suite's two `code` members. C2 corrected: the PSP's `+0x98` is ours `+0xA8`, the enemy's first item word, not agility. Shadows off and on, 0 mismatches, 3 of 3 controls refused; not seen in play. DIV F. BUILT. |

**`docs/psp-code-diff-battle.md`** C2: "`+0x98` is the actor's agility" should
read "`+0x98` is the PSP's offset of the enemy's first item word, ours
`+0xA8` (`psp-battle-changes.md` 2.2); 0x317 is accessory 0x17". The suite
column for both rows becomes "data (a code member, `psp=`)".

**`docs/launcher-suites.md`** (LT's, with TG's section): section 2 gains the
kind `code` and the value `switch`, and section 4 gains a paragraph: "`psp=`
carries the data suite's two code members (DIV F); `SuitesPspEdit` writes it
on `SuitesOptEdit`'s terms, and a suite's side reads both lines."

**`recipes/suites.toml`**, the data suite's `what` (TG's field, not touched
here): append ", and its two battle rules (a loss that goes on in fight 27,
Myria's item)".

**`docs/STATUS.md` / `docs/owner-review.md`**: C1 and C2 are switches now,
on for a PSP-only install. The owner's eye and play are owed: fight 27 lost
with duel-loss on, and what Myria carries with myria-item on.

**Code**: `src/` carries the draft id as "DIV F" in comments and in the log
line `DIV F       the PSP's battle changes: ...` (`psp_code.cpp`). These
become the number at merge: `grep -rn "DIV F" src tools recipes`.
