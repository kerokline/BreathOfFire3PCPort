"""Drive the launcher's save editor through memory cards and conversion, as a player would.

    python tools/launcher_host/save_editor_drive.py <folder with bof3x-launcher.exe> --game <install copy>
        --card <a copy of a memory card file that holds a save of the game> --out <dir>

Everything it touches is a scratch copy: <install copy> holds copies of PC save files (BISLPS0X.DAT) and
the card is a copy too; the editor writes into both and into <install copy>/save-backups. It starts the
launcher, opens the Save Editor, and then:

  1. opens the card through "A different file or memory card..." and reads the two file boxes;
  2. writes the card's first save to PC slot F (card to PC) and compares the file with
     tools/save_convert.py's psx2pc of the same save: the same bytes but for the names, which the
     editor changes to the install's default names (--pc-names gives what record 1's must be);
  3. opens PC slot F again and writes it to a new save on the card (PC to card), of the first save's
     release when the card has room for one, then checks the card: directory check bytes, the block's
     checksum, and that the block is the first save's block (but for the names when the release differs);
  4. writes the same save to a memory card file that does not exist yet, and checks that card.

It finds every window by the process it started, never by a title (the owner's own launcher may be
open). Captures go to --out as PNG. Needs a desktop session. Exit code 0 when every check passed.
"""
import argparse, ctypes, io, contextlib, os, subprocess, sys, time
from ctypes import wintypes as W

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from capture_page import u, windows_of, grab          # noqa: E402
import save_convert as sc                             # noqa: E402

WM_COMMAND, WM_SETTEXT, WM_GETTEXT, WM_GETTEXTLENGTH = 0x111, 0x0C, 0x0D, 0x0E
CB_GETCOUNT, CB_GETLBTEXT, CB_GETLBTEXTLEN, CB_SETCURSEL, CB_GETCURSEL = 0x146, 0x148, 0x149, 0x14E, 0x147
CBN_SELCHANGE = 1
IDOK, IDCANCEL, IDYES = 1, 2, 6
IDC_SAVEEDITOR = 1038                                                 # src/launcher/resource.h
IDC_SE_SLOT, IDC_SE_FILENOTE, IDC_SE_STATUS, IDC_SE_TARGET, IDC_SE_SAVE = 1800, 1802, 1809, 1810, 1811

u.SendMessageW.restype = ctypes.c_ssize_t
u.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, ctypes.c_void_p]
u.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
u.GetParent.restype = W.HWND
u.GetParent.argtypes = [W.HWND]
ENUMCHILD = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
u.EnumChildWindows.argtypes = [W.HWND, ENUMCHILD, W.LPARAM]

checks = failures = 0


def expect(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(('  ok   ' if ok else '  FAIL ') + what)


def cls(h):
    b = ctypes.create_unicode_buffer(64)
    u.GetClassNameW(h, b, 64)
    return b.value


def text(h):
    n = u.SendMessageW(h, WM_GETTEXTLENGTH, 0, None)
    b = ctypes.create_unicode_buffer(n + 1)
    u.SendMessageW(h, WM_GETTEXT, n + 1, ctypes.cast(b, ctypes.c_void_p))
    return b.value


def item(dlg, cid):
    return u.GetDlgItem(dlg, cid)


def rows(dlg, cid):
    h, out = item(dlg, cid), []
    for i in range(u.SendMessageW(h, CB_GETCOUNT, 0, None)):
        b = ctypes.create_unicode_buffer(u.SendMessageW(h, CB_GETLBTEXTLEN, i, None) + 1)
        u.SendMessageW(h, CB_GETLBTEXT, i, ctypes.cast(b, ctypes.c_void_p))
        out.append(b.value)
    return out


def selected(dlg, cid):
    r, i = rows(dlg, cid), u.SendMessageW(item(dlg, cid), CB_GETCURSEL, 0, None)
    return r[i] if 0 <= i < len(r) else ''


def pick(dlg, cid, row):
    """Select a row as the player does: the selection, then the box's notice (posted: it can open a dialog)."""
    u.SendMessageW(item(dlg, cid), CB_SETCURSEL, row, None)
    u.PostMessageW(dlg, WM_COMMAND, (CBN_SELCHANGE << 16) | cid, item(dlg, cid))


def row_with(dlg, cid, part):
    for i, t in enumerate(rows(dlg, cid)):
        if part in t:
            return i
    raise SystemExit(f'no row with "{part}" in box {cid}: {rows(dlg, cid)}')


def children(h):
    out = []
    u.EnumChildWindows(h, ENUMCHILD(lambda c, _: out.append(c) or True), 0)
    return out


def popup(pid, known, timeout=8):
    """A new top-level window of the process, or None once the wait is over."""
    t0 = time.time()
    while time.time() - t0 < timeout:
        for h, _ in windows_of(pid):
            if h not in known:
                time.sleep(0.4)
                return h
        time.sleep(0.15)
    return None


def answer(pid, known, out, name, file=None, limit=6):
    """Answers the dialogs that follow an action: a file dialog gets `file`, a question gets Yes.
    Returns the words of each message."""
    said = []
    for n in range(limit):
        h = popup(pid, known, 8 if n == 0 else 2.5)
        if not h:
            break
        edits = [c for c in children(h) if cls(c) == 'Edit' and cls(u.GetParent(c)) == 'ComboBox']
        if edits:                                   # the file dialog
            if file is None:
                raise SystemExit('a file dialog opened and the step has no file for it')
            u.SendMessageW(edits[0], WM_SETTEXT, 0, ctypes.cast(ctypes.c_wchar_p(file), ctypes.c_void_p))
            time.sleep(0.3)
            u.PostMessageW(h, WM_COMMAND, IDOK, 0)
        else:                                       # a message box
            words = ' '.join(text(c) for c in children(h) if cls(c) == 'Static' and text(c))
            said.append(words)
            try:
                grab(h, os.path.join(out, f'{name}_msg{len(said)}.png'))
            except SystemError:                     # the window went away first
                pass
            has_yes = any(cls(c) == 'Button' and 'Yes' in text(c) for c in children(h))
            u.PostMessageW(h, WM_COMMAND, IDYES if has_yes else IDOK, 0)
        t0 = time.time()
        while time.time() - t0 < 5 and any(w == h for w, _ in windows_of(pid)):
            time.sleep(0.1)
    time.sleep(0.4)
    return said


def card_saves(path):
    card = sc.read_card(path)
    return card, [(slot, name) for slot, name in sc.card_slots(card) if 'BOF3' in name]


def card_block(card, slot):
    return bytes(card[slot * sc.BLOCK + sc.GAME_OFF:slot * sc.BLOCK + sc.GAME_OFF + sc.GAME_LEN])


def name_bytes(pc):
    """The offsets of a block that hold text: the eight names (9 bytes on the PC, 5 on a card), the
    summary's copy, the sixty faerie names; and the checksum, which follows them."""
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
    """The structure of every save of the game on the card."""
    card, saves = card_saves(path)
    for slot, name in saves:
        frame = card[slot * 0x80:(slot + 1) * 0x80]
        x = 0
        for b in frame[:0x7F]:
            x ^= b
        game = card_block(card, slot)
        expect(x == frame[0x7F] and frame[4:10] == bytes([0, 0x20, 0, 0, 0xFF, 0xFF]), f'{what}: frame {slot} {name} has its size and check byte')
        expect(sc.stored_checksum(game) == sc.checksum(game), f'{what}: frame {slot} has a correct checksum')
        expect(card[slot * sc.BLOCK:slot * sc.BLOCK + 2] == b'SC', f'{what}: frame {slot} starts SC')
    return card, saves


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('launcher_dir')
    ap.add_argument('--game', required=True)
    ap.add_argument('--card', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--pc-names', default='Ryu,Nina,Garr,Teepo,Rei,Momo,Peco,Whelp',
                    help='the eight default names of the install, as its letters (the English layer\'s by default)')
    a = ap.parse_args()
    game, card_path, out = os.path.abspath(a.game), os.path.abspath(a.card), os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    slot_f = os.path.join(game, 'BISLPS0F.DAT')
    new_card = os.path.join(out, 'made-by-editor.mcr')
    for f in (new_card,):
        if os.path.exists(f):
            os.remove(f)
    card0, saves0 = card_saves(card_path)
    if not saves0:
        sys.exit('the card holds no save of the game')
    first_slot, first_name = saves0[0]
    with contextlib.redirect_stderr(io.StringIO()):
        want_pc = bytes(sc.convert(sc.card_game(card0, first_slot, card_path), True, None))

    env = {k: v for k, v in os.environ.items() if not k.startswith('BOF3X_')}
    p = subprocess.Popen([os.path.join(a.launcher_dir, 'bof3x-launcher.exe'), '--game', game], env=env, cwd=a.launcher_dir)
    try:
        main_dlg = popup(p.pid, set(), 20)
        if not main_dlg:
            sys.exit('the launcher did not open')
        u.PostMessageW(main_dlg, WM_COMMAND, IDC_SAVEEDITOR, 0)
        dlg = popup(p.pid, {main_dlg}, 10)
        if not dlg or not item(dlg, IDC_SE_SLOT):
            sys.exit('the save editor did not open')
        known = {main_dlg, dlg}
        time.sleep(0.5)
        grab(dlg, os.path.join(out, '0_opened.png'))

        print('1. open the card')
        pick(dlg, IDC_SE_SLOT, row_with(dlg, IDC_SE_SLOT, 'A different file'))
        answer(p.pid, known, out, '1_open', file=card_path)
        src, dst = rows(dlg, IDC_SE_SLOT), rows(dlg, IDC_SE_TARGET)
        print('   file box   :', *src[16:], sep='\n      ')
        print('   write box  :', *dst[16:], sep='\n      ')
        print('   note       :', text(item(dlg, IDC_SE_FILENOTE)))
        expect(sum('Card save' in t for t in src) == len(saves0), 'the file box lists each save of the card')
        expect(selected(dlg, IDC_SE_SLOT).startswith('Card save'), 'the first save of the card is open')
        expect(selected(dlg, IDC_SE_TARGET) == selected(dlg, IDC_SE_SLOT), 'the save writes to its own place')
        grab(dlg, os.path.join(out, '1_card_open.png'))

        print('2. card to PC: write it to slot F')
        pick(dlg, IDC_SE_TARGET, 15)
        time.sleep(0.3)
        u.PostMessageW(dlg, WM_COMMAND, IDC_SE_SAVE, 0)
        said = answer(p.pid, known, out, '2_to_pc')
        for s in said:
            print('   asked:', s[:140].replace('\n', ' '))
        print('   status     :', text(item(dlg, IDC_SE_STATUS)))
        got = open(slot_f, 'rb').read() if os.path.exists(slot_f) else b''
        expect(len(got) == sc.PC_LEN and same_but_names(got[:sc.GAME_LEN], want_pc, True) and not any(got[sc.GAME_LEN:]),
               'slot F is save_convert.py\'s psx2pc of the save, but for the names')
        expect(len(got) == sc.PC_LEN and sc.stored_checksum(got[:sc.GAME_LEN]) == sc.checksum(got[:sc.GAME_LEN]), 'slot F has a correct checksum')
        pc_names = [n.encode('ascii') for n in a.pc_names.split(',')]
        print('   names      :', [n.decode('latin1') for n in record_names(got, True)] if got else None)
        expect(bool(got) and record_names(got, True) == pc_names, 'the eight names in slot F are the install\'s default names')
        expect(bool(got) and got[0xCA0:0xCA5].split(b'\0')[0] == pc_names[0][:5], 'the summary has record 1\'s name')
        expect(any('PlayStation memory card to the PC game' in s for s in said), 'the editor said that the save changes game')
        expect(selected(dlg, IDC_SE_SLOT).startswith('Slot F'), 'slot F is the open save now')
        expect(open(card_path, 'rb').read() == bytes(card0), 'the card file did not change')
        grab(dlg, os.path.join(out, '2_in_slot_f.png'))

        print('3. PC to card: write slot F to a new save on the card')
        release = {'BASLUS-00422BOF3': 'US release', 'BISLPS-00990BOF3': 'Japan release', 'BESLES-01304BOF3': 'European release',
                   'BESLES-01319BOF3': 'French release', 'BESLES-01320BOF3': 'German release'}.get(first_name[:-2], first_name[:-2])
        same_release = any('Card: a new save, ' + release in t for t in rows(dlg, IDC_SE_TARGET))
        new_row = row_with(dlg, IDC_SE_TARGET, 'Card: a new save, ' + release if same_release else 'Card: a new save')
        print('   to         :', rows(dlg, IDC_SE_TARGET)[new_row])
        pick(dlg, IDC_SE_TARGET, new_row)
        time.sleep(0.3)
        u.PostMessageW(dlg, WM_COMMAND, IDC_SE_SAVE, 0)
        said = answer(p.pid, known, out, '3_to_card')
        for s in said:
            print('   asked:', s[:140].replace('\n', ' '))
        print('   status     :', text(item(dlg, IDC_SE_STATUS)))
        card1, saves1 = check_card(card_path, 'the card')
        expect(len(saves1) == len(saves0) + 1, 'the card has one more save')
        added = [s for s in saves1 if s not in saves0]
        if added:
            slot, name = added[0]
            print('   new save   :', slot, name)
            if same_release:
                expect(card_block(card1, slot) == card_block(card0, first_slot), 'the new save\'s block is the first save\'s block: there and back, names too')
            else:
                expect(same_but_names(card_block(card1, slot), card_block(card0, first_slot), False),
                       'the new save\'s block is the first save\'s block but for the names')
                print('   names      :', [n.hex(' ') for n in record_names(card_block(card1, slot), False)])
                expect(record_names(card_block(card1, slot), False) != record_names(card_block(card0, first_slot), False),
                       'the names are the other release\'s')
            expect(card1[slot * sc.BLOCK + 0x60:slot * sc.BLOCK + 0x200] == card0[first_slot * sc.BLOCK + 0x60:first_slot * sc.BLOCK + 0x200],
                   'the new save has the icon of a save of the game on the card')
            same = all(card1[i] == card0[i] for i in range(sc.CARD_SIZE)
                       if not (slot * sc.BLOCK <= i < (slot + 1) * sc.BLOCK or slot * 0x80 <= i < (slot + 1) * 0x80))
            expect(same, 'nothing else on the card changed')
        backups = os.listdir(os.path.join(game, 'save-backups')) if os.path.isdir(os.path.join(game, 'save-backups')) else []
        stem = os.path.splitext(os.path.basename(card_path))[0]
        expect(any(b.startswith(stem) for b in backups), 'a copy of the card from before is in save-backups')
        expect(selected(dlg, IDC_SE_SLOT).startswith('Card save'), 'the card save is the open save now')
        grab(dlg, os.path.join(out, '3_on_card.png'))

        print('4. a memory card file that is not there yet')
        pick(dlg, IDC_SE_TARGET, row_with(dlg, IDC_SE_TARGET, 'memory card file...'))
        answer(p.pid, known, out, '4_pick', file=new_card)
        print('   write box  :', *rows(dlg, IDC_SE_TARGET)[16:], sep='\n      ')
        print('   to         :', selected(dlg, IDC_SE_TARGET))
        print('   status     :', text(item(dlg, IDC_SE_STATUS)))
        expect(not os.path.exists(new_card), 'the new card is not a file before Save')
        expect(selected(dlg, IDC_SE_TARGET).startswith('Card: a new save'), 'a new save on the new card is selected')
        u.PostMessageW(dlg, WM_COMMAND, IDC_SE_SAVE, 0)
        said = answer(p.pid, known, out, '4_to_new_card')
        for s in said:
            print('   asked:', s[:140].replace('\n', ' '))
        print('   status     :', text(item(dlg, IDC_SE_STATUS)))
        expect(os.path.exists(new_card) and os.path.getsize(new_card) == sc.CARD_SIZE, 'the new card file is there')
        if os.path.exists(new_card):
            card2, saves2 = check_card(new_card, 'the new card')
            expect(len(saves2) == 1 and added and card_block(card2, saves2[0][0]) == card_block(card1, added[0][0]),
                   'the new card holds the one save, the same block as the card save that was open')
            expect(saves2 and added and saves2[0][1][:-2] == added[0][1][:-2], 'the save keeps the release of the card save that was open')
        grab(dlg, os.path.join(out, '4_new_card.png'))

        u.PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0)
        answer(p.pid, {main_dlg}, out, '5_close', limit=2)
        u.PostMessageW(main_dlg, WM_COMMAND, IDCANCEL, 0)
        time.sleep(0.5)
    finally:
        if p.poll() is None:
            p.kill()
    print(f'{checks} checks, {failures} failures')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
