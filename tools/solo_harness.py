"""Shared pieces of the solo-character round's harness (docs/solo-character-proof.md):
a harness root of its own, a recipe run through tools/input_run.py with a clean
environment, and the reads of a run's log the three runners share.

Used by tools/solo_proof.py (the state-hash proof with the behaviours off),
tools/solo_routes.py (the battle routes with a team of one) and
tools/solo_boss_sweep.py (the boss sweep's pre-fight saves with a team of one).
Nothing here writes into the tree: the root holds the launcher copies, the game
folder (BOF3.exe and the movies copied, DAT / BGM / SND junctioned read-only),
the recipe saves, the state hashes and the logs - game data and run output,
outside the repository (CLAUDE.md rule 1).

The root's shape (`prepare`):

    <root>/L<name>/   bof3x-launcher.exe, bof3x-launcher-dev.exe, bof3x.exe, bof3x.dll
                      of one build, no bof3x.ini (a play ini's cheats fail tests)
    <root>/g/         BOF3.exe, BOF3.CFG, capcom.avi, logos.avi; DAT, BGM, SND junctions
    <root>/saves/     the recipe saves (BOF3X_RECIPE_SAVES) and, for the boss sweep, the crafts

Runs are sequential, one game at a time; a game is ended only by input_run.py,
by its own pid. A file <root>/STOP ends a batch before its next run.
"""
import os
import re
import shutil
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)

LAUNCHER_FILES = ('bof3x-launcher.exe', 'bof3x-launcher-dev.exe', 'bof3x.exe', 'bof3x.dll')
GAME_FILES = ('BOF3.exe', 'BOF3.CFG', 'capcom.avi', 'logos.avi')
GAME_DIRS = ('DAT', 'BGM', 'SND')

# The ten state-hash routes (docs/state-hash.md 6; the order of the 2026-11-12 proof chains)
TEN_ROUTES = ('combat', 'shop', 'caughFish', 'field_menu', 'menu_screens', 'dragonTransform', 'cutsceneAndNue',
              'masterAndManillo', 'worldMapAndAreaTransition', 'whelpBoss')

# The six champions BOF3X_SOLO names (src/game/solo/solo.h), records 0, 1, 2, 4, 5, 6
CHAMPIONS = {'ryu': 0, 'nina': 1, 'garr': 2, 'rei': 4, 'momo': 5, 'peco': 6}

# Addresses the runners watch (symbols.toml; docs/solo-character-round.md 0a)
FIELD_MODE = 0x66C7E8        # u16: 2 field, 5 battle
FIGHT_BYTE = 0x904AAA        # u8 EventBattle_Id
PARTY_BLOCK = 0x904060       # formation, 0x904061, first list 0x904062..64, second list 0x904065..67
MEMBER_COUNT = 0x929EC0      # Field_MemberCount
CHAMPION_MARK = 0x904A21     # Solo_ChampionMark, save block +0x1041 (DIV-0228)
CHAPTER = 0x8034E0           # Cond_ByteFA, the chapter byte
PARTY_SET = 0x90412C         # AreaW0b_PartySet (block +0x74C)
RECORDS = 0x903A70           # CharacterRecords, eight of 0xA4
RECORD_STRIDE = 0xA4
REC_ID, REC_LEVEL, REC_FLAGS, REC_EXP, REC_HP, REC_AP = 0x09, 0x0A, 0x0B, 0x0C, 0x18, 0x1A
REC_WATCH_FROM, REC_WATCH_LEN = 0x09, 0x13   # +0x09..+0x1B: id, level, flags, EXP (u32), ..., HP, AP

SAVE_CHAPTER = 0x00          # in a slot file (the 0x10B0-byte block from offset 0; tools/chapter_jump.py)
SAVE_MARK = 0x1041


def main_checkout():
    """A worktree's main checkout (abc_run.py's main_checkout), else this tree."""
    try:
        common = subprocess.run(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=ROOT,
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(os.path.normpath(common))
    except (OSError, subprocess.CalledProcessError):
        return ROOT


def winpath(p):
    return os.path.normpath(os.path.abspath(p))


def junction(link, target):
    if os.path.exists(link):
        return
    subprocess.run(['cmd', '/c', 'mklink', '/J', winpath(link), winpath(target)], check=True,
                   capture_output=True)


def prepare_launcher(root, name, build):
    """<root>/L<name>/ holding one build's four player files and no ini."""
    d = os.path.join(root, 'L' + name)
    os.makedirs(d, exist_ok=True)
    for f in LAUNCHER_FILES:
        src = os.path.join(build, f)
        if not os.path.isfile(src):
            if f == 'bof3x-launcher-dev.exe':
                continue
            sys.exit(f'{src}: not found - is {build} a built tree\'s build directory?')
        shutil.copy2(src, os.path.join(d, f))
    ini = os.path.join(d, 'bof3x.ini')
    if os.path.exists(ini):
        os.remove(ini)
    return d


def prepare_game(root, game_src):
    """<root>/g/: the exe, the CFG and the movies copied, DAT / BGM / SND junctioned (read only)."""
    g = os.path.join(root, 'g')
    os.makedirs(g, exist_ok=True)
    for f in GAME_FILES:
        src = os.path.join(game_src, f)
        if not os.path.isfile(src):
            sys.exit(f'{src}: not found - --game-src names the PC install (the folder with BOF3.exe and DAT)')
        if not os.path.exists(os.path.join(g, f)):
            shutil.copy2(src, os.path.join(g, f))
    for d in GAME_DIRS:
        junction(os.path.join(g, d), os.path.join(game_src, d))
    return g


def prepare_saves(root, saves_src):
    """<root>/saves/: a copy of the recipe saves (*.DAT) - game data, outside the tree."""
    s = os.path.join(root, 'saves')
    os.makedirs(s, exist_ok=True)
    n = 0
    for f in os.listdir(saves_src):
        if f.upper().endswith('.DAT'):
            dst = os.path.join(s, f)
            if not os.path.exists(dst):
                shutil.copy2(os.path.join(saves_src, f), dst)
            n += 1
    return s, n


def add_prepare_args(p):
    mc = main_checkout()
    p.add_argument('--game-src', default=os.path.join(mc, 'bof3'),
                   help='the PC install to copy BOF3.exe and the movies from and junction DAT/BGM/SND to '
                        '(read only; default the main checkout\'s bof3)')
    p.add_argument('--saves-src', default=os.path.join(mc, 'tools', 'recipe_saves'),
                   help='where the recipe saves are copied from (default the main checkout\'s tools/recipe_saves)')


def game_busy(root):
    """Whether a game or launcher run from this root is still alive (a launcher call returns before the
    game ends, docs/HANDOFF.md traps): processes whose path lies under <root>. Read only, kills nothing."""
    want = winpath(root).lower().replace("'", "''")
    ps = ("Get-Process -Name bof3x,BOF3,bof3x-launcher,bof3x-launcher-dev -ErrorAction SilentlyContinue | "
          f"Where-Object {{ $_.Path -and $_.Path.ToLower().StartsWith('{want}') }} | Measure-Object | "
          "ForEach-Object { $_.Count }")
    out = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True).stdout.strip()
    return out not in ('', '0')


def wait_idle(root, timeout=600):
    t0 = time.time()
    while game_busy(root):
        if time.time() - t0 > timeout:
            sys.exit(f'a game under {root} has not exited in {timeout} s: end it by its pid and run again')
        time.sleep(3)


def clean_env():
    """os.environ without any BOF3X_ variable: a run gets only what its runner sets."""
    return {k: v for k, v in os.environ.items() if not k.startswith('BOF3X_')}


def run_recipe(root, recipe, out, launcher_dir, env, runlog, minutes=40, extra=(), host='exe'):
    """One recipe through input_run.py, sequentially: waits for any game of this root to be gone first and
    after. `env` is a dict of BOF3X_ variables for the game. Returns (input_run's exit code, the status
    word it printed, the copied bof3x.log path or None, wall seconds)."""
    wait_idle(root)
    launcher = os.path.join(launcher_dir, 'bof3x-launcher.exe')
    ini = os.path.join(launcher_dir, 'bof3x.ini')
    if os.path.exists(ini):
        os.remove(ini)
    stale = os.path.join(launcher_dir, 'bof3x.log')
    if os.path.exists(stale):
        os.remove(stale)   # a run that never launches must not be read from the previous run's log
    cmd = [sys.executable, os.path.join(TOOLS, 'input_run.py'), recipe, '--out', out, '--no-front',
           '--game', os.path.join(root, 'g'), '--launcher', launcher, '--slot0-wait', '2400',
           '--minutes', str(minutes), '--host', host,
           # the slot files swapped are this root's own game folder's, never a player's: a game the owner plays
           # beside the harness (from another folder) does not share them (memory: harness beside the live game)
           '--slot0-shared'] + list(extra)
    full = clean_env()
    full.update(env)
    os.makedirs(out, exist_ok=True)
    t0 = time.time()
    with open(runlog, 'w', encoding='utf-8') as f:
        rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT, env=full).returncode
    time.sleep(3)
    wait_idle(root)
    status = ''
    with open(runlog, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('end: '):
                status = line[5:].strip()
    src = os.path.join(launcher_dir, 'bof3x.log')
    logcopy = os.path.splitext(runlog)[0] + '.log'
    if os.path.exists(src):
        shutil.copy2(src, logcopy)
    else:
        logcopy = None
    return rc, status, logcopy, time.time() - t0


def stopped(root):
    return os.path.exists(os.path.join(root, 'STOP'))


# ---- the log ----------------------------------------------------------------------------------

WATCH_LINE = re.compile(r'input\s+watch (\S+) ([0-9A-F]+) -> ([0-9A-F]+) recipe frame (\d+)')
WATCH_SET = re.compile(r'input\s+watch (\S+) 0x[0-9A-F]+ len \d+(?: shot)? = ([0-9A-F]+); recipe frame (\d+)')
END_LINE = re.compile(r'input\s+(done|FAILED) at recipe frame (\d+)')


def read_watches(log):
    """[(frame, name, old, new)] from a run's log ('' old for the value when the watch was set)."""
    out = []
    if not log or not os.path.exists(log):
        return out
    with open(log, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = WATCH_LINE.search(line)
            if m:
                out.append((int(m.group(4)), m.group(1), m.group(2), m.group(3)))
                continue
            m = WATCH_SET.search(line)
            if m:
                out.append((int(m.group(3)), m.group(1), '', m.group(2)))
    return out


def log_facts(log):
    """What a run's log says beyond the watches: a Fatal, a crash report, the recipe's end, the solo lines."""
    facts = {'fatal': '', 'crash': '', 'end': '', 'end_frame': None, 'solo': [], 'behaviours': ''}
    if not log or not os.path.exists(log):
        return facts
    with open(log, encoding='utf-8', errors='replace') as f:
        for line in f:
            s = line.strip()
            if re.match(r'(\S+\s+)?FATAL', s) and not facts['fatal']:
                facts['fatal'] = s[:300]
            if re.match(r'(\S+\s+)?CRASH \d+:', s) and not facts['crash']:
                facts['crash'] = s[:300]   # src/hook/crash.cpp's report lines
            m = END_LINE.search(s)
            if m:
                facts['end'], facts['end_frame'] = m.group(1), int(m.group(2))
            if 'DIV-0228' in s or 'DIV-0229' in s or s.find('solo:') >= 0:
                facts['solo'].append(s[:300])
            m = re.search(r'behaviours: \d+ of \d+ on', s)
            if m and not facts['behaviours']:
                facts['behaviours'] = m.group(0)
    return facts


def party_watches():
    """Recipe lines (no frame of their own) that watch the party: both lists and the formation, the
    member count, the party set, the champion's mark, the chapter, and each record's id, level, joined
    bits, EXP, HP and AP. Changes land in the log as `input watch NAME OLD -> NEW recipe frame F`."""
    L = [f'watch 0x{FIELD_MODE:X} 2 sl_mode',
         f'watch 0x{FIGHT_BYTE:X} 1 sl_fight',
         f'watch 0x{PARTY_BLOCK:X} 8 sl_party',
         f'watch 0x{MEMBER_COUNT:X} 1 sl_count',
         f'watch 0x{PARTY_SET:X} 1 sl_set',
         f'watch 0x{CHAMPION_MARK:X} 1 sl_mark',
         f'watch 0x{CHAPTER:X} 1 sl_chapter']
    for r in range(8):
        L.append(f'watch 0x{RECORDS + r * RECORD_STRIDE + REC_WATCH_FROM:X} {REC_WATCH_LEN} sl_rec{r}')
    return L


def le(hexs, off, n):
    b = bytes.fromhex(hexs)[off:off + n]
    return int.from_bytes(b, 'little')


def party_text(hexs):
    """The 8 bytes at 0x904060 as `form F | list1 a b c | list2 a b c` (ids in hex, -- for 0xFF)."""
    b = bytes.fromhex(hexs)
    ids = lambda s: ' '.join('--' if x == 0xFF else f'{x:X}' for x in s)
    return f'form {b[0]:X} | first {ids(b[2:5])} | second {ids(b[5:8])}'


def record_text(hexs):
    """A record watch's 0x13 bytes (+0x09..+0x1B) as `id I lv L j B exp E hp H ap A`."""
    b = bytes.fromhex(hexs)
    off = lambda o: o - REC_WATCH_FROM
    return (f'id {b[off(REC_ID)]:X} lv {b[off(REC_LEVEL)]} j {b[off(REC_FLAGS)] & 3} '
            f'exp {int.from_bytes(b[off(REC_EXP):off(REC_EXP) + 4], "little")} '
            f'hp {int.from_bytes(b[off(REC_HP):off(REC_HP) + 2], "little")} '
            f'ap {int.from_bytes(b[off(REC_AP):off(REC_AP) + 2], "little")}')


def timeline(watches, name):
    """[(frame, value)] of one watch, its first value included."""
    return [(f, v) for f, n, o, v in watches if n == name]


def at_or_before(watches, name, frame):
    """The watch's value at `frame` (its latest change at or before it); the last value when frame is None."""
    v = None
    for f, n, o, val in watches:
        if n == name and (frame is None or f <= frame):
            v = val
    return v


def first_last(watches, name):
    t = timeline(watches, name)
    return (t[0][1], t[-1][1]) if t else (None, None)


def battles(watches):
    """[(start frame, end frame or None)] of the field mode's stays at 5."""
    out, start = [], None
    for f, v in timeline(watches, 'sl_mode'):
        m = le(v, 0, 2)
        if m == 5 and start is None:
            start = f
        elif m != 5 and start is not None:
            out.append((start, f))
            start = None
    if start is not None:
        out.append((start, None))
    return out


def save_bytes(path):
    """The chapter byte and the champion's mark of a slot file - those two bytes, nothing else of it."""
    with open(path, 'rb') as f:
        data = f.read()
    if len(data) <= SAVE_MARK:
        return None, None
    return data[SAVE_CHAPTER], data[SAVE_MARK]


def recipe_save_name(recipe):
    with open(recipe, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.match(r'#\s*save\s+(\S+)', line)
            if m:
                return m.group(1)
            if not line.startswith('#'):
                break
    return None


def insert_after_header(recipe_lines, extra):
    """The recipe with `extra` lines put after its leading comment block (before frame 0's first step).
    Watches and peeks take no frame, so the route plays frame for frame as before."""
    i = 0
    while i < len(recipe_lines) and (recipe_lines[i].startswith('#') or not recipe_lines[i].strip()):
        i += 1
    return recipe_lines[:i] + extra + recipe_lines[i:]


def write_tsv(path, columns, rows, head_comment):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'# {head_comment}\n')
        f.write('\t'.join(columns) + '\n')
        for r in rows:
            f.write('\t'.join('' if r.get(c) is None else str(r.get(c)) for c in columns) + '\n')


def md_table(columns, rows):
    out = ['| ' + ' | '.join(columns) + ' |', '|' + '---|' * len(columns)]
    for r in rows:
        out.append('| ' + ' | '.join('' if r.get(c) is None else str(r.get(c)).replace('|', '/') for c in columns) + ' |')
    return '\n'.join(out)
