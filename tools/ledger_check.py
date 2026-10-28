#!/usr/bin/env python
"""Check the repository's two ledgers against each other and against the code.

    python tools/ledger_check.py            # exit 1 on any error
    python tools/ledger_check.py --verbose  # also list the informational notes

Reads only files in this repository - no game data, no build tree - so it runs
anywhere, including CI (.github/workflows/checks.yml).

The divergence ledger, docs/DIVERGENCE.md (CLAUDE.md rule 2):
  * every entry under "## Entries" is a `### title` followed by exactly one
    `- **ID:** DIV-NNNN`, and carries the fields the entry format names;
  * its Tier is one the ledger's "Tiers" table defines;
  * IDs run DIV-0001, DIV-0002, ... with no gap and no repeat;
  * the Status line's "N entries, DIV-0001..NNNN" matches the entries;
  * every DIV-NNNN cited anywhere in the repository exists;
  * the six switch fields (the ledger-fields round, docs/ledger-fields-round.md
    sections 1 and 2): `Switch:`, `Default:`, `Player:`, `Toward:`,
    `Languages:`, `State:`, each one line in its grammar; every `Switch:` name
    one that src/ or the launcher reads (tools/ledger_switches.py's inventory);
    `Player:` the value the names' classes give (`yes` refused on a name the
    player's build cannot reach), `never` for a Forced entry and for
    `Switch: none`; `Languages:` `any` or tags the engine knows
    (tools/language_tags.py) and `original`, never `any` for `Switch: lang`,
    and `Default: on - under its languages` for an entry it limits; a
    superseding entry that exists. A missing or malformed field is an error (STRICT_FIELDS,
    set by the round's stage C). Each run also tests these checks on the worked
    examples, DIV-0176 and DIV-0183, each with one field line changed, added or
    removed (FIXTURE_CASES).

The ownership ledger, symbols.toml `impl` against the detours in src/:
  * every function detoured to ours (BOF3_INJECT, or a direct bof3::Inject
    with bof3::addr::<name>) has an `impl` line, and every `impl` is detoured -
    so "N impl" and the log's "N ours" are the same number;
  * no name is detoured twice.

Rule 4 (no stubs):
  * no TODO / FIXME / XXX / "for now" markers in src/.

A missing field that only the author can write may be listed in KNOWN below
with its reason; the check fails once a listed gap is closed, so the list
cannot go stale.
"""
import argparse, os, re, sys, tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ledger_switches   # noqa: E402 - the switch inventory, beside this file
import language_tags     # noqa: E402 - the engine's language tags, beside this file

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join('docs', 'DIVERGENCE.md')

# Fields every entry carries (docs/DIVERGENCE.md, "Entry format"). A field may
# carry a parenthetical qualifier: "Rationale (why this side)".
REQUIRED = ['ID', 'Date', 'Subsystem', 'Tier', 'Original behaviour', 'New behaviour',
            'Rationale', 'Also in the PSX version?', 'Reversible?']

# The tiers docs/DIVERGENCE.md "Tiers" defines. The field is the tier word,
# optionally followed by " - <why>".
TIERS = ('Forced', 'Intent', 'Sensible', 'Extension')

# The switch fields (docs/DIVERGENCE.md "Entry format"; the ledger-fields
# round, docs/ledger-fields-round.md section 1), one line each, after
# Reversible?. A value may be followed by " - <clause>".
SWITCH_FIELDS = ['Switch', 'Default', 'Player', 'Toward', 'Languages', 'State']
# Set by the round's stage C (2026-10-28, after the audit's edits): every entry
# carries the six (`Languages:` added the same day, filled on every entry before
# it was listed here), and a missing one is an error like any other. Were a
# seventh field added, False would count the entries still without it as a note
# until they are filled, as stage B did.
STRICT_FIELDS = True
PLAYER_VALUES = ('yes', 'cheap', 'costly', 'never')
TOWARD_VALUES = ('psx', 'psp', 'pc')
# `Languages:` (the owner's call, 2026-10-28: explicit tags, no class names):
# `any` alone, or the tags the engine knows - imported, so the ledger cannot
# name one src/game/language_tags.h does not - and `original`, the port's own
# text (BOF3X_LANG unset or `original`).
LANGUAGE_VALUES = tuple(language_tags.TAGS) + ('original',)
# The Default clause of an entry whose Languages is not `any` and Default `on`.
LANG_DEFAULT = 'under its languages'
# (DIV id, field) -> why: an entry whose line of that name predates the switch
# fields and means something else. Its line is not read as the field; the
# check fails once the line is renamed, so the list cannot go stale.
FIELD_CLASH = {}  # DIV-0169's old "State:" line was renamed "Kept state:" by LF4 (2026-10-28)

# (DIV id, missing field) -> why it is accepted. Content only the author can
# write; the check will not invent it.
KNOWN = {}

# Where a DIV-NNNN may be cited: every file with one of these extensions under
# the repository root, tracked or not, outside CITE_SKIP_DIRS.
CITE_EXTS = {'.md', '.py', '.c', '.cpp', '.h', '.toml', '.rc', '.txt', '.yml', '.cmake', '.sh'}
CITE_SKIP_DIRS = {'.git', 'build', 'analysis', '_deps', '.claude'}

STUB_MARKERS = re.compile(r'\b(TODO|FIXME|XXX)\b|\bfor now\b', re.I)


class Report:
    def __init__(self):
        self.errors, self.notes = [], []

    def error(self, msg):
        self.errors.append(msg)

    def note(self, msg):
        self.notes.append(msg)


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return f.read()


def walk(top, exts):
    for d, dirs, files in os.walk(os.path.join(ROOT, top)):
        dirs[:] = sorted(x for x in dirs if x not in CITE_SKIP_DIRS)
        for name in sorted(files):
            if os.path.splitext(name)[1] in exts:
                p = os.path.join(d, name)
                yield os.path.relpath(p, ROOT).replace(os.sep, '/'), p


# --- the divergence ledger ---------------------------------------------------

def check_divergence(r, inv):
    text = read(LEDGER)
    head, sep, body = text.partition('\n## Entries\n')
    if not sep:
        r.error(f'{LEDGER}: no "## Entries" section')
        return set()
    body_line0 = head.count('\n') + 3   # 1-based, past the "## Entries" line

    # Split into entries at each `### ` heading, keeping line numbers.
    entries, cur = [], None
    for i, line in enumerate(body.split('\n')):
        if line.startswith('### '):
            cur = {'title': line[4:].strip(), 'line': body_line0 + i, 'lines': []}
            entries.append(cur)
        elif line.startswith('## '):
            cur = None
        elif cur is not None:
            cur['lines'].append(line)

    ids, used_known, fielded = [], set(), []
    for e in entries:
        where = f'{LEDGER}:{e["line"]} "{e["title"]}"'
        chunk = '\n'.join(e['lines'])
        found = re.findall(r'^- \*\*ID:\*\*\s*(DIV-\d{4})\b', chunk, re.M)
        if len(found) != 1:
            r.error(f'{where}: {len(found)} ID lines, want exactly 1'
                    + (f' ({", ".join(found)} - a heading missing?)' if found else ''))
        if not found:
            continue
        did = found[0]
        ids.append((did, where))
        fielded.append((did, where, chunk))
        for field in REQUIRED:
            pat = r'^- \*\*' + re.escape(field) + r'(?: \([^)]*\))?:?\*\*'
            if re.search(pat, chunk, re.M):
                continue
            if (did, field) in KNOWN:
                used_known.add((did, field))
                r.note(f'{did}: no "{field}" field (known: {KNOWN[did, field]})')
            else:
                r.error(f'{where}: {did} has no "- **{field}:**" field')
        m = re.search(r'^- \*\*Tier:\*\*\s*(.*)$', chunk, re.M)
        if m and not re.fullmatch(r'(' + '|'.join(TIERS) + r')(?: - .+)?', m.group(1).strip()):
            r.error(f'{where}: {did} tier {m.group(1).strip()!r} is not one of '
                    f'{", ".join(TIERS)} (optionally " - <why>")')
        m = re.search(r'^- \*\*Date:\*\*\s*(\S+)', chunk, re.M)
        if m and not re.fullmatch(r'\d{4}-\d{2}-\d{2}', m.group(1)):
            r.error(f'{where}: {did} date {m.group(1)!r} is not YYYY-MM-DD')

    for key in KNOWN.keys() - used_known:
        r.error(f'ledger_check.py KNOWN lists {key[0]} missing "{key[1]}", '
                'but it is there now - remove the KNOWN line')

    seen = {}
    for n, (did, where) in enumerate(ids, 1):
        if did in seen:
            r.error(f'{where}: {did} reused (first at {seen[did]})')
            continue
        seen[did] = where
        want = f'DIV-{n:04d}'
        if did != want:
            r.error(f'{where}: {did} out of sequence, expected {want}')
    last = ids[-1][0] if ids else None

    m = re.search(r'^\*\*Status:\*\*.*?(\d+) entries, DIV-0001\.\.(\d{4})', head, re.M)
    if not m:
        r.error(f'{LEDGER}: Status line does not say "N entries, DIV-0001..NNNN"')
    elif ids:
        n, top = int(m.group(1)), f'DIV-{m.group(2)}'
        if n != len(ids) or top != last:
            r.error(f'{LEDGER}: Status line says {n} entries to {top}; '
                    f'the ledger has {len(ids)} to {last}')
    check_switch_fields(r, fielded, set(seen), inv)
    selftest_fields(r, fielded, set(seen), inv)
    return set(seen)


# --- the switch fields -------------------------------------------------------

def id_ranges(ids):
    """DIV-0001, DIV-0002, DIV-0003, DIV-0007 -> 'DIV-0001..0003, DIV-0007'."""
    nums, out = sorted(int(x[4:]) for x in ids), []
    i = 0
    while i < len(nums):
        j = i
        while j + 1 < len(nums) and nums[j + 1] == nums[j] + 1:
            j += 1
        out.append(f'DIV-{nums[i]:04d}' + (f'..{nums[j]:04d}' if j > i else ''))
        i = j + 1
    return ', '.join(out)


def switch_token(inv, tok):
    """(class, Player value, error) for one `Switch:` name."""
    if tok == 'lang':
        return 'launcher', 'yes', None
    m = re.fullmatch(r'ini:([a-z_]+(?:\.[a-z_0-9]+)?)=(\S*)', tok)
    if m:
        key = m.group(1)
        if key == ledger_switches.PLAYER_INI:
            return None, None, f"ini:{key}= is the player's switch itself; `Switch:` names what it takes"
        if key not in inv.ini_keys and key.split('.')[0] + '.' not in inv.ini_keys:
            return None, None, f'ini key {key!r} is not one the launcher reads (src/launcher/config.cpp ConfigLoad)'
        return 'launcher', 'yes', None
    m = re.fullmatch(r'env:(BOF3X_[A-Z0-9_]+)=(\S*)', tok)
    if m:
        name, value = m.groups()
        if name == ledger_switches.PLAYER_SWITCH:
            return None, None, f"{name} is the player's switch itself; `Switch:` names what it takes"
        if name in ledger_switches.TOOLING_ENV:
            return None, None, (f'{name} is tooling, not a switch: a BOF3X_ORIGINAL name is written bare '
                                '(`SunderLoop`), and the fuzz and the trace switch no entry')
        if name not in inv.env_names:
            return None, None, f'nothing in src/ reads {name}'
        key = f'env:{name}={value}' if f'env:{name}={value}' in inv.names else f'env:{name}'
        return inv.cls(key), inv.player(key), None
    if re.fullmatch(r'[A-Za-z_]\w*', tok):
        cls = inv.cls(tok)
        if cls is None:
            return None, None, (f'nothing in src/ reads the name {tok!r} (not a PatchBytes, RetargetCall or '
                                'Inject name)')
        if cls == 'instrument':
            return None, None, f'{tok} is a probe (instrument), which no name switches off'
        return cls, inv.player(tok), None
    return None, None, (f'{tok!r} is not a switch name: a BOF3X_ORIGINAL name, ini:<key>=<value>, '
                        'env:<BOF3X_NAME>=<value> or lang')


def field_lines(chunk, name):
    """[value] for every `- **<name>:**` line of the entry: the text before an
    optional " - <clause>"."""
    return [m.group(1).split(' - ', 1)[0].strip()
            for m in re.finditer(r'^- \*\*' + re.escape(name) + r':\*\*[ \t]*(.*)$', chunk, re.M)]


def check_switch_fields(r, fielded, defined, inv, whole=True):
    rank = ledger_switches.PLAYER_RANK
    missing = {f: [] for f in SWITCH_FIELDS}
    filled, clash_seen = 0, set()
    for did, where, chunk in fielded:
        vals, default_clause = {}, ''
        for f in SWITCH_FIELDS:
            lines = field_lines(chunk, f)
            if (did, f) in FIELD_CLASH and lines:
                clash_seen.add((did, f))
                r.note(f'{did}: its "{f}:" line is not the switch field ({FIELD_CLASH[did, f]})')
                lines = lines[1:]
            if len(lines) > 1:
                r.error(f'{where}: {did} has {len(lines)} "{f}:" lines, want one')
            if not lines:
                missing[f].append(did)
                if STRICT_FIELDS:
                    r.error(f'{where}: {did} has no "- **{f}:**" field')
                continue
            vals[f] = lines[0]
            if f == 'Default':
                m = re.search(r'^- \*\*Default:\*\*[ \t]*\S+(?: - (.*))?$', chunk, re.M)
                default_clause = (m.group(1) or '').strip() if m else ''
        if len(vals) == len(SWITCH_FIELDS):
            filled += 1
        m = re.search(r'^- \*\*Tier:\*\*\s*(\w+)', chunk, re.M)
        tier = m.group(1) if m else None

        # Switch: none alone, or distinct names, each one src/ or the launcher reads.
        sw_player, sw_none, sw_lang = None, False, False
        if 'Switch' in vals:
            toks = [t.strip().strip('`') for t in vals['Switch'].split(',')]
            sw_lang = 'lang' in toks
            if toks == ['none']:
                sw_none = True
            elif 'none' in toks or '' in toks:
                r.error(f'{where}: {did} Switch {vals["Switch"]!r}: `none` stands alone, and no name is empty')
            else:
                if len(set(toks)) != len(toks):
                    r.error(f'{where}: {did} Switch names a switch twice')
                worst = None
                for t in toks:
                    cls, pv, err = switch_token(inv, t)
                    if err:
                        r.error(f'{where}: {did} Switch: {err}')
                        worst = None
                        break
                    if worst is None or rank[pv] > rank[worst]:
                        worst = pv
                sw_player = worst

        if 'Default' in vals and vals['Default'] not in ('on', 'off'):
            r.error(f'{where}: {did} Default {vals["Default"]!r} is not on or off')

        # Player: the value the Switch names' classes give, or never.
        if 'Player' in vals:
            pv = vals['Player']
            if pv not in PLAYER_VALUES:
                r.error(f'{where}: {did} Player {pv!r} is not one of {", ".join(PLAYER_VALUES)}')
            else:
                if tier == 'Forced' and pv != 'never':
                    r.error(f'{where}: {did} is Forced, which no preset can turn off: Player is never')
                if sw_none and pv != 'never':
                    r.error(f'{where}: {did} has Switch: none, so Player is never, not {pv}')
                if sw_player and pv != 'never' and pv != sw_player:
                    why = ("the player's build cannot reach every name of its Switch" if pv == 'yes'
                           else 'that is not what its Switch names give')
                    built = 'is' if inv.player_switch_built else 'is not yet'
                    r.error(f'{where}: {did} Player {pv}: {why} - they give {sw_player} '
                            f'(tools/ledger_switches.py; {ledger_switches.PLAYER_SWITCH} {built} read by src/)')
                if vals.get('Default') == 'off' and pv != 'yes' and vals.get('State') == 'active':
                    r.note(f'{did}: Default off and Player {pv}: a player who cannot switch it on never gets it')

        # Toward: none alone, or a list of distinct releases; no cascade.
        if 'Toward' in vals:
            toks = [t.strip() for t in vals['Toward'].split(',')]
            if toks != ['none'] and (not set(toks) <= set(TOWARD_VALUES) or len(set(toks)) != len(toks)):
                r.error(f'{where}: {did} Toward {vals["Toward"]!r}: none alone, or a list of distinct '
                        f'{", ".join(TOWARD_VALUES)}')

        # Languages: any alone, or distinct tags the engine knows and `original`;
        # the language pick governs an entry it limits, so `Switch: lang` names
        # some, and its `Default: on` is read under them.
        if 'Languages' in vals:
            toks = [t.strip().strip('`') for t in vals['Languages'].split(',')]
            if toks == ['any']:
                if sw_lang:
                    r.error(f'{where}: {did} has Switch: lang, so Languages names the languages it acts under, '
                            'not any')
            elif 'any' in toks or '' in toks:
                r.error(f'{where}: {did} Languages {vals["Languages"]!r}: `any` stands alone, and no tag is empty')
            elif len(set(toks)) != len(toks):
                r.error(f'{where}: {did} Languages names a language twice')
            elif not set(toks) <= set(LANGUAGE_VALUES):
                bad = ', '.join(t for t in toks if t not in LANGUAGE_VALUES)
                r.error(f'{where}: {did} Languages: {bad} is not a tag the engine knows - '
                        f'any, or a list of {", ".join(LANGUAGE_VALUES)} (tools/language_tags.py TAGS)')
            elif vals.get('Default') == 'on' and not default_clause.startswith(LANG_DEFAULT):
                r.error(f'{where}: {did} Languages {vals["Languages"]}: its Default reads '
                        f'"on - {LANG_DEFAULT}"')

        # State: active, withdrawn, or superseded by an entry that exists.
        if 'State' in vals:
            st = vals['State']
            m = re.fullmatch(r'superseded by (DIV-\d{4})', st)
            if st not in ('active', 'withdrawn') and not m:
                r.error(f'{where}: {did} State {st!r} is not active, withdrawn or superseded by DIV-NNNN')
            elif m and (m.group(1) not in defined or m.group(1) == did):
                r.error(f'{where}: {did} is superseded by {m.group(1)}, which is '
                        + ('itself' if m.group(1) == did else 'not in the ledger'))
            elif st == 'active' and re.search(r'\*\*Withdrawn\b|\bSUPERSEDED by\b', chunk):
                r.note(f"{did}: State active, and the entry's prose says withdrawn or superseded - check")

    for key in (FIELD_CLASH.keys() - clash_seen) if whole else ():
        r.error(f'ledger_check.py FIELD_CLASH lists the "{key[1]}:" line of {key[0]}, which is gone - '
                'remove the FIELD_CLASH line')
    for f in SWITCH_FIELDS:
        if missing[f] and not STRICT_FIELDS:
            r.note(f'{len(missing[f])} entries have no "{f}:" field (the ledger-fields round\'s stage B '
                   f'fills them): {id_ranges(missing[f])}')
    r.note(f'{filled} of {len(fielded)} entries carry all {len(SWITCH_FIELDS)} switch fields')


def check_citations(r, defined):
    cited = {}   # DIV id -> where it is first cited
    for rel, path in walk('.', CITE_EXTS):
        if rel == LEDGER:
            continue
        try:
            with open(path, encoding='utf-8') as f:
                lines = f.read().split('\n')
        except UnicodeDecodeError:
            continue
        for i, line in enumerate(lines, 1):
            for m in re.finditer(r'\bDIV-(\d{4})\b', line):
                cited.setdefault(f'DIV-{m.group(1)}', f'{rel}:{i}')
    for did, where in sorted(cited.items()):
        if did not in defined:
            r.error(f'{where}: cites {did}, which the ledger does not have')
    code = set()
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            code.update(f'DIV-{x}' for x in re.findall(r'\bDIV-(\d{4})\b', f.read()))
    quiet = sorted(defined - code)
    if quiet:
        r.note(f'{len(quiet)} entries not cited in src/ (data-only or tooling '
               f'divergences are expected here): {", ".join(quiet)}')


# The worked examples as fixtures: DIV-0176 (a code name, cheap) and DIV-0183
# (a detour name, costly) as the ledger has them must pass, and each case
# below - one field line changed - must give the error it names (None: none).
FIXTURE_IDS = ('DIV-0176', 'DIV-0183')
FIXTURE_CASES = [
    ('DIV-0176', {}, None),
    ('DIV-0183', {}, None),
    ('DIV-0176', {'Switch': 'NoSuchSwitchName'}, 'nothing in src/ reads the name'),
    ('DIV-0176', {'Switch': 'SunderLoop, SunderLoop'}, 'names a switch twice'),
    ('DIV-0176', {'Switch': 'none, SunderLoop'}, '`none` stands alone'),
    ('DIV-0176', {'Switch': 'none'}, 'Switch: none, so Player is never'),
    ('DIV-0176', {'Switch': 'none', 'Player': 'never'}, None),
    ('DIV-0176', {'Switch': 'InputScript'}, 'a probe'),
    ('DIV-0176', {'Switch': 'env:BOF3X_OFF=SunderLoop'}, "the player's switch itself"),
    ('DIV-0176', {'Switch': 'ini:off=SunderLoop'}, "the player's switch itself"),
    ('DIV-0176', {'Switch': 'env:BOF3X_SHADOW=sound'}, 'is tooling'),
    ('DIV-0176', {'Switch': 'env:BOF3X_NO_SUCH_NAME=1'}, 'nothing in src/ reads BOF3X_NO_SUCH_NAME'),
    ('DIV-0176', {'Switch': 'ini:no_such_key=1'}, 'not one the launcher reads'),
    ('DIV-0176', {'Switch': 'sunder loop'}, 'is not a switch name'),
    ('DIV-0176', {'Player': 'yes'}, "cannot reach every name"),
    ('DIV-0176', {'Player': 'costly'}, 'not what its Switch names give'),
    ('DIV-0176', {'Player': 'maybe'}, 'is not one of'),
    ('DIV-0176', {'Switch': 'ini:wide=0', 'Player': 'yes'}, None),
    ('DIV-0176', {'Switch': 'lang', 'Player': 'yes', 'Languages': 'en-US, en-150, fr-FR, de-DE, ja-JP',
                  'Default': 'on - under its languages'}, None),
    ('DIV-0176', {'Switch': 'lang, SunderLoop', 'Player': 'yes'}, 'they give cheap'),
    ('DIV-0176', {'Switch': 'lang, SunderLoop', 'Player': 'cheap', 'Languages': 'fr-FR',
                  'Default': 'on - under its languages'}, None),
    ('DIV-0176', {'Switch': 'env:BOF3X_MP3=original', 'Player': 'costly'}, None),
    ('DIV-0176', {'Switch': 'env:BOF3X_MP3=original', 'Player': 'yes'}, 'cannot reach every name'),
    ('DIV-0176', {'Tier': 'Forced'}, 'is Forced'),
    ('DIV-0176', {'Tier': 'Forced', 'Player': 'never'}, None),
    ('DIV-0176', {'Default': 'maybe'}, 'is not on or off'),
    ('DIV-0176', {'Toward': 'psx, psp'}, None),
    ('DIV-0176', {'Toward': 'psx, psx'}, 'list of distinct'),
    ('DIV-0176', {'Toward': 'none, psx'}, 'list of distinct'),
    ('DIV-0176', {'Toward': 'ps2'}, 'list of distinct'),
    ('DIV-0176', {'State': 'superseded by DIV-0183'}, None),
    ('DIV-0176', {'State': 'superseded by DIV-0176'}, 'which is itself'),
    ('DIV-0176', {'State': 'superseded by <past the last>'}, 'not in the ledger'),
    ('DIV-0176', {'State': 'retired'}, 'is not active, withdrawn'),
    ('DIV-0176', {'+Switch': 'SunderLoop'}, 'want one'),
    ('DIV-0183', {'Player': 'cheap'}, 'they give costly'),
    ('DIV-0183', {'Player': 'never'}, None),
    ('DIV-0176', {'-State': ''}, 'has no "- **State:**" field'),
    ('DIV-0183', {'-Toward': ''}, 'has no "- **Toward:**" field'),
    ('DIV-0176', {'Languages': 'xx-XX'}, 'not a tag the engine knows'),
    ('DIV-0176', {'Languages': 'en'}, 'not a tag the engine knows'),
    ('DIV-0176', {'Switch': 'lang', 'Player': 'yes'}, 'Switch: lang, so Languages'),
    ('DIV-0176', {'Switch': 'lang', 'Player': 'yes', 'Languages': 'ja-JP',
                  'Default': 'on - under its languages'}, None),
    ('DIV-0176', {'Languages': 'en-US, en-150, fr-FR, de-DE', 'Default': 'on - under its languages; why'}, None),
    ('DIV-0176', {'Languages': 'original', 'Default': 'off'}, None),
    ('DIV-0176', {'Languages': 'ja-JP'}, 'its Default reads'),
    ('DIV-0176', {'Languages': 'ja-JP, ja-JP'}, 'names a language twice'),
    ('DIV-0176', {'Languages': 'any, ja-JP'}, '`any` stands alone'),
    ('DIV-0183', {'-Languages': ''}, 'has no "- **Languages:**" field'),
]


def fixture_chunk(chunk, changes):
    for f, v in changes.items():
        if f.startswith('+'):
            chunk += f'\n- **{f[1:]}:** {v}'
            continue
        if f.startswith('-'):
            pat = re.compile(r'^- \*\*' + re.escape(f[1:]) + r':\*\*.*\n?', re.M)
            if not pat.search(chunk):
                raise KeyError(f[1:])
            chunk = pat.sub('', chunk, count=1)
            continue
        pat = re.compile(r'^- \*\*' + re.escape(f) + r':\*\*.*$', re.M)
        if not pat.search(chunk):
            raise KeyError(f)
        chunk = pat.sub(lambda m: f'- **{f}:** {v}', chunk, count=1)
    return chunk


def selftest_fields(r, fielded, defined, inv):
    have = {did: chunk for did, _, chunk in fielded if did in FIXTURE_IDS}
    for did in FIXTURE_IDS:
        if did not in have or any(not field_lines(have[did], f) for f in SWITCH_FIELDS):
            r.error(f'{did} is the switch fields\' worked example and must carry all {len(SWITCH_FIELDS)} '
                    '(ledger_check.py FIXTURE_IDS)')
            return
    past = f'DIV-{max(int(x[4:]) for x in defined) + 1:04d}'
    for did, changes, want in FIXTURE_CASES:
        changes = {f: v.replace('<past the last>', past) for f, v in changes.items()}
        t = Report()
        try:
            chunk = fixture_chunk(have[did], changes)
        except KeyError as e:
            r.error(f'self-test: {did} has no "{e.args[0]}:" line to change')
            continue
        check_switch_fields(t, [(did, f'self-test {did} {changes}', chunk)], defined, inv, whole=False)
        if want is None and t.errors:
            r.error(f'self-test: {did} with {changes} should pass, gave: {t.errors[0]}')
        elif want is not None and not any(want in e for e in t.errors):
            r.error(f'self-test: {did} with {changes} should give "{want}", gave '
                    + (repr(t.errors[0]) if t.errors else 'no error'))
    r.note(f'switch fields self-test: {len(FIXTURE_CASES)} cases on {", ".join(FIXTURE_IDS)}')


# --- the ownership ledger ----------------------------------------------------

INJECT_MACRO = re.compile(r'\bBOF3_INJECT\(\s*(\w+)\s*\)')
INJECT_DIRECT = re.compile(r'\bInject\(\s*"(\w+)"\s*,\s*(?:::)?bof3::addr::(\w+)\s*,')


def check_ownership(r):
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
        t = tomllib.load(f)
    impl = {}
    for e in t.get('func', []):
        if 'impl' in e:
            impl[e['name']] = e['impl']
    for kind in ('data', 'block'):
        for e in t.get(kind, []):
            if 'impl' in e:
                r.error(f'symbols.toml: [[{kind}]] {e["name"]} has an impl; only functions can')

    injected = {}
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            src = f.read()
        # The macro's own #define is not a use of it. Blanked, not removed, so
        # the line numbers below stay right.
        src = re.sub(r'^\s*#define[^\n]*(?:\\\n[^\n]*)*', lambda m: '\n' * m.group(0).count('\n'),
                     src, flags=re.M)
        hits = [(m.start(), m.group(1)) for m in INJECT_MACRO.finditer(src)]
        for m in INJECT_DIRECT.finditer(src):   # the call may wrap lines
            if m.group(1) != m.group(2):
                r.error(f'{rel}:{src.count(chr(10), 0, m.start()) + 1}: Inject names '
                        f'"{m.group(1)}" but passes bof3::addr::{m.group(2)}')
            hits.append((m.start(), m.group(2)))
        for pos, name in sorted(hits):
            where = f'{rel}:{src.count(chr(10), 0, pos) + 1}'
            if name in injected:
                r.error(f'{where}: {name} detoured twice (also {injected[name]})')
            else:
                injected[name] = where

    for name, where in sorted(injected.items()):
        if name not in impl:
            r.error(f'{where}: {name} is detoured to ours but symbols.toml has no impl for it')
    for name, path in sorted(impl.items()):
        if name not in injected:
            r.error(f'symbols.toml: {name} has impl = "{path}" but nothing in src/ detours it')
        if not os.path.isfile(os.path.join(ROOT, path)):
            r.error(f'symbols.toml: {name} impl file {path} does not exist')
    r.note(f'{len(impl)} impl lines, {len(injected)} functions detoured')


# --- rule 4 ------------------------------------------------------------------

def check_stubs(r):
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            for i, line in enumerate(f, 1):
                m = STUB_MARKERS.search(line)
                if m:
                    r.error(f'{rel}:{i}: "{m.group(0)}" - CLAUDE.md rule 4: '
                            'implement it fully or abort loudly')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--verbose', '-v', action='store_true')
    a = ap.parse_args()

    r = Report()
    inv = ledger_switches.inventory()
    for p in inv.problems:
        r.error(f'tools/ledger_switches.py: {p}')
    defined = check_divergence(r, inv)
    check_citations(r, defined)
    check_ownership(r)
    check_stubs(r)

    if a.verbose:
        for n in r.notes:
            print(f'note: {n}')
    for e in r.errors:
        print(f'error: {e}')
    print(f'ledger_check: {len(defined)} ledger entries, '
          f'{len(r.errors)} error(s), {len(r.notes)} note(s)')
    return 1 if r.errors else 0


if __name__ == '__main__':
    sys.exit(main())
