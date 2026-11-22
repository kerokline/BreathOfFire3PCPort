#!/usr/bin/env python3
"""The function-pointer census: every call shape a cast hides, checked against the callee.

    python3 tools/fnptr_census.py --build build-linux-x86_64            # print every finding
    python3 tools/fnptr_census.py --build build-linux-x86_64 --check    # exit 1 on a finding
    python3 tools/fnptr_census.py --build build-linux-x86_64 --all      # every resolved site too
    python3 tools/fnptr_census.py --build B --only 'src/game/battle/*'  # some translation units

Why. A function called through a pointer whose type the compiler did not
check against the function - `reinterpret_cast<T>(f)`, `As<T>(f)` through
`void*`, `Fn<T>(::bof3::Code(bof3::addr::Name))` from an address - is called
with T's shape. On x86-32 every argument is a 4-byte stack slot, so a
`std::uint32_t` where the function takes a pointer is the same call. On
x86-64 it is not: an argument past the registers (the 5th and later under
`ms_abi`, which `__cdecl` is on Linux; past the 6th integer under System V)
lies in an 8-byte stack slot, and a caller that stores 4 bytes there leaves
the high half as it was, which the callee reads as part of its pointer (the
banner fault of wave C, docs/linux-round.md). In a register the high half
of a 32-bit argument is undefined by both ABIs: zero by the habit of the
code that produced it, not by rule. The census finds every such site the
compiler cannot see.

How. libclang (the C API through ctypes; no Python package needed) parses
every translation unit of a build's compile database (`ninja -t compdb`, made
here when the build has no compile_commands.json) with that build's flags,
so each type is the one that build compiles: `__cdecl` as `ms_abi`, a
pointer 8 bytes. Then, in files under src/ (headers too, once each):

  cast  an explicit cast whose type is a pointer to a function
        (reinterpret_cast, C-style, functional, static).
  call  a call that returns a pointer to a function from one argument of
        another type: the As<>/Fn<> helpers, whose own cast is on a
        template parameter and so is checked at each call instead.

The operand is followed through parentheses, casts and `&` to what it
names: a function (its type as this translation unit declares it), or an
integer the compiler can evaluate - `bof3::addr::Name`, a `CodeVa` constant,
a literal - taken as a VA and named by symbols.gen.h's `bof3::addr`. The
callee's real type is then its definition's, from whichever translation unit
defines it with C linkage (what BOF3_INJECT binds to the VA), or else
symbols.gen.h's prototype. The two shapes are compared parameter by
parameter, by what x86-64 does with each: an integer of 4 bytes or less, one
of 8, a pointer, a float, a double, a record and its size; the calling
convention; `...`; the return.

A third check, `decl`: every C-linkage declaration of a function in a file
under src/ against its definition. Two declarations in one translation unit
must agree, but a declaration in another file need not, and a call through
it is the same mismatch.

What a finding is (the `verdict` column):

  stack-narrow  an argument the caller passes as 4 bytes where the callee
                reads 8 (pointer or 64-bit integer), on the stack: the
                banner fault. Wrong on every call.
  reg-narrow    the same in a register: the high half is undefined by the
                ABI and zero by habit. Latent.
  ret-narrow    the caller reads 8 bytes of a return the callee makes 4:
                the high half undefined. Latent.
  truncating    the caller passes or reads 8 bytes where the callee has 4:
                the low half arrives; wrong if the value is a host pointer
                above 4 GB.
  class         integer against float or double, or records of another
                size: other registers or another layout.
  conv          another calling convention (ms_abi against System V).
  variadic      one side is variadic and the other is not.
  count         another number of parameters (the same on x86-32).
  unresolved    the operand names nothing the census can follow: a word
                read at run time (a .data table, a parameter, a field).
                Listed, not a failure: no static tool sees it.
  unknown       a VA with no function of ours and no prototype.

Differences of sub-word width (a byte against a 4-byte integer) and of
signedness within 4 bytes are the same on x86-32 and are not reported.

--check exits 1 on any stack-narrow, reg-narrow, ret-narrow, truncating,
class, conv, variadic or decl finding that WAITING (below) does not list, and
on a WAITING entry no longer found: the list may only shrink, as
os_census.py's HEADERS_WAITING. Reads src/ and the build's generated
header only - no game data. Needs libclang (--libclang, or the newest
/usr/lib/llvm-*/lib/libclang*.so*). The banner round of wave C,
docs/linux-round.md.
"""

from __future__ import annotations

import argparse
import ctypes
import fnmatch
import glob
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- libclang through ctypes ---------------------------------------------------


class CXString(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("flags", ctypes.c_uint)]


class CXCursor(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int), ("xdata", ctypes.c_int), ("data", ctypes.c_void_p * 3)]


class CXType(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int), ("data", ctypes.c_void_p * 2)]


class CXSourceLocation(ctypes.Structure):
    _fields_ = [("ptr_data", ctypes.c_void_p * 2), ("int_data", ctypes.c_uint)]


VISITOR = ctypes.CFUNCTYPE(ctypes.c_int, CXCursor, CXCursor, ctypes.c_void_p)

# CXCursorKind
K_STRUCT, K_UNION, K_CLASS = 2, 3, 4
K_FUNCTION, K_VAR, K_PARM = 8, 9, 10
K_NAMESPACE, K_LINKAGE = 22, 23
K_CXX_METHOD, K_CONSTRUCTOR, K_DESTRUCTOR, K_CONVERSION = 21, 24, 25, 26
K_FUNCTION_TEMPLATE, K_CLASS_TEMPLATE = 30, 31
K_UNEXPOSED_EXPR, K_DECL_REF, K_MEMBER_REF, K_CALL = 100, 101, 102, 103
K_PAREN, K_UNARY = 111, 112
K_CSTYLE_CAST = 117
K_STATIC_CAST, K_DYNAMIC_CAST, K_REINTERPRET_CAST, K_CONST_CAST, K_FUNCTIONAL_CAST = 124, 125, 126, 127, 128
K_TU = 350
CAST_KINDS = {K_CSTYLE_CAST, K_STATIC_CAST, K_REINTERPRET_CAST, K_CONST_CAST, K_FUNCTIONAL_CAST}
# CXTypeKind
T_VOID, T_BOOL = 2, 3
T_FLOAT, T_DOUBLE, T_LONGDOUBLE = 21, 22, 23
T_POINTER, T_LVALUEREF, T_RVALUEREF, T_RECORD, T_ENUM = 101, 103, 104, 105, 106
T_FUNCNOPROTO, T_FUNCPROTO, T_MEMBERPOINTER, T_NULLPTR = 110, 111, 117, 24
SIGNED_INTS = {13, 14, 15, 16, 17, 18, 19, 20}  # Char_S, SChar, WChar, Short, Int, Long, LongLong, Int128
UNSIGNED_INTS = {T_BOOL, 4, 5, 6, 7, 8, 9, 10, 11, 12}  # Char_U, UChar, Char16, Char32, UShort, UInt, ULong, ULongLong, UInt128
CONV_NAMES = {1: "C", 10: "ms_abi", 11: "sysv"}

_lib = None


def lib(path: str | None = None):
    global _lib
    if _lib is not None:
        return _lib
    if path is None:
        found = sorted(p for p in glob.glob("/usr/lib/llvm-*/lib/libclang-*.so.*") + glob.glob("/usr/lib/llvm-*/lib/libclang.so*")
                       if "libclang-cpp" not in p)
        if not found:
            sys.exit("fnptr_census: no libclang found; pass --libclang")
        path = found[-1]
    L = ctypes.CDLL(path)
    sig = {
        "clang_createIndex": (ctypes.c_void_p, [ctypes.c_int, ctypes.c_int]),
        "clang_parseTranslationUnit": (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p),
                                                         ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]),
        "clang_disposeTranslationUnit": (None, [ctypes.c_void_p]),
        "clang_getTranslationUnitCursor": (CXCursor, [ctypes.c_void_p]),
        "clang_getNumDiagnostics": (ctypes.c_uint, [ctypes.c_void_p]),
        "clang_getDiagnostic": (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_uint]),
        "clang_getDiagnosticSeverity": (ctypes.c_int, [ctypes.c_void_p]),
        "clang_getDiagnosticSpelling": (CXString, [ctypes.c_void_p]),
        "clang_disposeDiagnostic": (None, [ctypes.c_void_p]),
        "clang_visitChildren": (ctypes.c_uint, [CXCursor, VISITOR, ctypes.c_void_p]),
        "clang_getCString": (ctypes.c_char_p, [CXString]),
        "clang_disposeString": (None, [CXString]),
        "clang_getCursorSpelling": (CXString, [CXCursor]),
        "clang_getCursorType": (CXType, [CXCursor]),
        "clang_getCursorResultType": (CXType, [CXCursor]),
        "clang_getCursorReferenced": (CXCursor, [CXCursor]),
        "clang_getCursorDefinition": (CXCursor, [CXCursor]),
        "clang_getCursorSemanticParent": (CXCursor, [CXCursor]),
        "clang_getCursorLocation": (CXSourceLocation, [CXCursor]),
        "clang_Cursor_isNull": (ctypes.c_int, [CXCursor]),
        "clang_isCursorDefinition": (ctypes.c_uint, [CXCursor]),
        "clang_Cursor_getMangling": (CXString, [CXCursor]),
        "clang_getExpansionLocation": (None, [CXSourceLocation, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint),
                                              ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]),
        "clang_getFileName": (CXString, [ctypes.c_void_p]),
        "clang_getCanonicalType": (CXType, [CXType]),
        "clang_getPointeeType": (CXType, [CXType]),
        "clang_getResultType": (CXType, [CXType]),
        "clang_getNumArgTypes": (ctypes.c_int, [CXType]),
        "clang_getArgType": (CXType, [CXType, ctypes.c_uint]),
        "clang_isFunctionTypeVariadic": (ctypes.c_uint, [CXType]),
        "clang_getFunctionTypeCallingConv": (ctypes.c_int, [CXType]),
        "clang_Type_getSizeOf": (ctypes.c_longlong, [CXType]),
        "clang_getTypeSpelling": (CXString, [CXType]),
        "clang_Cursor_Evaluate": (ctypes.c_void_p, [CXCursor]),
        "clang_EvalResult_getKind": (ctypes.c_int, [ctypes.c_void_p]),
        "clang_EvalResult_getAsUnsigned": (ctypes.c_ulonglong, [ctypes.c_void_p]),
        "clang_EvalResult_dispose": (None, [ctypes.c_void_p]),
        "clang_getSpecializedCursorTemplate": (CXCursor, [CXCursor]),
    }
    for name, (res, args) in sig.items():
        f = getattr(L, name)
        f.restype = res
        f.argtypes = args
    _lib = L
    return L


def s(cx: CXString) -> str:
    L = lib()
    v = L.clang_getCString(cx)
    L.clang_disposeString(cx)
    return v.decode("utf-8", "replace") if v else ""


# --- shapes -------------------------------------------------------------------


def classify(t: CXType) -> str:
    """One argument or return as x86-64 sees it."""
    L = lib()
    c = L.clang_getCanonicalType(t)
    k = c.kind
    if k == T_VOID:
        return "void"
    if k in (T_POINTER, T_LVALUEREF, T_RVALUEREF, T_NULLPTR):
        return "ptr"
    if k == T_MEMBERPOINTER:
        return "mptr%d" % L.clang_Type_getSizeOf(c)
    if k == T_FLOAT:
        return "f4"
    if k == T_DOUBLE:
        return "f8"
    if k == T_LONGDOUBLE:
        return "f%d" % L.clang_Type_getSizeOf(c)
    if k == T_RECORD:
        return "rec%d" % L.clang_Type_getSizeOf(c)
    if k == T_ENUM or k in SIGNED_INTS or k in UNSIGNED_INTS:
        n = L.clang_Type_getSizeOf(c)
        if k == T_ENUM:
            sign = "i"
        else:
            sign = "s" if k in SIGNED_INTS else "u"
        return ("int" if n <= 4 else sign + "%d" % n)
    return "?" + s(L.clang_getTypeSpelling(c))


def fn_type_of(t: CXType):
    """A function type (or a pointer or reference to one) -> its prototype type, else None."""
    L = lib()
    c = L.clang_getCanonicalType(t)
    if c.kind in (T_POINTER, T_LVALUEREF, T_RVALUEREF):
        c = L.clang_getCanonicalType(L.clang_getPointeeType(c))
    if c.kind in (T_FUNCPROTO, T_FUNCNOPROTO):
        return c
    return None


def shape(fn: CXType) -> dict:
    L = lib()
    n = L.clang_getNumArgTypes(fn)
    return {
        "conv": CONV_NAMES.get(L.clang_getFunctionTypeCallingConv(fn), str(L.clang_getFunctionTypeCallingConv(fn))),
        "variadic": bool(L.clang_isFunctionTypeVariadic(fn)),
        "ret": classify(L.clang_getResultType(fn)),
        "params": [classify(L.clang_getArgType(fn, i)) for i in range(max(n, 0))],
        "spelling": s(L.clang_getTypeSpelling(fn)),
    }


def is_wide(c: str) -> bool:
    return c == "ptr" or c in ("s8", "u8")


def stack_positions(sh: dict) -> list[bool]:
    """For each parameter, True when it is passed on the stack."""
    out = []
    if sh["conv"] == "ms_abi":
        for i, _ in enumerate(sh["params"]):
            out.append(i >= 4)
        return out
    ints = flts = 0
    for c in sh["params"]:
        if c.startswith("f") and c in ("f4", "f8"):
            out.append(flts >= 8)
            flts += 1
        elif c.startswith("rec") and int(c[3:] or 0) > 16:
            out.append(True)
        else:
            out.append(ints >= 6)
            ints += 1
    return out


def compare(caller: dict, callee: dict) -> list[tuple[str, str]]:
    """The findings of a call made with `caller`'s shape to a function of `callee`'s."""
    found: list[tuple[str, str]] = []
    if caller["conv"] != callee["conv"]:
        found.append(("conv", "%s call to a %s function" % (caller["conv"], callee["conv"])))
    if caller["variadic"] != callee["variadic"]:
        found.append(("variadic", "caller %s, callee %s" % ("variadic" if caller["variadic"] else "fixed",
                                                            "variadic" if callee["variadic"] else "fixed")))
    cp, ep = caller["params"], callee["params"]
    if len(cp) != len(ep):
        found.append(("count", "%d arguments to %d parameters" % (len(cp), len(ep))))
    stack = stack_positions(callee)
    for i, (a, b) in enumerate(zip(cp, ep)):
        if a == b:
            continue
        where = "stack" if stack[i] else "register"
        if a == "int" and is_wide(b):
            found.append(("stack-narrow" if stack[i] else "reg-narrow", "argument %d: 4 bytes passed, %s read (%s)" % (i + 1, b, where)))
        elif is_wide(a) and b == "int":
            found.append(("truncating", "argument %d: %s passed, 4 bytes read (%s)" % (i + 1, a, where)))
        elif is_wide(a) and is_wide(b):
            continue  # 8 bytes either way
        else:
            found.append(("class", "argument %d: %s passed, %s read (%s)" % (i + 1, a, b, where)))
    a, b = caller["ret"], callee["ret"]
    if a != b and a != "void":
        if is_wide(a) and b == "int":
            found.append(("ret-narrow", "return: 8 bytes read (%s), 4 made" % a))
        elif a == "int" and is_wide(b):
            found.append(("truncating", "return: 4 bytes read, %s made" % b))
        elif is_wide(a) and is_wide(b):
            pass
        elif b == "void":
            found.append(("count", "return: %s read, the callee returns nothing" % a))
        else:
            found.append(("class", "return: %s read, %s made" % (a, b)))
    return found


# --- one translation unit -----------------------------------------------------


def norm(path: str) -> str:
    p = os.path.normpath(path)
    if p.startswith(REPO_ROOT + os.sep):
        return os.path.relpath(p, REPO_ROOT).replace(os.sep, "/")
    return p


REPO_ROOT = REPO


def where(cursor: CXCursor):
    L = lib()
    f = ctypes.c_void_p()
    line = ctypes.c_uint()
    col = ctypes.c_uint()
    off = ctypes.c_uint()
    L.clang_getExpansionLocation(L.clang_getCursorLocation(cursor), ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
    if not f.value:
        return None, 0, 0
    return s(L.clang_getFileName(f)), line.value, col.value


def evaluate(cursor: CXCursor):
    L = lib()
    r = L.clang_Cursor_Evaluate(cursor)
    if not r:
        return None
    try:
        if L.clang_EvalResult_getKind(r) == 1:  # CXEval_Int
            return L.clang_EvalResult_getAsUnsigned(r) & 0xFFFFFFFFFFFFFFFF
        return None
    finally:
        L.clang_EvalResult_dispose(r)


def children(cursor: CXCursor) -> list[CXCursor]:
    out: list[CXCursor] = []

    def visit(c, _p, _d):
        out.append(c)
        return 1

    lib().clang_visitChildren(cursor, VISITOR(visit), None)
    return out


def c_linkage(cursor: CXCursor) -> bool:
    L = lib()
    name = s(L.clang_getCursorSpelling(cursor))
    mangled = s(L.clang_Cursor_getMangling(cursor))
    if mangled not in (name, "_" + name):
        return False
    p = L.clang_getCursorSemanticParent(cursor)
    while not L.clang_Cursor_isNull(p) and p.kind == K_LINKAGE:
        p = L.clang_getCursorSemanticParent(p)
    return p.kind == K_TU or L.clang_Cursor_isNull(p) or p.kind == K_NAMESPACE


def trace(cursor: CXCursor, depth: int = 0):
    """What an operand names: ("fn", cursor) / ("va", int) / ("param", None) / ("runtime", text)."""
    L = lib()
    if depth > 12:
        return ("runtime", "too deep")
    k = cursor.kind
    if k in (K_PAREN, K_UNEXPOSED_EXPR) or k in CAST_KINDS or k == K_UNARY:
        v = evaluate(cursor) if k != K_UNARY else None
        if v is not None:
            return ("va", v)
        kids = [c for c in children(cursor) if c.kind >= 100 and c.kind < 200]
        if len(kids) == 1:
            return trace(kids[0], depth + 1)
        return ("runtime", "expr kind %d" % k)
    if k == K_DECL_REF or k == K_MEMBER_REF:
        ref = L.clang_getCursorReferenced(cursor)
        if L.clang_Cursor_isNull(ref):
            return ("runtime", "no referent")
        if ref.kind in (K_FUNCTION, K_CXX_METHOD):
            return ("fn", ref)
        if ref.kind == K_PARM:
            return ("param", s(L.clang_getCursorSpelling(ref)))
        v = evaluate(cursor)
        if v is not None:
            return ("va", v)
        if ref.kind == K_VAR:
            # a constexpr record (CodeVa{bof3::addr::X}): its initialiser's integer
            for c in children(ref):
                if c.kind >= 100 and c.kind < 200:
                    r = trace(c, depth + 1)
                    if r[0] in ("va", "fn"):
                        return r
        return ("runtime", s(L.clang_getCursorSpelling(cursor)))
    if k == K_CALL:
        v = evaluate(cursor)
        if v is not None:
            return ("va", v)
        # ::bof3::Code(va), CodeToCall(va), Img(va): the VA argument
        # (the first child is the callee itself, never what the call names; a
        # function found in an argument is not what a call returns - a lambda's
        # operator(), say - so only an address is taken from one)
        args = [c for c in children(cursor) if c.kind >= 100 and c.kind < 200]
        for a in args[1:]:
            r = trace(a, depth + 1)
            if r[0] == "va":
                return r
        return ("runtime", "call " + s(L.clang_getCursorSpelling(cursor)))
    v = evaluate(cursor)
    if v is not None:
        return ("va", v)
    kids = [c for c in children(cursor) if c.kind >= 100 and c.kind < 200]
    if len(kids) == 1:
        return trace(kids[0], depth + 1)
    return ("runtime", "expr kind %d" % k)


def scan_tu(job):
    entry, src_root, libclang = job
    L = lib(libclang)
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    keep: list[str] = []
    skip = False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a == "-c" or a == entry["file"] or a.startswith("-MD") or a == "-MMD" or a.startswith("-fcolor"):
            continue
        keep.append(a)
    os.chdir(entry["directory"])
    index = L.clang_createIndex(0, 0)
    cargs = (ctypes.c_char_p * len(keep))(*[a.encode() for a in keep])
    tu = L.clang_parseTranslationUnit(index, entry["file"].encode(), cargs, len(keep), None, 0, 0x200)
    if not tu:
        return {"file": entry["file"], "error": "parse failed", "sites": [], "decls": []}
    errors = []
    for i in range(L.clang_getNumDiagnostics(tu)):
        d = L.clang_getDiagnostic(tu, i)
        if L.clang_getDiagnosticSeverity(d) >= 3:
            errors.append(s(L.clang_getDiagnosticSpelling(d)))
        L.clang_disposeDiagnostic(d)
    sites = []
    decls = []
    root = L.clang_getTranslationUnitCursor(tu)
    file_ok: dict[str, bool] = {}

    def ours(path):
        if path is None:
            return False
        r = file_ok.get(path)
        if r is None:
            p = os.path.normpath(os.path.join(entry["directory"], path))
            r = p.startswith(src_root + os.sep)
            file_ok[path] = r
        return r

    def fn_site(kind, cursor, cast_type, operand):
        path, line, col = where(cursor)
        fn = fn_type_of(cast_type)
        if fn is None:
            return
        target = trace(operand)
        row = {"file": norm(os.path.normpath(os.path.join(entry["directory"], path))), "line": line, "col": col,
               "kind": kind, "caller": shape(fn)}
        if target[0] == "fn":
            ref = target[1]
            row["target"] = s(L.clang_getCursorSpelling(ref))
            row["c_linkage"] = c_linkage(ref)
            tfn = fn_type_of(L.clang_getCursorType(ref))
            row["local"] = shape(tfn) if tfn is not None else None
            # the same type as the cast: the compiler saw it
            if tfn is not None and row["local"]["spelling"] == row["caller"]["spelling"]:
                row["same"] = True
        elif target[0] == "va":
            row["va"] = target[1]
        elif target[0] == "param":
            row["param"] = target[1]
        else:
            row["runtime"] = target[1]
        sites.append(row)

    def record_decl(c):
        if not c_linkage(c):
            return
        path, line, _ = where(c)
        if path is None:
            return
        fn = fn_type_of(L.clang_getCursorType(c))
        if fn is None:
            return
        p = os.path.normpath(os.path.join(entry["directory"], path))
        decls.append({"name": s(L.clang_getCursorSpelling(c)), "def": bool(L.clang_isCursorDefinition(c)),
                      "file": norm(p), "line": line, "ours": ours(path) or p.endswith("symbols.gen.h"),
                      "gen": p.endswith("symbols.gen.h"), "shape": shape(fn)})

    def visit(c, parent, _d):
        k = c.kind
        if k in (K_FUNCTION_TEMPLATE, K_CLASS_TEMPLATE):
            return 1  # patterns: their casts are on template parameters
        if k == K_FUNCTION:
            path, _, _ = where(c)
            if path is not None and (ours(path) or path.endswith("symbols.gen.h")):
                record_decl(c)
            if not ours(path):
                return 1
            return 2
        if k == K_LINKAGE or k == K_NAMESPACE:
            return 2
        if k < 100 or k >= 200:
            if k in (K_STRUCT, K_CLASS, K_UNION, K_VAR, K_CXX_METHOD, K_CONSTRUCTOR, K_DESTRUCTOR, K_CONVERSION) or k >= 200:
                path, _, _ = where(c)
                return 2 if ours(path) else 1
            return 2
        path, _, _ = where(c)
        if not ours(path):
            return 1
        if k in CAST_KINDS:
            t = L.clang_getCursorType(c)
            if fn_type_of(t) is not None:
                kids = [x for x in children(c) if x.kind >= 100 and x.kind < 200]
                if kids:
                    fn_site("cast", c, t, kids[-1])
        elif k == K_CALL:
            t = L.clang_getCursorType(c)
            if fn_type_of(t) is not None:
                kids = [x for x in children(c) if x.kind >= 100 and x.kind < 200]
                # kids[0] is the callee reference; one argument after it
                if len(kids) == 2:
                    at = L.clang_getCursorType(kids[1])
                    ac = L.clang_getCanonicalType(at)
                    tc = L.clang_getCanonicalType(t)
                    if s(L.clang_getTypeSpelling(ac)) != s(L.clang_getTypeSpelling(tc)):
                        fn_site("call", c, t, kids[1])
        return 2

    L.clang_visitChildren(root, VISITOR(visit), None)
    L.clang_disposeTranslationUnit(tu)
    return {"file": entry["file"], "errors": errors[:5], "nerrors": len(errors), "sites": sites, "decls": decls}


# --- the whole build ------------------------------------------------------------


def load_db(build: str) -> list[dict]:
    db = os.path.join(build, "compile_commands.json")
    if os.path.isfile(db):
        with open(db) as f:
            return json.load(f)
    out = subprocess.run(["ninja", "-C", build, "-t", "compdb"], capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def addr_names(build: str) -> dict[int, list[str]]:
    gen = os.path.join(build, "gen", "bof3", "symbols.gen.h")
    names: dict[int, list[str]] = {}
    inside = False
    with open(gen, encoding="utf-8") as f:
        for line in f:
            if line.startswith("namespace bof3::addr {"):
                inside = True
                continue
            if inside and line.startswith("}"):
                inside = False
                continue
            if inside:
                m = re.match(r"inline constexpr std::uint32_t (\w+) = (0x[0-9a-fA-F]+);", line)
                if m:
                    names.setdefault(int(m.group(2), 16), []).append(m.group(1))
    return names


VERDICTS_FAIL = {"stack-narrow", "reg-narrow", "ret-narrow", "truncating", "class", "conv", "variadic", "decl"}

# Findings left on purpose, each read (docs/linux-round.md, "The banner fault
# and its class, as run"): (file, target, verdict, detail). Line numbers are
# not in the key, so an edit above a site does not stale it. The list may only
# shrink: --check fails on an entry no longer found, as on a new finding.
_SHADOW = "shadow fuzz table: WantsShadow is a constant false on x86-64, never called there"
WAITING: dict[tuple[str, str, str, str], str] = {}


def _wait(file, target, verdict, details, why):
    for d in details:
        WAITING[(file, target, verdict, d)] = why


_LOW = ("the value is a place of the game's (the image, its pools or the low-memory reservations), "
        "below 4 GB by the 64-bit design; the callee keeps 32 bits of it as the original does")
_REG = ("a register argument: the caller computes the word with 32-bit arithmetic, which clears the "
        "high half on x86-64; the ABI leaves that half undefined, so it is latent, not wrong today")
_VAR = ("System V both sides (B2d): the integer and pointer arguments are in the same registers; "
        "al, the variadic callee's count of vector registers, is not set, which only bounds a save")
_wait("src/game/area/area_174.cpp", "Field_SlotStart", "truncating", ["argument 2: ptr passed, 4 bytes read (register)"], _LOW)
_wait("src/game/area/area_198.cpp", "Field_SlotStart", "truncating", ["argument 2: ptr passed, 4 bytes read (register)"], _LOW)
_wait("src/game/area/area_85.cpp", "Field_SlotStart", "truncating", ["argument 2: ptr passed, 4 bytes read (register)"], _LOW)
_wait("src/game/battle/battle_3.cpp", "Char_AbilityList", "truncating", ["return: 4 bytes read, ptr made"], _LOW)
_wait("src/game/battle/battle_3.cpp", "Msg_SystemPtr", "truncating", ["return: 4 bytes read, ptr made"], _LOW)
_wait("src/game/battle/battle_3.cpp", "Str_CopyN", "truncating", ["return: 4 bytes read, ptr made"], _LOW)
_wait("src/game/battle/battle_3.cpp", "Str_CopyN", "reg-narrow",
      ["argument 1: 4 bytes passed, ptr read (register)", "argument 2: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_3.cpp", "Sprite_SetTint", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_3.cpp", "EnemyAI_RowDone", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_3.cpp", "EnemyAI_ApplyAction", "reg-narrow",
      ["argument 1: 4 bytes passed, ptr read (register)", "argument 2: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_3.cpp", "EnemyAI_SetRowDone", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_banner.cpp", "Sprite_ReleaseTint", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_banner.cpp", "Sprite_SetTint", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/battle_obj.cpp", "Sprite_PoseFromSet", "reg-narrow", ["argument 2: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/battle/dragon_cmd.cpp", "Task_Restart", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/core/sound.cpp", "SndBuf_SetVolume", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/effect/effect.cpp", "Stat_AddClampedTo", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/effect/effect_1a.cpp", "EffectKind0F_DrawGlyph", "reg-narrow", ["argument 2: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/effect/effect_kind_0f.cpp", "EffectKind0F_DrawGlyph", "reg-narrow", ["argument 2: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/effect/effect_kind_0f.cpp", "EffectKind0F_CharCount", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
_wait("src/game/effect/effect_kind_0f.cpp", "UiSprite_Draw", "truncating", ["return: 4 bytes read, ptr made"], _LOW)
_wait("src/game/minigame/shisu.cpp", "Shisu_DrawModel", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
for _f in ("scena_03", "scena_04_2", "scena_07_2"):
    _wait("src/game/scenario/%s.cpp" % _f, "Field_CellTriggerAt", "reg-narrow", ["argument 1: 4 bytes passed, ptr read (register)"], _REG)
for _f in ("buff_spike", "leech_shell", "shield_spark"):
    _wait("src/game/magic/%s.cpp" % _f, "MagicFx_LinkByDepth", "truncating", ["argument 4: ptr passed, 4 bytes read (register)"], _LOW)
_wait("src/game/field/area_map.cpp", "Crt_sprintf", "variadic", ["caller fixed, callee variadic"], _VAR)
_wait("src/game/platform/magic_080.cpp", "Crt_sscanf", "variadic", ["caller fixed, callee variadic"], _VAR)
_wait("src/game/platform/magic_080.cpp", "Crt_sprintf", "variadic", ["caller fixed, callee variadic"], _VAR)
_wait("src/game/platform/psx_gte_float.cpp", "Gte_DepthRamp", "truncating", ["argument 1: ptr passed, 4 bytes read (register)"], _SHADOW)
_wait("src/game/platform/psx_gte_float.cpp", "Gte_SetGeomOffset", "truncating",
      ["argument 1: ptr passed, 4 bytes read (register)", "argument 2: ptr passed, 4 bytes read (register)"], _SHADOW)
_wait("src/game/platform/psx_gte_float.cpp", "Gte_SetGeomScreen", "truncating", ["argument 1: ptr passed, 4 bytes read (register)"], _SHADOW)
for _n in ("Gte_RotMatrixX", "Gte_RotMatrixY", "Gte_RotMatrixZ"):
    _wait("src/game/platform/psx_gte_matrix.cpp", _n, "truncating", ["argument 1: ptr passed, 4 bytes read (register)"], _SHADOW)
_wait("src/game/platform/psx_gte_transform.cpp", "Gte_ScaleMatrix", "truncating", ["return: 4 bytes read, ptr made"], _SHADOW)
_wait("src/game/platform/psx_gte_transform.cpp", "Math_Cos", "truncating", ["argument 1: ptr passed, 4 bytes read (register)"], _SHADOW)
_wait("src/game/platform/psx_gte_transform.cpp", "Gte_SetBackColor", "truncating",
      ["argument %d: ptr passed, 4 bytes read (register)" % i for i in (1, 2, 3)], _SHADOW)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--build", required=True, help="a configured and built build directory (the Linux preset's)")
    ap.add_argument("--libclang", default=None)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--only", action="append", default=[], help="glob over src/ paths of the translation units")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--all", action="store_true", help="print resolved sites without a finding too")
    ap.add_argument("--json", default=None, help="write every site and declaration as JSON")
    a = ap.parse_args()
    build = os.path.abspath(a.build)
    src_root = os.path.join(REPO, "src")
    db = [e for e in load_db(build) if os.path.normpath(os.path.join(e["directory"], e["file"])).startswith(src_root + os.sep)]
    if a.only:
        db = [e for e in db if any(fnmatch.fnmatch(norm(os.path.normpath(os.path.join(e["directory"], e["file"]))), g) for g in a.only)]
    if not db:
        print("fnptr_census: no translation unit under src/ in the compile database")
        return 2
    names = addr_names(build)
    results = []
    with ProcessPoolExecutor(max_workers=a.jobs) as ex:
        for r in ex.map(scan_tu, [(e, src_root, a.libclang) for e in db], chunksize=1):
            results.append(r)
    parse_errors = [r for r in results if r.get("nerrors") or r.get("error")]
    # the real type of each C-linkage function: its definition, else symbols.gen.h's prototype
    defs: dict[str, dict] = {}
    protos: dict[str, dict] = {}
    others: dict[str, list] = {}
    for r in results:
        for d in r["decls"]:
            if d["def"] and d["ours"] and not d["gen"]:
                defs[d["name"]] = d
            elif d["gen"]:
                protos.setdefault(d["name"], d)
            elif d["ours"]:
                others.setdefault(d["name"], []).append(d)
    rows = []
    seen = set()
    counts: dict[str, int] = {}
    for r in results:
        for site in r["sites"]:
            key = (site["file"], site["line"], site["col"], site["kind"])
            if key in seen:
                continue
            seen.add(key)
            real = None
            name = None
            if "target" in site:
                name = site["target"]
                if site.get("c_linkage") and name in defs:
                    real = defs[name]["shape"]
                else:
                    real = site.get("local")
            elif "va" in site:
                cands = names.get(site["va"], [])
                for n in cands:
                    if n in defs:
                        name, real = n, defs[n]["shape"]
                        break
                else:
                    for n in cands:
                        if n in protos:
                            name, real = n, protos[n]["shape"]
                            break
                if real is None:
                    name = (",".join(cands) or "?") + " @0x%X" % site["va"]
            if real is None:
                verdict = "unknown" if "va" in site else ("helper" if "param" in site else "unresolved")
                detail = site.get("runtime") or site.get("param") or ""
                findings = [(verdict, detail)]
            else:
                findings = compare(site["caller"], real) or [("ok", "")]
            for v, detail in findings:
                counts[v] = counts.get(v, 0) + 1
                rows.append((site["file"], site["line"], site["kind"], v, name or "-", detail, site["caller"]["spelling"],
                             real["spelling"] if real else "-"))
    # declarations against definitions
    for name, lst in sorted(others.items()):
        if name not in defs:
            continue
        for d in lst:
            for v, detail in compare(d["shape"], defs[name]["shape"]):
                if v in ("count",) or v in VERDICTS_FAIL:
                    key = (d["file"], d["line"], "decl")
                    if key in seen:
                        continue
                    seen.add(key)
                    counts["decl"] = counts.get("decl", 0) + 1
                    rows.append((d["file"], d["line"], "decl", "decl", name, v + ": " + detail, d["shape"]["spelling"],
                                 defs[name]["shape"]["spelling"]))
    for name, d in sorted(protos.items()):
        if name in defs:
            for v, detail in compare(d["shape"], defs[name]["shape"]):
                key = (d["file"], d["line"], "decl")
                if key in seen:
                    continue
                seen.add(key)
                counts["decl"] = counts.get("decl", 0) + 1
                rows.append((d["file"], d["line"], "decl", "decl", name, v + ": " + detail + " (symbols.gen.h against "
                             + defs[name]["file"] + ")", d["shape"]["spelling"], defs[name]["shape"]["spelling"]))
    rows.sort()
    # findings left on purpose: shown as "waiting", not failures; a stale entry is one
    found_keys = set()
    marked = []
    for row in rows:
        key = (row[0], row[4], row[3], row[5])
        if row[3] in VERDICTS_FAIL and key in WAITING:
            found_keys.add(key)
            row = row[:3] + ("waiting:" + row[3],) + row[4:]
        marked.append(row)
    rows = marked
    stale = sorted(set(WAITING) - found_keys) if not a.only else []
    if a.json:
        with open(a.json, "w") as f:
            json.dump({"rows": rows, "counts": counts}, f, indent=1)
    shown = 0
    for row in rows:
        if row[3] == "ok" and not a.all:
            continue
        if row[3] in ("unresolved", "helper", "unknown", "count") and not a.all:
            continue
        if row[3].startswith("waiting:") and a.check and not a.all:
            continue
        print("\t".join(str(x) for x in row))
        shown += 1
    total_tu = len(results)
    failing = [row for row in rows if row[3] in VERDICTS_FAIL]
    print("fnptr_census: %d translation units (%d with parse errors), %d sites; %s; %d waiting (listed in WAITING), %d failing" % (
        total_tu, len(parse_errors), len(seen), ", ".join("%s %d" % (k, v) for k, v in sorted(counts.items())),
        len(found_keys), len(failing)))
    for key in stale:
        print("  listed in WAITING but no longer found: %s" % "\t".join(key))
    for r in parse_errors[:10]:
        print("  parse errors in %s: %s" % (r["file"], (r.get("errors") or [r.get("error")])[:2]))
    if parse_errors:
        return 2
    if a.check and (failing or stale):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
