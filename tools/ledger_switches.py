#!/usr/bin/env python
"""Every switch name src/ reads, its class, and what it makes of each ledger entry.

    python tools/ledger_switches.py           # write analysis/ledger_switches.tsv, print the counts
    python tools/ledger_switches.py --check   # the readers below still find what they must; writes nothing
    python tools/ledger_switches.py --out F   # write the TSV somewhere else

Reads only src/ and docs/DIVERGENCE.md - no game data, no build tree - so it
runs anywhere, CI included (.github/workflows/checks.yml). The ledger-fields
round's tool (docs/ledger-fields-round.md section 2); tools/ledger_check.py
imports it to check the entries' `Switch:` and `Player:` lines.

The names, each with every file and line that reads it, and one class:

  code         the first argument of bof3::PatchBytes or bof3::RetargetCall
               (or of a function of ours that passes its own name argument on
               to one): our code reads the answer in both hosts
               (docs/operand-store.md), so the player's build could read the
               name from a switch of its own - only the switch is missing
  detour       a name given only to bof3::Inject / BOF3_INJECT: the switch
               leaves Capcom's function, which bof3x.exe does not map
  instrument   an Inject or RetargetCall with `instrument` true (a probe, not
               game code): no name or `*` switches it off
  launcher     an ini key the launcher reads (src/launcher/config.cpp
               ConfigLoad), `lang`, or an environment name the launcher sets
               from one (ConfigApplyEnvironment): a player's switch today
  env          an environment name both hosts read and the launcher does not
               set: a player can set it, and bof3x.exe reads it
  hosted-only  an environment name, or name=value, that bof3x.exe refuses at
               start-up (src/hook/exe_host.cpp RefuseHostedSwitches) and the
               launcher hands to the hosted build (src/launcher/launcher.cpp
               HostedOnlyAsked); the two lists must agree

What each class gives an entry switched by it (the `Player:` field,
docs/DIVERGENCE.md "Entry format"): launcher, env and code `yes`, detour and
hosted-only `costly`. A code name is a player's switch through PLAYER_SWITCH
(src/hook/detour.cpp, both hosts) and the launcher's ini key PLAYER_INI -
the ledger-fields round's stage D, 2026-10-28, which retired the `cheap`
value code names gave before; `--check` fails if src/ stops reading either.

The entries: each `Reversible?` line's names - `BOF3X_ORIGINAL=<names>`,
other `BOF3X_*` names, `ini=value` pairs of the launcher's keys - their
classes, the kind of the entry (section 0's table of the round doc) and the
`Player:` value the best of its switches gives. The prose is read, not
trusted: a name in it that nothing in src/ reads is listed as `unread`.
"""
import argparse, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join('docs', 'DIVERGENCE.md')
TSV = os.path.join('analysis', 'ledger_switches.tsv')

# The player's switch of the ledger-fields round's stage D (the owner,
# 2026-10-27, docs/ledger-fields-round.md section 6 call 3): a comma list of
# `Switch:` names, BOF3X_ORIGINAL's list syntax, and its ini key.
PLAYER_SWITCH = 'BOF3X_OFF'
PLAYER_INI = 'off'

# Environment names that are tooling, not a switch of any entry: a
# `Reversible?` line that mentions one is not switched by it. A name ending in
# LOG writes a log and changes nothing in play.
TOOLING_ENV = {'BOF3X_ORIGINAL', 'BOF3X_SHADOW', 'BOF3X_CALLTRACE', 'BOF3X_SELFTEST_ONLY'}
TOOLING_ENV_RE = re.compile(r'BOF3X_\w*LOG')

# A Reversible? field longer than this carries amendments or verification
# prose too: its names are listed, and the row says to read it by hand.
LONG_PROSE = 600

# What each class gives an entry, best first. `code` gave `cheap` until stage D
# (2026-10-28) made BOF3X_OFF read by src/; `cheap` is retired.
PLAYER_OF = {'launcher': 'yes', 'env': 'yes', 'code': 'yes', 'detour': 'costly', 'hosted-only': 'costly'}
PLAYER_RANK = {'yes': 0, 'costly': 2, 'never': 3}

CLASSES = ('code', 'detour', 'instrument', 'launcher', 'env', 'hosted-only')


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return f.read()


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def blank_comments(text):
    """The text with // and /* */ comments blanked to spaces (newlines kept),
    string and character literals left as they are."""
    out, i, n = list(text), 0, len(text)
    while i < n:
        c = text[i]
        if c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            i = j + 1
        elif text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(out)


def call_args(text, open_pos):
    """The top-level arguments of the call whose '(' is at open_pos, as
    stripped strings, and the position after its ')'. None if unbalanced."""
    depth, i, n, start, args = 0, open_pos, len(text), open_pos + 1, []
    while i < n:
        c = text[i]
        if c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            i = j + 1
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(text[start:i].strip())
                return args, i + 1
        elif c == ',' and depth == 1:
            args.append(text[start:i].strip())
            start = i + 1
        i += 1
    return None, n


def sources(top='src', exts=('.cpp', '.h')):
    for d, dirs, files in os.walk(os.path.join(ROOT, top)):
        dirs.sort()
        for name in sorted(files):
            if name.endswith(exts):
                p = os.path.join(d, name)
                rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
                with open(p, encoding='utf-8') as f:
                    yield rel, f.read()


def function_body(text, name):
    """The body of the first definition of `name` in text (comment-blanked),
    and its offset, or (None, -1)."""
    m = re.search(r'\b' + re.escape(name) + r'\s*\([^;{)]*\)\s*\{', text)
    if not m:
        return None, -1
    depth, i = 0, m.end() - 1
    while i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[m.end():i], m.end()
        i += 1
    return None, -1


class Inventory:
    """Every switch name src/ reads. names: key -> {'class', 'how', 'where'}."""

    def __init__(self):
        self.names = {}
        self.problems = []   # what --check and ledger_check report as errors

    def add(self, key, cls, how, where):
        e = self.names.setdefault(key, {'classes': set(), 'how': set(), 'where': []})
        e['classes'].add(cls)
        e['how'].add(how)
        if where not in e['where']:
            e['where'].append(where)

    def cls(self, key):
        """One class per name: code beats detour (our code honours the name in
        both hosts), instrument only when nothing else uses the name."""
        e = self.names.get(key)
        if not e:
            return None
        for c in ('hosted-only', 'launcher', 'code', 'env', 'detour', 'instrument'):
            if c in e['classes']:
                return c
        return None

    def player(self, key):
        return PLAYER_OF.get(self.cls(key))


API_CALL = re.compile(r'(?<![\w.>])(?:(?:::)?bof3::)?(PatchBytes|RetargetCall|Inject|BOF3_INJECT)\s*\(')
ENV_LIT = re.compile(r'L?"(BOF3X_[A-Z0-9_]+)"')


def scan_code_names(inv):
    """PatchBytes / RetargetCall / Inject names, wrappers resolved."""
    wrappers = []   # (file, wrapper name, arg index, api)
    texts = {}
    for rel, raw in sources():
        text = blank_comments(raw)
        texts[rel] = text
        for m in API_CALL.finditer(text):
            api = m.group(1)
            args, _ = call_args(text, m.end() - 1)
            if not args or not args[0]:
                continue
            first = args[0]
            where = f'{rel}:{line_of(text, m.start())}'
            if api == 'BOF3_INJECT':
                if re.fullmatch(r'\w+', first) and not text[max(0, m.start() - 8):m.start()].rstrip().endswith('define'):
                    inv.add(first, 'detour', 'BOF3_INJECT', where)
                continue
            if first.startswith('const char') or first == '#name':
                continue   # the declaration, the definition, the macro's body
            lit = re.fullmatch(r'"(\w+)"', first)
            instrument = (api == 'Inject' and len(args) >= 4 and args[3] == 'true') or \
                         (api == 'RetargetCall' and len(args) >= 5 and args[4] == 'true')
            cls = 'instrument' if instrument else ('detour' if api == 'Inject' else 'code')
            if lit:
                inv.add(lit.group(1), cls, api, where)
            elif re.fullmatch(r'\w+', first):
                # A function of ours that hands its own name argument on.
                sig = None
                for s in re.finditer(r'\b(\w+)\s*\(([^()]*?\bconst\s+char\s*\*\s*' + re.escape(first)
                                     + r'\b[^()]*)\)\s*\{', text[:m.start()]):
                    sig = s
                if not sig:
                    inv.problems.append(f'{where}: {api}({first}, ...) - a name argument the tool cannot trace '
                                        'to a caller; tools/ledger_switches.py needs to learn this form')
                    continue
                params = [p.strip() for p in sig.group(2).split(',')]
                idx = next(i for i, p in enumerate(params) if re.search(r'\b' + re.escape(first) + r'$', p))
                wrappers.append((rel, sig.group(1), idx, api, cls, where))
            else:
                inv.problems.append(f'{where}: {api}({first[:40]}, ...) - a name the tool cannot read')
    for rel, wname, idx, api, cls, where in wrappers:
        text, found = texts[rel], 0
        for m in re.finditer(r'(?<![\w.>])' + re.escape(wname) + r'\s*\(', text):
            args, _ = call_args(text, m.end() - 1)
            if not args or len(args) <= idx:
                continue
            lit = re.fullmatch(r'"(\w+)"', args[idx])
            if lit:
                inv.add(lit.group(1), cls, f'{api} via {wname}', f'{rel}:{line_of(text, m.start())}')
                found += 1
        if not found:
            inv.problems.append(f'{where}: {wname} passes its name to {api}, but no call of {wname} in {rel} '
                                'gives it a literal')
    return texts


def scan_hosted_only(inv, texts):
    """RefuseHostedSwitches and HostedOnlyAsked, which must agree."""
    exe = texts.get('src/hook/exe_host.cpp', '')
    body, off = function_body(exe, 'RefuseHostedSwitches')
    refused = {}
    if body is None:
        inv.problems.append('src/hook/exe_host.cpp: no RefuseHostedSwitches')
    else:
        for m in re.finditer(r'EnvSet\("(BOF3X_\w+)"\)', body):
            refused[m.group(1)] = f'src/hook/exe_host.cpp:{line_of(exe, off + m.start())}'
        for m in re.finditer(r'GetEnvironmentVariableA\("(BOF3X_\w+)"[^;]*?strcmp\(\w+,\s*"(\w+)"\)', body, re.S):
            refused[f'{m.group(1)}={m.group(2)}'] = f'src/hook/exe_host.cpp:{line_of(exe, off + m.start())}'
    la = texts.get('src/launcher/launcher.cpp', '')
    body, off = function_body(la, 'HostedOnlyAsked')
    asked = {}
    if body is None:
        inv.problems.append('src/launcher/launcher.cpp: no HostedOnlyAsked')
    else:
        for m in re.finditer(r'\{([^{}]*)\}', body):
            for n in re.finditer(r'L"(BOF3X_\w+)"', m.group(1)):
                asked[n.group(1)] = f'src/launcher/launcher.cpp:{line_of(la, off + m.start())}'
        for m in re.finditer(r'GetEnvironmentVariableW\(L"(BOF3X_\w+)"[^;]*;.*?wcscmp\(\w+,\s*L"(\w+)"\)', body, re.S):
            asked[f'{m.group(1)}={m.group(2)}'] = f'src/launcher/launcher.cpp:{line_of(la, off + m.start())}'
    if not refused:
        inv.problems.append('RefuseHostedSwitches: no switch found - the tool no longer reads it')
    for k in sorted(refused.keys() ^ asked.keys()):
        side = 'RefuseHostedSwitches (exe_host.cpp)' if k in refused else 'HostedOnlyAsked (launcher.cpp)'
        inv.problems.append(f'{k}: hosted-only in {side} only - the exe host and the launcher must refuse the '
                            'same switches')
    for k in sorted(refused.keys() | asked.keys()):
        for w in (refused.get(k), asked.get(k)):
            if w:
                inv.add(f'env:{k}', 'hosted-only', 'refused by bof3x.exe', w)
    return refused


def cfg_fields(text):
    """The Config fields a span of the launcher's code names: `cfg.cheats.exp`
    -> cheats.exp; a method called on one (`cfg.psp.c_str()`) is not a field."""
    out = set()
    for m in re.finditer(r'\bcfg\.(\w+(?:\.\w+)*)(\s*\()?', text):
        parts = m.group(1).split('.')
        if m.group(2) and len(parts) > 1:
            parts = parts[:-1]
        out.add('.'.join(parts))
    return out


def scan_launcher(inv, texts):
    """The ini keys ConfigLoad reads and the environment ConfigApplyEnvironment
    sets from them."""
    rel = 'src/launcher/config.cpp'
    text = texts.get(rel, '')
    body, off = function_body(text, 'ConfigLoad')
    keys = {}   # ini key (or 'key.' prefix) -> (line, cfg fields)
    if body is None:
        inv.problems.append(f'{rel}: no ConfigLoad')
    else:
        heads = list(re.finditer(
            r'key\s*==\s*"([\w.]+)"|key\.compare\(\s*0\s*,\s*\d+\s*,\s*"([\w.]+)"\s*\)', body))
        # Only the tests that choose a key's block, not a use of the key inside one.
        heads = [m for m in heads if re.search(r'(\bif\s*\(|\|\|)\s*$', body[max(0, m.start() - 16):m.start()])]
        # Keys tested in one condition (`a || b`) share the block after it.
        groups = []
        for i, m in enumerate(heads):
            if groups and '{' not in body[heads[i - 1].end():m.start()]:
                groups[-1].append(m)
            else:
                groups.append([m])
        for i, g in enumerate(groups):
            end = groups[i + 1][0].start() if i + 1 < len(groups) else len(body)
            fields = cfg_fields(body[g[0].start():end])
            for m in g:
                k = m.group(1) or m.group(2)
                keys.setdefault(k, (line_of(text, off + m.start()), set()))[1].update(fields)
    if not keys:
        inv.problems.append(f'{rel}: ConfigLoad reads no key the tool can see')
    body, off = function_body(text, 'ConfigApplyEnvironment')
    env_fields = {}
    if body is None:
        inv.problems.append(f'{rel}: no ConfigApplyEnvironment')
    else:
        for m in re.finditer(r'SetEnvironmentVariable[AW]\(\s*L?"(BOF3X_\w+)"', body):
            name = m.group(1)
            # The statement that guards it: back to the test of the same name.
            g = body.rfind(f'"{name}"', 0, m.start())
            g = body.rfind('if (', 0, g) if g >= 0 else m.start()
            _, after = call_args(body, m.end() - len(m.group(0)) + m.group(0).index('('))
            span = body[max(g, 0):after]
            env_fields.setdefault(name, (line_of(text, off + m.start()), set()))[1].update(
                cfg_fields(span))
    for k, (ln, fields) in sorted(keys.items()):
        feeds = sorted(n for n, (_, f) in env_fields.items() if fields & f)
        inv.add(f'ini:{k}', 'launcher', 'ini key' + (' -> ' + ','.join(feeds) if feeds else ''), f'{rel}:{ln}')
    for n, (ln, _) in sorted(env_fields.items()):
        inv.add(f'env:{n}', 'launcher', 'set by the launcher', f'{rel}:{ln}')
    inv.add('lang', 'launcher', 'the language box (ini:language -> BOF3X_LANG)',
            f'{rel}:{keys["language"][0]}' if 'language' in keys else rel)
    inv.ini_keys = set(keys)
    return keys


def scan_env(inv, texts):
    for rel, text in texts.items():
        for m in ENV_LIT.finditer(text):
            inv.add(f'env:{m.group(1)}', 'env', 'read in src/', f'{rel}:{line_of(text, m.start())}')


def inventory():
    inv = Inventory()
    texts = scan_code_names(inv)
    scan_hosted_only(inv, texts)
    scan_launcher(inv, texts)
    scan_env(inv, texts)
    inv.env_names = {k[4:] for k in inv.names if k.startswith('env:') and '=' not in k}
    inv.player_switch_built = PLAYER_SWITCH in inv.env_names and PLAYER_INI in inv.ini_keys
    if not inv.player_switch_built:
        inv.problems.append(f'src/ no longer reads {PLAYER_SWITCH}, or the launcher its ini key {PLAYER_INI}=: '
                            "a code name is then no player's switch, and the ledger's Player: yes for "
                            'one is false')
    if not any(inv.cls(k) == 'code' for k in inv.names):
        inv.problems.append('no PatchBytes or RetargetCall name found - the tool no longer reads src/')
    if not any(inv.cls(k) == 'detour' for k in inv.names):
        inv.problems.append('no Inject name found - the tool no longer reads src/')
    return inv


# --- the ledger ---------------------------------------------------------------

def ledger_entries(text=None):
    """[(id, title, chunk, first line)] for every entry under "## Entries"."""
    text = text if text is not None else read(LEDGER)
    head, sep, body = text.partition('\n## Entries\n')
    if not sep:
        return []
    line0 = head.count('\n') + 3
    out, cur = [], None
    for i, line in enumerate(body.split('\n')):
        if line.startswith('### '):
            cur = [line[4:].strip(), line0 + i, []]
            out.append(cur)
        elif line.startswith('## '):
            cur = None
        elif cur is not None:
            cur[2].append(line)
    res = []
    for title, ln, lines in out:
        chunk = '\n'.join(lines)
        m = re.search(r'^- \*\*ID:\*\*\s*(DIV-\d{4})\b', chunk, re.M)
        if m:
            res.append((m.group(1), title, chunk, ln))
    return res


def field(chunk, name):
    """A field's whole text (continuation lines joined), or None."""
    m = re.search(r'^- \*\*' + re.escape(name) + r'(?: \([^)]*\))?:?\*\*:?(.*?)(?=^- \*\*|\Z)', chunk, re.M | re.S)
    return ' '.join(m.group(1).split()) if m else None


def prose_names(text, inv):
    """The switch keys a Reversible? text names: (keys found, unread names)."""
    found, unread = [], []

    def take(key, raw):
        if key in found:
            return
        (found if key in inv.names else unread).append(key if key in inv.names else raw)

    for m in re.finditer(r'BOF3X_ORIGINAL=([A-Za-z_]\w*(?:\s*,\s*-?[A-Za-z_]\w*)*)', text):
        for n in re.split(r'\s*,\s*', m.group(1)):
            if not n.startswith('-'):
                take(n, n)
    for m in re.finditer(r'\bBOF3X_([A-Z0-9_]+)(=[\w.]+)?', text):
        name = 'BOF3X_' + m.group(1)
        if name in TOOLING_ENV or TOOLING_ENV_RE.fullmatch(name):
            continue
        if name == 'BOF3X_LANG':
            take('lang', 'lang')
            continue
        val = m.group(2) or ''
        key = f'env:{name}{val}' if f'env:{name}{val}' in inv.names else f'env:{name}'
        take(key, f'env:{name}')
    for m in re.finditer(r'`([a-z_]+(?:\.[a-z_]+)?)=([\w.,-]*)`', text):
        k = m.group(1)
        if k in getattr(inv, 'ini_keys', ()) or k.split('.')[0] + '.' in getattr(inv, 'ini_keys', ()):
            take(f'ini:{k}', f'ini:{k}')
    return found, unread


def entry_rows(inv, entries=None):
    rows = []
    for did, title, chunk, ln in (entries if entries is not None else ledger_entries()):
        rev = field(chunk, 'Reversible?') or ''
        tier = (field(chunk, 'Tier') or '').split(' ')[0]
        keys, unread = prose_names(rev, inv)
        classes = sorted({'lang' if k == 'lang' else inv.cls(k) for k in keys})
        says_no = bool(re.match(r'(\*\*)?(No\b|Nothing\b|Not a\b)', rev))
        kind = entry_kind(classes, says_no)
        players = [p for p in (key_player(inv, k) for k in keys) if p]
        if kind == 'no':
            player = 'never'
        else:
            player = min(players, key=PLAYER_RANK.get) if players else ''
        best = [k for k in keys if key_player(inv, k) == player]
        rows.append({'id': did, 'tier': tier, 'kind': kind, 'classes': classes, 'player': player,
                     'names': keys, 'best': best, 'unread': unread, 'says_no': says_no,
                     'long': len(rev) if len(rev) > LONG_PROSE else 0})
    return rows


def key_player(inv, key):
    return 'yes' if key == 'lang' else inv.player(key)


def entry_kind(classes, says_no):
    """Section 0's "how the entry is switched today", by its narrowest switch:
    a BOF3X_ORIGINAL name (one function, one patch) before a launcher or
    environment key, and either before the language, which switches every
    text entry at once."""
    named = {('detour' if c == 'hosted-only' else c) for c in classes} & {'code', 'detour'}
    if len(named) == 2:
        return 'mixed'
    if named:
        return named.pop()
    if {'launcher', 'env'} & set(classes):
        return 'launcher'
    if 'lang' in classes:
        return 'lang'
    return 'no' if says_no else 'unparsed'


def tsv(inv, rows):
    out = ['# tools/ledger_switches.py - every switch name src/ reads and each ledger entry\'s switches.',
           '# Generated; no game data. Columns: row (switch | entry), key, class, player, names, where, note.',
           'row\tkey\tclass\tplayer\tnames\twhere\tnote']
    for key in sorted(inv.names, key=lambda k: (k.startswith(('env:', 'ini:', 'lang')), k.lower())):
        e = inv.names[key]
        out.append('\t'.join(['switch', key, inv.cls(key), inv.player(key) or '', '',
                              ' '.join(e['where']), ', '.join(sorted(e['how']))]))
    for r in rows:
        note = [f'tier {r["tier"]}', f'kind {r["kind"]}']
        if r['long']:
            note.append(f'Reversible? is {r["long"]} characters: read it by hand')
        if r['says_no'] and r['kind'] != 'no':
            note.append('the prose starts "No" but names a switch')
        if r['unread']:
            note.append('unread: ' + ', '.join(r['unread']))
        out.append('\t'.join(['entry', r['id'], ','.join(r['classes']), r['player'],
                              ', '.join(r['best']), '', '; '.join(note)]))
    return '\n'.join(out) + '\n'


def counts(inv, rows):
    lines = []
    by_cls = {}
    for k in inv.names:
        by_cls[inv.cls(k)] = by_cls.get(inv.cls(k), 0) + 1
    lines.append('switch names: ' + ', '.join(f'{c} {by_cls.get(c, 0)}' for c in CLASSES)
                 + f' ({len(inv.names)} in all)')
    kinds = {}
    for r in rows:
        kinds[r['kind']] = kinds.get(r['kind'], 0) + 1
    order = ('code', 'detour', 'launcher', 'lang', 'no', 'mixed', 'unparsed')
    lines.append(f'entries: {len(rows)}; by kind: ' + ', '.join(f'{k} {kinds.get(k, 0)}' for k in order))
    pl = {}
    for r in rows:
        pl[r['player'] or '(by hand)'] = pl.get(r['player'] or '(by hand)', 0) + 1
    lines.append('Player from the best switch: ' + ', '.join(
        f'{k} {pl.get(k, 0)}' for k in ('yes', 'costly', 'never', '(by hand)')))
    lines.append(f'entries whose Reversible? is over {LONG_PROSE} characters (read by hand): '
                 f'{sum(1 for r in rows if r["long"])}')
    unread = [r for r in rows if r['unread']]
    lines.append(f'entries whose Reversible? names something src/ does not read: {len(unread)}')
    lines.append(f'the player\'s switch {PLAYER_SWITCH} / ini {PLAYER_INI}=: '
                 + ('read by src/ - code names count as yes' if inv.player_switch_built else 'NOT read by src/'))
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true', help='check the readers; write nothing')
    ap.add_argument('--out', default=os.path.join(ROOT, TSV))
    a = ap.parse_args()

    inv = inventory()
    rows = entry_rows(inv)
    for p in inv.problems:
        print(f'error: {p}')
    if not rows:
        print(f'error: {LEDGER}: no entries read')
        inv.problems.append('no entries')
    if a.check:
        print(f'ledger_switches: {len(inv.names)} switch names, {len(rows)} entries, '
              f'{len(inv.problems)} error(s)')
        return 1 if inv.problems else 0
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(tsv(inv, rows))
    for line in counts(inv, rows):
        print(line)
    print(f'wrote {os.path.relpath(os.path.abspath(a.out), ROOT)}')
    return 1 if inv.problems else 0


if __name__ == '__main__':
    sys.exit(main())
