#!/usr/bin/env python
"""Watch a running game and log each change of the story's state and flags.

    python tools/story_watch.py                    watch until Ctrl+C
    python tools/story_watch.py --label newgame    name the log file
    python tools/story_watch.py --steps            also log each step of a run
    python tools/story_watch.py --only story,chapter,event

Run it beside the game (BOF3.exe or bof3x.exe; it waits for one). Each change
is one line on the console and one row of the log:

    chapter     the chapter byte (Cond_ByteFA 0x8034E0)
    status      Field_StatusBits 0x8034E1 bit 7, "the next chapter starts at the next area"
    run, step   the chapter's run (0x8034E4) and, with --steps, its step (0x8034E5)
    area        Game_AreaNumber 0x904EFC
    chapter flag  a bit of Cond_Flags 0x903F90, 20 rows of 64 (docs/story-flags.md)
    event flag    a bit of the event bank 0x904030, 256 bits
    defeated, skill, identified, chest, passage
                  the record and find banks 0x904068.. (docs/save-block.md)
    gene, master  Gene_Bits 0x904650 and the masters' bytes 0x904654

Each line carries where the game was: chapter, run, step, area and the
leader's cell. A flag that a map of docs/story-flags/ names shows what the map
says sets it; one the maps do not name is marked NOT IN THE MAPS.

**Notes.** Type a line and press Enter at any time: it is logged as a note for
the changes since the last note. The game pauses while its window is not in
front, so nothing is missed while you type. A sound plays when a flag changes
(--no-beep turns it off).

The log is analysis/storywatch/<label>.tsv (not in git), tab-separated:
time, play clock (h:mm:ss), chapter, run, step, area, x, z, what, number, change, known,
note. A `note` row applies to the rows above it, back to the note before.

Read-only, as tools/mem_watch.py: nothing is written to the game and no input
is sent. Sampling is every ~10 ms against a 33 ms logic frame; a flag set and
cleared inside one frame is not seen. Many bits changing at once (a load, a
new game) are logged as one row.
"""
import argparse, datetime, os, queue, re, struct, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import task_stacks as ts  # noqa: E402

ROOT = os.path.dirname(HERE)

# Live addresses (symbols.toml; docs/save-block.md gives each bank's block offset,
# block = address - 0x9039E0).
SCENARIO = 0x8034E0         # chapter, status bits, state, chapter's byte, run, step, u16 timer
AREA = 0x904EFC             # Game_AreaNumber, u16
LEADER_XZ = 0x802D74        # the leader's x and z, 16.16 (Save_BuildBlock copies them to the block)
PLAY_CLOCK = 0x9040C8       # block +0x6E8
BANKS = 0x903F90            # Cond_Flags; one read covers every bank below
BANKS_END = 0x90412C
GENES = 0x904650            # Gene_Bits dword, then the masters' six bytes at 0x904654

# what, address, bytes, the group --only selects it by
BANK_LIST = [
    ('event flag', 0x904030, 32, 'event'),
    ('defeated',   0x904068, 32, 'records'),
    ('skill',      0x904088, 32, 'records'),
    ('identified', 0x9040A8, 32, 'records'),
    ('chest',      0x9040CC, 32, 'finds'),
    ('passage',    0x90410C, 32, 'finds'),
]
GROUPS = ('story', 'chapter', 'event', 'records', 'finds', 'genes')
BULK = 12                   # more bit changes than this in one sample is a load or a new game


def bits(b):
    """The set bit numbers of a byte string, low bit of byte 0 first (Flags_Set)."""
    return {i * 8 + j for i, v in enumerate(b) for j in range(8) if v >> j & 1}


def clean(cell):
    cell = re.sub(r'`', '', cell)
    cell = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', cell)
    return cell.strip()


def load_chapter_maps():
    """{(chapter, flag): 'set by ...; kind'} from docs/story-flags/chapter-NN.md."""
    out = {}
    for c in range(20):
        path = os.path.join(ROOT, 'docs', 'story-flags', 'chapter-%02d.md' % c)
        if not os.path.exists(path):
            continue
        in_table = False
        for line in open(path, encoding='utf-8'):
            if line.startswith('| Flag |'):
                in_table = True
                continue
            if not in_table:
                continue
            if not line.startswith('|'):
                in_table = False
                continue
            cells = [clean(x) for x in line.strip().strip('|').split('|')]
            if len(cells) < 3 or set(cells[0]) <= set('-: '):
                continue
            # "4", "8, 9", "32..47", "0x10"
            nums = set()
            for part in re.split(r'[,/]| and ', re.sub(r'\(.*?\)|\[.*?\]', '', cells[0])):   # "32..47 (0x20..0x2F)"
                m = re.match(r'\s*(0x[0-9A-Fa-f]+|\d+)\s*(?:\.\.|-|to)\s*(0x[0-9A-Fa-f]+|\d+)\s*$', part)
                if m:
                    nums |= set(range(int(m.group(1), 0), int(m.group(2), 0) + 1))
                    continue
                m = re.match(r'\s*(0x[0-9A-Fa-f]+|\d+)\b', part)
                if m:
                    nums.add(int(m.group(1), 0))
            text = '%s [%s]' % (cells[1], cells[-1]) if cells[-1] != cells[1] else cells[1]
            for n in nums:
                if n < 64:
                    out.setdefault((c, n), text)
    return out


def load_event_map():
    """{flag: 'set: Function, ...'} from docs/story-flags/event-flags.md."""
    out, flag, sets, others = {}, None, [], []

    def close():
        if flag is not None and (sets or others):
            out[flag] = ('set by ' + ', '.join(sets[:3])) if sets else ('no setter in the map; ' + ', '.join(others[:2]))

    path = os.path.join(ROOT, 'docs', 'story-flags', 'event-flags.md')
    if not os.path.exists(path):
        return out
    for line in open(path, encoding='utf-8'):
        m = re.match(r'## Flag (\d+)', line)
        if m:
            close()
            flag, sets, others = int(m.group(1)), [], []
            continue
        if line.startswith('## '):
            close()
            flag = None
            continue
        if flag is None or not line.startswith('|'):
            continue
        cells = [clean(x) for x in line.strip().strip('|').split('|')]
        if len(cells) < 3 or cells[0] in ('Op', '') or set(cells[0]) <= set('-: '):
            continue
        (sets if cells[0].startswith('set') or cells[0].startswith('toggle') else others).append(
            '%s (%s)' % (cells[1], cells[2]) if cells[0].startswith(('set', 'toggle')) else '%s %s' % (cells[0], cells[1]))
    close()
    return out


def load_reference_names():
    """{'gene': {n: name}, 'master': .., 'area': .., 'chapter': ..} from data/save-names.tsv."""
    out = {}
    path = os.path.join(ROOT, 'data', 'save-names.tsv')
    if not os.path.exists(path):
        return out
    for line in open(path, encoding='utf-8'):
        if line.startswith('#') or not line.strip():
            continue
        cells = line.rstrip('\n').split('\t')
        if len(cells) < 3:
            continue
        try:
            out.setdefault(cells[0], {})[int(cells[1], 0)] = cells[2]
        except ValueError:
            pass
    return out


def load_bank_names(game_dir):
    """The record and find names from the player's install (tools/bank_names.py), or {}."""
    try:
        import bank_names
    except ImportError:
        return {}
    if game_dir is None and os.path.exists(os.path.join(ROOT, 'game', 'bof3x.ini')):
        game_dir = os.path.join(ROOT, 'game')       # the play install of this checkout, with its language
    try:
        return bank_names.load(game_dir) or {}
    except Exception as e:  # the watcher still works with numbers
        print('names not read (%s): the record and find bits show as numbers' % e, file=sys.stderr)
        return {}


def find_game():
    try:
        return ts.find_game()
    except SystemExit:
        return None


class Watch:
    def __init__(self, a):
        self.a = a
        self.groups = set(a.only.split(',')) if a.only else set(GROUPS)
        bad = self.groups - set(GROUPS)
        if bad:
            sys.exit('--only takes %s; not %s' % (', '.join(GROUPS), ', '.join(sorted(bad))))
        self.chapter_map = load_chapter_maps()
        self.event_map = load_event_map()
        self.ref = load_reference_names()
        names = load_bank_names(a.game)
        self.names = {'defeated': names.get('enemy', {}), 'identified': names.get('enemy', {}),
                      'skill': names.get('skill', {}), 'chest': names.get('chest', {}),
                      'passage': names.get('passage', {}),
                      'gene': self.ref.get('gene', {}), 'master met': self.ref.get('master', {}),
                      'master open': self.ref.get('master', {})}
        d = a.out or os.path.join(ROOT, 'analysis', 'storywatch')
        os.makedirs(d, exist_ok=True)
        label = a.label or datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
        self.path = os.path.join(d, label + '.tsv')
        new = not os.path.exists(self.path)
        self.log = open(self.path, 'a', encoding='utf-8')
        if new:
            self.log.write('time\tclock\tchapter\trun\tstep\tarea\tx\tz\twhat\tnumber\tchange\tknown\tnote\n')
        self.game = None
        self.last = None
        self.ctx = None
        self.pending = 0        # rows since the last note

    # --- reading -----------------------------------------------------------
    def sample(self):
        g = self.game
        sc = g.read(SCENARIO, 8)
        banks = g.read(BANKS, BANKS_END - BANKS)
        genes = g.read(GENES, 10)
        area = g.read(AREA, 2)
        xz = g.read(LEADER_XZ, 8)
        clock = g.read(PLAY_CLOCK, 4)
        if not (sc and banks and genes and area and xz and clock) or len(banks) != BANKS_END - BANKS:
            return None
        x, z = struct.unpack('<ii', xz)
        return {'chapter': sc[0], 'status': sc[1], 'run': sc[4], 'step': sc[5],
                'area': struct.unpack('<H', area)[0], 'x': x >> 16, 'z': z >> 16,
                'clock': struct.unpack('<I', clock)[0], 'banks': banks, 'genes': genes}

    # --- writing -----------------------------------------------------------
    def row(self, what, number, change, known='', note='', beep=False):
        c = self.ctx
        when = datetime.datetime.now().strftime('%H:%M:%S')
        where = 'ch %-2d run %-2d step %-3d area %03d (%d, %d)' % (c['chapter'], c['run'], c['step'], c['area'], c['x'], c['z'])
        self.log.write('\t'.join(str(v) for v in (
            when, '%d:%02d:%02d' % (c['clock'] & 0xFF, c['clock'] >> 8 & 0xFF, c['clock'] >> 16 & 0xFF), c['chapter'], c['run'], c['step'], c['area'], c['x'], c['z'],
            what, number, change, known, note)) + '\n')
        self.log.flush()
        if what in ('note', 'start'):
            return
        self.pending += 1
        text = '%s  %s  | %s%s %s' % (when, where, what, '' if number == '' else ' %s' % number, change)
        if known:
            text += '  : ' + (known if len(known) <= self.a.width else known[:self.a.width - 3] + '...')
        print(text, flush=True)
        if beep and not self.a.no_beep:
            try:
                import winsound
                winsound.MessageBeep(winsound.MB_OK)
            except Exception:
                pass

    def area_name(self, n):
        name = self.ref.get('area', {}).get(n)
        return '%03d%s' % (n, ' ' + name if name else '')

    def flag_changes(self, what, old, new, known_of, group):
        """Rows for the bits that differ between two byte strings."""
        if group not in self.groups or old == new:
            return []
        was, now = bits(old), bits(new)
        out = [(what, n, 'SET', known_of(n)) for n in sorted(now - was)]
        out += [(what, n, 'cleared', known_of(n)) for n in sorted(was - now)]
        return out

    def diff(self, old, new):
        rows = []
        ob, nb = old['banks'], new['banks']
        for c in range(20):
            o, n = ob[c * 8:c * 8 + 8], nb[c * 8:c * 8 + 8]
            rows += self.flag_changes(
                'chapter %d flag' % c, o, n,
                lambda f, c=c: self.chapter_map.get((c, f), 'NOT IN THE MAPS'), 'chapter')
        for what, addr, size, group in BANK_LIST:
            off = addr - BANKS
            if what == 'event flag':
                known = lambda f: self.event_map.get(f, 'NOT IN THE MAPS')
            else:
                known = lambda f, what=what: self.names[what].get(f, '')
            rows += self.flag_changes(what, ob[off:off + size], nb[off:off + size], known, group)
        og, ng = old['genes'], new['genes']
        rows += self.flag_changes('gene', og[0:4], ng[0:4], lambda f: self.names['gene'].get(f, ''), 'genes')
        rows += self.flag_changes('master met', og[4:7], ng[4:7], lambda f: self.names['master met'].get(f, ''), 'genes')
        rows += self.flag_changes('master open', og[7:10], ng[7:10], lambda f: self.names['master open'].get(f, ''), 'genes')
        return rows

    def step(self):
        new = self.sample()
        if new is None:
            return False
        old = self.last
        self.ctx = new
        if old is None:
            self.last = new
            ch = self.ref.get('chapter', {}).get(new['chapter'])
            row = new['banks'][new['chapter'] * 8:new['chapter'] * 8 + 8] if new['chapter'] < 20 else b''
            print('watching: chapter %d%s, run %d, step %d, area %s (%d, %d); this chapter\'s flags set: %s' % (
                new['chapter'], ' (%s)' % ch if ch else '', new['run'], new['step'], self.area_name(new['area']),
                new['x'], new['z'], ' '.join(str(b) for b in sorted(bits(row))) or 'none'), flush=True)
            self.row('start', '', 'watching', note='chapter flags set: ' + ' '.join(str(b) for b in sorted(bits(row))))
            self.pending = 0
            return True
        story = 'story' in self.groups
        rows = self.diff(old, new)
        if len(rows) > BULK:
            # A load, a new game or a chapter start: one row, the bits in the note column.
            parts = {}
            for what, n, change, _ in rows:
                parts.setdefault('%s %s' % (what, change), []).append(str(n))
            detail = '; '.join('%s: %s' % (k, ' '.join(v)) for k, v in parts.items())
            self.row('bulk', len(rows), 'bits changed at once (a load or a new game)', note=detail, beep=True)
            rows = []
        if story and new['chapter'] != old['chapter']:
            ch = self.ref.get('chapter', {}).get(new['chapter'], '')
            self.row('chapter', new['chapter'], 'was %d' % old['chapter'], ch, beep=True)
        if story and (new['status'] ^ old['status']) & 0x80:
            self.row('status', 'bit 7', 'SET' if new['status'] & 0x80 else 'cleared',
                     'the next chapter starts when the party next enters an area', beep=True)
        if new['area'] == 0xFFFF:       # between two areas: keep the area it left until the next one is in
            new['area'] = old['area']
        if story and new['area'] != old['area']:
            self.row('area', self.area_name(new['area']), 'was %03d' % old['area'])
        if story and new['run'] != old['run']:
            self.row('run', new['run'], 'was %d' % old['run'])
        elif story and self.a.steps and new['step'] != old['step']:
            self.row('step', new['step'], 'was %d' % old['step'])
        for what, n, change, known in rows:
            self.row(what, n, change, known, beep=True)
        self.last = new
        return True

    def note(self, text):
        if self.ctx is None:
            print('(no game yet: the note is not logged)')
            return
        self.row('note', '', '', note=text)
        print('    noted, for the last %d change%s' % (self.pending, '' if self.pending == 1 else 's'), flush=True)
        self.pending = 0

    def run(self):
        notes = queue.Queue()

        def reader():
            while True:
                try:
                    line = sys.stdin.readline()
                except Exception:
                    return
                if not line:
                    return
                if line.strip():
                    notes.put(line.strip())

        threading.Thread(target=reader, daemon=True).start()
        print('log: %s' % self.path)
        print('maps: %d chapter flags, %d event flags named; names: %s' % (
            len(self.chapter_map), len(self.event_map),
            ', '.join('%s %d' % (k, len(v)) for k, v in (
                ('enemies', self.names['defeated']), ('skills', self.names['skill']),
                ('chests', self.names['chest']), ('passages', self.names['passage'])))))
        print('type a note and press Enter at any time; Ctrl+C stops.')
        t0 = time.time()
        waiting = False
        while not self.a.seconds or time.time() - t0 < self.a.seconds:
            if self.game is None:
                pid = find_game()
                if pid is None:
                    if not waiting:
                        print('waiting for the game...', flush=True)
                        waiting = True
                    time.sleep(1)
                    continue
                waiting = False
                self.game = ts.Game(pid)
                self.last = None
            if not self.step():
                print('the game closed.', flush=True)
                self.game = None
                continue
            while not notes.empty():
                self.note(notes.get())
            time.sleep(0.01)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--label', help='the log is analysis/storywatch/<label>.tsv (default: the date and time); an existing log is added to')
    ap.add_argument('--out', help='another folder for the log')
    ap.add_argument('--steps', action='store_true', help='also log each step of a run')
    ap.add_argument('--only', help='groups to watch, comma-separated: ' + ', '.join(GROUPS))
    ap.add_argument('--game', help="the install the enemy, skill and find names are read from "
                                   "(default: <repo>\game if it is there, else tools/bank_names.py's)")
    ap.add_argument('--no-beep', action='store_true', help='no sound on a flag change')
    ap.add_argument('--seconds', type=float, default=0, help='stop after this long (default: until Ctrl+C)')
    ap.add_argument('--width', type=int, default=110, help='the longest "known" text on the console; the log holds all of it')
    a = ap.parse_args()
    try:
        Watch(a).run()
    except KeyboardInterrupt:
        print('\nstopped.')


if __name__ == '__main__':
    main()
