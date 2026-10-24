# The sound-layer round: the PlayStation's positional sound, pause and cuts

**Status:** DRAFT (staged 2026-10-23 at the owner's word; four groups cut,
briefs written, branches `sound/in`, `sound/sp`, `sound/st`, `sound/hm` off `main`
`300ce67f`, merge target `sound/round`; nothing built yet). Fills in as the
groups hand back; "as built" replaces this header at merge.

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
