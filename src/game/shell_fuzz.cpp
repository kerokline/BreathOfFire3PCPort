// BOF3X_SHADOW=shell: a differential fuzz of group PW's seven functions against
// byte-copies of Capcom's, once at start-up (docs/shell.md section 4).
//
// Each function alone: every relative call out of its copy is re-aimed at a
// recording stand-in, and ours is put on the same stand-ins through shell::g;
// Disc_Probe's GetDriveTypeA, an absolute operand naming the import slot, is
// re-aimed by its disp32 at a slot of ours holding a recorder. The C runtime's
// stand-ins act as the runtime would where the caller reads the effect - fgets
// fills the buffer with the round's next line, sscanf stores its two ints
// through the byte pointers it is given, sprintf joins its two strings - since
// this runs before BOF3.exe's C runtime is up. Stand-ins may disturb what their
// caller reads afterwards (the pad words, the window handle, the buffer index,
// the current block, the slot pointers, the drive root).
//
// Every call is made on a stack of the fuzz's own, from one call site, with the
// bytes around it seeded per round: Cfg_Load's key table runs 0x58 bytes past
// its frame into the return address and the caller's bytes, which must be the
// same for both, and Game_Init's jmp to Task_SetStackBase is checked by the esp
// the stand-in arrives with. Addresses on that stack are recorded relative to
// its top; for Cfg_Load the frame and everything above it are compared too.
//
// Compared per round: the regions below, byte for byte, the log of calls, and
// Disc_Probe's result.
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/shell_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

#define SHELL_STR2(x) #x
#define SHELL_STR(x) SHELL_STR2(x)
#define SHELL_SYM(name) SHELL_STR(__USER_LABEL_PREFIX__) #name

// Calls fn(args[0..4]) with esp at `top`, from this one site; returns eax.
extern "C" __attribute__((naked)) std::uint32_t __cdecl Shell_CallOnStack(const void*, const std::uint32_t*,
                                                                           std::uint32_t) {
    asm("pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 8(%ebp), %eax\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "movl 16(%ebp), %esp\n\t"
        "pushl 16(%ecx)\n\t"
        "pushl 12(%ecx)\n\t"
        "pushl 8(%ecx)\n\t"
        "pushl 4(%ecx)\n\t"
        "pushl (%ecx)\n\t"
        "call *%eax\n\t"
        "leal -12(%ebp), %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret");
}

// Task_SetStackBase's stand-in: the esp it arrives with, then the recorder.
// Reached by Game_Init's jmp, so its ret is Game_Init's.
extern "C" {
std::uint32_t Shell_FuzzSeenEsp = 0;
void __cdecl Shell_FuzzStackBaseSeen();
}
extern "C" __attribute__((naked)) void __cdecl Shell_FuzzStackBase() {
    asm("movl %esp, " SHELL_SYM(Shell_FuzzSeenEsp) "\n\t"
        "jmp " SHELL_SYM(Shell_FuzzStackBaseSeen));
}

namespace shell {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

// --- Random numbers of our own ----------------------------------------------
U g_rand = 0x4FD030A5u;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}

// --- The fuzz's stack -----------------------------------------------------------
constexpr U kStackBytes = 0x10000, kAbove = 0x100, kSeedBelow = 0x800;
alignas(16) unsigned char g_stack[kStackBytes];
U g_top;   // esp at the call site; the five argument dwords go below it
// Cfg_Load's frame starts 0x3C under its return address, which is under the
// five arguments: compared from there to the end of the seeded bytes above.
constexpr U kCfgCompareBelow = 0x14 + 4 + kCfgFrameBytes;
U Rel(const void* p) { return Addr(p) - g_top; }

// --- The memory a round compares ------------------------------------------------
constexpr unsigned kEnvBytes = 0x200, kStrBytes = 0x80;
unsigned char g_env[kEnvBytes];       // the blocks Gfx_InitBufferBlock is handed
char g_local[kStrBytes];              // Disc_Probe's two names
char g_disc[kStrBytes];

struct Region { U at, bytes; };
const Region kRegions[] = {
    {0x65DA44, 8},                            // Cfg_Fullscreen, Cfg_RenderMode
    {kCdRoot, 4},                             // File_CdRootBuf
    {0x6BC620, 0x1C},                         // Game_Hwnd, Game_HInstance .. Game_QuitFlag 0x6BC639
    {kKeyTableDefault, 0x80},
    {kKeyTable, 0x80},
    {0x7E1BE8, 0x10},                         // Input_Held .. Input2_Pressed
    {0x905B88, 4},                            // Gfx_BufferIndex 0x905B89
    {kGfxOtPointers, 0x20},
    {0x937F84, 4},                            // Gfx_CurrentEnv
};
constexpr unsigned kRegionBytes = 8 + 4 + 0x1C + 0x80 + 0x80 + 0x10 + 4 + 0x20 + 4;

struct Entry { U what, a, b, c, d, e; };
constexpr unsigned kLog = 64;   // Cfg_Load makes at most 45 calls here, Game_Init 15
struct State {
    unsigned char memory[kRegionBytes];
    unsigned char env[kEnvBytes];
    unsigned char stack[kSeedBelow + kAbove];
    Entry log[kLog];
    unsigned log_n;
    U result;
};
Entry g_log[kLog];
unsigned g_log_n;
U g_seed;

void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.memory + at, At(r.at), r.bytes);
        at += r.bytes;
    }
    std::memcpy(s.env, g_env, sizeof s.env);
    std::memcpy(s.stack, At(g_top - kSeedBelow), sizeof s.stack);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
    s.result = 0;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.at), s.memory + at, r.bytes);
        at += r.bytes;
    }
    std::memcpy(g_env, s.env, sizeof g_env);
    std::memcpy(At(g_top - kSeedBelow), s.stack, sizeof s.stack);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
U FirstDifference(const State& a, const State& b) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        for (U k = 0; k < r.bytes; ++k)
            if (a.memory[at + k] != b.memory[at + k]) return r.at + k;
        at += r.bytes;
    }
    for (U k = 0; k < kEnvBytes; ++k)
        if (a.env[k] != b.env[k]) return Addr(g_env) + k;
    for (U k = 0; k < sizeof a.stack; ++k)
        if (a.stack[k] != b.stack[k]) return g_top - kSeedBelow + k;
    return 0;
}

U Hash(U salt = 0) {
    U h = (g_seed + g_log_n * 0x632BE5ABu + salt * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}
U Bytes(const void* p, unsigned n) {
    U h = 2166136261u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}
// A string's bytes to its NUL, at most 0x60.
U StrHash(const char* s) {
    unsigned n = 0;
    while (n < 0x60 && s[n] != 0) ++n;
    return Bytes(s, n) ^ n;
}

// --- A round's world ----------------------------------------------------------
// BOF3.CFG as fgets will hand it out, a line at a time.
constexpr unsigned kMaxLines = 20;   // a 21st reaches the return address, in both
char g_lines[kMaxLines][kCfgLineBytes];
unsigned g_line_count, g_line_next;
U g_open_null_odds;    // fopen answers null when Hash % this is 0
U g_cdrom_odds;        // GetDriveTypeA answers DRIVE_CDROM when Hash % this is 0
unsigned g_file_id;    // the streams handed out, by number
unsigned char g_files[4];

U FileId(const void* p) {
    for (unsigned i = 0; i < 4; ++i)
        if (p == &g_files[i]) return 0x100 + i;
    return Addr(p);
}

// What a callee may change under its caller.
void Disturb() {
    const U h = Hash(0xD157);
    if (h & 0x300) return;   // one call in four
    const U v = h >> 12;
    switch ((h >> 4) % 9) {
    case 0: std::memcpy(At(0x7E1BE8 + (v % 8) * 2), &v, 2); break;   // a pad word
    case 1: PutLong(kGameHwnd, v * 0x9E3779B1u); break;
    case 2: PutLong(kGameHInstance, v * 0x85EBCA6Bu); break;
    case 3: At(kGfxBufferIndex)[0] = static_cast<unsigned char>(v & 1 ? v >> 1 : v & 2); break;
    case 4: PutLong(kGfxCurrentEnv, Addr(g_env) + (v % 0x40) * 4); break;
    case 5: PutLong(kGfxOtPointers + (v % 8) * 4, v * 0x2545F491u); break;
    case 6: At(kCdRoot + v % 4)[0] = static_cast<unsigned char>(v >> 8); break;
    case 7: At(kGameQuitFlag)[0] = static_cast<unsigned char>(v); break;
    default: g_env[v % kEnvBytes] = static_cast<unsigned char>(v >> 8); break;
    }
}

// --- The stand-ins ------------------------------------------------------------
unsigned __cdecl StubPadRead() {
    Record(0x01);
    const U h = Hash(0x9AD);
    Disturb();
    switch (h % 4) {
    case 0: return h * 0x2545F491u;              // both words
    case 1: return 0;                            // nothing held
    default: return (h >> 8) & 0xFFFF;           // pad 1 only, as Pad_Read
    }
}

void* __cdecl StubOpen(const char* path, const char* mode) {
    Record(0x10, StrHash(path), Addr(mode));
    if (Hash(0x0BE) % g_open_null_odds == 0) return nullptr;
    return &g_files[g_file_id++ % 4];
}
char* __cdecl StubGets(char* buf, int n, void* stream) {
    Record(0x11, Rel(buf), static_cast<U>(n), FileId(stream));
    if (g_line_next >= g_line_count) return nullptr;
    const char* const line = g_lines[g_line_next++];
    int i = 0;
    for (; i < n - 1 && line[i] != 0; ++i) buf[i] = line[i];
    buf[i] = 0;
    return (Hash(0x6E7) & 3) ? buf : buf + 1;   // only null is tested
}
int __cdecl StubToInt(const char* s) {
    Record(0x12, Rel(s), StrHash(s));
    const U h = Hash(0xA70);
    switch (h % 4) {
    case 0: return 0;
    case 1: return 1;
    default: return static_cast<int>(h);
    }
}
int __cdecl StubScanPair(const char* s, const char* fmt, void* a, void* b) {
    Record(0x13, Rel(s), Addr(fmt), Rel(a), Rel(b), StrHash(s));
    const U h = Hash(0x5CA);
    const int count = static_cast<int>(h % 4) - 1;   // EOF, none, one, two
    const U first = (h & 0x10) ? (h >> 5) % 0x100 : h * 0x9E3779B1u;
    const U second = (h & 0x20) ? (h >> 7) % 0x10000 : h * 0x85EBCA6Bu;
    if (count >= 1) std::memcpy(a, &first, 4);
    if (count >= 2) std::memcpy(b, &second, 4);
    return count;
}
int __cdecl StubClose(void* stream) {
    Record(0x14, FileId(stream));
    return 0;
}
void __cdecl StubSetKeyTable(const void* table) { Record(0x15, Rel(table), Bytes(table, 0x80)); }
void __cdecl StubDefaultKeys() { Record(0x16); }

int __cdecl StubFormat(char* dst, const char* fmt, const char* a, const char* b) {
    Record(0x20, Addr(fmt), Addr(a), Addr(b), StrHash(a), StrHash(b));
    unsigned n = 0;
    for (const char* s : {a, b})
        for (unsigned i = 0; i < 0x20 && s[i] != 0 && n < 0x4F; ++i) dst[n++] = s[i];
    dst[n] = 0;
    Disturb();
    return static_cast<int>(n);
}
unsigned __stdcall StubDriveType(const char* root) {
    Record(0x21, Addr(root), Long(Addr(root)));
    const U h = Hash(0xD71);
    Disturb();
    if (h % g_cdrom_odds == 0) return kDriveCdrom;
    return (h >> 8) % 7;   // DRIVE_UNKNOWN .. DRIVE_RAMDISK, 5 among them
}
// Disc_Probe's copy reads GetDriveTypeA from here.
const void* g_slot_drive_type = reinterpret_cast<const void*>(&StubDriveType);

void __cdecl StubInitGeom() { Record(0x30); Disturb(); }
void __cdecl StubGeomOffset(long x, long y) { Record(0x31, static_cast<U>(x), static_cast<U>(y)); Disturb(); }
void __cdecl StubGeomScreen(long h) { Record(0x32, static_cast<U>(h)); Disturb(); }
void __cdecl StubDInput(void* hinstance, void* hwnd) { Record(0x33, Addr(hinstance), Addr(hwnd)); Disturb(); }
void __cdecl StubSound(void* hwnd) { Record(0x34, Addr(hwnd)); Disturb(); }
// Capcom pushes a whole dword (push 0); Port_DroppedCall reads the byte
// (symbols.toml, `unsigned char b`): the byte is compared.
void __cdecl StubDropped(unsigned char b) { Record(0x35, b); Disturb(); }
unsigned char* __cdecl StubDrawEnv(unsigned char* env, int x, int y, int w, int h) {
    Record(0x36, Addr(env), static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h));
    Disturb();
    return env;
}
unsigned char* __cdecl StubDispEnv(unsigned char* env, int x, int y, int w, int h) {
    Record(0x37, Addr(env), static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h));
    Disturb();
    return env;
}
void __cdecl StubInitBlock(unsigned char* block) { Record(0x38, Addr(block)); Disturb(); }
void __cdecl StubBeginFrame() { Record(0x39); Disturb(); }

void __cdecl StubClearOTagR(unsigned long* ot, int n) {
    Record(0x40, Addr(ot), static_cast<U>(n));
    // The block's DRAWENV bytes the caller stores after this call.
    const U h = Hash(0xC1E);
    if (h & 1) At(Addr(ot) - kBlockOt + 0x2C + (h >> 1) % 4)[0] = static_cast<unsigned char>(h >> 8);
    Disturb();
}
void __cdecl StubAddPrim(unsigned long* ot, unsigned long* prim, unsigned long* link_out) {
    Record(0x41, Addr(ot), Addr(prim), Addr(link_out));
    Disturb();
}

}  // namespace
}  // namespace shell

extern "C" void __cdecl Shell_FuzzStackBaseSeen() {
    shell::Record(0x3A, Shell_FuzzSeenEsp - shell::g_top);
    shell::Disturb();
}

namespace shell {
namespace {

const void* F(auto fn) { return reinterpret_cast<const void*>(fn); }
template <class To, class From>
To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<void*>(f)); }

const Callees kStubs = {
    StubPadRead,
    StubOpen, StubGets, StubToInt, StubScanPair, StubClose, StubSetKeyTable, StubDefaultKeys,
    StubFormat, Addr(&g_slot_drive_type),
    StubInitGeom, StubGeomOffset, StubGeomScreen, StubDInput, StubSound, StubDropped,
    StubDrawEnv, StubDispEnv, StubInitBlock, StubBeginFrame, Cast<void (__cdecl*)()>(&Shell_FuzzStackBase),
    StubClearOTagR, StubAddPrim,
};

// --- The copies, by capstone 2026-10-05; every jump stays inside -------------
enum Kind : unsigned { kLatch, kCfgLoad, kGameInit, kInitBlock, kLinkOTags, kDiscProbe, kDefaultKeys, kKinds };
const char* const kNames[kKinds] = {"Input_Latch", "Cfg_Load", "Game_Init", "Gfx_InitBufferBlock", "Gfx_LinkOTags",
                                    "Disc_Probe", "Cfg_SetDefaultKeys"};
void* g_clones[kKinds];

void* Clone(const char* name, U base, U size, std::initializer_list<bof3::CloneCall> calls) {
    return bof3::CloneOriginal(name, base, size, calls.begin(), static_cast<int>(calls.size()));
}

void CloneAll() {
    g_clones[kLatch] = Clone("Input_Latch", 0x4FC6A0, 0x4E, {{0x00, F(&StubPadRead), 0x5A9700}});
    g_clones[kCfgLoad] = Clone("Cfg_Load", 0x4FD030, 0xD4,
                               {{0x0E, F(&StubOpen), 0x5B9B6D},
                                {0x2B, F(&StubGets), 0x5B9ADA},
                                {0x55, F(&StubScanPair), 0x5B9AA6},
                                {0x64, F(&StubToInt), 0x5B9A9B},
                                {0x78, F(&StubToInt), 0x5B9A9B},
                                {0x8E, F(&StubGets), 0x5B9ADA},
                                {0x9B, F(&StubClose), 0x5B9993},
                                {0xAE, F(&StubSetKeyTable), 0x5A9860},
                                {0xCA, F(&StubDefaultKeys), 0x5A9880}});
    g_clones[kGameInit] = Clone("Game_Init", 0x4FD110, 0xE2,
                                {{0x00, F(&StubInitGeom), 0x5A7AA0},
                                 {0x0C, F(&StubGeomOffset), 0x5A7AE0},
                                 {0x16, F(&StubGeomScreen), 0x5A7B00},
                                 {0x28, F(&StubDInput), 0x5A94C0},
                                 {0x34, F(&StubSound), kSoundSetup},
                                 {0x3B, F(&StubDropped), 0x4DF820},
                                 {0x53, F(&StubDrawEnv), 0x5A7910},
                                 {0x6E, F(&StubDispEnv), 0x5A78E0},
                                 {0x8C, F(&StubDrawEnv), 0x5A7910},
                                 {0xA4, F(&StubDispEnv), 0x5A78E0},
                                 {0xAE, F(&StubInitBlock), 0x4FD200},
                                 {0xB8, F(&StubInitBlock), 0x4FD200},
                                 {0xD1, F(&StubBeginFrame), 0x4FD230},
                                 {0xDD, F(&Shell_FuzzStackBase), 0x5A9907}});   // the tail jmp
    g_clones[kInitBlock] = Clone("Gfx_InitBufferBlock", 0x4FD200, 0x24, {{0x0B, F(&StubClearOTagR), 0x5A7960}});
    g_clones[kLinkOTags] = Clone("Gfx_LinkOTags", 0x4FD290, 0x4A, {{0x32, F(&StubAddPrim), 0x5A7540}});
    g_clones[kDiscProbe] = Clone("Disc_Probe", 0x5A72C0, 0xB0,
                                 {{0x13, F(&StubOpen), 0x5B9B6D},
                                  {0x52, F(&StubFormat), 0x5B9380},
                                  {0x61, F(&StubOpen), 0x5B9B6D},
                                  {0x7E, F(&StubClose), 0x5B9993},
                                  {0x94, F(&StubClose), 0x5B9993}});
    g_clones[kDefaultKeys] = Clone("Cfg_SetDefaultKeys", 0x5A9880, 0x16, {});
    // mov edi, [GetDriveTypeA]: the disp32 at +0x25 moved to our slot.
    auto* const code = static_cast<unsigned char*>(g_clones[kDiscProbe]);
    U disp;
    std::memcpy(&disp, code + 0x25, sizeof disp);
    if (code[0x23] != 0x8B || code[0x24] != 0x3D || disp != kImportGetDriveType)
        bof3::Fatal("shell: Disc_Probe +0x23 is not mov edi, [0x%X]", (unsigned)kImportGetDriveType);
    disp = Addr(&g_slot_drive_type);
    std::memcpy(code + 0x25, &disp, sizeof disp);
}

const void* Ours(unsigned k) {
    switch (k) {
    case kLatch: return F(&Input_Latch);
    case kCfgLoad: return F(&Cfg_Load);
    case kGameInit: return F(&Game_Init);
    case kInitBlock: return F(&Gfx_InitBufferBlock);
    case kLinkOTags: return F(&Gfx_LinkOTags);
    case kDiscProbe: return F(&Disc_Probe);
    case kDefaultKeys: return F(&Cfg_SetDefaultKeys);
    default: bof3::Fatal("shell: no function for kind %u", k);
    }
}

// --- A round's input ------------------------------------------------------------
void RandomLine(char* line) {
    const unsigned n = Next() % 4 == 0 ? Next() % kCfgLineBytes : Next() % 8;
    for (unsigned i = 0; i < n; ++i) {
        const U r = Next();
        line[i] = (r & 3) ? static_cast<char>('0' + r % 10) : static_cast<char>(r >> 8 | 1);   // digits, mostly
    }
    line[n] = 0;
}

void Generate(unsigned k, U* args, unsigned* coverage) {
    for (const Region& r : kRegions)
        for (U a = r.at; a < r.at + r.bytes; ++a) At(a)[0] = static_cast<unsigned char>(Next() % 3 ? Next() : 0);
    for (unsigned i = 0; i < kEnvBytes; ++i) g_env[i] = static_cast<unsigned char>(Next());
    for (U a = g_top - kSeedBelow; a < g_top + kAbove; ++a) At(a)[0] = static_cast<unsigned char>(Next());
    std::memset(args, 0, 5 * sizeof(U));
    g_line_count = g_line_next = 0;
    g_open_null_odds = 6;
    g_cdrom_odds = 4;
    g_file_id = 0;
    switch (k) {
    case kLatch:
        // The old words equal to the new now and then: no edge.
        if (Next() % 4 == 0) std::memset(At(0x7E1BE8), 0, 0x10);
        break;
    case kCfgLoad: {
        // The line count's boundaries (0, 1, 2, 3) weighted; up to 20.
        const U r = Next() % 8;
        g_line_count = r < 4 ? r : Next() % (kMaxLines + 1);
        for (unsigned i = 0; i < g_line_count; ++i) RandomLine(g_lines[i]);
        coverage[0] += g_line_count > 2;
        coverage[1] += g_line_count == kMaxLines;
        break;
    }
    case kGameInit:
        break;
    case kInitBlock:
        args[0] = Addr(g_env) + Next() % 0x100;
        break;
    case kLinkOTags:
        if (Next() % 2) At(kGfxBufferIndex)[0] = static_cast<unsigned char>(Next() & 1);
        break;
    case kDiscProbe: {
        g_open_null_odds = 1 + Next() % 3;   // 1: every open fails
        g_cdrom_odds = 1 + Next() % 6;
        if (Next() % 4) std::memcpy(At(kCdRoot), "C:\\", 4);
        else At(kCdRoot + 3)[0] = 0;
        RandomLine(g_local);
        RandomLine(g_disc);
        args[0] = Next() % 2 ? 0x65DAB8 : Addr(g_local);   // the image's CAPCOM.AVI, or ours
        args[1] = Addr(g_disc);
        break;
    }
    case kDefaultKeys:
        break;
    default:
        break;
    }
}

}  // namespace

void SelfTest() {
    g_top = (Addr(g_stack) + kStackBytes - kAbove) & ~0xFu;
    CloneAll();
    static State saved, input, theirs, ours;
    Capture(saved);
    const Callees g_saved = g;
    g = kStubs;
    constexpr unsigned kPerKind = 3000;
    constexpr unsigned kRounds = kPerKind * kKinds;
    unsigned bad = 0, bad_per[kKinds] = {}, coverage[2] = {};
    unsigned counts[0x50] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kKinds;
        g_seed = round * 0x9E3779B9u + 0x4FD110u;
        U args[5];
        Generate(k, args, coverage);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_line_next = 0;
            g_file_id = 0;
            State& out = pass ? ours : theirs;
            const U result = Shell_CallOnStack(pass ? Ours(k) : g_clones[k], args, g_top);
            Capture(out);
            out.result = k == kDiscProbe ? result : 0;
            // The stack is compared only where the function's own frame and
            // what lies above it must agree: Cfg_Load's.
            if (k != kCfgLoad) std::memset(out.stack, 0, sizeof out.stack);
            else std::memset(out.stack, 0, kSeedBelow - kCfgCompareBelow);
        }
        const unsigned n = theirs.log_n < kLog ? theirs.log_n : kLog;
        for (unsigned i = 0; i < n; ++i) ++counts[theirs.log[i].what % 0x50];
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < n && std::memcmp(&theirs.log[first], &ours.log[first], sizeof(Entry)) == 0) ++first;
                const Entry& t = theirs.log[first < kLog ? first : 0];
                const Entry& o = ours.log[first < kLog ? first : 0];
                bof3::Log("shadow      shell MISMATCH: round %u, %s, result %08X / %08X, log %u / %u, memory at 0x%X, "
                          "call %u: %X(%X %X %X %X %X) / %X(%X %X %X %X %X)",
                          round, kNames[k], theirs.result, ours.result, theirs.log_n, ours.log_n,
                          (unsigned)FirstDifference(theirs, ours), first, t.what, t.a, t.b, t.c, t.d, t.e, o.what, o.a,
                          o.b, o.c, o.d, o.e);
            }
        }
    }
    g = g_saved;
    Apply(saved);
    bof3::Log("shadow      shell self-test: %u rounds (%u per kind, %u kinds), %u MISMATCHES; key lines %u, 20 lines %u; "
              "calls: pad %u, fopen %u, fgets %u, atoi %u, sscanf %u, fclose %u, key table %u, default keys %u, "
              "sprintf %u, drive type %u, Game_Init's %u..%u, stack base %u, ClearOTagR %u, AddPrim %u",
              kRounds, kPerKind, (unsigned)kKinds, bad, coverage[0], coverage[1], counts[0x01], counts[0x10],
              counts[0x11], counts[0x12], counts[0x13], counts[0x14], counts[0x15], counts[0x16], counts[0x20],
              counts[0x21], counts[0x30], counts[0x39], counts[0x3A], counts[0x40], counts[0x41]);
    for (unsigned k = 0; k < kKinds; ++k)
        if (bad_per[k]) bof3::Log("shadow      shell self-test: %s %u of %u rounds differ", kNames[k], bad_per[k], kPerKind);
    if (bad) bof3::Fatal("the shell functions differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace shell
