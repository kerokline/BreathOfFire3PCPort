// The exe host's start-up on Linux: `main`, which does what Windows' loader,
// DllMain and Engine_Main do for bof3x.exe (docs/linux-round.md, "R3: what a
// Linux start-up needs from the PE host", and "B1 as run"). The order is
// src/hook/exe_host_steps.h's 25 steps; each is marked below by its number,
// with what it does on this system where that is not what Windows does.
// Where a step's code is exe_host.cpp's own (the switches refused, the data
// copied, the rebase, the x87 word, the command line), it is that code with
// the seam's calls in place of Windows': exe_host.cpp is a Windows file by
// design, and the two must be read side by side when either changes.
//
// There is no PE loader here. The ELF is linked at 0x10000000 (stage LA2);
// the image's span - Capcom's sections at their VAs, exe_layout.h - is mapped
// by this file's .preinit_array entry, before any initialiser of a library
// or of ours runs, at 0x400000 or at a developer's BOF3X_IMAGEBASE, with the
// main thread's stack at its Windows place (the reservations' table in
// "B1 as run"). `main` then moves the main thread onto that stack and runs
// the engine there, as Windows runs Engine_Main on the stack its header
// placed.
//
// The Windows builds do not compile this file's body; the Linux build adds
// it (tools/linux_probe/linux_tree.py, LINUX_ONLY).
#ifndef _WIN32

#include <SDL3/SDL.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "audio/sound_sdl.h"
#include "bof3/rebase.gen.h"
#include "bof3/symbols.gen.h"
#include "common/cache_manifest.h"
#include "common/sha256.h"
#include "game/draw_pool_room.h"
#include "game/platform/game_window_native.h"
#include "game/rdata_consts.h"
#include "game/shell.h"
#include "hook/bind.h"
#include "hook/calltrace.h"
#include "hook/crash.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/exe_file.h"
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
#include "hook/stp_census.h"
#include "hook/wav_dump.h"
#include "hook/write_watch.h"
#include "os/os.h"

#if !defined(__x86_64__)
#error "exe_host_posix.cpp: the Linux exe host is the 64-bit build alone (the import slots' thunks, bind.h)"
#endif

namespace bof3 {

namespace image {
std::uintptr_t g_ownBase = 0;   // image_base.h: the ELF's own base, step 5
}  // namespace image

namespace {

using namespace exe_layout;

void* At(std::uint32_t place) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(place)); }
void* Place(std::uint32_t va) { return At(Img(va)); }

bool EnvSet(const char* name) {
    char v[2];
    return os::GetEnv(name, v, sizeof v) > 0;
}

// --- 1. The reservations, before any initialiser ----------------------------

// The image's span on this system: Capcom's sections' extents, 0x400000 to
// the end of .pool. Windows' SizeOfImage reaches 0x98F000 with our own
// .text, .idata and .rsrc after .pool; here those are the ELF's, at
// 0x10000000, and nothing of ours lies after .pool.
constexpr std::uint32_t kSpan = kOwnVa - kImageBase;
// The main thread's stack at its Windows place (docs/x64-build-design.md 7.1,
// the 1 MB reservation 0x60000..0x160000), with one guard page under it.
constexpr std::uint32_t kStackLo = 0x60000, kStackHi = 0x160000, kGuard = 0x1000;

// What the .preinit_array entry did, for steps 8 to 10 to report: the log is
// not open that early, and nothing there may Fatal with a message the player
// never sees. An errno of 0 is a range made.
struct Reserved {
    std::uint32_t base = 0;        // the image's base, from BOF3X_IMAGEBASE or 0x400000
    bool base_asked = false;       // BOF3X_IMAGEBASE set (SetHost checks it against `base`)
    int image_errno = -1;          // row 1 of the table
    int header_errno = -1;         // row 1a
    int commit_errno = -1;         // rows 1c and 1d
    int fence_errno = -1;          // row 2 (a moved base only)
    int stack_errno = -1;          // row 3
};
Reserved g_reserved;

void* AtU(std::uintptr_t a) { return reinterpret_cast<void*>(a); }

// The image's base: BOF3X_IMAGEBASE when it names a base the 16 MB window
// holds at a 64 KB boundary, else 0x400000 - and a BOF3X_IMAGEBASE that is
// not a base SetHost refuses with its own sentence at step 2. Read from the
// environment block the loader hands the .preinit_array entry, not through
// getenv: the C library's environment pointer is not set yet that early
// (measured: os::GetEnv answered "unset" there with the variable set).
std::uint32_t BaseAsked(char** envp, bool* asked) {
    static const char kName[] = "BOF3X_IMAGEBASE=";
    const char* text = nullptr;
    for (char** e = envp; e && *e && !text; ++e)
        if (std::strncmp(*e, kName, sizeof kName - 1) == 0) text = *e + sizeof kName - 1;
    *asked = text != nullptr;
    if (!text || !*text) return kImageBase;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(text, &end, 0);
    if (!end || *end || v < 0x10000 || v % 0x10000 || v + kSpan > 0x1000000) return kImageBase;
    return static_cast<std::uint32_t>(v);
}

// Run by the dynamic loader before every initialiser of the program's and its
// libraries' (the ELF's DT_PREINIT_ARRAY; glibc's _dl_init calls it ahead of
// the libraries' constructors and the program's own): ImageBase() is set
// before any namespace-scope table that places an address is built, and the
// fixed ranges are made before anything else can map there. Only system
// calls and the variable's store: no allocation, no C++ library object.
void PreInit(int, char**, char** envp) {
    Reserved& r = g_reserved;
    r.base = BaseAsked(envp, &r.base_asked);
    SetImageBase(r.base);
    // Row 1: the span reserved, no access; .game and .pool committed in it.
    if (os::MemReserve(AtU(r.base), kSpan)) {
        r.image_errno = 0;
        // Row 1a: the header page read-only, holding what is read of
        // bof3x.exe's PE header: bind.cpp's Mode, at the first Code() of an
        // initialiser, tells the exe host from the hosted build by the
        // header's TimeDateStamp (e_lfanew at +0x3C, the stamp at e_lfanew +
        // 8), as Windows' loader maps it. The DOS and PE signatures and the
        // machine as make_exe.py writes them; nothing else of the header.
        if (os::MemCommit(AtU(r.base), 0x1000, os::Prot::ReadWrite)) {
            auto* h = static_cast<std::uint8_t*>(AtU(r.base));
            const std::uint32_t lfanew = 0x40, pe = 0x00004550, stamp = kTimestamp;
            const std::uint16_t mz = 0x5A4D, machine = 0x8664;
            std::memcpy(h, &mz, 2);
            std::memcpy(h + 0x3C, &lfanew, 4);
            std::memcpy(h + lfanew, &pe, 4);
            std::memcpy(h + lfanew + 4, &machine, 2);
            std::memcpy(h + lfanew + 8, &stamp, 4);
            r.header_errno = os::MemProtect(h, 0x1000, os::Prot::Read) ? 0 : errno;
        } else {
            r.header_errno = errno;
        }
        r.commit_errno = os::MemCommit(Place(kGameVa), kGameSize + kPoolSize, os::Prot::ReadWrite) ? 0 : errno;
        // Row 1d: the stamp DrawPool_Reserve looks for, as make_exe.py writes
        // it into the file's .pool on Windows.
        if (r.commit_errno == 0) std::memcpy(Place(kPoolVa), draw_pool_room::kStamp, sizeof draw_pool_room::kStamp);
    } else {
        r.image_errno = errno ? errno : EEXIST;
    }
    // Row 2: at a moved base, the linked span fenced.
    if (r.base != kImageBase) r.fence_errno = os::MemReserve(AtU(kImageBase), kSpan) ? 0 : (errno ? errno : EEXIST);
    // Row 3: the main thread's stack and its guard.
    if (os::MemAlloc(AtU(kStackLo - kGuard), kStackHi - kStackLo + kGuard, os::Prot::ReadWrite))
        r.stack_errno = os::MemProtect(AtU(kStackLo - kGuard), kGuard, os::Prot::None) ? 0 : errno;
    else
        r.stack_errno = errno ? errno : EEXIST;
}

__attribute__((section(".preinit_array"), used)) void (*g_preinit)(int, char**, char**) = &PreInit;

[[noreturn]] void RefuseRange(const char* what, std::uintptr_t lo, std::uintptr_t hi, int error) {
    const std::string held = os::MemHolders(lo, hi);
    Fatal("start-up: %s, 0x%llX..0x%llX, could not be mapped (errno %d, %s); what holds it now (/proc/self/maps):\n%s", what,
          static_cast<unsigned long long>(lo), static_cast<unsigned long long>(hi), error, std::strerror(error),
          held.empty() ? "(nothing: it was taken and given back, or the system refused it)" : held.c_str());
}

// --- 7. The switches that need Capcom's code (exe_host.cpp's) ---------------

void RefuseHostedSwitches() {
    if (EnvSet("BOF3X_ORIGINAL"))
        Fatal("BOF3X_ORIGINAL is set: the A/B switch runs Capcom's code, which bof3x does not map. "
              "The hosted build is Windows' alone (bof3x-launcher --host bof3 there).");
    if (EnvSet("BOF3X_SHADOW"))
        Fatal("BOF3X_SHADOW is set: the shadow fuzz clones Capcom's code, which bof3x does not map. "
              "The hosted build is Windows' alone (bof3x-launcher --host bof3 there).");
    char mp3[16] = {};
    if (os::GetEnv("BOF3X_MP3", mp3, sizeof mp3) && std::strcmp(mp3, "original") == 0)
        Fatal("BOF3X_MP3=original: Capcom's MP3 decoder is not mapped in bof3x (DIV-0091). "
              "The hosted build is Windows' alone (bof3x-launcher --host bof3 there).");
    if (EnvSet("BOF3X_MP3_COMPARE"))
        Fatal("BOF3X_MP3_COMPARE is set: the comparison runs Capcom's MP3 decoder, which bof3x does not map "
              "(DIV-0091). The hosted build is Windows' alone (bof3x-launcher --host bof3 there).");
    if (EnvSet("BOF3X_CALLTRACE"))
        Fatal("BOF3X_CALLTRACE is set: the call trace counts calls into Capcom's code, and bof3x maps none "
              "(.stub is the no-access trap). The hosted build is Windows' alone.");
}

// --- 8. The fence: the reservations reported --------------------------------

// The largest free run below 16 MB (exe_host.cpp LogLowFreeRun's measure).
void LogLowFreeRun() {
    std::uint32_t best = 0, best_at = 0;
    for (std::uint32_t a = 0x10000; a < 0x1000000;) {
        os::MemInfo m;
        if (!os::MemQuery(At(a), &m)) break;
        std::uintptr_t r_hi = m.base + m.size;
        if (r_hi > 0x1000000 || r_hi <= a) r_hi = 0x1000000;
        if (m.state == os::MemState::Free && r_hi - a > best) best = static_cast<std::uint32_t>(r_hi - a), best_at = a;
        a = static_cast<std::uint32_t>(r_hi);
    }
    Log("fence: the largest free run below 16 MB at main is 0x%X bytes at 0x%08X (the image takes 0x%X)", (unsigned)best,
        (unsigned)best_at, (unsigned)(kPoolVa + kPoolSize - kImageBase));
}

void Fence() {
    const Reserved& r = g_reserved;
    if (r.image_errno != 0) RefuseRange("the image's span (row 1)", ImageBase(), ImageBase() + kSpan, r.image_errno);
    if (r.header_errno != 0) RefuseRange("the image's header page (row 1a)", ImageBase(), ImageBase() + 0x1000, r.header_errno);
    if (r.commit_errno != 0)
        RefuseRange("the image's .game and .pool (rows 1c, 1d)", Img(kGameVa), Img(kPoolVa + kPoolSize), r.commit_errno);
    if (r.stack_errno != 0)
        RefuseRange("the main thread's stack at its Windows place (row 3)", kStackLo - kGuard, kStackHi, r.stack_errno);
    LogLowFreeRun();
    Log("fence: the image's span 0x%08X..0x%08X reserved by the start-up before any initialiser, .game and .pool "
        "read-write in it; the main thread's stack 0x%08X..0x%08X, its guard page under it (docs/linux-round.md, "
        "\"B1 as run\")",
        (unsigned)ImageBase(), (unsigned)(ImageBase() + kSpan), (unsigned)kStackLo, (unsigned)kStackHi);
    if (ImageBase() == kImageBase) {
        Log("fence: the image at its linked base 0x%08X; nothing to fence", (unsigned)kImageBase);
        return;
    }
    if (r.fence_errno != 0) RefuseRange("the linked span's fence (row 2)", kImageBase, kImageBase + kSpan, r.fence_errno);
    Log("fence: the image at 0x%08X, moved from 0x%08X: the linked span 0x%08X..0x%08X reserved no-access whole, "
        "before any initialiser",
        (unsigned)ImageBase(), (unsigned)kImageBase, (unsigned)kImageBase, (unsigned)(kImageBase + kSpan));
}

// --- 9. The layout: the start-up's own mapping read back ---------------------

// Each part of [va, va + size) as row 1 of the table made it: `writable`
// committed read-write and not executable, else reserved with no access. No
// MEM_IMAGE here: the mapping is anonymous and private.
void CheckSection(const char* name, std::uint32_t va, std::uint32_t size, bool writable) {
    for (std::uint32_t a = Img(va); a < Img(va) + size;) {
        os::MemInfo m;
        if (!os::MemQuery(At(a), &m)) Fatal("layout: MemQuery(0x%08X) failed, errno %u", (unsigned)a, os::LastError());
        const bool ok = writable ? m.state == os::MemState::Committed && m.readable && m.writable && !m.executable
                                 : m.state == os::MemState::Reserved;
        if (!ok || !m.is_private)
            Fatal("layout: %s at 0x%08X (VA 0x%08X) is not the start-up's %s mapping (state %u, r%u w%u x%u, private %u)",
                  name, (unsigned)a, (unsigned)(a - Img(va) + va), writable ? "read-write" : "no-access",
                  (unsigned)m.state, m.readable, m.writable, m.executable, m.is_private);
        const std::uintptr_t next = m.base + m.size;
        if (next <= a) Fatal("layout: MemQuery(0x%08X) answered an empty run", (unsigned)a);
        a = next > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<std::uint32_t>(next);
    }
}

void CheckLayout() {
    CheckSection(".stub", kStubVa, kStubSize, false);
    CheckSection(".game", kGameVa, kGameSize, true);
    CheckSection(".pool", kPoolVa, kPoolSize, true);
    Log("layout: .stub 0x%08X +0x%X no access, .game 0x%08X +0x%X RW, .pool 0x%08X +0x%X RW, as exe_layout.h (VAs; the "
        "image at 0x%08X, an anonymous mapping of the start-up's)",
        (unsigned)kStubVa, (unsigned)kStubSize, (unsigned)kGameVa, (unsigned)kGameSize, (unsigned)kPoolVa,
        (unsigned)kPoolSize, (unsigned)ImageBase());
}

// --- 10. The trap -----------------------------------------------------------

// .stub was never made accessible on this system (row 1b); this is
// exe_host.cpp's TrapStub all the same - the protection set and read back -
// so the trap does not rest on the mapping's first state alone.
void TrapStub() {
    if (!os::MemProtect(Place(kStubVa), kStubSize, os::Prot::None))
        Fatal("stub: MemProtect(.stub, no access) failed, errno %u", os::LastError());
    for (std::uint32_t a = Img(kStubVa); a < Img(kStubVa) + kStubSize;) {
        os::MemInfo m;
        if (!os::MemQuery(At(a), &m)) Fatal("stub: MemQuery(0x%08X) failed, errno %u", (unsigned)a, os::LastError());
        if (m.readable || m.writable || m.executable) Fatal("stub: 0x%08X is still accessible after the change", (unsigned)a);
        a = static_cast<std::uint32_t>(m.base + m.size);
    }
    Log("stub: .stub 0x%08X..0x%08X (VA 0x%08X..0x%08X) is no-access: the trap for a code value that is not bound",
        (unsigned)Img(kStubVa), (unsigned)Img(kStubVa + kStubSize), (unsigned)kStubVa, (unsigned)(kStubVa + kStubSize));
}

// --- 11. The data (exe_host.cpp's OpenExe and CopyData) ----------------------

void OpenExe() {
    if (!bof3x::InstallExeFileSwitch()) {
        if (!EnvSet("BOF3X_EXEIMAGE"))
            Fatal("data: BOF3X_EXEIMAGE unset. .data comes from the cache's image that BOF3X_EXEIMAGE names "
                  "(bof3x-launcher sets it); BOF3X_EXEFILE=1 reads the player's own BOF3.exe in the game's directory "
                  "instead, a developer's run.");
        Log("data: .data from BOF3X_EXEIMAGE's cache; BOF3.exe is not opened (BOF3X_EXEFILE=1 opens it), and the "
            "checks against it are not made");
        return;
    }
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::current_path(ec);
    if (ec) Fatal("data: the current directory is unusable (%s)", ec.message().c_str());
    const std::filesystem::path path = dir / "BOF3.exe";
    const std::string shown = path.string();
    if (!std::filesystem::is_regular_file(path, ec)) {
        if (!EnvSet("BOF3X_EXEIMAGE"))
            Fatal("data: BOF3X_EXEFILE=1, no BOF3.exe in %s and BOF3X_EXEIMAGE unset. bof3x runs with the game's "
                  "directory as its current directory (bof3x-launcher starts it so); .data comes from the player's own "
                  "BOF3.exe there, or from the cache BOF3X_EXEIMAGE names.", dir.string().c_str());
        Log("data: BOF3X_EXEFILE=1 but no BOF3.exe in %s: .data from BOF3X_EXEIMAGE's cache, nothing read from "
            "the file, and the checks against it skipped", dir.string().c_str());
        return;
    }

    // The hash before any byte of the file is used.
    std::string sha, why;
    if (!bof3x::Sha256Hex(path.wstring().c_str(), sha, why)) Fatal("data: cannot hash %s: %s", shown.c_str(), why.c_str());
    if (sha != image::kSha256)
        Fatal("data: %s is not the build this project targets (sha256 %s, expected %s). Every address is specific to "
              "that one file; refusing to start.", shown.c_str(), sha.c_str(), image::kSha256);
    if (!exe_file::Open(path)) Fatal("data: cannot open %s, errno %u", shown.c_str(), os::LastError());
    Log("data: %s, sha256 %s (the build symbols.toml describes)", shown.c_str(), sha.c_str());

    exe_file::ReadSections();
    const exe_file::Section& g_text = exe_file::Text();
    const exe_file::Section& g_rdata = exe_file::Rdata();
    const exe_file::Section& g_data = exe_file::Data();
    if (g_text.va != kStubVa || g_text.raw_size > kStubSize || g_rdata.va != kRdataVa || g_rdata.raw_size != kRdataBytes ||
        g_data.va != kDataVa || g_data.raw_size != kDataBytes || g_data.va + g_data.vsize != kBssEnd)
        Fatal("data: BOF3.exe's sections (.text 0x%X +0x%X, .rdata 0x%X +0x%X, .data 0x%X +0x%X to 0x%X) are not the "
              "ones exe_layout.h was written for", (unsigned)g_text.va, (unsigned)g_text.raw_size, (unsigned)g_rdata.va,
              (unsigned)g_rdata.raw_size, (unsigned)g_data.va, (unsigned)g_data.raw_size, (unsigned)(g_data.va + g_data.vsize));
}

void CopyData() {
    OpenExe();
    const auto* game = static_cast<const std::uint8_t*>(Place(kGameVa));
    for (std::uint32_t i = 0; i < kGameSize; ++i)
        if (game[i]) Fatal("data: .game is not zero at VA 0x%08X before the copy", (unsigned)(kGameVa + i));
    rdata::Map();
    const bool from_cache = ExeImage_Source(Place(kDataVa), kDataVa, kDataBytes, kBssEnd);
    if (!from_cache) {   // only under BOF3X_EXEFILE=1 with the file opened (OpenExe)
        if (!exe_file::IsOpen()) Fatal("data: no .data source: neither BOF3X_EXEIMAGE's image nor an opened BOF3.exe");
        exe_file::ReadAt(exe_file::Data().raw, Place(kDataVa), kDataBytes, ".data");
    }
    Shell_MapDefaultKeys();
    ExeImage_LayOptTables(Place(kDataVa), kDataVa, kDataBytes);   // DIV-0111: a BOF3X_OPT layer's part
    ExeImage_LogMapped(Place(kDataVa), kDataBytes);
    ExeImage_CompareWithFile();
    for (std::uint32_t a = kDataVa + kDataBytes; a < kGameVa + kGameSize; ++a)
        if (*static_cast<const std::uint8_t*>(Place(a))) Fatal("data: .bss is not zero at VA 0x%08X after the copy", (unsigned)a);
    Log("data: .rdata 0x%08X + 0x%X bytes the engine's (none from the file), .data 0x%08X + 0x%X bytes from %s; .bss "
        "zero 0x%08X..0x%08X (virtual end 0x%08X)",
        (unsigned)kRdataVa, (unsigned)kRdataBytes, (unsigned)kDataVa, (unsigned)kDataBytes,
        from_cache ? "BOF3X_EXEIMAGE's data.bin" : "the file", (unsigned)(kDataVa + kDataBytes),
        (unsigned)(kGameVa + kGameSize), (unsigned)kBssEnd);
}

// --- 12. The rebase (exe_host.cpp's, unchanged) ------------------------------

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
        "their linked ranges; the data words moved by 0x%X (the image at 0x%08X); the code words bound, %u to their places "
        "(a jmp to our function each) and %u to the trap (no function of ours: design 4.2)",
        rebase::kRunCount, words[rebase::kData], words[rebase::kBss], words[rebase::kRdata], words[rebase::kText],
        rebase::kNotMoved, (unsigned)move, (unsigned)ImageBase(), bound, trapped);
}

// --- 13. The import slots: a function of ours for each -----------------------

// What src/ calls through BOF3.exe's import slots (exe_host.cpp kSlots): no
// system library of this one holds any of them, so each slot gets a function
// of ours with the Windows function's meaning on this system. Declared
// as their callers' pointer types are (on x86-64 the system's one
// convention either way). Each slot holds a 14-byte thunk's place below 4 GB
// (Bind_ImportThunk), as on 64-bit Windows.

constexpr std::uint32_t kWaitObject0 = 0, kWaitTimeout = 0x102, kWaitFailed = 0xFFFFFFFFu, kInfinite = 0xFFFFFFFFu;
constexpr int kDriveNoRootDir = 1;   // DRIVE_NO_ROOT_DIR
constexpr int kIdOk = 1;             // IDOK
constexpr unsigned kMbTypeMask = 0xF, kMbIconMask = 0xF0, kMbIconHand = 0x10;
const std::uintptr_t kCurrentProcess = ~static_cast<std::uintptr_t>(0);   // GetCurrentProcess()'s pseudo handle

// DSOUND #1, DirectSoundCreate: SoundSdl_Start (step 14) puts our Create in
// this slot straight after, on this build whatever BOF3X_OFF says
// (sound_sdl.cpp: the 64-bit build has no DirectSound path). Reached, this
// would be a call no system can answer.
long SlotDirectSoundCreate(const void*, void**, void*) {
    Fatal("DirectSoundCreate was called through its import slot 0x5C4014, which SoundSdl_Start binds to SDL3's output: "
          "this system has no DirectSound");
}

// GDI32's three: Display_TextOut's (d3d.cpp), on a device context the back
// buffer's GetDC hands out - which the renderer's surfaces refuse
// (render_shim.cpp, slot 17 of a surface: a Fatal), so no device context
// reaches these. This system has no GDI to draw with.
[[noreturn]] void NoGdi(const char* name, std::uint32_t slot) {
    Fatal("GDI32's %s was called through its import slot 0x%08X, but this system has no GDI and the renderer hands out "
          "no device context (render_shim.cpp's GetDC refuses)", name, (unsigned)slot);
}
std::uint32_t SlotSetTextColor(void*, std::uint32_t) { NoGdi("SetTextColor", 0x5C4020); }
int SlotSetBkMode(void*, int) { NoGdi("SetBkMode", 0x5C4024); }
int SlotTextOutA(void*, int, int, const char*, int) { NoGdi("TextOutA", 0x5C4028); }

// GetTickCount: milliseconds since the system started, its low 32 bits
// (os::TickMs32, the monotonic clock; DIV-0022's GameClock_Inject puts the
// game's own clock over the slot after this, as on Windows).
std::uint32_t SlotGetTickCount() { return os::TickMs32(); }

// CreateEventA, CloseHandle, MsgWaitForMultipleObjects: the music's two
// position notifications (music.cpp, sound_callees.h). The handle is an
// os::Event, the object sound_sdl.cpp's mixer sets (os/event.h).
void* SlotCreateEventA(void* attributes, int manual, int initial, const char* name) {
    if (attributes || name)
        Fatal("CreateEventA through its import slot with %s: only an unnamed event with no attributes is served here",
              name ? "a name" : "security attributes");
    return os::EventCreate(manual != 0, initial != 0);
}

int SlotCloseHandle(void* handle) {
    if (!handle) return 0;
    os::EventClose(handle);
    return 1;
}

// Any of `count` events, in index order as Windows answers: the first set
// one is taken and its index returned; none within `ms`, WAIT_TIMEOUT. No
// thread message queue exists here, so the "input in the queue" answer
// (WAIT_OBJECT_0 + count) never comes: the window's events are SDL's,
// pumped by the frame loop. Waiting for all at once is not served.
unsigned SlotMsgWaitForMultipleObjects(unsigned count, void* const* handles, int all, unsigned ms, unsigned) {
    if (all) Fatal("MsgWaitForMultipleObjects through its import slot with bWaitAll: not served here");
    if (count == 0 || count > 64 || !handles) return kWaitFailed;
    const std::uint64_t deadline = os::TickMs64() + ms;
    for (;;) {
        for (unsigned i = 0; i < count; ++i) {
            const os::WaitResult w = os::EventWait(handles[i], 0);
            if (w == os::WaitResult::Signalled) return kWaitObject0 + i;
            if (w == os::WaitResult::Failed) return kWaitFailed;
        }
        if (ms == 0 || (ms != kInfinite && os::TickMs64() >= deadline)) return kWaitTimeout;
        // A millisecond's sleep on the first: set meanwhile, it is the lowest
        // index set, which is what Windows answers too.
        const os::WaitResult w = os::EventWait(handles[0], 1);
        if (w == os::WaitResult::Signalled) return kWaitObject0;
        if (w == os::WaitResult::Failed) return kWaitFailed;
    }
}

// GetDriveTypeA: Disc_Probe (file.cpp) asks it of "C:\" .. "L:\" when the
// local movie does not open. This system has no drive letters: every such
// root is DRIVE_NO_ROOT_DIR, so no drive is a CD-ROM drive.
unsigned SlotGetDriveTypeA(const char*) { return kDriveNoRootDir; }

// ExitProcess: the log closed and the process ended with the code, the other
// threads not waited for (Windows ends them before the detach).
void SlotExitProcess(std::uint32_t code) {
    Log("exit: ExitProcess(%u) through its import slot", (unsigned)code);
    LogClose();
    std::fflush(nullptr);
    std::_Exit(static_cast<int>(code));
}

// TerminateProcess: of this process (GetCurrentProcess()'s pseudo handle),
// at once with the code; no other process's handle exists here.
int SlotTerminateProcess(void* process, std::uint32_t code) {
    if (reinterpret_cast<std::uintptr_t>(process) != kCurrentProcess)
        Fatal("TerminateProcess through its import slot of a process that is not this one (%p)", process);
    LogClose();
    std::_Exit(static_cast<int>(code));
}

// PostQuitMessage: the thread's quit message, put in the window's queue
// (game_window.cpp Pump hands Got::Quit to the loop as WM_QUIT).
void SlotPostQuitMessage(int code) { game_window::Post({game_window::Got::Quit, code, 0}); }

// IntersectRect: USER32's, exactly - right and bottom exclusive, an empty
// rectangle intersects nothing, and no intersection empties the result.
struct Rect32 {
    std::int32_t left, top, right, bottom;
};
int SlotIntersectRect(Rect32* out, const Rect32* a, const Rect32* b) {
    const Rect32 r{a->left > b->left ? a->left : b->left, a->top > b->top ? a->top : b->top,
                   a->right < b->right ? a->right : b->right, a->bottom < b->bottom ? a->bottom : b->bottom};
    if (r.left >= r.right || r.top >= r.bottom) {
        *out = {0, 0, 0, 0};
        return 0;
    }
    *out = r;
    return 1;
}

// MessageBoxA: SDL3's message box, over the game's window when it is one,
// its one button OK; IDOK when read, 0 (MessageBoxA's failure) when no box
// could be shown. The game asks for MB_OK boxes alone; another button set
// has no answer here to give.
int SlotMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned type) {
    if (type & kMbTypeMask) Fatal("MessageBoxA through its import slot with buttons 0x%X: only MB_OK is served here", type & kMbTypeMask);
    Log("MessageBoxA: \"%s\": %s", caption ? caption : "", text ? text : "");
    SDL_Window* parent = hwnd ? SDL_GetWindowFromID(static_cast<SDL_WindowID>(reinterpret_cast<std::uintptr_t>(hwnd))) : nullptr;
    const SDL_MessageBoxFlags flags = (type & kMbIconMask) == kMbIconHand ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION;
    if (!SDL_ShowSimpleMessageBox(flags, caption ? caption : "", text ? text : "", parent)) {
        Log("MessageBoxA: SDL_ShowSimpleMessageBox failed: %s", SDL_GetError());
        return 0;
    }
    return kIdOk;
}

struct Slot {
    std::uint32_t at;
    const char* dll;
    const char* name;
    const void* ours;
};
const Slot kSlots[] = {
    {0x5C4014, "DSOUND.dll", "#1 DirectSoundCreate", reinterpret_cast<const void*>(&SlotDirectSoundCreate)},
    {0x5C4020, "GDI32.dll", "SetTextColor", reinterpret_cast<const void*>(&SlotSetTextColor)},
    {0x5C4024, "GDI32.dll", "SetBkMode", reinterpret_cast<const void*>(&SlotSetBkMode)},
    {0x5C4028, "GDI32.dll", "TextOutA", reinterpret_cast<const void*>(&SlotTextOutA)},
    {0x5C407C, "KERNEL32.dll", "GetTickCount", reinterpret_cast<const void*>(&SlotGetTickCount)},
    {0x5C4080, "KERNEL32.dll", "CreateEventA", reinterpret_cast<const void*>(&SlotCreateEventA)},
    {0x5C4084, "KERNEL32.dll", "CloseHandle", reinterpret_cast<const void*>(&SlotCloseHandle)},
    {0x5C4088, "KERNEL32.dll", "GetDriveTypeA", reinterpret_cast<const void*>(&SlotGetDriveTypeA)},
    {0x5C40B8, "KERNEL32.dll", "ExitProcess", reinterpret_cast<const void*>(&SlotExitProcess)},
    {0x5C4114, "KERNEL32.dll", "TerminateProcess", reinterpret_cast<const void*>(&SlotTerminateProcess)},
    {0x5C4158, "USER32.dll", "PostQuitMessage", reinterpret_cast<const void*>(&SlotPostQuitMessage)},
    {0x5C4164, "USER32.dll", "MsgWaitForMultipleObjects", reinterpret_cast<const void*>(&SlotMsgWaitForMultipleObjects)},
    {0x5C4168, "USER32.dll", "IntersectRect", reinterpret_cast<const void*>(&SlotIntersectRect)},
    {0x5C4174, "USER32.dll", "MessageBoxA", reinterpret_cast<const void*>(&SlotMessageBoxA)},
};
constexpr unsigned kSlotCount = sizeof kSlots / sizeof kSlots[0];

// exe_host.cpp's 64-bit BindSlots with ours for Windows': every slot word the
// thunk's place, every thunk's target the function, and GetTickCount called
// through the slot's word.
void BindSlots() {
    std::uint32_t thunk[kSlotCount];
    for (unsigned i = 0; i < kSlotCount; ++i) {
        thunk[i] = Bind_ImportThunk(i, kSlots[i].ours);
        std::memcpy(Place(kSlots[i].at), &thunk[i], 4);
    }
    for (unsigned i = 0; i < kSlotCount; ++i) {
        const Slot& s = kSlots[i];
        std::uint32_t now = 0;
        std::memcpy(&now, Place(s.at), 4);
        std::uint64_t target = 0;
        std::memcpy(&target, static_cast<const std::uint8_t*>(At(now)) + 6, sizeof target);
        if (now != thunk[i] || target != reinterpret_cast<std::uintptr_t>(s.ours))
            Fatal("slots: 0x%08X does not read back as the thunk 0x%08X of our %s!%s", (unsigned)s.at, (unsigned)thunk[i], s.dll,
                  s.name);
    }
    std::uint32_t tick_word = 0;
    std::memcpy(&tick_word, Place(Imp_GetTickCount_addr), 4);
    const std::uint32_t before = os::TickMs32();
    const std::uint32_t through = reinterpret_cast<std::uint32_t(*)()>(static_cast<std::uintptr_t>(tick_word))();
    const std::uint32_t after = os::TickMs32();
    if (through < before || through > after)
        Fatal("slots: GetTickCount through 0x5C407C's thunk answered %u, outside %u..%u", through, before, after);
    Log("slots: %u import slots bound to 14-byte thunks at 0x%08X.. of functions of ours (no system library holds "
        "DSOUND #1, GDI32 3, KERNEL32 6, USER32 4 here; exe_host_posix.cpp says what each does) and read back; "
        "GetTickCount through 0x5C407C answers %u",
        kSlotCount, (unsigned)thunk[0], through);
}

// --- 15. The x87 control word (exe_host.cpp's) -------------------------------

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

// --- 4. The hand-off to the launcher (exe_host.cpp's HandOffToLauncher) ------

void HandOffToLauncher() {
    if (EnvSet("BOF3X_LAUNCHER") || EnvSet("BOF3X_HANDOFF")) return;
    std::string self;
    if (!os::ExecutablePath(&self)) return;
    const std::size_t slash = self.rfind('/');
    if (slash == std::string::npos) return;
    const std::string dir = self.substr(0, slash + 1);
    const std::wstring launcher = os::ToWide((dir + "bof3x-launcher").c_str());
    if (!os::IsFileW(launcher)) return;
    os::SetEnv("BOF3X_HANDOFF", "1");
    os::ChildSpec spec;
    spec.program = launcher;
    spec.args = {L"--no-config"};
    spec.directory = os::ToWide(dir.c_str());
    os::Child child;
    std::uint32_t error = 0;
    if (!os::ChildStart(spec, &child, &error))
        Fatal("bof3x was not started by bof3x-launcher, and starting %sbof3x-launcher failed (errno %u)", dir.c_str(), error);
    const std::uint32_t pid = child.pid;
    os::ChildClose(child);
    Log("Engine_Main: not started by bof3x-launcher: handed over to %sbof3x-launcher --no-config (pid %u); ending",
        dir.c_str(), pid);
    LogClose();
    std::_Exit(0);
}

// --- 24. The command line (exe_host.cpp's PastProgramName) -------------------

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

// The line Windows' GetCommandLineA would give: the program's name quoted,
// then each argument, quoted when it holds a space or a tab (an argument's own
// quote is not escaped: none of ours carries one).
std::string CommandLine(int argc, char** argv) {
    std::string line = "\"" + std::string(argc > 0 && argv[0] ? argv[0] : "bof3x") + "\"";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i] ? argv[i] : "";
        line += ' ';
        line += a.find_first_of(" \t") == std::string::npos && !a.empty() ? a : "\"" + a + "\"";
    }
    return line;
}

// --- 3 to 25: Engine_Main's body, on the placed stack ------------------------

struct Run {
    void* module;           // step 5: the ELF's own module
    std::string line;       // step 24's input
    int code;               // step 25's answer
};

constexpr int kShowDefault = 10;   // SW_SHOWDEFAULT: what GetStartupInfoA leaves for a start with no wShowWindow

void EngineMain(void* p) {
    Run& run = *static_cast<Run*>(p);
    // 3. Entered once, in the exe host.
    static bool entered = false;
    if (entered) Fatal("Engine_Main entered twice");
    entered = true;
    if (!IsExeHost()) Fatal("Engine_Main in the hosted build: only bof3x's start-up calls it");
    // 4. The hand-off to the launcher beside us.
    HandOffToLauncher();
    // 5. The module: the ELF's own (one image, no DLL), found in step 2.
    void* const module = run.module;
    // 1 (the hinstance): Capcom's WinMain received the image's base, which
    // the game keeps in a 32-bit word (Game_HInstance); the image is ours to
    // place here, so its base stands in for GetModuleHandle(nullptr).
    void* const hinstance = At(ImageBase());
    const int show = kShowDefault;
    // 6. The thread, and the narrow encoding in place of the code page line.
    const os::StackRange stack = os::CurrentStack();
    Log("Engine_Main: hinstance %p, show %d, on thread %u, its stack 0x%llX..0x%llX (the placed stack, row 3)", hinstance,
        show, os::ThreadId(), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(stack.limit)),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(stack.base)));
    Log("code page: none on this system; the narrow encoding is UTF-8 (os/file.h)");

    RefuseHostedSwitches();   // 7
    Fence();                  // 8
    CheckLayout();            // 9
    TrapStub();               // 10
    CopyData();               // 11
    Rebase();                 // 12
    BindSlots();              // 13
    SoundSdl_Start();         // 14: DIV-0201, the DirectSoundCreate slot 0x5C4014 to our Create
    SetX87();                 // 15

    Crash_Start(module);      // 16
    // 17. The unattended run's marker for tools/recipe_saves.py: a named
    // event bof3x_unattended_<pid> (os/named_event.h; on this system a named
    // semaphore under /dev/shm, which stays after the process until unlinked).
    if (EnvSet("BOF3X_INPUT") || EnvSet("BOF3X_SELFTEST_ONLY")) {
        char name[64];
        std::snprintf(name, sizeof name, "bof3x_unattended_%u", os::ProcessId());
        if (!os::CreateNamedEvent(name)) Log("unattended: the marker %s could not be made (errno %u)", name, os::LastError());
    }
    StateHash_SetImageBase(ImageBase());   // 18
    StateHash_BeforeInjects(module);
    InjectAll();
    StateHash_AfterInjects();
    StackWatch_Start();       // 19: stackwatch_posix.cpp refuses BOF3X_STACKWATCH=1 (the owner's call 2, wave B)
    if (EnvSet("BOF3X_SELFTEST_ONLY")) {   // 20
        ExeIcon_SelfTest();   // DIV-0095: only with BOF3X_ICON_VRAM (exe_icon_sdl.cpp)
        Log("self-test only: done, ending the process");
        LogClose();
        std::fflush(nullptr);
        std::_Exit(0);
    }
    if (EnvSet("BOF3X_ICON_VRAM")) Log("DIV-0095    icon: BOF3X_ICON_VRAM is a self-test switch (BOF3X_SELFTEST_ONLY); ignored");   // 21
    CallTrace_Start(module);   // 22: refused above when asked for; windows_only.cpp's otherwise
    WavDump_Start();
    InputScript_Start();
    GapWatch_Start(module);
    draw_order::Start();
    write_watch::Start();
    stp_census::Start();
    // 23. An exception nobody handled ends the process as Fatal does, code 3,
    // as Windows' filter ends it: crash_posix.cpp's handler, armed at step 16,
    // writes the CRASH lines, then its reporter calls Fatal. Before this step
    // a fault ends by its signal, as one before the filter ends by its
    // exception on Windows.
    Crash_EndUnhandledAsFatal();
    char* const line = PastProgramName(run.line.c_str());   // 24
    Log("Engine_Main: Game_WinMain(0x%p, null, \"%s\", %d) on the main thread", hinstance, line, show);
    run.code = ::Game_WinMain(hinstance, nullptr, line, show);   // 25
    GapWatch_Flush();
    Log("Engine_Main: Game_WinMain returned %d, the exit code", run.code);
}

// An address inside this ELF, for its module (os::ModuleImage).
void Anchor() {}

}  // namespace
}  // namespace bof3

int main(int argc, char** argv) {
    using namespace bof3;
    // 2. What DllMain does: the log, then the host. The module is the ELF
    // itself; its base is image::g_ownBase (crt_rest.cpp's messages).
    os::ModuleImageInfo self;
    if (!os::ModuleImage(reinterpret_cast<const void*>(&Anchor), &self)) {
        std::fprintf(stderr, "bof3x: the program's own image is not found (dl_iterate_phdr)\n");
        return 3;
    }
    image::g_ownBase = self.base;
    void* const module = reinterpret_cast<void*>(self.base);
    LogOpen(module);
    Log("bof3x started as process %u (the ELF at 0x%llX..0x%llX)", os::ProcessId(), static_cast<unsigned long long>(self.base),
        static_cast<unsigned long long>(self.base + self.extent));
    // Step 1's proof: ImageBase() is a Fatal when it is read unset
    // (image_base.h), and every namespace-scope table that places an address
    // ran before main; reaching this line means each read the base the
    // .preinit_array entry set.
    Log("start-up: ImageBase() 0x%X was set by the .preinit_array entry, before every initialiser (BOF3X_IMAGEBASE %s)",
        (unsigned)ImageBase(), g_reserved.base_asked ? "set" : "unset");
    SetHost(HostFound{HostKind::Exe, g_reserved.base, kTimestamp, kSpan});
    // Rows 1 and 3 must be there before the switch onto the stack (row 3) and
    // before step 8 reads them: a refusal here names the range.
    if (g_reserved.stack_errno != 0)
        RefuseRange("the main thread's stack at its Windows place (row 3)", kStackLo - kGuard, kStackHi, g_reserved.stack_errno);

    Run run{module, CommandLine(argc, argv), 0};
    if (!os::RunOnPlacedStack(At(kStackLo), At(kStackHi), &EngineMain, &run))
        Fatal("start-up: the switch onto the main thread's placed stack 0x%08X..0x%08X failed, errno %u", (unsigned)kStackLo,
              (unsigned)kStackHi, os::LastError());
    // 25's end: as ExitProcess - the log closed, the process ended with the
    // code, the other threads (SDL's, DXVK's, the log writer) not joined and
    // no static destructor run under them.
    LogClose();
    std::fflush(nullptr);
    std::_Exit(run.code);
}

#endif  // !_WIN32
