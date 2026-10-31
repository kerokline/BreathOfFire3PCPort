# The sound-layer round: the PlayStation's positional sound, pause and cuts

**Status:** BUILT (2026-10-23 evening, `sound/round` off `main` `300ce67f`; the four groups merged
one at a time, DIV-0169..0175, D353 and D354; the end batch's state hash and WAV dumps in section 5,
its last runs re-made on the fixed build and every WAV difference attributed to its entry; **not heard by the owner** - the listens and two calls in
[`owner-review.md`](owner-review.md); merged as PR #122). Sections 0..3 are the staging as written;
4 and 5 are what was built and measured.

## 0. What the round is for

The twin-diff of 2026-10-23 ([`known-defects.md`](known-defects.md)
D335..D347, PRs #115 and #116) left one class of port defect unbuilt in fix
wave 1023 (PR #117): **the port's DirectSound layer replaced the
PlayStation's positional sound, its pause and resume, and its cuts with flat
effects and fades.** Four entries name it:

| Defect | Where | The PlayStation | The PC |
|---|---|---|---|
| D343 | the inn night, `InnNight_*` `0x57FBE0..0x57FCB0` | pauses the four SEP slots as the fade starts and replays them, at their position, after the restore's message | plays the jingle over the area's released stream and the song never returns: `Music_Track` still names it, so every later `Music_Play` of it is a no-op (measured by IN to `shop.txt`'s end) |
| D345 | three hooks: movement op `B8`, `Sprite_QueueOverlay`'s bit-3 update, `Sound_PlayById` | a sequence stopped / set / replayed; a voice's volume and pan by the sprite's position; a level table and a distance-gated repeat | a bare `ret`, a bare `ret`, `Sound_PlayEffect(arg0)` |
| D347 | every `Music_FadeOutStop(0xA)` whose twin is `SsSepStop` | cuts | fades ten frames, then stops |
| D337 | `BattleAction_EffectPreload`'s key-off | releases voices 16..23 | DIV-0167 stops the loops; a one-shot plays on |

Each is the port's (the PlayStation twin differs, every masked word of the
twin identical between the US and JP discs), so each repair is a
restoration in [`psx-twin-check.md`](psx-twin-check.md)'s sense: Sensible
tier, on by default, a `BOF3X_ORIGINAL` switch each, the PC's behaviour
one environment variable away. D347 is the one the owner's ear decides:
built on, flagged.

## 1. The groups

Briefs: `C:\Users\kerok\bof3x-runs\sl1023\brief_common.md` and `groups.md`
(game-derived scratch, not in the tree). DIV reservations: IN 0169 (0170 spare, folded at merge),
SP 0170..0172, ST 0173..0174, HM 0175 (0176 unused); new defects numbered at merge
from D353. The twin-diff remainder (HANDOFF item 0b: the 193 unmarked
twins, `psx_pair.py propagate` checked against `us_jp.tsv`, the records
audit's naming items) runs beside the round on `audit/twins-remainder`, its
own PR - tooling and a typing pass under `codegen_same.py`, no behaviour.

| Group | Branch | Defect | Builds | Runs |
|---|---|---|---|---|
| IN | `sound/in` | D343 | the area's song halted at the night's fade and resumed at its position after the restore's message; measured first on `shop.txt` with `BOF3X_MUSIC_LOG=1` to find the path that brings the song back today | one live run, `--no-front`, its own root |
| SP | `sound/sp` | D345 | op `B8`, the bit-3 voice update (volume and pan by position on the voice's buffer), `Sound_PlayById`'s level and distance gate - each a DIV, each reported instead of built where the PC's records cannot carry it | headless |
| HM | `sound/hm` | D340, D344, D350, D352 | the held measurements: D352's reset built as DIV-0175 (`WindowResetFull`, the US clears `+0..+0x1F`; only the window records' page moves, nothing downstream); D350 measured and held (the US wait would add 34..58 frames to every Western attract pass - the owner's call, `Scena16CaptionWait` reserved); D344 a plan (two recipes answer a field save's yes: `balioAndSunder_2`, `sigma_swap`); D340's route never draws the panel's marker | live, after IN; its own root |
| ST | `sound/st` | D347, D337 | `StopCuts`: every `Music_FadeOutStop` caller read beside its twin, cut where the twin is `SsSepStop`, the fade kept where it is `SsSepSetDecrescendo`; the one-shot key-off remembered DLL-side per effect channel | headless |

**Rules the groups share** (the common brief): no logic frame added or
removed (the recipes are frame-exact); new state DLL-side, never in game
memory, so the state hash and the saves see nothing; helpers in a file per
group so the branches merge clean; the twin bundles read, never named or
quoted in the tree.

## 2. The end batch (at merge)

1. Merge `sound/in`, `sound/sp`, `sound/st` into `sound/round` one at a
   time `--no-ff`, the build and `BOF3X_SHADOW='*'` after each;
   `tools/ledger_check.py`.
2. The state hash on the ten routes against the `orig4` pairs
   ([`state-hash.md`](state-hash.md) 6): the only new pages must be the ones
   each DIV names, each confirmed gone with its switch off.
3. The WAV dump on `shop.txt` (the inn), `combat.txt` (the key-off),
   `whelpBoss.txt` (the dream's cut, DIV-0143's knock-on gone) and one route
   per `B8` / bit-3 scene SP names: `tools/wav_diff.py` before and after,
   each difference one the DIVs predict.
4. [`owner-review.md`](owner-review.md): the listens - the inn's song
   resuming where it stopped, the cuts (D347: their call), the positional
   voices in the scenes SP found, the one-shot key-off in a fight.
5. PR.

## 3. Not this round's

The 193 unmarked twins and the pairing check (`audit/twins-remainder`,
above); D344's repair itself, if its measurement says the recipes must be
re-recorded (the owner's call).

## 4. As built

Every repair is on by default and reversible by `BOF3X_ORIGINAL=<switch>`; each ledger entry says what
changes to the ear. The groups' numbers were renumbered at merge to close the gap IN left (SP's and ST's
own commits say 0171..0175; the merge commits record the shift).

| DIV | Switch | Defect | What changed |
|---|---|---|---|
| 0169 | `InnNightPause` | D343 | the inn's night halts the area's song as the fade starts (`Sound_StopMusic`, no fade) and starts it again at the sample heard, after the morning's message (`music_resume::Heard` / `Pend` + `Music_Start`); both music paths. The measurement first: the PC's song **never returned** after a night (`Music_Track` still named it; wav-dump.md 4c's "99 s" was the picture) - there was no restart to suppress |
| 0170 | `MoveB8Restart` | D345 | op `B8` restarts the loaded song from its top at the script's level, as the PlayStation's stop / set / replay of sequence b2 (only b2 = 0 can map; any other is fatal). Two shipped scripts use it: area 142 (+0x18 script 2), area 174 (+0x10 script 35), both `B8 00 00 64`. Writes game memory only when it runs |
| 0171 | `SpriteVoicePosition` | D345 | `Sound_PlayById`'s buffers are remembered (DLL-side) under the sprite that played them and, each frame that sprite is drawn, given the PlayStation's volume-and-pan law against the buffer's own level (up to about 3.5 dB down on the far side at the screen edge; under `Field_ScriptFlags2` bit 0x1000 louder lower on screen and panned fully). The buffer gets its level and pan back when it stops or before its next flat play. The PlayStation's bit 3 and `+0x70` voice word are not built: no PC code reads them |
| 0172 | `SoundLevelGate` | D345 | ops `BC` / `BD` / `D1` and the scenario's level cues play at their level from the PlayStation's table (a rule, 78 of 82 entries; the four others read by no shipped call), capped at the PC's full level; `D1`'s repeats gated by distance from their object (areas 3 and 8: silent beyond 22 cells, -23.6 dB within, -3.5 dB within 12, full within 8); area 9's `BC` plays `0x1202` hard left; `Scena01_Scene09`'s `0x1206` is level 0 on both discs and so silent (the owner's call) |
| 0173 | `StopCuts` | D347 | `Music_FadeOutStop` cuts (`Music_Stop`, the synth too on the sequenced path) and still writes every game word the fade wrote, so `Music_Track` goes 0xFF at the first tick as before. All 131 PC call sites were read beside their twins: every one pairs with a stop (`0x8015DDF4`, the every-SEP stop `0x8015DFC8`, or `Music_FadeOutStop`'s own twin `0x801626C0`, which decrescendos and then stops at once); none with the decrescendo alone (JP `0x8015DEE8`, US `0x8015D4F8` - the brief's attribution corrected; its sites line up with the PC's 33 `Music_FadeOut` calls). No caller changed. DIV-0143's 27-frame knock-on is gone |
| 0174 | `OneShotKeyOff` | D337 | `Sound_PlayEffect` remembers (DLL-side) the last one-shot's bank entry and buffer per effect channel; DIV-0167's key-off at `BattleAction_EffectPreload` stops each whose entry still holds it and no looping channel holds |
| 0175 | `WindowResetFull` | D352 | `Window_ResetAll` clears `+0..+0x1F` of each of the 22 records, as the US executable (not all 0x24: `+0x20..+0x23` are kept - the defect's text corrected). Only the window records' page moves in the state hash, on the attract and six routes, every differing byte in a freed record but one: `shop.txt` tick 1,574, a list window's first frame drawn with the last list's marked index (`+0xC`), one frame |

**Measured and held** (group HM): **D350** - on the attract run message 2 is already closed when counter
4 is raised with no layer and under `ja-JP`, but under the Western layers it is still open (en-US 34 frames,
fr-FR 40, de-DE 58), so the US wait would shift every Western attract pass: the owner's call,
`Scena16CaptionWait` reserved. **D344** - 52 recipes played with logging; two answer a field save's yes
(`balioAndSunder_2` f21,726 slot 6; `sigma_swap` f3,788 slot 7, the button round's references after it);
`recipe_shots.py` cannot insert frames, so the repair wants an `--insert` step and those re-recorded: the
owner decides. **D340** - `caughFish` captured every 10 frames never reaches `0x4680F0`'s draws; the
PlayStation side wants a savestate the sibling's runtime does not have: more than an hour, next step a
recording that opens the fishing screen's other tabs with a call trace on `0x4680F0`.

**Found on the way:** D353 (two `psx` fields that differ from the US disc by four bytes - region drift,
settled on the JP disc, nothing to change) and D354 (the merged build's first combat run crashed in
`DSOUND.dll` at the battle's end: `Snd_LoadBank` released a buffer DIV-0171's table still held; fixed by
telling both DLL-side tables by address before each release, `5e85d91a`). The brief's "cite the
PlayStation addresses, never the decompilation" held in every file.

## 5. The end batch (2026-10-23 night, `C:\Users\kerok\bof3x-runs\sl1023\merge\`)

- **Self-tests** `BOF3X_SHADOW='*'` from an ini-less launcher copy: exit 0 after each merge (1,092 counts
  after IN, 1,094 after SP and ST, 1,094 on the merged build and again on the D354 fix), every count 0,
  `inject: 10087 ours`, the seven DIV lines armed. `ledger_check.py`: 175 entries, 0 errors.
- **The state hash**, ours against the `orig4` pairs on the ten routes (`chain_sh.py`, the exe host,
  `BOF3X_OPT=none BOF3X_LAYERING=0`), read against the pre-round signatures (`sh1023/check_*_w1023.txt`):
  caughFish, field_menu, menu_screens identical to the pre-round pages; cutsceneAndNue, whelpBoss (13,121
  ticks) and worldMapAndAreaTransition one new page, `0x803000` (the window records, DIV-0175);
  masterAndManillo three run-noise pages fewer; shop six pages, all predicted - `0x803000` from tick 1,206,
  the four one-tick draw pages at 1,574..1,575 (DIV-0175's one frame) and `0x7DE000` from 2,731 (DIV-0169's
  morning `Music_Start`: the music globals its entry names). **combat and dragonTransform crashed on the
  first run** (D354, tick 1,332 and mid-fight) and were re-run on the fixed build (`chain_sh2.py`, the
  `sl1023b` runs): combat the full 2,561 ticks with its one page `0x803000` at 1,663..2,315 exactly as the
  pre-round signature has it; dragonTransform the full 4,289 ticks with `0x803000` the one new page. So on
  all ten routes the round adds `0x803000` (DIV-0175) where a window is reset, shop's `0x7DE000` (DIV-0169),
  and nothing else; DIV-0170..0174 keep no state the hash can see, as designed.
- **WAV dumps** before (`main`'s behaviour, the fix wave's launcher) and after, `tools/wav_diff.py`:
  `shop` first difference at 83.0 s, 16 windows, the before side at -120 dBFS from 91 s where the after
  side has the town's song (DIV-0169, as IN measured); `whelpBoss` four windows (34, 38, 184, 345 s) each a
  fraction of a dB and none at a `StopCuts` line (f115, 1157, 2642, 7057, 8846, 10346, 10363: at each the
  volume was already 0 from the `Music_FadeOut` before it, so the cut and the fade are the same audio),
  attribution below); `combat` (re-run on the fixed build) **no window differs**, at 1 s or at 0.1 s: its
  four `StopCuts` lines (f175, 426, 2501, 2619) all fall where a new track starts in the same frame, so the
  fade never sounded, and no one-shot of the route rang long enough for the key-off to cut.
- **The whelpBoss windows at 0.1 s:** 34.4..34.6 s (+0.3..0.5 dB), 38.6..38.7 s (+11, +71 dB), 184.7 s
  (+2.2 dB), 345.2..345.3 s (+7, +76 dB) - the two large ones are a sound present on the after side, not a
  pan's few dB. **Attributed by switch** (one whelpBoss run each, `--original <switch>`, diffed at 0.1 s;
  `end_batch3.py`..`5.py`): with **every switch of the round off** the run is identical to before in all
  4,400 windows - so none of it is run noise and the hosted host does not matter; `SpriteVoicePosition`
  off removes 34.4..34.6 s (DIV-0171's pan, ±0.3..0.5 dB); `SoundLevelGate` off removes 184.7 s (DIV-0172,
  -2.2 dB); `StopCuts` off removes the two large windows, 38.6..38.7 s and 345.2..345.3 s. Read with the run's
  frame rate (the music log ends at f12,071 in a 439 s dump, about 30 frames a wall second) they are the cuts
  themselves: f1,157 (track 4's stop) and f10,346 / 10,363 (the bedroom's track 14 before the dream,
  DIV-0143's knock-on), and `wav_diff`'s delta is before minus after, so +71 / +76 dB there is the after
  side **silent where the fade's 0.17 s tail was** - the cut, exactly. `OneShotKeyOff` off differs nowhere:
  no one-shot of the route rings into the next action. So every window of every dump is one a ledger entry
  predicts, and the route with every switch off is the original to the sample.

## 6. Left

- The owner's listens and two calls ([`owner-review.md`](owner-review.md) 2026-10-23 evening).
- D350's build if the owner takes the US timing (re-record the Western attract references); D344's if two
  frames on every save are wanted (an `--insert` step, `sigma_swap` and `balioAndSunder_2` re-recorded).
- D340's PlayStation capture.
- The `B8` areas 142 and 174 and the positional scenes have no recorded route: a listen in play.
- The `--speed 8` state-hash noise HM saw (VRAM, sound, `0x937000`) and worldMap's run-to-run `0x6C3000`:
  a reading for the state-hash doc, not this round's.
