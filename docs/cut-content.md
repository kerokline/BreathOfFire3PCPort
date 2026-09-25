# Cut content: what the disc holds that play never shows

**Status:** DRAFT (2026-09-25) - a map from a third-party list to our tables; nothing here is seen in game yet

The Cutting Room Floor's Breath of Fire III page lists what the PlayStation
release carries and never shows: skills, an item, music, narration, text, a
portrait, an animation, and places cheats can reach
(<https://tcrf.net/Breath_of_Fire_III>, oldid 1140694, last edited
2022-01-05; content CC BY 3.0). The owner saved it as a PDF on 2026-09-25,
because the site serves agents a landing page. This document **summarises**
it and ties each item to what this repo can find in `BOF3.exe`. It does not
copy the page, and it does not reproduce the unused dialogue: that is
Capcom's text.

Everything TCRF says is a claim about the PlayStation release, made by
people playing it with cheat codes. None of it has been seen on the PC port.
Where a claim is re-stated here it says "TCRF", and it stays that until the
owner sees it in game ([`README.md`](README.md), the evidence rule). What
the game does is the owner's call on sight, not the page's or the model's.

## 1. The owner's list (2026-09-25)

What to take from the page, in the owner's words, and what each wants:

| Item | TCRF says | What it would take here |
|---|---|---|
| **The unused spells and skills** | eleven unfinished skills (§2), and 29 that only enemies use | named and located below; they are rows of the effect table like any other, so the spell round takes them with the rest |
| **Unused music and audio** | three unused songs, found when the music was ripped; five Japanese narration clips of the prologue, `BIN/SCE_XA/VOICE.STR` on the JP and US discs | not looked at on the PC. The port's music is `BGM/` (166 files); whether the three songs came across, and whether any `VOICE.STR` did, is unread |
| **Unused text** | lines in eleven places (Cedar Woods, the burning hut, Yraall Road, Syn City, Mt. Glaus, McNeil Manor, Ogre Road, Mt. Myrneg, the Dump, Wyndia's basement, Parch), a Faerie Village minigame's script, camp lines, the chicken coop's chickens, a demo-disc message | the area scripts are in the `DAT` message pools this repo already decodes (`loc_build.py`, [`dialogue-localisation.md`](dialogue-localisation.md)); an id reached by no script op is the lead |
| **The whelp's portrait** | a menu portrait for the whelp, never shown because the menu is off until Rei and Teepo equip you; the portrait's whelp is blue, the field's green | the owner will try to reach the menu with an F12 save; whether the PC build has the portrait at all is unread |
| **Sunder's animation on Mt. Myrneg** | an animation that plays on entering the area and lasts about four seconds, over before the player can reach the place where Sunder is on screen; seen only with a walk-through-walls cheat | the owner's proposal: **make it loop**, so a player arriving normally sees it. A divergence, for when that area is taken over (§4) |

The page also lists an unused item ("THE MOCHI", cures a petrification that
does not exist), places behind walls with items in them (McNeil's safes,
Rhapala's shelves), and the edges of the map. Those are recorded here and
not planned.

## 2. The skills, located

TCRF gives each unused skill a hex number. It is the **ability id**, and it
confirms the off-by-one the sibling found reading Pilfer
(`../BreathOfFire3Recomp/docs/STEAL.md`): the name of engine id N is
`names/magic.toml`'s label for id N-1. For Again `0x70`, Pentagram `0x71`,
Ink Ink `0x92`, Death Bomb `0x93` and Roulette `0x94`, TCRF's name is the
label one id lower. The four skills TCRF says only print Japanese text or
gibberish (The World, Again, Death Bomb, Roulette) all load one row, 27. So
**`magic.toml`'s English names are to be read shifted**; its rows, files
and entries are right as they stand.

The effect table's row gives the PC entry: PC `Magic_Rows` `0x64C2B8` is
the sibling's PSX effect table `0x800B3538` row for row (on all 142 overlay
rows the PC's file id is the PSX's plus `0x105`; the five engine-side rows
are engine-side on both). Measured 2026-09-25.

**Unfinished skills** (TCRF "Entirely Unused"):

| Id | TCRF's name | Row | Overlay | PC entry | TCRF says, in short |
|---|---|--:|---|---|---|
| `0x19` | White Flag | 9 | `MAGIC010` | `0x49DEF0` | flashes white, does nothing |
| `0x50` | Raaku | 27 | `MAGIC080` | `0x4FC330` | field only, does nothing |
| `0x6F` | The World | 27 | `MAGIC080` | `0x4FC330` | prints text, does nothing |
| `0x70` | Again | 27 | `MAGIC080` | `0x4FC330` | prints text |
| `0x71` | Pentagram | 10 | `MAGIC113` | `0x4D67F0` | **works**, with an animation seen nowhere else |
| `0x7F` | Assault | 126 | engine | `0x4378D0` | halts the game a moment, then a plain attack |
| `0x91` | Ink | 149 | `MAGIC145` | `0x4E9A70` | a smoke effect, no damage |
| `0x92` | Ink Ink | 150 | `MAGIC146` | `0x4E9EF0` | the same |
| `0x93` | Death Bomb | 27 | `MAGIC080` | `0x4FC330` | prints text |
| `0x94` | Roulette | 27 | `MAGIC080` | `0x4FC330` | prints text |
| `0xD5` | PurifyAll | 148 | `MAGIC213` | `0x4F4A60` | an animation, no effect |

Row 27 is shared with MagicShuffle, Recall, Lark and others the player does
meet, so it is a general handler, not a stub; what it prints for these
four is unread.

**Enemy-only skills** (TCRF's second table), where TCRF reports something
beyond "an enemy uses it":

| Id | TCRF's name | Row | Overlay | PC entry | TCRF says |
|---|---|--:|---|---|---|
| `0x80` | Head Cracker | 128 | engine | `0x43FC80` | **freezes the game** |
| `0x8B` | Paralyzer | 123 | engine | `0x43F3B0` | **crashes the game** |
| `0x81` | Holocaust | 145 | `MAGIC129` | `0x4E3260` | wrong description; neutral damage |
| `0x39` | Bone Dance | 84 | `MAGIC057` | `0x4AE7C0` | wrong description |
| `0x51` | RottenBreath | 85 | `MAGIC081` | `0x4BF8D0` | wrong description; poisons all enemies |
| `0x74` | UtmostAttack | 59 | `MAGIC116` | `0x4D9AE0` | five elements at once |
| `0x20` | Absorb | 44 | `MAGIC070` | `0x4B8D70` | shares row 44 with Heal - **ours** since round eight (`Sparkle_Task`) |

The other 22 are located the same way. The full list with every row, file
and entry is the output of the script in §5. One name on TCRF's list,
Hypnotize, is carried by no id.

Holocaust is not cut content and not engine-side: it is an enemy skill in
its own overlay. The engine-side rows are rows 0, 108, 123, 126 and 128, and the two
TCRF reports as freezing and crashing are among them. Both are
**known-defects candidates**, not entries: the claim is a third party's, on
the PlayStation, with the skill hacked into a player's list. The PC's
behaviour is unread.

Four rows have an overlay and are loaded by no ability in `magic.toml`:
rows 2, 7, 119 and 147. They may be item or enemy magic through the item
row table, which was not checked, or cut content.

## 3. What this changes for the spell round

The spell round proposed on 2026-09-25 takes the effect table's rows by
harness, fuzz-only. This page gives it three things:

1. **Names**, shifted, for the queue and the docs.
2. **The rows no player casts**: the unfinished skills and the enemy-only
   ones are rows like any other and are taken with the rest. Their fuzz is
   as good as any row's; their live check is the owner, with a cheat that
   puts the skill in a list, if at all.
3. **Two rows to read first**: the engine-side 123 and 128. If ours
   reproduces a freeze or a crash, that is a defect to write down, and a
   living project may fix it (a divergence, ledgered). Read 2026-09-25,
   [`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3:
   row 123 reads an event-battle-only pointer of the current enemy
   unconditionally (a crash if it is 0 in an ordinary battle - not
   measured), row 128 has three unbounded waits on the target's reaction
   state (a freeze candidate). Neither is taken yet.

## 4. The Sunder loop, as a divergence to come

TCRF's account: the animation starts when the area is entered and ends in
about four seconds, before the player can reach it; it fits Sunder's line
when spoken to before the cutscene that removes him. The owner wants it to
loop, so it is seen. When the area's code is ours:

- find the animation's start in the area's entry (Mt. Myrneg's area number
  is in the area table; the object and its animation are the movement
  script's, [`movement-script.md`](movement-script.md));
- make it repeat while Sunder is in the area, and stop where the cutscene
  removes him;
- a DIVERGENCE entry: what the original did (plays once, unseen), what this
  does (repeats), why (the owner: the scene was meant to be seen).

Nothing is built. The owner judges it by eye. TCRF flags the scene as not
safe for work, so whether the loop is on by default or opt-in is the
owner's call.

## 5. Regenerating §2

The mapping is a few lines of Python over `bof3/BOF3.exe`'s `Magic_Rows`
(151 rows of a u16 file id and a code pointer at `0x64C2B8`) and the
sibling's `names/magic.toml`, with a skill's name taken from the label one
id lower. It was run in the session scratchpad and not kept as a tool; the
spell round's harness group made it one: `tools/magic_rows.py`
(2026-09-25), which also finds each overlay's PC extent
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §1).
