#!/usr/bin/env python
"""The width census: every site in src/ that assumes a pointer is 32 bits wide.

    python tools/width_census.py                 # write analysis/width_census.tsv, print the counts
    python tools/width_census.py --out FILE      # write elsewhere
    python tools/width_census.py --counts        # print the counts only, write nothing
    python tools/width_census.py --check         # fail on a live site the committed file does not list
    python tools/width_census.py --list CLASS    # print every site of one class, write nothing

Group XA1 of the 64-bit build round (docs/x64-build-round.md sections 0 and
3). Reads src/, symbols.toml and src/hook/exe_host.cpp - no game data - and
writes our own source lines, addresses and names only. It reuses
tools/data_census.py's reader (comment and string blanking, so a spelling in
a comment is not a site; the line numbering), as tools/reloc_census.py does.

**Standing** (the `standing` column): `fuzz` for any file whose name holds
`fuzz` (the shadow fuzz and its harnesses: hosted only, 32-bit for ever,
section 2 of the round doc), else `live`. Counts are given for both and for
the live files alone; --check gates the live rows only.

**The classes** (the `class` column), each with its `detail` sub-class:

  narrow        a native address narrowed into a 32-bit slot:
                `static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(x))`
                and the spellings with U / unsigned / unsigned long / uint32_t
                for the outer cast and size_t for the inner; `reinterpret_cast<U>(f)`
                (an integer cast of a pointer, `detail` prefixed `rc_int:`,
                which x86-64 refuses outright). `detail` says what is narrowed,
                read from the argument (classify_native):
                  helper_def    the body of a one-line helper (`Key(const void* p)`,
                                `Address`, `Addr`, `Va`, `AsU32`...): what it
                                narrows is its callers', listed as `narrow_call`
                                rows
                  func_ours     a function of ours (`&Name` of a function defined
                                in src/, `callee`, `entry`, `__builtin_return_address`)
                  object_ours   an object of ours (`&g_x`, a static table, a
                                local, a buffer of ours, `__ImageBase`)
                  pool          the draw pool (`draw_pool::Items()`, a packet or
                                primitive pointer into it: exe_layout.h's low block)
                  windows       a Windows object, function or handle (`BaseAddress`,
                                `ExceptionAddress`, the TEB, a module, `&GetTickCount`,
                                a DirectInput descriptor)
                  image         an address inside the image (a typed name, `Img(...)`,
                                a view of one, a pointer our text helpers return)
                  image_local   in a game file, a local pointer or an offset of one
                                (`s`, `o + 8`, `Cur() + 0x2E`, `Party(who)`, `rv::Bytes(X)`): the
                                record the function works on, handed out of the
                                image. By rule, not by eye: an object of ours in
                                a game file is `g_`-named or `&`-taken (above)
                  crt           our C run time's object (`Crt_GetPtd`, a FILE)
                  heap          a block from `malloc` / `Crt_malloc`: the heap the
                                game sees (XA2's low arena)
                  unread        none of the above matched: a row for stage B2 to
                                read by hand (the argument is in `token`)
  narrow_call   a call of a narrowing helper defined in the same file (or in a
                header the file includes), classified like `narrow`; the helper
                in `type`
  widen         a 32-bit word used as a native address:
                `reinterpret_cast<T>(static_cast<std::uintptr_t>(w))` and
                `reinterpret_cast<T>(::bof3::Img(va))` (Img returns the 32-bit
                word today). `type` is T; `detail`:
                  code_place    the word is `::bof3::Code(...)`, `CodeToCall`,
                                `CodePlace`, `CodePlaceToCall`: a code place
                  img_place     `::bof3::Img(...)`: a data place
                  word_read     a word read out of memory (`Long(Mem(...))`,
                                `UL(At(...))`, `L(...)`, `D(...)`): a pointer
                                the image holds, data or code by `type`
                  param         a bare name or member (`address`, `a`, `va`, `s.address`)
                  other         anything else (the argument is in `token`)
                and `fnptr` in `type`'s suffix ` [fn]` when T is a function-pointer
                type (spelled, or an alias `using X = R (__cdecl*)(...)`).
  long          `unsigned long` and `long` spelled in src/ (not `long long`,
                not `long double`): `detail` ul or l. And the generated ctypes:
                one row per symbols.toml [[data]] / [[block]] entry whose ctype
                spells `long` (`file` symbols.toml, `detail` ctype:<ctype>).
                Marked, not judged: Win64 is LLP64 (32 bits), LP64 is not.
  asm           one row per inline-asm block (`asm(`, `__asm__(`, with or
                without volatile / goto), `detail` the class of section 0 of the
                round doc, the first that applies:
                  stack         its text has push, pop, call, jmp, ret, a j-branch,
                                esp or ebp
                  fs            else it reads fs:
                  x87           else any mnemonic is an x87 operation other than
                                fnstcw / fldcw / fnstsw
                  cw            else every mnemonic is fnstcw, fldcw, stmxcsr or ldmxcsr
                  other         else (cltd; idivl, a byte load, `in`, an empty barrier)
                `type` holds the distinct mnemonics; `token` the first statement.
  asm_alias     an asm label alias on a declaration: `__asm__("_Music_Play")`
                (the i686 leading underscore spelled in the alias)
  asm_symbol    an operand in an asm template that names a C symbol by its
                i686 spelling, `call _Task_SetStackBase`, `jmp *(_soundrest_g+8)`;
                `detail` imagemove for `_Bof3_ImageMove` (an absolute load
                x86-64 cannot encode outside the low 2 GB), else underscore
  naked         a function with `__attribute__((naked))`; `token` its name
  fs_read       an `fs:` read (inside asm; `__readfsdword` if ever spelled)
  valloc_addr   a `VirtualAlloc` whose first argument is not null: a block at
                a chosen address
  sizeof_ptr    `sizeof(void*)`, `sizeof(T*)`, `sizeof(std::uintptr_t)`
  uintptr       every spelling of `std::uintptr_t` / `uintptr_t` (the ones
                inside a narrow or widen site too; the count is section 0's)
  fmt_place     a `%p` or `%08X` / `%08x` conversion in a format string;
                `token` the argument it prints, `detail`:
                  narrowed      the argument narrows a native address itself
                                (`reinterpret_cast<std::uintptr_t>`, a helper
                                `Address(p)`, a return address): B2's helper
                                decides what the print shows
                  pointer       the argument is a pointer (`%p`, `(void*)x`, a
                                Windows base, `self`): printed whole, 16 digits
                                on x86-64 where the design does not keep it low
                  word          anything else (a VA word, a count, a flag):
                                32 bits by nature, listed to be sure
  import_slot   one of exe_host.cpp kSlots' import slots: a 32-bit word of the
                image the exe host fills with a system DLL's function (`detail`
                windows_function, `token` DLL!name, `expr` the VA)
  ptr_member    a symbols.toml [[data]] row whose ctype holds a `*`: a
                pointer-typed member of the generated GameData (`using X_type
                = void *;` in symbols.gen.h), which is a 4-byte slot of the
                image that a 64-bit build would widen - XA3's build measured
                sizeof(GameData) 3,654,176 against 3,649,536 - and so stage
                B3's first unit (tools/gen_symbols.py emits a 32-bit slot for
                a pointer ctype). `token` the member, `type` its ctype (and
                count), `expr` its VA; `detail` what the slot holds, from the
                name and the row's evidence (SLOT_RULES):
                  windows_function   an import slot's typed name (the 14 above,
                                     seen from symbols.toml)
                  windows_object     a COM object of Windows' (DirectSound's,
                                     DirectInput's, the FMV's DirectDraw)
                  windows_handle     an HWND, HINSTANCE, HANDLE
                  ours_render        the render shim's objects (DirectDraw- and
                                     Direct3D-shaped, render_shim.cpp)
                  ours_decoder       the MP3 decoder's handle (src/audio/mp3.cpp)
                  ours_crt           a FILE of the C run time (file_io.cpp)
                  heap               a Crt_malloc block the game owns
                  code_table         a table of code words (bound by name)
                  image              a pointer into the image itself (a record,
                                     a string, a cell): 32 bits by design
  callconv      count only (no rows): __cdecl, __stdcall, __fastcall,
                __thiscall, WINAPI, CALLBACK, STDMETHODCALLTYPE spellings
  img_place     count only: `Img(`, `VaOf(`, `Code(` spellings

The TSV (write_tsv) holds `site` rows, then `file` rows (per file, standing
and class), then `#count` lines. `--check` keys every live site by (class,
detail, file, token, the line's text with whitespace collapsed) - not the
line number - and fails when a key's count rises above the committed file's
or a key is new (tools/reloc_bare.py's rule): a new width assumption in a
live file is refused until the census is regenerated on purpose, and the
reviewer sees the row in the diff. Stage B lowers the counts.
"""
import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data_census as dc  # noqa: E402

ROOT = dc.ROOT
DEFAULT_OUT = 'analysis/width_census.tsv'
EXE_HOST = 'src/hook/exe_host.cpp'

CLASSES = ('narrow', 'narrow_call', 'widen', 'long', 'asm', 'asm_alias', 'asm_symbol', 'naked',
           'fs_read', 'valloc_addr', 'sizeof_ptr', 'uintptr', 'fmt_place', 'import_slot', 'ptr_member')
ASM_CLASSES = ('x87', 'stack', 'cw', 'fs', 'other')
NARROW_CLASSES = ('helper_def', 'func_ours', 'object_ours', 'pool', 'windows', 'image', 'image_local', 'crt', 'heap', 'unread')
WIDEN_CLASSES = ('code_place', 'img_place', 'word_read', 'param', 'other')
SLOT_CLASSES = ('windows_function', 'windows_object', 'windows_handle', 'ours_render', 'ours_decoder',
                'ours_crt', 'heap', 'code_table', 'image')
CALLCONV = ('__cdecl', '__stdcall', '__fastcall', '__thiscall', 'WINAPI', 'CALLBACK', 'STDMETHODCALLTYPE')

U32 = r'(?:std::uint32_t|uint32_t|U|unsigned|unsigned\s+int|unsigned\s+long|DWORD|ULONG)'
UPTR = r'(?:std::uintptr_t|uintptr_t|std::size_t|size_t|ULONG_PTR|DWORD_PTR)'
NARROW = re.compile(r'static_cast<\s*' + U32 + r'\s*>\s*\(\s*reinterpret_cast<\s*' + UPTR + r'\s*>\s*\(')
RC_INT = re.compile(r'reinterpret_cast<\s*(?:std::uint32_t|uint32_t|U|unsigned|unsigned\s+long|DWORD|std::int32_t|int)\s*>\s*\(')
WIDEN = re.compile(r'reinterpret_cast<\s*([^;{}()]*?)\s*>\s*\(\s*static_cast<\s*' + UPTR + r'\s*>\s*\(')
WIDEN_IMG = re.compile(r'reinterpret_cast<\s*([^;{}()]*?)\s*>\s*\(\s*(?=(?:::)?bof3::(?:Img|Code|CodeToCall|CodePlace|CodePlaceToCall)\s*\()')
HELPER_DEF = re.compile(r'(?:^|[;}\n])[ \t]*(?:(?:static|inline|constexpr)\s+)*(?:std::u?int32_t|U|unsigned|unsigned\s+long)\s+'
                        r'(\w+)\s*\(\s*(?:const\s+)?(?:volatile\s+)?(?:const\s+)?(?:void|unsigned\s+char|char)\s*\*\s*(\w+)\s*\)\s*\{'
                        r'\s*return\s+(?:static_cast<std::int32_t>\()?static_cast<\s*' + U32 + r'\s*>\s*\(\s*reinterpret_cast<\s*' + UPTR)
ASM = re.compile(r'(?<![\w.])(?:__asm__|__asm|asm)\s*(?:(?:volatile|__volatile__|goto|inline)\s*)*\(')
NAKED = re.compile(r'__attribute__\s*\(\(\s*(?:[a-z_]+\s*,\s*)*naked\b[^)]*\)\)')
FUNC_NAME_AFTER = re.compile(r'[^;{}]*?\b(\w+)\s*\(')
FUNC_NAME_BEFORE = re.compile(r'\b(\w+)\s*\([^;{}()]*\)\s*(?:const\s*)?$')
VALLOC = re.compile(r'\bVirtualAlloc(?:Ex)?\s*\(')
SIZEOF_PTR = re.compile(r'\bsizeof\s*\(\s*(?:void\s*\*|[\w:]+\s*\*|(?:std::)?uintptr_t)\s*\)')
UINTPTR = re.compile(r'(?<![\w])(?:std::)?uintptr_t\b')
LONG = re.compile(r'(?<![\w:])(unsigned\s+long|long)\b(?!\s*(?:long|double)\b)')
FMT = re.compile(r'%(?:0?8[Xx]|p)')
FS = re.compile(r'%%?fs:|__readfsdword|__readfsqword')
ASM_LABEL_ALIAS = re.compile(r'\)\s*(?:__asm__|__asm|asm)\s*\(\s*"(_\w+)"\s*\)')
# an asm operand naming a C symbol by the i686 spelling: a leading underscore
# after a mnemonic, with or without the `*` / `(` of an indirect form
ASM_SYMBOL = re.compile(r'(?<![\w%$.])_[A-Za-z]\w*')
FP_TYPE = re.compile(r'\(\s*(?:__\w+\s*|WINAPI\s*|CALLBACK\s*)?\*\s*\w*\s*\)')
WORD_READ = re.compile(r'^(?:static_cast<\s*(?:std::uint32_t|U|unsigned)\s*>\s*\(\s*)*(?:[\w:]+::)?(?:Long|UL|L|D|Dword|U32|GetLong|Mem|At|Word|W)\s*\(')
PLACER = re.compile(r'^(?:::)?bof3::(Img|Code|CodeToCall|CodePlace|CodePlaceToCall)\s*\(')

WINDOWS_ARG = re.compile(r'BaseAddress|ExceptionAddress|\bteb\b|NtCurrentTeb|GetModuleHandle|GetProcAddress|\bmodule\b|\bdll\b'
                         r'|\bhwnd\b|\bHMODULE\b|\brgodf\b|\bpguid\b|&GetTickCount\b|StackBase|StackLimit|\bdevice\b|\bhandle\b')
CRT_ARG = re.compile(r'\bCrt_\w+|\bFILE\b|\bstream\b')
POOL_ARG = re.compile(r'draw_pool::|\bpacket\b|\bprim\b|\bpacket_next\b|Gfx_PacketNext|\bot\b|\bhead\b|\bitems\b|g_prims')
FUNC_ARG = re.compile(r'^(?:callee|entry|code|ret|f|fn|handler|target|__builtin_return_address\(\d\)|Bind_Ours\(.*\))$')


def standing_of(rel):
    return 'fuzz' if 'fuzz' in os.path.basename(rel).lower() else 'live'


# --------------------------------------------------------------------------
# The names the classifier needs
# --------------------------------------------------------------------------

FUNC_DEF = re.compile(r'(?:^|[;}\n])[ \t]*(?:(?:extern\s+"[^"]*"|static|inline|constexpr|template\s*<[^>]*>|\w+_EXPORT'
                      r'|__attribute__\s*\(\([^()]*(?:\([^()]*\))?[^()]*\)\))\s+)*'
                      r'(?:(?:const|unsigned|signed|volatile|struct)\s+)*[A-Za-z_][\w:<>]*(?:\s*[*&])*\s+'
                      r'(?:__\w+\s+|WINAPI\s+|CALLBACK\s+)?(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:\{|;)')


def defined_functions(files):
    """Names defined or declared as functions anywhere in src/ (a return
    type, a name, a parameter list, then `{` or `;`), and the names of
    symbols.toml's functions."""
    names = set()
    for f in files:
        for m in FUNC_DEF.finditer(f.code):
            names.add(m.group(1))
    names -= {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'static_assert', 'catch'}
    return names


def typed_names(toml):
    import tomllib
    with open(toml, 'rb') as fh:
        t = tomllib.load(fh)
    data = t.get('data', [])
    blocks = t.get('block', [])
    funcs = t.get('func', [])
    typed = {e['name'] for e in data + blocks if 'ctype' in e}
    return typed, {e['name'] for e in funcs}, data, blocks


def fp_aliases(files):
    out = set()
    for f in files:
        for m in re.finditer(r'\busing\s+(\w+)\s*=\s*([^;]*);', f.code):
            if FP_TYPE.search(m.group(2)):
                out.add(m.group(1))
        for m in re.finditer(r'\btypedef\b[^;]*\(\s*(?:__\w+\s*)?\*\s*(\w+)\s*\)\s*\(', f.code):
            out.add(m.group(1))
    return out


# --------------------------------------------------------------------------
# The classifiers
# --------------------------------------------------------------------------

def strip_parens(a):
    a = ' '.join(a.split())
    while a.startswith('(') and a.endswith(')') and dc.balanced_args(a, 0) and dc.balanced_args(a, 0)[1] == len(a):
        a = a[1:-1].strip()
    return a


def classify_native(arg, funcs, typed, toml_funcs, rel=''):
    """The sub-class of a narrowed native address, from its argument's text."""
    a = strip_parens(arg)
    # a pointer cast around the argument says nothing about what it is
    while True:
        m = re.match(r'^(?:reinterpret_cast|static_cast|const_cast)\s*<[^<>;]*(?:<[^<>;]*>[^<>;]*)*>\s*\((.*)\)$', a)
        if not m or dc.balanced_args(a, m.start(1) - 1) is None or dc.balanced_args(a, m.start(1) - 1)[1] != len(a):
            break
        a = strip_parens(m.group(1))
    if not a:
        return 'unread'
    if a == '&__ImageBase':
        return 'object_ours'
    if WINDOWS_ARG.search(a):
        return 'windows'
    if CRT_ARG.search(a):
        return 'crt'
    if re.search(r'\b(?:malloc|calloc|Crt_malloc|Crt_calloc)\s*\(|\bnew\b', a):
        return 'heap'
    if FUNC_ARG.match(a) or re.search(r'\.ours\b|^Stub(?:For|Of|At)\s*\(|^&\s*Stub\w*<', a):
        return 'func_ours'
    if re.search(r'\.data\(\)', a):
        return 'object_ours'
    if re.match(r'^AddressOf\s*\(', a):
        return 'image'
    if re.match(r'^&\s*\w+(?:\.\w+)+$', a):
        return 'object_ours'   # a member of a local or static of ours
    m = re.match(r'^&\s*(?:::)?((?:\w+::)*)(\w+)$', a)
    if m:
        name = m.group(2)
        if name in typed:
            return 'image'
        if name in funcs or name in toml_funcs:
            return 'func_ours'
        if re.match(r'g_|k[A-Z]', name) or m.group(1):
            return 'object_ours'
        return 'unread'
    m = re.match(r'^&\s*((?:\w+::)*)(\w+)\s*\[', a)
    if m:
        return 'object_ours' if re.match(r'g_|k[A-Z]', m.group(2)) else 'unread'
    if PLACER.match(a) or re.search(r'(?:^|[^\w:])(?:::)?bof3::Img\s*\(', a):
        return 'image'
    if POOL_ARG.search(a):
        return 'pool'
    m = re.match(r'^((?:\w+::)*)(\w+)\b', a)
    if m and m.group(2) in typed:
        return 'image'
    if re.match(r'^rv::As<records::', a) or re.match(r'^Enemy\(\)', a):
        return 'image'
    if m and re.match(r'^g_\w+', a):
        return 'object_ours'
    if re.match(r'^(?:Widescreen_SlideOperand|Labels_Slot|CountList|RouteTo|BH_CALL|g\.msg_system_ptr)\b', a):
        # a pointer our helper returns into the image's text or a table of ours
        return 'image'
    if rel.startswith('src/game/') and (
            re.match(r'^(?:this|[a-z]\w*)(?:(?:\.|->)\w+)*(?:\[[^\]]*\])?(?:\s*[-+]\s*[^&]+)?$', a)
            or re.match(r'^(?:[\w:]+::)?[A-Z]\w*\([^()]*(?:\([^()]*\)[^()]*)*\)(?:\s*\[[^\]]*\])?(?:\s*[-+]\s*.+)?$', a)
            or re.match(r'^rv::(?:Bytes|As)\b', a) or re.match(r'^(?:At|Mem|Ptr|PtrAt|Bytes|P)\s*\(', a)):
        # a game file's local pointer, or an offset of one, or a view of a
        # typed name: the record the function works on, which the game handed
        # it out of the image. Read by this rule, not by eye: an object of
        # ours in a game file is `g_`-named or `&`-taken (the classes above)
        return 'image_local'
    return 'unread'


def classify_word(arg):
    a = strip_parens(arg)
    m = PLACER.match(a)
    if m:
        return 'img_place' if m.group(1) == 'Img' else 'code_place'
    if WORD_READ.match(a):
        return 'word_read'
    if re.match(r'^[\w:]+(?:(?:\.|->)\w+)*(?:\[[^\]]*\])?$', a):
        return 'param'
    return 'other'


def asm_text(code, raw, a, b):
    """The template string of the asm block at code[a:b]: the string literals
    before the first `:` (the operands' strings are constraints), located in
    the blanked code (so a comment between two literals is skipped) and read
    from raw, concatenated, with \\n and \\t decoded."""
    seg = code[a:b]
    k = seg.find('(')
    tmpl = []
    j = k + 1
    while j < len(seg):
        m = re.match(r'\s*"', seg[j:])
        if not m:
            break
        s = j + m.end()
        e = seg.find('"', s)
        if e < 0:
            break
        tmpl.append(raw[a + s:a + e])
        j = e + 1
    text = ''.join(tmpl)
    return text.replace('\\n', '\n').replace('\\t', '\t').replace('\\"', '"')


def asm_statements(text):
    out = []
    for stmt in re.split(r'[\n;]', text):
        stmt = stmt.strip()
        if not stmt or stmt.startswith(('.', '#')):
            continue
        stmt = re.sub(r'^\d+:\s*', '', stmt)   # a local label
        if not stmt:
            continue
        out.append(stmt)
    return out


def classify_asm(text):
    stmts = asm_statements(text)
    mnems = []
    for s in stmts:
        m = re.match(r'([a-z][a-z0-9]*)', s)
        if m:
            mnems.append(m.group(1))
    lower = text.lower()
    if re.search(r'\b(push|pop|call|jmp|ret)[a-z]*\b', lower) or re.search(r'%[er]?[se]p\b', lower) \
            or any(re.match(r'^j[a-z]+$', m) for m in mnems):
        return 'stack', mnems
    if re.search(r'fs:', lower):
        return 'fs', mnems
    if any(m.startswith('f') and m not in ('fnstcw', 'fldcw', 'fnstsw') for m in mnems):
        return 'x87', mnems
    if mnems and all(m in ('fnstcw', 'fldcw', 'stmxcsr', 'ldmxcsr') for m in mnems):
        return 'cw', mnems
    return 'other', mnems


def format_args(code, pos):
    """The call that holds the string literal at pos, its arguments after the
    format string: (format_index, args) or None."""
    br, _ = reloc_enclosing(code, pos)
    if br is None:
        return None
    r = dc.balanced_args(code, br)
    if r is None:
        return None
    args, k = r[0], br + 1
    idx = None
    for i, a in enumerate(args):
        if k <= pos < k + len(a):
            idx = i
            break
        k += len(a) + 1
    if idx is None:
        return None
    return idx, [' '.join(a.split()) for a in args[idx + 1:]]


def reloc_enclosing(code, pos, limit=3000):
    depth = {')': 0, ']': 0, '}': 0}
    lo = max(0, pos - limit)
    k = pos - 1
    while k >= lo:
        c = code[k]
        if c in ')]}':
            depth[c] += 1
        elif c in '([{':
            close = {'(': ')', '[': ']', '{': '}'}[c]
            if depth[close]:
                depth[close] -= 1
            elif c == '(':
                return k, code[max(0, k - 100):k]
            else:
                return None, ''
        elif c == ';' and not any(depth.values()):
            return None, ''
        k -= 1
    return None, ''


# a format argument that narrows a native address itself (the narrowing is
# B2's helper's; the print then shows the low word), or that is a pointer
# (printed whole by %p: 16 digits on x86-64, a VA only if the design keeps it low)
NARROWED_FMT_ARG = re.compile(r'reinterpret_cast<\s*(?:std::)?uintptr_t|\buintptr_t\b|__builtin_return_address|'
                              r'\b(?:Key|AsU32|Va|Address|Addr|U32|PtrValue)\s*\(|&__ImageBase')
POINTER_FMT_ARG = re.compile(r'\((?:const\s+)?void\s*\*\)|<(?:const\s+)?void\s*\*>|\bself\b|\bthis\b|BaseAddress|ExceptionAddress|'
                             r'\bteb\b|StackBase|StackLimit|\bmodule\b|\bdll\b|\bvt\b|\bslot\b|\bbase\b|\bat\b|\bpool\b')


# --------------------------------------------------------------------------
# The scan
# --------------------------------------------------------------------------

def scan(toml='symbols.toml'):
    files = dc.load_sources()
    by_rel = {f.rel: f for f in files}
    funcs = defined_functions(files)
    typed, toml_funcs, data_entries, block_entries = typed_names(toml)
    aliases = fp_aliases(files)
    sites = []
    extra = collections.Counter()

    def add(f, pos, klass, detail, token, typ='-', raw_line=False):
        ln = dc.line_of(f.starts, pos)
        src = f.raw.split('\n')[ln - 1] if raw_line else f.lines[ln - 1]
        sites.append({'file': f.rel, 'line': ln, 'class': klass, 'detail': detail,
                      'standing': standing_of(f.rel), 'token': ' '.join(str(token).split())[:120],
                      'type': ' '.join(str(typ).split())[:80], 'expr': ' '.join(src.split())[:160]})

    helpers = collections.defaultdict(set)   # rel -> {helper name}
    for f in files:
        for m in HELPER_DEF.finditer(f.code):
            helpers[f.rel].add(m.group(1))

    for f in files:
        code, raw = f.code, f.raw

        # narrow: the two-cast spelling
        for m in NARROW.finditer(code):
            r = dc.balanced_args(code, m.end() - 1)
            arg = r[0][0] if r else ''
            ln = dc.line_of(f.starts, m.start())
            line = f.lines[ln - 1]
            if re.match(r'\s*(?:(?:static|inline|constexpr)\s+)*(?:std::u?int32_t|U|unsigned|unsigned\s+long)\s+\w+\s*\('
                        r'\s*(?:const\s+)?(?:volatile\s+)?(?:const\s+)?(?:void|unsigned\s+char|char)\s*\*\s*\w+\s*\)\s*\{', line):
                detail = 'helper_def'
            elif re.match(r'\s*return\s+(?:static_cast<std::int32_t>\()?static_cast<', line) and ln >= 2 \
                    and re.match(r'\s*(?:(?:static|inline|constexpr)\s+)*(?:std::u?int32_t|U|unsigned|unsigned\s+long)\s+\w+\s*\('
                                 r'\s*(?:const\s+)?(?:volatile\s+)?(?:const\s+)?(?:void|unsigned\s+char)\s*\*\s*\w+\s*\)\s*\{?\s*$',
                                 f.lines[ln - 2]):
                detail = 'helper_def'
                helpers[f.rel].add(re.search(r'(\w+)\s*\(', f.lines[ln - 2]).group(1))
            else:
                detail = classify_native(arg, funcs, typed, toml_funcs, f.rel)
            add(f, m.start(), 'narrow', detail, arg)
        # narrow: reinterpret_cast<U>(pointer)
        for m in RC_INT.finditer(code):
            r = dc.balanced_args(code, m.end() - 1)
            arg = r[0][0] if r else ''
            a = strip_parens(arg)
            # an integer cast of an integer is a value cast, not a narrowing
            if re.match(r'^(?:0x[0-9A-Fa-f]+|\d+)', a) or re.match(r'^(?:[\w:]+::)?k[A-Z]\w*(?:\.v)?$', a) \
                    or re.match(r'^(?:[\w:]+::)?(?:Img|Code|VaOf|Long|UL|L|D)\s*\(', a):
                continue
            add(f, m.start(), 'narrow', 'rc_int:' + classify_native(arg, funcs, typed, toml_funcs, f.rel), arg)

        # widen
        for rx, how in ((WIDEN, 'word'), (WIDEN_IMG, 'img')):
            for m in rx.finditer(code):
                targ = ' '.join(m.group(1).split())
                r = dc.balanced_args(code, m.end() - 1)
                arg = r[0][0] if r else ''
                if how == 'img':
                    # reinterpret_cast<T>(::bof3::Img(va) + off): the placer is the word
                    arg = arg
                detail = classify_word(arg)
                fn = FP_TYPE.search(targ) is not None or targ.strip().rstrip('*').strip() in aliases \
                    or re.match(r'^(?:[\w:]+::)?(?:Handler|HookFn|Step|\w*Fn)$', targ.strip()) is not None
                add(f, m.start(), 'widen', detail, arg, targ + (' [fn]' if fn else ''))

        # long
        for m in LONG.finditer(code):
            add(f, m.start(), 'long', 'ul' if m.group(1).startswith('unsigned') else 'l', m.group(1))

        # asm blocks, aliases, symbol operands
        for m in ASM.finditer(code):
            depth, k = 0, m.end() - 1
            while k < len(code):
                c = code[k]
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            before = code[max(0, m.start() - 80):m.start()].rstrip()
            text = asm_text(code, raw, m.start(), k + 1)
            if before.endswith(')') and re.match(r'^_\w+$', text.strip()):
                add(f, m.start(), 'asm_alias', 'underscore', text.strip(), raw_line=True)
                continue
            klass, mnems = classify_asm(text)
            stmts = asm_statements(text)
            add(f, m.start(), 'asm', klass, stmts[0] if stmts else '(empty)',
                ' '.join(sorted(set(mnems))) if mnems else '-', raw_line=True)
            for fm in FS.finditer(text):
                add(f, m.start(), 'fs_read', 'asm', text[max(0, fm.start() - 12):fm.end() + 8].strip(), raw_line=True)
            for s in stmts:
                body = re.sub(r'^[a-z][a-z0-9]*\s*', '', s)
                for sm in ASM_SYMBOL.finditer(body):
                    name = sm.group(0)
                    add(f, m.start(), 'asm_symbol', 'imagemove' if name == '_Bof3_ImageMove' else 'underscore',
                        name, s, raw_line=True)

        # naked
        for m in NAKED.finditer(code):
            after = code[m.end():m.end() + 300]
            nm = FUNC_NAME_AFTER.match(after)
            bm = FUNC_NAME_BEFORE.search(code[max(0, m.start() - 300):m.start()])
            name = (bm.group(1) if bm else None) or (nm.group(1) if nm else '?')
            add(f, m.start(), 'naked', '-', name)

        # VirtualAlloc with an address
        for m in VALLOC.finditer(code):
            r = dc.balanced_args(code, m.end() - 1)
            if r is None or not r[0]:
                continue
            first = r[0][0] if not m.group(0).startswith('VirtualAllocEx') else (r[0][1] if len(r[0]) > 1 else '')
            if re.match(r'^\s*(?:nullptr|NULL|0)\s*$', first) or re.match(r'^\s*[\w:]+\s*$', first) and first.strip() in (
                    'size', 'bytes', 'n', 'kBytes'):
                continue
            if re.match(r'^\s*$', first) or re.match(r'^\s*[\w:]+\s*$', first) and re.search(r'\b' + re.escape(first.strip())
                                                                                           + r'\s*=\s*VirtualAlloc', code):
                continue
            # a declaration (`void* VirtualAlloc(`) has no call arguments of this shape
            if re.match(r'^\s*(?:LPVOID|void\s*\*|DWORD|SIZE_T)\b', first):
                continue
            add(f, m.start(), 'valloc_addr', '-', first)

        # sizeof a pointer
        for m in SIZEOF_PTR.finditer(code):
            add(f, m.start(), 'sizeof_ptr', '-', m.group(0))

        # uintptr_t
        for m in UINTPTR.finditer(code):
            add(f, m.start(), 'uintptr', '-', m.group(0))

        # formats that print a place: read from raw (strings are blanked in code)
        for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', raw):
            if not FMT.search(m.group(1)):
                continue
            if m.start() < len(code) and code[m.start()] != '"':
                continue   # inside a comment
            convs = list(re.finditer(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z|I64|I32|I)?[diouxXpsc%])', m.group(1)))
            fa = format_args(code, m.start())
            args = fa[1] if fa else []
            # a format with %% has no argument there
            ai = 0
            for c in convs:
                spec = c.group(0)
                if spec == '%%':
                    continue
                arg = args[ai] if ai < len(args) else '?'
                ai += 1
                if not FMT.fullmatch(spec):
                    continue
                if NARROWED_FMT_ARG.search(arg):
                    detail = 'narrowed'
                elif spec == '%p' or POINTER_FMT_ARG.search(arg):
                    detail = 'pointer'
                else:
                    detail = 'word'
                add(f, m.start(), 'fmt_place', detail, arg, spec, raw_line=True)

        # callconv and the placers: counts only
        for cc in CALLCONV:
            n = len(re.findall(r'\b' + cc + r'\b', code))
            if n:
                extra[f'callconv_{cc}'] += n
                extra[f'callconv_{cc}_{standing_of(f.rel)}'] += n
        for nm in ('Img', 'VaOf', 'Code'):
            n = len(re.findall(r'\b' + nm + r'\(', code))
            if n:
                extra[f'img_place_{nm}'] += n
                extra[f'img_place_{nm}_{standing_of(f.rel)}'] += n

    # narrow_call: every call of a narrowing helper, in its file and in the
    # files that include it when it is a header
    for rel, names in sorted(helpers.items()):
        hf = by_rel[rel]
        pool = [hf] + ([g for g in files if hf in dc.include_closure(g, by_rel)] if rel.endswith('.h') else [])
        for g in pool:
            for name in sorted(names):
                for m in re.finditer(r'(?<![\w.>:])' + re.escape(name) + r'\s*\(', g.code):
                    ln = dc.line_of(g.starts, m.start())
                    if re.match(r'\s*(?:(?:static|inline|constexpr)\s+)*(?:std::u?int32_t|U|unsigned|unsigned\s+long)\s+'
                                + re.escape(name) + r'\s*\(', g.lines[ln - 1]):
                        continue   # the definition
                    r = dc.balanced_args(g.code, m.end() - 1)
                    arg = r[0][0] if r and r[0] else ''
                    add(g, m.start(), 'narrow_call', classify_native(arg, funcs, typed, toml_funcs, g.rel), arg, name)

    # the generated ctypes that spell long
    toml_lines = open(os.path.join(ROOT, toml), encoding='utf-8').read().split('\n')
    for ln, line in enumerate(toml_lines, 1):
        m = re.match(r'ctype\s*=\s*"([^"]*)"', line)
        if m and re.search(r'\blong\b', m.group(1)):
            sites.append({'file': toml, 'line': ln, 'class': 'long', 'detail': 'ctype:' + m.group(1),
                          'standing': 'live', 'token': m.group(1), 'type': '-', 'expr': ' '.join(line.split())[:160]})

    # the slots
    sites.extend(slot_rows(data_entries, block_entries))
    sites.sort(key=lambda r: (r['file'], r['line'], r['class'], r['detail'], r['token']))
    return sites, extra


SLOT_RULES = [
    # (regex on the name, regex on the evidence, detail)
    (r'^(Imp_|PsxRest_Import|Sound_Import|Shell_Import)', None, 'windows_function'),
    (r'^(Snd_Device|Snd_Primary|Music_Buffer|Music_Notify|SndStream_Buffer)$', None, 'windows_object'),
    (r'^DInput_', None, 'windows_object'),
    (r'^Fmv_DDraw$', None, 'windows_object'),
    (r'^(Music_Events|Game_Hwnd|Game_HInstance)$', None, 'windows_handle'),
    (r'^(Dd_|DDraw_|D3d_)', None, 'ours_render'),
    (r'^Music_Decoder$', None, 'ours_decoder'),
    (r'^File_Slots$', None, 'ours_crt'),
    (r'^(Music_Data|Music_File|Font_GlyphData)$', None, 'heap'),
    (None, r'\bCrt_malloc\b', 'heap'),
    (r'(_Kinds|_Phases|_Steps|_Handlers|_States|_CounterOps|_Conditions|_Types)$', None, 'code_table'),
]


def slot_rows(data_entries, block_entries):
    rows = []
    text = open(os.path.join(ROOT, EXE_HOST), encoding='utf-8').read()
    starts = dc.line_starts(text)
    kslots = {}
    for m in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*"([^"]+)"\s*,\s*(?:"([^"]+)"|nullptr)\s*,\s*(\d+)\s*\}', text):
        va = int(m.group(1), 16)
        name = m.group(3) or f'#{m.group(4)}'
        kslots[va] = name
        rows.append({'file': EXE_HOST, 'line': dc.line_of(starts, m.start()), 'class': 'import_slot',
                     'detail': 'windows_function', 'standing': 'live', 'token': f'{m.group(2)}!{name}',
                     'type': 'FARPROC', 'expr': f'{m.group(1)} kSlots'})
    toml_text = open(os.path.join(ROOT, 'symbols.toml'), encoding='utf-8').read().split('\n')
    line_of_name = {}
    for ln, line in enumerate(toml_text, 1):
        m = re.match(r'name\s*=\s*"([^"]+)"', line)
        if m and m.group(1) not in line_of_name:
            line_of_name[m.group(1)] = ln
    for e in data_entries + block_entries:
        ct = e.get('ctype', '')
        if '*' not in ct:
            continue
        name, ev = e['name'], e.get('evidence', '') or ''
        detail = None
        for nrx, erx, d in SLOT_RULES:
            if nrx and re.search(nrx, name) or erx and re.search(erx, ev):
                detail = d
                break
        if detail is None:
            if e['pc'] in kslots:
                detail = 'windows_function'
            elif re.search(r'IDirect(Sound|Input)', ev):
                detail = 'windows_object'
            elif re.search(r'IDirect(Draw|3D)', ev):
                detail = 'ours_render'
            elif ct.startswith('void') and e.get('count', 1) != 1 and re.search(r'\bcode\b|\bcall\b|\bjmp\b', ev):
                detail = 'code_table'
            else:
                detail = 'image'
        rows.append({'file': 'symbols.toml', 'line': line_of_name.get(name, 0), 'class': 'ptr_member', 'detail': detail,
                     'standing': 'live', 'token': name, 'type': ct + (f' [{e["count"]}]' if e.get('count', 1) != 1 else ''),
                     'expr': f'{e["pc"]:#x}'})
    return rows


# --------------------------------------------------------------------------
# Counts, output, check
# --------------------------------------------------------------------------

def counts_of(sites, extra):
    c = collections.OrderedDict()
    live = [r for r in sites if r['standing'] == 'live']
    c['sites'] = len(sites)
    c['sites_live'] = len(live)
    c['sites_fuzz'] = len(sites) - len(live)
    c['fuzz_files'] = len({r['file'] for r in sites if r['standing'] == 'fuzz'})
    for k in CLASSES:
        rows = [r for r in sites if r['class'] == k]
        lrows = [r for r in rows if r['standing'] == 'live']
        c[f'{k}'] = len(rows)
        c[f'{k}_live'] = len(lrows)
        c[f'{k}_live_files'] = len({r['file'] for r in lrows})
        c[f'{k}_fuzz'] = len(rows) - len(lrows)
        c[f'{k}_fuzz_files'] = len({r['file'] for r in rows if r['standing'] == 'fuzz'})
        subs = sorted({r['detail'] for r in rows})
        for s in subs:
            srows = [r for r in rows if r['detail'] == s]
            c[f'{k}_{s}'] = len(srows)
            c[f'{k}_{s}_live'] = sum(1 for r in srows if r['standing'] == 'live')
            c[f'{k}_{s}_live_files'] = len({r['file'] for r in srows if r['standing'] == 'live'})
    c['asm_live_x87_only_files'] = len({f for f in {r['file'] for r in live if r['class'] == 'asm'}
                                        if all(r['detail'] == 'x87' for r in live if r['class'] == 'asm' and r['file'] == f)})
    c['widen_live_fnptr'] = sum(1 for r in live if r['class'] == 'widen' and r['type'].endswith('[fn]'))
    c['long_src'] = sum(1 for r in sites if r['class'] == 'long' and r['file'] != 'symbols.toml')
    c['long_src_game'] = sum(1 for r in sites if r['class'] == 'long' and r['file'].startswith('src/game/'))
    c['long_src_game_ul'] = sum(1 for r in sites if r['class'] == 'long' and r['file'].startswith('src/game/') and r['detail'] == 'ul')
    c['long_ctype'] = sum(1 for r in sites if r['class'] == 'long' and r['file'] == 'symbols.toml')
    for k, v in sorted(extra.items()):
        c[k] = v
    return c


SITE_HDR = ['rec', 'file', 'line', 'class', 'detail', 'standing', 'token', 'type', 'expr']
FILE_HDR = ['rec', 'file', 'standing', 'class', 'detail', 'n']


def key_of(r):
    return (r['class'], r['detail'], r['file'], r['token'], r['expr'])


def write_tsv(path, sites, counts):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    clean = lambda s: str(s).replace('\t', ' ')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# tools/width_census.py: every site in src/ that assumes a pointer is 32 bits wide. '
                'Our own source lines, addresses and names only (docs/x64-build-round.md 3, XA1).\n')
        f.write('#' + '\t'.join(SITE_HDR) + '\n')
        for r in sites:
            f.write('site\t' + '\t'.join(clean(r[k]) for k in SITE_HDR[1:]) + '\n')
        f.write('#' + '\t'.join(FILE_HDR) + '\n')
        per = collections.Counter((r['file'], r['standing'], r['class'], r['detail']) for r in sites)
        for (fl, st, k, d), n in sorted(per.items()):
            f.write(f'file\t{fl}\t{st}\t{k}\t{d}\t{n}\n')
        for k, v in counts.items():
            f.write(f'#count {k}\t{v}\n')


def committed_keys(path):
    c = collections.Counter()
    with open(path, encoding='utf-8') as f:
        for row in f:
            if not row.startswith('site\t'):
                continue
            parts = row.rstrip('\n').split('\t')
            r = dict(zip(SITE_HDR, parts))
            if r['standing'] == 'live':
                c[key_of(r)] += 1
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--toml', default='symbols.toml')
    ap.add_argument('--counts', action='store_true', help='print the counts, write nothing')
    ap.add_argument('--check', action='store_true', help='fail on a live site the committed --out file does not list')
    ap.add_argument('--list', metavar='CLASS', help='print every site of one class (or CLASS:DETAIL), write nothing')
    a = ap.parse_args()
    os.chdir(ROOT)
    sites, extra = scan(a.toml)
    counts = counts_of(sites, extra)
    if a.list:
        k, _, d = a.list.partition(':')
        for r in sites:
            if r['class'] == k and (not d or r['detail'] == d):
                print(f"{r['standing']}\t{r['file']}:{r['line']}\t{r['class']}\t{r['detail']}\t{r['token']}\t{r['type']}\t{r['expr']}")
        print(f"{sum(1 for r in sites if r['class'] == k and (not d or r['detail'] == d))} rows")
        return 0
    if a.check:
        if not os.path.exists(a.out):
            print(f'width_census --check: {a.out} is missing; run tools/width_census.py')
            return 1
        have = committed_keys(a.out)
        now = collections.Counter(key_of(r) for r in sites if r['standing'] == 'live')
        new = [(k, n - have.get(k, 0)) for k, n in now.items() if n > have.get(k, 0)]
        stale = [k for k in have if now.get(k, 0) < have[k]]
        print(f"width_census: {counts['sites_live']} live sites in {len({r['file'] for r in sites if r['standing'] == 'live'})} files, "
              + ', '.join(f"{k} {counts[k + '_live']}" for k in CLASSES))
        if stale:
            print(f'width_census --check: {len(stale)} listed key(s) fewer or gone (fine; rerun tools/width_census.py to drop them)')
        if new:
            lines = {}
            for r in sites:
                lines.setdefault(key_of(r), r['line'])
            print(f'width_census --check: {sum(n for _, n in new)} new width assumption(s) in live files - '
                  f'a narrowing goes through the Low32 helper, a widening through Img / Code, asm by its class '
                  f'(docs/x64-build-round.md 3 and 4); if the site is deliberate, rerun tools/width_census.py and commit {a.out}:')
            for (k, d, fl, tok, expr), n in sorted(new, key=lambda x: (x[0][2], lines.get(x[0], 0))):
                print(f'  {k:12} {d:14} {fl}:{lines[(k, d, fl, tok, expr)]}  {tok[:60]}  (x{n})  {expr[:100]}')
            return 1
        print('width_census --check: no new width assumption in a live file; ok')
        return 0
    for k, v in counts.items():
        print(f'{k}\t{v}')
    if not a.counts:
        write_tsv(a.out, sites, counts)
        print(f'wrote {a.out}: {len(sites)} rows')
    return 0


if __name__ == '__main__':
    sys.exit(main())
