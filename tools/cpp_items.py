"""A C++ source file as top-level items, for the source-layout round's tools.

Not a C++ parser: a lexer that knows comments, strings, character literals,
raw strings and preprocessor lines, and a scanner that cuts a file into its
top-level items - each function definition, declaration, namespace block,
`extern "C"` block and preprocessor line - with the comment block that leads
it, the context it sits in (namespaces, linkage block, the
`#pragma clang attribute` stack and the `#if` stack above it), the names it
defines and the identifiers it uses. tools/move_funcs.py moves items by
these spans and tools/cite_lookup.py answers "which item holds line N".

Names are found by shape, not by type analysis; a case the shapes miss is
caught by the build and by tools/codegen_same.py --by-function.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
RAW_PREFIX = re.compile(r'(?:u8|u|U|L)?R"([^()\\ ]{0,16})\(')
STR_PREFIX = re.compile(r'(?:u8|u|U|L)?"')
CHR_PREFIX = re.compile(r"(?:u8|u|U|L)?'")
PUNCT2 = ("::", "->", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=",
          "%=", "&=", "|=", "^=", "##", ".*")
SPECIAL_CALLS = {"__attribute__", "__declspec", "alignas", "decltype", "sizeof", "alignof", "noexcept",
                 "static_assert", "__cdecl", "__stdcall", "__fastcall", "throw", "requires", "operator"}
KEYWORDS = {"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char", "char8_t", "char16_t",
            "char32_t", "class", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
            "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
            "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable",
            "namespace", "new", "noexcept", "nullptr", "operator", "private", "protected", "public", "register",
            "reinterpret_cast", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
            "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid",
            "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
            "__cdecl", "__stdcall", "__fastcall", "__attribute__", "__declspec"}


@dataclass
class Tok:
    kind: str      # id, num, str, chr, punct, comment, pp
    text: str
    start: int
    end: int
    line: int      # 1-based line of start


def lex(src: str) -> list[Tok]:
    toks: list[Tok] = []
    i, n, line = 0, len(src), 1
    at_line_start = True
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
            at_line_start = True
            continue
        if c in " \t\r\f\v":
            i += 1
            continue
        sline = line
        if c == "#" and at_line_start:
            j = i
            while j < n:
                if src[j] == "\n":
                    if src[j - 1] == "\\" or (src[j - 1] == "\r" and src[j - 2] == "\\"):
                        line += 1
                        j += 1
                        continue
                    break
                if src.startswith("/*", j):          # a block comment inside a directive
                    k = src.find("*/", j + 2)
                    k = n if k < 0 else k + 2
                    line += src.count("\n", j, k)
                    j = k
                    continue
                if src.startswith("//", j):
                    while j < n and src[j] != "\n":
                        j += 1
                    break
                j += 1
            toks.append(Tok("pp", src[i:j], i, j, sline))
            i = j
            continue
        at_line_start = False
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            toks.append(Tok("comment", src[i:j], i, j, sline))
            i = j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            toks.append(Tok("comment", src[i:j], i, j, sline))
            line += src.count("\n", i, j)
            i = j
            continue
        m = RAW_PREFIX.match(src, i)
        if m:
            close = ")" + m.group(1) + '"'
            j = src.find(close, m.end())
            j = n if j < 0 else j + len(close)
            toks.append(Tok("str", src[i:j], i, j, sline))
            line += src.count("\n", i, j)
            i = j
            continue
        m = STR_PREFIX.match(src, i)
        if m and (c == '"' or not (i > 0 and (src[i - 1].isalnum() or src[i - 1] == "_"))):
            j = m.end()
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            j += 1
            toks.append(Tok("str", src[i:j], i, j, sline))
            i = j
            continue
        m = CHR_PREFIX.match(src, i)
        if m and (c == "'" or not (i > 0 and (src[i - 1].isalnum() or src[i - 1] == "_"))):
            j = m.end()
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            j += 1
            toks.append(Tok("chr", src[i:j], i, j, sline))
            i = j
            continue
        if c.isalpha() or c == "_":
            m = IDENT.match(src, i)
            toks.append(Tok("id", m.group(0), i, m.end(), sline))
            i = m.end()
            continue
        if c.isdigit() or (c == "." and i + 1 < n and src[i + 1].isdigit()):
            j = i + 1
            while j < n and (src[j].isalnum() or src[j] in "._'" or (src[j] in "+-" and src[j - 1] in "eEpP")):
                j += 1
            toks.append(Tok("num", src[i:j], i, j, sline))
            i = j
            continue
        two = src[i:i + 2]
        if two in PUNCT2:
            toks.append(Tok("punct", two, i, i + 2, sline))
            i += 2
            continue
        if src.startswith("...", i):
            toks.append(Tok("punct", "...", i, i + 3, sline))
            i += 3
            continue
        toks.append(Tok("punct", c, i, i + 1, sline))
        i += 1
    return toks


@dataclass
class Item:
    kind: str                     # func, decl, namespace, linkage, pp, using, alias, typedef, type
    start: int                    # offset of the first byte of its first line (leading comment included)
    end: int                      # offset just past its last line's newline
    code_start: int               # offset of its first token (after the leading comment)
    first_line: int               # 1-based, leading comment included
    code_line: int                # line of its first token
    last_line: int
    names: list[str] = field(default_factory=list)
    uses: set[str] = field(default_factory=set)
    ns: tuple[str, ...] = ()      # enclosing namespaces ('' = anonymous)
    linkage: bool = False         # inside an extern "C" { } block
    pragmas: tuple[str, ...] = () # the #pragma clang attribute push texts in force
    conds: tuple[str, ...] = ()   # the #if / #ifdef texts in force
    is_static: bool = False
    is_const: bool = False        # a variable declared const / constexpr (a value, not state)
    is_var: bool = False
    children: list["Item"] = field(default_factory=list)
    text: str = ""

    @property
    def local(self) -> bool:
        """File-local: inside an anonymous namespace, or `static` at namespace scope, or a type,
        alias, typedef or using that a .cpp defines for itself."""
        return "" in self.ns or self.is_static or self.kind in ("using", "alias", "typedef", "type")


def _line_start(src: str, off: int) -> int:
    return src.rfind("\n", 0, off) + 1


def _line_end(src: str, off: int) -> int:
    j = src.find("\n", off)
    return len(src) if j < 0 else j + 1


class FileItems:
    """Every top-level item of one file."""

    def __init__(self, src: str):
        self.src = src
        self.toks = lex(src)
        self.items: list[Item] = []
        self.pragma_stack: list[str] = []
        self.cond_stack: list[str] = []
        self.pos = 0
        self._scope(self.items, (), False, top=True)

    # ---- the scanner -------------------------------------------------------
    def _leading(self, first_code: int) -> int:
        """The start of the comment block that leads the token at index first_code."""
        k = first_code - 1
        start_tok = first_code
        while k >= 0 and self.toks[k].kind == "comment":
            gap = self.src[self.toks[k].end:self.toks[start_tok].start]
            if gap.count("\n") > 1:
                break
            # a comment that trails code on its own line belongs to that code
            ls = _line_start(self.src, self.toks[k].start)
            if self.src[ls:self.toks[k].start].strip():
                break
            start_tok = k
            k -= 1
        return start_tok

    def _make(self, kind: str, i0: int, i1: int, ns, linkage) -> Item:
        """An item from token i0 to token i1 (inclusive)."""
        lead = self._leading(i0)
        t0, t1 = self.toks[lead], self.toks[i1]
        end = t1.end
        # a comment on the same line as the item's last token goes with it
        if i1 + 1 < len(self.toks):
            nx = self.toks[i1 + 1]
            if nx.kind == "comment" and "\n" not in self.src[t1.end:nx.start]:
                end = nx.end
                # and the comment lines below it aligned with it: its continuation
                col = nx.start - _line_start(self.src, nx.start)
                k = i1 + 2
                while k < len(self.toks) and self.toks[k].kind == "comment":
                    c = self.toks[k]
                    if self.src[end:c.start].count("\n") != 1 or c.start - _line_start(self.src, c.start) != col:
                        break
                    end = c.end
                    k += 1
        start = _line_start(self.src, t0.start)
        stop = _line_end(self.src, end)
        it = Item(kind=kind, start=start, end=stop, code_start=self.toks[i0].start, first_line=t0.line,
                  code_line=self.toks[i0].line, last_line=self.src.count("\n", 0, max(stop - 1, 0)) + 1,
                  ns=tuple(ns), linkage=linkage, pragmas=tuple(self.pragma_stack), conds=tuple(self.cond_stack))
        it.text = self.src[start:stop]
        return it

    def _pp(self, tok: Tok, out: list[Item], ns, linkage, index: int) -> None:
        d = tok.text.lstrip("#").strip()
        word = d.split(None, 1)[0] if d else ""
        it = self._make("pp", index, index, ns, linkage)
        if word == "define":
            m = IDENT.match(d[6:].strip())
            if m:
                it.names = [m.group(0)]
            it.uses = set(IDENT.findall(d[6:])) - set(it.names)
        if word in ("if", "ifdef", "ifndef"):
            self.cond_stack.append(d)
        elif word == "endif":
            if self.cond_stack:
                self.cond_stack.pop()
        elif word == "pragma":
            m = re.match(r"pragma\s+clang\s+attribute\s+(push|pop)\b(.*)", d)
            if m and m.group(1) == "push":
                self.pragma_stack.append("#" + tok.text.lstrip("#").strip())
            elif m and self.pragma_stack:
                self.pragma_stack.pop()
        out.append(it)

    def _match(self, i: int, open_: str, close: str) -> int:
        depth = 0
        while i < len(self.toks):
            t = self.toks[i]
            if t.kind == "punct":
                if t.text == open_:
                    depth += 1
                elif t.text == close:
                    depth -= 1
                    if depth == 0:
                        return i
            i += 1
        raise ValueError(f"unbalanced {open_} at offset {self.toks[i - 1].start}")

    def _scope(self, out: list[Item], ns, linkage, top: bool) -> None:
        toks = self.toks
        while self.pos < len(toks):
            t = toks[self.pos]
            if t.kind == "comment":
                self.pos += 1
                continue
            if t.kind == "pp":
                self._pp(t, out, ns, linkage, self.pos)
                self.pos += 1
                continue
            if t.kind == "punct" and t.text == "}":
                if top:
                    raise ValueError(f"stray '}}' at line {t.line}")
                return
            if t.kind == "punct" and t.text == ";":
                self.pos += 1
                continue
            self._item(out, ns, linkage)

    def _item(self, out: list[Item], ns, linkage) -> None:
        toks = self.toks
        i0 = self.pos
        words = [toks[k].text for k in range(i0, min(i0 + 4, len(toks)))]
        # namespace X { ... } / namespace { ... } / extern "C" { ... }
        is_block = False
        if words[0] == "namespace":
            k = i0 + 1
            parts: list[str] = []
            while k < len(toks) and toks[k].kind == "id" and (not parts or toks[k - 1].text == "::"):
                parts.append(toks[k].text)
                k += 1
                if k < len(toks) and toks[k].text == "::":
                    k += 1
            is_block = k < len(toks) and toks[k].text == "{"
        if is_block:
            brace = k
            holder = self._make("namespace", i0, brace, ns, linkage)
            holder.names = ["::".join(parts)] if parts else []
            self.pos = brace + 1
            self._scope(holder.children, tuple(ns) + (tuple(parts) if parts else ("",)), linkage, top=False)
            close = self.pos
            holder.end = _line_end(self.src, toks[close].end)
            if close + 1 < len(toks) and toks[close + 1].kind == "comment" and "\n" not in self.src[toks[close].end:toks[close + 1].start]:
                holder.end = _line_end(self.src, toks[close + 1].end)
            holder.last_line = self.src.count("\n", 0, holder.end - 1) + 1
            holder.text = self.src[holder.start:holder.end]
            self.pos = close + 1
            out.append(holder)
            return
        if words[0] == "extern" and len(words) > 2 and words[1] == '"C"' and words[2] == "{":
            holder = self._make("linkage", i0, i0 + 2, ns, linkage)
            self.pos = i0 + 3
            self._scope(holder.children, ns, True, top=False)
            close = self.pos
            holder.end = _line_end(self.src, toks[close].end)
            holder.last_line = self.src.count("\n", 0, holder.end - 1) + 1
            holder.text = self.src[holder.start:holder.end]
            self.pos = close + 1
            out.append(holder)
            return
        # anything else runs to a ';' at depth 0, or is a function body
        i = i0
        depth = 0
        while i < len(toks):
            t = toks[i]
            if t.kind == "pp":       # a directive inside an item: keep scanning (rare; kept in the item)
                i += 1
                continue
            if t.kind == "punct":
                if t.text in "([":
                    depth += 1
                elif t.text in ")]":
                    depth -= 1
                elif t.text == ";" and depth == 0:
                    self._decl(out, i0, i, ns, linkage)
                    self.pos = i + 1
                    return
                elif t.text == "{" and depth == 0:
                    if self._is_function(i0, i):
                        close = self._match(i, "{", "}")
                        it = self._make("func", i0, close, ns, linkage)
                        self._fill_func(it, i0, i, close)
                        out.append(it)
                        self.pos = close + 1
                        return
                    i = self._match(i, "{", "}")
                elif t.text == "}" and depth == 0:
                    raise ValueError(f"unexpected '}}' at line {t.line} inside an item from line {toks[i0].line}")
            i += 1
        raise ValueError(f"an item from line {toks[i0].line} does not end")

    def _significant(self, i0: int, i1: int) -> list[Tok]:
        return [t for t in self.toks[i0:i1] if t.kind not in ("comment", "pp")]

    def _is_function(self, i0: int, brace: int) -> bool:
        ts = self._significant(i0, brace)
        if not ts:
            return False
        k = 0
        if ts[0].text == "template":      # skip the template header
            depth = 0
            for k, t in enumerate(ts):
                if t.text == "<":
                    depth += 1
                elif t.text == ">":
                    depth -= 1
                    if depth == 0:
                        k += 1
                        break
                elif t.text == ">>":
                    depth -= 2
                    if depth <= 0:
                        k += 1
                        break
        body = ts[k:]
        if not body or body[0].text in ("class", "struct", "union", "enum", "typedef", "using"):
            return False
        depth = 0
        has_paren = False
        for t in body:
            if t.text in "([":
                depth += 1
                if t.text == "(":
                    has_paren = True
            elif t.text in ")]":
                depth -= 1
            elif t.text == "=" and depth == 0:
                return False
        return has_paren

    def _uses(self, i0: int, i1: int) -> set[str]:
        out = set()
        prev = None
        for t in self.toks[i0:i1 + 1]:
            if t.kind == "comment":
                continue
            if t.kind == "pp":
                out.update(IDENT.findall(t.text))
                prev = None
                continue
            if t.kind == "id" and t.text not in KEYWORDS and not (prev is not None and prev.text in (".", "->", "::")):
                out.add(t.text)
            prev = t
        return out

    def _fill_func(self, it: Item, i0: int, brace: int, close: int) -> None:
        ts = self._significant(i0, brace)
        it.is_static = any(t.text == "static" for t in ts[:3])
        depth = 0
        name = None
        for k, t in enumerate(ts):
            if t.text == "(":
                if depth == 0 and k > 0 and ts[k - 1].kind == "id" and ts[k - 1].text not in SPECIAL_CALLS:
                    name = ts[k - 1].text
                    if k > 1 and ts[k - 2].text == "operator":
                        name = "operator"
                    break
                depth += 1
            elif t.text == ")":
                depth -= 1
        if name is None:
            m = re.search(r"operator\s*\S+", " ".join(t.text for t in ts))
            name = m.group(0) if m else "?"
        it.names = [name]
        it.uses = self._uses(i0, close) - {name}

    def _decl(self, out: list[Item], i0: int, semi: int, ns, linkage) -> None:
        ts = self._significant(i0, semi)
        first = ts[0].text if ts else ""
        kind = "decl"
        names: list[str] = []
        if first == "using":
            if len(ts) > 1 and ts[1].text == "namespace":
                kind = "using"
            elif len(ts) > 2 and ts[2].text == "=":
                kind, names = "alias", [ts[1].text]
            else:
                kind, names = "using", [ts[-1].text] if ts[-1].kind == "id" else []
        elif first == "namespace":
            kind, names = "alias", [ts[1].text]
        elif first == "typedef":
            kind = "typedef"
            txt = [t.text for t in ts]
            m = None
            for k in range(len(txt) - 2):
                if txt[k] == "(" and txt[k + 1] in ("*", "&") and ts[k + 2].kind == "id":
                    m = txt[k + 2]
                    break
                if txt[k] == "(" and txt[k + 1] in ("__cdecl", "__stdcall") and txt[k + 2] == "*" and k + 3 < len(txt):
                    m = txt[k + 3]
                    break
            names = [m] if m else [t.text for t in ts if t.kind == "id"][-1:]
        elif first in ("struct", "class", "union", "enum") or (first == "template" and any(t.text in ("struct", "class") for t in ts[:12])):
            kind = "type"
            kw = next(k for k, t in enumerate(ts) if t.text in ("struct", "class", "union", "enum"))
            k = kw + 1
            if k < len(ts) and ts[k].text in ("class", "struct"):    # enum class
                k += 1
            if k < len(ts) and ts[k].kind == "id" and ts[k].text not in ("alignas", "__attribute__"):
                names.append(ts[k].text)
            if ts[kw].text == "enum":                                  # its enumerators
                try:
                    b = next(j for j, t in enumerate(ts) if t.text == "{")
                    depth = 0
                    expect = True
                    for t in ts[b + 1:]:
                        if t.text in "({[":
                            depth += 1
                        elif t.text in ")}]":
                            if depth == 0:
                                break
                            depth -= 1
                        elif depth == 0 and t.text == ",":
                            expect = True
                        elif depth == 0 and expect and t.kind == "id":
                            names.append(t.text)
                            expect = False
                except StopIteration:
                    pass
            # `struct X {...} name;`: the declarator after the body
            try:
                b = next(j for j, t in enumerate(ts) if t.text == "{")
                depth = 0
                for j in range(b, len(ts)):
                    if ts[j].text == "{":
                        depth += 1
                    elif ts[j].text == "}":
                        depth -= 1
                        if depth == 0:
                            tail = [t for t in ts[j + 1:] if t.kind == "id"]
                            if tail:
                                names.append(tail[0].text)
                            break
            except StopIteration:
                pass
            if not names:
                kind = "decl"
        else:
            names = self._declarators(ts)
        it = self._make(kind, i0, semi, ns, linkage)
        it.names = names
        heads = [t.text for t in ts[:6]]
        it.is_static = "static" in heads
        it.is_const = "constexpr" in heads or "const" in heads
        it.is_var = kind == "decl" and not self._is_prototype(ts)
        it.uses = self._uses(i0, semi) - set(names)
        out.append(it)

    def _is_prototype(self, ts: list[Tok]) -> bool:
        depth = 0
        for k, t in enumerate(ts):
            if t.text == "=" and depth == 0:
                return False
            if t.text == "(":
                if depth == 0 and k > 0 and ts[k - 1].kind == "id" and ts[k - 1].text not in SPECIAL_CALLS:
                    # `T name(args);` with args that read as types is a prototype; `T name(1);` a variable
                    inner = []
                    d2 = 0
                    for u in ts[k + 1:]:
                        if u.text == "(":
                            d2 += 1
                        elif u.text == ")":
                            if d2 == 0:
                                break
                            d2 -= 1
                        inner.append(u)
                    if not inner or inner[0].kind == "id":
                        return True
                    return False
                depth += 1
            elif t.text == ")":
                depth -= 1
        return False

    def _declarators(self, ts: list[Tok]) -> list[str]:
        """The names a declaration declares: per top-level declarator, the identifier before its
        initialiser, array bound, brace or parameter list (`(*name)` for a function pointer)."""
        names = []
        depth = angle = 0
        cand = None
        in_init = False
        for k, t in enumerate(ts):
            x = t.text
            prev = ts[k - 1] if k > 0 else None
            if not in_init and x == "<" and prev is not None and prev.kind == "id":
                angle += 1
                continue
            if not in_init and angle and x in (">", ">>"):
                angle -= 1 if x == ">" else 2
                angle = max(angle, 0)
                continue
            if angle:
                continue
            if x in ("(", "[", "{"):
                if depth == 0 and not in_init and cand is None:
                    if x == "(" and k + 2 < len(ts) and ts[k + 1].text == "*" and ts[k + 2].kind == "id":
                        cand = ts[k + 2].text
                    elif prev is not None and prev.kind == "id" and prev.text not in SPECIAL_CALLS:
                        cand = prev.text
                if x == "{" and depth == 0:
                    in_init = True
                depth += 1
            elif x in (")", "]", "}"):
                depth -= 1
            elif x == "=" and depth == 0:
                if cand is None and prev is not None and prev.kind == "id":
                    cand = prev.text
                in_init = True
            elif x == "," and depth == 0:
                if cand is None and prev is not None and prev.kind == "id":
                    cand = prev.text
                if cand:
                    names.append(cand)
                cand, in_init = None, False
        if cand is None and ts and ts[-1].kind == "id":
            cand = ts[-1].text
        if cand:
            names.append(cand)
        return names

    # ---- queries -----------------------------------------------------------
    def walk(self, items=None):
        for it in self.items if items is None else items:
            yield it
            if it.children:
                yield from self.walk(it.children)

    def leaves(self):
        """Every item that is not a namespace or linkage holder."""
        return [it for it in self.walk() if it.kind not in ("namespace", "linkage")]

    def at_line(self, line: int):
        """The innermost item whose span (leading comment included) holds `line`, or None."""
        best = None
        for it in self.walk():
            if it.first_line <= line <= it.last_line:
                if best is None or (it.last_line - it.first_line) <= (best.last_line - best.first_line):
                    best = it
        return best

    def functions(self):
        return [it for it in self.leaves() if it.kind == "func"]
