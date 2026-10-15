"""What a setup can take from the previous cache instead of making again
(IDEAS I42, docs/install-sources.md section 5): a step's key - a hash of
everything it reads - kept in <cache>/build-keys/, and the helpers that
compare one with the previous cache's. tools/importer.py (the movie, the PSX
sound effects, the language layers) and tools/psp_layers.py (the PSP's
layers) use them.

A key is made from the tools, recipes and fixtures as they are now and the
Python running them, so a key made under another release never matches and a
new release builds everything once; then from what the step draws on. Nothing
installs or walks build-keys/."""
import hashlib
import json
import os
import sys

_TOOLS = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_TOOLS)

KEYS_DIR = "build-keys"
_fingerprint = []


def code_fingerprint():
    """The tools, recipes and fixtures, and the Python running them: a key
    made under another release never matches."""
    if not _fingerprint:
        h = hashlib.sha256(sys.version.encode())
        for top in (_TOOLS, os.path.join(_ROOT, "recipes"), os.path.join(_ROOT, "fixtures")):
            for root, dirs, files in os.walk(top):
                dirs[:] = sorted(d for d in dirs if d != "__pycache__")
                for f in sorted(files):
                    if top == _TOOLS and not f.endswith(".py"):
                        continue
                    p = os.path.join(root, f)
                    h.update(os.path.relpath(p, _ROOT).replace(os.sep, "/").encode() + b"\0")
                    with open(p, "rb") as fh:
                        h.update(hashlib.sha256(fh.read()).digest())
        _fingerprint.append(h.hexdigest())
    return _fingerprint[0]


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


def reuse_key(*parts):
    return hashlib.sha256("\0".join((code_fingerprint(),) + tuple(str(p) for p in parts)).encode()).hexdigest()


def reusable(reuse, step, key, rel):
    """The previous cache's `rel` folder when its step was built under the
    same key, else None."""
    if not reuse:
        return None
    try:
        with open(os.path.join(reuse, KEYS_DIR, step + ".txt"), encoding="utf-8") as fh:
            same = fh.read().strip() == key
    except OSError:
        return None
    src = os.path.join(reuse, rel)
    return src if same and os.path.isdir(src) else None


def write_key(out, step, key):
    os.makedirs(os.path.join(out, KEYS_DIR), exist_ok=True)
    with open(os.path.join(out, KEYS_DIR, step + ".txt"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(key + "\n")


def write_rows(out, step, rows):
    """A step's manifest rows beside its key: what reusing it writes."""
    with open(os.path.join(out, KEYS_DIR, step + ".rows.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump([list(r) for r in rows], fh)


def read_rows(reuse, step):
    try:
        with open(os.path.join(reuse, KEYS_DIR, step + ".rows.json"), encoding="utf-8") as fh:
            return [tuple(r) for r in json.load(fh)]
    except (OSError, ValueError):
        return None
