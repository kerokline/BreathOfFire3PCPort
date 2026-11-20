"""save_editor_drive.py's cases, and more, said again against the drawn launcher's save editor with no window.

    python tools/launcher_host/save_editor_check.py <folder with bof3x-launcher and save-names.tsv>
        --pc <a folder of PC save files> --card <a memory card file with a save of the game> --ini <a bof3x.ini>
        --out <scratch dir> [--pc-names Ryu,...] [--stand-in <tools/os_host's os_tests>] [--only NAME,...]
    python tools/launcher_host/save_editor_check.py --list

Stage P5 of docs/launcher-portable.md. Each case copies the PC saves (BISLPS00..0F.DAT) into <out>/<case>/game and
the card into <out>/<case>, then runs `bof3x-launcher --check --ini <ini> --game <out>/<case>/game --script ...`
(the editor opened from the Mods tab; a picker is answered by the script's `pick` step, a question by `answer`),
once or a few times, and judges the result from the `report saves` step's text and from the files written, read back
with tools/save_convert.py (the reader save_editor_drive.py checked the Win32 window's files with) and with the editor
itself (a later run opens the file and reports its fields). Nothing is shown on the screen and no game starts.

The ini names the install the editor reads its names and tables from (its cache= line, or BOF3X_HOME's); the
saves and the card are copies, and every file the editor writes is under --out. --stand-in is the os_host test
program: copied under another name and started in its stand-in mode, it is "a game that is running" for the
seam's question (os/running.h). The cases `drive_*` are save_editor_drive.py's 33 checks, one by one (for a card
with two saves of one release; another card gives another count, as there). Exit code 0 when every check passed.

save_editor_drive.py drove the Win32 launcher's save editor through ctypes; it went with that window at the cutover
(docs/launcher-portable.md, stage P6, 2026-11-20), and so did this tool's two cases that ran it beside the drawn
editor (win32, win32_drive: section 13 has their results). Its cases are kept here by their names.

These files are game data: they stay under --out, and this tool prints counts, names of fields and the editor's
own sentences only.
"""
import argparse, contextlib, io, os, re, shutil, subprocess, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import save_convert as sc                             # noqa: E402

checks = failures = 0
PREFIX = 'bof3x-launcher: report: '


def expect(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(('  ok   ' if ok else '  FAIL ') + what)
    return ok


class Report:
    """One `report saves` block."""

    def __init__(self, lines):
        self.lines = lines
        self.files, self.targets, self.sections, self.records, self.fields, self.said = [], [], [], [], [], []
        self.values = {}
        for line in lines:
            m = re.match(r'(file|write to|section|record|field) (\d+): (.*)$', line)
            if m:
                text, chosen = m.group(3), m.group(3).endswith(' [selected]')
                if chosen:
                    text = text[:-len(' [selected]')]
                kind = m.group(1)
                if kind == 'field':
                    cells = (text.split('\t') + ['', '', ''])[:3]
                    self.fields.append((cells[0], cells[1], cells[2], chosen))
                else:
                    {'file': self.files, 'write to': self.targets, 'section': self.sections,
                     'record': self.records}[kind].append((text, chosen))
                continue
            if line.startswith(('message: ', 'asked: ', 'answered: ')):
                self.said.append(line)
                continue
            key, _, value = line.partition(': ')
            self.values[key] = value

    def selected(self, rows):
        return next((t for t, chosen in rows if chosen), '')

    def field(self, label):
        return next((f for f in self.fields if f[0] == label), None)


def run(ui, case, script, ini, expect_code=0, env_add=None):
    """The drawn launcher with no window on one script; its reports and its stdout."""
    game = os.path.join(case, 'game')
    env = {k: v for k, v in os.environ.items() if not k.startswith('BOF3X_')}
    env.update(env_add or {})
    cmd = [ui, '--check', '--ini', ini, '--game', game, '--page', 'mods', '--sub', '1', '--script',
           'press Save Editor...; ' + script]
    p = subprocess.run(cmd, cwd=case, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace',
                       timeout=300)
    out = p.stdout + p.stderr
    with open(os.path.join(case, 'runs.log'), 'a', encoding='utf-8') as log:
        log.write('$ ' + script + '\n' + out + f'exit {p.returncode}\n\n')
    if p.returncode != expect_code:
        print(f'  the run ended with exit code {p.returncode} (wanted {expect_code}):')
        for line in out.splitlines()[-6:]:
            print('    ' + line[:200])
    reports, block = [], None
    for line in p.stdout.splitlines():
        if not line.startswith(PREFIX):
            continue
        text = line[len(PREFIX):]
        if text.startswith('file note: '):
            block = []
            reports.append(block)
        if block is not None:
            block.append(text)
    return p.returncode, [Report(b) for b in reports], out


def fresh(out, name, pc, card):
    case = os.path.join(out, name)
    if os.path.isdir(case):
        shutil.rmtree(case)
    os.makedirs(os.path.join(case, 'game'))
    for f in sorted(os.listdir(pc)):
        if re.fullmatch(r'BISLPS0[0-9A-F]\.DAT', f, re.I):
            shutil.copy2(os.path.join(pc, f), os.path.join(case, 'game', f))
    card_copy = None
    if card:
        card_copy = os.path.join(case, os.path.basename(card))
        shutil.copy2(card, card_copy)
    return case, card_copy


def slot(case, n):
    return os.path.join(case, 'game', 'BISLPS0%X.DAT' % n)


def read(path):
    with open(path, 'rb') as f:
        return f.read()


def backups(case):
    d = os.path.join(case, 'game', 'save-backups')
    return sorted(os.listdir(d)) if os.path.isdir(d) else []


# ---- save_editor_drive.py's helpers, the same rules -----------------------------------------

def card_saves(path):
    card = sc.read_card(path)
    return card, [(s, name) for s, name in sc.card_slots(card) if 'BOF3' in name]


def card_block(card, s):
    return bytes(card[s * sc.BLOCK + sc.GAME_OFF:s * sc.BLOCK + sc.GAME_OFF + sc.GAME_LEN])


def name_bytes(pc):
    width = 9 if pc else 5
    at = {0x70, 0x71}
    for i in range(8):
        at.update(range(0x90 + i * 0xA4, 0x90 + i * 0xA4 + width))
    at.update(range(0xCA0, 0xCA5))
    at.update(range(0xCB6, 0xCBA))
    at.update(range(0xF10, 0xF10 + 300))
    return at


def same_but_names(a, b, pc):
    skip = name_bytes(pc)
    return len(a) == len(b) and all(a[i] == b[i] for i in range(len(a)) if i not in skip)


def record_names(game, pc):
    width = 9 if pc else 5
    return [bytes(game[0x90 + i * 0xA4:0x90 + i * 0xA4 + width]).split(b'\0')[0] for i in range(8)]


def check_card(path, what):
    card, saves = card_saves(path)
    for s, name in saves:
        frame = card[s * 0x80:(s + 1) * 0x80]
        x = 0
        for b in frame[:0x7F]:
            x ^= b
        game = card_block(card, s)
        expect(x == frame[0x7F] and frame[4:10] == bytes([0, 0x20, 0, 0, 0xFF, 0xFF]),
               f'{what}: frame {s} has its size and check byte')
        expect(sc.stored_checksum(game) == sc.checksum(game), f'{what}: frame {s} has a correct checksum')
        expect(card[s * sc.BLOCK:s * sc.BLOCK + 2] == b'SC', f'{what}: frame {s} starts SC')
    return card, saves


RELEASES = {'BASLUS-00422BOF3': 'US release', 'BISLPS-00990BOF3': 'Japan release', 'BESLES-01304BOF3': 'European release',
            'BESLES-01319BOF3': 'French release', 'BESLES-01320BOF3': 'German release'}


# ---- the cases ------------------------------------------------------------------------------

def case_drive(a):
    """save_editor_drive.py's four steps: the card opened, card to PC slot F, slot F to a new card save, a new card."""
    case, card_path = fresh(a.out, 'drive', a.pc, a.card)
    new_card = os.path.join(case, 'made-by-editor.mcr')
    card0, saves0 = card_saves(card_path)
    if not saves0:
        sys.exit('the card holds no save of the game')
    first_slot, first_name = saves0[0]
    with contextlib.redirect_stderr(io.StringIO()):
        want_pc = bytes(sc.convert(sc.card_game(card0, first_slot, card_path), True, None))
    open_card = f'set file=has A different file; pick {card_path}'

    print('1. open the card')
    code, reps, _ = run(a.ui, case, open_card + '; report saves', a.ini)
    r = reps[-1] if reps else Report([])
    expect(code == 0 and sum('Card save' in t for t, _ in r.files) == len(saves0), 'the file box lists each save of the card')
    expect(r.selected(r.files).startswith('Card save'), 'the first save of the card is open')
    expect(r.selected(r.targets) == r.selected(r.files) and r.selected(r.files) != '', 'the save writes to its own place')

    print('2. card to PC: write it to slot F')
    code, reps, _ = run(a.ui, case, open_card + '; set target=15; press Save; answer yes; report saves', a.ini)
    r = reps[-1] if reps else Report([])
    for s in r.said:
        print('   ' + s[:140])
    got = read(slot(case, 15)) if os.path.exists(slot(case, 15)) else b''
    expect(len(got) == sc.PC_LEN and same_but_names(got[:sc.GAME_LEN], want_pc, True) and not any(got[sc.GAME_LEN:]),
           'slot F is save_convert.py\'s psx2pc of the save, but for the names')
    expect(len(got) == sc.PC_LEN and sc.stored_checksum(got[:sc.GAME_LEN]) == sc.checksum(got[:sc.GAME_LEN]),
           'slot F has a correct checksum')
    pc_names = [n.encode('ascii') for n in a.pc_names.split(',')]
    expect(bool(got) and record_names(got, True) == pc_names, 'the eight names in slot F are the install\'s default names')
    expect(bool(got) and got[0xCA0:0xCA5].split(b'\0')[0] == pc_names[0][:5], 'the summary has record 1\'s name')
    expect(any('PlayStation memory card to the PC game' in s for s in r.said), 'the editor said that the save changes game')
    expect(r.selected(r.files).startswith('Slot F'), 'slot F is the open save now')
    expect(read(card_path) == bytes(card0), 'the card file did not change')

    print('3. PC to card: write slot F to a new save on the card')
    release = RELEASES.get(first_name[:-2], first_name[:-2])
    code, reps, _ = run(a.ui, case, open_card + '; report saves', a.ini)
    rows = [t for t, _ in reps[-1].targets] if reps else []
    same_release = any('Card: a new save, ' + release in t for t in rows)
    want_row = 'Card: a new save, ' + release if same_release else 'Card: a new save'
    print('   to         :', next((t for t in rows if want_row in t), '(none)'))
    code, reps, _ = run(a.ui, case, open_card + f'; set file=15; set target=has {want_row}; press Save; answer yes; report saves',
                        a.ini)
    r = reps[-1] if reps else Report([])
    for s in r.said:
        print('   ' + s[:140])
    card1, saves1 = check_card(card_path, 'the card')
    expect(len(saves1) == len(saves0) + 1, 'the card has one more save')
    added = [s for s in saves1 if s not in saves0]
    if added:
        s_new, name = added[0]
        if same_release:
            expect(card_block(card1, s_new) == card_block(card0, first_slot),
                   'the new save\'s block is the first save\'s block: there and back, names too')
        else:
            expect(same_but_names(card_block(card1, s_new), card_block(card0, first_slot), False),
                   'the new save\'s block is the first save\'s block but for the names')
            expect(record_names(card_block(card1, s_new), False) != record_names(card_block(card0, first_slot), False),
                   'the names are the other release\'s')
        expect(card1[s_new * sc.BLOCK + 0x60:s_new * sc.BLOCK + 0x200] ==
               card0[first_slot * sc.BLOCK + 0x60:first_slot * sc.BLOCK + 0x200],
               'the new save has the icon of a save of the game on the card')
        same = all(card1[i] == card0[i] for i in range(sc.CARD_SIZE)
                   if not (s_new * sc.BLOCK <= i < (s_new + 1) * sc.BLOCK or s_new * 0x80 <= i < (s_new + 1) * 0x80))
        expect(same, 'nothing else on the card changed')
    stem = os.path.splitext(os.path.basename(card_path))[0]
    expect(any(b.startswith(stem) for b in backups(case)), 'a copy of the card from before is in save-backups')
    expect(r.selected(r.files).startswith('Card save'), 'the card save is the open save now')

    print('4. a memory card file that is not there yet')
    if not added:
        expect(False, 'step 4 needs the save step 3 added')
        return
    # The open save as save_editor_drive.py leaves it: the card save step 3 wrote. Its row in the file box.
    code, reps, _ = run(a.ui, case, open_card + '; report saves', a.ini)
    files = [t for t, _ in reps[-1].files] if reps else []
    card_rows = [i for i, t in enumerate(files) if t.startswith('Card save')]
    order = [s for s, _ in saves1]
    row = card_rows[order.index(added[0][0])] if len(card_rows) == len(order) else -1
    code, reps, _ = run(a.ui, case, open_card + f'; set file={row}; set target=has memory card file...; pick {new_card}; '
                        'report saves', a.ini)
    r = reps[-1] if reps else Report([])
    print('   to         :', r.selected(r.targets))
    expect(not os.path.exists(new_card), 'the new card is not a file before Save')
    expect(r.selected(r.targets).startswith('Card: a new save'), 'a new save on the new card is selected')
    code, reps, _ = run(a.ui, case, open_card + f'; set file={row}; set target=has memory card file...; pick {new_card}; '
                        'press Save; report saves', a.ini)
    r = reps[-1] if reps else Report([])
    for s in r.said:
        print('   ' + s[:140])
    expect(os.path.exists(new_card) and os.path.getsize(new_card) == sc.CARD_SIZE, 'the new card file is there')
    if os.path.exists(new_card):
        card2, saves2 = check_card(new_card, 'the new card')
        expect(len(saves2) == 1 and card_block(card2, saves2[0][0]) == card_block(card1, added[0][0]),
               'the new card holds the one save, the same block as the card save that was open')
        expect(bool(saves2) and saves2[0][1][:-2] == added[0][1][:-2], 'the save keeps the release of the card save that was open')


def first_good_slot(pc):
    for n in range(16):
        p = os.path.join(pc, 'BISLPS0%X.DAT' % n)
        if os.path.exists(p):
            b = read(p)
            if len(b) == sc.PC_LEN and sc.stored_checksum(b[:sc.GAME_LEN]) == sc.checksum(b[:sc.GAME_LEN]):
                return n
    return -1


def case_edit(a):
    """A field changed, Set, Save to the same slot: the file has the value and a correct checksum, the copy from
    before is in save-backups, and the editor opened again reads the value back."""
    case, _ = fresh(a.out, 'edit', a.pc, None)
    n = first_good_slot(a.pc)
    before = read(slot(case, n))
    code, reps, _ = run(a.ui, case, 'report saves; set fields=0; set value=12345; press Set; report saves; press Save; '
                        'report saves', a.ini)
    expect(code == 0 and len(reps) == 3, 'three reports')
    if len(reps) < 3:
        return
    r0, r1, r2 = reps
    expect(r0.selected(r0.files).startswith('Slot %X' % n), f'the editor opens on the first good slot, {n:X}')
    expect(r0.fields[0][0] == 'Zenny', 'Game\'s first field is Zenny')
    expect(r1.field('Zenny')[1] == '12345' and r1.field('Zenny')[2] != '', 'Set: the row shows the value and the file\'s')
    expect(r1.values.get('dirty') == 'yes' and 'You changed this save' in r1.values.get('file note', ''),
           'the note says the save changed')
    expect(r2.values.get('status', '').startswith('Saved. A copy of the file from before is in '), 'Save: Saved, and where '
           'the copy is')
    after = read(slot(case, n))
    diff = [i for i in range(len(after)) if after[i] != before[i]]
    expect(sc.stored_checksum(after[:sc.GAME_LEN]) == sc.checksum(after[:sc.GAME_LEN]), 'the file has a correct checksum')
    expect(0 < len(diff) <= 6, f'a few bytes changed ({len(diff)}): the field and the checksum')
    bk = backups(case)
    expect(len(bk) == 1 and read(os.path.join(case, 'game', 'save-backups', bk[0])) == before,
           'save-backups holds the file from before, byte for byte')
    code, reps, _ = run(a.ui, case, 'report saves', a.ini)
    expect(bool(reps) and reps[0].field('Zenny')[1] == '12345' and reps[0].field('Zenny')[2] == '',
           'opened again: Zenny reads 12345 and the file is the editor')


def case_checksum(a):
    """A save with a wrong checksum: the question; No keeps the open save; Yes opens it; Save writes a correct
    checksum and nothing else."""
    case, _ = fresh(a.out, 'checksum', a.pc, None)
    n = first_good_slot(a.pc)
    good = bytearray(read(slot(case, n)))
    bad = bytearray(good)
    bad[0x70] ^= 0x5A
    with open(slot(case, 3), 'wb') as f:
        f.write(bad)
    code, reps, _ = run(a.ui, case, 'report saves; set file=3; answer no; report saves; set file=3; answer yes; report saves; '
                        'press Save; report saves', a.ini)
    expect(code == 0 and len(reps) == 4, 'four reports')
    if len(reps) < 4:
        return
    expect(reps[0].files[3][0].endswith('(damaged)'), 'the file box says the save is damaged')
    expect(any('The checksum of this save is incorrect' in s for s in reps[1].said), 'the question was asked')
    expect(reps[1].selected(reps[1].files).startswith('Slot %X' % n), 'No: the save that was open stays open')
    expect(reps[2].selected(reps[2].files).startswith('Slot 3'), 'Yes: the damaged save is open')
    after = read(slot(case, 3))
    expect(after == bytes(good), 'Save: the file is the save with its correct checksum, byte for byte')
    expect(not reps[3].files[3][0].endswith('(damaged)'), 'the file box no longer says damaged')


def case_refusals(a):
    """A value the field does not take is refused with its reason; a save the checks refuse is not written."""
    case, _ = fresh(a.out, 'refuse', a.pc, None)
    n = first_good_slot(a.pc)
    before = read(slot(case, n))
    code, reps, _ = run(a.ui, case, 'set fields=0; set value=-5; press Set; report saves; set fields=1; set value=0:00:00.00; '
                        'press Set; report saves; press Save; report saves', a.ini)
    expect(code == 0 and len(reps) == 3, 'three reports')
    if len(reps) < 3:
        return
    expect(reps[0].values.get('status', '') != '' and reps[0].values.get('dirty') == 'no',
           'a negative zenny is refused, with its reason: ' + reps[0].values.get('status', '')[:80])
    expect(reps[1].values.get('dirty') == 'yes', 'an all-zero play time is taken by the field')
    expect(any(s.startswith('message: The editor cannot save these values:') for s in reps[2].said),
           'Save: "The editor cannot save these values"')
    expect(read(slot(case, n)) == before and not backups(case), 'the file did not change and no copy was made')


def case_running(a):
    """"A game is running": a stand-in that the seam finds refuses a write to a PC slot; once it ends, the write is
    made. A write to a card is not refused."""
    if not a.stand_in:
        print('  (no --stand-in: the case is not run)')
        return
    case, card_path = fresh(a.out, 'running', a.pc, a.card)
    n = first_good_slot(a.pc)
    before = read(slot(case, n))
    windows = sys.platform == 'win32'
    name = 'bof3x_p5_standin' + ('.exe' if windows else '')
    cls = 'bof3x_p5_standin_class'
    copy = os.path.join(case, name)
    shutil.copy2(a.stand_in, copy)
    edit = 'set fields=0; set value=4321; press Set'
    # the window class alone, then the program alone (Windows); the program alone elsewhere
    marks = [(f'game-program -; game-window {cls}', 'its window class'), (f'game-program {name}; game-window -', 'its program')]
    if not windows:
        marks = marks[1:]
    for mark, what in marks:
        env = dict(os.environ, BOF3X_RUNNING_STANDIN='30000', BOF3X_RUNNING_CLASS=cls)
        flags = 0x08000000 if windows else 0   # CREATE_NO_WINDOW
        p = subprocess.Popen([copy], env=env, creationflags=flags)
        try:
            time.sleep(1.0)
            code, reps, _ = run(a.ui, case, f'{mark}; {edit}; press Save; report saves', a.ini)
            r = reps[-1] if reps else Report([])
            expect(r.values.get('game running') == 'yes', f'the seam finds the stand-in by {what}')
            expect(any(s == 'message: The game is running. Close the game, then save.' for s in r.said),
                   'Save: "The game is running. Close the game, then save."')
            expect(read(slot(case, n)) == before, 'the slot did not change')
            code, reps, _ = run(a.ui, case, f'{mark}; set file=has A different file; pick {card_path}; '
                                'set target=has Card: a new save; report saves', a.ini)
            r = reps[-1] if reps else Report([])
            expect(r.values.get('game running') == 'yes' and r.selected(r.targets).startswith('Card'),
                   'a card target is selected while the stand-in runs (the write to a card is not refused: '
                   'save_dialog.cpp asks only for a PC write)')
        finally:
            p.kill()
            p.wait()
    time.sleep(0.5)
    code, reps, _ = run(a.ui, case, f'game-program {name}; game-window {cls}; {edit}; press Save; report saves', a.ini)
    r = reps[-1] if reps else Report([])
    expect(r.values.get('game running') == 'no' and r.values.get('status', '').startswith('Saved.'),
           'the stand-in ended: the seam says no game runs, and Save writes')
    expect(read(slot(case, n)) != before, 'the slot changed')
    code, reps, _ = run(a.ui, case, 'report saves', a.ini)
    print('   the game\'s own marks (bof3x, the class Bof3) now: game running:',
          reps[0].values.get('game running') if reps else '?')


def case_discard(a):
    """The questions before a change is lost: a change of file, Reload and Close."""
    case, _ = fresh(a.out, 'discard', a.pc, None)
    n = first_good_slot(a.pc)
    other = next((i for i in range(16) if i != n and os.path.exists(slot(case, i))), -1)
    edit = 'set fields=0; set value=777; press Set'
    code, reps, _ = run(a.ui, case, f'{edit}; set file={other}; answer no; report saves; set file={other}; answer yes; '
                        f'report saves; {edit}; press Reload; answer no; report saves; press Reload; answer yes; report saves; '
                        f'{edit}; press Close; answer no; report saves; press Close; answer yes; open mods 1; '
                        'press Save Editor...; press Close; report saves', a.ini)
    expect(code == 0 and len(reps) == 6, 'six reports')
    if len(reps) < 6:
        return
    q = 'asked: You changed this save and did not save it. | Discard the changes?'
    expect(q in reps[0].said and reps[0].selected(reps[0].files).startswith('Slot %X' % n) and reps[0].values['dirty'] == 'yes',
           'a change of file asks; No keeps the save and the change')
    expect(reps[1].selected(reps[1].files).startswith('Slot %X' % other) and reps[1].values['dirty'] == 'no',
           'Yes opens the other file')
    expect(reps[2].values['dirty'] == 'yes' and reps[2].field('Zenny')[1] == '777', 'Reload asks; No keeps the change')
    expect(reps[3].values['dirty'] == 'no' and reps[3].field('Zenny')[1] != '777', 'Yes reads the file again')
    expect(reps[4].values['dirty'] == 'yes' and reps[4].said.count(q) == 5, 'Close asks; No keeps the editor open')
    expect(reps[5].said.count(q) == 0, 'the editor opened again and closed with no change: no question')
    expect(not backups(case) and read(slot(case, n)) == read(os.path.join(a.pc, 'BISLPS0%X.DAT' % n)), 'no file changed')


def case_replace(a):
    """Save to another slot that holds a save: the question; Yes writes it and keeps the other's copy."""
    case, _ = fresh(a.out, 'replace', a.pc, None)
    n = first_good_slot(a.pc)
    other = next((i for i in range(16) if i != n and os.path.exists(slot(case, i))), -1)
    before_other = read(slot(case, other))
    code, reps, _ = run(a.ui, case, f'set fields=0; set value=4242; press Set; set target={other}; press Save; answer no; '
                        f'report saves; press Save; answer yes; report saves; set target=5; press Save; report saves', a.ini)
    expect(code == 0 and len(reps) == 3, 'three reports')
    if len(reps) < 3:
        return
    expect(any('This slot has a save. Replace it?' in s for s in reps[0].said) and read(slot(case, other)) != b'' and
           len(reps[0].said) == 2, 'the question; No writes nothing')
    expect(reps[1].values['status'].startswith('Saved. A copy') and reps[1].selected(reps[1].files).startswith('Slot %X' % other),
           'Yes: saved, and the slot written is the open save')
    bk = backups(case)
    expect(len(bk) == 1 and read(os.path.join(case, 'game', 'save-backups', bk[0])) == before_other,
           'the copy of the slot from before is in save-backups')
    expect(os.path.exists(slot(case, 5)) and reps[2].values['status'] == 'Saved.' and
           read(slot(case, 5)) == read(slot(case, other)), 'an empty slot: no question, "Saved.", the same file')


def case_fields(a):
    """The kinds of field: a pick from a list set at once, a switch changed by a double-click, a field that cannot
    change, the advanced sections, the record box, the position's entrances."""
    case, _ = fresh(a.out, 'fields', a.pc, None)
    code, reps, _ = run(a.ui, case, 'set sections=2; set record=1; report saves; set fields=5; report saves; '
                        'set value=entry 3; report saves; set sections=0; set fields=3; report saves; set value=entry 0; '
                        'report saves; set advanced=1; report saves', a.ini)
    expect(code == 0 and len(reps) == 6, 'six reports')
    if len(reps) < 6:
        return
    r = reps
    expect(r[0].values['record box'] == 'on' and r[0].selected(r[0].records).startswith('Record 2'),
           'Characters: the record box is on, record 2 chosen')
    lab = r[1].fields[5][0]
    expect(int(r[1].values['choices']) > 3, f'the field "{lab}" offers a list')
    expect(r[2].fields[5][1] == r[2].values['value'] and r[2].fields[5][2] != '' and r[2].values['dirty'] == 'yes',
           'an entry picked from the list is set at once')
    expect(r[3].fields[3][0] == 'Position' and int(r[3].values['choices']) >= 1, 'Position offers the area\'s entrances')
    expect(r[4].fields[3][1] != r[3].fields[3][1] or r[4].fields[3][2] == '', 'a pick sets the position')
    expect(len(r[5].sections) > len(r[4].sections) and r[5].values['advanced'] == 'on',
           f'the advanced box adds sections ({len(r[4].sections)} to {len(r[5].sections)})')
    names = [t for t, _ in r[5].sections]
    flags = names.index('Event flags') if 'Event flags' in names else -1
    expect(flags >= 0, 'an advanced section "Event flags"')
    if flags < 0:
        return
    code, reps, _ = run(a.ui, case, f'set advanced=1; set sections={flags}; report saves', a.ini)
    rows = reps[0].fields if reps else []
    bit = next((i for i, f in enumerate(rows) if f[1] in ('On', 'Off')), -1)
    expect(bit >= 0, 'Event flags has switches')
    if bit < 0:
        return
    was = rows[bit][1]
    code, reps, _ = run(a.ui, case, f'set advanced=1; set sections={flags}; set fields=use {bit}; report saves; '
                        f'set fields=use {bit}; report saves', a.ini)
    expect(len(reps) == 2 and reps[0].fields[bit][1] != was and reps[0].values['dirty'] == 'yes',
           'a double-click changes a switch')
    expect(len(reps) == 2 and reps[1].fields[bit][1] == was and reps[1].values['dirty'] == 'no',
           'a second one changes it back')
    code, reps, _ = run(a.ui, case, 'set advanced=1; report saves', a.ini)
    secs = [t for t, _ in reps[0].sections] if reps else []
    other = secs.index('Other bytes') if 'Other bytes' in secs else -1
    if other >= 0:
        code, reps, _ = run(a.ui, case, f'set advanced=1; set sections={other}; set fields=0; report saves; set value=1; '
                            'press Set', a.ini, expect_code=1)
        expect(bool(reps) and reps[0].values['value'].endswith('(grey)') and
               'You cannot change this field.' in reps[0].values['note'] and code == 1,
               'a preserved field: the value box and Set are grey, and the note says so')


def case_pcfile(a):
    """A PC save file outside the game folder, opened by the picker: its row, written back to the same file, its copy
    in the game folder's save-backups."""
    case, _ = fresh(a.out, 'pcfile', a.pc, None)
    n = first_good_slot(a.pc)
    elsewhere = os.path.join(case, 'elsewhere.DAT')
    shutil.copy2(slot(case, n), elsewhere)
    before = read(elsewhere)
    code, reps, _ = run(a.ui, case, f'set file=has A different file; pick {elsewhere}; report saves; set fields=0; '
                        'set value=99; press Set; press Save; report saves', a.ini)
    expect(code == 0 and len(reps) == 2, 'two reports')
    if len(reps) < 2:
        return
    expect(reps[0].selected(reps[0].files) == 'File: ' + elsewhere and reps[0].selected(reps[0].targets) == 'The file that is open',
           'the file is open, "File: <path>", and writes to "The file that is open"')
    expect(reps[1].values['status'].startswith('Saved. A copy') and read(elsewhere) != before, 'Save writes the file')
    expect(len(backups(case)) == 1, 'its copy from before is in the game folder\'s save-backups')
    code, reps, _ = run(a.ui, case, 'set file=has A different file; pick; report saves', a.ini)
    expect(bool(reps) and reps[0].selected(reps[0].files).startswith('Slot'), 'a picker closed with no file keeps the '
           'save that was open')


def case_untouched(a):
    """The editor opened from the Mods tab and closed untouched: the tabs come back, nothing is written."""
    case, _ = fresh(a.out, 'untouched', a.pc, None)
    code, reps, out = run(a.ui, case, 'report saves; press Close; press Data##sub', a.ini)
    expect(code == 0 and len(reps) == 1, 'the editor opened and Close went back to the tabs (Mods\' Data sub-tab pressed)')
    expect(not backups(case) and all(read(slot(case, i)) == read(os.path.join(a.pc, 'BISLPS0%X.DAT' % i))
                                     for i in range(16) if os.path.exists(slot(case, i))), 'no file changed')


CASES = {'drive': case_drive, 'edit': case_edit, 'checksum': case_checksum, 'refuse': case_refusals,
         'running': case_running, 'discard': case_discard, 'replace': case_replace, 'fields': case_fields,
         'pcfile': case_pcfile, 'untouched': case_untouched}
DEFAULT = list(CASES)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('launcher_dir', nargs='?')
    ap.add_argument('--pc')
    ap.add_argument('--card')
    ap.add_argument('--ini')
    ap.add_argument('--out')
    ap.add_argument('--pc-names', default='Ryu,Nina,Garr,Teepo,Rei,Momo,Peco,Whelp',
                    help='the eight default names of the install, as its letters (the English layer\'s by default)')
    ap.add_argument('--stand-in', help='tools/os_host\'s os_tests, for "a game is running"')
    ap.add_argument('--only', help='cases, with , between them')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    if a.list:
        for name, f in CASES.items():
            print(f'{name}: {" ".join(f.__doc__.split())}')
        return 0
    if not (a.launcher_dir and a.pc and a.card and a.ini and a.out):
        ap.error('the launcher folder, --pc, --card, --ini and --out are needed')
    exe = 'bof3x-launcher.exe' if sys.platform == 'win32' else 'bof3x-launcher'
    a.ui = os.path.join(os.path.abspath(a.launcher_dir), exe)
    a.pc, a.card, a.ini, a.out = (os.path.abspath(p) for p in (a.pc, a.card, a.ini, a.out))
    a.stand_in = os.path.abspath(a.stand_in) if a.stand_in else None
    os.makedirs(a.out, exist_ok=True)
    for name in (a.only.split(',') if a.only else DEFAULT):
        print(f'== {name}: {" ".join(CASES[name].__doc__.split())[:110]}')
        CASES[name](a)
    print(f'{checks} checks, {failures} failures')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
