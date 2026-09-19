# User checks — things only the owner can do in game

**Status:** IN PROGRESS (2026-09-19)

Checks that need a person at the keyboard playing the game. Agents add items
here instead of burying them in [`HANDOFF.md`](HANDOFF.md); the owner ticks
them and reports what they saw, and the result is then written into the doc
named on each item. **Rewrite, do not append:** a finished item is deleted once
its result is recorded where it belongs.

Launch for all of these: `build/bof3x-launcher.exe --game bof3`; log in
`build/bof3x.log`.

## Open

### 0. Load the converted PlayStation saves — do this one first

*Why:* [`save-interchange.md`](save-interchange.md) — the converter is
self-consistent but nothing it made has been loaded by a game.

- [ ] Load screen: slot **2** should be the JP save (12:48, Lv 11, party of
      three), slot **3** the US save (42:37, Lv 38, solo adult Ryu). Do the
      summaries show, with sane names, times and levels?
- [ ] Load each. Right place (US: the area the sibling's import landed in)?
      Party, inventory, equipment, abilities and zenny intact? Character names
      readable in the status menu (they are borrowed from PC slot 0)?
- [ ] Walk around, open menus, fight one battle, save to a new slot.
- [ ] Note the confirm/cancel buttons after loading the US save — the sibling
      saw US button config ride along in the save block.
- Anything odd, however small, is the finding. Result goes to:
  [`save-interchange.md`](save-interchange.md) §4.

### 1. Save and load through the fully-ours file layer

*Why:* the attract check covers open/read/size/close. `File_Write` and
`File_Seek` are only reached by saving and loading.

- [ ] Save to any slot, then load that save.
- [ ] Expect in `build/bof3x.log`: `first call File_Write(handle=…, size=4784)`
      and `first call File_Seek(…)`; the `BISLPS0?.DAT` on disk is 4,784 bytes;
      the save loads back correctly.
- Result goes to: [`asset-loading-path.md`](asset-loading-path.md) §1.

### 2. DIV-0003 — a save that cannot be written

*Why:* the fix was made by reading the code; the failing case has never been
run, original or ours ([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0003).

- [ ] Copy a `BISLPS0?.DAT` somewhere safe, then set its read-only attribute
      (Properties → Read-only). A read-only *directory* attribute does not make
      `fopen "wb"` fail on Windows; a read-only file does.
- [ ] Ours: save over that slot. Expect no crash and
      `openw … FAILED  BISLPS0?.DAT` in the log. **Note what the menu shows** —
      does it claim the save succeeded?
- [ ] Original: same again launched with `BOF3X_ORIGINAL=File_OpenWrite` set.
      By reading, this should crash.
- [ ] Clear the read-only attribute afterwards.
- Result goes to: DIV-0003's "not yet verified" sentences.

### 3. DIV-0002 — reproduce the vanishing save with all-original code

*Why:* the defect was first seen with `File_Read` ours; the clean A/B is owed
([`save-files.md`](save-files.md) §3).

- [ ] Launch with `BOF3X_ORIGINAL=*`, reach gameplay **without** visiting the
      load screen, save to a slot that has no file, reopen the save menu.
      Expect the slot to show empty.
- [ ] Corollary, same session: save over a slot that *did* have a file at
      launch. Expect it to display correctly.
- Result goes to: DIV-0002 and [`save-files.md`](save-files.md) §3.

### 4. Does the PC port ever ask you to name Ryu?

*Why:* on the PSX, New Game prompts for Ryu's name before the Dauna mines
scene; on the PC it does not (owner, 2026-09-19). The port may have dropped
naming, or moved it — the meeting with Rei and Teepo is the natural place.
Decides how much name conversion matters
([`save-interchange.md`](save-interchange.md) §2).

- [ ] Play on through the meeting with Rei and Teepo. Does a name-entry screen
      appear there, or anywhere?
- [ ] Look through the in-game menu and the start screen's options entry for
      anything that renames a character. The exe has name-editing code that
      works on *whoever is in the party*, reached from a large menu module
      ([`save-interchange.md`](save-interchange.md) §2a) — it smells like a
      menu option, not a story prompt.
- [ ] If it does: how many characters does it allow, and what does the
      save-menu summary show for a name longer than two characters? (The
      summary's name field is still 5 bytes.)
- Result goes to: [`save-interchange.md`](save-interchange.md) §2.
