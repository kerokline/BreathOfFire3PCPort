"""The launcher's three suites (docs/launcher-suites.md): recipes/suites.toml, checked
and turned into the launcher's table.

    python tools/suites.py check                 # what importer.py check runs (CI); no game data
    python tools/suites.py gen --out FILE.h      # the build step: the launcher's generated table

`check` holds the catalogue to the owner's calls and to the other recipes:
- exactly the three suites, graphics, audio and data (docs/psp-only-build.md 7 item 3);
- every member names builds fixtures.toml holds; an `opt` member names a layer of
  recipes/opt.toml, and its `from` is exactly the builds that layer's chunks are
  carried by (`on`), so the launcher never names a disc that cannot make it;
- every opt/ layer but the default ones (area4-walls, a repair: DIV-0080) is in
  exactly one suite, so no layer goes unoffered;
- a member that cannot be selected beside a PlayStation base says why;
- at most four members a suite, the places the dialog has for them.

`gen` writes the same catalogue as C++ constants (namespace bof3x::suites_gen),
with fixtures.toml's role for every build, so the launcher knows a PSP disc from a
PlayStation one without reading fixtures.toml at run time. It rewrites the file
only when the content changes.
"""
import argparse, os, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUITES = os.path.join(ROOT, "recipes", "suites.toml")
OPT = os.path.join(ROOT, "recipes", "opt.toml")
FIXTURES = os.path.join(ROOT, "fixtures.toml")

SUITE_IDS = ["graphics", "audio", "data"]          # the owner's three, in the page's order
KINDS = {"opt": "kOpt", "bgm": "kBgm", "snd": "kSnd", "exe": "kExe", "base": "kBase"}
OVER = {"layer": "kLayer", "engine": "kEngine", "none": "kNone"}
ROLES = {"pc-port": "kPc", "psx-disc": "kPsx", "psp-disc": "kPsp"}
DEFAULT_LAYERS = {"area4-walls"}                   # importer.py DEFAULT_OPT; config.h kOptDefault
MEMBERS_MAX = 4                                    # launcher.rc: four member lines per suite


def _load(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def _printable(s):
    return all(32 <= ord(c) < 127 for c in s) and '"' not in s and "\\" not in s


def check(suites=None, opt=None, fixtures=None):
    """Errors, as strings; empty when the catalogue is sound."""
    errs = []
    try:
        cat = _load(suites or SUITES)
    except (OSError, tomllib.TOMLDecodeError) as e:
        return ["suites.toml: %s" % e]
    builds = {b["id"]: b.get("role") for b in _load(fixtures or FIXTURES)["build"]}
    layers = {}
    for l in _load(opt or OPT).get("layer", []):
        on = set()
        for f in l.get("file", []):
            for c in f.get("chunks", []):
                on |= set(c.get("on", []))
        layers[l["name"]] = on
    ids = [s.get("id") for s in cat.get("suite", [])]
    if ids != SUITE_IDS:
        errs.append("suites: %s, not the owner's three %s in that order" % (ids, SUITE_IDS))
    seen, placed = set(), {}
    for s in cat.get("suite", []):
        for key in ("label", "what"):
            if not isinstance(s.get(key), str) or not s[key] or not _printable(s[key]):
                errs.append("suite %s: `%s` missing or not printable ASCII" % (s.get("id"), key))
        if not s.get("member") or len(s["member"]) > MEMBERS_MAX:
            errs.append("suite %s: %d members, not 1..%d (the dialog's four places a suite)"
                        % (s.get("id"), len(s.get("member") or []), MEMBERS_MAX))
        for m in s.get("member", []):
            mid = m.get("id")
            where = "suite %s member %s" % (s.get("id"), mid)
            if not mid or mid in seen:
                errs.append("%s: id missing or twice" % where)
            seen.add(mid)
            if not isinstance(m.get("label"), str) or not m["label"] or not _printable(m["label"]):
                errs.append("%s: label missing or not printable ASCII" % where)
            kind, over = m.get("kind"), m.get("over_psx")
            if kind not in KINDS:
                errs.append("%s: kind %r" % (where, kind))
            if over not in OVER:
                errs.append("%s: over_psx %r" % (where, over))
            frm = m.get("from") or []
            if not frm or len(set(frm)) != len(frm):
                errs.append("%s: from %r" % (where, frm))
            for b in frm:
                if b not in builds:
                    errs.append("%s: build %s not in fixtures.toml" % (where, b))
                elif builds[b] != "psp-disc":
                    errs.append("%s: build %s is not a PSP disc (a suite's other side is the PlayStation's)" % (where, b))
            why = m.get("why")
            if (over == "layer") != (why is None):
                errs.append("%s: `why` is wanted exactly when over_psx is not \"layer\"" % where)
            if why is not None and (not why or not _printable(why)):
                errs.append("%s: why not printable ASCII" % where)
            layer = m.get("layer")
            if (kind == "opt") != (layer is not None):
                errs.append("%s: `layer` is wanted exactly for kind opt" % where)
            if kind == "opt" and over != "layer":
                errs.append("%s: an opt/ layer plays today; over_psx must be \"layer\"" % where)
            if layer is not None:
                if layer not in layers:
                    errs.append("%s: layer %s not in recipes/opt.toml" % (where, layer))
                else:
                    if set(frm) != layers[layer]:
                        errs.append("%s: from %s, but the layer's chunks are on %s" % (where, sorted(frm), sorted(layers[layer])))
                    if layer in placed:
                        errs.append("%s: layer %s already in suite %s" % (where, layer, placed[layer]))
                    placed[layer] = s.get("id")
                if layer in DEFAULT_LAYERS:
                    errs.append("%s: %s is a default layer (a repair), not a suite's" % (where, layer))
    for name in layers:
        if name not in DEFAULT_LAYERS and name not in placed:
            errs.append("opt.toml layer %s is in no suite" % name)
    return errs


def _w(s):
    return 'L"%s"' % s


def generate(suites=None, fixtures=None):
    cat = _load(suites or SUITES)
    fx = _load(fixtures or FIXTURES)
    out = ["// GENERATED by tools/suites.py gen from recipes/suites.toml and fixtures.toml - do not edit;",
           "// edit recipes/suites.toml (docs/launcher-suites.md). Names, ids and builds only: no game data.",
           "#pragma once", "", "namespace bof3x::suites_gen {", "",
           "enum class Role { kPc, kPsx, kPsp };",
           "enum class Kind { kOpt, kBgm, kSnd, kExe, kBase };",
           "enum class OverPsx { kLayer, kEngine, kNone };", "",
           "struct BuildInfo {", "    const char* id;", "    Role role;", "};",
           "// fixtures.toml's builds, in its order.",
           "inline constexpr BuildInfo kBuilds[] = {"]
    for b in fx["build"]:
        if b.get("role") in ROLES:
            out.append('    {"%s", Role::%s},' % (b["id"], ROLES[b["role"]]))
    out += ["};", "",
            "struct SuiteInfo {", "    const char* id;", "    const wchar_t* label;", "    const wchar_t* what;", "};",
            "inline constexpr SuiteInfo kSuites[] = {"]
    for s in cat["suite"]:
        out.append('    {"%s", %s, %s},' % (s["id"], _w(s["label"]), _w(s["what"])))
    out += ["};", "",
            "// `from`: comma-separated build ids. `layer`: the opt/ layer, or nullptr. `why`: the",
            "// greyed reason beside a PlayStation base, or nullptr when it plays today.",
            "struct MemberInfo {", "    int suite;", "    const char* id;", "    const wchar_t* label;", "    Kind kind;",
            "    const char* layer;", "    const char* from;", "    OverPsx over_psx;", "    const wchar_t* why;", "};",
            "inline constexpr MemberInfo kMembers[] = {"]
    for i, s in enumerate(cat["suite"]):
        for m in s["member"]:
            out.append('    {%d, "%s", %s, Kind::%s, %s, "%s", OverPsx::%s, %s},' % (
                i, m["id"], _w(m["label"]), KINDS[m["kind"]],
                '"%s"' % m["layer"] if "layer" in m else "nullptr", ",".join(m["from"]), OVER[m["over_psx"]],
                _w(m["why"]) if "why" in m else "nullptr"))
    out += ["};", "", "}  // namespace bof3x::suites_gen", ""]
    return "\n".join(out)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    s = p.add_subparsers(dest="cmd", required=True)
    s.add_parser("check")
    g = s.add_parser("gen")
    g.add_argument("--out", required=True)
    a = p.parse_args(argv)
    errs = check()
    for e in errs:
        print("ERROR", e)
    if a.cmd == "check":
        print("suites check: %d error(s)" % len(errs))
        return 1 if errs else 0
    if errs:
        return 1
    text = generate()
    try:
        with open(a.out, encoding="utf-8", newline="\n") as f:
            if f.read() == text:
                return 0
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("suites: wrote %s" % a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
