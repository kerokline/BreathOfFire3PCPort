#!/usr/bin/env python
"""Write bof3x.exe, our own executable, and verify what was written.

    python tools/make_exe.py --layout src/hook/exe_layout.h --dll build/bof3x.dll --out build/bof3x.exe

docs/state3-design.md sections 1.2 and 1.3. The image is based at 0x400000
and spans Capcom's address range with sections of its own, so the loader
reserves that range before any heap or thread block can be placed there
(docs/state3-build.md section 1.1, measured):

    .stub   0x401000  int3 page the engine fills (a jmp at each owned start)
    .game   0x5C4000  zero; the engine copies .rdata and .data in
    .pool   0x93F000  DIV-0062's draw items; the file carries the stamp
    .text   0x987000  the entry: Engine_Main(hinstance, command line, show)
    .idata            KERNEL32.dll (four names) and bof3x.dll!Engine_Main
    .rsrc             the BOF3_ICON group (a placeholder of ours) and the manifest

No relocations, no dynamic base, stack reserve / commit from the layout
header. The file holds no game data: the section table is layout, as
symbols.toml's addresses are, and every constant comes from
src/hook/exe_layout.h, the table the engine reads too.

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
  <assemblyIdentity type="win32" name="bof3x" version="1.0.0.0" processorArchitecture="x86"/>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
                        version="6.0.0.0" processorArchitecture="x86"
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

ICON_NAME = 'BOF3_ICON'   # what Game_WinMain's LoadIconA asks for (win_main.cpp kStrIcon)


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


def placeholder_icon():
    """A 32 x 32, 32 bpp icon of ours: an ember-orange disc on transparency.
    A placeholder until the owner's artwork (docs/state3-design.md section 10)."""
    size = 32
    pixels = bytearray()
    mask = bytearray()
    for y in range(size - 1, -1, -1):   # bottom-up
        row_mask = 0
        for x in range(size):
            dx, dy = x - 15.5, y - 15.5
            r = (dx * dx + dy * dy) ** 0.5
            if r <= 14.5:
                t = r / 14.5
                red, green, blue = 0xF0, int(0xC0 - 0x80 * t), int(0x40 - 0x30 * t)
                pixels += bytes((blue, green, red, 0xFF))
            else:
                pixels += b'\0\0\0\0'
                row_mask |= 1 << (31 - x)
        mask += struct.pack('>I', row_mask)
    header = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, len(pixels) + len(mask), 0, 0, 0, 0)
    image = header + bytes(pixels) + bytes(mask)
    group = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHIH', size, size, 0, 0, 1, 32, len(image), 1)
    return image, group


def build_rsrc(rva, icon, group, manifest):
    """A resource tree: RT_ICON 1, RT_GROUP_ICON "BOF3_ICON", RT_MANIFEST 1, language neutral."""
    RT_ICON, RT_GROUP_ICON, RT_MANIFEST = 3, 14, 24
    types = [(RT_ICON, [(1, icon)]), (RT_GROUP_ICON, [(ICON_NAME, group)]), (RT_MANIFEST, [(1, manifest)])]

    def dir_header(named, ids):
        return struct.pack('<IIHHHH', 0, 0, 0, 0, named, ids)

    # Directories first (root, then one per type, then one per name), then
    # the data entries, then the name strings, then the data.
    n_names = sum(len(names) for _, names in types)
    root_size = 16 + 8 * len(types)
    type_dir_size = 16 + 8          # one name each
    lang_dir_size = 16 + 8          # one language each
    off_type_dirs = root_size
    off_lang_dirs = off_type_dirs + type_dir_size * len(types)
    off_data_entries = off_lang_dirs + lang_dir_size * n_names
    off_strings = off_data_entries + 16 * n_names
    name_str = struct.pack('<H', len(ICON_NAME)) + ICON_NAME.encode('utf-16-le')
    off_data = align(off_strings + len(name_str), 8)

    out = bytearray(dir_header(0, len(types)))
    for i, (t, _) in enumerate(types):
        out += struct.pack('<II', t, 0x80000000 | (off_type_dirs + i * type_dir_size))
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


def build_idata(rva):
    """The import directory, lookup tables, IAT and names. Returns (bytes,
    {name: IAT slot RVA}, (IAT rva, IAT size), (directory rva, size))."""
    dlls = [KERNEL32, ENGINE]
    desc_size = 20 * (len(dlls) + 1)
    lookup_sizes = [4 * (len(fns) + 1) for _, fns in dlls]
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
            slots[fn] = iat_rva + 4 * i
            ilt += struct.pack('<I', hint_rva[fn])
        ilt += struct.pack('<I', 0)
    desc += b'\0' * 20
    data = bytes(desc) + bytes(ilt) + bytes(ilt) + bytes(names)
    return data, slots, (rva + off_iat, len(ilt)), (rva, desc_size)


def build_entry(base, slots):
    """The entry: show from STARTUPINFO (wShowWindow if STARTF_USESHOWWINDOW,
    else SW_SHOWDEFAULT), Engine_Main(GetModuleHandleW(NULL), GetCommandLineA(),
    show) - cdecl, which skips the program name as Capcom's _wincmdln did -
    and its answer to ExitProcess (docs/crt-startup.md section 9, the exit row)."""
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


def write(layout, out_path):
    base = layout['ImageBase']
    stub_rva = layout['StubVa'] - base
    game_rva = layout['GameVa'] - base
    pool_rva = layout['PoolVa'] - base
    own_rva = layout['OwnVa'] - base

    # .idata and its slots first: the entry names them.
    text_rva = own_rva
    idata_rva = text_rva + SECTION_ALIGN
    idata, slots, iat_dir, imp_dir = build_idata(idata_rva)
    entry = build_entry(base, slots)
    assert len(entry) < SECTION_ALIGN
    rsrc_rva = idata_rva + align(len(idata), SECTION_ALIGN)
    icon, group = placeholder_icon()
    rsrc = build_rsrc(rsrc_rva, icon, group, MANIFEST)
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
    opt = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII',
                      0x10B, 1, 0,                       # magic, linker version
                      size_code, size_init, size_uninit,
                      text_rva, text_rva, game_rva,      # entry, BaseOfCode, BaseOfData
                      base, SECTION_ALIGN, FILE_ALIGN,
                      4, 0, 0, 0, 4, 0,                  # OS, image, subsystem versions
                      0, size_of_image, HEADERS_SIZE, 0,  # Win32VersionValue, size, headers, checksum
                      2, 0,                              # GUI, DllCharacteristics
                      layout['StackReserve'], layout['StackCommit'], 0x100000, 0x1000,
                      0, 16)
    opt += b''.join(struct.pack('<II', *d) for d in dirs)
    # RELOCS_STRIPPED | EXECUTABLE_IMAGE | LINE_NUMS_STRIPPED | LOCAL_SYMS_STRIPPED | 32BIT_MACHINE
    coff = struct.pack('<HHIIIHH', 0x14C, len(sections), layout['Timestamp'], 0, 0, len(opt), 0x010F)
    dos = bytearray(0x40)
    dos[0:2] = b'MZ'
    struct.pack_into('<I', dos, 0x3C, 0x40)
    headers = bytes(dos) + b'PE\0\0' + coff + opt + bytes(table)
    assert len(headers) <= HEADERS_SIZE
    image = headers + b'\0' * (HEADERS_SIZE - len(headers)) + bytes(body)
    with open(out_path, 'wb') as f:
        f.write(image)
    return slots


def verify(layout, out_path, dll_path):
    """Read the file back with pe_read and check every property the design
    names. Returns a list of failures."""
    pe = Pe(open(out_path, 'rb').read())
    base = layout['ImageBase']
    bad = []

    def check(what, got, want):
        if got != want:
            bad.append(f'{what}: {got!r}, want {want!r}')

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
        check(f'{name} VA', hex(base + s['va']), hex(va))
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
        check('own sections start at OwnVa', hex(base + t['va']), hex(layout['OwnVa']))
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
    check('resources', keys, [(3, '1', 0), (14, ICON_NAME, 0), (24, '1', 0)])
    manifest = res.get((24, 1, 0), b'')
    for needle in (b'<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>',
                   b'<longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>',
                   b'<dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>',
                   b'Microsoft.Windows.Common-Controls'):
        if needle not in manifest:
            bad.append(f'manifest lacks {needle.decode()}')
    if dll_path:
        exports = Pe(open(dll_path, 'rb').read()).exports()
        if 'Engine_Main' not in exports:
            bad.append(f'{dll_path} does not export Engine_Main (exports: {", ".join(exports[:8]) or "none"})')
    return bad, pe


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--layout', required=True, help='src/hook/exe_layout.h')
    ap.add_argument('--out', required=True, help='the executable to write')
    ap.add_argument('--dll', help='bof3x.dll, whose exports are checked for Engine_Main')
    ap.add_argument('--verify-only', action='store_true', help='check an existing file, write nothing')
    a = ap.parse_args()
    layout = read_layout(a.layout)
    if not a.verify_only:
        write(layout, a.out)
    bad, pe = verify(layout, a.out, a.dll)
    if bad:
        if not a.verify_only:
            os.remove(a.out)
        sys.exit('make_exe: ' + a.out + ' failed verification:\n  ' + '\n  '.join(bad))
    print(f'make_exe: {a.out} verified: base {pe.image_base:#x}, {len(pe.sections)} sections '
          f'({", ".join(s["name"] for s in pe.sections)}), entry {pe.image_base + pe.entry_rva:#x}, '
          f'stack {pe.stack_reserve:#x}/{pe.stack_commit:#x}, no relocations, imports '
          + '; '.join(f'{d}!{",".join(f)}' for d, f in pe.imports()))


if __name__ == '__main__':
    main()
