# Owner review list - things that want the owner's eye or ear

**Status:** IN PROGRESS (2026-10-06) - started the morning after the platform
round merged (PR #42), from the calls [`platform-round.md`](platform-round.md)
section 4 and [`round-14-cleanup.md`](round-14-cleanup.md) section 5
gathered. One list, so the questions only the owner can settle in play are
not scattered across round docs. **Rewrite, do not append**: an item leaves
when the owner has looked and said, and its answer goes where it belongs
(the ledger, the defect list, the round doc). Agents add here; the owner
strikes.

Each item says what to do at the machine, what to look or listen for, and
where the reading behind it is. Nothing here is a divergence until it is in
[`DIVERGENCE.md`](DIVERGENCE.md).

## To review in play

1. **Music coming back after a pause** (`Sound_ResumeAll`,
   [`sound-rest.md`](sound-rest.md) section 3 item 2). Read from the code,
   never heard: `Sound_ResumeAll` plays whatever music buffer is loaded,
   whether or not it was playing, and a stopping fade leaves the buffer in
   place. **At the machine:** reach a scene whose music fades to silence
   (an area change through the field mode's `Music_FadeOutStop`, or a
   scenario step that fades), then while it is silent press F9 (pause) and
   any key (resume). Also: Alt-Tab away and back during the silence. **Listen
   for** the faded track returning. Run it on ours and under
   `BOF3X_ORIGINAL='*'` - the same on both is Capcom's, and a fix is a ledger
   entry. The scripted resumes (the inn's night, the flag-message fanfare,
   area 113's reward, area 135's box, chapter 11 step 47, areas 57 / 108 /
   145's shared handler) stop the music themselves first and are not the
   case.
2. **DIV-0076, the save slot's summary** ([`round-14-cleanup.md`](round-14-cleanup.md)
   section 5): save with someone other than record 0 leading, then read the
   slot on the load screen - name and level should now be the same
   character's.
3. **The layering fix in play** (DIV-0071, HANDOFF item 0000000): a town,
   stairs, a bridge, followers close behind, with the default on.
4. **TILE_1 drawn as one point** ([`d3d-rest.md`](d3d-rest.md) D-a). Shown to
   the owner 2026-10-06 from the `whelpBoss` route's frame 11880 (the dream
   scene, Deis in the light pillar: the drifting specks are TILE_1s, each one
   screen pixel where the PlayStation's covered four at this scale). A paint
   mock of the 2x2 was sent; the owner: "go ahead and make it larger".
   **Built the same day as DIV-0077** (on by default, `BOF3X_TILE1=0` the
   point). Owed the owner's eye in play: the dream scene after the whelp
   fight, or any Kaiser cast.

## Debt 3: measured by a log line, review the log

- `PartyAction_SpawnKind1B` `0x5252B0` now logs `debt3` when
  `Effect_FindFree` answers 0xFF (added 2026-10-06, `rest_1f.cpp`; behaviour
  unchanged). After a play session, grep `bof3x.log` for `debt3`. A hit means
  the wait polled a packet-pool byte ([`rest_1d.md`](rest_1d.md) L1); the
  owner then decides whether the action should end at once with no effect
  placed (a ledger entry) or stay as Capcom's.

## Parked, documented, revisit if ever seen

- **The `0x91` camp cells** ([`rest_2d.md`](rest_2d.md) L1,
  [`psx-twin-check.md`](psx-twin-check.md)): 91 real cells where the camp
  opens though the compare says refuse; Capcom's on both machines. The owner
  (2026-10-06): leave it, revisit later. A fix is a one-line divergence.
- **The one-texel POLY_FT3 colour** ([`d3d-rest.md`](d3d-rest.md) D-b): a
  triangle whose corners share one texel is flattened to a colour read from
  the PSX offsets of a primitive this port widened to floats, then the wrong
  nibble. No route reaches it, no builder of one found. The owner
  (2026-10-06): leave it documented; review if a miscoloured speck is ever
  spotted.

## Decided, waiting to be built

- **`Cfg_Load`'s key-line overrun** ([`shell.md`](shell.md) section 5): the
  owner (2026-10-06) wants overrun protection. The fix the group proposed:
  read the pairs into a zero-filled 32-entry array of our own, two lines an
  entry as the original packs them, ignore lines past the 32nd. Wants a
  ledger entry (visible only to a hand-edited `BOF3.CFG`).
