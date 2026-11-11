#!/usr/bin/env python
"""The operating-system census: every Windows header, type, call and constant in src/.

    python tools/os_census.py                 # write analysis/os_census.tsv, print the counts
    python tools/os_census.py --out FILE      # write elsewhere
    python tools/os_census.py --counts        # print the counts only, write nothing
    python tools/os_census.py --check         # fail on a live site the committed file does not list
    python tools/os_census.py --list CLASS    # print every site of one class, write nothing
    python tools/os_census.py --files [PREFIX]  # per file: what it takes from the system (src/ paths under PREFIX)
    python tools/os_census.py --outside src/game/platform   # the same for every game file outside one folder

Stage LA1 of the Linux round (docs/linux-round.md section 4). Reads src/ only -
no game data - and writes our own file names, lines and the names of Windows
symbols. It reuses tools/data_census.py's reader (comments and string literals
blanked, so a spelling in a comment is not a site), as tools/width_census.py
does.

**What a site is** (`kind`):

  include   `#include <hdr>` of a Windows header (windows.h, d3d11.h, dinput.h,
            mmsystem.h, ...) or of the C run time's `io.h` / `malloc.h`
  call      a name from the API table (CALLS below) used as a function or
            taken by address. A name the project itself defines, or one reached
            as a member (`.X`, `->X`) or through a namespace (`ns::X`), is not
            a Win32 site. `::X` (the global scope) is.
  const     a Windows constant or flag from the constant table (CONSTS)
  type      a Windows type (TYPES) or a Direct3D / DXGI / DirectInput name
            by prefix
  keyword   `__try`, `__except`, `__finally`, `__leave` (structured exceptions)

The one `windows.h` include row does not name a class of its own: its `class` is
what the *file* takes beyond the header, so one grep answers "what does this
file want from windows.h": `nothing` (no other site in the file: the include is
dead weight), `type_only` (types and constants only), or `api` (at least one
call or keyword).

**The classes** (the `class` column; `group` keeps the API's own group):

  memory    VirtualAlloc / Protect / Query / Free, the Win32 heap, FlushInstructionCache
  file      CreateFile / ReadFile / WriteFile, directory walks, attributes, paths,
            CloseHandle (a handle is a file's far more often than not; the row's
            group says `handle`)
  clock     GetTickCount, QueryPerformanceCounter, Sleep, local time
  thread    threads, events, critical sections, Interlocked*, the TEB, the toolhelp walk
  fiber     CreateFiberEx / SwitchToFiber and kin
  fault     vectored and unhandled exception filters, MiniDumpWriteDump, CONTEXT, __try
  window    the game window, its message loop, the monitor, the cursor, GDI
  dialog    MessageBox, DialogBox, common controls and dialogs, the shell's pickers
  graphics  Direct3D 11 / DXGI / D3DCompile (satisfied by DXVK-native on Linux)
  input     DirectInput, GetAsyncKeyState, keyboard hooks
  sound     the waveOut calls (the launcher's Jukebox only today)
  hash      BCrypt (src/common/sha256.cpp exists)
  process   modules, LoadLibrary, processes, environment variables, the last
            error, code pages, COM start-up, resources
  crt       the Windows C run time's spellings glibc lacks (`io.h`, `_stricmp`,
            `_finddata32_t`, `_wfopen`, `_msize`); not windows.h, but as Windows-only
  type_only a Windows type with no call: the include exists for a spelling
  launcher  every site of a file under src/launcher (the launcher is a Windows
            program for now: linux-round.md section 5); `group` keeps the API group
  harness-only  every site of a diagnostic or test file: a name holding `fuzz`,
            `harness`, `census` or `probe`, and the hook's recipe and watcher
            files (HARNESS below); `group` keeps the API group

Precedence for a site: harness-only (by file), then launcher (by folder), then
the type rule, then the API's group.

The TSV holds `site` rows, then `file` rows (per file: its folder, standing and
what it takes), then `#count` lines. `--check` keys every site by (kind,
class, group, file, token) - not the line number - and fails when a key's count
rises above the committed file's or a key is new (tools/width_census.py's
rule): a new operating-system dependency in a file is refused until the census
is regenerated on purpose, and the reviewer sees the row in the diff.

The census counts spellings, not reaches: a game file that calls a project
helper which calls Win32 is the helper's file's site. A header that includes
windows.h is a site in that header; the files that include the header are not
listed for it.
"""
import argparse, bisect, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data_census as dc  # noqa: E402

ROOT = dc.ROOT
DEFAULT_OUT = 'analysis/os_census.tsv'

CLASSES = ('memory', 'file', 'clock', 'thread', 'fiber', 'fault', 'window', 'dialog', 'graphics', 'input',
           'sound', 'hash', 'process', 'type_only', 'launcher', 'harness-only')

# ---- headers ---------------------------------------------------------------
WIN_HEADER_CLASS = {
    'windows.h': None,           # the umbrella: classed by the file's other sites
    'd3d11.h': 'graphics', 'dxgi.h': 'graphics', 'd3dcompiler.h': 'graphics',
    'dinput.h': 'input',
    'mmsystem.h': 'sound',
    'commctrl.h': 'dialog', 'commdlg.h': 'dialog', 'shlobj.h': 'dialog', 'shellapi.h': 'dialog',
    'bcrypt.h': 'hash',
    'dbghelp.h': 'fault',
    'tlhelp32.h': 'thread',
    'wincodec.h': 'process', 'objbase.h': 'process', 'objidl.h': 'process',
    'io.h': 'crt', 'malloc.h': 'crt',
}
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*<([A-Za-z0-9_./]+)>', re.M)

# ---- the API table ---------------------------------------------------------
def _g(cls, group, names):
    return {n: (cls, group) for n in names.split()}

CALLS = {}
for cls, group, names in (
    ('memory', 'virtual', 'VirtualAlloc VirtualAllocEx VirtualFree VirtualFreeEx VirtualProtect VirtualProtectEx VirtualQuery VirtualQueryEx VirtualLock'),
    ('memory', 'heap', 'HeapAlloc HeapFree HeapCreate HeapDestroy HeapReAlloc GetProcessHeap GlobalAlloc GlobalFree GlobalLock GlobalUnlock LocalAlloc LocalFree'),
    ('memory', 'icache', 'FlushInstructionCache'),
    ('memory', 'foreign', 'ReadProcessMemory WriteProcessMemory'),
    ('memory', 'mapping', 'CreateFileMappingA CreateFileMappingW MapViewOfFile UnmapViewOfFile'),
    ('file', 'open', 'CreateFileA CreateFileW OpenFile'),
    ('file', 'io', 'ReadFile WriteFile SetFilePointer SetFilePointerEx SetEndOfFile FlushFileBuffers GetFileSize GetFileSizeEx GetFileType CopyFileA CopyFileW'),
    ('file', 'handle', 'CloseHandle'),
    ('file', 'walk', 'FindFirstFileA FindFirstFileW FindNextFileA FindNextFileW FindClose'),
    ('file', 'directory', 'CreateDirectoryA CreateDirectoryW RemoveDirectoryA RemoveDirectoryW GetCurrentDirectoryW GetCurrentDirectoryA SetCurrentDirectoryA SetCurrentDirectoryW'),
    ('file', 'name', 'DeleteFileA DeleteFileW MoveFileExA MoveFileExW MoveFileA MoveFileW GetFileAttributesA GetFileAttributesW GetFileAttributesExW GetFileAttributesExA GetTempPathA GetTempPathW GetTempFileNameA GetTempFileNameW GetFullPathNameW GetFullPathNameA SearchPathW CompareFileTime'),
    ('clock', 'tick', 'GetTickCount GetTickCount64 timeGetTime timeBeginPeriod timeEndPeriod'),
    ('clock', 'counter', 'QueryPerformanceCounter QueryPerformanceFrequency'),
    ('clock', 'sleep', 'Sleep SleepEx'),
    ('clock', 'wall', 'GetLocalTime GetSystemTime GetSystemTimeAsFileTime SystemTimeToFileTime FileTimeToSystemTime GetThreadTimes GetProcessTimes'),
    ('thread', 'create', 'CreateThread CreateRemoteThread OpenThread SuspendThread ResumeThread GetThreadContext SetThreadContext GetExitCodeThread ExitThread TerminateThread'),
    ('thread', 'id', 'GetCurrentThread GetCurrentThreadId NtCurrentTeb TlsAlloc TlsGetValue TlsSetValue TlsFree'),
    ('thread', 'event', 'CreateEventA CreateEventW SetEvent ResetEvent WaitForSingleObject WaitForSingleObjectEx WaitForMultipleObjects MsgWaitForMultipleObjects DuplicateHandle CreateMutexA CreateMutexW ReleaseMutex'),
    ('thread', 'critical_section', 'InitializeCriticalSection DeleteCriticalSection EnterCriticalSection LeaveCriticalSection TryEnterCriticalSection'),
    ('thread', 'interlocked', 'InterlockedIncrement InterlockedDecrement InterlockedExchange InterlockedCompareExchange InterlockedExchangeAdd InterlockedOr InterlockedAnd'),
    ('thread', 'toolhelp', 'CreateToolhelp32Snapshot Thread32First Thread32Next'),
    ('fiber', 'fiber', 'CreateFiber CreateFiberEx ConvertThreadToFiber ConvertThreadToFiberEx ConvertFiberToThread SwitchToFiber DeleteFiber GetCurrentFiber IsThreadAFiber GetFiberData'),
    ('fault', 'filter', 'AddVectoredExceptionHandler RemoveVectoredExceptionHandler SetUnhandledExceptionFilter RaiseException MiniDumpWriteDump'),
    ('window', 'class', 'CreateWindowExA CreateWindowExW RegisterClassA RegisterClassW RegisterClassExA RegisterClassExW DefWindowProcA DefWindowProcW DestroyWindow'),
    ('window', 'loop', 'PeekMessageA PeekMessageW GetMessageA GetMessageW TranslateMessage DispatchMessageA DispatchMessageW PostQuitMessage PostMessageA PostMessageW SendMessageA SendMessageW SetTimer KillTimer'),
    ('window', 'geometry', 'GetClientRect GetWindowRect AdjustWindowRect AdjustWindowRectEx SetWindowPos MoveWindow MapWindowPoints GetSystemMetrics MonitorFromPoint MonitorFromWindow GetMonitorInfoA GetMonitorInfoW SystemParametersInfoA SystemParametersInfoW GetDeviceCaps'),
    ('window', 'state', 'ShowWindow UpdateWindow InvalidateRect RedrawWindow SetForegroundWindow GetForegroundWindow GetFocus SetFocus GetActiveWindow GetParent GetWindow FindWindowW FindWindowA FindWindowExW EnableWindow GetClassNameW SetWindowTextW GetWindowTextW GetWindowTextLengthW'),
    ('window', 'longptr', 'GetWindowLongA GetWindowLongW GetWindowLongPtrA GetWindowLongPtrW SetWindowLongA SetWindowLongW SetWindowLongPtrA SetWindowLongPtrW SetClassLongPtrA SetClassLongPtrW SetPropW GetPropW RemovePropW'),
    ('window', 'cursor', 'ShowCursor SetCursor LoadCursorA LoadCursorW LoadIconA LoadIconW LoadImageW CreateIconIndirect DestroyIcon'),
    ('window', 'gdi', 'GetDC ReleaseDC StretchDIBits SetStretchBltMode CreateDIBSection CreateBitmap SelectObject DeleteObject GetStockObject BeginPaint EndPaint SetBkMode SetTextColor SetTextAlign SetBrushOrgEx CreateFontW TextOutW TabbedTextOutW DrawTextW DrawFocusRect GetTextExtentPoint32W GetTextMetricsW GetSysColor GetSysColorBrush'),
    ('dialog', 'message_box', 'MessageBoxA MessageBoxW MessageBeep'),
    ('dialog', 'dialog', 'DialogBoxParamW DialogBoxParamA CreateDialogParamW EndDialog GetDlgItem GetDlgCtrlID GetDlgItemTextW SetDlgItemTextW SetDlgItemTextA SetDlgItemInt SendDlgItemMessageW CheckDlgButton CheckRadioButton IsDlgButtonChecked MapDialogRect'),
    ('dialog', 'common', 'InitCommonControlsEx GetOpenFileNameW GetSaveFileNameW SHBrowseForFolderW SHGetPathFromIDListW SHGetKnownFolderPath SHGetFolderPathW ShellExecuteW ShellExecuteA'),
    ('input', 'key', 'GetAsyncKeyState GetKeyState SetWindowsHookExW SetWindowsHookExA CallNextHookEx UnhookWindowsHookEx DirectInput8Create DirectInputCreateA DirectInputCreateW'),
    ('sound', 'waveout', 'waveOutOpen waveOutClose waveOutWrite waveOutPrepareHeader waveOutUnprepareHeader waveOutReset waveOutGetPosition waveOutPause waveOutRestart waveOutSetVolume'),
    ('graphics', 'd3d', 'D3D11CreateDeviceAndSwapChain D3D11CreateDevice CreateDXGIFactory CreateDXGIFactory1 D3DCompile D3DCompileFromFile'),
    ('hash', 'bcrypt', 'BCryptOpenAlgorithmProvider BCryptCloseAlgorithmProvider BCryptCreateHash BCryptHashData BCryptFinishHash BCryptDestroyHash BCryptGenRandom'),
    ('process', 'module', 'GetModuleHandleA GetModuleHandleW GetModuleHandleExA GetModuleHandleExW GetModuleFileNameA GetModuleFileNameW LoadLibraryA LoadLibraryW LoadLibraryExA LoadLibraryExW FreeLibrary GetProcAddress DisableThreadLibraryCalls'),
    ('process', 'process', 'CreateProcessA CreateProcessW CreatePipe ExitProcess TerminateProcess GetExitCodeProcess GetCurrentProcess GetCurrentProcessId OpenProcess SetHandleInformation WaitForInputIdle'),
    ('process', 'environment', 'GetEnvironmentVariableA GetEnvironmentVariableW SetEnvironmentVariableA SetEnvironmentVariableW GetCommandLineA GetCommandLineW GetStdHandle AttachConsole AllocConsole GetConsoleWindow'),
    ('process', 'error', 'GetLastError SetLastError FormatMessageA FormatMessageW'),
    ('process', 'codepage', 'GetACP MultiByteToWideChar WideCharToMultiByte CompareStringA CompareStringW'),
    ('process', 'com', 'CoInitializeEx CoInitialize CoUninitialize CoCreateInstance CoTaskMemFree CreateStreamOnHGlobal GetHGlobalFromStream IsEqualGUID'),
    ('crt', 'string', '_stricmp _strnicmp _snwprintf _vsnwprintf _wcsicmp _strdup _strlwr _strupr'),
    ('crt', 'file', '_wfopen _fopen _findfirst _findfirst32 _findnext _findnext32 _findclose _fileno _filelength _fseeki64 _ftelli64 _open _read _write _close _lseek _commit _chsize _setmode _access _mkdir _rmdir _unlink _getcwd _chdir _stat _fstat'),
    ('crt', 'memory', '_msize _aligned_malloc _aligned_free _alloca _expand'),
    ('crt', 'float', '_controlfp _fpreset _clearfp _statusfp'),
    ('process', 'resource', 'FindResourceW FindResourceA LoadResource LockResource SizeofResource'),
):
    for n in names.split():
        CALLS[n] = (cls, group)

# ---- constants -------------------------------------------------------------
CONST_RULES = (   # (regex over the whole name, class, group)
    (r'MEM_[A-Z_]+|PAGE_[A-Z_]+', 'memory', 'const'),
    (r'GENERIC_(?:READ|WRITE|ALL|EXECUTE)|FILE_[A-Z_]+|OPEN_EXISTING|OPEN_ALWAYS|CREATE_ALWAYS|CREATE_NEW|TRUNCATE_EXISTING|INVALID_HANDLE_VALUE|INVALID_FILE_ATTRIBUTES|INVALID_SET_FILE_POINTER|MAX_PATH|MOVEFILE_[A-Z_]+', 'file', 'const'),
    (r'INFINITE|WAIT_OBJECT_0|WAIT_TIMEOUT|WAIT_FAILED|THREAD_[A-Z_]+|TH32CS_[A-Z_]+|STILL_ACTIVE|CREATE_SUSPENDED|CREATE_NO_WINDOW', 'thread', 'const'),
    (r'EXCEPTION_[A-Z_]+|CONTEXT_[A-Z_]+|MiniDump[A-Za-z]+|EXCEPTION_CONTINUE_[A-Z]+', 'fault', 'const'),
    (r'WS_[A-Z_]+|WM_[A-Z_]+|SW_[A-Z_]+|SWP_[A-Z_]+|CS_[A-Z_]+|GWL_[A-Z_]+|GWLP_[A-Z_]+|SM_[A-Z_]+|HWND_[A-Z_]+|PM_[A-Z]+|IDC_[A-Z_]+|BLACK_BRUSH|COLORONCOLOR|BI_RGB|BI_BITFIELDS|DIB_RGB_COLORS|SRCCOPY|HALFTONE|BITSPIXEL|LOGPIXELSX|LOGPIXELSY|ANTIALIASED_QUALITY|CLEARTYPE_QUALITY|CLIP_DEFAULT_PRECIS|COLOR_[A-Z_]+|MONITOR_[A-Z_]+|SPI_[A-Z_]+|SIZE_[A-Z]+|VK_[A-Z0-9_]+', 'window', 'const'),
    (r'MB_[A-Z_]+|ID(?:OK|CANCEL|YES|NO|ABORT|RETRY|IGNORE)|BM_[A-Z_]+|BN_[A-Z_]+|BST_[A-Z_]+|CB_[A-Z_]+|CBN_[A-Z_]+|LB_[A-Z_]+|EM_[A-Z_]+|BS_[A-Z_]+|BIF_[A-Z_]+|OFN_[A-Z_]+|ICC_[A-Z_]+|BROADCAST_QUERY_DENY|CSIDL_[A-Z_]+|FOLDERID_[A-Za-z]+|PBM_[A-Z_]+|TBM_[A-Z_]+|LVM_[A-Z_]+|TCM_[A-Z_]+', 'dialog', 'const'),
    (r'DIK_[A-Z0-9_]+|DISCL_[A-Z_]+|DIDFT_[A-Z_]+|DIPROP_[A-Z_]+|DI8DEVCLASS_[A-Z_]+|DIEDFL_[A-Z_]+|DIRECTINPUT_VERSION|GUID_[A-Za-z]+', 'input', 'const'),
    (r'MMSYSERR_[A-Z_]+|WAVE_[A-Z_]+|WHDR_[A-Z_]+|CALLBACK_EVENT|CALLBACK_NULL|CALLBACK_FUNCTION|WOM_[A-Z]+|TIMERR_[A-Z_]+', 'sound', 'const'),
    (r'D3D11_[A-Z0-9_]+|D3D_[A-Z0-9_]+|DXGI_[A-Z0-9_]+|D3DCOMPILE_[A-Z0-9_]+', 'graphics', 'const'),
    (r'BCRYPT_[A-Z0-9_]+', 'hash', 'const'),
    (r'CP_ACP|CP_UTF8|CSTR_[A-Z_]+|ATTACH_PARENT_PROCESS|STD_(?:INPUT|OUTPUT|ERROR)_HANDLE|HANDLE_FLAG_[A-Z_]+|STARTF_[A-Z_]+|PROCESS_[A-Z_]+|LOAD_LIBRARY_[A-Z_]+|DLL_PROCESS_[A-Z]+|DLL_THREAD_[A-Z]+|CLSCTX_[A-Z_]+|COINIT_[A-Z_]+|ERROR_[A-Z0-9_]+|S_OK|S_FALSE|E_[A-Z_]+|FAILED|SUCCEEDED|HRESULT_FROM_WIN32|RT_[A-Z]+|MAKEINTRESOURCE[AW]?|CLSID_[A-Za-z]+|IID_[A-Za-z0-9]+|GUID_WICPixelFormat[A-Za-z0-9]+|WIC[A-Za-z]+', 'process', 'const'),
)
CONST_RULES = tuple((re.compile(r'(?<![\w.])(?:' + p + r')\b'), c, g) for p, c, g in CONST_RULES)

# ---- types -----------------------------------------------------------------
TYPE_NAMES = ('DWORD WORD BYTE BOOL BOOLEAN UINT ULONG LONG USHORT SHORT UCHAR LPVOID LPCVOID PVOID LPBYTE LPDWORD PDWORD LPCSTR LPSTR LPCWSTR LPWSTR PCSTR PCWSTR '
              'LPCTSTR LPTSTR SIZE_T SSIZE_T ULONG_PTR DWORD_PTR LONG_PTR UINT_PTR INT_PTR LPARAM WPARAM LRESULT HRESULT ULONGLONG LONGLONG LARGE_INTEGER ULARGE_INTEGER '
              'HANDLE PHANDLE HWND HMODULE HINSTANCE HMENU HICON HCURSOR HBRUSH HBITMAP HDC HFONT HGDIOBJ HMONITOR HGLOBAL HKEY HRSRC HHOOK HPEN HRGN WCHAR TCHAR '
              'RECT POINT SIZE MSG WNDCLASSA WNDCLASSW WNDCLASSEXA WNDCLASSEXW PAINTSTRUCT MONITORINFO MONITORINFOEXA MONITORINFOEXW BITMAPINFO BITMAPINFOHEADER BITMAPV5HEADER RGBQUAD '
              'LOGFONTW TEXTMETRICW OPENFILENAMEW BROWSEINFOW ITEMIDLIST LPITEMIDLIST INITCOMMONCONTROLSEX '
              'SYSTEMTIME FILETIME WIN32_FIND_DATAA WIN32_FIND_DATAW WIN32_FILE_ATTRIBUTE_DATA OVERLAPPED SECURITY_ATTRIBUTES STARTUPINFOA STARTUPINFOW PROCESS_INFORMATION '
              'MEMORY_BASIC_INFORMATION SYSTEM_INFO CRITICAL_SECTION LPCRITICAL_SECTION THREADENTRY32 CONTEXT PCONTEXT EXCEPTION_POINTERS EXCEPTION_RECORD PEXCEPTION_POINTERS '
              'PVECTORED_EXCEPTION_HANDLER LPTOP_LEVEL_EXCEPTION_FILTER MINIDUMP_EXCEPTION_INFORMATION MINIDUMP_TYPE NT_TIB TEB '
              'WAVEHDR WAVEFORMATEX HWAVEOUT LPWAVEHDR MMRESULT '
              'GUID REFIID REFCLSID IUnknown IStream IWICImagingFactory IWICBitmapDecoder IWICBitmapFrameDecode IWICFormatConverter '
              'BCRYPT_ALG_HANDLE BCRYPT_HASH_HANDLE NTSTATUS PROC FARPROC WNDPROC DLGPROC TIMERPROC HOOKPROC').split()
TYPES = re.compile(r'(?<![\w.])(' + '|'.join(sorted(TYPE_NAMES, key=len, reverse=True)) + r')\b')
CRT_TYPES = re.compile(r'(?<![\w.])(_finddata32_t|_finddata_t|_finddatai64_t|_stat32|_stat64|_stat|_off_t|_timeb)\b')
TYPE_PREFIX = (  # (regex, class): D3D / DXGI / DirectInput names by prefix
    (re.compile(r'(?<![\w.])(ID3D1[01]\w+|ID3DBlob|D3D1[01]_[A-Z0-9_]+|D3D_[A-Z0-9_]+|IDXGI\w+|DXGI_[A-Z0-9_]+)\b'), 'graphics'),
    (re.compile(r'(?<![\w.])(IDirectInput\w*|LPDIRECTINPUT\w*|DIJOYSTATE\w*|DIDEVICEOBJECTDATA|DIDEVICEINSTANCE\w*|DIDEVCAPS|DIPROP\w*|DIOBJECTDATAFORMAT|DIDATAFORMAT|DIK_[A-Z0-9_]+|LPDIDEVICEINSTANCE\w*)\b'), 'input'),
)
KEYWORD = re.compile(r'(?<![\w.])(__try|__except|__finally|__leave)\b')
CALLCONV = re.compile(r'(?<![\w.])(__stdcall|__cdecl|__fastcall|__thiscall|WINAPI|CALLBACK|APIENTRY|STDMETHODCALLTYPE|__declspec)\b')

HARNESS_NAME = re.compile(r'fuzz|harness|census|probe')
HARNESS_FILES = {  # the hook's diagnostic and recipe files (each opens with "diagnostic only" or a BOF3X_ switch)
    'src/hook/calltrace.cpp', 'src/hook/gapwatch.cpp', 'src/hook/write_watch.cpp', 'src/hook/stallwatch.cpp',
    'src/hook/stp_census.cpp', 'src/hook/statehash.cpp', 'src/hook/wav_dump.cpp', 'src/hook/input_script.cpp',
    'src/hook/draw_order.cpp',
}


def is_harness(rel):
    base = os.path.basename(rel)
    return rel in HARNESS_FILES or bool(HARNESS_NAME.search(base)) or '/harness' in rel


def project_defined(files):
    """Names the project defines as functions or types: a Win32 name in this
    set is the project's own (a wrapper of the same spelling)."""
    out = set()
    for f in files:
        for m in re.finditer(r'\b(\w+)\s*\([^;(){}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{', f.code):
            out.add(m.group(1))
        for m in re.finditer(r'\b(?:struct|class|enum|using|typedef|namespace)\s+(?:class\s+)?(\w+)', f.code):
            out.add(m.group(1))
        for m in re.finditer(r'#\s*define\s+(\w+)', f.code):
            out.add(m.group(1))
    return out


class Site:
    __slots__ = ('file', 'line', 'kind', 'cls', 'group', 'token')

    def __init__(self, file, line, kind, cls, group, token):
        self.file, self.line, self.kind, self.cls, self.group, self.token = file, line, kind, cls, group, token


def folder_of(rel):
    parts = rel.split('/')
    if parts[1] == 'game' and len(parts) > 3:
        return 'game/' + parts[2]
    return parts[1] if parts[1] != 'game' else 'game(root)'


def scan(files):
    own = project_defined(files)
    sites = []
    for f in files:
        rel = f.rel
        starts = f.starts
        line_of = lambda pos: bisect.bisect_right(starts, pos)
        override = 'harness-only' if is_harness(rel) else ('launcher' if rel.startswith('src/launcher/') else None)
        found = []   # (pos, kind, cls, group, token)
        for m in INCLUDE.finditer(f.code):
            hdr = m.group(1)
            if hdr in WIN_HEADER_CLASS:
                found.append((m.start(), 'include', WIN_HEADER_CLASS[hdr], 'header', hdr))
        for m in re.finditer(r'(?<![\w.>:])(' + '|'.join(sorted(CALLS, key=len, reverse=True)) + r')\b', f.code):
            n = m.group(1)
            if n in own:
                continue
            found.append((m.start(), 'call', CALLS[n][0], CALLS[n][1], n))
        # `::X` (global scope) was excluded above by the lookbehind on ':'; add it back
        for m in re.finditer(r'(?<![\w:])::(' + '|'.join(sorted(CALLS, key=len, reverse=True)) + r')\b', f.code):
            n = m.group(1)
            if n in own:
                continue
            found.append((m.start(), 'call', CALLS[n][0], CALLS[n][1], n))
        for rx, cls, group in CONST_RULES:
            for m in rx.finditer(f.code):
                if m.group(0) in own:
                    continue
                found.append((m.start(), 'const', cls, group, m.group(0)))
        for m in TYPES.finditer(f.code):
            n = m.group(1)
            if n in own:
                continue
            found.append((m.start(), 'type', 'type_only', 'type', n))
        for rx, cls in TYPE_PREFIX:
            for m in rx.finditer(f.code):
                if m.group(1) in own:
                    continue
                found.append((m.start(), 'type', cls, 'type', m.group(1)))
        for m in CRT_TYPES.finditer(f.code):
            if m.group(1) not in own:
                found.append((m.start(), 'type', 'crt', 'type', m.group(1)))
        for m in KEYWORD.finditer(f.code):
            found.append((m.start(), 'keyword', 'fault', 'seh', m.group(1)))
        # drop a constant or type already taken as a more specific token at the same place
        seen = set()
        for pos, kind, cls, group, tok in sorted(found):
            if (pos, tok) in seen:
                continue
            seen.add((pos, tok))
            if kind != 'include' and override:
                sites.append(Site(rel, line_of(pos), kind, override, cls + '/' + group, tok))
            else:
                sites.append(Site(rel, line_of(pos), kind, cls, group, tok))
    # the windows.h rows are verdicts on the file's other sites
    by_file = collections.defaultdict(list)
    for s in sites:
        by_file[s.file].append(s)
    for rel, ss in by_file.items():
        others = [s for s in ss if s.kind != 'include']
        api = any(s.kind in ('call', 'keyword') for s in others)
        for s in ss:
            if s.kind == 'include' and s.cls is None:
                s.cls = 'umbrella'
                s.group = 'api' if api else ('type_only' if others else 'nothing')
    sites.sort(key=lambda s: (s.file, s.line, s.kind, s.token))
    return sites


def file_rows(files, sites):
    by = collections.defaultdict(list)
    for s in sites:
        by[s.file].append(s)
    rows = []
    for rel in sorted(by):
        ss = by[rel]
        inc = [s for s in ss if s.kind == 'include' and s.token == 'windows.h']
        others = [s for s in ss if s.kind != 'include']
        # the real classes under an override
        real = sorted({(s.group.split('/')[0] if s.cls in ('launcher', 'harness-only') and '/' in s.group else s.cls) for s in others})
        standing = 'harness-only' if is_harness(rel) else ('launcher' if rel.startswith('src/launcher/') else 'live')
        rows.append((rel, folder_of(rel), standing, inc[0].group if inc else '', ','.join(real) if real else 'nothing', len(ss)))
    return rows


def write_tsv(path, sites, frows):
    with open(path, 'w', encoding='utf-8', newline='\n') as o:
        o.write('#os_census: every Windows header, type, call and constant in src/ (tools/os_census.py; docs/linux-round.md 4, LA1)\n')
        o.write('type\tfile\tline\tkind\tclass\tgroup\ttoken\n')
        for s in sites:
            o.write(f'site\t{s.file}\t{s.line}\t{s.kind}\t{s.cls}\t{s.group}\t{s.token}\n')
        o.write('type\tfile\tfolder\tstanding\twindows_h\ttakes\tsites\n')
        for r in frows:
            o.write('file\t' + '\t'.join(str(x) for x in r) + '\n')
        for k, v in counts(sites, frows):
            o.write(f'#count\t{k}\t{v}\n')


def counts(sites, frows):
    out = []
    out.append(('sites', len(sites)))
    out.append(('files_with_a_site', len(frows)))
    by = collections.Counter(s.cls for s in sites)
    for c in sorted(by):
        out.append(('class:' + c, by[c]))
    kinds = collections.Counter(s.kind for s in sites)
    for k in sorted(kinds):
        out.append(('kind:' + k, kinds[k]))
    # the sites of live (non-launcher, non-harness) files by API class: what stage LB has to answer
    live = collections.Counter(s.cls for s in sites if s.cls not in ('launcher', 'harness-only'))
    out.append(('live_sites', sum(live.values())))
    # calls by the API's own group through the override: the underlying class
    under = collections.Counter()
    for s in sites:
        if s.kind in ('call', 'keyword'):
            under[s.group.split('/')[0] if s.cls in ('launcher', 'harness-only') and '/' in s.group else s.cls] += 1
    for c in sorted(under):
        out.append(('call_or_keyword_class:' + c, under[c]))
    folders = collections.Counter(folder_of(r[0]) for r in frows)
    for k in sorted(folders):
        out.append(('files_in:' + k, folders[k]))
    return out


def key_of(s):
    return (s[1], s[3], s[4], s[5], s[0], s[6])


def read_sites(path):
    with open(path, encoding='utf-8') as f:
        rows = [ln.rstrip('\n').split('\t') for ln in f if ln.startswith('site\t')]
    return rows


def check(path, sites):
    """Keys: (kind, class, group, file, token). Fails on a new key or a higher count."""
    committed = collections.Counter((r[3], r[4], r[5], r[1], r[6]) for r in read_sites(path))
    now = collections.Counter((s.kind, s.cls, s.group, s.file, s.token) for s in sites)
    bad = []
    for k, n in sorted(now.items()):
        if n > committed.get(k, 0):
            bad.append((k, committed.get(k, 0), n))
    return bad


# ---- the boundary gate -------------------------------------------------------
# Stage LB1 (docs/linux-round.md): the engine reaches the system through src/os.
# A live file outside src/os and src/launcher (harness-only files are marked by
# the census itself) that includes windows.h, or uses a Win32 name that bof3::os
# replaces, fails the gate - unless it waits for a later unit: BOUNDARY_WAITING
# names that unit. The list may only shrink: an entry whose file no longer
# breaks the rule fails too, so it is removed with the work that emptied it.
REPLACED_CLASSES = ('memory', 'file', 'clock', 'hash')
REPLACED_CALLS = {'GetEnvironmentVariableA', 'GetEnvironmentVariableW', 'GetLastError', 'GetCurrentThreadId',
                  'GetCurrentProcessId', 'GetModuleFileNameA', 'GetModuleFileNameW', 'GetModuleHandleExW',
                  'MessageBoxA', 'MessageBoxW'}
BOUNDARY_WAITING = {
    'src/hook/exe_host.cpp': 'W: the PE exe host loader, Windows only by design',
    'src/hook/exe_icon.cpp': 'W: the PE exe host, Windows only by design',
    'src/hook/dllmain.cpp': 'W: the DLL entry, Windows only by design',
    'src/hook/host.cpp': 'W: reads the PE headers of the host process',
    'src/hook/detour.cpp': 'W: reads the PE headers of the host process',
    'src/hook/exe_image.cpp': 'W: the PE image of BOF3.exe',
    'src/hook/crash.cpp': 'L3: crash catching (SEH, MiniDumpWriteDump)',
    'src/hook/stackwatch.cpp': 'L3: crash catching (the stack walk)',
    'src/hook/cpu_context.h': 'L3: crash catching (CONTEXT)',
    'src/hook/snapshot.cpp': 'L3: fibers (ConvertThreadToFiber)',
    'src/hook/log.cpp': 'L3: the log writer thread flushes from the crash path',
    'src/audio/sound_sdl.cpp': 'L3: the DirectSound notify events',
    'src/game/text/msgbox.cpp': 'L2: key codes (VK_*)',
    'src/game/menu/quick_menu.cpp': 'L2: key codes (VK_*)',
    'src/game/platform/win_main.cpp': 'L2: the window and its message loop',
    'src/game/platform/display_setup.cpp': 'L2: the window and the monitor',
    'src/game/platform/d3d.cpp': 'L2: the window (HWND)',
    'src/game/platform/magic_080.cpp': 'L2: the window and its message loop',
}


def boundary_violations(sites):
    bad = collections.defaultdict(list)
    for s in sites:
        if s.file.startswith(('src/os/', 'src/launcher/')) or s.cls in ('launcher', 'harness-only') or is_harness(s.file):
            continue
        if s.kind == 'include' and s.token == 'windows.h':
            bad[s.file].append((s.line, 'include <windows.h>'))
        elif s.kind == 'call' and (s.cls in REPLACED_CLASSES or s.token in REPLACED_CALLS):
            bad[s.file].append((s.line, s.token))
    return bad


def boundary(sites):
    bad = boundary_violations(sites)
    fail = 0
    for f in sorted(bad):
        if f in BOUNDARY_WAITING:
            continue
        fail += 1
        for line, what in bad[f]:
            print(f'{f}:{line}: {what}: use bof3::os (src/os/os.h), or list the file in BOUNDARY_WAITING with its unit')
    for f in sorted(BOUNDARY_WAITING):
        if f not in bad:
            fail += 1
            print(f'{f}: listed in BOUNDARY_WAITING but no longer breaks the rule: remove the entry')
    if fail:
        return 1
    print(f'os_census --boundary: ok ({len(bad)} files wait for {len({v.split(':')[0] for v in BOUNDARY_WAITING.values()})} later units)')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--counts', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--list', metavar='CLASS')
    ap.add_argument('--files', nargs='?', const='src/', metavar='PREFIX')
    ap.add_argument('--boundary', action='store_true', help='fail when a file outside src/os, src/launcher and the harness-only files reaches the system past the seam')
    ap.add_argument('--outside', metavar='PREFIX', help='game files outside PREFIX that include windows.h, and what they take')
    a = ap.parse_args()

    files = dc.load_sources()
    sites = scan(files)
    frows = file_rows(files, sites)

    if a.boundary:
        return boundary(sites)
    if a.list:
        for s in sites:
            if s.cls == a.list:
                print(f'{s.file}:{s.line}\t{s.kind}\t{s.group}\t{s.token}')
        return 0
    if a.files is not None:
        for r in frows:
            if r[0].startswith(a.files):
                print('\t'.join(str(x) for x in r))
        return 0
    if a.outside:
        sel = [r for r in frows if r[0].startswith('src/game/') and not r[0].startswith(a.outside.rstrip('/') + '/') and r[3] != '']
        tally = collections.Counter(r[4] for r in sel)
        print(f'{len(sel)} game files outside {a.outside} include windows.h')
        for k, v in sorted(tally.items(), key=lambda kv: -kv[1]):
            print(f'  {v:3d}  takes {k}')
        live = [r for r in sel if r[2] == 'live']
        print(f'  of them {len(live)} live, {len(sel) - len(live)} harness-only')
        tl = collections.Counter(r[4] for r in live)
        for k, v in sorted(tl.items(), key=lambda kv: -kv[1]):
            print(f'    live {v:3d}  takes {k}')
        return 0
    if a.counts:
        for k, v in counts(sites, frows):
            print(f'{k}\t{v}')
        return 0
    if a.check:
        path = os.path.join(ROOT, DEFAULT_OUT) if a.out == DEFAULT_OUT else a.out
        bad = check(path, sites)
        if bad:
            print('os_census: sites the committed file does not list (regenerate on purpose):')
            for k, was, now in bad:
                print(f'  {k[3]}  {k[0]} {k[1]}/{k[2]} {k[4]}  {was} -> {now}')
            return 1
        print(f'os_census: ok ({len(sites)} sites)')
        return 0
    out = os.path.join(ROOT, a.out) if not os.path.isabs(a.out) else a.out
    write_tsv(out, sites, frows)
    for k, v in counts(sites, frows):
        print(f'{k}\t{v}')
    print('wrote', os.path.relpath(out, ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
