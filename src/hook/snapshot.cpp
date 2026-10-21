// The snapshot core (snapshot.h; docs/quicksave.md). The quicksave round's
// group SN: the point, the span, the task arena, the process-owned words,
// the participant registry, the file BISLPS1X.DAT, F6's rotation and F7's
// load.
//
// DIVERGENCE (ledger draft A, docs/quicksave.md section 6): F6 and F7 are
// quicksave keys. The original's F7 was the next renderer device (nothing
// since DIV-0040), its F6 nothing.
#include "hook/snapshot.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "common/sha256.h"
#include "game/draw_pool.h"
#include "game/crt_rest.h"
#include "game/draw_pool_room.h"
#include "game/msg_pool.h"
#include "hook/detour.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"
#include "input/sigma.h"
#include "version.gen.h"

namespace bof3 {

void SnapshotWriter::Put(const void* data, std::size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    out_.insert(out_.end(), p, p + n);
}

void SnapshotReader::Skip(std::size_t n) {
    if (n > left_)
        Fatal("snapshot: participant '%s' skipped %u bytes where its record has %u left", tag_, static_cast<unsigned>(n),
              static_cast<unsigned>(left_));
    p_ += n;
    left_ -= n;
}

void SnapshotReader::Get(void* to, std::size_t n) {
    if (n > left_)
        Fatal("snapshot: participant '%s' read %u bytes where its record has %u left", tag_, static_cast<unsigned>(n),
              static_cast<unsigned>(left_));
    std::memcpy(to, p_, n);
    p_ += n;
    left_ -= n;
}

namespace {

using U = std::uint32_t;

// The save half: Save_WriteFile's 0x12B0 bytes, the summary at 0xCA0 where
// Save_ReadSummaries reads its 0x1C bytes (docs/save-files.md 1 and 2).
constexpr U kSaveHalf = 0x12B0;
constexpr U kSummaryAt = 0xCA0;
constexpr U kSummaryBytes = 0x1C;

// The task arena: four slices of 0x4000 below Task_StackTop (task_sched.cpp,
// Task_Create: slot k's stack top is Task_StackTop - k * 0x4000).
constexpr U kArenaBytes = 4 * 0x4000;
constexpr U kPage = 0x1000;

constexpr unsigned kFirstSlot = 0x10, kLastSlot = 0x1F;
constexpr int kOverlayFrames = 0x78;   // F12's (win_main.cpp)

constexpr char kMagic[8] = {'B', 'O', 'F', '3', 'X', 'Q', 'S', '\0'};
constexpr U kFormat = 1;
constexpr U kHostBof3 = 0, kHostExe = 1;
constexpr std::size_t kTagBytes = 16;
constexpr std::size_t kMaxFile = 64u << 20;

#pragma pack(push, 1)
// After the save half. Little-endian, as the machine is.
struct Header {
    char magic[8];         // "BOF3XQS\0"
    U format;              // kFormat
    U header_bytes;        // sizeof(Header)
    U file_bytes;          // the whole file, save half included
    U host;                // kHostBof3 (bof3x.dll in BOF3.exe) or kHostExe (bof3x.exe)
    char tag[32];          // BOF3X_VERSION_TAG, NUL-padded
    char dll_sha256[64];   // bof3x.dll's file, lowercase hex
    U dll_base;            // where bof3x.dll was loaded: our code, which the task stacks' return addresses name
    U frame_counter;       // Frame_Counter at the take
    U span_base, span_bytes;     // the image's span
    U pool_base, pool_bytes;     // the hosted build's draw pool (DIV-0062); 0, 0 when inside the span
    U arena_top, arena_bytes;    // the task stacks: [arena_top - arena_bytes, arena_top)
    U rdata_fnv;           // FNV-1a of .rdata at the take: not copied, compared at a load (logged)
    U participants;        // records after the arena
};
struct RecordHead {
    char tag[kTagBytes];
    U bytes;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 8 + 4 * 4 + 32 + 64 + 4 * 10, "the header's layout is the file's");

struct Participant {
    std::string tag;
    SnapshotSaveFn save;
    SnapshotRestoreFn restore;
    SnapshotDiscardFn discard = nullptr;   // before the span is written (SP-A)
    SnapshotCheckFn check = nullptr;       // while the file is checked, before anything is written
};
using Registry = std::vector<Participant>;

Registry g_registry;
bool g_registry_closed = false;   // set by the first take or restore

struct Range {
    U base = 0, bytes = 0;
};
struct Span {
    Range image, pool;
};

struct BuildId {
    U host = 0;
    char tag[32] = {};
    char sha[64] = {};
    U dll_base = 0;
};

// The keep list: bytes of the span that belong to the process, not the game,
// read live before the span is written and put back after it
// (docs/quicksave-round.md 2, SN; docs/quicksave-census.md 5).
// Task_SchedulerEsp is Task_RunAll's own esp, rewritten at its every entry;
// the window and instance handles are this process's; Frame_Deadline is the
// original loop's float, unwritten by ours since DIV-0047 (the loop's
// deadline is its own local), kept as the loop has it. The settings the
// player changes in play (F8's mode, the FPS line, the key table the
// launcher writes), the input objects, the font's heap block and the
// display's objects, window rectangle and CLUT row pointers are the live
// process's; what the display caches hold is SP-G's to clear and reconvert.
struct Kept {
    U at;
    U bytes;
    const char* name;
};
// symbols.toml's addresses; the names are macros of symbols.gen.h, so the
// literals are written here and checked against them by Snapshot_Inject.
constexpr Kept kKept[] = {
    {0x66C854, 4, "Task_SchedulerEsp"},
    {0x6BC620, 4, "Game_Hwnd"},
    {0x6BC624, 4, "Game_HInstance"},
    {0x6BC628, 4, "Frame_Deadline"},
    {0x6BC634, 4, "Fps_Shown"},
    {0x6BC63B, 1, "App_Active"},
    {0x65DA44, 4, "Cfg_Fullscreen"},
    {0x65DA48, 4, "Cfg_RenderMode"},
    {0x7DE7A8, 128, "Key_Table"},
    {0x7DE928, 952, "DInput_Object"},      // and the device records, to Gpu_DispEnv
    {0x7CC35C, 4, "Font_GlyphData"},
    {0x6BE1D0, 2096, "Gfx_WindowRect"},    // and the mode list, to Dd_CellBackdrop
    {0x6C2A40, 0x1000, "Gfx_ClutRows"},
    {0x7CC238, 0xFC, "D3d_DeviceDesc"},
    {0x7CC334, 4, "Dd_DirectDraw"},
    {0x7CC344, 4, "Dd_StageSurface"},
    {0x7CC350, 4, "D3d_Device"},
};
constexpr std::size_t kKeptCount = sizeof kKept / sizeof kKept[0];

unsigned char* At(U va) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(va)); }
U Va(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

std::string Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string Printf(const char* fmt, ...) {
    char text[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    return text;
}

// --- the span, per host ------------------------------------------------------

// .rdata (0x5C4000..0x5DA000) is not copied: the exe host's start-up writes
// it whole (constants, the bound import slots, one pointer into our DLL) and
// no game code is known to write it after (docs/quicksave-census.md 5); a
// take records its hash and a load logs whether it still matches.
constexpr Range kRdata{exe_layout::kRdataVa, exe_layout::kDataVa - exe_layout::kRdataVa};

U Fnv(const Range& r) {
    U h = 0x811C9DC5u;
    for (U i = 0; i < r.bytes; ++i) h = (h ^ At(r.base)[i]) * 0x01000193u;
    return h;
}

Span CurrentSpan() {
    // The exe host: .data and .bss to .game's end, then .pool, one run.
    if (IsExeHost()) return {{exe_layout::kDataVa, exe_layout::kOwnVa - exe_layout::kDataVa}, {}};
    // Hosted: BOF3.exe's .data to its last page (.rsrc follows at 0x93E000,
    // constant), and the draw pool where DrawPool_Reserve put it.
    const U end = (exe_layout::kBssEnd + kPage - 1) & ~(kPage - 1);
    const Range image{exe_layout::kDataVa, end - exe_layout::kDataVa};
    const U items = Va(draw_pool::Items());
    if (items >= image.base && items < image.base + image.bytes) return {image, {}};   // DIV-0062 off: Capcom's array
    return {image, {items, draw_pool_room::kBytes}};
}

bool Writable(DWORD protect) {
    return (protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
bool Readable(DWORD protect) {
    return !(protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
           (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                       PAGE_EXECUTE_WRITECOPY)) != 0;
}

// Every page of [base, base + bytes) committed and readable.
bool Committed(U base, U bytes) {
    for (U at = base; at < base + bytes;) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(At(at), &mbi, sizeof mbi) == 0) return false;
        if (mbi.State != MEM_COMMIT || !Readable(mbi.Protect)) return false;
        at = Va(mbi.BaseAddress) + static_cast<U>(mbi.RegionSize);
    }
    return true;
}

// The bytes of a committed range, written whatever its protection: hosted,
// .rdata is the loader's read-only (the IAT and the operand patches made it
// writable a page at a time and back).
void WriteRange(U base, const unsigned char* from, U bytes) {
    for (U at = base; at < base + bytes;) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(At(at), &mbi, sizeof mbi) == 0 || mbi.State != MEM_COMMIT)
            Fatal("snapshot: 0x%08X is not committed memory, inside a range checked before the restore began", at);
        U end = Va(mbi.BaseAddress) + static_cast<U>(mbi.RegionSize);
        if (end > base + bytes) end = base + bytes;
        DWORD old = 0;
        const bool open = Writable(mbi.Protect);
        if (!open && !VirtualProtect(At(at), end - at, PAGE_READWRITE, &old))
            Fatal("snapshot: VirtualProtect(0x%08X, 0x%X) failed (%lu)", at, end - at, GetLastError());
        std::memcpy(At(at), from + (at - base), end - at);
        if (!open) VirtualProtect(At(at), end - at, old, &old);
        at = end;
    }
}

// How much of the task arena below `top` is committed now, from the top
// down: the slices the game has touched (docs/crt-startup.md 11.4: the
// initial commit holds slices 0..2; slice 3 lies in guard pages until used).
U ArenaCommitted(U top) {
    if (top < kArenaBytes) return 0;
    U lo = top;
    while (lo > top - kArenaBytes) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(At(lo - 1), &mbi, sizeof mbi) == 0) break;
        if (mbi.State != MEM_COMMIT || !Readable(mbi.Protect)) break;
        const U region = Va(mbi.BaseAddress);
        lo = region > top - kArenaBytes ? region : top - kArenaBytes;
    }
    return top - lo;
}

// The arena's pages touched from the top down, one byte a page, so that the
// main thread's stack grows through its guard page as a deep call would grow
// it; then checked. On the main thread (the only one whose guard page this
// grows) - Snapshot_Pending's.
bool ArenaReady(U top, U bytes) {
    for (U at = (top - 1) & ~(kPage - 1); at + kPage > top - bytes; at -= kPage) {
        volatile unsigned char probe = *At(at);
        (void)probe;
        if (at < kPage) break;
    }
    return Committed(top - bytes, bytes);
}

// --- the build ---------------------------------------------------------------

HMODULE OurModule() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&OurModule), &module);
    return module;
}

// The launcher's version block's tag and bof3x.dll's file hash
// (docs/releases/README.md): the build whose code the task stacks' return
// addresses name. Hashed once, at the first F6 or F7, on the game's thread (a
// 64 MB file: that frame takes the time; not a worker, since the hosted
// self-test calls this under the loader lock, where a thread cannot start); the host and the
// DLL's base complete it, since a quicksave is only good in a process whose
// code sits where the stacks say.
const BuildId& EnsureBuild() {
    static BuildId build;
    static bool done = false;
    if (done) return build;
    build.host = IsExeHost() ? kHostExe : kHostBof3;
    std::snprintf(build.tag, sizeof build.tag, "%s", BOF3X_VERSION_TAG);
    wchar_t path[MAX_PATH];
    HMODULE module = OurModule();
    const DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) Fatal("snapshot: bof3x.dll's own path is not known (%lu)", GetLastError());
    std::string hex, why;
    if (!bof3x::Sha256Hex(path, hex, why) || hex.size() != sizeof build.sha)
        Fatal("snapshot: cannot hash bof3x.dll (%s): %s", Utf8Path(path).text, why.c_str());
    std::memcpy(build.sha, hex.data(), sizeof build.sha);
    build.dll_base = Va(module);
    Log("quicksave   this build: %s, bof3x.dll sha256 %.64s at 0x%08X, %s host", build.tag, build.sha, build.dll_base,
        build.host == kHostExe ? "exe" : "BOF3.exe");
    done = true;
    return build;
}

void Stamp(std::vector<unsigned char>& file, const BuildId& build) {
    auto* h = reinterpret_cast<Header*>(file.data() + kSaveHalf);
    h->host = build.host;
    std::memcpy(h->tag, build.tag, sizeof h->tag);
    std::memcpy(h->dll_sha256, build.sha, sizeof h->dll_sha256);
    h->dll_base = build.dll_base;
}

// --- the take ----------------------------------------------------------------

using SaveHalfFn = void (*)(unsigned char* to);

// The game's save half: F12's block, built as Save_QuickWrite builds it (slot
// 0), copied out of the staging buffer. Through Capcom's address, so its
// BOF3X_ORIGINAL switch holds. The take puts the span back afterwards.
// Rest2c_Slot is the save menu's slot word: Save_BuildBlock copies its
// summary into this slot's list entry and ours aborts past 16 (rest_2c.cpp
// SummaryOf); F12's Save_QuickWrite sets it to 0 first, and so does the take
// (the span is put back afterwards, so the game never sees either write).
void GameSaveHalf(unsigned char* to) {
    Rest2c_Slot = 0;
    orig::Save_BuildBlock();
    std::memcpy(to, Save_Staging, kSaveHalf);
}

// The file's bytes, the build fields left for Stamp. Between frames, on the
// main thread: participants first, then the span and the arena copied, then
// the save half built and the span written back over what building it wrote
// (the staging buffer, the live block's checksum word, a summary entry), so
// a take leaves the game's memory as it found it.
std::vector<unsigned char> Take(const Registry& registry, const Span& span, U arena_top, SaveHalfFn save_half) {
    std::vector<std::vector<unsigned char>> records(registry.size());
    for (std::size_t i = 0; i < registry.size(); ++i) {
        SnapshotWriter w(records[i]);
        registry[i].save(w);
    }
    const U arena = ArenaCommitted(arena_top);
    std::size_t size = kSaveHalf + sizeof(Header) + span.image.bytes + span.pool.bytes + arena;
    for (const auto& r : records) size += sizeof(RecordHead) + r.size();
    std::vector<unsigned char> file(size);

    unsigned char* p = file.data() + kSaveHalf + sizeof(Header);
    unsigned char* const image = p;
    std::memcpy(p, At(span.image.base), span.image.bytes);
    p += span.image.bytes;
    if (span.pool.bytes) std::memcpy(p, At(span.pool.base), span.pool.bytes);
    p += span.pool.bytes;
    if (arena) std::memcpy(p, At(arena_top - arena), arena);
    p += arena;
    for (std::size_t i = 0; i < registry.size(); ++i) {
        RecordHead rh{};
        std::snprintf(rh.tag, sizeof rh.tag, "%s", registry[i].tag.c_str());
        rh.bytes = static_cast<U>(records[i].size());
        std::memcpy(p, &rh, sizeof rh);
        p += sizeof rh;
        if (!records[i].empty()) std::memcpy(p, records[i].data(), records[i].size());
        p += records[i].size();
    }

    save_half(file.data());
    WriteRange(span.image.base, image, span.image.bytes);

    auto* h = reinterpret_cast<Header*>(file.data() + kSaveHalf);
    std::memcpy(h->magic, kMagic, sizeof kMagic);
    h->format = kFormat;
    h->header_bytes = sizeof(Header);
    h->file_bytes = static_cast<U>(size);
    U frame;
    std::memcpy(&frame, image + (Va(&Frame_Counter) - span.image.base), 4);
    h->frame_counter = frame;
    h->span_base = span.image.base;
    h->span_bytes = span.image.bytes;
    h->pool_base = span.pool.base;
    h->pool_bytes = span.pool.bytes;
    h->arena_top = arena_top;
    h->arena_bytes = arena;
    h->rdata_fnv = Fnv(kRdata);
    h->participants = static_cast<U>(registry.size());
    return file;
}

// --- the read ----------------------------------------------------------------

struct Parsed {
    bool save_half = false;   // the first 0x12B0 bytes are there: a save file to the menu's reader
    bool ok = false;          // the snapshot half may be restored
    std::string why;          // short: the overlay's refusal
    std::string detail;       // the log's
    Header header{};
    const unsigned char* image = nullptr;
    const unsigned char* pool = nullptr;
    const unsigned char* arena = nullptr;
    struct Rec {
        std::size_t participant;
        const unsigned char* data;
        U bytes;
    };
    std::vector<Rec> records;
};

Parsed Refuse(Parsed p, const std::string& why, const std::string& detail) {
    p.ok = false;
    p.why = why;
    p.detail = detail;
    return p;
}

std::string Field(const char* s, std::size_t n) { return std::string(s, strnlen(s, n)); }

// Everything a restore needs, checked before a byte is written: a refusal
// here leaves the game exactly as it was.
Parsed Parse(const std::vector<unsigned char>& file, const BuildId& build, const Span& span, U arena_top,
             const Registry& registry) {
    Parsed p;
    if (file.size() < kSaveHalf) return Refuse(p, "not a save file", Printf("%u bytes, under a save file's 0x%X", (unsigned)file.size(), kSaveHalf));
    p.save_half = true;
    if (file.size() < kSaveHalf + sizeof(Header)) return Refuse(p, "no snapshot", "a save file with no snapshot after it");
    std::memcpy(&p.header, file.data() + kSaveHalf, sizeof(Header));
    const Header& h = p.header;
    if (std::memcmp(h.magic, kMagic, sizeof kMagic) != 0) return Refuse(p, "no snapshot", "no BOF3XQS header after the save half");
    if (h.format != kFormat || h.header_bytes != sizeof(Header))
        return Refuse(p, "another format", Printf("snapshot format %u (header %u bytes); this build reads %u (%u)", h.format,
                                                  h.header_bytes, kFormat, (unsigned)sizeof(Header)));
    if (h.file_bytes != file.size())
        return Refuse(p, "truncated", Printf("the header says %u bytes, the file has %u", h.file_bytes, (unsigned)file.size()));
    if (h.host != build.host)
        return Refuse(p, "another host", Printf("made under %s, this is %s", h.host == kHostExe ? "bof3x.exe" : "BOF3.exe",
                                                build.host == kHostExe ? "bof3x.exe" : "BOF3.exe"));
    if (std::memcmp(h.tag, build.tag, sizeof h.tag) != 0 || std::memcmp(h.dll_sha256, build.sha, sizeof h.dll_sha256) != 0)
        return Refuse(p, "another build",
                      Printf("made by %s (bof3x.dll %.12s...), this is %s (%.12s...); the save half is still a save",
                             Field(h.tag, sizeof h.tag).c_str(), h.dll_sha256, Field(build.tag, sizeof build.tag).c_str(), build.sha));
    if (h.dll_base != build.dll_base)
        return Refuse(p, "another process",
                      Printf("bof3x.dll was at 0x%08X, here at 0x%08X: the task stacks name its code where it was", h.dll_base,
                             build.dll_base));
    if (h.span_base != span.image.base || h.span_bytes != span.image.bytes || h.pool_bytes != span.pool.bytes ||
        (span.pool.bytes && h.pool_base != span.pool.base))
        return Refuse(p, "another span",
                      Printf("span 0x%08X+0x%X, pool 0x%08X+0x%X; here 0x%08X+0x%X, 0x%08X+0x%X", h.span_base, h.span_bytes,
                             h.pool_base, h.pool_bytes, span.image.base, span.image.bytes, span.pool.base, span.pool.bytes));
    if (h.arena_top != arena_top || h.arena_bytes > kArenaBytes)
        return Refuse(p, "another process",
                      Printf("the task stacks hung from 0x%08X (0x%X bytes), here from 0x%08X: a quicksave is this process's",
                             h.arena_top, h.arena_bytes, arena_top));
    std::size_t at = kSaveHalf + sizeof(Header);
    const std::size_t fixed = std::size_t{h.span_bytes} + h.pool_bytes + h.arena_bytes;
    if (file.size() - at < fixed) return Refuse(p, "truncated", "the span and the arena run past the file");
    p.image = file.data() + at;
    p.pool = p.image + h.span_bytes;
    p.arena = p.pool + h.pool_bytes;
    at += fixed;
    std::vector<bool> seen(registry.size(), false);
    for (U i = 0; i < h.participants; ++i) {
        RecordHead rh;
        if (file.size() - at < sizeof rh) return Refuse(p, "truncated", Printf("record %u's head runs past the file", i));
        std::memcpy(&rh, file.data() + at, sizeof rh);
        at += sizeof rh;
        if (file.size() - at < rh.bytes) return Refuse(p, "truncated", Printf("record %u's bytes run past the file", i));
        const std::string tag = Field(rh.tag, sizeof rh.tag);
        std::size_t k = 0;
        while (k < registry.size() && registry[k].tag != tag) ++k;
        if (k == registry.size())
            return Refuse(p, "a record with no participant",
                          Printf("record '%s' (%u bytes) has no participant in this build: refused rather than restored in part",
                                 tag.c_str(), rh.bytes));
        if (seen[k]) return Refuse(p, "a record twice", Printf("record '%s' appears twice", tag.c_str()));
        seen[k] = true;
        if (registry[k].check) {
            SnapshotReader reader(registry[k].tag.c_str(), file.data() + at, rh.bytes);
            std::string why, detail;
            if (!registry[k].check(reader, why, detail)) return Refuse(p, why, Printf("record '%s': %s", tag.c_str(), detail.c_str()));
        }
        p.records.push_back({k, file.data() + at, rh.bytes});
        at += rh.bytes;
    }
    if (at != file.size()) return Refuse(p, "trailing bytes", Printf("%u bytes after the last record", (unsigned)(file.size() - at)));
    for (std::size_t k = 0; k < registry.size(); ++k)
        if (!seen[k])
            return Refuse(p, "a participant with no record",
                          Printf("participant '%s' has no record in the file: refused rather than restored in part",
                                 registry[k].tag.c_str()));
    if (!Committed(span.image.base, span.image.bytes) || (span.pool.bytes && !Committed(span.pool.base, span.pool.bytes)))
        return Refuse(p, "the span is not mapped", "a page of the span is not committed");
    p.ok = true;
    return p;
}

// --- the restore -------------------------------------------------------------

// A parsed file into the process: the span (the keep list's live bytes put
// back over it), every participant in the file's order, and the task arena
// LAST - on a stack of its own (OnOwnStack), so nothing after it writes into
// the slices it has just restored. The arena must be ready (ArenaReady) - the
// caller's check, so a refusal there still leaves everything untouched.
void Restore(const Parsed& p, const Registry& registry) {
    const Header& h = p.header;
    for (const Parsed::Rec& r : p.records)   // the live process's resources let go while its words still name them
        if (registry[r.participant].discard) registry[r.participant].discard();
    std::vector<std::vector<unsigned char>> live(kKeptCount);
    for (std::size_t i = 0; i < kKeptCount; ++i) live[i].assign(At(kKept[i].at), At(kKept[i].at + kKept[i].bytes));
    WriteRange(h.span_base, p.image, h.span_bytes);
    if (h.pool_bytes) WriteRange(h.pool_base, p.pool, h.pool_bytes);
    for (std::size_t i = 0; i < kKeptCount; ++i) WriteRange(kKept[i].at, live[i].data(), kKept[i].bytes);
    for (const Parsed::Rec& r : p.records) {
        const Participant& who = registry[r.participant];
        SnapshotReader reader(who.tag.c_str(), r.data, r.bytes);
        who.restore(reader);
        if (reader.Left() != 0)
            Fatal("snapshot: participant '%s' restored from %u of its record's %u bytes: a partial restore", who.tag.c_str(),
                  r.bytes - static_cast<U>(reader.Left()), r.bytes);
    }
    if (h.arena_bytes) std::memcpy(At(h.arena_top - h.arena_bytes), p.arena, h.arena_bytes);
}

// --- the files ---------------------------------------------------------------

std::wstring SlotPath(const std::wstring& dir, unsigned slot) {
    wchar_t name[32];
    std::swprintf(name, 32, L"BISLPS%02X.DAT", slot);
    return dir + name;
}
std::string SlotName(unsigned slot) { return Printf("BISLPS%02X.DAT", slot); }

bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// F6's history (the owner, 2026-10-17): 1F deleted, then 1E -> 1F down to
// 10 -> 11, renames only, top down; a gap is passed over, so every file
// moves up one. A failure stops the rotation where it is: every file still
// present under some name, nothing half-moved.
bool Rotate(const std::wstring& dir, std::string& why) {
    const std::wstring top = SlotPath(dir, kLastSlot);
    if (Exists(top) && !DeleteFileW(top.c_str())) {
        why = Printf("%s could not be deleted (error %lu)", SlotName(kLastSlot).c_str(), GetLastError());
        return false;
    }
    for (unsigned slot = kLastSlot - 1; slot + 1 > kFirstSlot; --slot) {
        const std::wstring from = SlotPath(dir, slot);
        if (!Exists(from)) continue;
        if (!MoveFileExW(from.c_str(), SlotPath(dir, slot + 1).c_str(), 0)) {
            why = Printf("%s could not be renamed to %s (error %lu)", SlotName(slot).c_str(), SlotName(slot + 1).c_str(),
                         GetLastError());
            return false;
        }
    }
    return true;
}

bool WriteAll(const std::wstring& path, const std::vector<unsigned char>& bytes, std::string& why) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        why = Printf("cannot create %s (error %lu)", Utf8Path(path.c_str()).text, GetLastError());
        return false;
    }
    DWORD wrote = 0;
    BOOL ok = WriteFile(f, bytes.data(), static_cast<DWORD>(bytes.size()), &wrote, nullptr);
    // On the disk before the rename makes it the newest: a power cut after the
    // rename must not leave a slot of zeros (the review of PR #95).
    if (ok) ok = FlushFileBuffers(f);
    const DWORD error = GetLastError();
    CloseHandle(f);
    if (!ok || wrote != bytes.size()) {
        DeleteFileW(path.c_str());
        why = Printf("writing %s failed after %lu of %u bytes (error %lu)", Utf8Path(path.c_str()).text, wrote,
                     (unsigned)bytes.size(), error);
        return false;
    }
    return true;
}

// The newest quicksave: written whole under a temporary name first, so a
// failed write rotates nothing; then the rotation; then the rename to 10.
bool WriteNewest(const std::wstring& dir, const std::vector<unsigned char>& bytes, std::string& why) {
    const std::wstring tmp = dir + L"BISLPS10.TMP";
    if (!WriteAll(tmp, bytes, why)) return false;
    if (!Rotate(dir, why)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), SlotPath(dir, kFirstSlot).c_str(), MOVEFILE_REPLACE_EXISTING)) {
        why = Printf("BISLPS10.TMP could not be renamed to %s (error %lu); the history moved up a slot", SlotName(kFirstSlot).c_str(),
                     GetLastError());
        return false;
    }
    return true;
}

bool ReadSlot(const std::wstring& dir, unsigned slot, std::vector<unsigned char>& bytes, std::string& why) {
    const std::wstring path = SlotPath(dir, slot);
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        why = error == ERROR_FILE_NOT_FOUND ? Printf("no quicksave in slot %X (%s)", slot, SlotName(slot).c_str())
                                            : Printf("cannot open %s (error %lu)", SlotName(slot).c_str(), error);
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f, &size) || size.QuadPart > static_cast<LONGLONG>(kMaxFile)) {
        CloseHandle(f);
        why = Printf("%s is larger than any quicksave", SlotName(slot).c_str());
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD got = 0;
    const BOOL ok = bytes.empty() || ReadFile(f, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr);
    CloseHandle(f);
    if (!ok || got != bytes.size()) {
        why = Printf("reading %s failed (%lu of %u bytes)", SlotName(slot).c_str(), got, (unsigned)bytes.size());
        return false;
    }
    return true;
}

// A stack of its own, as a fiber of the calling thread (the renderer's
// RunOnFiber and the decoder's DecodeFrame have theirs for the same reason):
// Snapshot_Pending runs in WinMain's loop, about 0x3E00 bytes above slice 0
// of the task arena (docs/quicksave-census.md 2.1), and the take, the
// restore, the file layer, the DLL's hash and every participant's Save and
// Restore (DirectSound, Direct3D) are kept off that stack - a take must copy
// slice 0 as the tasks left it, and a restore must not overwrite it after.
// The x87 control word is carried over: the game's (SetX87), not the new
// fiber's default.
void* g_own_fiber = nullptr;
void* g_return_fiber = nullptr;
const std::function<void()>* g_own_job = nullptr;
unsigned short g_own_cw = 0;

void CALLBACK OwnFiber(void*) {
    for (;;) {
        asm volatile("fldcw %0" : : "m"(g_own_cw));
        (*g_own_job)();
        g_own_job = nullptr;
        SwitchToFiber(g_return_fiber);
    }
}

void OnOwnStack(const std::function<void()>& job) {
    bool converted = false;
    void* caller = nullptr;
    if (IsThreadAFiber()) {
        caller = GetCurrentFiber();
    } else {
        caller = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (!caller) Fatal("snapshot: ConvertThreadToFiberEx failed (%lu)", GetLastError());
        converted = true;
    }
    if (!g_own_fiber) {
        g_own_fiber = CreateFiberEx(256 * 1024, 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, &OwnFiber, nullptr);
        if (!g_own_fiber) Fatal("snapshot: CreateFiberEx failed (%lu)", GetLastError());
    }
    asm volatile("fnstcw %0" : "=m"(g_own_cw));
    g_return_fiber = caller;
    g_own_job = &job;
    SwitchToFiber(g_own_fiber);
    // Back on the caller's own fiber: a thread made a fiber here is given
    // back as it came (the renderer converts the main thread itself later).
    if (converted && !ConvertFiberToThread()) Fatal("snapshot: ConvertFiberToThread failed (%lu)", GetLastError());
}

// --- the keys ------------------------------------------------------------------

enum class Request { None, Take, Load };
volatile Request g_request = Request::None;
unsigned g_request_slot = kFirstSlot;
char g_overlay[128];

void Show(const std::string& text) {
    std::snprintf(g_overlay, sizeof g_overlay, "%s", text.c_str());
    Overlay_Text = g_overlay;
    Overlay_Frames = kOverlayFrames;
}

// The toolchain's FILE* slots (File_Slots): all empty between frames, as
// every reader closes in its call, or the take is refused (a file open across
// a restore would be another process's handle).
bool FilesClosed() {
    for (U i = 0; i < File_Slots_count; ++i)
        if (File_Slots[i] != nullptr) return false;
    return true;
}

// On the own stack (Snapshot_Pending).
void DoTake() {
    if (!FilesClosed()) {
        Log("quicksave   F6 at Frame_Counter %u refused: a file is open in File_Slots (0x%08X) between frames",
            static_cast<U>(Frame_Counter), File_Slots_addr);
        Show("Quicksave refused: a file is open");
        return;
    }
    const Span span = CurrentSpan();
    const U top = static_cast<U>(Task_StackTop);
    std::vector<unsigned char> file = Take(g_registry, span, top, &GameSaveHalf);
    const auto* h = reinterpret_cast<const Header*>(file.data() + kSaveHalf);
    std::string why;
    Stamp(file, EnsureBuild());
    if (!WriteNewest(L"", file, why)) {
        Log("quicksave   F6 at Frame_Counter %u refused: %s", h->frame_counter, why.c_str());
        Show("Quicksave refused: " + why);
        return;
    }
    Log("quicksave   F6 at Frame_Counter %u: BISLPS10.DAT, %u bytes (save half 0x%X, span 0x%08X+0x%X, pool 0x%08X+0x%X, "
        "task arena 0x%08X-0x%X, %u participants; .rdata fnv %08X)",
        h->frame_counter, h->file_bytes, kSaveHalf, h->span_base, h->span_bytes, h->pool_base, h->pool_bytes, h->arena_top,
        h->arena_bytes, h->participants, h->rdata_fnv);
    Show("Quicksave OK");
}

// On the own stack; the arena already probed on the main thread's.
void DoLoad(unsigned slot) {
    std::vector<unsigned char> file;
    std::string why;
    const BuildId& build = EnsureBuild();
    if (!ReadSlot(L"", slot, file, why)) {
        Log("quicksave   F7 refused: %s", why.c_str());
        Show("Quickload refused: " + why);
        return;
    }
    const U top = static_cast<U>(Task_StackTop);
    const Parsed p = Parse(file, build, CurrentSpan(), top, g_registry);
    if (!p.ok) {
        Log("quicksave   F7 %s refused: %s - %s", SlotName(slot).c_str(), p.why.c_str(), p.detail.c_str());
        Show("Quickload refused: " + p.why);
        return;
    }
    if (!Committed(top - p.header.arena_bytes, p.header.arena_bytes)) {
        Log("quicksave   F7 %s refused: the task arena 0x%08X-0x%X is not committed", SlotName(slot).c_str(), top,
            p.header.arena_bytes);
        Show("Quickload refused: the task stacks");
        return;
    }
    const U rdata = Fnv(kRdata);
    if (rdata != p.header.rdata_fnv)
        Log("quicksave   F7 %s: .rdata fnv %08X now, %08X at the take - written in play, and not copied (the census's "
            "premise for leaving it out does not hold)",
            SlotName(slot).c_str(), rdata, p.header.rdata_fnv);
    const U from = Frame_Counter;
    Restore(p, g_registry);
    Log("quicksave   F7 %s restored at Frame_Counter %u: back to Frame_Counter %u (%u bytes; %u participants; .rdata %s)",
        SlotName(slot).c_str(), from, p.header.frame_counter, p.header.file_bytes, p.header.participants,
        rdata == p.header.rdata_fnv ? "unchanged" : "CHANGED");
    Show("Quickload");   // after the span: Overlay_Text is in it
}

// --- ST: our statics that are game state -------------------------------------
//
// docs/quicksave-census.md 3 ("ST"): the statics of ours that hold game state
// outside the span and belong to neither audio nor graphics. Rand's seed
// (crt_rest, MSVC's holdrand: ours since crt-rest 2.1), DIV-0062's free ring
// (DrawPool_Grow's free_list; the items and DrawItemPool_Top are in the
// span), DIV-0007's relocated system message pool (MsgPool_Relocate's
// buffer, under a language overlay), and DIV-0133/0134's "the release's
// words stand" flag, with sigma's table rebuilt from the restored Config
// words. A byte copy each; a size that differs at a restore is another
// build's, which the build check refused already, so it is a Fatal here -
// except the message pool's, which is the language's (StCheck).
constexpr U kStVersion = 1;

void StSave(SnapshotWriter& w) {
    w.Put(kStVersion);
    w.Put(*crt_rest::RandSeedCell());
    const U ring = draw_pool::Count();
    w.Put(ring);
    w.Put(draw_pool::Free(), ring * sizeof(unsigned short));
    std::uint32_t pool_bytes = 0;
    const std::uint8_t* pool = MsgPool_Moved(&pool_bytes);
    w.Put(pool_bytes);
    if (pool_bytes) w.Put(pool, pool_bytes);
    const unsigned char release_words = bof3x::input::sigma::RecipeTranslating() ? 1 : 0;
    w.Put(release_words);
}

// The message pool is moved (DIV-0007) under any language overlay and not
// under the original Chinese, and the language is the start's (BOF3X_LANG),
// not the build's: a quicksave taken in one and loaded in the other passes
// the build check. Refused here, before the span is written - StRestore's
// Fatal would come after it. One translation into another is let through:
// both pools are 0x8000 bytes, and the take's text stands until the game
// reloads its own (IDEAS I47 reviews that).
bool StCheck(SnapshotReader& r, std::string& why, std::string& detail) {
    if (r.Left() < 3 * sizeof(U)) return true;   // StRestore's Fatals say what is wrong
    if (r.Get<U>() != kStVersion) return true;
    r.Get<std::uint32_t>();
    const U ring = r.Get<U>();
    if (r.Left() < std::size_t{ring} * sizeof(unsigned short) + sizeof(std::uint32_t)) return true;
    std::vector<unsigned short> skip(ring);
    if (ring) r.Get(skip.data(), ring * sizeof(unsigned short));
    const std::uint32_t pool_bytes = r.Get<std::uint32_t>();
    std::uint32_t now_bytes = 0;
    MsgPool_Moved(&now_bytes);
    if ((pool_bytes != 0) == (now_bytes != 0)) return true;
    why = "another language";
    detail = pool_bytes ? "taken under a translation (BOF3X_LANG), this game runs the original Chinese text"
                        : "taken under the original Chinese text, this game runs a translation (BOF3X_LANG)";
    return false;
}

void StRestore(SnapshotReader& r) {
    if (r.Get<U>() != kStVersion) Fatal("snapshot: ST's record is another version");
    *crt_rest::RandSeedCell() = r.Get<std::uint32_t>();
    const U ring = r.Get<U>();
    if (ring != draw_pool::Count()) Fatal("snapshot: ST's draw-pool ring has %u entries, this process %u", ring, draw_pool::Count());
    r.Get(draw_pool::Free(), ring * sizeof(unsigned short));
    std::uint32_t now_bytes = 0;
    std::uint8_t* pool = MsgPool_Moved(&now_bytes);
    const std::uint32_t pool_bytes = r.Get<std::uint32_t>();
    if (pool_bytes != now_bytes) Fatal("snapshot: ST's message pool is %u bytes, this process's %u", pool_bytes, now_bytes);
    if (pool_bytes) r.Get(pool, pool_bytes);
    bof3x::input::sigma::SnapshotRestored(r.Get<unsigned char>() != 0);
}

// --- the self-test -----------------------------------------------------------

int g_fails = 0;
void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_fails;
        Log("shadow      snapshot: FAIL %s", what);
    }
}

std::vector<unsigned char> g_test_record;
std::vector<unsigned char> g_test_restored;
void TestSave(SnapshotWriter& w) { w.Put(g_test_record.data(), g_test_record.size()); }
void TestRestore(SnapshotReader& r) {
    g_test_restored.resize(r.Left());
    r.Get(g_test_restored.data(), g_test_restored.size());
}
void TestEmptySave(SnapshotWriter&) {}
void TestEmptyRestore(SnapshotReader&) {}
bool TestRefuse(SnapshotReader&, std::string& why, std::string& detail) {
    why = "refused by its check";
    detail = "the self-test's";
    return false;
}

// StCheck on ST records made by StSave, then with the pool's presence turned
// the other way: the language swap refused, the same language let through.
void SelfTestStCheck() {
    std::vector<unsigned char> rec;
    SnapshotWriter w(rec);
    StSave(w);
    std::string why, detail;
    SnapshotReader same("ST", rec.data(), rec.size());
    Check(StCheck(same, why, detail), "ST's check lets a record of this process's language through");
    const std::size_t at = 3 * sizeof(U) + std::size_t{draw_pool::Count()} * sizeof(unsigned short);
    std::uint32_t pool_bytes = 0;
    std::memcpy(&pool_bytes, rec.data() + at, sizeof pool_bytes);
    std::vector<unsigned char> other(rec.begin(), rec.begin() + at);
    const std::uint32_t flipped = pool_bytes ? 0 : 0x8000;
    const auto* f = reinterpret_cast<const unsigned char*>(&flipped);
    other.insert(other.end(), f, f + sizeof flipped);
    other.resize(other.size() + flipped + 1, 0);
    SnapshotReader swapped("ST", other.data(), other.size());
    Check(!StCheck(swapped, why, detail) && why == "another language",
          "ST's check refuses a record from the original Chinese under a translation, or the reverse");
}

void FakeSaveHalf(unsigned char* to) {
    for (U i = 0; i < kSaveHalf; ++i) to[i] = static_cast<unsigned char>(i * 7 + 3);
}

std::vector<unsigned char> ReadRaw(const Range& r) {
    std::vector<unsigned char> v(r.bytes);
    if (r.bytes) std::memcpy(v.data(), At(r.base), r.bytes);
    return v;
}

void Scribble(const Range& r, U seed) {
    std::vector<unsigned char> v(r.bytes);
    for (U i = 0; i < r.bytes; ++i) v[i] = static_cast<unsigned char>((i * 2654435761u + seed) >> 13);
    WriteRange(r.base, v.data(), r.bytes);
}

bool Same(const Range& r, const std::vector<unsigned char>& v) { return r.bytes == v.size() && std::memcmp(At(r.base), v.data(), r.bytes) == 0; }

// The span and the arena: a take into a file, a scribble over every page,
// the file read back and restored, the pages equal but the process-owned
// words, which keep the live (scribbled) values; then the participant cases
// and the build. The game's memory is put back from a raw copy at the end.
void SelfTestSpan(const std::wstring& dir) {
    const Span span = CurrentSpan();
    const auto image0 = ReadRaw(span.image), pool0 = ReadRaw(span.pool);
    // A stand-in arena on the heap: at start-up Task_StackTop is not set yet.
    std::vector<unsigned char> arena_block(kArenaBytes + kPage);
    const U top = (Va(arena_block.data()) + kArenaBytes + kPage - 1) & ~(kPage - 1);
    if (top - kArenaBytes < Va(arena_block.data())) Fatal("snapshot self-test: the stand-in arena does not fit");
    for (U i = 0; i < kArenaBytes; ++i) At(top - kArenaBytes)[i] = static_cast<unsigned char>(i ^ (i >> 8));
    const std::vector<unsigned char> arena0(At(top - kArenaBytes), At(top));
    {
        const U t = top;
        WriteRange(Va(&Task_StackTop), reinterpret_cast<const unsigned char*>(&t), 4);
    }
    const auto image1 = ReadRaw(span.image);   // with the stand-in top

    Registry reg{{"TEST", &TestSave, &TestRestore}, {"EMPTY", &TestEmptySave, &TestEmptyRestore}};
    g_test_record.assign(300, 0);
    for (std::size_t i = 0; i < g_test_record.size(); ++i) g_test_record[i] = static_cast<unsigned char>(0xA5 ^ i);
    BuildId build;
    build.host = IsExeHost() ? kHostExe : kHostBof3;
    std::snprintf(build.tag, sizeof build.tag, "self-test");
    std::memset(build.sha, 'a', sizeof build.sha);
    build.dll_base = 0x10000000;

    std::vector<unsigned char> file;
    OnOwnStack([&] { file = Take(reg, span, top, &FakeSaveHalf); });   // the game's path: a fiber of its own
    Stamp(file, build);
    Check(Same(span.image, image1), "a take leaves the span as it found it");
    Check(file.size() == kSaveHalf + sizeof(Header) + span.image.bytes + span.pool.bytes + kArenaBytes + 2 * sizeof(RecordHead) + 300,
          "the file's size is the save half, the header, the span, the arena and the records");
    std::string why;
    bool wrote = false, read = false;
    std::vector<unsigned char> back;
    wrote = WriteNewest(dir, file, why);   // directly: hosted, the injects run under the loader lock (no thread)
    Check(wrote, "the take is written to BISLPS10.DAT");

    // The scribble: every page of the span, and the arena.
    Scribble(span.image, 0x1234);
    Scribble(span.pool, 0x5678);
    for (U i = 0; i < kArenaBytes; ++i) At(top - kArenaBytes)[i] = static_cast<unsigned char>(~i);
    std::vector<std::vector<unsigned char>> kept_now(kKeptCount);
    std::vector<bool> kept_byte(span.image.bytes, false);
    for (std::size_t i = 0; i < kKeptCount; ++i) {
        kept_now[i].assign(At(kKept[i].at), At(kKept[i].at + kKept[i].bytes));
        for (U b = 0; b < kKept[i].bytes; ++b)
            if (kKept[i].at + b >= span.image.base && kKept[i].at + b < span.image.base + span.image.bytes)
                kept_byte[kKept[i].at + b - span.image.base] = true;
    }
    // The scribbled span's Task_StackTop is noise; the restore is checked
    // against the stand-in top, as the game's would be against the live one.
    read = ReadSlot(dir, kFirstSlot, back, why);
    Check(read && back == file, "BISLPS10.DAT reads back as written");

    Parsed p = Parse(back, build, span, top, reg);
    Check(p.ok, "the file parses under its own build and registry");
    if (p.ok) {
        Check(ArenaReady(top, p.header.arena_bytes), "the stand-in arena is ready");
        g_test_restored.clear();
        OnOwnStack([&] { Restore(p, reg); });
        U wrong = 0;
        for (U at = 0; at < span.image.bytes; ++at) {
            if (!kept_byte[at] && At(span.image.base)[at] != image1[at] && ++wrong <= 4)
                Log("shadow      snapshot: span byte 0x%08X is 0x%02X after the restore, 0x%02X at the take", span.image.base + at,
                    At(span.image.base)[at], image1[at]);
        }
        Check(wrong == 0, "the span equals the take's but the keep list");
        bool kept_ok = true;
        for (std::size_t i = 0; i < kKeptCount; ++i)
            if (std::memcmp(At(kKept[i].at), kept_now[i].data(), kKept[i].bytes) != 0) kept_ok = false;
        Check(kept_ok, "the keep list's bytes keep their live values (Task_SchedulerEsp, the handles, the settings, the objects)");
        Check(Same(span.pool, pool0), "the pool equals the take's");
        Check(std::memcmp(At(top - kArenaBytes), arena0.data(), kArenaBytes) == 0, "the task arena equals the take's");
        Check(g_test_restored == g_test_record, "the participant's Restore reads back what its Save wrote");
    }

    // A record with no participant, and a participant with no record: both
    // refused before anything is written.
    Scribble(span.image, 0x9ABC);
    const auto scribbled = ReadRaw(span.image);
    Registry fewer{{"TEST", &TestSave, &TestRestore}};
    Parsed q = Parse(back, build, span, top, fewer);
    Check(!q.ok && q.why == "a record with no participant", "a record with no participant is refused");
    Registry more = reg;
    more.push_back({"EXTRA", &TestEmptySave, &TestEmptyRestore});
    q = Parse(back, build, span, top, more);
    Check(!q.ok && q.why == "a participant with no record", "a participant with no record is refused");
    Registry checked = reg;
    checked[0].check = &TestRefuse;
    q = Parse(back, build, span, top, checked);
    Check(!q.ok && q.why == "refused by its check", "a participant's check refuses its record before the restore");
    Check(Same(span.image, scribbled), "a refused restore writes nothing");

    // Another build: the snapshot half refused, the save half still a save
    // with its summary where the menu reads it.
    BuildId other = build;
    std::snprintf(other.tag, sizeof other.tag, "self-test-2");
    q = Parse(back, other, span, top, reg);
    Check(!q.ok && q.why == "another build", "a build-id mismatch is refused");
    unsigned char summary[kSummaryBytes];
    std::vector<unsigned char> expect(kSaveHalf);
    FakeSaveHalf(expect.data());
    std::memcpy(summary, back.data() + kSummaryAt, kSummaryBytes);
    Check(q.save_half && std::memcmp(summary, expect.data() + kSummaryAt, kSummaryBytes) == 0,
          "with the build refused, the save half still parses: the summary at 0xCA0");
    other = build;
    other.sha[0] = 'b';
    q = Parse(back, other, span, top, reg);
    Check(!q.ok && q.why == "another build", "a DLL hash mismatch is refused");
    other = build;
    other.dll_base += 0x10000;
    q = Parse(back, other, span, top, reg);
    Check(!q.ok && q.why == "another process", "another DLL base is refused");
    q = Parse(back, build, span, top + kPage, reg);
    Check(!q.ok && q.why == "another process", "another task-arena top is refused");
    std::vector<unsigned char> cut(back.begin(), back.end() - 1);
    q = Parse(cut, build, span, top, reg);
    Check(!q.ok && q.why == "truncated" && q.save_half, "a truncated file is refused, its save half kept");

    // The game's memory as it was.
    WriteRange(span.image.base, image0.data(), span.image.bytes);
    if (span.pool.bytes) WriteRange(span.pool.base, pool0.data(), span.pool.bytes);
    Check(Same(span.image, image0) && Same(span.pool, pool0), "the span is put back after the self-test");
    DeleteFileW(SlotPath(dir, kFirstSlot).c_str());
}

std::vector<unsigned char> Text(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

bool HoldsText(const std::wstring& path, const std::string& want) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    char buf[64] = {};
    DWORD got = 0;
    ReadFile(f, buf, sizeof buf - 1, &got, nullptr);
    CloseHandle(f);
    return std::string(buf, got) == want;
}

void ClearDir(const std::wstring& dir) {
    for (unsigned s = kFirstSlot; s <= kLastSlot; ++s) DeleteFileW(SlotPath(dir, s).c_str());
    DeleteFileW((dir + L"BISLPS10.TMP").c_str());
}

// The rotation: sixteen files each move up one, 1F's gone, 10 new; a gapped
// set moves the same; a rename that fails stops it with every file present.
void SelfTestRotation(const std::wstring& dir) {
    std::string why;
    ClearDir(dir);
    for (unsigned s = kFirstSlot; s <= kLastSlot; ++s) WriteAll(SlotPath(dir, s), Text(SlotName(s)), why);
    bool ok = WriteNewest(dir, Text("new"), why);
    Check(ok, "the rotation over sixteen files");
    bool moved = HoldsText(SlotPath(dir, kFirstSlot), "new");
    for (unsigned s = kFirstSlot + 1; s <= kLastSlot; ++s) moved = moved && HoldsText(SlotPath(dir, s), SlotName(s - 1));
    Check(moved, "sixteen files: each moved up one, the old 1F gone, 10 the new one");

    ClearDir(dir);
    const unsigned gapped[] = {0x10, 0x13, 0x15, 0x1F};
    for (unsigned s : gapped) WriteAll(SlotPath(dir, s), Text(SlotName(s)), why);
    ok = WriteNewest(dir, Text("new"), why);
    bool gap = ok && HoldsText(SlotPath(dir, 0x10), "new") && HoldsText(SlotPath(dir, 0x11), SlotName(0x10)) &&
               HoldsText(SlotPath(dir, 0x14), SlotName(0x13)) && HoldsText(SlotPath(dir, 0x16), SlotName(0x15));
    for (unsigned s : {0x12u, 0x13u, 0x15u, 0x17u, 0x1Fu}) gap = gap && !Exists(SlotPath(dir, s));
    Check(gap, "a gapped set: each file moved up one, the gaps kept, the old 1F gone");

    ClearDir(dir);
    for (unsigned s = kFirstSlot; s <= kLastSlot; ++s) WriteAll(SlotPath(dir, s), Text(SlotName(s)), why);
    HANDLE hold = CreateFileW(SlotPath(dir, 0x1A).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ok = WriteNewest(dir, Text("new"), why);
    if (hold != INVALID_HANDLE_VALUE) CloseHandle(hold);
    bool stopped = !ok && why.find("BISLPS1A.DAT could not be renamed") != std::string::npos;
    // 1F deleted (the oldest goes first), 1E..1B moved up, 1A and below where they were, no 10 written.
    stopped = stopped && !Exists(SlotPath(dir, 0x1B)) && HoldsText(SlotPath(dir, 0x1A), SlotName(0x1A)) &&
              HoldsText(SlotPath(dir, 0x1F), SlotName(0x1E)) && HoldsText(SlotPath(dir, 0x1C), SlotName(0x1B)) &&
              HoldsText(SlotPath(dir, 0x10), SlotName(0x10)) && !Exists(dir + L"BISLPS10.TMP");
    Check(stopped, "a rename that fails stops the rotation with a refusal, every file present, no new 10");
    if (!ok) Log("shadow      snapshot: the refusal read \"%s\"", why.c_str());
    ClearDir(dir);
}

void SelfTest() {
    wchar_t temp[MAX_PATH];
    const DWORD n = GetTempPathW(MAX_PATH, temp);
    if (n == 0 || n >= MAX_PATH - 48) Fatal("snapshot self-test: no temporary directory");
    wchar_t name[48];
    std::swprintf(name, 48, L"bof3x-snapshot-%lu\\", GetCurrentProcessId());
    const std::wstring dir = std::wstring(temp) + name;
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        Fatal("snapshot self-test: cannot create %s", Utf8Path(dir.c_str()).text);
    SelfTestSpan(dir);
    SelfTestStCheck();
    SelfTestRotation(dir);
    ClearDir(dir);
    RemoveDirectoryW(dir.c_str());
    if (g_fails) Fatal("snapshot self-test: %d check(s) failed (the log has each)", g_fails);
    const Span span = CurrentSpan();
    Log("shadow      snapshot: self-test passed - span 0x%08X+0x%X and pool 0x%08X+0x%X copied, scribbled and restored equal; "
        "a missing participant either side, a participant's check, another language, another build, DLL base, arena top "
        "and a truncated file refused; the "
        "rotation over sixteen files, a gapped set and a failed rename",
        span.image.base, span.image.bytes, span.pool.base, span.pool.bytes);
}

}  // namespace

void Snapshot_Register(const char* tag, SnapshotSaveFn save, SnapshotRestoreFn restore, SnapshotDiscardFn discard,
                       SnapshotCheckFn check) {
    if (g_registry_closed) Fatal("snapshot: participant '%s' registered after the first quicksave or quickload", tag);
    const std::size_t n = tag ? std::strlen(tag) : 0;
    if (n == 0 || n >= kTagBytes) Fatal("snapshot: a participant's tag must be 1..%u characters", (unsigned)kTagBytes - 1);
    for (std::size_t i = 0; i < n; ++i)
        if (tag[i] < 0x21 || tag[i] > 0x7E) Fatal("snapshot: participant tag '%s' is not printable", tag);
    if (!save || !restore) Fatal("snapshot: participant '%s' registered without both Save and Restore", tag);
    for (const Participant& p : g_registry)
        if (p.tag == tag) Fatal("snapshot: participant '%s' registered twice", tag);
    g_registry.push_back({tag, save, restore, discard, check});
    Log("quicksave   participant '%s' registered", tag);
}

// One request waits for the next loop point; a second before it replaces the
// first, and the log says so (two keys inside one message drain, ~16 ms).
static void Replacing(const char* now) {
    if (g_request == Request::None) return;
    Log("quicksave   %s replaces a %s not yet run", now, g_request == Request::Take ? "quicksave (F6)" : "quick-load");
}

void Snapshot_RequestTake() {
    Replacing("a quicksave (F6)");
    g_request_slot = kFirstSlot;
    g_request = Request::Take;
}

void Snapshot_RequestLoad(unsigned slot) {
    if (slot < kFirstSlot || slot > kLastSlot) Fatal("snapshot: a load asked of slot 0x%X, outside 0x10..0x1F", slot);
    Replacing("a quick-load");
    g_request_slot = slot;
    g_request = Request::Load;
}

const char* Snapshot_LastLine() { return g_overlay; }

void Snapshot_Pending() {
    const Request request = g_request;
    if (request == Request::None) return;
    g_request = Request::None;
    // BOF3X_GAPWATCH guards .data pages and parks them by the latch's count
    // (gapwatch.h): a copy of the span would fault through every guard, and a
    // restore would undo its bookkeeping. Refused while it is armed.
    if (GetEnvironmentVariableA("BOF3X_GAPWATCH", nullptr, 0) > 0) {
        Log("quicksave   %s refused: BOF3X_GAPWATCH is armed", request == Request::Take ? "F6" : "F7");
        Show(request == Request::Take ? "Quicksave refused: the read watch" : "Quickload refused: the read watch");
        return;
    }
    g_registry_closed = true;
    if (request == Request::Take) {
        OnOwnStack([] { DoTake(); });
        return;
    }
    // The arena's pages committed from the top down while still on the main
    // thread's stack (only its own thread grows a stack through the guard
    // page), all four slices, whatever the file holds.
    const U top = static_cast<U>(Task_StackTop);
    ArenaReady(top, top >= kArenaBytes ? kArenaBytes : 0);
    const unsigned slot = g_request_slot;
    OnOwnStack([slot] { DoLoad(slot); });
}

void Snapshot_Inject() {
    const U names[] = {Va(&Task_SchedulerEsp), Va(&Game_Hwnd),      Va(&Game_HInstance), Va(&Frame_Deadline),
                       Va(&Fps_Shown),         Va(&App_Active),     Va(&Cfg_Fullscreen), Va(&Cfg_RenderMode),
                       Va(Key_Table),          Va(&DInput_Object),  Va(&Font_GlyphData), Va(Gfx_WindowRect),
                       Va(Gfx_ClutRows),       Va(D3d_DeviceDesc),  Va(&Dd_DirectDraw),  Va(&Dd_StageSurface),
                       Va(&D3d_Device)};
    static_assert(sizeof names / sizeof names[0] == kKeptCount, "a symbol for every kept range");
    for (std::size_t i = 0; i < kKeptCount; ++i)
        if (names[i] != kKept[i].at)
            Fatal("snapshot: kKept's %s is 0x%08X, symbols.toml's 0x%08X", kKept[i].name, kKept[i].at, names[i]);
    // Hosted, with the other self-tests (BOF3X_SHADOW=snapshot or '*'). The
    // exe host refuses BOF3X_SHADOW (it compares Capcom's code, which that
    // host does not map); this test compares nothing of Capcom's and the exe
    // host's span is the one players have, so there it runs on every
    // BOF3X_SELFTEST_ONLY start, as ExeIcon_SelfTest does.
    if (IsExeHost() ? GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : WantsShadow("snapshot")) SelfTest();
    Snapshot_Register("ST", &StSave, &StRestore, nullptr, &StCheck);   // after DrawPool_Grow, sigma::Arm and MsgPool_Relocate's inject
}

}  // namespace bof3
