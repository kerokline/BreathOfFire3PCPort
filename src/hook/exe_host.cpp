// The exe host's start-up: Engine_Main, called by bof3x.exe's entry on the
// process's main thread once the loader is done (docs/state3-design.md
// section 4, in its order). DllMain has only opened the log and told the
// hosts apart (dllmain.cpp).
//
//  1. host detection (DllMain) - and the switches that need Capcom's code
//     refused: BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_MP3=original,
//     BOF3X_CALLTRACE
//  2. the layout: our sections where exe_layout.h puts them
//  3. the data: the player's BOF3.exe by SHA-256 when it is there (optional
//     with BOF3X_EXEIMAGE set: a disc-built install has none); .rdata the
//     engine's own (rdata::Map: the 44 constants and the keyboard format;
//     the slots in 4), nothing of it from the file; .data from
//     BOF3X_EXEIMAGE's cache when set, else the file's; the default key
//     table written from the engine's rule; the .bss asserted zero
//  4. the fourteen import slots our code calls through, bound and read back
//  5. the x87 control word 0x027F, read back; MXCSR logged
//  6. .stub filled with int3; the state hash's census; InjectAll writes the
//     jmps (and DrawPool_Reserve takes .pool); DirectSoundCreate's thunk;
//     the census after; .stub checked to hold nothing but those; then
//     read-execute
//  7. the unhandled-exception filter; Game_WinMain on this thread (whose
//     loop generates the icon, DIV-0095, exe_icon.h)
#include "hook/exe_host.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "common/sha256.h"
#include "game/rdata_consts.h"
#include "game/shell.h"
#include "hook/calltrace.h"
#include "hook/crash.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/exe_icon.h"
#include "hook/exe_image.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/inject_all.h"
#include "hook/input_script.h"
#include "hook/log.h"
#include "hook/stackwatch.h"
#include "hook/statehash.h"
#include "hook/wav_dump.h"

namespace bof3 {
namespace {

using namespace exe_layout;

void* At(std::uint32_t va) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(va)); }

bool EnvSet(const char* name) { return GetEnvironmentVariableA(name, nullptr, 0) > 0; }

// --- 1. The switches that need Capcom's code ---------------------------------

void RefuseHostedSwitches() {
    if (EnvSet("BOF3X_ORIGINAL"))
        Fatal("BOF3X_ORIGINAL is set: the A/B switch runs Capcom's code, which bof3x.exe does not map. "
              "Start the hosted build (bof3x-launcher --host bof3) for it.");
    if (EnvSet("BOF3X_SHADOW"))
        Fatal("BOF3X_SHADOW is set: the shadow fuzz clones Capcom's code, which bof3x.exe does not map. "
              "Start the hosted build (bof3x-launcher --host bof3) for it.");
    char mp3[16] = {};
    if (GetEnvironmentVariableA("BOF3X_MP3", mp3, sizeof mp3) && std::strcmp(mp3, "original") == 0)
        Fatal("BOF3X_MP3=original: Capcom's MP3 decoder is not mapped in bof3x.exe (DIV-0091). "
              "Start the hosted build (bof3x-launcher --host bof3) for it.");
    if (EnvSet("BOF3X_CALLTRACE"))
        Fatal("BOF3X_CALLTRACE is set: the call trace counts calls into Capcom's code, and in bof3x.exe "
              "every entry it would arm is int3 or a jmp of ours. Start the hosted build (bof3x-launcher --host bof3).");
}

// --- 2. The layout ---------------------------------------------------------

void CheckSection(const char* name, std::uint32_t va, std::uint32_t size, bool exec) {
    for (std::uint32_t a = va; a < va + size;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(At(a), &m, sizeof m)) Fatal("layout: VirtualQuery(0x%08X) failed, error %lu", (unsigned)a, GetLastError());
        const DWORD p = m.Protect & 0xFF;
        const bool writable = p == PAGE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READWRITE ||
                              p == PAGE_EXECUTE_WRITECOPY;
        const bool executable = p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
        if (m.State != MEM_COMMIT || m.Type != MEM_IMAGE || m.AllocationBase != At(kImageBase) || !writable ||
            executable != exec)
            Fatal("layout: %s at 0x%08X is not our image's %s section (state 0x%lX type 0x%lX protect 0x%lX base %p): "
                  "a rebased or rewritten bof3x.exe",
                  name, (unsigned)a, exec ? "read-write-execute" : "read-write", m.State, m.Type, m.Protect, m.AllocationBase);
        a = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m.BaseAddress)) + static_cast<std::uint32_t>(m.RegionSize);
    }
}

void CheckLayout() {
    CheckSection(".stub", kStubVa, kStubSize, true);
    CheckSection(".game", kGameVa, kGameSize, false);
    CheckSection(".pool", kPoolVa, kPoolSize, false);
    Log("layout: .stub 0x%08X +0x%X RWX, .game 0x%08X +0x%X RW, .pool 0x%08X +0x%X RW, as exe_layout.h",
        (unsigned)kStubVa, (unsigned)kStubSize, (unsigned)kGameVa, (unsigned)kGameSize, (unsigned)kPoolVa,
        (unsigned)kPoolSize);
}

// --- 3. The data -----------------------------------------------------------

struct FileSection {
    std::uint32_t va = 0, vsize = 0, raw = 0, raw_size = 0;
};

HANDLE g_exe = INVALID_HANDLE_VALUE;   // the player's BOF3.exe, read-only, for the process's life; none: no file
FileSection g_text, g_rdata, g_data;   // its sections, when it is there

void ReadAt(std::uint32_t offset, void* to, std::uint32_t n, const char* what) {
    LARGE_INTEGER at;
    at.QuadPart = offset;
    DWORD got = 0;
    if (!SetFilePointerEx(g_exe, at, nullptr, FILE_BEGIN) || !ReadFile(g_exe, to, n, &got, nullptr) || got != n)
        Fatal("data: cannot read %s (0x%X bytes at file offset 0x%X) from BOF3.exe, error %lu", what, (unsigned)n,
              (unsigned)offset, GetLastError());
}

FileSection Section(const char* want) {
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    ReadAt(0, &dos, sizeof dos, "the DOS header");
    ReadAt(static_cast<std::uint32_t>(dos.e_lfanew), &nt, sizeof nt, "the PE header");
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.ImageBase != image::kBase)
        Fatal("data: BOF3.exe is not a 32-bit PE based at 0x%X", (unsigned)image::kBase);
    const std::uint32_t first = static_cast<std::uint32_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) +
                                nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER s;
        ReadAt(first + i * sizeof s, &s, sizeof s, "the section table");
        char name[9] = {};
        std::memcpy(name, s.Name, 8);
        if (std::strcmp(name, want) != 0) continue;
        return {image::kBase + s.VirtualAddress, s.Misc.VirtualSize, s.PointerToRawData, s.SizeOfRawData};
    }
    Fatal("data: BOF3.exe has no %s section", want);
}

// The player's BOF3.exe: hashed, opened and its sections checked when the
// game directory holds it. Without it, BOF3X_EXEIMAGE must name the .data
// source (a disc-built install, docs/disc-only-round.md): nothing is read
// from a file, and every verification against it says so.
void OpenExe() {
    wchar_t dir[MAX_PATH];
    const DWORD n = GetCurrentDirectoryW(MAX_PATH - 10, dir);
    if (n == 0 || n >= MAX_PATH - 10) Fatal("data: the current directory is unusable");
    const std::wstring path = std::wstring(dir) + L"\\BOF3.exe";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!EnvSet("BOF3X_EXEIMAGE"))
            Fatal("data: no BOF3.exe in %s and BOF3X_EXEIMAGE unset. bof3x.exe runs with the game's directory as its "
                  "current directory (bof3x-launcher starts it so); .data comes from the player's own BOF3.exe there, "
                  "or from the cache BOF3X_EXEIMAGE names.", Utf8Path(dir).text);
        Log("data: no BOF3.exe in %s: a disc-built install - .data from BOF3X_EXEIMAGE's cache, nothing read from "
            "the file, and the checks against it skipped", Utf8Path(dir).text);
        return;
    }

    // The hash before any byte of the file is used.
    std::string sha, why;
    if (!bof3x::Sha256Hex(path.c_str(), sha, why)) Fatal("data: cannot hash %s: %s", Utf8Path(path.c_str()).text, why.c_str());
    if (sha != image::kSha256)
        Fatal("data: %s is not the build this project targets (sha256 %s, expected %s). Every address is specific to "
              "that one file; refusing to start.", Utf8Path(path.c_str()).text, sha.c_str(), image::kSha256);
    g_exe = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_exe == INVALID_HANDLE_VALUE) Fatal("data: cannot open %s, error %lu", Utf8Path(path.c_str()).text, GetLastError());
    Log("data: %s, sha256 %s (the build symbols.toml describes)", Utf8Path(path.c_str()).text, sha.c_str());

    g_text = Section(".text");
    g_rdata = Section(".rdata");
    g_data = Section(".data");
    if (g_text.va != kStubVa || g_text.raw_size > kStubSize || g_rdata.va != kRdataVa || g_rdata.raw_size != kRdataBytes ||
        g_data.va != kDataVa || g_data.raw_size != kDataBytes || g_data.va + g_data.vsize != kBssEnd)
        Fatal("data: BOF3.exe's sections (.text 0x%X +0x%X, .rdata 0x%X +0x%X, .data 0x%X +0x%X to 0x%X) are not the "
              "ones exe_layout.h was written for", (unsigned)g_text.va, (unsigned)g_text.raw_size, (unsigned)g_rdata.va,
              (unsigned)g_rdata.raw_size, (unsigned)g_data.va, (unsigned)g_data.raw_size, (unsigned)(g_data.va + g_data.vsize));
}

void CopyData() {
    OpenExe();

    // .game is a zero-filled section: asserted, not trusted, before the copy.
    const auto* game = static_cast<const std::uint8_t*>(At(kGameVa));
    for (std::uint32_t i = 0; i < kGameSize; ++i)
        if (game[i]) Fatal("data: .game is not zero at 0x%08X before the copy", (unsigned)(kGameVa + i));

    // .rdata: the engine's own, none of it from the file (rdata_consts.h);
    // the import slots are bound in step 4.
    rdata::Map();
    const bool from_cache = ExeImage_Source(At(kDataVa), kDataVa, kDataBytes, kBssEnd);
    if (!from_cache) ReadAt(g_data.raw, At(kDataVa), kDataBytes, ".data");
    Shell_MapDefaultKeys();   // Key_TableDefault from the engine's rule, against the file's when it is there
    ExeImage_LayOptTables(At(kDataVa), kDataVa, kDataBytes);   // DIV-0111: a BOF3X_OPT layer's .data ranges
    ExeImage_CompareWithFile();   // a disc-built image, filled, against the file's .data when it is there

    // .bss: the original loader zeroed it, and so did ours - asserted, to
    // Capcom's .rsrc's end.
    for (std::uint32_t a = kDataVa + kDataBytes; a < kGameVa + kGameSize; ++a)
        if (*static_cast<const std::uint8_t*>(At(a))) Fatal("data: .bss is not zero at 0x%08X after the copy", (unsigned)a);
    Log("data: .rdata 0x%08X + 0x%X bytes the engine's (none from the file), .data 0x%08X + 0x%X bytes from %s; .bss "
        "zero 0x%08X..0x%08X (virtual end 0x%08X)",
        (unsigned)kRdataVa, (unsigned)kRdataBytes, (unsigned)kDataVa, (unsigned)kDataBytes,
        from_cache ? "BOF3X_EXEIMAGE's data.bin" : "the file", (unsigned)(kDataVa + kDataBytes),
        (unsigned)(kGameVa + kGameSize), (unsigned)kBssEnd);
}

// --- 4. The import slots ---------------------------------------------------

// What src/ calls through BOF3.exe's IAT (docs/exe-import-engine.md section 2;
// each name read from the player's import directory, docs/state3-build.md
// section 2.2). The loader bound them hosted; here the engine does.
struct Slot {
    std::uint32_t at;
    const char* dll;
    const char* name;   // null: by ordinal
    unsigned ordinal;
};
constexpr Slot kSlots[] = {
    {0x5C4014, "DSOUND.dll", nullptr, 1},                 // DirectSoundCreate: the thunk 0x5ACBBC's
    {0x5C4020, "GDI32.dll", "SetTextColor", 0},           // psx_rest_callees.h
    {0x5C4024, "GDI32.dll", "SetBkMode", 0},
    {0x5C4028, "GDI32.dll", "TextOutA", 0},
    {0x5C407C, "KERNEL32.dll", "GetTickCount", 0},        // Imp_GetTickCount: game_clock.cpp, win_main.cpp
    {0x5C4080, "KERNEL32.dll", "CreateEventA", 0},        // sound_callees.h
    {0x5C4084, "KERNEL32.dll", "CloseHandle", 0},
    {0x5C4088, "KERNEL32.dll", "GetDriveTypeA", 0},       // shell_callees.h
    {0x5C40B8, "KERNEL32.dll", "ExitProcess", 0},         // win_main.cpp's exit trace
    {0x5C4114, "KERNEL32.dll", "TerminateProcess", 0},
    {0x5C4158, "USER32.dll", "PostQuitMessage", 0},
    {0x5C4164, "USER32.dll", "MsgWaitForMultipleObjects", 0},   // sound_callees.h
    {0x5C4168, "USER32.dll", "IntersectRect", 0},         // gfx_texcache.cpp's "bound yet" test
    {0x5C4174, "USER32.dll", "MessageBoxA", 0},           // psx_rest_callees.h, the exit trace
};

FARPROC Resolve(const Slot& s) {
    HMODULE dll = GetModuleHandleA(s.dll);
    if (!dll) dll = LoadLibraryA(s.dll);
    if (!dll) Fatal("slots: cannot load %s for the slot 0x%08X, error %lu", s.dll, (unsigned)s.at, GetLastError());
    FARPROC fn = s.name ? GetProcAddress(dll, s.name)
                        : GetProcAddress(dll, reinterpret_cast<LPCSTR>(static_cast<std::uintptr_t>(s.ordinal)));
    if (!fn) Fatal("slots: %s has no %s%s (#%u) for the slot 0x%08X", s.dll, s.name ? s.name : "ordinal", s.name ? "" : "",
                   s.ordinal, (unsigned)s.at);
    return fn;
}

void BindSlots() {
    for (const Slot& s : kSlots) {
        const FARPROC fn = Resolve(s);
        std::memcpy(At(s.at), &fn, 4);
    }
    // The self-test: every row read back, and GetTickCount called through its slot.
    for (const Slot& s : kSlots) {
        FARPROC now;
        std::memcpy(&now, At(s.at), 4);
        if (now != Resolve(s)) Fatal("slots: 0x%08X does not read back as %s!%s", (unsigned)s.at, s.dll, s.name ? s.name : "#1");
    }
    const DWORD before = GetTickCount();
    const DWORD through = reinterpret_cast<DWORD(WINAPI*)()>(Imp_GetTickCount)();
    const DWORD after = GetTickCount();
    if (through < before || through > after)
        Fatal("slots: GetTickCount through 0x5C407C answered %lu, outside %lu..%lu", through, before, after);
    Log("slots: %u import slots bound and read back (DSOUND #1, GDI32 3, KERNEL32 6, USER32 4); GetTickCount through "
        "0x5C407C answers %lu", static_cast<unsigned>(sizeof kSlots / sizeof kSlots[0]), through);
}

// --- 5. The x87 control word -------------------------------------------------

void SetX87() {
    unsigned short before = 0, cw = 0x027F, after = 0;
    unsigned mxcsr = 0;
    asm volatile("fnstcw %0" : "=m"(before));
    asm volatile("fldcw %0" : : "m"(cw));
    asm volatile("fnstcw %0" : "=m"(after));
    asm volatile("stmxcsr %0" : "=m"(mxcsr));
    if (after != 0x027F) Fatal("x87: the control word reads back 0x%04X after fldcw 0x027F", after);
    Log("x87: control word 0x%04X -> 0x%04X (53-bit precision, every exception masked, round to nearest, as "
        "Capcom's start-up left it); MXCSR 0x%08X, not set", before, after, mxcsr);
}

// --- 6. The stub page --------------------------------------------------------

// DirectSoundCreate's import thunk, `jmp [0x5C4014]`, which Snd_Init calls
// as a function (sound_rest_callees.h kDirectSoundCreate): ours now, with
// the slot BindSlots filled.
constexpr std::uint32_t kDirectSoundThunk = 0x5ACBBC;
constexpr std::uint8_t kThunk[6] = {0xFF, 0x25, 0x14, 0x40, 0x5C, 0x00};

void FillStub() {
    DWORD old = 0;
    if (!VirtualProtect(At(kStubVa), kStubSize, PAGE_EXECUTE_READWRITE, &old))
        Fatal("stub: VirtualProtect(.stub) failed, error %lu", GetLastError());
    std::memset(At(kStubVa), 0xCC, kStubSize);
    Log("stub: .stub 0x%08X..0x%08X filled with int3", (unsigned)kStubVa, (unsigned)(kStubVa + kStubSize));
}

void WriteThunk() {
    auto* p = static_cast<std::uint8_t*>(At(kDirectSoundThunk));
    for (unsigned i = 0; i < sizeof kThunk; ++i)
        if (p[i] != 0xCC) Fatal("stub: DirectSoundCreate's thunk 0x%08X overlaps a jmp (0x%02X at +%u)", (unsigned)kDirectSoundThunk, p[i], i);
    std::memcpy(p, kThunk, sizeof kThunk);
}

// The inventory's self-test (docs/state3-design.md section 7): nothing of
// ours wrote .stub but Inject's jmps and the thunk. A PatchBytes or a
// RetargetCall into Capcom's code writes nothing in this host
// (docs/operand-store.md), so every other byte must still be int3.
void CheckStub() {
    const auto* page = static_cast<const std::uint8_t*>(At(kStubVa));
    static std::uint8_t mine[kStubSize];   // 1: written by a jmp or the thunk
    unsigned jmps = 0;
    for (int i = 0; i < OwnedCount(); ++i) {
        const std::uint32_t a = OwnedAddress(i);
        if (a < kStubVa || a + 5 > kStubVa + kStubSize) Fatal("stub: an owned start 0x%08X outside .stub", (unsigned)a);
        if (page[a - kStubVa] != 0xE9) Fatal("stub: the owned start 0x%08X holds 0x%02X, not a jmp", (unsigned)a, page[a - kStubVa]);
        for (unsigned k = 0; k < 5; ++k) {
            if (mine[a - kStubVa + k]) Fatal("stub: the jmp at 0x%08X overlaps another at 0x%08X", (unsigned)a, (unsigned)(a + k));
            mine[a - kStubVa + k] = 1;
        }
        ++jmps;
    }
    for (unsigned k = 0; k < sizeof kThunk; ++k) mine[kDirectSoundThunk - kStubVa + k] = 1;
    unsigned stray = 0;
    std::uint32_t first = 0;
    for (std::uint32_t i = 0; i < kStubSize; ++i) {
        if (mine[i] || page[i] == 0xCC) continue;
        if (!stray) first = kStubVa + i;
        ++stray;
    }
    if (stray)
        Fatal("stub: %u bytes of .stub were written by something other than Inject, the first at 0x%08X (0x%02X): a "
              "write into Capcom's code that the exe host has no home for", stray, (unsigned)first, page[first - kStubVa]);
    Log("stub: inventory zero - .stub holds %u jmps of ours and DirectSoundCreate's thunk 0x%08X, every other byte "
        "int3; no PatchBytes or RetargetCall wrote it", jmps, (unsigned)kDirectSoundThunk);
}

void ProtectStub() {
    DWORD old = 0;
    if (!VirtualProtect(At(kStubVa), kStubSize, PAGE_EXECUTE_READ, &old))
        Fatal("stub: VirtualProtect(.stub, read-execute) failed, error %lu", GetLastError());
    FlushInstructionCache(GetCurrentProcess(), At(kStubVa), kStubSize);
    for (std::uint32_t a = kStubVa; a < kStubVa + kStubSize;) {
        MEMORY_BASIC_INFORMATION m{};
        VirtualQuery(At(a), &m, sizeof m);
        if (m.Protect != PAGE_EXECUTE_READ) Fatal("stub: 0x%08X is protect 0x%lX after the change", (unsigned)a, m.Protect);
        a = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m.BaseAddress)) + static_cast<std::uint32_t>(m.RegionSize);
    }
    Log("stub: .stub is read-execute; a later write faults");
}

// --- 7. The exception nobody handled ----------------------------------------

// Capcom's entry had an __except that _exit()ed silently; the runtime's
// R6xxx boxes went with the runtime. Ours: the crash reporter (vectored,
// already armed) has written its CRASH lines and dump; this ends the process
// as Fatal does, code 3.
LONG WINAPI Unhandled(EXCEPTION_POINTERS* info) {
    Fatal("an exception nobody handled: 0x%08lX at 0x%08lX (see the CRASH lines above)",
          info->ExceptionRecord->ExceptionCode,
          static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress)));
}

// The command line past the program name, as Capcom's _wincmdln produced it:
// a quoted name to its closing quote, else to the first space or control
// character; then the white space after it.
char* PastProgramName(const char* line) {
    const char* p = line ? line : "";
    if (*p == '"') {
        ++p;
        while (*p && *p != '"') ++p;
        if (*p == '"') ++p;
    } else {
        while (static_cast<unsigned char>(*p) > ' ') ++p;
    }
    while (*p && static_cast<unsigned char>(*p) <= ' ') ++p;
    return const_cast<char*>(p);
}

}  // namespace
}  // namespace bof3

extern "C" __declspec(dllexport) int __cdecl Engine_Main(void* hinstance, const char* command_line, int show) {
    using namespace bof3;
    static bool entered = false;
    if (entered) Fatal("Engine_Main entered twice");
    entered = true;
    if (!IsExeHost()) Fatal("Engine_Main in the hosted build: only bof3x.exe's entry calls it");
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&Engine_Main), &module);
    Log("Engine_Main: hinstance %p, show %d, on thread %lu", hinstance, show, GetCurrentThreadId());
    // docs/code-page-audit.md section 4: the manifest asks for UTF-8; an older
    // Windows that ignores it shows here and behaves as the hosted build.
    Log("code page: GetACP() = %u%s", GetACP(), GetACP() == CP_UTF8 ? " (UTF-8, as the manifest asks)" : " (not UTF-8: the manifest's activeCodePage was not honoured)");

    RefuseHostedSwitches();
    CheckLayout();
    CopyData();
    BindSlots();
    SetX87();

    Crash_Start(module);
    if (EnvSet("BOF3X_INPUT") || EnvSet("BOF3X_SELFTEST_ONLY")) {
        // An unattended run marks itself for tools/recipe_saves.py (dllmain.cpp).
        wchar_t name[64];
        std::swprintf(name, 64, L"Local\\bof3x_unattended_%lu", GetCurrentProcessId());
        CreateEventW(nullptr, TRUE, FALSE, name);
    }
    StateHash_BeforeInjects(module);   // .data as the engine copied it
    FillStub();
    InjectAll();
    WriteThunk();
    StateHash_AfterInjects();
    CheckStub();
    ProtectStub();
    StackWatch_Start();
    if (EnvSet("BOF3X_SELFTEST_ONLY")) {
        ExeIcon_SelfTest();   // DIV-0095: only with BOF3X_ICON_VRAM; .data (the icon table) is copied by now
        Log("self-test only: done, ending the process");
        LogClose();
        TerminateProcess(GetCurrentProcess(), 0);
    }
    if (EnvSet("BOF3X_ICON_VRAM")) Log("DIV-0095    icon: BOF3X_ICON_VRAM is a self-test switch (BOF3X_SELFTEST_ONLY); ignored");
    CallTrace_Start(module);   // refused above when asked for; a no-op here otherwise
    WavDump_Start();   // BOF3X_WAV_DIR: after BindSlots filled 0x5C4014, before the latch (wav_dump.h)
    InputScript_Start();
    draw_order::Start();
    SetUnhandledExceptionFilter(Unhandled);
    char* const line = PastProgramName(command_line);
    Log("Engine_Main: Game_WinMain(0x%p, null, \"%s\", %d) on the main thread", hinstance, line, show);
    const int code = Game_WinMain(hinstance, nullptr, line, show);
    Log("Engine_Main: Game_WinMain returned %d, the exit code", code);
    return code;
}

namespace bof3 {

bool ExeFile_Present() { return !IsExeHost() || g_exe != INVALID_HANDLE_VALUE; }

void ExeFile_Read(std::uint32_t va, void* to, std::uint32_t n) {
    if (!IsExeHost()) Fatal("ExeFile_Read(0x%08X) in the hosted build: Capcom's image is mapped, read it there", (unsigned)va);
    if (g_exe == INVALID_HANDLE_VALUE) Fatal("ExeFile_Read(0x%08X): there is no BOF3.exe (ask ExeFile_Present first)", (unsigned)va);
    for (const FileSection* s : {&g_text, &g_rdata, &g_data})
        if (va >= s->va && va + n <= s->va + s->raw_size && va + n >= va) {
            ReadAt(s->raw + (va - s->va), to, n, "a verification's bytes");
            return;
        }
    Fatal("ExeFile_Read(0x%08X + %u): not inside the initialised bytes of BOF3.exe's .text, .rdata or .data", (unsigned)va,
          (unsigned)n);
}

}  // namespace bof3
