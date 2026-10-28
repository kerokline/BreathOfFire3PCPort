"""Where the game's image lies in a running process, for the live readers.

The tools speak VAs, symbols.toml's frame (docs/relocatable-image-design.md
9). The game's image lies at 0x400000 - BOF3.exe always, and the player's
bof3x.exe - but a developer's bof3x.exe may be written at another base
(tools/make_exe.py --image-base, the launcher's BOF3X_IMAGEBASE). A reader
that takes a VA adds the move, the process's main module base less 0x400000,
to an address in the linked span 0x400000..0x98F000; any other address (a
heap block, a stack, our DLL) is read as it is. At a moved base nothing of
the process lies in the linked span (the launcher reserves it, src/hook/
exe_host.cpp Fence), so the two cannot be confused. tools/task_stacks.py,
tools/attract_watch.py and tools/mem_dump.py read through this.
"""
import ctypes
from ctypes import wintypes as w

LINKED_BASE, LINKED_END = 0x400000, 0x98F000

_psapi = ctypes.WinDLL('psapi', use_last_error=True)
_psapi.EnumProcessModulesEx.argtypes = [w.HANDLE, ctypes.POINTER(w.HMODULE), w.DWORD, ctypes.POINTER(w.DWORD), w.DWORD]
_psapi.EnumProcessModulesEx.restype = w.BOOL


def main_module_base(handle):
    """The process's main module base (its first module), or None. The handle
    needs PROCESS_QUERY_INFORMATION | PROCESS_VM_READ (0x0410)."""
    mods = (w.HMODULE * 1)()
    need = w.DWORD()
    if not _psapi.EnumProcessModulesEx(handle, mods, ctypes.sizeof(mods), ctypes.byref(need), 0x03):   # LIST_MODULES_ALL
        return None
    return mods[0] or None


def move_of(handle):
    """The image's move: its base less 0x400000; 0 when the base is unknown."""
    base = main_module_base(handle)
    return (base - LINKED_BASE) if base else 0


def place(addr, move):
    """A VA in the linked span as its place in the process; any other address as it is."""
    return addr + move if LINKED_BASE <= addr < LINKED_END else addr


def va(addr, move):
    """A place in the moved image as its VA; any other address as it is."""
    return addr - move if move and LINKED_BASE + move <= addr < LINKED_END + move else addr
