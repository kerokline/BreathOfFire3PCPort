# Owner-review sweep, 2026-11-08 - a proposal sheet, not the file

**Status:** DRAFT (2026-11-08; proposes, changes nothing - [`owner-review.md`](owner-review.md) is edited only after
the owner confirms each row; [`beta-readiness.md`](beta-readiness.md) section 4 item 7)

## 0. What this is

[`beta-readiness.md`](beta-readiness.md) 4.7 asks for the owner-review list swept against the two full clears, so
that the beta notes' "Watched, not yet played" is section 1's remainder and nothing else. This sheet walks
[`owner-review.md`](owner-review.md) top to bottom, in its order, and proposes one verdict per open item. The owner
reads it, answers the questions in section 2 and says which strikes stand; then the file changes.

**The evidence a strike may rest on:**

- **The owner's clear**, 2026-11-07: the PC-port install in `game\`, under the furigana row (DIV-0155 / DIV-0156),
  at 21:9, sound on (SDL3, DIV-0201's default), the Recommended preset (the defaults, every ledger switch at its
  default - [`ledger-fields-round.md`](ledger-fields-round.md) "Recommended: nothing"), with the enemy-information
  cheat (DIV-0187) and the no-fail minigame cheats (DIV-0188) on; the ending and the clear save. Their play was at
  chapter 10 on 2026-10-30 ([`STATUS.md`](STATUS.md), the mountain passes), so the clear continued a game begun
  earlier unless the owner says otherwise - question 3 below.
- **The 64-bit stretch**, earlier the same day: music, voices, the inn's night and the Capcom movie heard and seen
  right. Counts for sound and movie items; it is not a full play.
- **ScrubOasis's clear**: PSX-US + PSP-EU mixed install, English, to the clear save, on their own build. It proves
  a disc-built install plays to the end without a blocker. It is **not the owner's eye**: a "look at" item on a
  disc install is not struck on their silence unless the owner says that counts (question 14).
- **The third player**, PSX-FR disc alone, in progress: named where it will be the evidence; strikes nothing yet.
- **The owner's own answers already in the file** (2026-11-07 night, 2026-10-31, and the struck sub-sections)
  stand and are not re-proposed.

**The three verdicts:** **STRIKE** - a clear under those conditions reaches it, and the row says which clear and
why. **KEEP** - no clear reaches it; the row names the install, condition, recording or decision that would.
**ASK** - a clear reaches it only if the owner did one particular thing; the row states the one question. The rule
applied is strict: a defect the owner would not have noticed without looking for it is not struck on "nothing was
reported" unless the item's own wording asks for a word only if something is wrong.

Rows marked "(stale text)" propose rewording the item as well, because the file's wording predates something that
has since landed.

## 1. The rows, in file order

### 2026-11-07 evening, the ending scenes (PR #209)

| Item | Verdict | Evidence or question |
|---|---|---|
| The *Fin* disc and the night, DIV-0207 `FinNight`; the sheet call on `kFinRadius` | STRIKE | The owner's clear reached the ending at 21:9, and STATUS (2026-11-07) records the ending scenes "played and approved by the owner (the night, the disappear and reappear effect)". The radius call stays an open offer, not an owed look. |
| The desert's start: no sand flash in the bands; the 320-wide hold pillarboxed | STRIKE | The same outro, seen in the same replay. "Say if that hold wants bars" is answered by silence (leave it) unless the owner says otherwise. |
| Myria's chamber: Rei and Garr scatter and gather; the one-frame white silhouettes | STRIKE | STATUS's "the disappear and reappear effect" approval. The silhouettes are the scene's own, by the code; nothing owed unless the owner says they read wrong. |
| `PixelRoom`, the new switch | STRIKE | Nothing to look at: a Player switch, on by default, through the approved replay. The row is a notice, not an owed item. |

### 2026-11-07, the 64-bit round's stage D

| Item | Verdict | Evidence or question |
|---|---|---|
| The 64-bit build played: music past its first quarter second, battle voices, inn, the two FMVs' picture, the Sources page | STRIKE | Answered in the section itself (the owner, 2026-11-07): music, voices and inn right; the Capcom movie shows since DIV-0205; the Sources page looks right. |
| One import run from the 64-bit launcher (the child's start and its pipe) | KEEP | Developer build, a specific action nobody has taken; "if you care to" in the file. Retired by one Sources import from a 64-bit launcher folder. |
| The song's place under SDL3 against DirectSound (the 546 ms figure); the inn under `off=SoundSdl` | STRIKE | The owner heard the inn's night under SDL3 and found nothing unusual; the section's own rule says that leaves the figure as the dump's model, and the doc already says so. The DirectSound half is moot by that rule. |
| The rest of DIV-0201's list: voices at their cue frequencies | STRIKE | "The voices sound fine" (the owner, 2026-11-07, the 64-bit stretch; the same output code on both widths), then two clears with sound on. |
| ... DIV-0171's pan | ASK | See the sound-layer round below (question 7). |
| ... the fades of DIV-0028 / DIV-0143 | ASK | DIV-0028's title fade is heard at every start (STRIKE on its own). DIV-0143 is the bedroom's song through Ryu's dream at the game's start: reached only if the clear began as a new game (question 3). |
| ... the quicksave across a sound | KEEP | This is D406 (a quick-load during a battle voice), fixed 2026-11-07 and not yet heard. A known-issues line if not heard before the cut (beta-readiness 2). |

### 2026-11-03, issue #184's desert directions (DIV-0203)

| Item | Verdict | Evidence or question |
|---|---|---|
| The German wording on a rebuilt `de-DE` layer; the French and English lines if to hand | KEEP | German: a rebuilt `de-DE` layer and Horis's talk, no clear reaches it. English: the owner's main text was Japanese under the row, and ScrubOasis's build may predate PR #193 (a layer gets the rule only when rebuilt). French: the third player's disc is the nearest live evidence, in progress. |

### 2026-10-28..31, owed after the merges

| Item | Verdict | Evidence or question |
|---|---|---|
| Exe-gap: the loss tune on a disc-only install (DIV-0192, D320) | KEEP | Needs a disc-built install **and** a lost fight; the owner's install is the PC's (question 1 decides whether a disc is even present). ScrubOasis's clear tells nothing unless they lost a fight. |
| Exe-gap: the PAL banks on a European disc-only install (DIV-0194, D257) | KEEP | A European PlayStation disc as the base; ScrubOasis's base is the US disc. The third player's French disc is the live evidence, in progress. |
| Exe-gap: DIV-0195's regional boxes on a Western PSX-only install (area 135's panel pieces, kind 0x5E's frame and menu box) | KEEP | A Western PSX-only install and an eye. ScrubOasis's mixed install passed without a report (question 14). |
| Exe-gap: the word on a fresh `cn_<route>_orig4` re-record | KEEP | A decision, not play. |
| The Preset page: the other three presets | KEEP | Answered in part 2026-11-07 (page seen, play under Recommended). The three presets are "checked headless" in the notes; beta-readiness 1 lists them. |
| The consumables sorted by id (DIV-0189): a sort on a long game's bag; a hand-written type order | ASK + KEEP | ASK: did the owner use the Items screen's sort on the clear's bag (question 9)? The type-order word is a decision: KEEP. |
| The no-fail cheats: the well's start recording | KEEP | Harness, "not for the release" in the file. |

### 2026-10-23 evening, the sound-layer round (DIV-0169..0175)

| Item | Verdict | Evidence or question |
|---|---|---|
| The inn's night, DIV-0169 `InnNightPause` (D343) | STRIKE | "Nothing unusual at the inn's night" (the owner, 2026-11-07, the same sound code on the 64-bit build), then a full clear with sound on through every inn. **D343 in beta-readiness 2 reads "open"; this strike would close its heard half - see section 4.** |
| The cuts, DIV-0173 `StopCuts` (D347, the owner's call) | STRIKE | The item asks for a word only where a cut sounds wrong. Two clears heard every fight's end, the title's stop, every chapter change; "the combat loop sounds right". No scene named. |
| A voice that follows its sprite, DIV-0171 `SpriteVoicePosition` | ASK | The item says headphones. Question 7: was any of the play on headphones (or stereo speakers), and did a left-side hit or spell pan left? |
| The levels and the distance gate, DIV-0172 `SoundLevelGate`; the silent sound in chapter 1's scene 9 | STRIKE (conditional) | The item's own rule: "if you remember a sound there and miss it, say so"; nothing was said. Holds only if the clear passed chapter 1 (question 3); otherwise KEEP for a new-game start. |
| Op `B8`, DIV-0170 `MoveB8Restart` (areas 142 and 174) | ASK | "No route reaches either; a visit tells." Question 6: did the area's song restart from its beginning mid-area anywhere, in particular in areas 142 or 174? |
| The one-shot key-off, DIV-0174 `OneShotKeyOff` | STRIKE | Fights only, on by default, two clears with sound; a ringing tail cut short is the audible shape and "the combat loop sounds right". |
| D352's window reset, DIV-0175 `WindowResetFull` | STRIKE | "Nothing to hear" by the item's own words; at most one frame in a shop, and shops were used through the clear. |
| D350, the attract caption's wait (build it, or the JP timing) | KEEP | A decision. Beta-readiness 2 lists it. |
| D344, the save confirm's two frames | KEEP | A decision (two frames on every save, a recipe `--insert` and re-records). |

### 2026-10-23, the battle twin-diff against the PlayStation

| Item | Verdict | Evidence or question |
|---|---|---|
| D335, the scaled damage's width | KEEP | A decision; "nothing to see at the machine until a hit that large is found". |
| D337, the effect preload's voice cut: a spell's tail under the next action's effect | STRIKE | The item's rule is "if heard"; two clears of fights with sound on, nothing heard or named. D337 stays a latent line in known-defects, as the port's. |
| The grown command icons' texture column (dragon fights) | ASK | Question 8: in a dragon fight, was every grown command icon the right picture, none cut? |

### 2026-10-22, the reading-row round (DIV-0155 / 0156 / 0157)

The "Still open" sub-section is decisions and work, not play. One grouped row for it, then the "To play" list.

| Item | Verdict | Evidence or question |
|---|---|---|
| Still open: the release gate (SudachiPy not bundled); ML's two questions (a mod into `cache=`; the dropdown's wording); the readings audit's remaining (b) and (d), the lexicon follow-on and fugashi `--compare`; the launcher's two-step first-install text; the `prev`-across-a-row-break builder follow-on; D328 unproofread | KEEP | Decisions and follow-on work. D328 is in beta-readiness 2 and the notes' known issues; the release gate shapes the notes' furigana line ("does not build from the zip"). |
| To play: the new game's opening with stops; "say if you want an arrow" | ASK | Question 3: did the clear begin as a new game under the row? If so STRIKE (every timed box held to a press; silence on the arrow = leave it). If the clear continued the October game, KEEP for a new-game start. |
| To play: the opening with rows kept (English over Japanese); the balance rule's few words above the name row | ASK | Question 4: which mode did the owner play - Japanese text with furigana, or a second language over Japanese (or Japanese over English)? The row-kept sheet is the second-language mode's. |
| To play: the Mods page's Data / Extras sub-row and the hidden dropdown | STRIKE | To play under the row at all, the owner chose its entry in *Row above the text*, which lives behind the Extras toggle on the Mods page; the path was walked. |
| To play: the dolphin fight with furigana - the gap (1.5 units) and how a reading appears with its word | STRIKE | The gap and the reading's appearance were under the owner's eye for a whole clear's dialogue under the row; the dolphin scene was only the capture scene. The one-more-press note is the recipe's (D329), harness. |
| To play: F4 hides and shows the readings | ASK | Question 4: did the owner press F4 during the clear? |
| To play: English over Japanese / Japanese over English | ASK + KEEP | Question 4 decides which mode the clear covers; the other mode stays KEEP. |
| To play: the Mods tab's install of the zip | STRIKE (source build) + KEEP (release folder) | The owner added the zip and the layer built on their machine (SudachiPy present), so the install path is walked. The release folder's loud error instead of a layer is unplayed and is the gate decision above. |
| Also on the sheets: D326 the nue save's names | KEEP | The recipe save's, harness. |
| Also on the sheets: D327 the next-page arrow on a one-pair page | STRIKE | The original's rule, seen on one-pair pages through a whole clear under the row, nothing said. |

### 2026-10-21, the wide-ring round (DIV-0041 amended, DIV-0151)

| Item | Verdict | Evidence or question |
|---|---|---|
| The ring: field scenes at 16:9 and 21:9, far or turned cameras | STRIKE | The whole clear at 21:9 with the ring on by default; "the side edges should not notch" is the kind of thing the owner reports (they reported the first ring picture and every band glitch since). |
| The ring: a quicksave taken at one width and loaded at another | STRIKE (moot) + check | Beta-readiness 4.4 says a quicksave now refuses the other width's take (the x64 round's stage C measurement). If that is so, the cross-width rebuild is unreachable in play and the line is struck as moot; the editor should confirm against DIV-0145's entry before striking. |
| The sweeps: the fishing *READY!* banner | ASK | Question 5: did the owner fish during the clear? |
| The sweeps: the bad ending's *Fin* wipe (area 172) | ASK | Question 6: was the bad ending seen in this clear? The owner's route is the Myria ending. |
| The table faerie (DIV-0151): the dolphin coast scene | ASK | Question 6: did the clear pass the coast scene with the gold faerie on the table? |
| The pillarbox: areas 31 (the lift shaft) and 90; a menu opened there | ASK (stale text) | Area 198 left the framed list on 2026-11-07 ([`narrow-pillarbox.md`](narrow-pillarbox.md) amendment), so the row names one area too many. Question 6: did the owner notice the frame in areas 31 and 90 at 21:9, and open a menu in 90? |
| Garr at the fire: the camps of areas 53 (chapter 6) and 192 | ASK | Question 6: did a camp after 2026-10-21's build show Garr with the flames over his arm and weapon (area 192's camp is the likelier one in a clear continued from chapter 10)? |

### 2026-10-21, docs/own-evidence-fixes

| Item | Verdict | Evidence or question |
|---|---|---|
| DIV-0152, the pad reset (hold Start and Select) | KEEP | A deliberate action nobody takes by accident; the recipe `tools/recipes/field_padreset.txt` is the cheap retire, unrun. |
| DIV-0153, the Holy Mantle in area 83 (D325) | ASK | Question 6: did the owner walk area 83 with the Mantle on, and did fights come rarely? |
| DIV-0154, the extra round (D63) | KEEP | Invisible unless a fight takes a specific shape; the harness's, not play's. |

### 2026-10-20, typed-data stage C; 2026-10-20, answered at the machine

No open item: every line in both sections carries its answer. Nothing proposed.

### 2026-10-18, the install-gap round

| Item | Verdict | Evidence or question |
|---|---|---|
| `Area124_Weights+0x50` (EU, DE): the disc's own or the PC's | KEEP | A decision, and a European disc. |
| The FR / DE music-player labels (a raw-byte label group after a texture capture per install kind) | KEEP | French and German, and a decision; the nine `pending` gap rows of the exe-gap round. |
| D310's format `0x654900..0x654904`: the engine's string or the disc's copy | KEEP | A decision. |
| The two `LeaderPanel` `psx` entries: correct them? | KEEP | Tooling, a decision. |
| D321's corner on a PSP-JP install: the `L` / `R` from the `ja-JP` font | KEEP | A PSP-JP install. |

### 2026-10-17, the button unification round (DIV-0133..0141)

| Item | Verdict | Evidence or question |
|---|---|---|
| Record the minigame starts from placed saves (`chop8`, `well68`, `rope75`, `tower112`, `ram121`, `dock128`, `crane135`) | KEEP (stale text) | Harness recordings. Several exist since (the rope, the valve, the chop, the dock's parts menu - STATUS, DIV-0188 and DIV-0190); the row should name only the ones still wanted (the well's start, the antenna, the ram, the crane). |
| The contact sheets (`worldmap`, `fishing`, `config`, `items`): the PC row | STRIKE | A full clear on the PC install under a Japan-source Config (DIV-0134) saw the world map's legend, the item lists' corner and the Config panel in play; the fishing row is question 5. |
| The contact sheets: the PSX-US, PSP-EU and PC + en-US rows | KEEP | A disc install's eye (question 14), or the sheets regenerated under a new harness root. |
| Confirm on a shoulder (Speak on R1) | KEEP | A specific Config choice; nothing says the owner made it. |
| `Scena19_Run0`'s R1 | KEEP | A decision ("you do not place that section yet"). |
| D314: record more than the defect, or leave it | KEEP | A decision. |

### 2026-10-17, stale installs, the gap census's fixes, the PSP-only title

| Item | Verdict | Evidence or question |
|---|---|---|
| The stale-install prompt in a real start | KEEP (folded) | Beta-readiness 4.3, the upgrade test over a real alpha.5 folder, is this check made properly; it retires the row. The owner has very likely met the prompt across the October builds, but the item wants the box's wording read, which the upgrade test does. |
| The PSP-only title in play (DIV-0131) at the three widths | KEEP | A PSP-only install; ScrubOasis's base is the US disc. |
| On a disc-only install (D310, D311): area 75's counters, area 104's countdown, the faerie board's rows, a level-up's ability name (Western discs), Identify's `?` marks (D278), the play-time colon, the hi-lo payout | KEEP | Disc-built and an eye. ScrubOasis's clear necessarily passed level-ups, the field menu and Identify panels without a report; the counters, the board and hi-lo are optional (question 14). |
| The PSP's copyright line on a PSP-only title | KEEP | A decision, and a PSP-only install. |
| The census's owner items (the event battles' track words first) | KEEP (stale text) | The track half was filled by rule on 2026-10-17 (D316) and every gap row was settled on 2026-10-31; the row should say what of [`install-gap-census.md`](install-gap-census.md) 8 is still open, if anything. |

### 2026-10-14, the ultrawide round

| Item | Verdict | Evidence or question |
|---|---|---|
| The faster slide (DIV-0041 SL): menus, shops, a gene screen in the wide modes | STRIKE | A full clear at 21:9 opened menus, shops and the gene screen throughout. |
| ... and D296, a box sliding in seen whole in the 21:9 band: fine, or fix | STRIKE (answered) | Beta-readiness 2 records the owner's word on D296: leave for now. The question is answered; the defect line stays. |
| The 21:9 left message box: at the picture's edge, or nearer the middle | ASK | Question 12: left-placed message boxes at the 21:9 picture's left edge - fine where they are? (Seen through the clear; the item wants a choice, not a look.) |
| The terrain margin at 21:9: corner popping as the camera turns | STRIKE | A full clear at 21:9 in every field; the owner reports edge faults when seen. |
| Census 3's band sheets (parked and still rows in scenes) | STRIKE | The policy was "flag only what looks wrong"; the scenes themselves were played at 21:9 and nothing was flagged. |
| Two `dolphinBoss` plant frames: plants in front of a character drawn in front again | STRIKE | DIV-0071 on by default through the clear; the owner found and reported its wrongs (#132, #141), none about plants. |
| The gene crystal (D242): CF's rule if the cut ever bothers you | KEEP | Parked; a standing offer, not an owed look. |
| The patch cells (D295): leave for now | KEEP | Parked by the owner's word; beta-readiness 2 lists it. |
| The six other effects the GTE fix changes (DIV-0126, kinds 0x1E .. 0xA0): look when one is reached | KEEP | A clear may well have crossed several, but which kinds were cast cannot be told from play; a recipe per kind, or the ledger's own measurement, retires it. |
| Effect kind 0x51's grab (areas 34 and 108 at given chapter counters) | KEEP | A specific scene at a specific chapter counter; a chapter-jump save retires it. |

### 2026-10-13, the away day

The "Decided by the owner, to build" sub-section is all decided and built or recorded; nothing proposed there.

| Item | Verdict | Evidence or question |
|---|---|---|
| Eye batch q1 (D256, the PSP title's seam and second ™) | KEEP | A PSP-only base. |
| Eye batch q2 (the PSP logo movie's wordmark; D287 the launcher's two clipped lines) | KEEP | A PSP source for the movie; D287 is a launcher fix decision (fine to fix?). |
| Eye batch q3 (the wide picture, anything besides D284 / D285) | STRIKE | A full clear at 21:9. |
| Eye batch q5 (the fishing ticker's style: disc, PC or PSP) | KEEP | A decision. |
| Eye batch q7 (D255) | KEEP | See "To decide" below. |
| The arena sheets (member 2's nudges, member 1's added part, Ryu's byte) | STRIKE (answered) | The three decisions they fed were given on 2026-10-15 (take the disc's; take the disc's; the split). The look is spent. |
| The D56 capture (DIV-0116, before and after): right? | KEEP | A capture's judgement, not play; nothing in play shows it (no battle route placed an enemy off the left). |
| The labels sheet: `WEAK POINT`, `ITEM` / `OBJET` / `OBJEKT`, `EP` on Identify (DIV-0117) | KEEP | The owner's Identify panel under the row was the Japanese layer's; English placement is ScrubOasis's silent evidence (question 14), French the third player's in progress, German nobody's. |
| The Japanese world map (D281): anything to add? | KEEP | A PSX-JP install, and a twin read; recorded as the owner's hypothesis. |
| Area 65's canopy; `bossAndFlash`'s three boss-effect frames | KEEP | Harness captures, gone with their root; a re-run makes them. |
| Listen a: the area's song resuming after a random fight (DIV-0092) | STRIKE | Two clears with sound on, every random fight's end; D241 (the fight's music running on) was the owner's own report and is fixed; "the combat loop sounds right". |
| Listen b: the loop rows `064`, `076`, `037` with DIV-0081's table | ASK | Question 2: is the owner's `game\` music the PC's MP3s (the loop table in play) or a disc's? If the MP3s, a clear heard the long songs loop many times: STRIKE. If a disc's, KEEP for a PC-music play. |
| Listen c: the inn jingle on a PSX-only install (D255) | KEEP | A PSX-only install; ScrubOasis's mixed install may take its jingle from the PSP source. |
| To decide: the hi-lo count - a route first, and one to fight 26 / Beyd's duel | KEEP | Recordings, then a decision. |
| To decide: resume or restart after a random encounter, in mednafen from the `cutsceneAndNue` save | KEEP | A PlayStation listen in the emulator, not a play of ours. |

### To listen to (the music investigation, 2026-10-06)

| Item | Verdict | Evidence or question |
|---|---|---|
| `153_loop_fixed.wav`, then the game with `BOF3X_MUSIC_LOOPS` on against `=0` | ASK | Question 2 (the music source). The "on" half of a game listen is a whole clear if the MP3s play; the `=0` comparison is a deliberate A/B and stays a harness item either way. |
| The town theme `000`'s short file: the rewind, a stretched loop, or outside samples | KEEP | A decision. |
| DIV-0081's second cut: `064` / `076` whole before the join, one new row (`037`, `084`, `109`, `130`) | ASK | Question 2, as above. |

### To review in play

**The away day of 2026-10-27 (PR #158):**

| Item | Verdict | Evidence or question |
|---|---|---|
| Area 9 on a PSP-built install | KEEP | A PSP-built exe image. Whether ScrubOasis's mixed install built its image from the PSP disc is not known here; a PSP-only install retires it cleanly. |
| The fishing spots 89 and 129 on a US-only install | KEEP | A US-only install **and** a visit to the sea or river spot; a recording of the walk there. |
| The fishing headings in `game\` (EQUIP, LURE, GUIDE centred) | ASK | Question 5 (fishing). The launcher's upgrade since 2026-10-27 brings the overlay current, so the answer is expected to be "centred". |
| The sweep by enemy kind (`kind_sheet.png`) | KEEP | A sheet to look at, in the harness root. |
| To record: seven boss-only skills (Shroom, Myria, Gaist, Ammonite, the Chimera, the Gazer) | KEEP | Harness recordings; the owner's clear fought the bosses but a recording is what the sweep needs. |

**State 3, `bof3x.exe` (PR #53): the icon (DIV-0095) and the window and its DPI scale (DIV-0094)**

| Item | Verdict | Evidence or question |
|---|---|---|
| The icon and the window, "never looked at for themselves" | STRIKE | Every start of the owner's play since 2026-10-12 has been `bof3x.exe` through the launcher from `game\`; the icon was built to the owner's own call (the whelp portrait, 2026-10-11) and the window has framed two months of play and a clear. |

**The disc-only round (2026-10-12):**

| Item | Verdict | Evidence or question |
|---|---|---|
| A key press in `bof3x.exe` (the keyboard dead in the exe host at state 3) | STRIKE | The owner's quicksaves (F6 / F7), F4 and the pause are keyboard; beta-readiness 1 says the quicksaves were played through. The keyboard works in the exe host. |
| The disc-only install itself: title, field, fight, menu, save; "what you see decides the first item under Decisions" | STRIKE (ScrubOasis) | A disc-built install played to the end without a blocker; the decision it fed (the Capcom values no disc carries) was settled row by row on 2026-10-31 by the exe-gap round. The one caveat: ScrubOasis's install is US + PSP-EU, not the US disc alone. |
| DIV-0096, the eleven arenas as the disc's: member 2's field sprites, member 1's battle sprite | KEEP | An eye on a disc-built install (question 14): a missing part in one animation is not a blocker and ScrubOasis's silence is not a look. |
| DIV-0097, the disc's own images: the four area pages, the faerie village sheet; the FR / DE sheets | KEEP | US pages: question 14. FR / DE: French is the third player's, German nobody's. |
| DIV-0099, the Capcom logo from the disc's XA through the converted AVI | KEEP | A disc-built install's start. Beta-readiness 4.2 (one start of the 32-bit zip) covers the PC install's movies through DIV-0205, not the disc-converted file; ScrubOasis's install starts with it every launch (question 14). |

**The PSP-only round (2026-10-12):** the two "Answered" blocks are closed; the rest:

| Item | Verdict | Evidence or question |
|---|---|---|
| The eye: the PSP stand-ins as a PSP-only base (tiles, UI page, dial and title pages, Ryu's forms), the converted PSP logo movie, the launcher's suites page, the Jukebox's PSP column and chord box | KEEP | A PSP source. Question 1 says whether the owner's `game\` holds one at all; if not, a PSP-only install is the retire. |
| The ear: the PSP's music, the ATRAC3plus effects and voices, the re-encoded jingles, the chord one tone against two | KEEP | A PSP source, as above. |
| Play: the Beyd-Zig duel lost under `duel-loss` on a PSP-only install | KEEP | A PSP-only install and a deliberate loss. |
| Play: Myria with `myria-item` on and off (a steal, or the drop) | KEEP | A PSP base and a specific action. |
| Play: a battle with magic (the filled `Magic_Rows`) | STRIKE (ScrubOasis) | A disc-built clear casts magic from the first chapter to the last; a wrong effect file on a spell is a blatant fault no clear passes without a report. |
| Play: a level-up of roster index 6 (the PSP's table, DIV-0104 / DIV-0111) | KEEP | A PSP base. |
| Still open: D56's repair | KEEP | The same as the away day's D56 capture row above. |
| Still open: D255 the jingles on a PSX-only install | KEEP | A decision and a PSX-only install. |
| Still open: D256 the PSP title's framing | KEEP | A PSP-only install. |
| Still open: the field menu's time and money boxes in wide mode, "you check on the PSP" | KEEP | A check against the PSP by the owner, then a decision. |
| Still open: the PSP-only mode (CD2 / CD3) | KEEP | A decision. |

**The numbered items 0..4:**

| Item | Verdict | Evidence or question |
|---|---|---|
| 0. The game from the cache (DIV-0089): `BOF3X_CACHE` with nothing in `DAT\`, against `BOF3X_CACHE_DATA=0`; the one-root call | KEEP | A specific configuration and an A/B, then a decision. If the owner's `game\` is itself a Sources-page build reading the cache, the differences half is the daily path and moot; the one-root call stays. |
| 1. Music coming back after a pause (`Sound_ResumeAll`): F9 or Alt-Tab during a faded silence | ASK | Question 11: did the owner pause (F9) or Alt-Tab away while the music was faded to silence, and did the faded song come back? |
| 2. The layering fix in play (DIV-0071) | STRIKE | The whole clear with the default on; its wrongs were found and fixed through the owner's own reports (#132, #141). |
| 3. TILE_1 drawn as 2x2 (DIV-0077): the dream scene after the whelp fight, or any Kaiser cast | ASK | Question 3 (a new-game start reaches the dream scene) or a Kaiser cast in the clear. |
| 4. Lines as quads (DIV-0079): the fishing gauge and grey lines; a magic trail | ASK (fishing) + STRIKE (trails) | Magic trails: a clear casts magic throughout, the quads on by default. The gauge: question 5. |

### Reported by the owner, for a cleanup session

| Item | Verdict | Evidence or question |
|---|---|---|
| Battle music runs on after a fight, the area's music does not resume (D241; fixed 2026-10-10, the owner's ear owed) | STRIKE | Two clears with sound on; "the combat loop sounds right" (2026-11-07). |
| The sort screens, formation screen and Skill Notes: the French and German builds' words | KEEP | French and German as main languages; the third player's French in progress is the nearest evidence. |
| Unrendered squares at the wide view's edges: the canyon bridge and the attract sequence's map | STRIKE | The wide inset lowered since 2026-10-07; a clear at 21:9 walked the wide picture's extra columns everywhere, and the attract map runs at every title wait. |
| The Volt's EXP bonus (the owner's fight of 2026-10-06) | ASK (stale text) | The row still says "staged for tomorrow"; it was built on 2026-10-07 as DIV-0082 (the AI rows' done bits cleared at spawn). Question 13: did a Volt triple its EXP after an electric hit in this clear? If not, the row becomes a one-line pointer at DIV-0082 with the eye still owed. |
| The Skill Notes prompt's hand a word short of `Yes` | KEEP | A work item: no fix found in the ledger (DIV-0027's list does not name the Note tab's prompt). Not retirable by play; a cleanup session, then the eye. |
| The master list: the star beside a completed master (DIV-0064 / DIV-0059, 2026-10-07; "no committed save has a completed master") | ASK | Question 10: does the clear save's master list show a star beside a completed master? |
| Nina through a crate (DIV-0071's rule, fixed 2026-10-07) | STRIKE | The layering default through the whole clear (item 2 above). |
| The bridge scene's parts (the inset, Garr behind the railing, the sky, the "waterfall") | STRIKE (answered) | Every part carries its answer in the row ("Sky looks perfect", the deck edges walked 2026-10-08, the railing fine as it is). |

### Decisions the measurements raised

All decisions; none is retired by play. Grouped, with what has moved:

| Item | Verdict | Evidence or question |
|---|---|---|
| The faerie village's talk line under a Western layer on the PC install (carry the operand?) | KEEP | A decision. |
| The Capcom game values no disc carries | STRIKE (answered) | Settled row by row on 2026-10-31 by the exe-gap round, as the row's own last sentence says. |
| The faerie village's label group writing names one record late (fix it?) | KEEP | A decision; "look at the village's member list first" is a disc-built eye (question 14). |
| The porting house's logo movie | STRIKE (answered) | Answered 2026-11-02: dropped, DIV-0202. |
| Six doubtful `.bss` pointer words; DIV-0055's 94 plate bytes | KEEP | Listed so they are not lost; tooling. |
| 34 French and 11 German messages the PC build leaves Chinese | KEEP | A decision, and French / German. |
| What the engine holds as layout for a disc-only build | KEEP | A decision, not blocking. |
| minimp3 replaces Capcom's decoder: "nothing to hear until built, then the PCM bound and the loop seams" | ASK (stale text) | The decoder is built (DIV-0091 / DIV-0093 in the defects list). The seams are question 2's (the music source). |
| `symbols.toml`'s quoted values: the history rewrite; the same runs in `docs/` unscanned | KEEP | Decisions (the rewrite recommended against; the docs scan is work). |
| Step 3's four calls (the disc-only dial page; the FR / DE enemy names over 8 bytes; the 14 port-edited arenas; `AREA004` from a Western disc) | KEEP (stale text) | Decisions. The editor should check which have since been overtaken (the dial page by DIV-0135's glyph legend; the name width by the glyph-room rule of issue 127; the arenas by DIV-0096 and the 2026-10-15 arena decisions) and reword the row to what is left. |
| Step 8's four calls (`exe-pointers.tsv` in the repo; blanking the six name tables; a PSP-only `base/exe/`; the data pointers from a disc) | KEEP | Decisions. |
| The disc's music by default: the listening session that gates step 9; the 11 jingles' rule | ASK + KEEP | The listen is question 2's; the jingles' rule is a decision (D255's family). |
| Step 6's call (3): the 11 jingles for a disc-only player | KEEP | A decision (the same D255 family). |
| Step 4's "to look at once built": Stallion with `psp-art` on and off, ability 116's banner under `psp-names-en-150`, `AREA128` wide with `psp-maps`, a few `psp-tiles` areas | KEEP | A PSP source (question 1 for the owner's install; otherwise a PSP-only install or ScrubOasis, question 14). |
| The world map's gauge words in French and German (a render first) | KEEP | French / German, and a render to look at before a yes or no. |
| The `area4-walls` layer: the minecart area's raised strip and corridor edge, blocked with the layer | ASK | Question 1 (a Western disc installed in `game\`) and question 3 (whether the clear passed area 4 under the layer). Without a Western disc the row is KEEP for a disc install. |
| Stallion's PSP recolour as an option (the renders in `analysis/stallion/`) | KEEP | A decision, and a PSP source. |

### Seen by the owner (struck)

Nothing open.

### Debt 3: measured by a log line, review the log

| Item | Verdict | Evidence or question |
|---|---|---|
| Grep `bof3x.log` for `debt3` after a play session | KEEP (cheap) | Not the owner's to play: an agent reads `game\bof3x.log` after the clear at the owner's word. A hit becomes a decision; no hit closes it for this clear. |

### Parked, documented, revisit if ever seen

| Item | Verdict | Evidence or question |
|---|---|---|
| Area 24's descriptor init word; the `0x91` camp cells; the one-texel POLY_FT3 colour | KEEP | Parked by the owner's word; nothing owed. |

### Decided and built, not yet seen live

| Item | Verdict | Evidence or question |
|---|---|---|
| The launcher's Music, Cache folder and PSP extras boxes at the owner's DPI; Browse... for a cache folder; the PSP boxes with PSP layers installed | STRIKE (layout) + KEEP (PSP boxes) | The owner has used the launcher daily for a month and found the Sources page right (2026-11-07); the dialog's layout at their DPI is seen. The PSP boxes want PSP layers (question 1). |
| `Cfg_Load`'s key-line overrun (DIV-0078): 25 hand-added key lines | KEEP | A hand-edited `BOF3.CFG`; the `shell` self-test covers it headless. |

## 2. Summary

### Counts

Counting each table row once, a split row ("STRIKE + KEEP", "ASK + KEEP") by its first verdict, and the rows that
only record an answer already given ("STRIKE (answered)", "STRIKE (moot)") as strikes:

| Verdict | Rows |
|---|---|
| STRIKE | 40 |
| KEEP | 82 |
| ASK | 29 |

(The reading-row "Still open" bullets, the parked trio and the "Decisions" step-call bundles are each one row here;
unbundled, KEEP is larger still. The counts describe this sheet, not the file's line count.)

### The questions for the owner

Answer each in a line; the sheet's ASK rows resolve from them.

1. **Sources:** what does `game\`'s Sources page hold - the PC install alone, or also a PlayStation or PSP disc?
   (Gates the `area4-walls` eye, the PSP suites and Jukebox column, DIV-0192's loss tune.)
2. **Music source:** does `game\` play the PC's MP3s (DIV-0081's loop table) or a disc's music? (Gates the loop-join
   listens, minimp3's seams, the town theme's rewind, the step-9 listening session.)
3. **The clear's start:** did it begin as a new game under the row, or continue the game that stood at chapter 10
   on 2026-10-30? (Gates the opening with stops, chapter 1's silent sound, the dream scene's 2x2 motes, the
   bedroom song through the dream, area 4 under the walls layer.)
4. **The row's mode:** Japanese text with furigana readings, or a second language over Japanese (or Japanese over
   English)? And did you press F4?
5. **Fishing:** did you fish during the clear - the *READY!* banner at 21:9, the gauge's lines, EQUIP / LURE / GUIDE
   centred in their bars?
6. **Scenes:** which of these did the clear pass, with nothing wrong seen: the dolphin coast scene (the gold faerie
   on the table); the framed areas 31 and 90 at 21:9, a menu opened in 90; a camp with Garr at the fire (areas 53
   or 192); area 83 with the Holy Mantle on; a song restarting from its beginning mid-area (areas 142 or 174); the
   bad ending's *Fin* wipe (area 172)?
7. **Headphones:** was any of the play on headphones or stereo speakers, and did a hit or spell on the left pan left?
8. **Dragon fights:** were the grown command icons each the right picture, none cut?
9. **The Items sort:** did you sort the clear's bag (DIV-0189)? And yes or no to a hand-written type order.
10. **The master list:** does the clear save show a star beside a completed master?
11. **A pause during silence:** did you ever pause (F9) or Alt-Tab while the music was faded out, and did the faded
    song come back on resume?
12. **Left message boxes at 21:9:** fine at the picture's left edge, or nearer the middle?
13. **Volts:** did a Volt triple its EXP after an electric hit in this clear (DIV-0082)?
14. **ScrubOasis's clear:** does a disc-built clear with no report retire the disc-install "look" items (DIV-0096's
    arenas, DIV-0097's US pages, DIV-0099's movie, DIV-0195's boxes, D310 / D311's panels and digits, DIV-0117's
    labels, the village member list), or does each want your own eye or a word from them?

### The KEEP list, grouped by what retires it

This is the shape of the notes' "Watched, not yet played". Each group names the install or condition; the items are
the rows above.

- **A German disc or a rebuilt `de-DE` layer (German as a main language):** DIV-0203's wording; the sort screens'
  words; the FR / DE music-player labels (the nine pending gap rows); the 34 FR / 11 DE Chinese messages; the world
  map's gauge words; DIV-0097's DE sheet; the Identify labels (DIV-0117) in German; `Area124_Weights` (EU / DE).
  The French half of each is the third player's, in progress.
- **A European PlayStation disc as the base:** the PAL banks (DIV-0194); `Area124_Weights`. The third player's
  French disc will be the evidence.
- **A PSP-only install, or a PSP source in the owner's install (question 1):** the PSP-only title (DIV-0131, D256,
  q1) and its copyright line; the PSP logo movie's wordmark (q2); area 9 on a PSP-built image; the Beyd-Zig duel
  lost; Myria's item toggle; roster index 6's level-up; the PSP stand-ins, suites page, Jukebox column and chord
  box; the PSP's music, effects and jingles; Stallion under `psp-art`, ability 116's banner, `AREA128` under
  `psp-maps`, the `psp-tiles` areas; the launcher's PSP boxes; the field menu's boxes "checked on the PSP"; the
  PSP-only mode decision.
- **A PSX-JP install:** D281's world map; D321's corner (PSP-JP).
- **A Western PSX-only or disc-built install with an eye on it (question 14, or the owner's own):** DIV-0192's
  loss tune (and a lost fight); DIV-0195's boxes; D310 / D311's counters, countdown, board, ability names, `?`
  marks, colon and hi-lo payout; DIV-0096's arenas; DIV-0097's US pages; DIV-0099's converted movie; D255's jingle
  (PSX-only); the fishing spots 89 and 129 (US-only, and a visit); the contact sheets' disc rows; the village
  member list (group 16).
- **The three non-Recommended presets:** a stretch of play under each.
- **D406:** a quick-load during a battle voice, after DIV-0145's amendment.
- **A specific action or configuration nobody has taken:** DIV-0152's Start + Select; DIV-0154's extra round;
  confirm on a shoulder; DIV-0089's cache configuration and one-root call; `Cfg_Load`'s 25 key lines; one Sources
  import from the 64-bit launcher; the release folder's loud furigana error; the stale-install prompt (folded into
  beta-readiness 4.3).
- **A recording, a harness run or a sheet:** the minigame starts still unrecorded; the well's start; the seven
  boss-only skills; the hi-lo and Beyd's-duel routes; the mednafen resume listen; area 65 and `bossAndFlash`'s
  frames; the kind sheet; the D56 capture; DIV-0126's six kinds; kind 0x51's grab; D326; the `debt3` grep (cheap:
  an agent does it at the owner's word).
- **A decision rather than play:** D350; D344; D335; D242; D295; D310's format; the `LeaderPanel` entries;
  `Scena19_Run0`'s R1; D314; the census's remaining owner items; the fishing ticker's style (q5); D287; the orig4
  re-record word; DIV-0189's type order; the reading-row round's release gate, ML's questions, the audit's (b) and
  (d), the lexicon and fugashi follow-ons, the arrow at a stop, the balance rule, D328's proofreading; the faerie
  village's talk-line operand and label group 16; the `.bss` words and plate bytes; the engine's disc-only layout;
  the `symbols.toml` history and the docs scan; step 3's, step 8's, step 6's remaining calls; the jingles' rule;
  the town theme's loop; Stallion's option; the Skill Notes prompt (a fix first); the parked trio.

### What beta-readiness section 1 does not list

Section 1's remainder is: a PSP-only install, a PSX-JP install, German and Japanese as main languages, the three
other presets, the 32-bit zip's movies, the upgrade path, and (section 2) D406 and the open D-numbers. The KEEP
groups above add these, which the notes' "Watched, not yet played" should name or the owner should knowingly leave
out:

- **A European PlayStation disc as the base** (the PAL banks, DIV-0194): section 1 treats "disc-only" as retired
  by the US + PSP-EU clear and the French play in progress; the European banks are reached only by the European
  base, and the French play is not finished.
- **The Western PSX-only "look" items** (DIV-0192's loss tune, DIV-0195's regional boxes, DIV-0096's arenas,
  DIV-0097's pages, D310 / D311's panels): section 1 says the disc-only installs were "played to the end" on the
  mixed install, which is true of blockers and not of these. Question 14 decides whether they are a line in the
  notes or struck on ScrubOasis's word.
- **The disc-built install's converted Capcom movie** (DIV-0099): section 1's movie line (DIV-0205, the 32-bit zip)
  is the PC install's path; the disc-built path has only ScrubOasis's silent starts.
- **The music source** (question 2): section 1's "the sound changes heard through two full plays" is right for the
  sound layer (DIV-0169..0175, DIV-0201), but the loop table (DIV-0081) and the MP3 decoder's seams (DIV-0091 /
  DIV-0093) are heard only if the owner's install plays the MP3s; a disc-music install never reaches them, and the
  reverse.
- **DIV-0171's pan** (question 7): a headphone listen that two plays on speakers may not have given.
- **D343 the other way round:** section 2 lists D343 as open; if the inn's-night strike above stands (the owner's
  2026-11-07 word plus the clear), its heard half is closed and section 2's line should say so, leaving whatever
  of D343 was not DIV-0169's.
- **The row's mode** (question 4): section 1 says the owner's whole play was under the row; the notes should say
  which of the two modes (furigana readings, or a second language) that was, since the other is then "checked by
  captures, not played".

Everything else the KEEP groups hold is either a decision, a recording, or already in section 1's remainder.
