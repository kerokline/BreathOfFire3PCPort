// The snapshot core (snapshot.h; docs/quicksave.md). The quicksave round's
// group SN: the point, the span, the task arena, the process-owned words,
// the participant registry, the file BISLPS1X.DAT, F6's rotation and F7's
// load.
//
// DIVERGENCE (ledger draft A, docs/quicksave.md section 6): F6 and F7 are
// quicksave keys. The original's F7 was the next renderer device (nothing
// since DIV-0040), its F6 nothing.
#include "hook/snapshot.h"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "common/sha256.h"
#include "game/draw_pool.h"
#include "game/crt_rest.h"
#include "game/draw_pool_room.h"
#include "game/msg_pool.h"
#include "game/psp_code.h"
#include "game/save_tail.h"
#include "game/task_sched_callees.h"
#include "hook/bind.h"
#include "hook/detour.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/span_words.h"
#include "hook/task_yield.h"
#include "input/sigma.h"
#include "version.gen.h"
#include "os/os.h"

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

// The task arena: four slices below Task_StackTop (task_sched.cpp,
// Task_Create: slot k's stack top is Task_StackTop - k * kSlice). The slice
// is the width's: 0x4000 on i686 (task_sched_callees.h kStackSize); on x86-64
// the frames are about twice as deep and docs/x64-build-design.md 7.4
// estimates 0x8000 (kSlice64), the number stage C's stackwatch run on the
// routes settles - the task switch's constant and this one must then agree.
constexpr U kSlice = sizeof(void*) == 8 ? 0x8000 : 0x4000;
static_assert(kSlice == task_sched::kStackSize, "the arena's slice is the task switch's");
static_assert(task_yield::kTasks == task_sched::kTasks, "one yield entry a task record");
constexpr U kArenaBytes = 4 * kSlice;
constexpr U kPage = 0x1000;

constexpr unsigned kFirstSlot = 0x10, kLastSlot = 0x1F;
constexpr int kOverlayFrames = 0x78;   // F12's (win_main.cpp)

constexpr char kMagic[8] = {'B', 'O', 'F', '3', 'X', 'Q', 'S', '\0'};
// Format 1: every quicksave before the state-by-name round - the header alone,
// the span written as the process had it. Format 2 (2026-11-25,
// docs/state-by-name-format.md 2): the header's extension, the TASKS record,
// the settings and the words written by what they name.
constexpr U kFormat1 = 1;
constexpr U kFormat = 2;
constexpr U kHostBof3 = 0, kHostExe = 1;
constexpr std::size_t kTagBytes = 16;
constexpr std::size_t kMaxFile = 64u << 20;

// The state revision (docs/state-by-name-round.md 3.4; the owner's calls 6 and
// 7, 2026-11-25): raised by a change that gives a byte of saved state a new
// meaning, each step a row naming the ledger entry that raised it. A file of
// another revision is refused, its save offered: no converter is written
// until one change makes it worth its cost (call 7 (a)). The ledger's entry
// says "Saved state: changed (revision N)" (call 6; tools/ledger_check.py).
struct Revision {
    U number;
    const char* raised_by;   // the ledger entry, or "-" for the first
    const char* what;
};
constexpr Revision kRevisions[] = {
    {1, "-", "the first: the span, the participants' records and the TASKS record as format 2 has them (2026-11-25)"},
};
constexpr U kStateRevision = kRevisions[sizeof kRevisions / sizeof kRevisions[0] - 1].number;

#pragma pack(push, 1)
// After the save half. Little-endian, as the machine is. Format 1's whole
// header; format 2 follows it with HeaderExt.
struct Header {
    char magic[8];         // "BOF3XQS\0"
    U format;              // kFormat1 or kFormat
    U header_bytes;        // sizeof(Header), + sizeof(HeaderExt) in format 2
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
// Format 2's extension, right after Header (docs/state-by-name-format.md 2).
struct HeaderExt {
    U state_revision;      // kStateRevision at the take
    U portable;            // 1: another build may restore it (the tasks and the words, below); 0: this build's only
    U not_portable;        // why not: a NotPortable code | the task's number << 8; 0 when portable
    U settings_fnv;        // FNV-1a of the settings' lines (Settings)
    U settings_bytes;      // the lines' text, after the TASKS record
    U tasks_bytes;         // the TASKS record, after the arena: kTasks TaskRecs
    U words;               // entries of the WORDS list (a U each), after the settings
    U table_lo;            // the table of code places (Bind_TableLo): its top byte is the process's; 0 hosted
    U bind_rows, bind_fnv; // the VAs this build binds: how many, FNV-1a over them
    U image_base;          // ImageBase() at the take
    U pointer_bytes;       // sizeof(void*) of the writing build (the log's)
};
// One task's place in the TASKS record.
struct TaskRec {
    U kind;                // TaskKind
    U name;                // created: the entry's VA; site: the site's id; else 0
    U bytes;               // site: the locals' bytes
    unsigned char locals[task_yield::kLocalsMax];
};
struct RecordHead {
    char tag[kTagBytes];
    U bytes;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 8 + 4 * 4 + 32 + 64 + 4 * 10, "the header's layout is the file's");
static_assert(sizeof(HeaderExt) == 4 * 12, "the extension's layout is the file's");
static_assert(sizeof(TaskRec) == 12 + task_yield::kLocalsMax, "a TASKS entry's layout is the file's");

// Where a task stands at the take (docs/state-by-name-format.md 4).
enum TaskKind : U {
    kTaskFree = 0,      // state 0
    kTaskCreated = 1,   // created and not yet run: its place is its entry word
    kTaskSite = 2,      // at a site of task_yield.h, with its locals
    kTaskNowhere = 3,   // anywhere else: the take is this build's only
};

// Why a take is not portable (HeaderExt.not_portable's low byte).
enum NotPortable : U {
    kIsPortable = 0,
    kNpHosted = 1,           // the hosted build: Capcom's code mapped, no code places
    kNpTableUnmeasured = 2,  // the code places under a top byte the dumps did not measure
    kNpTaskNowhere = 3,      // a task stands at no named place (a nested yield)
    kNpTaskEntry = 4,        // a task created and not yet run whose entry names no VA
    kNpSlotCode = 5,         // a slot of Capcom's holds a function of ours by its address
    kNpRuntimeSlot = 6,      // a run-time slot names our module outside the message pool
    kNpTextOurs = 7,         // a text word (span_words::kTextWords) names our module outside the pool and the text slots
};

const char* NotPortableText(U code) {
    switch (code & 0xFF) {
    case kIsPortable: return "portable";
    case kNpHosted: return "taken by the hosted build";
    case kNpTableUnmeasured: return "the code places lie under an unmeasured top byte";
    case kNpTaskNowhere: return "a task stands at no named place";
    case kNpTaskEntry: return "a new task's entry names no VA";
    case kNpSlotCode: return "a slot holds a function of ours";
    case kNpRuntimeSlot: return "a message slot names our module";
    case kNpTextOurs: return "a message box's text is ours outside the message pool";
    default: return "a reason this build does not know";
    }
}

// The WORDS list (docs/state-by-name-format.md 3): a U per word of .data
// written by what it names, (offset from .data's start / 4) << 3 | kind.
enum WordKind : U {
    kWordCode = 0,     // a code place, written as its VA; back as this process's place (Code)
    kWordFold = 1,     // a pointer into the moved image, written as at 0x400000 (span_words::Fold)
    kWordFold24 = 2,   // a link into the moved image, its low 24 bits so (span_words::Fold24)
    kWordPool = 3,     // a run-time slot into our message pool, written as its offset in the pool
    kWordText = 4,     // a slot of Capcom's holding text of ours: written raw, kept as the process has it
    kWordTextCopy = 5, // a word inside the text a slot names: (offset in the text << 24) | the slot's offset, placed from the slot
};

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
// SnapshotKept (snapshot.h): the rows are exported for the region table's
// run-time check (tools/region_table.py, group A3), which also parses them
// from this source as {0xVA, bytes, "Name"} - keep that form.
using Kept = SnapshotKept;
// symbols.toml's addresses, as VAs (names; a range's place is Img(at)); the
// names are macros of symbols.gen.h, so the literals are written here and
// checked against them by Snapshot_Inject.
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
    {0x939A2C, 12, "Desktop_Bpp"},         // and Desktop_Height, Desktop_Width: the machine's (state-by-name-format.md 1)
};
constexpr std::size_t kKeptCount = sizeof kKept / sizeof kKept[0];

unsigned char* At(U va) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(va)); }
// A pointer as the 32-bit word the file holds (BuildId.dll_base, the arena's
// top): a Fatal above 4 GB (image_base.h Low32; docs/x64-build-design.md 10
// keeps dll_base a 32-bit word, since the 64-bit loader honours the DLL's
// base 0x10000000 as the 32-bit one does).
U Va(const void* p) { return Low32(p); }

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
Range Rdata() { return {Img(exe_layout::kRdataVa), exe_layout::kDataVa - exe_layout::kRdataVa}; }

U Fnv(const Range& r) {
    U h = 0x811C9DC5u;
    for (U i = 0; i < r.bytes; ++i) h = (h ^ At(r.base)[i]) * 0x01000193u;
    return h;
}

Span CurrentSpan() {
    // The exe host: .data and .bss to .game's end, then .pool, one run - at
    // the image's place, so a quicksave taken with the image at one base is
    // refused at another ("another span": its run-time pointers are the
    // take's, docs/relocatable-image-design.md 7).
    if (IsExeHost()) return {{Img(exe_layout::kDataVa), exe_layout::kOwnVa - exe_layout::kDataVa}, {}};
    // Hosted: BOF3.exe's .data to its last page (.rsrc follows at 0x93E000,
    // constant), and the draw pool where DrawPool_Reserve put it.
    const U end = (exe_layout::kBssEnd + kPage - 1) & ~(kPage - 1);
    const Range image{Img(exe_layout::kDataVa), end - exe_layout::kDataVa};
    const U items = Va(draw_pool::Items());
    if (items >= image.base && items < image.base + image.bytes) return {image, {}};   // DIV-0062 off: Capcom's array
    return {image, {items, draw_pool_room::kBytes}};
}

// Every page of [base, base + bytes) committed and readable.
bool Committed(U base, U bytes) {
    for (U at = base; at < base + bytes;) {
        os::MemInfo mbi;
        if (!os::MemQuery(At(at), &mbi)) return false;
        if (mbi.state != os::MemState::Committed || !mbi.readable) return false;
        at = Va(reinterpret_cast<const void*>(mbi.base)) + static_cast<U>(mbi.size);
    }
    return true;
}

// The bytes of a committed range, written whatever its protection: hosted,
// .rdata is the loader's read-only (the IAT and the operand patches made it
// writable a page at a time and back).
void WriteRange(U base, const unsigned char* from, U bytes) {
    for (U at = base; at < base + bytes;) {
        os::MemInfo mbi;
        if (!os::MemQuery(At(at), &mbi) || mbi.state != os::MemState::Committed)
            Fatal("snapshot: 0x%08X is not committed memory, inside a range checked before the restore began", at);
        U end = Va(reinterpret_cast<const void*>(mbi.base)) + static_cast<U>(mbi.size);
        if (end > base + bytes) end = base + bytes;
        os::ProtSave old;
        const bool open = mbi.writable;
        if (!open && !os::MemProtect(At(at), end - at, os::Prot::ReadWrite, &old))
            Fatal("snapshot: MemProtect(0x%08X, 0x%X) failed (%lu)", at, end - at, (unsigned long)os::LastError());
        std::memcpy(At(at), from + (at - base), end - at);
        if (!open) os::MemRestore(At(at), end - at, old);
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
        os::MemInfo mbi;
        if (!os::MemQuery(At(lo - 1), &mbi)) break;
        if (mbi.state != os::MemState::Committed || !mbi.readable) break;
        const U region = Va(reinterpret_cast<const void*>(mbi.base));
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
    std::string path;          // UTF-8 (os/env.h)
    std::uintptr_t module = 0;  // the module's base: a Windows module handle is its base
    if (!os::ModulePath(reinterpret_cast<const void*>(&EnsureBuild), &path, &module) || path.empty())
        Fatal("snapshot: bof3x.dll's own path is not known (%lu)", static_cast<unsigned long>(bof3::os::LastError()));
    std::string hex, why;
    if (!bof3x::Sha256Hex(os::ToWide(path.c_str()).c_str(), hex, why) || hex.size() != sizeof build.sha)
        Fatal("snapshot: cannot hash bof3x.dll (%s): %s", path.c_str(), why.c_str());
    std::memcpy(build.sha, hex.data(), sizeof build.sha);
    build.dll_base = Va(reinterpret_cast<const void*>(module));
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

// --- format 2: the settings, the bound functions, the save half ----------------

U FnvBytes(const void* data, std::size_t n, U h = 0x811C9DC5u) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

// The VAs this build binds (bind.h): a portable file's code words are VAs,
// placed here through this build's table; and a code word the writing build
// left unbound is the trap's Img(va), which reads as a number and is not in
// the WORDS list. So a portable load wants the same set of bound VAs: then
// every code word of the file is placed, and none is left a trap here that
// this build would have bound (docs/state-by-name-format.md 3.1).
void BindPrint(U* rows, U* fnv) {
    *rows = Bind_Rows();
    U h = 0x811C9DC5u;
    for (unsigned i = 0; i < *rows; ++i) {
        const U va = Bind_RowVa(i);
        h = FnvBytes(&va, sizeof va, h);
    }
    *fnv = h;
}

// An environment variable's value as the game reads it, "" when unset.
std::string EnvValue(const char* name) {
    static char text[16384];
    const std::uint32_t n = bof3::os::GetEnv(name, text, sizeof text);
    if (n == 0) return std::string();
    if (n >= sizeof text) Fatal("snapshot: %s is longer than %u bytes", name, static_cast<unsigned>(sizeof text) - 1);
    return std::string(text, n);
}

// A list of names (BOF3X_OFF's, BOF3X_ON's syntax: commas or spaces between)
// in one spelling: sorted, each once.
std::string NameList(const std::string& list) {
    std::vector<std::string> names;
    std::size_t at = 0;
    while (at < list.size()) {
        while (at < list.size() && (list[at] == ',' || list[at] == ' ')) ++at;
        std::size_t end = at;
        while (end < list.size() && list[end] != ',' && list[end] != ' ') ++end;
        if (end > at) names.push_back(list.substr(at, end - at));
        at = end;
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    std::string out;
    for (const std::string& n : names) out += (out.empty() ? "" : ",") + n;
    return out;
}

// The settings that are not safe to cross (the owner's call 8; the table and
// its evidence: docs/state-by-name-format.md 1), one "name=value" line each,
// in this order. A quicksave whose lines differ from this process's is
// refused with the names that differ, and its save offered: a load under
// another code switch, behaviour, mod list, champion, PSP rule, layering or
// would restore state the other value's code reads otherwise. The display
// scale crosses: every span word Rescale writes for it is kept live by the
// graphics participant (gfx_snapshot.cpp kKept). The language is its class only - the original text or a
// translation (quicksave.md 4b: one translation into another is let
// through, the owner's call of 2026-10-20). The music source is SOUND's
// check (quicksave.md 4c); the wide mode crosses (wide-ring.md 4.5).
std::string Settings() {
    std::uint32_t pool = 0;
    MsgPool_Moved(&pool);
    std::string s;
    const auto line = [&s](const char* name, const std::string& value) {
        s += name;
        s += '=';
        s += value;
        s += '\n';
    };
    line("lang", pool ? "translation" : "original");
    line("off", NameList(EnvValue("BOF3X_OFF")));
    line("original", NameList(EnvValue("BOF3X_ORIGINAL")));
    line("on", NameList(EnvValue("BOF3X_ON")));
    line("solo", EnvValue("BOF3X_SOLO"));
    line("opt", EnvValue("BOF3X_OPT"));
    line("psp", Printf("%u", ::psp_code::On()));
    line("desert-faerie", EnvValue("BOF3X_DESERT_FAERIE") == "1" ? "1" : "0");
    line("layering", EnvValue("BOF3X_LAYERING"));
    return s;
}

// The names whose lines differ between two settings texts ("psp", "on,
// opt"), and the log's detail: each as the file has it and as here.
std::string SettingsDiff(const std::string& file, const std::string& here, std::string& detail) {
    const auto parse = [](const std::string& text) {
        std::vector<std::pair<std::string, std::string>> lines;
        std::size_t at = 0;
        while (at < text.size()) {
            std::size_t end = text.find('\n', at);
            if (end == std::string::npos) end = text.size();
            const std::string l = text.substr(at, end - at);
            const std::size_t eq = l.find('=');
            lines.push_back({l.substr(0, eq), eq == std::string::npos ? std::string() : l.substr(eq + 1)});
            at = end + 1;
        }
        return lines;
    };
    const auto a = parse(file), b = parse(here);
    std::string names;
    const auto value = [](const std::vector<std::pair<std::string, std::string>>& lines, const std::string& name,
                          bool* found) -> std::string {
        for (const auto& l : lines)
            if (l.first == name) {
                *found = true;
                return l.second;
            }
        *found = false;
        return std::string();
    };
    std::vector<std::string> seen;
    for (const auto* side : {&a, &b})
        for (const auto& l : *side) {
            if (std::find(seen.begin(), seen.end(), l.first) != seen.end()) continue;
            seen.push_back(l.first);
            bool in_a = false, in_b = false;
            const std::string va = value(a, l.first, &in_a), vb = value(b, l.first, &in_b);
            if (in_a && in_b && va == vb) continue;
            names += (names.empty() ? "" : ", ") + l.first;
            detail += Printf("%s%s: '%s' in the file, '%s' here", detail.empty() ? "" : "; ", l.first.c_str(),
                             in_a ? va.c_str() : "(none)", in_b ? vb.c_str() : "(none)");
        }
    return names;
}

// The save half as LoadMenu_Read takes a save (sound.cpp; save_menu_callees.h
// kChecksum, kBlockBytes): the u16 at +0x70 is the stored sum, zeroed, and
// the low word of the sum of the block's 0x10B0 bytes must equal it. A
// refusal offers the save only when this holds (docs/state-by-name-format.md 5).
constexpr U kBlockBytes = 0x10B0;
constexpr U kChecksumAt = 0x70;
bool SaveHalfWhole(const unsigned char* f, std::size_t n) {
    if (n < kSaveHalf) return false;
    const U stored = static_cast<U>(f[kChecksumAt]) | (static_cast<U>(f[kChecksumAt + 1]) << 8);
    U sum = 0;
    for (U i = 0; i < kBlockBytes; ++i) sum += (i == kChecksumAt || i == kChecksumAt + 1) ? 0 : f[i];
    return (sum & 0xFFFF) == stored;
}

// --- format 2: the words written by what they name ------------------------------
//
// docs/state-by-name-format.md 3. A word of .data is written by its name when
// one of the state hash's own tests (span_words.h, bind.h) says what it
// names; every other byte of the span is written as the process had it. The
// tests are by value for code places (measured: no data word is one under the
// five top bytes, state-hash.md 8) and for the moved image (the fold), and by
// the list of Capcom's slots and of the run-time slots for text of ours.

// What this process names a word by, and places it back by.
struct WordPlace {
    U pool = 0, pool_bytes = 0;   // MsgPool_Moved: the system messages under a translation
    U image_base = span_words::kExeLo;
    bool code = false;            // code places to name: bof3x.exe, the table under a measured top byte
    // At a take (ReadTextSlots): (value, offset from kDataLo) of each of
    // Capcom's slots holding text of ours, by value - a word holding one of
    // these values is a copy of that slot's pointer (kWordTextCopy).
    std::vector<std::pair<U, U>> texts;
};

WordPlace LiveWordPlace() {
    WordPlace wp;
    std::uint32_t bytes = 0;
    const std::uint8_t* pool = MsgPool_Moved(&bytes);
    wp.pool = pool ? Va(pool) : 0;
    wp.pool_bytes = pool ? bytes : 0;
    wp.image_base = ImageBase();
    wp.code = Bind_Active() && Bind_TableMeasured();
    return wp;
}

// The words of the span that are the stack's or the process's, never named:
// the keep list (written back live after the span) and the scheduler's stack
// words, which a portable load writes fresh (Task_StackTop kept, each
// record's saved stack pointer from its new first frame).
bool Unnamed(U va) {
    for (std::size_t i = 0; i < kKeptCount; ++i)
        if (va >= kKept[i].at && va < kKept[i].at + kKept[i].bytes) return true;
    if (va == Task_StackTop_addr) return true;
    for (U k = 0; k < task_sched::kTasks; ++k)
        if (va == bof3::addr::Task_Records + k * task_sched::kRecordSize + task_sched::kSavedEsp) return true;
    return false;
}

static_assert(span_words::kTextWords[0] == Msgbox_Base_addr && span_words::kTextWords[1] == MenuWindows_ListText_addr &&
                  span_words::kTextWords[2] == Msgbox_Resume_addr && span_words::kTextWords[3] == BattleFlow_TaskCurrent_addr + 0x38,
              "span_words::kTextWords against symbols.toml (the box state's three, the battle block's +0x38)");

// The text slots as they stand, into wp.texts (a take's, and the probe's).
void ReadTextSlots(WordPlace& wp) {
    wp.texts.clear();
    const U data = Img(span_words::kDataLo);
    for (const U off : span_words::g_slots) {
        if (Unnamed(span_words::kDataLo + off)) continue;
        U w;
        std::memcpy(&w, At(data + off), sizeof w);
        const span_words::Section* s = span_words::g_ours.Named(w);
        if (s && s->content) wp.texts.push_back({w, off});
    }
    std::sort(wp.texts.begin(), wp.texts.end());
}

// The bytes of text at `v`, a pointer into a section of ours, up to its NUL
// (which a cursor at the end names) and at most 255: the extent a copy of the
// pointer may lie in. 0 when `v` names no content section of ours.
U TextExtent(U v) {
    const span_words::Section* s = span_words::g_ours.Named(v);
    if (!s || !s->content) return 0;
    const U room = s->hi - v < 255 ? s->hi - v : 255;
    const auto* p = reinterpret_cast<const char*>(At(v));
    U n = 0;
    while (n < room && p[n] != 0) ++n;
    return n;
}

// The slot whose text `w` points into - at its start, or inside it up to its
// NUL, as the effect objects' cursors into DIV-0064's lines do
// (masterAndManillo) - as an offset from kDataLo, and how far in; false if
// none. The greatest text start at or below `w` is the one tried.
bool TextSlotOf(const WordPlace& wp, U w, U* slot, U* delta) {
    auto it = std::upper_bound(wp.texts.begin(), wp.texts.end(), std::pair<U, U>{w, ~U{0}});
    if (it == wp.texts.begin()) return false;
    --it;
    const U d = w - it->first;
    if (d > TextExtent(it->first)) return false;
    *slot = it->second;
    *delta = d;
    return true;
}

// The word at `off` (bytes from .data's start) holding `w`, at a take: true
// with its kind and the value the file holds for it when it is written by
// its name; false to write it raw. `np` is set (when 0) to the reason the
// word makes the take this build's only.
bool NameWord(const WordPlace& wp, U off, U w, U* kind, U* stored, U* np) {
    const U va = span_words::kDataLo + off;
    // A run-time slot or a text word into the message pool: by its offset in
    // the pool. A copy of a text slot's pointer, in any word that is not a
    // slot itself: by that slot (B1b; docs/state-by-name-participants.md 10).
    bool runtime = span_words::IsTextWord(va);
    for (U i = 0; i < span_words::kRuntimeSlots && !runtime; ++i) runtime = va == span_words::RuntimeSlot(i);
    if (runtime && wp.pool_bytes && w - wp.pool < wp.pool_bytes) {
        *kind = kWordPool;
        *stored = w - wp.pool;
        return true;
    }
    U slot = 0, delta = 0;
    if (!span_words::IsSlot(off) && TextSlotOf(wp, w, &slot, &delta)) {
        *kind = kWordTextCopy;
        *stored = delta << 24 | slot;
        return true;
    }
    if (runtime && span_words::g_ours.Named(w) && !*np) *np = span_words::IsTextWord(va) ? kNpTextOurs : kNpRuntimeSlot;
    if (wp.code) {
        if (const U named = Bind_VaOf(w)) {
            *kind = kWordCode;
            *stored = named;
            return true;
        }
    }
    if (wp.image_base != span_words::kExeLo) {
        const U folds[1] = {wp.image_base};
        U c;
        if (span_words::Fold(folds, 1, w, &c)) {
            *kind = kWordFold;
            *stored = c;
            return true;
        }
        if (span_words::Fold24(folds, 1, w, &c)) {
            *kind = kWordFold24;
            *stored = c;
            return true;
        }
    }
    if (span_words::IsSlot(off)) {
        if (const span_words::Section* s = span_words::g_ours.Named(w)) {
            if (s->content) {
                *kind = kWordText;
                *stored = w;
                return true;
            }
            if (!*np) *np = kNpSlotCode;
        }
    }
    return false;
}

// A WORDS entry back as this process places it: `stored` is the file's
// value, `live` the word this process holds there now. A text slot is the
// file's raw value at an exact load and the process's own at a portable one
// (`keep_live`), which must then be text of ours. False with `why` when this
// process has no place for the name.
bool PlaceWord(const WordPlace& wp, U kind, U stored, U live, bool keep_live, U* w, std::string& why) {
    switch (kind) {
    case kWordCode:
        if (!Bind_Ours(stored)) {
            why = Printf("a code word names 0x%06X, which this build does not bind", stored);
            return false;
        }
        *w = ::bof3::Code(stored);
        return true;
    case kWordFold:
        if (stored - span_words::kExeLo >= span_words::kImageExtent) break;
        *w = wp.image_base + (stored - span_words::kExeLo);
        return true;
    case kWordFold24: {
        const U low = (stored & 0xFFFFFFu) - span_words::kExeLo;
        if (low >= span_words::kImageExtent || wp.image_base + low > 0xFFFFFFu) break;
        *w = (stored & 0xFF000000u) | (wp.image_base + low);
        return true;
    }
    case kWordPool:
        if (stored >= wp.pool_bytes) {
            why = Printf("a message slot names byte 0x%X of a message pool of 0x%X bytes here", stored, wp.pool_bytes);
            return false;
        }
        *w = wp.pool + stored;
        return true;
    case kWordText:
        if (!keep_live) {
            *w = stored;
            return true;
        }
        if (const span_words::Section* s = span_words::g_ours.Named(live); !s || !s->content) {
            why = Printf("a slot of Capcom's held text of ours at the take and holds 0x%08X here", live);
            return false;
        }
        *w = live;
        return true;
    default:
        break;
    }
    why = Printf("a WORDS entry of kind %u holding 0x%08X has no place here", kind, stored);
    return false;
}

// --- format 2: where each task stands (task_yield.h) ---------------------------

unsigned char* TaskRecord(U k) { return At(Img(bof3::addr::Task_Records) + k * task_sched::kRecordSize); }

std::uint16_t U16(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U U32(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// A fresh first frame's saved stack pointer for task k under `top`
// (Task_Create, task.cpp: the architecture's slice and first frame).
U FirstFrame(U top, U k) { return top - k * task_sched::kStackSize - task_sched::kFirstFrame; }

// Task k's place at a take, from its record and, for a task created and not
// yet run, the entry word on its slice (`top` the arena's top). `np` is set
// (when 0) to why the task makes the take this build's only.
TaskRec TaskPlace(U k, U top, U* np) {
    TaskRec t{};
    const unsigned char* rec = TaskRecord(k);
    const std::uint16_t state = U16(rec + task_sched::kState);
    if (state == 0) return t;   // kTaskFree
    const U sp = U32(rec + task_sched::kSavedEsp);
    if (state != task_sched::kSleeping && sp == FirstFrame(top, k)) {
        // Created (Task_Create, Task_Restart) and not yet run: the entry is
        // what the scheduler's ret pops, a pointer's width.
        std::uintptr_t entry = 0;
        std::memcpy(&entry, At(sp + task_sched::kEntrySlot), sizeof entry);
        const U va = static_cast<std::uint64_t>(entry) >> 32 ? 0 : Bind_VaOf(static_cast<U>(entry));
        if (va) {
            t.kind = kTaskCreated;
            t.name = va;
            return t;
        }
        t.kind = kTaskNowhere;
        if (!*np) *np = kNpTaskEntry | (k << 8);
        return t;
    }
    const task_yield::Entry& e = task_yield::g_entries[k];
    if (e.site != 0 && e.bytes <= task_yield::kLocalsMax) {
        t.kind = kTaskSite;
        t.name = e.site;
        t.bytes = e.bytes;
        std::memcpy(t.locals, e.locals, e.bytes);
        return t;
    }
    t.kind = kTaskNowhere;
    if (!*np) *np = kNpTaskNowhere | (k << 8);
    return t;
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
// The state-by-name round's A4 (ledger draft C, game/save_tail.h): the tail
// after the block holds the came-from place, as F12's save and a save point's
// hold it, written in the copy (the take's live words are the game's).
void GameSaveHalf(unsigned char* to) {
    Rest2c_Slot = 0;
    orig::Save_BuildBlock();
    std::memcpy(to, Save_Staging, kSaveHalf);
    SaveTail_Write(to + save_tail::kBlockBytes);
}

// --- BOF3X_QSWORDS: the census of words that name our module ---------------
//
// A measurement (docs/state-by-name-participants.md 10): with BOF3X_QSWORDS
// naming a file, at every pass of Snapshot_Pending - the take's point between
// two logic frames, whether or not a take is asked - the aligned words of the
// span that a take would write raw and whose value lies in one of our
// module's sections (span_words::g_ours.Named). Written as changes, so a word
// that holds one number all run is one line: a pass is "F <Frame_Counter>
// <words now>", then "+ <va> <value> <k>" for a word that came or changed and
// "- <va>" for one that went; k = 0 for .data / .bss (NameWord left it raw),
// 1 for the span's tail past .data (our end of .game and the pool, which no
// rule names). Nothing in the game is changed; off when the variable is unset.
std::FILE* g_words_probe = nullptr;
bool g_words_probe_read = false;
std::vector<U> g_words_last;   // the last pass's (va, value, k), ascending by va

void WordsProbe() {
    if (!g_words_probe_read) {
        g_words_probe_read = true;
        char path[512];
        const U n = bof3::os::GetEnv("BOF3X_QSWORDS", path, sizeof path);
        if (n == 0 || n >= sizeof path) return;
        g_words_probe = std::fopen(path, "w");
        if (!g_words_probe) Fatal("quicksave: BOF3X_QSWORDS=%s cannot be written", path);
        Log("quicksave   BOF3X_QSWORDS: the words that name our module, each pass, to %s", path);
    }
    if (!g_words_probe || !span_words::g_slots_taken) return;
    WordPlace wp = LiveWordPlace();
    ReadTextSlots(wp);
    const U data = Img(span_words::kDataLo);
    std::vector<U> now;
    for (U off = 0; off < span_words::kDataHi - span_words::kDataLo; off += 4) {
        if (Unnamed(span_words::kDataLo + off)) continue;
        const U w = U32(At(data + off));
        if (!span_words::g_ours.Named(w)) continue;
        U kind = 0, value = 0, np = 0;
        if (NameWord(wp, off, w, &kind, &value, &np)) continue;
        now.insert(now.end(), {span_words::kDataLo + off, w, 0});
    }
    for (U va = span_words::kDataHi; va < exe_layout::kPoolVa + exe_layout::kPoolSize; va += 4) {
        const U w = U32(At(Img(va)));
        if (span_words::g_ours.Named(w)) now.insert(now.end(), {va, w, 1});
    }
    std::fprintf(g_words_probe, "F %u %u\n", static_cast<unsigned>(Frame_Counter), static_cast<unsigned>(now.size() / 3));
    std::size_t i = 0, j = 0;
    const std::vector<U>& was = g_words_last;
    while (i < now.size() || j < was.size()) {
        if (j >= was.size() || (i < now.size() && now[i] < was[j])) {
            std::fprintf(g_words_probe, "+ %08X %08X %u\n", now[i], now[i + 1], now[i + 2]);
            i += 3;
        } else if (i >= now.size() || was[j] < now[i]) {
            std::fprintf(g_words_probe, "- %08X\n", was[j]);
            j += 3;
        } else {
            if (now[i + 1] != was[j + 1]) std::fprintf(g_words_probe, "+ %08X %08X %u\n", now[i], now[i + 1], now[i + 2]);
            i += 3;
            j += 3;
        }
    }
    std::fflush(g_words_probe);
    g_words_last.swap(now);
}

// The file's bytes, the build fields left for Stamp. Between frames, on the
// main thread: participants first, then the span and the arena copied, then
// the save half built and the span written back over what building it wrote
// (the staging buffer, the live block's checksum word, a summary entry), so
// a take leaves the game's memory as it found it.
//
// Format 2 (the game's; `format` kFormat1 only for the self-test's file of the
// format before it): before the copy, each task's place (TaskPlace) and each
// word of .data written by its name (NameWord), read from the live span; the
// file's copy takes those values after the span is written back, so the live
// game never holds them.
std::vector<unsigned char> Take(const Registry& registry, const Span& span, U arena_top, SaveHalfFn save_half,
                                U format = kFormat) {
    std::vector<std::vector<unsigned char>> records(registry.size());
    for (std::size_t i = 0; i < registry.size(); ++i) {
        SnapshotWriter w(records[i]);
        registry[i].save(w);
    }
    const bool two = format == kFormat;
    HeaderExt ext{};
    TaskRec tasks[task_yield::kTasks] = {};
    std::string settings;
    std::vector<U> words, stored;
    if (two) {
        if (!span_words::g_slots_taken) Fatal("snapshot: a take before the slot census (span_words::BeforeInjects)");
        U np = kIsPortable;
        if (!IsExeHost() || !Bind_Active()) np = kNpHosted;
        else if (!Bind_TableMeasured()) np = kNpTableUnmeasured;
        for (U k = 0; k < task_yield::kTasks; ++k) tasks[k] = TaskPlace(k, arena_top, &np);
        WordPlace wp = LiveWordPlace();
        ReadTextSlots(wp);
        const U data = Img(span_words::kDataLo);
        // A word no rule names that holds an address in the message pool is
        // written raw, and logged: as a refusal it would cost a take whose
        // number happens to fall there (docs/state-by-name-participants.md
        // 10: one word, on 2,432 of cutsceneAndNue's 7,420 passes).
        U pool_raw = 0, pool_first = 0;
        for (U off = 0; off < span_words::kDataHi - span_words::kDataLo; off += 4) {
            if (Unnamed(span_words::kDataLo + off)) continue;
            U kind = 0, value = 0;
            const U w = U32(At(data + off));
            if (!NameWord(wp, off, w, &kind, &value, &np)) {
                if (wp.pool_bytes && w - wp.pool < wp.pool_bytes && !pool_raw++) pool_first = span_words::kDataLo + off;
                continue;
            }
            words.push_back((off >> 2) << 3 | kind);
            stored.push_back(value);
        }
        if (pool_raw)
            Log("quicksave   take: %u word(s) no rule names hold an address in the message pool, written raw (the first "
                "0x%08X); a portable load writes them as the take had them",
                pool_raw, pool_first);
        settings = Settings();
        ext.state_revision = kStateRevision;
        ext.portable = np == kIsPortable ? 1 : 0;
        ext.not_portable = np;
        ext.settings_fnv = FnvBytes(settings.data(), settings.size());
        ext.settings_bytes = static_cast<U>(settings.size());
        ext.tasks_bytes = sizeof tasks;
        ext.words = static_cast<U>(words.size());
        ext.table_lo = Bind_TableLo();
        BindPrint(&ext.bind_rows, &ext.bind_fnv);
        ext.image_base = ImageBase();
        ext.pointer_bytes = sizeof(void*);
    }
    const std::size_t head = sizeof(Header) + (two ? sizeof(HeaderExt) : 0);
    const U arena = ArenaCommitted(arena_top);
    std::size_t size = kSaveHalf + head + span.image.bytes + span.pool.bytes + arena;
    if (two) size += ext.tasks_bytes + ext.settings_bytes + std::size_t{ext.words} * sizeof(U);
    for (const auto& r : records) size += sizeof(RecordHead) + r.size();
    std::vector<unsigned char> file(size);

    unsigned char* p = file.data() + kSaveHalf + head;
    unsigned char* const image = p;
    std::memcpy(p, At(span.image.base), span.image.bytes);
    p += span.image.bytes;
    if (span.pool.bytes) std::memcpy(p, At(span.pool.base), span.pool.bytes);
    p += span.pool.bytes;
    if (arena) std::memcpy(p, At(arena_top - arena), arena);
    p += arena;
    if (two) {
        std::memcpy(p, tasks, sizeof tasks);
        p += sizeof tasks;
        if (!settings.empty()) std::memcpy(p, settings.data(), settings.size());
        p += settings.size();
        if (!words.empty()) std::memcpy(p, words.data(), words.size() * sizeof(U));
        p += words.size() * sizeof(U);
    }
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
    // The words by their names, into the file's copy only (the span's offset
    // of .data's start is 0: the span begins at .data in both hosts).
    static_assert(span_words::kDataLo == exe_layout::kDataVa, "the span starts at .data");
    for (std::size_t i = 0; i < words.size(); ++i) std::memcpy(image + (words[i] >> 3) * 4, &stored[i], sizeof(U));

    auto* h = reinterpret_cast<Header*>(file.data() + kSaveHalf);
    std::memcpy(h->magic, kMagic, sizeof kMagic);
    h->format = format;
    h->header_bytes = static_cast<U>(head);
    if (two) std::memcpy(file.data() + kSaveHalf + sizeof(Header), &ext, sizeof ext);
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
    h->rdata_fnv = Fnv(Rdata());
    h->participants = static_cast<U>(registry.size());
    return file;
}

// --- the read ----------------------------------------------------------------

// The three answers to a load (docs/state-by-name-round.md 3.1;
// docs/state-by-name-format.md 5).
enum class Answer {
    Refused,    // nothing is written; `offer_save` says whether the file's save half is whole
    Exact,      // the build and the process layout that wrote it: the arena written back, as format 1
    Portable,   // another build or process: the span and the records, each task started again at its place
};

struct Parsed {
    bool save_half = false;   // the first 0x12B0 bytes are there: a save file to the menu's reader
    bool save_whole = false;  // and their block's sum is right (SaveHalfWhole): a refusal offers the save
    bool ok = false;          // the snapshot half may be restored (answer is not Refused)
    Answer answer = Answer::Refused;
    bool offer_save = false;  // refused, and the save half is whole: the menu may load it as a save (group A4)
    std::string why;          // short: the overlay's refusal
    std::string detail;       // the log's
    Header header{};
    HeaderExt ext{};          // format 2's; zero for a format-1 file
    const unsigned char* image = nullptr;   // the span to write: the file's (format 1) or `placed` (format 2)
    const unsigned char* pool = nullptr;
    const unsigned char* arena = nullptr;
    std::vector<unsigned char> placed;      // format 2: the span with every named word placed for this process
    TaskRec tasks[task_yield::kTasks] = {};
    struct Rec {
        std::size_t participant;
        const unsigned char* data;
        U bytes;
    };
    std::vector<Rec> records;
};

Parsed Refuse(Parsed p, const std::string& why, const std::string& detail) {
    p.ok = false;
    p.answer = Answer::Refused;
    p.offer_save = p.save_whole;
    p.why = why;
    p.detail = detail;
    return p;
}

std::string Field(const char* s, std::size_t n) { return std::string(s, strnlen(s, n)); }

// The participants' records from `at` to the file's end, each matched to its
// participant and checked by it (a refusal here writes nothing); and every
// participant has one. True, or `p` refused.
bool ParseRecords(Parsed& p, const std::vector<unsigned char>& file, std::size_t at, const Registry& registry) {
    const auto refuse = [&p](const std::string& why, const std::string& detail) {
        p = Refuse(p, why, detail);
        return false;
    };
    std::vector<bool> seen(registry.size(), false);
    for (U i = 0; i < p.header.participants; ++i) {
        RecordHead rh;
        if (file.size() - at < sizeof rh) return refuse("truncated", Printf("record %u's head runs past the file", i));
        std::memcpy(&rh, file.data() + at, sizeof rh);
        at += sizeof rh;
        if (file.size() - at < rh.bytes) return refuse("truncated", Printf("record %u's bytes run past the file", i));
        const std::string tag = Field(rh.tag, sizeof rh.tag);
        std::size_t k = 0;
        while (k < registry.size() && registry[k].tag != tag) ++k;
        if (k == registry.size())
            return refuse("a record with no participant",
                              Printf("record '%s' (%u bytes) has no participant in this build: refused rather than restored in part",
                                     tag.c_str(), rh.bytes));
        if (seen[k]) return refuse("a record twice", Printf("record '%s' appears twice", tag.c_str()));
        seen[k] = true;
        if (registry[k].check) {
            SnapshotReader reader(registry[k].tag.c_str(), file.data() + at, rh.bytes);
            std::string why, detail;
            if (!registry[k].check(reader, why, detail))
                return refuse(why, Printf("record '%s': %s", tag.c_str(), detail.c_str()));
        }
        p.records.push_back({k, file.data() + at, rh.bytes});
        at += rh.bytes;
    }
    if (at != file.size())
        return refuse("trailing bytes", Printf("%u bytes after the last record", (unsigned)(file.size() - at)));
    for (std::size_t k = 0; k < registry.size(); ++k)
        if (!seen[k])
            return refuse("a participant with no record",
                              Printf("participant '%s' has no record in the file: refused rather than restored in part",
                                     registry[k].tag.c_str()));
    return true;
}

bool SameSpan(const Header& h, const Span& span) {
    return h.span_base == span.image.base && h.span_bytes == span.image.bytes && h.pool_bytes == span.pool.bytes &&
           (!span.pool.bytes || h.pool_base == span.pool.base);
}

std::string SpanText(const Header& h, const Span& span) {
    return Printf("span 0x%08X+0x%X, pool 0x%08X+0x%X; here 0x%08X+0x%X, 0x%08X+0x%X", h.span_base, h.span_bytes, h.pool_base,
                  h.pool_bytes, span.image.base, span.image.bytes, span.pool.base, span.pool.bytes);
}

// Format 1, as every build before format 2 read it: this build and process
// only, the arena written back. Another build's is refused, its save offered
// (docs/state-by-name-round.md 7: a format-1 file loads exactly in its own
// build and as its save elsewhere).
Parsed ParseFormat1(Parsed p, const std::vector<unsigned char>& file, const BuildId& build, const Span& span, U arena_top,
                    const Registry& registry) {
    const Header& h = p.header;
    if (h.header_bytes != sizeof(Header))
        return Refuse(p, "another format", Printf("snapshot format 1 with a header of %u bytes, not %u", h.header_bytes,
                                                  (unsigned)sizeof(Header)));
    if (h.file_bytes != file.size())
        return Refuse(p, "truncated", Printf("the header says %u bytes, the file has %u", h.file_bytes, (unsigned)file.size()));
    if (h.host != build.host)
        return Refuse(p, "another host", Printf("made under %s, this is %s", h.host == kHostExe ? "bof3x.exe" : "BOF3.exe",
                                                build.host == kHostExe ? "bof3x.exe" : "BOF3.exe"));
    if (std::memcmp(h.tag, build.tag, sizeof h.tag) != 0 || std::memcmp(h.dll_sha256, build.sha, sizeof h.dll_sha256) != 0)
        return Refuse(p, "another build",
                      Printf("format 1, made by %s (bof3x.dll %.12s...), this is %s (%.12s...); the save half is still a save",
                             Field(h.tag, sizeof h.tag).c_str(), h.dll_sha256, Field(build.tag, sizeof build.tag).c_str(), build.sha));
    if (h.dll_base != build.dll_base)
        return Refuse(p, "another process",
                      Printf("bof3x.dll was at 0x%08X, here at 0x%08X: the task stacks name its code where it was", h.dll_base,
                             build.dll_base));
    if (!SameSpan(h, span)) return Refuse(p, "another span", SpanText(h, span));
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
    if (!ParseRecords(p, file, at, registry)) return p;
    if (!Committed(span.image.base, span.image.bytes) || (span.pool.bytes && !Committed(span.pool.base, span.pool.bytes)))
        return Refuse(p, "the span is not mapped", "a page of the span is not committed");
    p.ok = true;
    p.answer = Answer::Exact;
    return p;
}

// BOF3X_QSPORTABLE=1, the developer's switch (docs/state-by-name-sites.md 4):
// F7 takes a file it could restore exactly down the portable road instead, so
// that one build shows its resume entries equal to its stacks (the proof's
// first row, docs/state-by-name-round.md 5). Set by DoLoad alone, so the
// self-test's Parse calls never see it.
bool g_force_portable = false;

// Everything a restore needs, checked before a byte is written: a refusal
// here leaves the game exactly as it was. Format 2's order: the revision, the
// host, the settings, the span, the records; then exact when the build and
// the process layout are the take's, else portable when the file is and this
// build can place every task and every named word, else refused.
Parsed Parse(const std::vector<unsigned char>& file, const BuildId& build, const Span& span, U arena_top,
             const Registry& registry) {
    Parsed p;
    if (file.size() < kSaveHalf) return Refuse(p, "not a save file", Printf("%u bytes, under a save file's 0x%X", (unsigned)file.size(), kSaveHalf));
    p.save_half = true;
    p.save_whole = SaveHalfWhole(file.data(), file.size());
    if (file.size() < kSaveHalf + sizeof(Header)) return Refuse(p, "no snapshot", "a save file with no snapshot after it");
    std::memcpy(&p.header, file.data() + kSaveHalf, sizeof(Header));
    const Header& h = p.header;
    if (std::memcmp(h.magic, kMagic, sizeof kMagic) != 0) return Refuse(p, "no snapshot", "no BOF3XQS header after the save half");
    if (h.format == kFormat1) return ParseFormat1(std::move(p), file, build, span, arena_top, registry);
    if (h.format != kFormat || h.header_bytes != sizeof(Header) + sizeof(HeaderExt))
        return Refuse(p, "another format", Printf("snapshot format %u (header %u bytes); this build reads %u (%u) and 1",
                                                  h.format, h.header_bytes, kFormat, (unsigned)(sizeof(Header) + sizeof(HeaderExt))));
    if (h.file_bytes != file.size())
        return Refuse(p, "truncated", Printf("the header says %u bytes, the file has %u", h.file_bytes, (unsigned)file.size()));
    std::memcpy(&p.ext, file.data() + kSaveHalf + sizeof(Header), sizeof(HeaderExt));
    const HeaderExt& x = p.ext;
    if (x.state_revision != kStateRevision)
        return Refuse(p, "another state revision",
                      Printf("state revision %u; this build reads %u (raised by %s) and converts none", x.state_revision,
                             kStateRevision, kRevisions[sizeof kRevisions / sizeof kRevisions[0] - 1].raised_by));
    if (h.host != build.host)
        return Refuse(p, "another host", Printf("made under %s, this is %s", h.host == kHostExe ? "bof3x.exe" : "BOF3.exe",
                                                build.host == kHostExe ? "bof3x.exe" : "BOF3.exe"));
    std::size_t at = kSaveHalf + h.header_bytes;
    const std::size_t fixed = std::size_t{h.span_bytes} + h.pool_bytes + h.arena_bytes;
    if (x.tasks_bytes != sizeof p.tasks) return Refuse(p, "another format", Printf("a TASKS record of %u bytes, not %u", x.tasks_bytes, (unsigned)sizeof p.tasks));
    const std::size_t sections = fixed + x.tasks_bytes + x.settings_bytes + std::size_t{x.words} * sizeof(U);
    if (file.size() - at < sections) return Refuse(p, "truncated", "the span, the arena, the tasks, the settings and the words run past the file");
    const unsigned char* const image = file.data() + at;
    p.pool = image + h.span_bytes;
    p.arena = p.pool + h.pool_bytes;
    at += fixed;
    std::memcpy(p.tasks, file.data() + at, sizeof p.tasks);
    at += x.tasks_bytes;
    const std::string settings(reinterpret_cast<const char*>(file.data() + at), x.settings_bytes);
    at += x.settings_bytes;
    const unsigned char* const words = file.data() + at;
    at += std::size_t{x.words} * sizeof(U);
    if (FnvBytes(settings.data(), settings.size()) != x.settings_fnv)
        return Refuse(p, "damaged", "the settings' lines do not hash to the header's fingerprint");
    {
        const std::string here = Settings();
        if (FnvBytes(here.data(), here.size()) != x.settings_fnv) {
            std::string detail;
            const std::string names = SettingsDiff(settings, here, detail);
            return Refuse(p, "other settings (" + names + ")", "taken under other settings: " + detail);
        }
    }
    if (!SameSpan(h, span)) return Refuse(p, "another span", SpanText(h, span));
    if (!ParseRecords(p, file, at, registry)) return p;
    if (!Committed(span.image.base, span.image.bytes) || (span.pool.bytes && !Committed(span.pool.base, span.pool.bytes)))
        return Refuse(p, "the span is not mapped", "a page of the span is not committed");

    // Exact, or portable, or neither.
    const bool same_build =
        std::memcmp(h.tag, build.tag, sizeof h.tag) == 0 && std::memcmp(h.dll_sha256, build.sha, sizeof h.dll_sha256) == 0;
    std::string reason, detail;
    if (!same_build) {
        reason = "another build";
        detail = Printf("made by %s (bof3x.dll %.12s...), this is %s (%.12s...)", Field(h.tag, sizeof h.tag).c_str(), h.dll_sha256,
                        Field(build.tag, sizeof build.tag).c_str(), build.sha);
    } else if (h.dll_base != build.dll_base) {
        reason = "another process";
        detail = Printf("bof3x.dll was at 0x%08X, here at 0x%08X: the task stacks name its code where it was", h.dll_base, build.dll_base);
    } else if (h.arena_top != arena_top || h.arena_bytes > kArenaBytes) {
        reason = "another process";
        detail = Printf("the task stacks hung from 0x%08X (0x%X bytes), here from 0x%08X", h.arena_top, h.arena_bytes, arena_top);
    } else if (x.table_lo != Bind_TableLo()) {
        reason = "another process";
        detail = Printf("the code places lay at 0x%08X, here at 0x%08X: the task stacks hold them", x.table_lo, Bind_TableLo());
    } else if (x.image_base != ImageBase()) {
        reason = "another process";
        detail = Printf("the image lay at 0x%08X, here at 0x%08X", x.image_base, ImageBase());
    }
    if (reason.empty() && g_force_portable) {   // BOF3X_QSPORTABLE (DoLoad): the developer's proof of the resume entries
        reason = "forced portable";
        detail = "BOF3X_QSPORTABLE=1: a file this build and process could restore exactly, taken the portable road";
    }
    if (reason.empty()) {
        p.answer = Answer::Exact;
    } else {
        if (!x.portable)
            return Refuse(p, reason + " (not portable)",
                          detail + Printf("; not portable: %s", NotPortableText(x.not_portable)) +
                              ((x.not_portable & 0xFF) == kNpTaskNowhere || (x.not_portable & 0xFF) == kNpTaskEntry
                                   ? Printf(" (task %u)", x.not_portable >> 8)
                                   : std::string()));
        if (!IsExeHost() || !Bind_Active() || !Bind_TableMeasured())
            return Refuse(p, reason, detail + "; this process cannot place a portable quicksave's code words (the hosted build, "
                                              "or the code places under an unmeasured top byte)");
        U rows = 0, fnv = 0;
        BindPrint(&rows, &fnv);
        if (rows != x.bind_rows || fnv != x.bind_fnv)
            return Refuse(p, reason + " (other functions bound)",
                          detail + Printf("; the file's build binds %u VAs (FNV %08X), this one %u (%08X)", x.bind_rows, x.bind_fnv, rows, fnv));
        for (U k = 0; k < task_yield::kTasks; ++k) {
            const TaskRec& t = p.tasks[k];
            if (t.kind == kTaskFree) continue;
            if (t.kind == kTaskCreated) {
                if (!Bind_Ours(t.name))
                    return Refuse(p, reason + " (a task this build cannot start)",
                                  detail + Printf("; task %u's entry 0x%06X has no function of ours here", k, t.name));
                continue;
            }
            const task_yield::Site* site = t.kind == kTaskSite ? task_yield::FindSite(t.name) : nullptr;
            if (!site)
                return Refuse(p, reason + " (a task at an unknown place)",
                              detail + Printf("; task %u stands at site %u, which this build does not list (kind %u)", k, t.name, t.kind));
            if (t.bytes != site->bytes)
                return Refuse(p, reason + " (a task at an unknown place)",
                              detail + Printf("; task %u's site %u (%s) holds %u bytes of locals, this build's %u", k, t.name,
                                              site->name, t.bytes, site->bytes));
        }
        p.answer = Answer::Portable;
    }

    // The named words placed for this process, into a copy of the span.
    p.placed.assign(image, image + h.span_bytes);
    const WordPlace wp = LiveWordPlace();
    const bool portable = p.answer == Answer::Portable;
    const U data = Img(span_words::kDataLo);
    for (U i = 0; i < x.words; ++i) {
        const U e = U32(words + std::size_t{i} * sizeof(U));
        const U off = (e >> 3) * 4, kind = e & 7;
        if (off + 4 > span_words::kDataHi - span_words::kDataLo || off + 4 > h.span_bytes || Unnamed(span_words::kDataLo + off))
            return Refuse(p, "damaged", Printf("WORDS entry %u names offset 0x%X, outside the words a take names", i, off));
        U w = 0;
        std::string why;
        U place_kind = kind, file_value = U32(p.placed.data() + off), live_value = U32(At(data + off)), delta = 0;
        if (kind == kWordTextCopy) {   // placed from the slot it points into: the file's slot at an exact load, the process's at a portable one
            const U slot = file_value & 0xFFFFFFu;
            delta = file_value >> 24;
            if (!span_words::IsSlot(slot) || slot + 4 > h.span_bytes)
                return Refuse(p, "damaged", Printf("WORDS entry %u copies offset 0x%X, which is no slot of Capcom's", i, slot));
            place_kind = kWordText;
            file_value = U32(image + slot);
            live_value = U32(At(data + slot));
        }
        const auto cannot = [&](const std::string& what) {
            return Refuse(p, reason.empty() ? std::string("a word this process cannot place") : reason + " (a word this build cannot place)",
                          (detail.empty() ? std::string() : detail + "; ") + Printf("the word at 0x%08X: ", span_words::kDataLo + off) + what);
        };
        if (!PlaceWord(wp, place_kind, file_value, live_value, portable, &w, why)) return cannot(why);
        if (kind == kWordTextCopy) {
            if (portable && delta > TextExtent(w))
                return cannot(Printf("it lay 0x%X bytes into a slot's text, and the text here is 0x%X bytes", delta, TextExtent(w)));
            w += delta;
        }
        std::memcpy(p.placed.data() + off, &w, sizeof w);
    }
    p.image = p.placed.data();
    p.ok = true;
    return p;
}

// --- the restore -------------------------------------------------------------

// A portable load's tasks (docs/state-by-name-round.md 3.3; task_yield.h):
// for each task not free, a fresh first frame on its slice under this
// process's Task_StackTop, made by Task_Create itself, entered at the code
// place of the VA its entry named (a task created and not yet run) or at its
// site's resume entry; then the record's state put back as the file has it
// (Task_Create makes it runnable; the sleep word it leaves alone), so a task
// that slept n frames wakes in n. The yield entries become the file's, which
// a resume entry reads. Nothing of the file's arena is read.
void PlaceTasks(const Parsed& p) {
    for (U k = 0; k < task_yield::kTasks; ++k) {
        task_yield::g_entries[k] = task_yield::Entry{};
        const TaskRec& t = p.tasks[k];
        if (t.kind == kTaskFree) continue;
        void* entry = nullptr;
        if (t.kind == kTaskCreated) {
            entry = reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(t.name)));
        } else {
            const task_yield::Site* site = task_yield::FindSite(t.name);
            if (t.kind != kTaskSite || !site) Fatal("snapshot: task %u stands at site %u (kind %u), checked before the restore began", k, t.name, t.kind);
            entry = reinterpret_cast<void*>(site->resume);
        }
        unsigned char* rec = TaskRecord(k);
        const std::uint16_t state = U16(rec + task_sched::kState);
        ::Task_Create(static_cast<int>(k), entry);   // which clears the slot's entry (task_yield.h item 2)
        std::memcpy(rec + task_sched::kState, &state, sizeof state);
        if (t.kind == kTaskSite) {   // so the site's entry is written after it, for the resume entry to read
            task_yield::g_entries[k].site = t.name;
            task_yield::g_entries[k].bytes = t.bytes;
            std::memcpy(task_yield::g_entries[k].locals, t.locals, t.bytes);
        }
    }
}

// A parsed file into the process: the span (the keep list's live bytes put
// back over it), every participant in the file's order, and the tasks LAST -
// on a stack of its own (OnOwnStack), so nothing after it writes into the
// slices it has just written. Exact: the task arena as the file has it (it
// must be ready, ArenaReady - the caller's check, so a refusal there still
// leaves everything untouched) and the yield entries the file's. Portable:
// Task_StackTop kept as the process has it (the text slots of ours are
// already the process's in `placed`: PlaceWord), then PlaceTasks.
void Restore(const Parsed& p, const Registry& registry) {
    const Header& h = p.header;
    const bool portable = p.answer == Answer::Portable;
    if (p.answer == Answer::Refused) Fatal("snapshot: a refused file handed to the restore");
    for (const Parsed::Rec& r : p.records)   // the live process's resources let go while its words still name them
        if (registry[r.participant].discard) registry[r.participant].discard();
    std::vector<std::vector<unsigned char>> live(kKeptCount);
    for (std::size_t i = 0; i < kKeptCount; ++i)
        live[i].assign(At(Img(kKept[i].at)), At(Img(kKept[i].at) + kKept[i].bytes));
    const U stack_top = Task_StackTop;
    WriteRange(h.span_base, p.image, h.span_bytes);
    if (h.pool_bytes) WriteRange(h.pool_base, p.pool, h.pool_bytes);
    for (std::size_t i = 0; i < kKeptCount; ++i) WriteRange(Img(kKept[i].at), live[i].data(), kKept[i].bytes);
    if (portable) WriteRange(Img(Task_StackTop_addr), reinterpret_cast<const unsigned char*>(&stack_top), sizeof stack_top);
    for (const Parsed::Rec& r : p.records) {
        const Participant& who = registry[r.participant];
        SnapshotReader reader(who.tag.c_str(), r.data, r.bytes);
        who.restore(reader);
        if (reader.Left() != 0)
            Fatal("snapshot: participant '%s' restored from %u of its record's %u bytes: a partial restore", who.tag.c_str(),
                  r.bytes - static_cast<U>(reader.Left()), r.bytes);
    }
    if (portable) {
        PlaceTasks(p);
        return;
    }
    if (h.arena_bytes) std::memcpy(At(h.arena_top - h.arena_bytes), p.arena, h.arena_bytes);
    for (U k = 0; k < task_yield::kTasks; ++k) {
        task_yield::Entry e{};
        const TaskRec& t = p.tasks[k];   // all free in a format-1 file: no entries
        if (t.kind == kTaskSite) {
            e.site = t.name;
            e.bytes = t.bytes;
            std::memcpy(e.locals, t.locals, t.bytes <= task_yield::kLocalsMax ? t.bytes : task_yield::kLocalsMax);
        }
        task_yield::g_entries[k] = e;
    }
}

// --- the files ---------------------------------------------------------------
//
// Through the standard library and os/file_move.h. A folder is a
// std::filesystem::path: the game's is the empty one (the current folder,
// where the quicksaves always were), the self-test's a temporary folder.

namespace fs = std::filesystem;

fs::path SlotPath(const fs::path& dir, unsigned slot) {
    char name[32];
    std::snprintf(name, sizeof name, "BISLPS%02X.DAT", slot);
    return dir / name;
}
std::string SlotName(unsigned slot) { return Printf("BISLPS%02X.DAT", slot); }

// A path for a log line, as UTF-8.
std::string PathText(const fs::path& path) {
    try {
        const std::u8string u = path.u8string();
        return std::string(u.begin(), u.end());
    } catch (...) {
        return "(a name that does not convert)";
    }
}

bool Exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// The rename of one slot, the rotation's step: the target must not be there
// (os::MoveFileKeep). The self-test hands one of its own, to make a step fail.
using MoveFn = os::MoveResult (*)(const fs::path& from, const fs::path& to);
os::MoveResult SystemMove(const fs::path& from, const fs::path& to) { return os::MoveFileKeep(from, to); }

// F6's history (the owner, 2026-10-17): 1F deleted, then 1E -> 1F down to
// 10 -> 11, renames only, top down; a gap is passed over, so every file
// moves up one. A failure stops the rotation where it is: every file still
// present under some name, nothing half-moved. A rename never replaces: a
// file at the target (one made there since the step above moved it) stops
// the rotation, never lost.
//
// A slot file of zero bytes is no quicksave - none is ever written so:
// WriteNewest writes the temporary file whole and renames it - and it is what
// a crash inside os::MoveFileKeep's third way leaves at a step's target (its
// exclusive create done, the rename over it not; os/file_move.h). The seam
// refuses such a file as it refuses any file at a target (its own rule:
// "an empty file at the target is a file"); the rotation, which knows what a
// slot holds, removes it as a gap when its step comes, so it is neither moved
// up the history as an empty slot nor in the way of a later step (D431's
// open item, docs/linux-round.md "B2c as run").
bool EmptySlotFile(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && fs::file_size(path, ec) == 0 && !ec;
}

bool Rotate(const fs::path& dir, std::string& why, MoveFn move = &SystemMove) {
    const fs::path top = SlotPath(dir, kLastSlot);
    std::error_code ec;
    if (Exists(top) && !fs::remove(top, ec)) {
        why = Printf("%s could not be deleted (error %d)", SlotName(kLastSlot).c_str(), ec.value());
        return false;
    }
    for (unsigned slot = kLastSlot - 1; slot + 1 > kFirstSlot; --slot) {
        const fs::path from = SlotPath(dir, slot);
        if (!Exists(from)) continue;
        if (EmptySlotFile(from)) {
            if (!fs::remove(from, ec)) {
                why = Printf("%s is empty and could not be deleted (error %d)", SlotName(slot).c_str(), ec.value());
                return false;
            }
            Log("quicksave   %s was empty (no quicksave is; a crash in a rotation's rename leaves one): removed, a gap",
                SlotName(slot).c_str());
            continue;
        }
        const os::MoveResult moved = move(from, SlotPath(dir, slot + 1));
        if (moved != os::MoveResult::Moved) {
            why = Printf("%s could not be renamed to %s (%s, error %lu)", SlotName(slot).c_str(), SlotName(slot + 1).c_str(),
                         moved == os::MoveResult::TargetExists ? "the target is there" : "refused",
                         static_cast<unsigned long>(bof3::os::LastError()));
            return false;
        }
    }
    return true;
}

bool WriteAll(const fs::path& path, const std::vector<unsigned char>& bytes, std::string& why) {
    const os::OutFile f = os::OutCreate(path);
    if (f == os::kNoFile) {
        why = Printf("cannot create %s (error %lu)", PathText(path).c_str(), static_cast<unsigned long>(bof3::os::LastError()));
        return false;
    }
    bool ok = os::OutWrite(f, bytes.data(), bytes.size());
    // On the disk before the rename makes it the newest: a power cut after the
    // rename must not leave a slot of zeros (the review of PR #95).
    if (ok) ok = os::OutFlush(f);
    const unsigned long error = bof3::os::LastError();
    os::OutClose(f);
    if (!ok) {
        std::error_code ec;
        fs::remove(path, ec);
        // 0: the system took a part of the bytes and reported no error
        // (os/out_file.h), so there is no number to quote.
        why = error ? Printf("writing %s failed, %u bytes (error %lu)", PathText(path).c_str(), (unsigned)bytes.size(), error)
                    : Printf("writing %s failed, %u bytes (a short write, no error number)", PathText(path).c_str(),
                             (unsigned)bytes.size());
        return false;
    }
    return true;
}

// The newest quicksave: written whole under a temporary name first, so a
// failed write rotates nothing; then the rotation; then the rename to 10.
bool WriteNewest(const fs::path& dir, const std::vector<unsigned char>& bytes, std::string& why, MoveFn move = &SystemMove) {
    const fs::path tmp = dir / "BISLPS10.TMP";
    if (!WriteAll(tmp, bytes, why)) return false;
    if (!Rotate(dir, why, move)) {
        std::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
    if (os::MoveFileReplace(tmp, SlotPath(dir, kFirstSlot)) != os::MoveResult::Moved) {
        why = Printf("BISLPS10.TMP could not be renamed to %s (error %lu); the history moved up a slot", SlotName(kFirstSlot).c_str(),
                     static_cast<unsigned long>(bof3::os::LastError()));
        return false;
    }
    return true;
}

bool ReadSlot(const fs::path& dir, unsigned slot, std::vector<unsigned char>& bytes, std::string& why) {
    const fs::path path = SlotPath(dir, slot);
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec) {
        why = ec == std::errc::no_such_file_or_directory ? Printf("no quicksave in slot %X (%s)", slot, SlotName(slot).c_str())
                                                         : Printf("cannot open %s (error %d)", SlotName(slot).c_str(), ec.value());
        return false;
    }
    if (size > kMaxFile) {
        why = Printf("%s is larger than any quicksave", SlotName(slot).c_str());
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    // The stream opens through the C library, whose failure is in errno; the
    // system's last error is not the stream's to set. 0 if it left none.
    errno = 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        const int error = errno;
        why = error ? Printf("cannot open %s (errno %d)", SlotName(slot).c_str(), error)
                    : Printf("cannot open %s (no error number)", SlotName(slot).c_str());
        return false;
    }
    std::size_t got = 0;
    if (!bytes.empty()) {
        f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        got = static_cast<std::size_t>(f.gcount());
    }
    if (got != bytes.size()) {
        why = Printf("reading %s failed (%u of %u bytes)", SlotName(slot).c_str(), (unsigned)got, (unsigned)bytes.size());
        return false;
    }
    return true;
}

// A stack of its own (os/own_stack.h; on Windows a fiber of the calling
// thread, as the renderer's RunOnFiber and the MP3 decoder's DecodeFrame
// have, for the same reason): Snapshot_Pending runs in WinMain's
// loop, about 0x3E00 bytes above slice 0 of the task arena
// (docs/quicksave-census.md 2.1), and the take, the restore, the file layer,
// the DLL's hash and every participant's Save and Restore (DirectSound,
// Direct3D) are kept off that stack - a take must copy slice 0 as the tasks
// left it, and a restore must not overwrite it after. The x87 control word is
// carried over: the game's (SetX87), not the new stack's default. A thread
// made a fiber for the run is given back as it came (the renderer converts
// the main thread itself later). Measured for R7 (2026-11-18): no
// participant calls the renderer's present, set-up or frame save, so no run
// of the render stack nests inside this one; the seam would let it.
os::OwnStack g_own_stack{{256 * 1024, 1024 * 1024, true, true}};

void RunJob(void* job) { (*static_cast<const std::function<void()>*>(job))(); }

void OnOwnStack(const std::function<void()>& job) {
    const os::OwnStackResult r = g_own_stack.Run(&RunJob, const_cast<std::function<void()>*>(&job));
    if (r != os::OwnStackResult::Done) Fatal("snapshot: %s (%lu)", os::OwnStackFailure(r), static_cast<unsigned long>(bof3::os::LastError()));
}

// --- the keys ------------------------------------------------------------------

enum class Request { None, Take, Load };
volatile Request g_request = Request::None;
unsigned g_request_slot = kFirstSlot;
char g_overlay[128];
SnapshotLoadResult g_last_load;   // the last load's answer (Snapshot_LastLoad)

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
    if (!WriteNewest(fs::path(), file, why)) {
        Log("quicksave   F6 at Frame_Counter %u refused: %s", h->frame_counter, why.c_str());
        Show("Quicksave refused: " + why);
        return;
    }
    HeaderExt x;
    std::memcpy(&x, file.data() + kSaveHalf + sizeof(Header), sizeof x);
    Log("quicksave   F6 at Frame_Counter %u: BISLPS10.DAT, %u bytes (save half 0x%X, span 0x%08X+0x%X, pool 0x%08X+0x%X, "
        "task arena 0x%08X-0x%X, %u participants; .rdata fnv %08X); format %u, state revision %u, %u words by their names, "
        "settings %08X, %s%s",
        h->frame_counter, h->file_bytes, kSaveHalf, h->span_base, h->span_bytes, h->pool_base, h->pool_bytes, h->arena_top,
        h->arena_bytes, h->participants, h->rdata_fnv, h->format, x.state_revision, x.words, x.settings_fnv,
        x.portable ? "portable" : "not portable: ", x.portable ? "" : Printf("%s (task %u)", NotPortableText(x.not_portable), x.not_portable >> 8).c_str());
    Show("Quicksave OK");
}

// On the own stack; the arena already probed on the main thread's.
void DoLoad(unsigned slot) {
    std::vector<unsigned char> file;
    std::string why;
    const BuildId& build = EnsureBuild();
    g_last_load = SnapshotLoadResult{};
    g_last_load.slot = slot;
    if (!ReadSlot(fs::path(), slot, file, why)) {
        Log("quicksave   F7 refused: %s", why.c_str());
        Show("Quickload refused: " + why);
        g_last_load.answer = SnapshotAnswer::Refused;
        g_last_load.why = why;
        return;
    }
    const U top = static_cast<U>(Task_StackTop);
    {
        char v[4] = {};
        g_force_portable = bof3::os::GetEnv("BOF3X_QSPORTABLE", v, sizeof v) == 1 && v[0] == '1';
    }
    const Parsed p = Parse(file, build, CurrentSpan(), top, g_registry);
    g_force_portable = false;
    if (!p.ok) {
        Log("quicksave   F7 %s refused: %s - %s%s", SlotName(slot).c_str(), p.why.c_str(), p.detail.c_str(),
            p.offer_save ? " (the save half is whole: it can be loaded as a save)" : " (no whole save half)");
        Show("Quickload refused: " + p.why);
        g_last_load.answer = p.offer_save ? SnapshotAnswer::RefusedSaveOffered : SnapshotAnswer::Refused;
        g_last_load.why = p.why;
        g_last_load.detail = p.detail;
        return;
    }
    const bool portable = p.answer == Answer::Portable;
    if (!portable && !Committed(top - p.header.arena_bytes, p.header.arena_bytes)) {
        Log("quicksave   F7 %s refused: the task arena 0x%08X-0x%X is not committed", SlotName(slot).c_str(), top,
            p.header.arena_bytes);
        Show("Quickload refused: the task stacks");
        g_last_load.answer = p.save_whole ? SnapshotAnswer::RefusedSaveOffered : SnapshotAnswer::Refused;
        g_last_load.why = "the task stacks";
        return;
    }
    // .rdata is another build's at a portable load (one pointer into our
    // module, the import slots' places): compared only at an exact one.
    const U rdata = Fnv(Rdata());
    if (!portable && rdata != p.header.rdata_fnv)
        Log("quicksave   F7 %s: .rdata fnv %08X now, %08X at the take - written in play, and not copied (the census's "
            "premise for leaving it out does not hold)",
            SlotName(slot).c_str(), rdata, p.header.rdata_fnv);
    const U from = Frame_Counter;
    Restore(p, g_registry);
    if (portable) {
        std::string places;
        for (U k = 0; k < task_yield::kTasks; ++k) {
            const TaskRec& t = p.tasks[k];
            places += Printf("%s%u %s", k ? ", " : "", k,
                             t.kind == kTaskFree      ? "free"
                             : t.kind == kTaskCreated ? Printf("new at 0x%06X", t.name).c_str()
                                                      : Printf("site %u", t.name).c_str());
        }
        Log("quicksave   F7 %s restored PORTABLE at Frame_Counter %u: back to Frame_Counter %u (%u bytes; %u participants; "
            "%u words by their names; taken by %s, %u-bit; tasks %s)",
            SlotName(slot).c_str(), from, p.header.frame_counter, p.header.file_bytes, p.header.participants, p.ext.words,
            Field(p.header.tag, sizeof p.header.tag).c_str(), p.ext.pointer_bytes * 8, places.c_str());
    } else {
        Log("quicksave   F7 %s restored at Frame_Counter %u: back to Frame_Counter %u (%u bytes; format %u; %u participants; "
            ".rdata %s)",
            SlotName(slot).c_str(), from, p.header.frame_counter, p.header.file_bytes, p.header.format, p.header.participants,
            rdata == p.header.rdata_fnv ? "unchanged" : "CHANGED");
    }
    g_last_load.answer = portable ? SnapshotAnswer::Portable : SnapshotAnswer::Exact;
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
// words. A byte copy each, in fixed-width words; every way the record can
// differ from this process's is refused by StCheck before the span is
// written (another version, the ring's size, the language's pool), so
// StRestore's Fatals are not reached by a record that passed.
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

// Checked before a byte of the span is written: every condition StRestore
// fails on is a refusal here, so a record that passes reaches none of its
// Fatals (docs/state-by-name-participants.md 3). Another version; a record it
// cannot walk to its end ("damaged"); the draw-item ring of another size,
// which is a setting (BOF3X_ORIGINAL=DrawPool keeps the original's 1,024:
// draw_pool.cpp) that format 2's fingerprint catches first but a format-1
// file of this build does not. And the language: the message pool is moved
// (DIV-0007) under any language overlay and not under the original Chinese,
// and the language is the start's (BOF3X_LANG), not the build's, so a
// quicksave taken in one and loaded in the other passes the build check. One
// translation into another is let through: both pools are 0x8000 bytes, and
// the take's text stands until the game reloads its own (IDEAS I47 reviews
// that). The record is fixed-width words and u16s: the same at both widths
// and on both systems.
bool StCheck(SnapshotReader& r, std::string& why, std::string& detail) {
    const auto damaged = [&](const char* what) {
        why = "damaged";
        detail = std::string("ST's record: ") + what;
        return false;
    };
    if (r.Left() < sizeof(U)) return damaged("no version word");
    const U version = r.Get<U>();
    if (version != kStVersion) {
        why = "another state record";
        detail = Printf("ST's record is version %u; this build reads %u only", version, kStVersion);
        return false;
    }
    if (r.Left() < 2 * sizeof(U)) return damaged("the seed and the ring's count run past the record");
    r.Get<std::uint32_t>();
    const U ring = r.Get<U>();
    if (ring != draw_pool::Count()) {
        why = "other settings (DrawPool)";
        detail = Printf("ST's draw-item ring has %u entries, this process %u (BOF3X_ORIGINAL=DrawPool keeps the original's)", ring,
                        draw_pool::Count());
        return false;
    }
    if (r.Left() < std::size_t{ring} * sizeof(unsigned short) + sizeof(std::uint32_t)) return damaged("the ring runs past the record");
    r.Skip(std::size_t{ring} * sizeof(unsigned short));
    const std::uint32_t pool_bytes = r.Get<std::uint32_t>();
    std::uint32_t now_bytes = 0;
    MsgPool_Moved(&now_bytes);
    if ((pool_bytes != 0) != (now_bytes != 0)) {
        why = "another language";
        detail = pool_bytes ? "taken under a translation (BOF3X_LANG), this game runs the original Chinese text"
                            : "taken under the original Chinese text, this game runs a translation (BOF3X_LANG)";
        return false;
    }
    if (pool_bytes != now_bytes) {
        why = "another language";
        detail = Printf("the take's message pool is %u bytes, this process's %u", pool_bytes, now_bytes);
        return false;
    }
    if (r.Left() != std::size_t{pool_bytes} + 1) return damaged("the message pool and the flag are not the rest of the record");
    return true;
}

void StRestore(SnapshotReader& r) {
    if (r.Get<U>() != kStVersion) Fatal("snapshot: ST's record is another version, past StCheck");
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
    const auto refused = [&why, &detail](const std::vector<unsigned char>& bytes, const char* want) {
        SnapshotReader reader("ST", bytes.data(), bytes.size());
        return !StCheck(reader, why, detail) && why == want;
    };
    std::vector<unsigned char> d = rec;
    const U v2 = kStVersion + 1;
    std::memcpy(d.data(), &v2, sizeof v2);
    Check(refused(d, "another state record"), "ST's check refuses another version");
    d = rec;
    const U ring = draw_pool::Count() == 1024 ? 2048 : 1024;
    std::memcpy(d.data() + 2 * sizeof(U), &ring, sizeof ring);
    Check(refused(d, "other settings (DrawPool)"), "ST's check refuses a draw-item ring of another size");
    d = rec;
    d.pop_back();
    Check(refused(d, "damaged"), "ST's check refuses a record a byte short");
    d = rec;
    d.push_back(0);
    Check(refused(d, "damaged"), "ST's check refuses a byte past the record's end");
}

// A save half of noise whose block sums as a save's (SaveHalfWhole), so a
// refusal offers it.
void FakeSaveHalf(unsigned char* to) {
    for (U i = 0; i < kSaveHalf; ++i) to[i] = static_cast<unsigned char>(i * 7 + 3);
    U sum = 0;
    for (U i = 0; i < kBlockBytes; ++i) sum += (i == kChecksumAt || i == kChecksumAt + 1) ? 0 : to[i];
    to[kChecksumAt] = static_cast<unsigned char>(sum);
    to[kChecksumAt + 1] = static_cast<unsigned char>(sum >> 8);
}

// Format 2's extension of a file, and its TASKS record's and settings' places.
HeaderExt ExtOf(const std::vector<unsigned char>& f) {
    HeaderExt x;
    std::memcpy(&x, f.data() + kSaveHalf + sizeof(Header), sizeof x);
    return x;
}
void PutExt(std::vector<unsigned char>& f, const HeaderExt& x) { std::memcpy(f.data() + kSaveHalf + sizeof(Header), &x, sizeof x); }
std::size_t TasksAt(const std::vector<unsigned char>& f) {
    Header h;
    std::memcpy(&h, f.data() + kSaveHalf, sizeof h);
    return kSaveHalf + h.header_bytes + std::size_t{h.span_bytes} + h.pool_bytes + h.arena_bytes;
}
std::size_t SettingsAt(const std::vector<unsigned char>& f) { return TasksAt(f) + ExtOf(f).tasks_bytes; }
std::size_t WordsAt(const std::vector<unsigned char>& f) { return SettingsAt(f) + ExtOf(f).settings_bytes; }

// A copy of a file marked not portable, as a take with a task nested would be.
std::vector<unsigned char> NotPortableCopy(const std::vector<unsigned char>& f) {
    std::vector<unsigned char> c = f;
    HeaderExt x = ExtOf(c);
    x.portable = 0;
    x.not_portable = kNpTaskNowhere | (1u << 8);
    PutExt(c, x);
    return c;
}

bool StartsWith(const std::string& s, const char* head) { return s.rfind(head, 0) == 0; }

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
void SelfTestSpan(const fs::path& dir) {
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
    {
        const HeaderExt x = ExtOf(file);
        Check(file.size() == kSaveHalf + sizeof(Header) + sizeof(HeaderExt) + span.image.bytes + span.pool.bytes + kArenaBytes +
                                 sizeof(TaskRec) * task_yield::kTasks + x.settings_bytes + std::size_t{x.words} * sizeof(U) +
                                 2 * sizeof(RecordHead) + 300,
              "the file's size is the save half, the header, the span, the arena, the tasks, the settings, the words and the records");
        Check(x.state_revision == kStateRevision && x.tasks_bytes == sizeof(TaskRec) * task_yield::kTasks,
              "a take is format 2 at this build's state revision");
    }
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
        const U at = Img(kKept[i].at);
        kept_now[i].assign(At(at), At(at + kKept[i].bytes));
        for (U b = 0; b < kKept[i].bytes; ++b)
            if (at + b >= span.image.base && at + b < span.image.base + span.image.bytes)
                kept_byte[at + b - span.image.base] = true;
    }
    // The scribbled span's Task_StackTop is noise; the restore is checked
    // against the stand-in top, as the game's would be against the live one.
    read = ReadSlot(dir, kFirstSlot, back, why);
    Check(read && back == file, "BISLPS10.DAT reads back as written");

    Parsed p = Parse(back, build, span, top, reg);
    Check(p.ok && p.answer == Answer::Exact, "the file parses under its own build and registry: exact");
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
            if (std::memcmp(At(Img(kKept[i].at)), kept_now[i].data(), kKept[i].bytes) != 0) kept_ok = false;
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

    // Another build, the file not portable: the snapshot half refused, the
    // save half still a save with its summary where the menu reads it, and
    // offered.
    const std::vector<unsigned char> notp = NotPortableCopy(back);
    BuildId other = build;
    std::snprintf(other.tag, sizeof other.tag, "self-test-2");
    q = Parse(notp, other, span, top, reg);
    Check(!q.ok && q.why == "another build (not portable)" && q.offer_save,
          "a build-id mismatch is refused when the file is not portable, its save half offered");
    unsigned char summary[kSummaryBytes];
    std::vector<unsigned char> expect(kSaveHalf);
    FakeSaveHalf(expect.data());
    std::memcpy(summary, back.data() + kSummaryAt, kSummaryBytes);
    Check(q.save_half && std::memcmp(summary, expect.data() + kSummaryAt, kSummaryBytes) == 0,
          "with the build refused, the save half still parses: the summary at 0xCA0");
    other = build;
    other.sha[0] = 'b';
    q = Parse(notp, other, span, top, reg);
    Check(!q.ok && StartsWith(q.why, "another build"), "a DLL hash mismatch is refused when not portable");
    other = build;
    other.dll_base += 0x10000;
    q = Parse(notp, other, span, top, reg);
    Check(!q.ok && StartsWith(q.why, "another process"), "another DLL base is refused when not portable");
    q = Parse(notp, build, span, top + kPage, reg);
    Check(!q.ok && StartsWith(q.why, "another process"), "another task-arena top is refused when not portable");
    std::vector<unsigned char> cut(back.begin(), back.end() - 1);
    q = Parse(cut, build, span, top, reg);
    Check(!q.ok && q.why == "truncated" && q.save_half && q.offer_save, "a truncated file is refused, its save half kept and offered");
    std::vector<unsigned char> bad_sum = back;
    bad_sum[kChecksumAt] ^= 1;
    q = Parse(NotPortableCopy(bad_sum), other, span, top, reg);
    Check(!q.ok && q.save_half && !q.offer_save, "a save half that does not sum is not offered");

    // The game's memory as it was.
    WriteRange(span.image.base, image0.data(), span.image.bytes);
    if (span.pool.bytes) WriteRange(span.pool.base, pool0.data(), span.pool.bytes);
    Check(Same(span.image, image0) && Same(span.pool, pool0), "the span is put back after the self-test");
    std::error_code ec;
    fs::remove(SlotPath(dir, kFirstSlot), ec);
    std::vector<unsigned char> none;
    Check(!ReadSlot(dir, kFirstSlot, none, why) && why.rfind("no quicksave in slot", 0) == 0,
          "an empty slot reads as no quicksave, not as an error");
}

// Format 2's answers (docs/state-by-name-format.md 6). On the live span with
// a stand-in arena, as SelfTestSpan: a format-1 file loads exactly in its own
// build and is refused elsewhere with its save offered; then, in bof3x.exe
// (the code places are its), a take with task 0 created and not yet run and
// a text pointer of ours in one of Capcom's slots is portable, and stamped as
// another build it restores portably - the span and the record as at the
// take, the text slot kept as this process has it, task 0 on a fresh first
// frame at the code place of its VA, nothing of the arena read - and in its
// own build exactly; a take with a task nested is not portable and refused
// elsewhere with its save offered; a site this build does not list, another
// state revision, other settings, damaged settings, a code word this build
// does not bind and another set of bound functions are each refused with
// their reasons. The word tests' other kinds on single words. Everything put
// back from raw copies at the end.
const char kTextA[] = "state-by-name: the take's text";
const char kTextB[] = "state-by-name: this process's text";
unsigned char g_test_warp_page[0x40];   // B1b: a stand-in for DIV-0225's page, a buffer of ours the message box reads

void SelfTestFormat2() {
    const Span span = CurrentSpan();
    const auto image0 = ReadRaw(span.image), pool0 = ReadRaw(span.pool);
    task_yield::Entry entries0[task_yield::kTasks];
    std::memcpy(entries0, task_yield::g_entries, sizeof entries0);
    std::vector<unsigned char> arena_block(kArenaBytes + kPage);
    const U top = (Va(arena_block.data()) + kArenaBytes + kPage - 1) & ~(kPage - 1);
    if (top - kArenaBytes < Va(arena_block.data())) Fatal("snapshot self-test: the stand-in arena does not fit");
    unsigned char* const low = At(top - kArenaBytes);   // the stand-in arena's lowest byte
    const auto scribble_arena = [low](unsigned char seed) {
        for (U i = 0; i < kArenaBytes; ++i) low[i] = static_cast<unsigned char>(i * 13 + seed);
    };
    const auto set_word = [](U va, U value) { WriteRange(Img(va), reinterpret_cast<const unsigned char*>(&value), sizeof value); };
    const auto word = [](U va) { return U32(At(Img(va))); };
    scribble_arena(0);
    set_word(Task_StackTop_addr, top);
    {
        unsigned char zero[task_sched::kTasks * task_sched::kRecordSize] = {};
        WriteRange(Img(bof3::addr::Task_Records), zero, sizeof zero);
    }
    for (auto& e : task_yield::g_entries) e = task_yield::Entry{};

    Registry reg{{"TEST", &TestSave, &TestRestore}};
    g_test_record.assign(77, 0);
    for (std::size_t i = 0; i < g_test_record.size(); ++i) g_test_record[i] = static_cast<unsigned char>(0x3C + i * 5);
    BuildId build;
    build.host = IsExeHost() ? kHostExe : kHostBof3;
    std::snprintf(build.tag, sizeof build.tag, "self-test");
    std::memset(build.sha, 'a', sizeof build.sha);
    build.dll_base = 0x10000000;
    BuildId other = build;
    std::snprintf(other.tag, sizeof other.tag, "self-test-2");
    std::memset(other.sha, 'c', sizeof other.sha);

    // Equal to `want` but the keep list and the bytes of `but`.
    const auto same_but = [&span](const std::vector<unsigned char>& want, U but, const char* what) {
        U wrong = 0;
        for (U at = 0; at < span.image.bytes; ++at) {
            const U va = span_words::kDataLo + at;
            bool kept = va >= but && va < but + 4;
            for (std::size_t i = 0; i < kKeptCount && !kept; ++i) kept = va >= kKept[i].at && va < kKept[i].at + kKept[i].bytes;
            if (!kept && At(span.image.base)[at] != want[at] && ++wrong <= 4)
                Log("shadow      snapshot: %s: span byte 0x%08X is 0x%02X, 0x%02X wanted", what, span.image.base + at,
                    At(span.image.base)[at], want[at]);
        }
        return wrong == 0;
    };

    // (1) Format 1: exact in its own build, the save elsewhere.
    std::vector<unsigned char> f1;
    OnOwnStack([&] { f1 = Take(reg, span, top, &FakeSaveHalf, kFormat1); });
    Stamp(f1, build);
    const auto take1 = ReadRaw(span.image);
    const std::vector<unsigned char> arena1(low, low + kArenaBytes);
    Scribble(span.image, 0x4242);
    scribble_arena(0x55);
    Parsed q = Parse(f1, build, span, top, reg);
    Check(q.ok && q.answer == Answer::Exact && q.header.format == kFormat1, "a format-1 file parses exactly in its own build");
    if (q.ok) {
        g_test_restored.clear();
        OnOwnStack([&] { Restore(q, reg); });
        Check(same_but(take1, 0, "format 1") && std::memcmp(low, arena1.data(), kArenaBytes) == 0 &&
                  g_test_restored == g_test_record,
              "a format-1 file restores exactly: the span but the keep list, the arena, the record");
    }
    q = Parse(f1, other, span, top, reg);
    Check(!q.ok && q.why == "another build" && q.offer_save, "a format-1 file of another build is refused, its save offered");

    const bool can = IsExeHost() && Bind_Active() && Bind_TableMeasured() && Bind_Rows() > 0 && Bind_Ours(bof3::addr::Boot_Task);
    if (!can) {
        Log("shadow      snapshot: the portable cases need bof3x.exe's code places, which this host has not: not run");
    } else {
        // (2) A portable take: task 0 created (WinMain's Boot_Task, as
        // magic_080.cpp creates it), the rest free; a slot of Capcom's with
        // text of ours in it.
        WriteRange(span.image.base, take1.data(), span.image.bytes);   // the live span again: the code places as bound
        set_word(Task_StackTop_addr, top);
        scribble_arena(0);
        ::Task_Create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Boot_Task))));
        U slot = 0;
        for (const U off : span_words::g_slots) {
            const U va = span_words::kDataLo + off;
            if (Unnamed(va) || Bind_VaOf(word(va))) continue;
            bool runtime = false;
            for (U i = 0; i < span_words::kRuntimeSlots; ++i) runtime = runtime || va == span_words::RuntimeSlot(i);
            if (!runtime) {
                slot = va;
                break;
            }
        }
        Check(slot != 0, "a slot of Capcom's to hold text of ours");
        const U text_a = Va(kTextA), text_b = Va(kTextB);
        const span_words::Section* sa = span_words::g_ours.Named(text_a);
        Check(sa && sa->content, "a string of ours lies in a section of ours a pointer names by its bytes");
        if (slot) set_word(slot, text_a);
        std::vector<unsigned char> f;
        OnOwnStack([&] { f = Take(reg, span, top, &FakeSaveHalf); });
        Stamp(f, build);
        const auto take2 = ReadRaw(span.image);
        const std::vector<unsigned char> arena2(low, low + kArenaBytes);
        const HeaderExt x = ExtOf(f);
        TaskRec t[task_yield::kTasks];
        std::memcpy(t, f.data() + TasksAt(f), sizeof t);
        U code_words = 0, text_words = 0, first_code = 0;
        for (U i = 0; i < x.words; ++i) {
            const U e = U32(f.data() + WordsAt(f) + std::size_t{i} * sizeof(U));
            if ((e & 7) == kWordCode && !code_words++) first_code = i;
            if ((e & 7) == kWordText && span_words::kDataLo + (e >> 3) * 4 == slot) ++text_words;
        }
        Check(x.portable == 1 && x.not_portable == kIsPortable, "a take with every task free or new is portable");
        Check(t[0].kind == kTaskCreated && t[0].name == bof3::addr::Boot_Task && t[1].kind == kTaskFree,
              "the TASKS record: task 0 new at Boot_Task's VA, task 1 free");
        Check(code_words > 0 && text_words == 1, "the WORDS list holds the code places as VAs and the text slot");
        {
            const unsigned char* image = f.data() + kSaveHalf + sizeof(Header) + sizeof(HeaderExt);
            const U e = U32(f.data() + WordsAt(f) + std::size_t{first_code} * sizeof(U));
            const U off = (e >> 3) * 4;
            Check(U32(image + off) == Bind_VaOf(U32(take2.data() + off)) && U32(image + off) != U32(take2.data() + off),
                  "a code place is written as its VA in the file, and the live span keeps the place");
        }

        // Stamped as this build, loaded by another: portable.
        Scribble(span.image, 0x7777);
        scribble_arena(0x99);
        set_word(Task_StackTop_addr, top);   // the live process's own, as the game's is
        if (slot) set_word(slot, text_b);    // this process's text of ours, at its own place
        q = Parse(f, other, span, top, reg);
        Check(q.ok && q.answer == Answer::Portable, "a portable file stamped as another build parses as portable");
        if (q.ok) {
            g_test_restored.clear();
            const unsigned char deep = low[kArenaBytes - kSlice - 0x100];
            OnOwnStack([&] { Restore(q, reg); });
            Check(same_but(take2, slot, "portable"), "a portable restore: the span as at the take but the keep list and the text slot");
            Check(!slot || word(slot) == text_b, "a portable restore keeps the text slot as this process has it");
            std::uintptr_t entry = 0;
            std::memcpy(&entry, low + (FirstFrame(top, 0) + task_sched::kEntrySlot - (top - kArenaBytes)), sizeof entry);
            Check(U32(TaskRecord(0) + task_sched::kSavedEsp) == FirstFrame(top, 0) &&
                      entry == static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Boot_Task)) &&
                      U16(TaskRecord(0) + task_sched::kState) == task_sched::kRunnable,
                  "a portable restore starts task 0 on a fresh first frame at the code place of its VA");
            Check(low[kArenaBytes - kSlice - 0x100] == deep, "a portable restore reads nothing of the file's arena");
            Check(g_test_restored == g_test_record, "a portable restore restores the participant's record");
        }
        // The same file in its own build: exact, the arena and the text slot the file's.
        Scribble(span.image, 0x3131);
        scribble_arena(0x11);
        set_word(Task_StackTop_addr, top);
        if (slot) set_word(slot, text_b);
        q = Parse(f, build, span, top, reg);
        Check(q.ok && q.answer == Answer::Exact, "the portable file parses exactly in its own build");
        if (q.ok) {
            OnOwnStack([&] { Restore(q, reg); });
            Check(same_but(take2, 0, "exact") && std::memcmp(low, arena2.data(), kArenaBytes) == 0,
                  "an exact restore of format 2: the span (the text slot the take's) and the arena as at the take");
        }

        // (2b) B1b: a word holding the text slot's pointer (as an effect
        // object copies a slot's, masterAndManillo) follows that slot; the
        // message box aimed at a buffer of ours (DIV-0225's town warp page)
        // makes the take not portable.
        if (slot) {
            U copy = 0;
            for (U off = 0; off < span_words::kDataHi - span_words::kDataLo && !copy; off += 4) {
                const U va = span_words::kDataLo + off;
                bool runtime = span_words::IsTextWord(va);
                for (U i = 0; i < span_words::kRuntimeSlots; ++i) runtime = runtime || va == span_words::RuntimeSlot(i);
                if (!span_words::IsSlot(off) && !Unnamed(va) && !runtime && word(va) == 0) copy = va;
            }
            Check(copy != 0, "a word of .data no rule names, to hold a copy");
            set_word(slot, text_a);
            set_word(copy, text_a + 3);   // a cursor three bytes into the slot's text
            std::vector<unsigned char> c;
            OnOwnStack([&] { c = Take(reg, span, top, &FakeSaveHalf); });
            Stamp(c, build);
            const HeaderExt cx = ExtOf(c);
            U copies = 0;
            const unsigned char* image = c.data() + kSaveHalf + sizeof(Header) + sizeof(HeaderExt);
            for (U i = 0; i < cx.words; ++i) {
                const U e = U32(c.data() + WordsAt(c) + std::size_t{i} * sizeof(U));
                if ((e & 7) == kWordTextCopy && span_words::kDataLo + (e >> 3) * 4 == copy &&
                    U32(image + (e >> 3) * 4) == (3u << 24 | (slot - span_words::kDataLo)))
                    ++copies;
            }
            Check(cx.portable == 1 && copies == 1, "a copy of a text slot's pointer is written as that slot, and the take stays portable");
            set_word(slot, text_b);
            set_word(copy, 0x5A5A5A5A);
            q = Parse(c, other, span, top, reg);
            Check(q.ok && q.answer == Answer::Portable, "the take with the copy parses as portable in another build");
            if (q.ok) {
                OnOwnStack([&] { Restore(q, reg); });
                Check(word(copy) == text_b + 3 && word(slot) == text_b, "a portable restore places the cursor in its slot's text: this process's");
            }
            set_word(slot, text_b);
            set_word(copy, 0x5A5A5A5A);
            q = Parse(c, build, span, top, reg);
            Check(q.ok && q.answer == Answer::Exact, "and exactly in its own build");
            if (q.ok) {
                OnOwnStack([&] { Restore(q, reg); });
                Check(word(copy) == text_a + 3 && word(slot) == text_a, "an exact restore places the cursor in its slot's text: the take's");
            }
            set_word(copy, 0);
            const U box0 = word(Msgbox_Base_addr);
            set_word(Msgbox_Base_addr, Va(g_test_warp_page));
            std::vector<unsigned char> d;
            OnOwnStack([&] { d = Take(reg, span, top, &FakeSaveHalf); });
            Stamp(d, build);
            const HeaderExt dx = ExtOf(d);
            Check(dx.portable == 0 && (dx.not_portable & 0xFF) == kNpTextOurs,
                  "the message box aimed at a buffer of ours (the town warp's page): the take is not portable");
            q = Parse(d, other, span, top, reg);
            Check(!q.ok && q.why == "another build (not portable)" && q.offer_save,
                  "another build refuses it, not portable, its save half offered");
            q = Parse(d, build, span, top, reg);
            Check(q.ok && q.answer == Answer::Exact, "its own build restores it exactly");
            set_word(Msgbox_Base_addr, box0);
        }

        // (3) Not portable: task 1 asleep inside a body, at no site.
        {
            unsigned char* rec = TaskRecord(1);
            const std::uint16_t sleeping = task_sched::kSleeping, frames = 3;
            const U sp = FirstFrame(top, 1) - 0x200;
            std::memcpy(rec + task_sched::kState, &sleeping, 2);
            std::memcpy(rec + task_sched::kSleep, &frames, 2);
            std::memcpy(rec + task_sched::kSavedEsp, &sp, 4);
        }
        std::vector<unsigned char> g;
        OnOwnStack([&] { g = Take(reg, span, top, &FakeSaveHalf); });
        Stamp(g, build);
        const HeaderExt gx = ExtOf(g);
        Check(gx.portable == 0 && gx.not_portable == (kNpTaskNowhere | (1u << 8)), "a take with task 1 nested is not portable");
        q = Parse(g, other, span, top, reg);
        Check(!q.ok && q.why == "another build (not portable)" && q.save_half && q.offer_save,
              "a file that is not portable is refused by another build, its save half kept and offered");
        // With a site entry for it, the same task stands at a site: portable,
        // and refused here only because this build lists no such site.
        task_yield::g_entries[1] = task_yield::Entry{0xFFFF, 2, {0x12, 0x34}};
        OnOwnStack([&] { g = Take(reg, span, top, &FakeSaveHalf); });
        task_yield::g_entries[1] = task_yield::Entry{};
        Stamp(g, build);
        std::memcpy(t, g.data() + TasksAt(g), sizeof t);
        Check(ExtOf(g).portable == 1 && t[1].kind == kTaskSite && t[1].name == 0xFFFF && t[1].bytes == 2 && t[1].locals[1] == 0x34,
              "a task at a site is portable, its site and locals in the TASKS record");
        q = Parse(g, other, span, top, reg);
        Check(!q.ok && q.why == "another build (a task at an unknown place)" && q.offer_save,
              "a site this build does not list is refused, the save offered");

        // (4) Another state revision, other settings, damaged settings.
        std::vector<unsigned char> c = f;
        HeaderExt cx = ExtOf(c);
        cx.state_revision = kStateRevision + 1;
        PutExt(c, cx);
        q = Parse(c, build, span, top, reg);
        Check(!q.ok && q.why == "another state revision" && q.offer_save, "another state revision is refused, the save offered");
        c = f;
        {
            const std::size_t at = SettingsAt(c);
            std::string text(reinterpret_cast<const char*>(c.data() + at), ExtOf(c).settings_bytes);
            const std::size_t k = text.find("\npsp=");
            if (k != std::string::npos && k + 5 < text.size()) {
                c[at + k + 5] = text[k + 5] == '9' ? '8' : '9';
                cx = ExtOf(c);
                cx.settings_fnv = FnvBytes(c.data() + at, cx.settings_bytes);
                PutExt(c, cx);
            }
            q = Parse(c, build, span, top, reg);
            Check(k != std::string::npos && !q.ok && q.why == "other settings (psp)" && q.offer_save,
                  "a file taken under other PSP rules is refused, the setting named, the save offered");
            cx.settings_fnv ^= 1;
            PutExt(c, cx);
            q = Parse(c, build, span, top, reg);
            Check(!q.ok && q.why == "damaged", "settings whose lines do not hash to the fingerprint are refused as damaged");
        }

        // (5) A code word this build does not bind; another set of bound functions.
        c = f;
        {
            const U e = U32(c.data() + WordsAt(c) + std::size_t{first_code} * sizeof(U));
            unsigned char* image = c.data() + kSaveHalf + sizeof(Header) + sizeof(HeaderExt);
            U unbound = Bind_RowVa(0) + 1;
            while (Bind_Ours(unbound)) ++unbound;
            std::memcpy(image + (e >> 3) * 4, &unbound, 4);
        }
        q = Parse(c, other, span, top, reg);
        Check(!q.ok && q.why == "another build (a word this build cannot place)" && q.offer_save,
              "a code word naming a VA this build does not bind is refused");
        c = f;
        cx = ExtOf(c);
        cx.bind_fnv ^= 1;
        PutExt(c, cx);
        q = Parse(c, other, span, top, reg);
        Check(!q.ok && q.why == "another build (other functions bound)", "a build that binds other functions refuses a portable file");
    }

    // (6) The other kinds of word, one at a time.
    {
        WordPlace wp;
        wp.pool = 0x12340000;
        wp.pool_bytes = 0x8000;
        wp.code = false;
        U kind = 0, stored = 0, np = 0, w = 0;
        std::string why;
        const U ring = span_words::RuntimeSlot(3) - span_words::kDataLo;
        Check(NameWord(wp, ring, wp.pool + 0x10, &kind, &stored, &np) && kind == kWordPool && stored == 0x10 && np == 0,
              "a message slot into the pool is written as its offset");
        WordPlace there = wp;
        there.pool = 0x56780000;
        Check(PlaceWord(there, kWordPool, 0x10, 0, false, &w, why) && w == 0x56780010, "and placed at this process's pool");
        Check(!PlaceWord(there, kWordPool, 0x9000, 0, false, &w, why), "an offset past the pool is refused");
        U plain = 0;
        while (span_words::IsSlot(plain) || plain == ring) plain += 4;
        wp.image_base = 0x70000000;
        Check(NameWord(wp, plain, 0x70001234, &kind, &stored, &np) && kind == kWordFold && stored == 0x401234,
              "a pointer into the moved image is written as at 0x400000");
        Check(PlaceWord(wp, kWordFold, stored, 0, false, &w, why) && w == 0x70001234, "and placed at the image's base");
        wp.image_base = 0x00A00000;
        Check(NameWord(wp, plain, 0x0BA01234, &kind, &stored, &np) && kind == kWordFold24 && stored == 0x0B401234,
              "a link into the moved image keeps its top byte, its low 24 bits as at 0x400000");
        Check(PlaceWord(wp, kWordFold24, stored, 0, false, &w, why) && w == 0x0BA01234, "and is placed back so");
        wp.image_base = span_words::kExeLo;
        Check(!NameWord(wp, plain, span_words::kExeLo + 0x1234, &kind, &stored, &np), "at 0x400000 nothing is folded");
        Check(PlaceWord(wp, kWordText, Va(kTextA), Va(kTextB), true, &w, why) && w == Va(kTextB) &&
                  PlaceWord(wp, kWordText, Va(kTextA), Va(kTextB), false, &w, why) && w == Va(kTextA) &&
                  !PlaceWord(wp, kWordText, Va(kTextA), 0x1234, true, &w, why),
              "a text slot: the file's at an exact load, the process's at a portable one, refused when the process's is not ours");
        // B1b: the text words, a copy of a text slot's pointer, and a word no rule names.
        const U box = Msgbox_Base_addr - span_words::kDataLo, battle = span_words::kTextWords[3] - span_words::kDataLo;
        Check(NameWord(wp, box, wp.pool + 0x272, &kind, &stored, &np) && kind == kWordPool && stored == 0x272 && np == 0 &&
                  NameWord(wp, battle, wp.pool + 0x399, &kind, &stored, &np) && kind == kWordPool && stored == 0x399 && np == 0,
              "a message box's text word into the pool, and the battle block's, are written as their offsets in it");
        U text_slot = 0;
        while (!span_words::IsSlot(text_slot)) text_slot += 4;
        wp.texts = {{Va(kTextA), text_slot}};
        Check(NameWord(wp, plain, Va(kTextA), &kind, &stored, &np) && kind == kWordTextCopy && stored == text_slot && np == 0 &&
                  NameWord(wp, battle, Va(kTextA), &kind, &stored, &np) && kind == kWordTextCopy && stored == text_slot,
              "a word holding a text slot's pointer is written as that slot's offset, a text word's too");
        const U end = static_cast<U>(sizeof kTextA - 1);
        Check(NameWord(wp, plain, Va(kTextA) + 5, &kind, &stored, &np) && kind == kWordTextCopy && stored == (5u << 24 | text_slot) &&
                  NameWord(wp, plain, Va(kTextA) + end, &kind, &stored, &np) && stored == (end << 24 | text_slot) &&
                  !NameWord(wp, plain, Va(kTextA) + end + 1, &kind, &stored, &np),
              "a cursor inside the slot's text, up to its NUL, is written as the slot and how far in; one past the NUL is not");
        Check(!NameWord(wp, box, Va(kTextB), &kind, &stored, &np) && np == kNpTextOurs,
              "a message box's text word naming our module elsewhere (the town warp's page) makes the take not portable");
        np = 0;
        Check(!NameWord(wp, plain, wp.pool + 0x40, &kind, &stored, &np) && np == 0,
              "a word no rule names holding a pool address is written raw (logged at the take), not refused");
        wp.texts.clear();
    }

    WriteRange(span.image.base, image0.data(), span.image.bytes);
    if (span.pool.bytes) WriteRange(span.pool.base, pool0.data(), span.pool.bytes);
    std::memcpy(task_yield::g_entries, entries0, sizeof entries0);
    Check(Same(span.image, image0) && Same(span.pool, pool0), "the span is put back after format 2's self-test");
}

std::vector<unsigned char> Text(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

bool HoldsText(const fs::path& path, const std::string& want) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char buf[64] = {};
    f.read(buf, sizeof buf - 1);
    return std::string(buf, static_cast<std::size_t>(f.gcount())) == want;
}

void ClearDir(const fs::path& dir) {
    std::error_code ec;
    for (unsigned s = kFirstSlot; s <= kLastSlot; ++s) fs::remove(SlotPath(dir, s), ec);
    fs::remove(dir / "BISLPS10.TMP", ec);
}

// The rotation's failing step, made the same way on every system: as 1A is
// to be renamed, a file appears at 1B - which 1B's own step had just moved
// away - and the system's refusing rename (os::MoveFileKeep) refuses for real.
// (Before the seam the self-test held 1A open with no sharing, which only
// Windows refuses a rename for.)
fs::path g_rival_dir;
os::MoveResult RivalAt1B(const fs::path& from, const fs::path& to) {
    if (from == SlotPath(g_rival_dir, 0x1A)) {
        std::string why;
        WriteAll(to, Text("rival"), why);
    }
    return os::MoveFileKeep(from, to);
}

// The rotation: sixteen files each move up one, 1F's gone, 10 new; a gapped
// set moves the same; a rename that fails stops it with every file present.
void SelfTestRotation(const fs::path& dir) {
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
    g_rival_dir = dir;
    ok = WriteNewest(dir, Text("new"), why, &RivalAt1B);
    bool stopped = !ok && why.find("BISLPS1A.DAT could not be renamed") != std::string::npos &&
                   why.find("the target is there") != std::string::npos;
    // 1F deleted (the oldest goes first), 1E..1B moved up, the rival at 1B kept,
    // 1A and below where they were, no 10 written.
    stopped = stopped && HoldsText(SlotPath(dir, 0x1B), "rival") && HoldsText(SlotPath(dir, 0x1A), SlotName(0x1A)) &&
              HoldsText(SlotPath(dir, 0x1F), SlotName(0x1E)) && HoldsText(SlotPath(dir, 0x1C), SlotName(0x1B)) &&
              HoldsText(SlotPath(dir, 0x10), SlotName(0x10)) && !Exists(dir / "BISLPS10.TMP");
    Check(stopped, "a rename that fails stops the rotation with a refusal, every file present, the rival kept, no new 10");
    if (!ok) Log("shadow      snapshot: the refusal read \"%s\"", why.c_str());

    // What a crash inside MoveFileKeep's third way leaves (D431): the step
    // 14 -> 15 had made its claim, an empty 15, when the process ended - the
    // steps above it done (16..1F each one up, the old 1F gone), 10..14 where
    // they were, the crashed F6's take still under BISLPS10.TMP. The next F6
    // rotates whole: the empty file is removed as a gap (not moved up to 16
    // as an empty slot), every quicksave moves up one, 10 is the new take,
    // and no file of zero bytes is left.
    ClearDir(dir);
    for (unsigned s = kFirstSlot; s <= 0x14; ++s) WriteAll(SlotPath(dir, s), Text(SlotName(s)), why);
    std::ofstream(SlotPath(dir, 0x15), std::ios::binary).close();   // the claim: a file of zero bytes
    for (unsigned s = 0x16; s <= kLastSlot; ++s) WriteAll(SlotPath(dir, s), Text(SlotName(s - 1)), why);
    WriteAll(dir / "BISLPS10.TMP", Text("crashed"), why);
    ok = WriteNewest(dir, Text("new"), why);
    bool crashed = ok && HoldsText(SlotPath(dir, kFirstSlot), "new") && !Exists(SlotPath(dir, 0x16)) &&
                   !Exists(dir / "BISLPS10.TMP");
    for (unsigned s = kFirstSlot + 1; s <= 0x15; ++s) crashed = crashed && HoldsText(SlotPath(dir, s), SlotName(s - 1));
    for (unsigned s = 0x17; s <= kLastSlot; ++s) crashed = crashed && HoldsText(SlotPath(dir, s), SlotName(s - 2));
    for (unsigned s = kFirstSlot; s <= kLastSlot; ++s) crashed = crashed && !EmptySlotFile(SlotPath(dir, s));
    Check(crashed, "an empty slot file a crash left: removed as a gap, the rest moved up one, 10 the new one");
    ClearDir(dir);
}

void SelfTest() {
    std::error_code ec;
    const fs::path temp = fs::temp_directory_path(ec);
    if (ec || temp.empty()) Fatal("snapshot self-test: no temporary directory");
    const fs::path dir = temp / Printf("bof3x-snapshot-%lu", static_cast<unsigned long>(bof3::os::ProcessId()));
    fs::create_directory(dir, ec);   // false with no error when it is there already
    if (ec) Fatal("snapshot self-test: cannot create %s (error %d)", PathText(dir).c_str(), ec.value());
    SelfTestSpan(dir);
    SelfTestFormat2();
    SelfTestStCheck();
    SelfTestRotation(dir);
    ClearDir(dir);
    fs::remove(dir, ec);
    if (g_fails) Fatal("snapshot self-test: %d check(s) failed (the log has each)", g_fails);
    const Span span = CurrentSpan();
    Log("shadow      snapshot: self-test passed - span 0x%08X+0x%X and pool 0x%08X+0x%X copied, scribbled and restored equal; "
        "a missing participant either side, a participant's check, another language, another build, DLL base, arena top "
        "and a truncated file refused; format 2: a format-1 file exact, a portable file of another build restored "
        "portably and exactly in its own, one not portable, an unknown site, another state revision, other settings, "
        "damaged settings, an unbound code word and other bound functions refused with the save offered, the word kinds; "
        "the text words into the pool, a copy of a text slot placed as its slot, the message box on a buffer of ours not portable; "
        "an empty slot; the "
        "rotation over sixteen files, a gapped set, a failed rename and an empty slot file a crash left",
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

const SnapshotLoadResult& Snapshot_LastLoad() { return g_last_load; }

const SnapshotKept* Snapshot_Kept(std::size_t& count) {
    count = kKeptCount;
    return kKept;
}

bool Snapshot_ReadSaveHalf(unsigned slot, std::vector<unsigned char>& out, std::string& why) {
    if (slot < kFirstSlot || slot > kLastSlot) Fatal("snapshot: the save half of slot 0x%X asked, outside 0x10..0x1F", slot);
    std::vector<unsigned char> file;
    if (!ReadSlot(fs::path(), slot, file, why)) return false;
    if (!SaveHalfWhole(file.data(), file.size())) {
        why = file.size() < kSaveHalf ? Printf("%s is shorter than a save file", SlotName(slot).c_str())
                                      : Printf("%s's save half does not sum as a save's block", SlotName(slot).c_str());
        return false;
    }
    out.assign(file.begin(), file.begin() + kSaveHalf);
    return true;
}

void Snapshot_Pending() {
    WordsProbe();
    const Request request = g_request;
    if (request == Request::None) return;
    g_request = Request::None;
    // BOF3X_GAPWATCH guards .data pages and parks them by the latch's count
    // (gapwatch.h): a copy of the span would fault through every guard, and a
    // restore would undo its bookkeeping. Refused while it is armed.
    if (bof3::os::GetEnv("BOF3X_GAPWATCH", nullptr, 0) > 0) {
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
                       Va(&D3d_Device),        Va(&Desktop_Bpp)};
    static_assert(sizeof names / sizeof names[0] == kKeptCount, "a symbol for every kept range");
    for (std::size_t i = 0; i < kKeptCount; ++i)
        if (names[i] != Img(kKept[i].at))
            Fatal("snapshot: kKept's %s is 0x%08X, symbols.toml's at 0x%08X (VA 0x%08X)", kKept[i].name, kKept[i].at,
                  names[i], names[i] - Img(kKept[i].at) + kKept[i].at);
    // Hosted, with the other self-tests (BOF3X_SHADOW=snapshot or '*'). The
    // exe host refuses BOF3X_SHADOW (it compares Capcom's code, which that
    // host does not map); this test compares nothing of Capcom's and the exe
    // host's span is the one players have, so there it runs on every
    // BOF3X_SELFTEST_ONLY start, as ExeIcon_SelfTest does.
    if (IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : WantsShadow("snapshot")) SelfTest();
    Snapshot_Register("ST", &StSave, &StRestore, nullptr, &StCheck);   // after DrawPool_Grow, sigma::Arm and MsgPool_Relocate's inject
}

}  // namespace bof3
