"""What a setup can take from the previous cache instead of making again, and
what a release's tools would make differently (IDEAS I42, docs/install-sources.md
section 5; HANDOFF item 11, docs/install-staleness.md).

Each piece of the cache - a step: the base chunks, base/exe, base/bgm, base/snd,
each loc/<tag>, each opt/ layer, the manifest, the movie, the game folder - is
built inside a `Step`, which MEASURES what the step reads and keeps it in
<cache>/build-keys/<step>.json:

  code     every function of tools/ that ran during the step, by file and
           qualified name, with a digest of its compiled body (no line
           numbers, so an edit elsewhere in the file does not count).
           Measured with sys.monitoring's PY_START, each function once
           (DISABLE after its first call), so it costs next to nothing.
  globals  the module-level values of tools/ those functions name (a table
           like psp_exe.TWINS, a constant): every data-like global of a
           loaded tools module whose name a recorded function's code names,
           its value digested in a fresh interpreter (so a memo's contents,
           filled during the build, never count).
  data     every file of the release's data (SHIPPED_DATA: recipes/,
           fixtures/, ...) the step opened, with its sha256 - by an audit hook.
  inputs   what the caller says the step draws on: the source discs, the
           player's ffmpeg, digests of earlier steps' output.
  python   the interpreter's version.

A child Python a step starts (loc_build.py) records into the same step: the
step names a file in BOF3X_STEP_RECORD, and this module, imported by the
child, appends the child's run to it at exit.

What a memo carries from one step to the next would be read without being
measured, so every step starts with the memos of tools/ emptied: each
functools.lru_cache, and the module-level memos in MEMOS. `python
tools/build_reuse.py lint` finds a module-level global the setup's tools
write at run time that MEMOS does not list (tools/importer.py check runs it).

A step's record HOLDS when the python and the inputs are the same and every
function, global and data file it lists still digests the same. Then setup
takes the previous cache's piece instead of building it (`Step.previous`), and
`importer.py stale` reports it fresh for this release. A function the step did
not run cannot change what it made unless a function it did run changed to
call it - which that function's own digest catches.

Nothing installs or walks build-keys/."""
import ast
import atexit
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

_TOOLS = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_TOOLS)

KEYS_DIR = "build-keys"
RECORD_ENV = "BOF3X_STEP_RECORD"
FORMAT = 1                      # build-keys/<step>.json's shape; another number never holds

# What the tools read relative to ROOT (tools/make_release.py ships these as
# runtime data; measured 2026-10-15 by an audit hook on every open() under ROOT
# during setups, docs/install-sources.md section 6). Directories whole.
SHIPPED_DATA = ["fixtures.toml", "fixtures", "recipes", "symbols.toml", "tables.toml", "exe_maps",
                "src/hook/exe_own.cpp"]

# The module-level memos of the setup's tools, emptied at each step's start:
# (module, global, its empty value). `lint` finds one missing.
MEMOS = [("cache_inventory", "_psp_table", None), ("psp_exe", "_BASE_IMAGE", dict),
         ("psp_standins", "_table", None), ("region_diff", "_CODE_MEMO", dict), ("vag", "_CACHE", dict)]
# Run-time-written globals of tools that are no memo of a build step: loc_build.py
# runs in a process of its own for each step; the others are not the setup's.
LINT_EXEMPT = {"loc_build", "attract_run", "band_rows", "close_probe", "input_run", "magic_rows", "scenario_rows",
               "statehash", "build_reuse", "data_census"}

_MON_ID = 4                     # a sys.monitoring tool id no debugger, coverage or profiler takes
_CO_FUNCTION = 0x0001 | 0x0002  # CO_OPTIMIZED | CO_NEWLOCALS: a function's body, not a module's or class's


def _rel(path):
    """`path` relative to ROOT with forward slashes, or None outside it."""
    try:
        r = os.path.relpath(os.path.abspath(path), _ROOT)
    except (TypeError, ValueError):     # another drive, or not a path
        return None
    if r == ".." or r.startswith(".." + os.sep):
        return None
    return r.replace(os.sep, "/")


def _is_data(rel):
    return any(rel == d or rel.startswith(d + "/") for d in SHIPPED_DATA)


def _sha_file(path):
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


# ---------------------------------------------------------------- code digests

def _code_digest(co, h=None):
    """A code object's behaviour without its position: bytecode, names,
    constants (a nested function by its qualified name - it has a record of
    its own if it ran; other nested code by its contents), flags and argument
    shape, the exception table."""
    top = h is None
    h = h or hashlib.sha256()
    for part in (co.co_code, co.co_exceptiontable):
        h.update(len(part).to_bytes(4, "little") + part)
    h.update(repr((co.co_names, co.co_varnames, co.co_freevars, co.co_cellvars, co.co_argcount,
                   co.co_posonlyargcount, co.co_kwonlyargcount, co.co_flags)).encode())
    for c in co.co_consts:
        if hasattr(c, "co_code"):
            if c.co_flags & _CO_FUNCTION:
                h.update(b"fn:" + c.co_qualname.encode())
            else:
                h.update(b"code:")
                _code_digest(c, h)
        else:
            h.update(b"k:" + repr((type(c).__name__, c)).encode())
    return h.hexdigest() if top else None


def _names(co):
    """Every name a code object's own code (and its non-function nested code) uses."""
    out = set(co.co_names)
    for c in co.co_consts:
        if hasattr(c, "co_code") and not c.co_flags & _CO_FUNCTION:
            out |= _names(c)
    return out


_file_cache = {}


def _file_code(rel):
    """{qualname: [(digest, names)]} for every function in ROOT/rel; None when
    the file is gone or does not compile."""
    if rel not in _file_cache:
        try:
            with open(os.path.join(_ROOT, rel), "rb") as fh:
                mod = compile(fh.read(), rel, "exec", dont_inherit=True)
        except (OSError, SyntaxError, ValueError):
            _file_cache[rel] = None
            return None
        out, todo = {}, [mod]
        while todo:
            co = todo.pop()
            for c in co.co_consts:
                if hasattr(c, "co_code"):
                    if c.co_flags & _CO_FUNCTION:
                        out.setdefault(c.co_qualname, []).append((_code_digest(c), _names(c)))
                    todo.append(c)
        _file_cache[rel] = out
    return _file_cache[rel]


# ---------------------------------------------------------------- global values

def _canon(v, depth=0):
    """A data-like value as a canonical, hashable text; None for anything else
    (a function, a class, a module, an open file)."""
    if depth > 50:
        return None
    if v is None or isinstance(v, (bool, int, float, complex, str, bytes)):
        return repr((type(v).__name__, v))
    if isinstance(v, re.Pattern):
        return repr(("re", v.pattern, v.flags))
    if isinstance(v, (list, tuple)):
        parts = [_canon(x, depth + 1) for x in v]
        return None if None in parts else "%s[%s]" % (type(v).__name__, ",".join(parts))
    if isinstance(v, (set, frozenset)):
        parts = [_canon(x, depth + 1) for x in v]
        return None if None in parts else "set[%s]" % ",".join(sorted(parts))
    if isinstance(v, dict):
        parts = [(_canon(k, depth + 1), _canon(x, depth + 1)) for k, x in v.items()]
        if any(a is None or b is None for a, b in parts):
            return None
        return "dict{%s}" % ",".join("%s:%s" % p for p in sorted(parts))
    return None


def _globals_here(wanted):
    """{module: {name: digest}} for the data-like globals named, in THIS
    interpreter (a fresh one: _globals). A module that will not import, or a
    name that is no longer there or not data-like, gets "gone"."""
    sys.path.insert(0, _TOOLS)
    out = {}
    for mod, names in wanted.items():
        try:
            m = __import__(mod)
        except Exception:
            out[mod] = {n: "gone" for n in names}
            continue
        row = out.setdefault(mod, {})
        for n in names:
            c = _canon(getattr(m, n)) if hasattr(m, n) else None
            row[n] = hashlib.sha256(c.encode()).hexdigest() if c is not None else "gone"
    return out


def _globals(wanted):
    """_globals_here in a fresh interpreter: the values as the modules define
    them, not as a build has filled them."""
    if not wanted:
        return {}
    env = dict(os.environ)
    env.pop(RECORD_ENV, None)
    r = subprocess.run([sys.executable, os.path.abspath(__file__), "globals"], input=json.dumps(wanted),
                       capture_output=True, text=True, env=env)
    if r.returncode:
        raise RuntimeError("build_reuse globals failed:\n%s%s" % (r.stdout, r.stderr))
    return json.loads(r.stdout)


def _data_globals(mods):
    """{module: set of its data-like global names} in a fresh interpreter's
    view is _globals' job; here the candidates: every top-level assignment
    target of each module's source."""
    out = {}
    for mod in mods:
        try:
            with open(os.path.join(_TOOLS, mod + ".py"), encoding="utf-8") as fh:
                tree = ast.parse(fh.read())
        except (OSError, SyntaxError, ValueError):
            out[mod] = set()
            continue
        names = set()
        for n in tree.body:
            targets = n.targets if isinstance(n, ast.Assign) else [n.target] if isinstance(n, (ast.AnnAssign,
                                                                                             ast.AugAssign)) else []
            for t in targets:
                for x in ast.walk(t):
                    if isinstance(x, ast.Name):
                        names.add(x.id)
        out[mod] = names
    return out


# ---------------------------------------------------------------- recording

def _reset_memos():
    """Every memo of a loaded tools module emptied (module docstring)."""
    for name, m in list(sys.modules.items()):
        f = getattr(m, "__file__", None)
        if not f or _rel(f) is None or not _rel(f).startswith("tools/"):
            continue
        for v in list(vars(m).values()):
            for fn in [v] + (list(vars(v).values()) if isinstance(v, type) else []):
                if hasattr(fn, "cache_clear") and hasattr(fn, "cache_info"):
                    fn.cache_clear()
    for mod, name, empty in MEMOS:
        m = sys.modules.get(mod)
        if m is not None and hasattr(m, name):
            setattr(m, name, empty() if empty else None)


class _Recorder:
    """The code that ran, the tools modules loaded and the data files opened, while on."""

    def __init__(self):
        self.on = False
        self.code = set()       # (rel, qualname)
        self.data = set()       # rel
        self._hooked = False

    def start(self):
        if not self._hooked:
            sys.addaudithook(self._audit)       # cannot be removed: `on` gates it
            self._hooked = True
        mon = sys.monitoring
        if mon.get_tool(_MON_ID) is None:
            mon.use_tool_id(_MON_ID, "bof3x build_reuse")
            mon.register_callback(_MON_ID, mon.events.PY_START, self._py_start)
        mon.restart_events()                    # a function seen by an earlier step fires again
        mon.set_events(_MON_ID, mon.events.PY_START)
        self.on = True

    def stop(self):
        self.on = False
        sys.monitoring.set_events(_MON_ID, 0)
        code, data = self.code, self.data
        self.code, self.data = set(), set()
        return code, data, _loaded_tools()

    def _py_start(self, co, offset):
        rel = _rel(co.co_filename) if self.on else None
        # not this module: the measuring (its audit hook runs on every open), not the making; FORMAT covers it
        if rel and rel.startswith("tools/") and rel.endswith(".py") and rel != "tools/build_reuse.py":
            self.code.add((rel, co.co_qualname))
        return sys.monitoring.DISABLE

    def _audit(self, event, args):
        if not self.on or event != "open" or not args or not isinstance(args[0], (str, bytes, os.PathLike)):
            return
        mode = args[1] if len(args) > 1 and isinstance(args[1], str) else "r"
        if any(m in mode for m in "wax+"):
            return
        try:
            rel = _rel(os.fsdecode(args[0]))
        except (TypeError, ValueError):
            return
        if rel and _is_data(rel) and os.path.isfile(os.path.join(_ROOT, rel)):
            self.data.add(rel)


def _loaded_tools():
    out = set()
    for name, m in list(sys.modules.items()):
        f = getattr(m, "__file__", None)
        rel = _rel(f) if f else None
        if rel and rel.startswith("tools/") and rel.endswith(".py") and "/" not in rel[6:]:
            out.add(rel[6:-3])
    return out


_rec = _Recorder()


def _record(code, data, mods, inputs):
    """The JSON record of a measured step."""
    by_file = {}
    for rel, q in sorted(code):
        by_file.setdefault(rel, set()).add(q)
    out_code, used = {}, set()
    for rel, quals in by_file.items():
        now = _file_code(rel)
        if now is None:
            continue
        row = out_code.setdefault(rel, {})
        for q in sorted(quals):
            row[q] = sorted({d for d, _ in now.get(q, [])})
            for _, names in now.get(q, []):
                used |= names
    cand = _data_globals(sorted(mods))
    wanted = {m: sorted(n & used) for m, n in cand.items() if n & used}
    return {"format": FORMAT, "python": sys.version, "inputs": inputs, "code": out_code,
            "globals": _globals(wanted),
            "data": {rel: _sha_file(os.path.join(_ROOT, rel)) for rel in sorted(data)}}


def inputs_digest(*parts):
    return hashlib.sha256("\0".join(str(p) for p in parts).encode()).hexdigest()


def why_stale(rec, inputs=None, globals_now=None):
    """None when `rec` (a build-keys record) holds for this release's tools and
    data - and, given `inputs`, for these inputs; else the first reason, one
    short phrase. `globals_now`: the current values of rec's globals, when the
    caller has them (stale() asks once for every record)."""
    if not isinstance(rec, dict) or rec.get("format") != FORMAT:
        return "built by an older release (no measured record)"
    if rec.get("python") != sys.version:
        return "another Python (%s)" % sys.version.split()[0]
    if inputs is not None and rec.get("inputs") != inputs:
        return "other sources or tools given"
    for rel, quals in rec.get("code", {}).items():
        now = _file_code(rel)
        if now is None:
            return "%s is gone" % rel
        for q, digests in quals.items():
            if not set(digests) <= {d for d, _ in now.get(q, [])}:
                return "%s changed (%s)" % (rel, q)
    want = rec.get("globals", {})
    now = globals_now if globals_now is not None else _globals({m: sorted(n) for m, n in want.items()})
    for mod, row in want.items():
        for n, h in row.items():
            if now.get(mod, {}).get(n) != h:
                return "tools/%s.py changed (%s)" % (mod, n)
    for rel, h in rec.get("data", {}).items():
        p = os.path.join(_ROOT, rel)
        if not os.path.isfile(p) or _sha_file(p) != h:
            return "%s changed" % rel
    return None


def read_record(cache, step):
    try:
        with open(os.path.join(cache, KEYS_DIR, step + ".json"), encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


class Step:
    """One cache piece built under measurement:

        with Step(out, reuse, "snd", disc.id, version) as st:
            src = st.previous(os.path.join("base", "snd"))
            if src:
                ...copy it...            # the previous record is kept
            else:
                ...build it...           # what ran and what was read is recorded

    The record is written to <out>/build-keys/<name>.json when the block ends
    without an exception. Steps do not nest."""

    def __init__(self, out, reuse, name, *inputs):
        self.out, self.reuse, self.name = out, reuse, name
        self.inputs = inputs_digest(*inputs)
        self.adopted = None
        self.child = None

    def __enter__(self):
        if _rec.on:
            raise RuntimeError("build_reuse.Step %s inside another step" % self.name)
        fd, self.child = tempfile.mkstemp(prefix="bof3x-step-", suffix=".jsonl")
        os.close(fd)
        self._env = os.environ.get(RECORD_ENV)
        os.environ[RECORD_ENV] = self.child
        _reset_memos()
        _rec.start()
        return self

    def previous(self, rel=None):
        """The previous cache's `rel` folder (or True with no `rel`) when its
        record for this step holds; else None. Holding, its record is the one
        this step keeps."""
        if not self.reuse:
            return None
        rec = read_record(self.reuse, self.name)
        if rec is None or why_stale(rec, self.inputs) is not None:
            return None
        src = os.path.join(self.reuse, rel) if rel is not None else None
        if src is not None and not os.path.isdir(src):
            return None
        self.adopted = rec
        return src if src is not None else True

    def __exit__(self, kind, value, tb):
        code, data, mods = _rec.stop()
        if self._env is None:
            os.environ.pop(RECORD_ENV, None)
        else:
            os.environ[RECORD_ENV] = self._env
        try:
            with open(self.child, encoding="utf-8") as fh:
                for line in fh:
                    got = json.loads(line)
                    code |= {tuple(x) for x in got["code"]}
                    data |= set(got["data"])
                    mods |= set(got["modules"])
        except (OSError, ValueError):
            pass
        os.remove(self.child)
        if kind is not None:
            return False
        rec = dict(self.adopted, inputs=self.inputs) if self.adopted else _record(code, data, mods, self.inputs)
        os.makedirs(os.path.join(self.out, KEYS_DIR), exist_ok=True)
        with open(os.path.join(self.out, KEYS_DIR, self.name + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            json.dump(rec, fh, indent=1, sort_keys=True)
            fh.write("\n")
        return False


def _child_start():
    """In a Python a step started: record this run into the step's file."""
    path = os.environ.get(RECORD_ENV)
    if not path or _rec.on or __name__ == "__main__":
        return
    _rec.start()

    def done():
        code, data, mods = _rec.stop()
        with open(path, "a", encoding="utf-8") as fh:
            fh.write(json.dumps({"code": sorted(code), "data": sorted(data), "modules": sorted(mods)}) + "\n")
    atexit.register(done)


_child_start()


# ---------------------------------------------------------------- the rest

def tree_digest(d):
    """Every file under `d` by its path and content, or "none"."""
    if not os.path.isdir(d):
        return "none"
    h = hashlib.sha256()
    for root, dirs, files in os.walk(d):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(root, f)
            h.update(os.path.relpath(p, d).replace(os.sep, "/").encode() + b"\0")
            with open(p, "rb") as fh:
                h.update(hashlib.sha256(fh.read()).digest())
    return h.hexdigest()


def write_rows(out, step, rows):
    """A step's manifest rows beside its record: what reusing it writes."""
    os.makedirs(os.path.join(out, KEYS_DIR), exist_ok=True)
    with open(os.path.join(out, KEYS_DIR, step + ".rows.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump([list(r) for r in rows], fh)


def read_rows(reuse, step):
    try:
        with open(os.path.join(reuse, KEYS_DIR, step + ".rows.json"), encoding="utf-8") as fh:
            return [tuple(r) for r in json.load(fh)]
    except (OSError, ValueError):
        return None


# Steps every cache the measured importer builds has a record of.
REQUIRED = ("base", "manifest")


def stale(cache):
    """[(step, reason)] for every piece of `cache` whose record does not hold
    for this release - its own code, globals or data changed (the inputs are
    the player's and are not re-measured here). A cache without the measured
    records - built before them, or a build that stopped part-way - is
    reported whole, as "cache"."""
    d = os.path.join(cache, KEYS_DIR)
    names = sorted(os.listdir(d)) if os.path.isdir(d) else []
    steps = [n[:-5] for n in names if n.endswith(".json") and not n.endswith(".rows.json")]
    if any(r not in steps for r in REQUIRED) or any(n.endswith(".txt") for n in names):
        return [("cache", "built by an older release (no measured record)")]
    recs = {s: read_record(cache, s) for s in steps}
    wanted = {}
    for rec in recs.values():
        for mod, row in (rec or {}).get("globals", {}).items():
            wanted.setdefault(mod, set()).update(row)
    now = _globals({m: sorted(n) for m, n in wanted.items()})
    out = []
    for step, rec in sorted(recs.items()):
        why = why_stale(rec, globals_now=now)
        if why:
            out.append((step, why))
    return out


# ---------------------------------------------------------------- lint

def lint():
    """Module-level globals the setup's tools write at run time (a `global`
    statement, an item or method store on a module-level name) that MEMOS
    does not list: [(module, name)]."""
    listed = {(m, n) for m, n, _ in MEMOS}
    out = []
    for f in sorted(os.listdir(_TOOLS)):
        mod = f[:-3]
        if not f.endswith(".py") or mod in LINT_EXEMPT:
            continue
        with open(os.path.join(_TOOLS, f), encoding="utf-8") as fh:
            tree = ast.parse(fh.read())
        top = set()
        for n in tree.body:
            if isinstance(n, ast.Assign):
                top |= {t.id for t in n.targets if isinstance(t, ast.Name)}
        written = set()
        for fn in ast.walk(tree):
            if not isinstance(fn, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            for n in ast.walk(fn):
                if isinstance(n, ast.Global):
                    written |= set(n.names)
                targets = n.targets if isinstance(n, ast.Assign) else [n.target] if isinstance(n, ast.AugAssign) else []
                for t in targets:
                    if isinstance(t, ast.Subscript) and isinstance(t.value, ast.Name) and t.value.id in top:
                        written.add(t.value.id)
                if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and isinstance(n.func.value, ast.Name) \
                        and n.func.value.id in top and n.func.attr in ("append", "add", "update", "setdefault",
                                                                        "extend", "insert", "pop", "clear"):
                    written.add(n.func.value.id)
                # a module-level mutable handed to a call (vag's bank(..., _CACHE))
                if isinstance(n, ast.Call):
                    for x in n.args:
                        if isinstance(x, ast.Name) and x.id in top and x.id.startswith("_") and x.id.isupper():
                            written.add(x.id)
        out += [(mod, n) for n in sorted(written) if (mod, n) not in listed]
    return out


def main(argv):
    if argv[:1] == ["globals"]:
        json.dump(_globals_here(json.load(sys.stdin)), sys.stdout)
        return 0
    if argv[:1] == ["lint"]:
        bad = lint()
        for mod, n in bad:
            print("tools/%s.py: %s is written at run time and is not in build_reuse.MEMOS (a memo a step would "
                  "read unmeasured); list it there, or in LINT_EXEMPT if no build step reaches it" % (mod, n))
        return 1 if bad else 0
    if argv[:1] == ["stale"] and len(argv) == 2:
        for step, why in stale(argv[1]):
            print("%s: %s" % (step, why))
        return 0
    print("usage: build_reuse.py lint | stale CACHE | globals (stdin: {module: [name]})", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
