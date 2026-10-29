// The exe host's start-up: Engine_Main, called by bof3x.exe's entry on the
// process's main thread once the loader is done (docs/state3-design.md
// section 4, in its order). DllMain has only opened the log and told the
// hosts apart (dllmain.cpp).
//
//  1. host detection (DllMain) - and the switches that need Capcom's code
//     refused: BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_MP3=original,
//     BOF3X_CALLTRACE
//  2. the fence: when the image lies elsewhere than 0x400000 (a developer's
//     bof3x.exe written by make_exe.py --image-base,
//     docs/relocatable-image-design.md 1.5), the free parts of the linked
//     span reserved no-access; then the layout: our sections where
//     exe_layout.h puts them, relative to the image's base; then .stub made
//     the trap - no access, nothing written into it (design 4.5)
//  3. the data: the player's BOF3.exe by SHA-256 when it is there (optional
//     with BOF3X_EXEIMAGE set: a disc-built install has none); .rdata the
//     engine's own (rdata::Map: the 44 constants and the keyboard format;
//     the slots in 4), nothing of it from the file; .data from
//     BOF3X_EXEIMAGE's cache when set, else the file's; the default key
//     table written from the engine's rule; the .bss asserted zero; then
//     every pointer word checked, the data words moved with the image and the
//     code words bound to our functions (design 3, 4.1)
//  4. the fourteen import slots our code calls through, bound and read back
//  5. the x87 control word 0x027F, read back; MXCSR logged
//  6. the state hash's census; InjectAll, which writes nothing into the image's
//     code - every Inject is checked against the bind table (bind.cpp), and
//     DrawPool_Reserve takes .pool; the census after
//
// Every address below is a VA, Capcom's frame; its place is Img(va)
// (common/image_base.h), the identity at the player's base.
//  7. the unhandled-exception filter; Game_WinMain on this thread (whose
//     loop generates the icon, DIV-0095, exe_icon.h)
#include "hook/exe_host.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "bof3/rebase.gen.h"
#include "bof3/symbols.gen.h"
#include "common/sha256.h"
#include "game/rdata_consts.h"
#include "game/shell.h"
#include "hook/bind.h"
#include "hook/calltrace.h"
#include "hook/crash.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/stp_census.h"
#include "hook/exe_icon.h"
#include "hook/exe_image.h"
#include "hook/exe_layout.h"
#include "hook/gapwatch.h"
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

// A place as a pointer, and a VA's place (image_base.h): every constant of
// exe_layout.h is a VA, in the frame the image was linked in; the image lies
// at ImageBase(), 0x400000 for the player's executable, and the log prints
// both where they differ.
void* At(std::uint32_t place) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(place)); }
void* Place(std::uint32_t va) { return At(Img(va)); }
std::uint32_t AsU32(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

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
    for (std::uint32_t a = Img(va); a < Img(va) + size;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(At(a), &m, sizeof m)) Fatal("layout: VirtualQuery(0x%08X) failed, error %lu", (unsigned)a, GetLastError());
        const DWORD p = m.Protect & 0xFF;
        const bool writable = p == PAGE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READWRITE ||
                              p == PAGE_EXECUTE_WRITECOPY;
        const bool executable = p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
        if (m.State != MEM_COMMIT || m.Type != MEM_IMAGE || m.AllocationBase != At(ImageBase()) || !writable ||
            executable != exec)
            Fatal("layout: %s at 0x%08X (VA 0x%08X) is not our image's %s section (state 0x%lX type 0x%lX protect "
                  "0x%lX base %p): a rewritten bof3x.exe",
                  name, (unsigned)a, (unsigned)(a - Img(va) + va), exec ? "read-write-execute" : "read-write", m.State,
                  m.Type, m.Protect, m.AllocationBase);
        a = AsU32(m.BaseAddress) + static_cast<std::uint32_t>(m.RegionSize);
    }
}

void CheckLayout() {
    CheckSection(".stub", kStubVa, kStubSize, true);
    CheckSection(".game", kGameVa, kGameSize, false);
    CheckSection(".pool", kPoolVa, kPoolSize, false);
    Log("layout: .stub 0x%08X +0x%X RWX, .game 0x%08X +0x%X RW, .pool 0x%08X +0x%X RW, as exe_layout.h (VAs; the "
        "image at 0x%08X)",
        (unsigned)kStubVa, (unsigned)kStubSize, (unsigned)kGameVa, (unsigned)kGameSize, (unsigned)kPoolVa,
        (unsigned)kPoolSize, (unsigned)ImageBase());
}

// --- 2a. The fence (docs/relocatable-image-design.md 1.5) --------------------

// The linked span, 0x400000 to the executable's SizeOfImage: every VA of
// ours names a place in it.
std::uint32_t LinkedEnd() {
    const auto* base = static_cast<const std::uint8_t*>(At(ImageBase()));
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    return kImageBase + nt->OptionalHeader.SizeOfImage;
}

// When the image lies elsewhere than 0x400000, the free parts of the linked
// span are reserved no-access for the life of the process: an address our
// source left absolute - a data VA read or written as a place, a code VA
// called - then faults at its own VA, and the crash reporter names it. By
// Engine_Main the process's own early allocations (its heaps, mapped NLS and
// locale files, thread stacks) hold most of the span - measured 2026-10-28,
// all but 64 KB - so the developer launcher reserves it first, while the
// process is suspended (launcher.cpp FenceLinkedSpan); those reservations are
// counted here, and what is still free is taken. What neither could take is
// logged region by region (design 13). A module in the linked code range is a Fatal: CodePlace
// (image_base.h) reads a word there as a VA, and a word into a module there
// would be one of its functions.
// The largest free run below 16 MB at this point (design 13, option A: an
// image in a VirtualAlloc block would need 0x587000 bytes there). A
// measurement for the log; it changes nothing.
void LogLowFreeRun() {
    std::uint32_t best = 0, best_at = 0;
    for (std::uint32_t a = 0x10000; a < 0x1000000;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(At(a), &m, sizeof m)) break;
        const std::uint32_t r_lo = AsU32(m.BaseAddress);
        std::uint32_t r_hi = r_lo + static_cast<std::uint32_t>(m.RegionSize);
        if (r_hi > 0x1000000) r_hi = 0x1000000;
        if (m.State == MEM_FREE && r_hi - a > best) best = r_hi - a, best_at = a;
        a = r_hi;
    }
    Log("fence: the largest free run below 16 MB at Engine_Main is 0x%X bytes at 0x%08X (the image takes 0x%X)",
        (unsigned)best, (unsigned)best_at, (unsigned)(kPoolVa + kPoolSize - kImageBase));
}

void Fence() {
    const std::uint32_t lo = kImageBase, hi = LinkedEnd();
    LogLowFreeRun();
    if (ImageBase() == kImageBase) {
        Log("fence: the image at its linked base 0x%08X; nothing to fence", (unsigned)lo);
        return;
    }
    std::uint32_t fenced = 0, regions = 0, other = 0;
    for (std::uint32_t a = lo; a < hi;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(At(a), &m, sizeof m)) Fatal("fence: VirtualQuery(0x%08X) failed, error %lu", (unsigned)a, GetLastError());
        const std::uint32_t r_lo = AsU32(m.BaseAddress);
        std::uint32_t r_hi = r_lo + static_cast<std::uint32_t>(m.RegionSize);
        if (r_hi > hi) r_hi = hi;
        if (m.State == MEM_FREE) {
            // A reservation starts at a 64 KB boundary.
            const std::uint32_t from = (a + 0xFFFF) & ~0xFFFFu;
            if (from > a) Log("fence: 0x%08X..0x%08X free, not taken (below a 64 KB boundary)", (unsigned)a, (unsigned)(from < r_hi ? from : r_hi));
            if (from < r_hi) {
                if (!VirtualAlloc(At(from), r_hi - from, MEM_RESERVE, PAGE_NOACCESS))
                    Fatal("fence: cannot reserve 0x%08X..0x%08X, error %lu", (unsigned)from, (unsigned)r_hi, GetLastError());
                fenced += r_hi - from;
                ++regions;
            }
        } else if (m.State == MEM_RESERVE && m.Type == MEM_PRIVATE && m.AllocationProtect == PAGE_NOACCESS) {
            // The developer launcher's reservation, made while this process
            // was suspended (launcher.cpp FenceLinkedSpan): already a fence.
            fenced += r_hi - a;
            ++regions;
        } else {
            char path[MAX_PATH] = "";
            if (m.Type == MEM_IMAGE) GetModuleFileNameA(static_cast<HMODULE>(m.AllocationBase), path, sizeof path);
            Log("fence: 0x%08X..0x%08X not free, left: state 0x%lX type 0x%lX protect 0x%lX allocation %p%s%s",
                (unsigned)a, (unsigned)r_hi, m.State, m.Type, m.Protect, m.AllocationBase, *path ? ", " : "", path);
            other += r_hi - a;
            if (m.Type == MEM_IMAGE && a < image::kCodeHi && r_hi > image::kCodeLo)
                Fatal("fence: the module %s lies at 0x%08X..0x%08X, in the linked code range 0x%X..0x%X, where a code "
                      "word is read as a VA (CodePlace); start bof3x.exe at another base",
                      *path ? path : "(unnamed)", (unsigned)a, (unsigned)r_hi, (unsigned)image::kCodeLo,
                      (unsigned)image::kCodeHi);
        }
        a = r_hi;
    }
    Log("fence: the image at 0x%08X, moved from 0x%08X: 0x%X bytes of the linked span 0x%08X..0x%08X reserved "
        "no-access in %u regions, 0x%X bytes held by something else (above)",
        (unsigned)ImageBase(), (unsigned)lo, (unsigned)fenced, (unsigned)lo, (unsigned)hi, (unsigned)regions,
        (unsigned)other);
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
    const auto* game = static_cast<const std::uint8_t*>(Place(kGameVa));
    for (std::uint32_t i = 0; i < kGameSize; ++i)
        if (game[i]) Fatal("data: .game is not zero at VA 0x%08X before the copy", (unsigned)(kGameVa + i));

    // .rdata: the engine's own, none of it from the file (rdata_consts.h);
    // the import slots are bound in step 4.
    rdata::Map();
    const bool from_cache = ExeImage_Source(Place(kDataVa), kDataVa, kDataBytes, kBssEnd);
    if (!from_cache) ReadAt(g_data.raw, Place(kDataVa), kDataBytes, ".data");
    Shell_MapDefaultKeys();   // Key_TableDefault from the engine's rule, against the file's when it is there
    ExeImage_LayOptTables(Place(kDataVa), kDataVa, kDataBytes);   // DIV-0111: a BOF3X_OPT layer's .data ranges
    ExeImage_CompareWithFile();   // a disc-built image, filled, against the file's .data when it is there

    // .bss: the original loader zeroed it, and so did ours - asserted, to
    // Capcom's .rsrc's end.
    for (std::uint32_t a = kDataVa + kDataBytes; a < kGameVa + kGameSize; ++a)
        if (*static_cast<const std::uint8_t*>(Place(a))) Fatal("data: .bss is not zero at VA 0x%08X after the copy", (unsigned)a);
    Log("data: .rdata 0x%08X + 0x%X bytes the engine's (none from the file), .data 0x%08X + 0x%X bytes from %s; .bss "
        "zero 0x%08X..0x%08X (virtual end 0x%08X)",
        (unsigned)kRdataVa, (unsigned)kRdataBytes, (unsigned)kDataVa, (unsigned)kDataBytes,
        from_cache ? "BOF3X_EXEIMAGE's data.bin" : "the file", (unsigned)(kDataVa + kDataBytes),
        (unsigned)(kGameVa + kGameSize), (unsigned)kBssEnd);
}

// --- 3a. The rebase (docs/relocatable-image-design.md 3) ----------------------

// After the copy and every fill, before anything of ours writes .data
// (BindSlots, the state hash's census, InjectAll, the language overlays): each
// pointer word of rebase.gen.h (recipes/exe-pointers.tsv's runs, less four
// numbers the recipe holds as pointers, tools/gen_rebase.py) is checked to
// hold an address in its section's linked range - else a Fatal naming the
// word: a layer or a cache that wrote a number over a pointer is refused, not
// moved - and then the data words moved by ImageBase() - 0x400000. Code words
// are bound (mode B, design 4.1): each becomes the function of ours the bind
// table names for its VA, or the trap Img(va) when the table names none
// (design 4.2: the C runtime's tables, the blitters' jump table, three words at
// the head of .data). At the player's base the data move is zero and the check
// still runs: it is the loader's own test of the table against the data it
// copied.
void Rebase() {
    struct Linked {
        const char* name;
        std::uint32_t lo, hi;
    };
    const Linked linked[4] = {
        {"data", kDataVa, kBssEnd},
        {"bss", kDataVa, kBssEnd},
        {"rdata", kRdataVa, kDataVa},
        {"text", kStubVa, kStubVa + kStubSize},
    };
    const std::uint32_t move = ImageBase() - kImageBase;
    unsigned words[4] = {};
    unsigned bound = 0, trapped = 0;
    for (const rebase::Run& r : rebase::kRuns) {
        const Linked& want = linked[r.to];
        if (r.pc < kDataVa || r.pc + 4u * r.words > kDataVa + kDataBytes)
            Fatal("rebase: the run 0x%08X x%u lies outside .data's file bytes", (unsigned)r.pc, (unsigned)r.words);
        for (unsigned k = 0; k < r.words; ++k) {
            const std::uint32_t va = r.pc + 4 * k;
            std::uint32_t v;
            std::memcpy(&v, Place(va), 4);
            if (v < want.lo || v >= want.hi)
                Fatal("rebase: the %s pointer word at VA 0x%08X holds 0x%08X, outside 0x%08X..0x%08X: not the pointer "
                      "recipes/exe-pointers.tsv lists (a layer or a cache wrote a number there)",
                      want.name, (unsigned)va, (unsigned)v, (unsigned)want.lo, (unsigned)want.hi);
            if (r.to == rebase::kText) {
                bool b = false;
                v = Bind_Word(v, &b);
                ++(b ? bound : trapped);
            } else {
                v += move;
            }
            std::memcpy(Place(va), &v, 4);
        }
        words[r.to] += r.words;
    }
    for (unsigned c = 0; c < 4; ++c)
        if (words[c] != rebase::kWords[c])
            Fatal("rebase: %u %s words moved, the table counts %u", words[c], linked[c].name, rebase::kWords[c]);
    Log("rebase: %u runs, words data %u, bss %u, rdata %u, text %u (%u the recipe holds as pointers left alone: strings and numbers) checked in "
        "their linked ranges; the data words moved by 0x%X (the image at 0x%08X); the code words bound, %u to a function "
        "of ours and %u to the trap (no function of ours: design 4.2)",
        rebase::kRunCount, words[rebase::kData], words[rebase::kBss], words[rebase::kRdata], words[rebase::kText],
        rebase::kNotMoved, (unsigned)move, (unsigned)ImageBase(), bound, trapped);
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
        std::memcpy(Place(s.at), &fn, 4);
    }
    // The self-test: every row read back, and GetTickCount called through its slot.
    for (const Slot& s : kSlots) {
        FARPROC now;
        std::memcpy(&now, Place(s.at), 4);
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

// --- 2b. The trap (docs/relocatable-image-design.md 4.5) ----------------------

// .stub, the VAs of Capcom's code, holds nothing: every code word and every
// Code() of ours is a function of ours (bind.cpp), so the page is made
// no-access before anything runs. A code value that was not bound - one of
// the 111 words with no function of ours, a VA placed without Code(), a value
// computed from a code constant - lands on it and faults at
// base + (VA - 0x400000), and the crash reporter writes the VA and its
// [[func]] (crash.cpp). Nothing writes it: Inject records its pair, and
// PatchBytes and RetargetCall write no code in this host
// (docs/operand-store.md).
void TrapStub() {
    DWORD old = 0;
    if (!VirtualProtect(Place(kStubVa), kStubSize, PAGE_NOACCESS, &old))
        Fatal("stub: VirtualProtect(.stub, no access) failed, error %lu", GetLastError());
    for (std::uint32_t a = Img(kStubVa); a < Img(kStubVa) + kStubSize;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(At(a), &m, sizeof m)) Fatal("stub: VirtualQuery(0x%08X) failed, error %lu", (unsigned)a, GetLastError());
        if (m.Protect != PAGE_NOACCESS) Fatal("stub: 0x%08X is protect 0x%lX after the change", (unsigned)a, m.Protect);
        a = AsU32(m.BaseAddress) + static_cast<std::uint32_t>(m.RegionSize);
    }
    Log("stub: .stub 0x%08X..0x%08X (VA 0x%08X..0x%08X) is no-access: the trap for a code value that is not bound",
        (unsigned)Img(kStubVa), (unsigned)Img(kStubVa + kStubSize), (unsigned)kStubVa, (unsigned)(kStubVa + kStubSize));
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

// bof3x.exe double-clicked, or started by anything but the launcher (which
// sets BOF3X_LAUNCHER): the game folder, the cache, the language and the
// layers are the launcher's to resolve from the install's folder and its
// bof3x.ini, so the bof3x-launcher.exe beside us is started with --no-config
// - the game straight away, as the launcher would start it - and this
// process ends (the owner, 2026-10-15). BOF3X_HANDOFF marks that launcher's
// environment, so a launcher that does not set BOF3X_LAUNCHER cannot loop us.
// With no launcher beside us, nothing changes: the checks below say what is
// missing.
void HandOffToLauncher() {
    if (EnvSet("BOF3X_LAUNCHER") || EnvSet("BOF3X_HANDOFF")) return;
    wchar_t self[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    std::wstring dir(self, n);
    dir.resize(dir.find_last_of(L"\\/") + 1);
    const std::wstring launcher = dir + L"bof3x-launcher.exe";
    if (GetFileAttributesW(launcher.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    SetEnvironmentVariableW(L"BOF3X_HANDOFF", L"1");
    std::wstring line = L"\"" + launcher + L"\" --no-config";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(launcher.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
        Fatal("bof3x.exe was not started by bof3x-launcher, and starting %ls failed (%lu)", launcher.c_str(), GetLastError());
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Log("Engine_Main: not started by bof3x-launcher: handed over to %ls --no-config (pid %lu); ending",
        launcher.c_str(), pi.dwProcessId);
    LogClose();
    TerminateProcess(GetCurrentProcess(), 0);
}

}  // namespace
}  // namespace bof3

extern "C" __declspec(dllexport) int __cdecl Engine_Main(void* hinstance, const char* command_line, int show) {
    using namespace bof3;
    static bool entered = false;
    if (entered) Fatal("Engine_Main entered twice");
    entered = true;
    if (!IsExeHost()) Fatal("Engine_Main in the hosted build: only bof3x.exe's entry calls it");
    HandOffToLauncher();
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&Engine_Main), &module);
    Log("Engine_Main: hinstance %p, show %d, on thread %lu", hinstance, show, GetCurrentThreadId());
    // docs/code-page-audit.md section 4: the manifest asks for UTF-8; an older
    // Windows that ignores it shows here and behaves as the hosted build.
    Log("code page: GetACP() = %u%s", GetACP(), GetACP() == CP_UTF8 ? " (UTF-8, as the manifest asks)" : " (not UTF-8: the manifest's activeCodePage was not honoured)");

    RefuseHostedSwitches();
    Fence();
    CheckLayout();
    TrapStub();
    CopyData();
    Rebase();
    BindSlots();
    SetX87();

    Crash_Start(module);
    if (EnvSet("BOF3X_INPUT") || EnvSet("BOF3X_SELFTEST_ONLY")) {
        // An unattended run marks itself for tools/recipe_saves.py (dllmain.cpp).
        wchar_t name[64];
        std::swprintf(name, 64, L"Local\\bof3x_unattended_%lu", GetCurrentProcessId());
        CreateEventW(nullptr, TRUE, FALSE, name);
    }
    StateHash_SetImageBase(ImageBase());   // .data read at its place; a moved image's range folded
    StateHash_BeforeInjects(module);   // .data as the engine copied, moved and bound it
    InjectAll();
    StateHash_AfterInjects();
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
    GapWatch_Start(module);   // BOF3X_GAPWATCH: after the recipe has read its addresses (gapwatch.h)
    draw_order::Start();
    stp_census::Start();
    SetUnhandledExceptionFilter(Unhandled);
    char* const line = PastProgramName(command_line);
    Log("Engine_Main: Game_WinMain(0x%p, null, \"%s\", %d) on the main thread", hinstance, line, show);
    const int code = Game_WinMain(hinstance, nullptr, line, show);
    GapWatch_Flush();
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
