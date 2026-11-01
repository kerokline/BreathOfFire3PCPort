#!/usr/bin/env python
"""Write bof3x.exe, our own executable, and verify what was written.

    python tools/make_exe.py --layout src/hook/exe_layout.h --dll build/bof3x.dll --out build/bof3x.exe
    python tools/make_exe.py ... --image-base 0xA00000 --out build/bof3x-A00000.exe
    python tools/make_exe.py ... --width 64 --out build-x86_64/bof3x.exe

docs/state3-design.md sections 1.2 and 1.3. The image is based at 0x400000
and spans Capcom's address range with sections of its own, so the loader
reserves that range before any heap or thread block can be placed there
(docs/state3-build.md section 1.1, measured):

    .stub   0x401000  int3 page the engine fills (a jmp at each owned start)
    .game   0x5C4000  zero; the engine copies .rdata and .data in
    .pool   0x93F000  DIV-0062's draw items; the file carries the stamp
    .text   0x987000  the entry: Engine_Main(hinstance, command line, show)
    .idata            KERNEL32.dll (four names) and bof3x.dll!Engine_Main
    .rsrc             the BOF3_ICON group, the manifest, and the version block

No relocations, no dynamic base, stack reserve / commit from the layout
header. The file holds no game data: the section table is layout, as
symbols.toml's addresses are, and every constant comes from
src/hook/exe_layout.h, the table the engine reads too.

--image-base writes the same executable at another base
(docs/relocatable-image-design.md 1.5): every section at the same RVA, the
entry's five absolute operands (call [IAT slot]) written for that base, the
rest of the file unchanged. The engine reads the base at run time
(src/common/image_base.h) and moves the image's pointer words to it; the
launcher's developer switch BOF3X_IMAGEBASE starts such a file, named
bof3x-<BASE>.exe beside it (BASE in upper-case hex, no 0x). The base must
keep the image below 16 MB (the ordering table's 24-bit links,
src/game/draw_pool.cpp) and, unless it is 0x400000, the image wholly off the
linked span 0x400000..0x98F000 (image_base.h CodePlace): 0x990000..0xA70000,
checked here and again by the engine.

--width 64 writes the same image as a PE32+ for the 64-bit build
(docs/x64-build-round.md 3 "XA4"): optional header magic 0x20B, machine
0x8664, the 64-bit optional header (no BaseOfData, an 8-byte ImageBase and
8-byte stack and heap sizes), 8-byte import thunks, the manifest's
processorArchitecture amd64, and the entry in x86-64 code - the same three
calls and Engine_Main(hinstance, command line, show) in rcx, rdx, r8d under
the Win64 convention, every call through the IAT RIP-relative, so the
moved image's entry needs no absolute operand at all. The sections, their
RVAs and the data they carry are the 32-bit file's: the image is the same
image (round doc section 1, the narrow image). The 32-bit file is byte for
byte what it was before the flag existed. Two header choices a 64-bit run
has yet to measure (stage C): the subsystem version is 5.2 rather than 4.0
(the x64 minimum link.exe documents; still below 6.0, so the old window
frame metrics hold), and the COFF characteristics carry LARGE_ADDRESS_AWARE
as every x64 linker sets it - without it a 64-bit process is confined to
the low 2 GB, which XA2's low-memory design may yet prefer.

An Authenticode signature (signtool, after the build) appends a certificate
table to the file and names it in data directory 4; --verify-only accepts a
signed file whose table is exactly that trailing blob and reports it signed,
so a signed release can be re-verified against the layout.

After writing, the file is read back with tools/pe_read.py and every
property above is checked, and bof3x.dll's export table is checked for
Engine_Main; any mismatch deletes the output and fails the build.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_read import Pe  # noqa: E402

FILE_ALIGN = 0x200
SECTION_ALIGN = 0x1000
HEADERS_SIZE = 0x400

# Section characteristics.
CNT_CODE, CNT_INIT, CNT_UNINIT = 0x20, 0x40, 0x80
MEM_X, MEM_R, MEM_W = 0x20000000, 0x40000000, 0x80000000

KERNEL32 = ('KERNEL32.dll', ['ExitProcess', 'GetModuleHandleW', 'GetCommandLineA', 'GetStartupInfoA'])
ENGINE = ('bof3x.dll', ['Engine_Main'])

# draw_pool_room.h's kStamp, with its NUL: what DrawPool_Reserve looks for.
POOL_STAMP = b'BOF3X-DRAWPOOL-2048\0'

# The launcher's declarations (src/launcher/launcher.manifest) plus the code
# page and long paths (docs/state3-design.md section 9) and, since the owner's
# call of 2026-10-11, DPI awareness (per-monitor v2 with the v1 fallback):
# BOF3.exe has no manifest, so Windows stretched its window on a scaled
# desktop and blurred the picture after our renderer drew it; aware, the
# window gets real pixels and DIV-0036's default k follows the scale
# (DisplaySetup_WindowedScale), so the window opens the same apparent size.
MANIFEST = b'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="bof3x" version="1.0.0.0" processorArchitecture="ARCH"/>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
                        version="6.0.0.0" processorArchitecture="ARCH"
                        publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
      <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
    </windowsSettings>
  </application>
</assembly>
'''

# The widths' header words: machine, the manifest's processorArchitecture,
# the subsystem version, and the COFF characteristics (RELOCS_STRIPPED |
# EXECUTABLE_IMAGE | LINE_NUMS_STRIPPED | LOCAL_SYMS_STRIPPED, plus
# 32BIT_MACHINE for the PE32 as BOF3.exe has it, LARGE_ADDRESS_AWARE for the
# PE32+ as x64 linkers set it).
WIDTHS = {
    32: dict(machine=0x14C, arch=b'x86', subsystem_version=(4, 0), characteristics=0x010F),
    64: dict(machine=0x8664, arch=b'amd64', subsystem_version=(5, 2), characteristics=0x002F),
}


def manifest(width):
    return MANIFEST.replace(b'ARCH', WIDTHS[width]['arch'])


ICON_NAME = 'BOF3_ICON'   # what Game_WinMain's LoadIconA asks for (win_main.cpp kStrIcon)

# The version block's strings: the same as src/version.rc's, which the DLL and
# the launcher carry - keep the two together. Explorer's Properties shows
# them; antivirus heuristics score an anonymous file against a named one.
RT_VERSION = 16
VERSION_STRINGS = [
    ('CompanyName', 'bof3x project (kerokline)'),
    ('FileDescription', 'bof3x - Breath of Fire III PC'),
    ('InternalName', 'bof3x'),
    ('LegalCopyright', "Copyright (c) 2026 Kevin (kerokline). Breath of Fire III is Capcom's; this is an engine, not the game."),
    ('OriginalFilename', 'bof3x.exe'),
    ('ProductName', 'bof3x - Breath of Fire III PC'),
]


def build_version(numbers, tag):
    """A VS_VERSIONINFO resource: VS_FIXEDFILEINFO with the four numbers, one
    StringFileInfo table (language 0x0409, Unicode) of VERSION_STRINGS plus
    FileVersion and ProductVersion, and the matching VarFileInfo Translation.
    Each node: wLength, wValueLength, wType, szKey (UTF-16, NUL), padding to
    4, the value, padding to 4, the children."""
    def node(key, value, kind, children=b''):
        head = struct.pack('<HHH', 0, len(value) // 2 if kind else len(value), kind) + key.encode('utf-16-le') + b'\0\0'
        head += b'\0' * (align(len(head), 4) - len(head))
        body = value + b'\0' * (align(len(value), 4) - len(value)) if value else b''
        out = bytearray(head + body + children)
        struct.pack_into('<H', out, 0, len(out))
        return bytes(out)

    def wstr(key, text):
        return node(key, text.encode('utf-16-le') + b'\0\0', 1)

    major, minor, patch, build = numbers
    fixed = struct.pack('<IIIIIIIIIIIII', 0xFEEF04BD, 0x10000,
                        (major << 16) | minor, (patch << 16) | build,   # file version
                        (major << 16) | minor, (patch << 16) | build,   # product version
                        0x3F, 0,                                        # flags mask, flags
                        0x40004, 1, 0,                                  # VOS_NT_WINDOWS32, VFT_APP, subtype
                        0, 0)                                           # file date
    strings = [wstr(k, v) for k, v in VERSION_STRINGS]
    strings.append(wstr('FileVersion', '.'.join(str(n) for n in numbers[:3])))
    strings.append(wstr('ProductVersion', tag))
    table = node('040904B0', b'', 1, b''.join(strings))
    sfi = node('StringFileInfo', b'', 1, table)
    var = node('Translation', struct.pack('<HH', 0x409, 1200), 0)
    vfi = node('VarFileInfo', b'', 1, var)
    return node('VS_VERSION_INFO', fixed, 0, sfi + vfi)



def align(v, a):
    return (v + a - 1) // a * a


def read_layout(path):
    """The `kName = 0x...;` constants of exe_layout.h."""
    out = {}
    for m in re.finditer(r'\bk(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;', open(path, encoding='utf-8').read()):
        out[m.group(1)] = int(m.group(2), 16)
    want = ('ImageBase', 'StubVa', 'StubSize', 'GameVa', 'GameSize', 'PoolVa', 'PoolSize', 'OwnVa',
            'StackReserve', 'StackCommit', 'Timestamp')
    missing = [k for k in want if k not in out]
    if missing:
        sys.exit(f'make_exe: {path} lacks {", ".join("k" + k for k in missing)}')
    return out


# Ours: bold white "III" in Palatino on black (tools/make_brand_icon.py, the
# owner's choice of 2026-10-15), committed, so the executable carries its icon
# from the build and is never rewritten on the player's machine.
ICON_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'data', 'icon', 'bof3x.ico')


def brand_icon(path=ICON_FILE):
    """The .ico's images, each an RT_ICON (ids 1..n), and the RT_GROUP_ICON
    naming them: an .ico's directory with each entry's file offset replaced
    by its resource id."""
    with open(path, 'rb') as f:
        data = f.read()
    reserved, kind, count = struct.unpack_from('<HHH', data, 0)
    if reserved != 0 or kind != 1 or count == 0:
        sys.exit(f'make_exe: {path} is not an icon file')
    images, group = [], struct.pack('<HHH', 0, 1, count)
    for i in range(count):
        w, h, colours, res, planes, bits, size, at = struct.unpack_from('<BBBBHHII', data, 6 + 16 * i)
        images.append(data[at:at + size])
        group += struct.pack('<BBBBHHIH', w, h, colours, res, planes, bits, size, i + 1)
    return images, group


def build_rsrc(rva, icons, group, version, manifest):
    """A resource tree: RT_ICON 1..n, RT_GROUP_ICON "BOF3_ICON", RT_VERSION 1,
    RT_MANIFEST 1, language neutral; types in ascending order, as the loader
    expects (FindResource binary-searches the directory)."""
    RT_ICON, RT_GROUP_ICON, RT_MANIFEST = 3, 14, 24
    types = [(RT_ICON, [(i + 1, icon) for i, icon in enumerate(icons)]), (RT_GROUP_ICON, [(ICON_NAME, group)]),
             (RT_VERSION, [(1, version)]), (RT_MANIFEST, [(1, manifest)])]

    def dir_header(named, ids):
        return struct.pack('<IIHHHH', 0, 0, 0, 0, named, ids)

    # Directories first (root, then one per type, then one per name), then
    # the data entries, then the name strings, then the data.
    n_names = sum(len(names) for _, names in types)
    root_size = 16 + 8 * len(types)
    type_dir_sizes = [16 + 8 * len(names) for _, names in types]   # a directory of its names each
    lang_dir_size = 16 + 8          # one language each
    off_type_dirs = root_size
    off_lang_dirs = off_type_dirs + sum(type_dir_sizes)
    off_data_entries = off_lang_dirs + lang_dir_size * n_names
    off_strings = off_data_entries + 16 * n_names
    name_str = struct.pack('<H', len(ICON_NAME)) + ICON_NAME.encode('utf-16-le')
    off_data = align(off_strings + len(name_str), 8)

    out = bytearray(dir_header(0, len(types)))
    for i, (t, _) in enumerate(types):
        out += struct.pack('<II', t, 0x80000000 | (off_type_dirs + sum(type_dir_sizes[:i])))
    k = 0
    for t, names in types:
        named = sum(1 for n, _ in names if isinstance(n, str))
        out += dir_header(named, len(names) - named)
        for n, _ in names:
            ident = (0x80000000 | off_strings) if isinstance(n, str) else n
            out += struct.pack('<II', ident, 0x80000000 | (off_lang_dirs + k * lang_dir_size))
            k += 1
    for j in range(n_names):
        out += dir_header(0, 1)
        out += struct.pack('<II', 0, off_data_entries + 16 * j)   # language 0, neutral
    blobs = [blob for _, names in types for _, blob in names]
    data_at = off_data
    for blob in blobs:
        out += struct.pack('<IIII', rva + data_at, len(blob), 0, 0)
        data_at = align(data_at + len(blob), 8)
    assert len(out) == off_strings
    out += name_str
    out += b'\0' * (off_data - len(out))
    for blob in blobs:
        out += blob
        out += b'\0' * (align(len(out), 8) - len(out))
    return bytes(out)


def build_idata(rva, width=32):
    """The import directory, lookup tables, IAT and names. Returns (bytes,
    {name: IAT slot RVA}, (IAT rva, IAT size), (directory rva, size)). A
    thunk is the width's word (4 or 8 bytes); the descriptors and names are
    the same in both."""
    dlls = [KERNEL32, ENGINE]
    desc_size = 20 * (len(dlls) + 1)
    thunk = width // 8
    thunk_fmt = '<I' if width == 32 else '<Q'
    lookup_sizes = [thunk * (len(fns) + 1) for _, fns in dlls]
    off_ilt = desc_size
    off_iat = off_ilt + sum(lookup_sizes)
    off_names = off_iat + sum(lookup_sizes)
    names = bytearray()
    hint_rva = {}
    for _, fns in dlls:
        for fn in fns:
            hint_rva[fn] = rva + off_names + len(names)
            names += struct.pack('<H', 0) + fn.encode() + b'\0'
            names += b'\0' * (len(names) & 1)
    dll_rva = {}
    for dll, _ in dlls:
        dll_rva[dll] = rva + off_names + len(names)
        names += dll.encode() + b'\0'
        names += b'\0' * (len(names) & 1)
    desc = bytearray()
    ilt = bytearray()
    slots = {}
    for (dll, fns), size in zip(dlls, lookup_sizes):
        ilt_rva = rva + off_ilt + len(ilt)
        iat_rva = rva + off_iat + len(ilt)
        desc += struct.pack('<IIIII', ilt_rva, 0, 0, dll_rva[dll], iat_rva)
        for i, fn in enumerate(fns):
            slots[fn] = iat_rva + thunk * i
            ilt += struct.pack(thunk_fmt, hint_rva[fn])
        ilt += struct.pack(thunk_fmt, 0)
    desc += b'\0' * 20
    data = bytes(desc) + bytes(ilt) + bytes(ilt) + bytes(names)
    return data, slots, (rva + off_iat, len(ilt)), (rva, desc_size)


def build_entry(base, slots, width=32, text_rva=0):
    """The entry: show from STARTUPINFO (wShowWindow if STARTF_USESHOWWINDOW,
    else SW_SHOWDEFAULT), Engine_Main(GetModuleHandleW(NULL), GetCommandLineA(),
    show) - cdecl, which skips the program name as Capcom's _wincmdln did -
    and its answer to ExitProcess (docs/crt-startup.md section 9, the exit row).

    Width 64: the same calls in x86-64 code under the Win64 convention
    (rcx, rdx, r8; 32 bytes of shadow space; rsp 16-aligned at each call),
    every call [IAT slot] RIP-relative, so the code holds no absolute
    address and a moved image's entry is the same bytes. STARTUPINFOA is
    0x68 bytes there, dwFlags at +0x3C and wShowWindow at +0x40; the frame
    is 0x98 bytes: the shadow space at rsp, the STARTUPINFOA at rsp+0x20,
    show at rsp+0x88 and the command line at rsp+0x90, since every call
    may clobber rcx, rdx and r8."""
    if width == 64:
        return build_entry64(slots, text_rva)

    def call(name):
        return b'\xFF\x15' + struct.pack('<I', base + slots[name])   # call [IAT slot]
    code = b''.join([
        b'\x83\xEC\x44',                 # sub esp, 0x44          a STARTUPINFOA
        b'\x54',                         # push esp
        call('GetStartupInfoA'),         # (stdcall: pops its argument)
        b'\xB8\x0A\x00\x00\x00',         # mov eax, 10            SW_SHOWDEFAULT
        b'\xF6\x44\x24\x2C\x01',         # test byte [esp+0x2C], 1   dwFlags & STARTF_USESHOWWINDOW
        b'\x74\x05',                     # jz +5
        b'\x0F\xB7\x44\x24\x30',         # movzx eax, word [esp+0x30] wShowWindow
        b'\x50',                         # push eax               show
        call('GetCommandLineA'),
        b'\x50',                         # push eax               the whole command line
        b'\x6A\x00',                     # push 0
        call('GetModuleHandleW'),
        b'\x50',                         # push eax               hinstance (0x400000)
        call('Engine_Main'),
        b'\x83\xC4\x0C',                 # add esp, 12            cdecl
        b'\x50',                         # push eax               the exit code
        call('ExitProcess'),
        b'\xCC',                         # int3: ExitProcess does not return
    ])
    return code


def build_entry64(slots, text_rva):
    """build_entry's x86-64 form (its docstring). The code is laid out in
    two passes, since a RIP-relative operand is the slot's distance from the
    instruction after the call."""
    parts = [
        b'\x48\x81\xEC\x98\x00\x00\x00',     # sub rsp, 0x98 (imm32: 0x98 as an imm8 would be -0x68)
        b'\x48\x8D\x4C\x24\x20',             # lea rcx, [rsp+0x20]       a STARTUPINFOA
        ('call', 'GetStartupInfoA'),
        b'\xB8\x0A\x00\x00\x00',             # mov eax, 10               SW_SHOWDEFAULT
        b'\xF6\x44\x24\x5C\x01',             # test byte [rsp+0x5C], 1   dwFlags & STARTF_USESHOWWINDOW
        b'\x74\x05',                         # jz +5
        b'\x0F\xB7\x44\x24\x60',             # movzx eax, word [rsp+0x60] wShowWindow
        b'\x89\x84\x24\x88\x00\x00\x00',     # mov [rsp+0x88], eax       show
        ('call', 'GetCommandLineA'),
        b'\x48\x89\x84\x24\x90\x00\x00\x00', # mov [rsp+0x90], rax       the whole command line
        b'\x31\xC9',                         # xor ecx, ecx
        ('call', 'GetModuleHandleW'),
        b'\x48\x89\xC1',                     # mov rcx, rax              hinstance
        b'\x48\x8B\x94\x24\x90\x00\x00\x00', # mov rdx, [rsp+0x90]       command line
        b'\x44\x8B\x84\x24\x88\x00\x00\x00', # mov r8d, [rsp+0x88]       show
        ('call', 'Engine_Main'),
        b'\x89\xC1',                         # mov ecx, eax              the exit code
        ('call', 'ExitProcess'),
        b'\xCC',                             # int3: ExitProcess does not return
    ]
    code = bytearray()
    for part in parts:
        if isinstance(part, bytes):
            code += part
        else:
            # call [rip + disp32]: disp from the end of this 6-byte instruction.
            next_rva = text_rva + len(code) + 6
            code += b'\xFF\x15' + struct.pack('<i', slots[part[1]] - next_rva)
    return bytes(code)


# The 16 MB window (docs/relocatable-image-design.md 1.2): the image's end,
# base + SizeOfImage up to .pool's end, at or below 0x1000000; 64 KB aligned;
# the first allocatable address the floor. And a moved image lies wholly off
# the linked span 0x400000..0x400000 + SizeOfImage, where the engine reads a
# word that may be a VA or a place as a VA (image_base.h CodePlace,
# detour.cpp PatchBytes) and fences what is free (src/hook/host.cpp checks
# the same).
WINDOW_LO, WINDOW_TOP = 0x10000, 0x1000000
# SizeOfImage, as write() lays the sections out: .rsrc's end, page-aligned.
# Known only once the file is written, so the span checked here is up to our
# own sections' start plus the eight pages they take today (0x98F000), and
# verify() checks the written file's SizeOfImage again.
OWN_PAGES = 8


def check_base(layout, base, size_of_image=None):
    """None if `base` is a base the engine accepts, else why not."""
    linked = layout['ImageBase']
    if base % 0x10000:
        return f'{base:#x} is not a multiple of 64 KB'
    end = base + (layout['PoolVa'] + layout['PoolSize'] - linked)
    if base < WINDOW_LO or end > WINDOW_TOP:
        return f'the image at {base:#x} ends at {end:#x}: outside {WINDOW_LO:#x}..{WINDOW_TOP:#x} (24-bit ordering-table links)'
    span = size_of_image or (layout['OwnVa'] - linked + OWN_PAGES * SECTION_ALIGN)
    if base != linked and base < linked + span and base + span > linked:
        return (f'the image at {base:#x}..{base + span:#x} overlaps the linked span {linked:#x}..{linked + span:#x}, '
                'where a word is read as a VA')
    return None


def write(layout, out_path, version, tag, base, width=32):
    w = WIDTHS[width]
    linked = layout['ImageBase']
    stub_rva = layout['StubVa'] - linked
    game_rva = layout['GameVa'] - linked
    pool_rva = layout['PoolVa'] - linked
    own_rva = layout['OwnVa'] - linked

    # .idata and its slots first: the entry names them.
    text_rva = own_rva
    idata_rva = text_rva + SECTION_ALIGN
    idata, slots, iat_dir, imp_dir = build_idata(idata_rva, width)
    entry = build_entry(base, slots, width, text_rva)
    assert len(entry) < SECTION_ALIGN
    rsrc_rva = idata_rva + align(len(idata), SECTION_ALIGN)
    icons, group = brand_icon()
    rsrc = build_rsrc(rsrc_rva, icons, group, build_version(version, tag), manifest(width))
    size_of_image = rsrc_rva + align(len(rsrc), SECTION_ALIGN)

    pool_init = POOL_STAMP
    sections = [
        # name, rva, virtual size, data (None: uninitialised), characteristics
        ('.stub', stub_rva, layout['StubSize'], None, CNT_CODE | CNT_UNINIT | MEM_X | MEM_R | MEM_W),
        ('.game', game_rva, layout['GameSize'], None, CNT_UNINIT | MEM_R | MEM_W),
        ('.pool', pool_rva, layout['PoolSize'], pool_init, CNT_INIT | MEM_R | MEM_W),
        ('.text', text_rva, len(entry), entry, CNT_CODE | MEM_X | MEM_R),
        ('.idata', idata_rva, len(idata), idata, CNT_INIT | MEM_R | MEM_W),
        ('.rsrc', rsrc_rva, len(rsrc), rsrc, CNT_INIT | MEM_R),
    ]
    raw_at = HEADERS_SIZE
    table = bytearray()
    body = bytearray()
    for name, rva, vsize, data, ch in sections:
        rsz = align(len(data), FILE_ALIGN) if data else 0
        ptr = raw_at if data else 0
        table += struct.pack('<8sIIIIIIHHI', name.encode(), vsize, rva, rsz, ptr, 0, 0, 0, 0, ch)
        if data:
            body += data + b'\0' * (rsz - len(data))
            raw_at += rsz

    size_code = sum(align(s[2], FILE_ALIGN) for s in sections if s[4] & CNT_CODE and s[3])
    size_init = sum(align(s[2], FILE_ALIGN) for s in sections if s[4] & CNT_INIT)
    size_uninit = sum(s[2] for s in sections if s[4] & CNT_UNINIT)

    dirs = [(0, 0)] * 16
    dirs[1] = imp_dir
    dirs[2] = (rsrc_rva, len(rsrc))
    dirs[12] = iat_dir

    # BOF3.exe's own header choices where they bear on behaviour: subsystem
    # and OS version 4.0 (below 6.0 Windows keeps the old window-frame
    # metrics BOF3.exe's window was laid out under), no DllCharacteristics
    # (no DEP opt-in, no ASLR, as his image), GUI.
    os_major, os_minor = w['subsystem_version']
    if width == 32:
        opt = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII',
                          0x10B, 1, 0,                       # magic, linker version
                          size_code, size_init, size_uninit,
                          text_rva, text_rva, game_rva,      # entry, BaseOfCode, BaseOfData
                          base, SECTION_ALIGN, FILE_ALIGN,
                          os_major, os_minor, 0, 0, os_major, os_minor,   # OS, image, subsystem versions
                          0, size_of_image, HEADERS_SIZE, 0,  # Win32VersionValue, size, headers, checksum
                          2, 0,                              # GUI, DllCharacteristics
                          layout['StackReserve'], layout['StackCommit'], 0x100000, 0x1000,
                          0, 16)
    else:
        # PE32+: no BaseOfData, ImageBase and the four sizes 8 bytes each.
        opt = struct.pack('<HBBIIIIIQIIHHHHHHIIIIHHQQQQII',
                          0x20B, 1, 0,                       # magic, linker version
                          size_code, size_init, size_uninit,
                          text_rva, text_rva,                # entry, BaseOfCode
                          base, SECTION_ALIGN, FILE_ALIGN,
                          os_major, os_minor, 0, 0, os_major, os_minor,   # OS, image, subsystem versions
                          0, size_of_image, HEADERS_SIZE, 0,  # Win32VersionValue, size, headers, checksum
                          2, 0,                              # GUI, DllCharacteristics
                          layout['StackReserve'], layout['StackCommit'], 0x100000, 0x1000,
                          0, 16)
    opt += b''.join(struct.pack('<II', *d) for d in dirs)
    coff = struct.pack('<HHIIIHH', w['machine'], len(sections), layout['Timestamp'], 0, 0, len(opt),
                       w['characteristics'])
    dos = bytearray(0x40)
    dos[0:2] = b'MZ'
    struct.pack_into('<I', dos, 0x3C, 0x40)
    headers = bytes(dos) + b'PE\0\0' + coff + opt + bytes(table)
    assert len(headers) <= HEADERS_SIZE
    image = headers + b'\0' * (HEADERS_SIZE - len(headers)) + bytes(body)
    with open(out_path, 'wb') as f:
        f.write(image)
    return slots


def verify(layout, out_path, dll_path, version, tag, base, width=32):
    """Read the file back with pe_read and check every property the design
    names, at `base` (the layout's ImageBase, or --image-base's) and for
    `width`. Returns a list of failures."""
    pe = Pe(open(out_path, 'rb').read())
    linked = layout['ImageBase']
    w = WIDTHS[width]
    bad = []
    why = check_base(layout, base, pe.size_of_image)
    if why:
        bad.append(f'ImageBase: {why}')

    def check(what, got, want):
        if got != want:
            bad.append(f'{what}: {got!r}, want {want!r}')

    check('width', pe.width, width)
    check('machine', hex(pe.machine), hex(w['machine']))
    check('characteristics', hex(pe.characteristics), hex(w['characteristics']))
    check('subsystem version', (pe.subsys_major, pe.subsys_minor), w['subsystem_version'])
    check('ImageBase', hex(pe.image_base), hex(base))
    check('TimeDateStamp', hex(pe.timestamp), hex(layout['Timestamp']))
    check('RELOCS_STRIPPED', bool(pe.characteristics & 1), True)
    check('DllCharacteristics DYNAMIC_BASE', bool(pe.dll_characteristics & 0x40), False)
    check('base relocation directory', pe.dirs[5], (0, 0))
    check('stack reserve', hex(pe.stack_reserve), hex(layout['StackReserve']))
    check('stack commit', hex(pe.stack_commit), hex(layout['StackCommit']))
    check('subsystem', pe.subsystem, 2)
    names = [s['name'] for s in pe.sections]
    check('sections', names, ['.stub', '.game', '.pool', '.text', '.idata', '.rsrc'])
    by = {s['name']: s for s in pe.sections}
    for name, va, size in (('.stub', layout['StubVa'], layout['StubSize']),
                           ('.game', layout['GameVa'], layout['GameSize']),
                           ('.pool', layout['PoolVa'], layout['PoolSize'])):
        s = by.get(name)
        if not s:
            continue
        check(f'{name} VA', hex(base + s['va']), hex(base + va - linked))
        check(f'{name} size', hex(s['vsz']), hex(size))
    if '.stub' in by:
        check('.stub has no file bytes', by['.stub']['rsz'], 0)
        check('.stub is RWX code', by['.stub']['ch'] & (MEM_X | MEM_R | MEM_W | CNT_CODE), MEM_X | MEM_R | MEM_W | CNT_CODE)
    if '.game' in by:
        check('.game has no file bytes', by['.game']['rsz'], 0)
        check('.game is RW, not X', by['.game']['ch'] & (MEM_X | MEM_R | MEM_W), MEM_R | MEM_W)
    if '.pool' in by:
        o = by['.pool']['raw']
        check('.pool stamp', pe.data[o:o + len(POOL_STAMP)], POOL_STAMP)
    if '.text' in by:
        t = by['.text']
        check('entry in .text', t['va'] <= pe.entry_rva < t['va'] + t['vsz'], True)
        check('own sections start at OwnVa', hex(base + t['va']), hex(base + layout['OwnVa'] - linked))
        # The entry's five call [IAT slot] operands: absolute at this base in
        # the 32-bit code, RIP-relative (an RVA whatever the base) in the 64-bit.
        code = pe.data[t['raw']:t['raw'] + t['vsz']]
        if width == 32:
            calls = [struct.unpack_from('<I', code, i + 2)[0] for i in range(len(code) - 5) if code[i:i + 2] == b'\xFF\x15']
            lo, hi = base + t['va'], base + pe.size_of_image
        else:
            calls = [t['va'] + i + 6 + struct.unpack_from('<i', code, i + 2)[0]
                     for i in range(len(code) - 5) if code[i:i + 2] == b'\xFF\x15']
            lo, hi = t['va'], pe.size_of_image
        check('entry calls through the IAT at this base', [hex(c) for c in calls if not lo <= c < hi], [])
        check('entry calls', len(calls), 5)
    # Sections in order, each where the last ended (page-aligned), nothing overlapping.
    end = HEADERS_SIZE
    for s in pe.sections:
        if s['va'] < align(end, SECTION_ALIGN):
            bad.append(f"{s['name']} at {s['va']:#x} overlaps what precedes it (ends {end:#x})")
        end = s['va'] + s['vsz']
    check('SizeOfImage', hex(pe.size_of_image), hex(align(end, SECTION_ALIGN)))
    check('imports', pe.imports(), [(d, list(f)) for d, f in (KERNEL32, ENGINE)])
    res = pe.resources()
    keys = sorted(((t, str(n), l) for t, n, l in res))
    check('resources', keys, sorted([(3, str(i + 1), 0) for i in range(len(brand_icon()[0]))] +
                                    [(14, ICON_NAME, 0), (RT_VERSION, '1', 0), (24, '1', 0)]))
    check('version block', res.get((RT_VERSION, 1, 0), b''), build_version(version, tag))
    # Data directory 4, the certificate table (an Authenticode signature,
    # applied after the build): absent, or exactly a trailing WIN_CERTIFICATE
    # (file offset, not RVA; 8-aligned; dwLength the size; revision 2.0; type
    # 2, PKCS#7 signed data). Anything else is a file nobody of ours wrote.
    sec_at, sec_size = pe.dirs[4]
    pe.signed = bool(sec_at or sec_size)
    if pe.signed:
        if sec_at % 8 or sec_at + sec_size != len(pe.data) or sec_size < 8:
            bad.append(f"certificate table at {sec_at:#x}+{sec_size:#x} is not the file's trailing blob (file is {len(pe.data):#x} bytes)")
        else:
            length, revision, kind = struct.unpack_from('<IHH', pe.data, sec_at)
            check('certificate dwLength', length, sec_size)
            check('certificate wRevision', hex(revision), hex(0x200))
            check('certificate wCertificateType', kind, 2)
    found = res.get((24, 1, 0), b'')
    for needle in (b'<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>',
                   b'<longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>',
                   b'<dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>',
                   b'Microsoft.Windows.Common-Controls',
                   b'processorArchitecture="' + w['arch'] + b'"'):
        if needle not in found:
            bad.append(f'manifest lacks {needle.decode()}')
    if dll_path:
        dll = Pe(open(dll_path, 'rb').read())
        check('bof3x.dll width', dll.width, width)
        exports = dll.exports()
        if 'Engine_Main' not in exports:
            bad.append(f'{dll_path} does not export Engine_Main (exports: {", ".join(exports[:8]) or "none"})')
    return bad, pe


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--layout', required=True, help='src/hook/exe_layout.h')
    ap.add_argument('--out', required=True, help='the executable to write')
    ap.add_argument('--dll', help='bof3x.dll, whose exports are checked for Engine_Main')
    ap.add_argument('--version', default='0.0.0', help="the version block's numbers, major.minor.patch (CMake project(VERSION))")
    ap.add_argument('--version-tag', default=None, help='its ProductVersion string, the release tag (default v<version>)')
    ap.add_argument('--verify-only', action='store_true', help='check an existing file, write nothing')
    ap.add_argument('--image-base', type=lambda v: int(v, 0), default=None,
                    help="the executable's base (default the layout's kImageBase, 0x400000): a developer's moved "
                         'image (docs/relocatable-image-design.md 1.5)')
    ap.add_argument('--width', type=int, choices=(32, 64), default=32,
                    help='32: a PE32 for the i686 build (default); 64: a PE32+ for the x86_64 build '
                         '(docs/x64-build-round.md 3 "XA4")')
    a = ap.parse_args()
    layout = read_layout(a.layout)
    m = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)', a.version)
    if not m:
        sys.exit(f'make_exe: --version {a.version!r} is not major.minor.patch')
    version = tuple(int(x) for x in m.groups()) + (0,)
    tag = a.version_tag or 'v' + a.version
    base = layout['ImageBase'] if a.image_base is None else a.image_base
    why = check_base(layout, base)
    if why:
        sys.exit(f'make_exe: --image-base: {why}')
    if not a.verify_only:
        write(layout, a.out, version, tag, base, a.width)
    bad, pe = verify(layout, a.out, a.dll, version, tag, base, a.width)
    if bad:
        if not a.verify_only:
            os.remove(a.out)
        sys.exit('make_exe: ' + a.out + ' failed verification:\n  ' + '\n  '.join(bad))
    print(f'make_exe: {a.out} verified: PE32{"+" if a.width == 64 else ""} (machine {pe.machine:#x}), '
          f'base {pe.image_base:#x}, {len(pe.sections)} sections '
          f'({", ".join(s["name"] for s in pe.sections)}), entry {pe.image_base + pe.entry_rva:#x}, '
          f'stack {pe.stack_reserve:#x}/{pe.stack_commit:#x}, no relocations, imports '
          + '; '.join(f'{d}!{",".join(f)}' for d, f in pe.imports())
          + f', version {".".join(str(n) for n in version[:3])} ({tag}), '
          + ('signed (certificate table present)' if pe.signed else 'unsigned'))


if __name__ == '__main__':
    main()
