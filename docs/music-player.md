# The launcher's Jukebox: the game's music from each source

**Status:** BUILT (2026-10-11, `launcher/music-player`, merged as PR #52) and
**in the owner's regular use**: the owner, 2026-10-12 night - "I've been using
the jukebox mode a lot, it's great"; they catalogued every song against the
soundtrack box set's official titles (section 4, `data/music-names.tsv`). The faerie-menu and PSF measurements in
section 4 are STABLE; the names themselves are the owner's catalogue.
**2026-10-12 (the PSP-only round):** the PSP source is lit - from a PSP-only
cache's `base\bgm` (DIV-0105), or beside a PlayStation disc's songs from the
`psp-music` layer, each song on its own directory's banks (DIV-0109) - and the
screen has a box for the battle chord as the PSP plays it (DIV-0113);
[`launcher-suites.md`](launcher-suites.md) 9.3 and 9.6.

The owner asked for a music player reachable from the launcher, playing the
songs from whichever of the PSX disc, the PSP disc and the PC port the install
has, listed by disc name now and by natural names later (the owner's
cataloguing job). Songs only, no effects.

## 1. What it is

`Jukebox...` on the settings dialog opens `IDD_MUSIC`
(`src/launcher/music_player.cpp`):

- **Source**: three radio buttons. A source the install lacks is greyed, with the reason
  under it (the Config screen's rule: everything listed, greyed by source).
- **Song**: a drop-down of every song by its number. Each entry shows the disc's own name for
  it (`BGM019.EMI sub 1` for the PSX, `BGM\019.DAT` for the PC), and a name when the catalogue has one.
- **Play / Stop / Close**. Changing the song or the source while playing switches at once.

The owner's calls of 2026-10-11:

- **Songs loop until stopped.** A song the game plays once plays once here, and the status says so.
- **The duplicates are dropped.** The list is by song number, so each song appears once. The 38
  battle, boss and demo bundles are whole copies of seven `BGM` files
  ([`seq-import.md`](seq-import.md)) and never appear.
- **The PSP's volume merge is played as the PSP has it**, when its import exists (section 3).

This is a launcher feature: it changes nothing the game does, so it has no
DIVERGENCE entry.

## 2. The sources

| Source | Where the songs are | What plays them |
|---|---|---|
| PlayStation disc | the cache's `base\bgm\NNN.DAT` and `bank\` ([`seq-format.md`](seq-format.md)): `BOF3X_CACHE`, else the Cache folder box as it stands | `psx::MusicSynth`, started as `music_seq.cpp`'s `Begin` starts a song (`Play(song, 1, 1)`, then `SetVolume(127, 127)`); the loop is in the data |
| PSP disc | not imported | greyed (section 3) |
| PC port | the game folder's `BGM\NNN.DAT` / `NNNN.DAT` | minimp3, decoded whole. Looping files loop at DIV-0081's measured points when `music_loops_table.inc` has a row for the file (same track, same size), with the same crossfade as `LoopDecode`; otherwise they rewind to the file's start, as the port does. `N` files play once |

Notes on the implementation:

- **Output is `waveOut`** (winmm): four 2,048-frame buffers at 44.1 kHz stereo, refilled by a
  thread of ours. The launcher's SDL is built without its audio subsystem, and that SDL build is
  the DLL's too, so turning SDL audio on would change the game's binary for a launcher feature.
- **A malformed song or bank** ends the launcher with a message naming the file (CLAUDE.md rule 4).
  This is the engine's `MusicFatal`, through a launcher hook.
- **The PC has file 166 and lacks 021.** The PSX lists 000..165.

## 3. The PSP: what is scaffolded

`ListPsp` greys the source with the reason. The PSP's songs (`pPMS`, a table of
SMFs) and its banks (`PPHD` / `pBVC`) need the importer and a PSP-native bank
format ([`psp-only-build.md`](psp-only-build.md) section 2.1, step 5). When
they exist, `ListPsp` lists the cache's PSP songs and `Play` gets a third
`Stream`. That stream is `psx::MusicSynth` over a bank whose tone volumes and
pans are already merged with their programs', as the PSP has them (the owner's
call, 2026-10-11). Until then, a PSP `Play` cannot be reached, and it aborts if it ever is.

## 4. Names

**The catalogue is [`data/music-names.tsv`](../data/music-names.tsv).** The build copies it beside the
launcher, and the player reads that copy. Edit the repo's file: every build refreshes the copy.

- **Format:** UTF-8, tab-separated, `#` lines are comments.
- **Columns:** `song name box_disc box_track box_title source note`.
  - The Jukebox shows `box_title` where it is set, else `name`, with the spaces at either end trimmed.
  - A row whose `source` is `same-notes` is **not listed** (section 4.3).
  - A line whose first field is not a three-digit song number is skipped.
- **Committing it is the owner's call of 2026-10-11.** The box's track titles are public, and the
  file holds only our mapping of titles to song numbers, no audio.
- **Filled in by the owner on 2026-10-12.** The titles come from the collector's box, the *Breath of
  Fire Original Soundtrack Special Box*, discs 5–7. 133 rows have a `box_title`.
- **`source`**, as the owner set it:

  | `source` | Meaning | Rows |
  |---|---|--:|
  | `ossb` | the box's own track | 57 |
  | `game` | the faerie village's music menu, US disc (section 4.1); these rows carry box titles too | 41 |
  | `kk` | the owner's name from listening, mostly sound effects | 30 |
  | `psf` | the zophar rip, unsourced (section 4.2) | 4 |
  | `same-notes` | hidden | 35 |

- **`note`** holds the measurements and the clashes. The ripper's own comments are not carried over.

The first names came from two measured sources.

### 4.1 The faerie village's music menu (the game's own titles)

The community's music building lists up to 40 songs by title.

**The PC.** `CommuMusic_DrawList` `0x45FA80` draws the selected row's title. Read by capstone:

- `movsx eax,[0x675FD2]` reads the row.
- `mov dx,[eax*2+0x803A62]` reads the slot. That address is `MessagePools` `0x803580` + 2 × `0x271`,
  so the title is area text slot **`0x271 + row`**: slots `0x271..0x298` of the `AREA175..185` text
  block. The block is byte-identical across all 11 areas.
- `CommuMusic_Play` `0x45F950` passes the row's byte from a **40-byte table** to `Music_Play(song, 8)`.
  The table is at `BOF3.exe` `0x653098` (`.data`, file offset `0x253098`).

**The discs.** The same table is in `COMMU03.EMI` section 6 (dest `0x801D0C00`), at a different
offset per build:

| Build | Offset |
|---|---|
| JP, both PSPs | `+0xD00` |
| US | `+0xD18` |
| DE, EU | `+0xD38` |
| FR | `+0xD68` |

The PSPs also hold a copy in `BOOT.BIN`. **All eight builds hold the same 40 bytes.** The titles
are in `AREA175.EMI`'s text section (dest `0x80010000`), slots `0x271..0x298`, in each build's language.

**The fan wiki's numbers are menu rows, not song numbers.** Its list (Fandom, "Faerie Village") is
numbered 1..40 in menu order, and its names are the PSP-EU's text.

**For an importer later**, all from the player's own disc or exe, not copied into the repo:

- The 40 bytes go in `base/`. The repo holds only their hash.
- The titles go in `loc/<tag>/`, per language.
- The PC's Chinese titles are glyph indices into the port's font, with no table to Unicode.

### 4.2 The PSF rip (`Breath of Fire III (EMU).zophar.zip`)

**What the rip is.** 101 full `.psf` files: no minipsfs and no `_lib`. Each is a whole-RAM
snapshot of the US game (`SLUS_004.22`).

- One complete `BGM*.EMI` is loaded at `0x80010000`.
- A driver ("psf driver v1.0") sits at `0x80080000`.
- A parameter block at `0x80090000` holds: byte 0 the sub-song, byte 1 the sub count,
  byte 2 play-once, bytes 3–4 the volume.

**How each file was mapped**, by measurement and independently of the titles:

1. The EMI is identified by an exact byte match against the disc's 119 sequence-bearing EMIs.
2. The sub-song is byte 0. Two rips of one EMI differ in that byte alone.
3. (file, sub) gives the song number through the PSX song table.
4. Every song number was checked against the cache's song headers: 0 mismatches.

**The result:** 100 of 101 files give exactly one song.

- "Fight!" matches songs 151 and 153, which are identical files.
- The fanfare is song 165, the battle bundles' sub 1.
- 31 more songs play the same events as a ripped one under another bank.
- 29 songs have no tune match. Most are short or placeholder sequences.

**The tags** name the composers (Akari Kaida, Yoshino Aoki), copyright Capcom, ripped by Neill
Corlett; some comments are signed "YK". There is no licence text.

**Where the two sources disagree:**

- Song 9 is "30" in the game's menu, but the PSF titles it "Neverending Game"; the rip's own
  comment says it is really "30".
- The game's "Neverending Game" is song 125, which has no PSF.
- Songs 74 and 51 share their events under different banks; the game's menu names 51.

### 4.3 Hidden songs

The owner marks a row `same-notes` when it is the same song as another, or too like one to be worth
listing (2026-10-12). The Jukebox drops those songs from both sources' lists, which leaves 129 PSX
songs and 130 PC songs. Each is still a song the game plays; the mark only keeps it out of the list.

Three box tracks are still shared by more than one listed song. They are left as they are; each is
the owner's to hide or keep:

| Box track | Songs |
|---|---|
| 5-13 | 086, 166 |
| 5-27 | 009, 125 |
| 6-13 | 102, 106 |

The owner hid 153 (the disc's file for it is identical to 151's) and 090 (056's notes on other
channels) on 2026-10-12.

## 5. Checks

**Scripted, 2026-10-11.** The launcher was started with `--config`, and messages were posted to
the dialogs, looking windows up by the launcher's own process ID.

- **Sources:** PSX enabled with 166 songs, PC enabled with 166, PSP greyed with its reason.
- **Statuses on Play:**
  - PSX 001: looping.
  - PSX 009: once.
  - PC 001: looping at its measured points.
  - PC 004N: once.
- **Stop and Close:** both work; the launcher exited 0.
- **Names:** loaded from the catalogue, and a `box_title` shown over its row's `name` (song 51, a test row).
- **Hidden rows (2026-10-12):** with the owner's catalogue, 131 PSX and 132 PC songs listed; the first, last and three middle entries carry box titles; play, stop and close as before.

**Owed:**

- **The owner's listen:** each source, a loop seam (PC 001, PSX 001), and a song that plays once.
- Pad navigation in the Jukebox dialog. The settings dialogs' `padnav` is not attached here.
