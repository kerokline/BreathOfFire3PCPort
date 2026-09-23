// BOF3X_SHADOW=save_menu: a differential fuzz of group X's 44 functions
// against byte-copies of Capcom's, once at start-up (docs/save-menu.md
// section 4).
//
// Each function alone: every relative call out of its copy is re-aimed at a
// recording stand-in, and ours is put on the same stand-ins through
// save_menu::g - the calls between this module's own functions included.
// The three dispatchers jump through tables in .data; those are filled with
// recorders for the test. Shop_Equip's jump table is inside its body: the
// copy's entries and its `jmp [eax*4 + table]` are moved onto the copy.
// Music_IsPlaying's DirectSound buffer is a fake COM object. Nothing touches
// a file: every file call, the CRT's _findfirst / _findnext, Crt_sprintf,
// Crt_malloc / Crt_free and Rand are stand-ins (this runs before BOF3.exe's C
// runtime is up, and no self-test may write a file).
//
// Stand-ins answer from a hash of the round and the call's index, so both
// sides see the same answers in the same order, and give their callers the
// side effects those read afterwards: Gfx_CommitPrim moves Gfx_PacketNext,
// the recalculation writes the maxima the inn copies, Save_ReadFile fills
// Save_Staging (with a right checksum half the time), Field_MemberSprite
// moves Sprite_Current, a sound or a transition moves the state byte.
//
// Compared per round: the regions the kind touches, byte for byte, the log of
// calls, and the result where there is one. Arguments whose upper bits the
// original takes from a register it never set (a byte pushed in a dword) are
// logged as the byte the callee reads.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/save_menu_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// Calls fn with ecx set: the two naked entries pass their caller's ecx on.
extern "C" std::uint32_t __cdecl SaveMenuFuzz_CallWithEcx(const void* fn, std::uint32_t ecx);
__asm__(
    ".globl _SaveMenuFuzz_CallWithEcx\n"
    "_SaveMenuFuzz_CallWithEcx:\n"
    "    movl 8(%esp), %ecx\n"
    "    movl 4(%esp), %eax\n"
    "    jmp *%eax\n");

namespace save_menu {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void PutByte(U address, U v) { At(address)[0] = static_cast<unsigned char>(v); }

// --- Random numbers of our own ----------------------------------------------
U g_rand = 0x58880A5Au;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}
U Pick(std::initializer_list<U> values) { return values.begin()[Next() % values.size()]; }

// --- Buffers of our own -------------------------------------------------------
constexpr unsigned kPrimBytes = 0x400, kHeapBytes = 0x2400, kFileBytes = 0x2000, kSpriteBytes = 0x100,
                   kNamesBytes = 0x400;
unsigned char g_prim[kPrimBytes];       // what Gfx_PacketNext points into
unsigned char g_heap[kHeapBytes];       // what the Crt_malloc stand-in hands out
unsigned char g_file[kFileBytes];       // the "file" File_Read copies from
unsigned char g_sprite[kSpriteBytes];   // a second sprite for Sprite_Current / Field_State
unsigned char g_names[kNamesBytes];     // a stream name table
unsigned char g_rate[8];                // Shop_PriceRate's out word, with room either side

// --- The regions ---------------------------------------------------------------
struct Region { U at, bytes; };
enum : unsigned {
    rFlow, rStream, rStep, rWait, rMisc, rDirectory, rStaging, rMenuFlags, rButtons, rSlot, rGrowth, rCounters,
    rBlock, rPath, rArea, rSummaries, rPosition, rShopWin, rShopWin2, rObj, rSprite, rClut, rChoice, rMusicBuf,
    rDatNames, rStreamTables, rPacketNext, rPrim, rHeap, rSpriteBuf, rNames, rRate, rFile, rInput, rMembers, kRegions
};
Region g_regions[kRegions];
void DefineRegions() {
    g_regions[rFlow] = {0x6BDF80, 0x20};
    g_regions[rStream] = {0x6BDE40, 0x18};
    g_regions[rStep] = {0x66C7E8, 4};
    g_regions[rWait] = {0x66C810, 4};
    g_regions[rMisc] = {0x929EC0, 0x50};
    g_regions[rDirectory] = {0x929F40, 0x1A0};
    g_regions[rStaging] = {0x92A0E0, 0x1C00};
    g_regions[rMenuFlags] = {0x7DEE40, 8};
    g_regions[rButtons] = {0x903580, 0x14};
    g_regions[rSlot] = {0x9036D4, 4};
    g_regions[rGrowth] = {0x903640, 0x100};
    g_regions[rCounters] = {0x903848, 4};
    g_regions[rBlock] = {0x9039A0, 0x10F0};
    g_regions[rPath] = {0x904BA0, 0x20};
    g_regions[rArea] = {0x904EFC, 4};
    g_regions[rSummaries] = {0x905BA0, 0x200};
    g_regions[rPosition] = {0x8034D0, 0x20};
    g_regions[rShopWin] = {0x803160, 0x58};
    g_regions[rShopWin2] = {0x803430, 0x30};
    g_regions[rObj] = {0x802D40, 0x100};
    g_regions[rSprite] = {0x937F88, 0xC};
    g_regions[rClut] = {at::kClutSource, 0xC000};
    g_regions[rChoice] = {0x6BC880, 0x40};
    g_regions[rMusicBuf] = {0x7DE3C0, 0xC};
    g_regions[rDatNames] = {at::kDatNames, 0x40};
    g_regions[rStreamTables] = {at::kStreamTables, 0x50};
    g_regions[rPacketNext] = {at::kPacketNext, 4};
    g_regions[rPrim] = {Addr(g_prim), kPrimBytes};
    g_regions[rHeap] = {Addr(g_heap), kHeapBytes};
    g_regions[rSpriteBuf] = {Addr(g_sprite), kSpriteBytes};
    g_regions[rNames] = {Addr(g_names), kNamesBytes};
    g_regions[rRate] = {Addr(g_rate), sizeof g_rate};
    g_regions[rFile] = {Addr(g_file), kFileBytes};
    g_regions[rInput] = {0x7E1BE8, 8};
    g_regions[rMembers] = {0x802E80, 0x540};   // Field_Members' bytes of five working records
}
constexpr std::uint64_t Bit(unsigned r) { return std::uint64_t{1} << r; }

// --- The log -----------------------------------------------------------------
struct Entry { U what, a, b, c, d, e, f; };
constexpr unsigned kLog = 160;
Entry g_log[kLog];
unsigned g_log_n;
U g_seed;

U Hash(U salt = 0) {
    U h = (g_seed + g_log_n * 0x632BE5ABu + salt * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0, U f = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f};
    ++g_log_n;
}

// --- Round state -----------------------------------------------------------------
unsigned g_kind;
U g_load_done_after;    // File_LoadDone answers 0 this many times, then 1
U g_find_left;          // _findnext succeeds this many more times
U g_file_size;          // what File_Size answers
U g_equip_target;       // the slot byte Shop_Equip will read back this round

// A stand-in's effect on the state byte and the counter its caller reads
// after it - one call in four.
void DisturbFlow() {
    const U h = Hash(0xF10);
    if (h & 3) return;
    switch ((h >> 2) % 4) {
    case 0: PutByte(at::kFlowState, h >> 8); break;
    case 1: PutByte(at::kAnswer, (h >> 8) & 1); break;
    case 2: PutLong(at::kSlot, (h >> 8) % 20); break;
    default: PutLong(at::kSlotTop, (h >> 8) % 20); break;
    }
}
// Gfx_PacketNext moved within g_prim, as a commit moves it.
void MovePacket() {
    const U h = Hash(0x9AC);
    PutLong(at::kPacketNext, Addr(g_prim) + (h % 0x30) * 4);
}

// --- The stand-ins -------------------------------------------------------------
int __cdecl StubFileOpen(const char* path, int a, int b) {
    // The name is compared by its first bytes, which the sprintf stand-in wrote.
    U head = 0;
    std::memcpy(&head, path, 4);
    Record(0x01, head, static_cast<U>(a), static_cast<U>(b), Addr(path) == at::kPath);
    const U h = Hash(0x0E1);
    return (h % 5 == 0) ? -1 : static_cast<int>(h % 16);
}
int __cdecl StubFileSize(int handle) {
    Record(0x02, static_cast<U>(handle));
    return static_cast<int>(g_file_size);
}
int __cdecl StubFileSeek(int handle, int offset) {
    Record(0x03, static_cast<U>(handle), static_cast<U>(offset));
    return static_cast<int>(Hash(0x5EE));
}
unsigned __cdecl StubFileRead(int handle, void* dst, unsigned size) {
    Record(0x04, static_cast<U>(handle), Addr(dst), size);
    // The whole "file", whatever the size asked: a chunk header the walk
    // reaches just below the end must not run into noise.
    if (Addr(dst) >= Addr(g_heap) && Addr(dst) < Addr(g_heap) + kHeapBytes) {
        const U room = Addr(g_heap) + kHeapBytes - Addr(dst);
        std::memcpy(dst, g_file, room < kFileBytes ? room : kFileBytes);
    }
    return Hash(0x4EA);
}
void __cdecl StubFileClose(int handle) { Record(0x05, static_cast<U>(handle)); }
void* __cdecl StubMalloc(unsigned n) {
    Record(0x06, n);
    return g_heap + (Hash(0x3A1) % 8) * 4;
}
void __cdecl StubFree(void* p) { Record(0x07, Addr(p)); }
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const U arg = va_arg(ap, U);
    va_end(ap);
    Record(0x08, Addr(dst) == at::kPath ? 1 : 0, Addr(fmt), arg);
    // Something short and deterministic for File_Open to see.
    const U h = Hash(0x5F1) ^ arg;
    for (int i = 0; i < 8; ++i) dst[i] = static_cast<char>('A' + ((h >> (i * 3)) & 15));
    dst[8] = 0;
    return 8;
}
long __cdecl StubFindFirst(const char* pattern, void* found) {
    Record(0x09, Addr(pattern), 0);
    const U h = Hash(0xF1F);
    if (h % 6 == 0) return -1;
    auto* f = static_cast<unsigned char*>(found);
    for (U i = 0; i < 0x14; ++i) f[i] = static_cast<unsigned char>(Hash(0x100 + i));
    const U len = Hash(0xF1E) % 5 == 0 ? Hash(0xF1D) % 0x28 : 8 + Hash(0xF1D) % 8;
    for (U i = 0; i < len; ++i) f[0x14 + i] = static_cast<unsigned char>(0x21 + Hash(0x200 + i) % 0x5E);
    f[0x14 + len] = 0;
    return static_cast<long>(h | 1);
}
int __cdecl StubFindNext(long handle, void* found) {
    Record(0x0A, static_cast<U>(handle));
    if (g_find_left == 0) return static_cast<int>(Hash(0xF2E) | 1);
    --g_find_left;
    auto* f = static_cast<unsigned char*>(found);
    for (U i = 0; i < 0x14; ++i) f[i] = static_cast<unsigned char>(Hash(0x300 + i));
    const U len = Hash(0xF2F) % 7 == 0 ? Hash(0xF2D) % 0x24 : 8 + Hash(0xF2D) % 8;
    for (U i = 0; i < len; ++i) f[0x14 + i] = static_cast<unsigned char>(0x21 + Hash(0x400 + i) % 0x5E);
    f[0x14 + len] = 0;
    return 0;
}
int __cdecl StubRand() {
    Record(0x0B);
    const U h = Hash(0x4A4);
    return (h % 9 == 0) ? static_cast<int>(h) : static_cast<int>(h & 0x7FFF);
}
void __cdecl StubLoadBank(unsigned tag, const void* payload, unsigned size) {
    Record(0x0C, tag, Addr(payload), size);
}
void __cdecl StubPlayEffect(unsigned short id) {
    Record(0x0D, id);
    DisturbFlow();
}
void __cdecl StubMusicStop() { Record(0x0E); }
void __cdecl StubMusicStart(const void* file, unsigned size, int loops) {
    Record(0x0F, Addr(file), size, static_cast<U>(loops));
}
void __cdecl StubMusicVolume(float v) {
    U bits;
    std::memcpy(&bits, &v, 4);
    Record(0x10, bits);
}
int __cdecl StubMusicPlaying() {
    Record(0x11);
    return static_cast<int>(Hash(0x11A) % 3 == 0 ? Hash(0x11B) : Hash(0x11B) & 1);
}
void __cdecl StubVoicePlay(const void* p) { Record(0x12, Addr(p)); }
int __cdecl StubVoicePlaying() {
    Record(0x13);
    return static_cast<int>(Hash(0x13A) % 3 == 0 ? Hash(0x13B) : Hash(0x13B) & 1);
}
unsigned char __cdecl StubClearStatus(unsigned id, unsigned mask, unsigned battle) {
    Record(0x14, id & 0xFF, mask, battle);
    // The status word it would clear, to show the inn does not read it back.
    PutByte(at::kCharRecords + (id & 7) * at::kCharStride + 0x10, Hash(0x14A));
    return static_cast<unsigned char>(Hash(0x14B));
}
void __cdecl StubRecalc(unsigned char* record) {
    Record(0x15, Addr(record));
    // The maxima the inn copies after it.
    if (Addr(record) >= at::kCharRecords && Addr(record) < at::kCharRecords + 8 * at::kCharStride) {
        PutWord(Addr(record) + 0x20, Hash(0x15A));
        PutWord(Addr(record) + 0x22, Hash(0x15B));
        PutByte(Addr(record) + 0x2E, Hash(0x15C));
    }
}
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(0x16, Addr(bits), index);
    return static_cast<unsigned char>(Hash(0x16A) % 3 == 0 ? 0 : Hash(0x16B) | 1);
}
U g_party;   // the party count this round
int __cdecl StubPartyCount(unsigned slot) {
    Record(0x17, slot);
    // Its low byte is what the callers read; the rest is noise.
    return static_cast<int>((Hash(0x17A) & 0xFFFFFF00u) | g_party);
}
unsigned __cdecl StubItemPrice(unsigned kind, unsigned item) {
    Record(0x18, kind, item);
    return Hash(0x18A);
}
unsigned char __cdecl StubShopFlag(unsigned a) {
    Record(0x19, a);
    return static_cast<unsigned char>(Hash(0x19A) % 2 ? 0 : Hash(0x19B) | 1);
}
unsigned char __cdecl StubInvRemove(unsigned kind, unsigned item, unsigned count, unsigned zero) {
    Record(0x1A, kind, item, count, zero);
    // The slot byte Shop_Equip reads after it - that one half the time.
    const U h = Hash(0x1AA);
    if (h % 2 == 0) PutByte(g_equip_target != 0 ? g_equip_target : at::kCharRecords + 0x12, h >> 24);
    else if (h % 3 == 0) PutByte(at::kCharRecords + (h >> 8) % 16 * at::kCharStride + 0x12 + (h >> 16) % 6, h >> 24);
    return static_cast<unsigned char>(h);
}
unsigned char __cdecl StubInvAdd(unsigned kind, unsigned item, unsigned count, unsigned zero) {
    Record(0x1B, kind, item & 0xFF, count, zero);
    return static_cast<unsigned char>(Hash(0x1BA));
}
void __cdecl StubWindowFrame(int x, int y, int w, int h, int z, unsigned colour) {
    Record(0x1C, static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h), static_cast<U>(z),
           colour & 0xFF);
}
void __cdecl StubWindowBox(int x, int y, int w, int h) {
    Record(0x1D, static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h));
}
void __cdecl StubWindowBack(unsigned style) { Record(0x1E, style & 0xFF); }
void __cdecl StubWindowFrame5(int x, int y, int w, int h, unsigned colour) {
    Record(0x1F, static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h), colour & 0xFF);
}
void __cdecl StubSlotDraw(unsigned slot, int x, int y, const unsigned char* summary) {
    Record(0x20, slot, static_cast<U>(x), static_cast<U>(y), Addr(summary));
    DisturbFlow();
}
void __cdecl StubSlotCursor(int x, int y, int w, int h, unsigned flag, int n) {
    Record(0x21, static_cast<U>(x), static_cast<U>(y), static_cast<U>(w), static_cast<U>(h), flag, static_cast<U>(n));
}
unsigned char __cdecl StubYesNo() {
    Record(0x22);
    DisturbFlow();
    return static_cast<unsigned char>(Hash(0x22A) % 3 == 0 ? 0 : Hash(0x22B) | 1);
}
const unsigned char* __cdecl StubMsgPtr(unsigned id) {
    Record(0x23, id & 0xFFFF);
    return At(0x700000u + (Hash(0x23A) & 0xFFFF));
}
const unsigned char* __cdecl StubTextDraw(int x, int y, int color, int count, const unsigned char* text) {
    Record(0x24, static_cast<U>(x), static_cast<U>(y), static_cast<U>(color), static_cast<U>(count), Addr(text));
    return At(Hash(0x24A));
}
void __cdecl StubDrawHand(int x, int y, int z) {
    Record(0x25, static_cast<U>(x), static_cast<U>(y), static_cast<U>(z));
}
unsigned __cdecl StubAutoRepeat(unsigned pressed) {
    Record(0x26, pressed);
    const U h = Hash(0x26A);
    return (h & 0xFFFFAFFFu) | (Hash(0x26B) & 0x5000);
}
void __cdecl StubSetTile(unsigned char* p) {
    Record(0x27, Addr(p));
    for (U i = 0; i < 0x20; ++i) p[i] = static_cast<unsigned char>(Hash(0x270 + i));
}
void __cdecl StubSetSprt(unsigned char* p) {
    Record(0x28, Addr(p));
    for (U i = 0; i < 0x20; ++i) p[i] = static_cast<unsigned char>(Hash(0x280 + i));
}
void __cdecl StubSetSemi(unsigned char* p, unsigned abe) { Record(0x29, Addr(p), abe); }
void __cdecl StubSetDrawMode(unsigned char* p, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(0x2A, Addr(p), static_cast<U>(dfe), static_cast<U>(dtd), tpage, static_cast<U>(tw));
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(0x2B, tp, abr, static_cast<U>(x), static_cast<U>(y));
    return Hash(0x2BA);
}
unsigned __cdecl StubGetClut(int x, int y) {
    Record(0x2C, static_cast<U>(x), static_cast<U>(y));
    return Hash(0x2CA);
}
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(0x2D, slot, size, Long(at::kPacketNext) - Addr(g_prim));
    MovePacket();
}
void __cdecl StubTransition(unsigned char kind) {
    Record(0x2E, kind);
    DisturbFlow();
}
void __cdecl StubMemberSprite(unsigned member, unsigned slot) {
    Record(0x2F, member, slot);
    const U h = Hash(0x2FA);
    if (h & 1) PutLong(at::kSpriteCurrent, (h & 2) ? Addr(g_sprite) : at::kObjTrio);
    if (h & 4) PutLong(at::kFieldState, (h & 8) ? Addr(g_sprite) : at::kObjTrio);
}
void __cdecl StubSetAnimation(unsigned char a) { Record(0x30, a); }
void __cdecl StubInitCharacters() { Record(0x31); }
void __cdecl StubScenarioStart(int c) { Record(0x32, static_cast<U>(c)); }
void __cdecl StubScenarioLoad() {
    Record(0x33);
    PutByte(at::kFlowState, Hash(0x33A));
}
void __cdecl StubPartyLoad(unsigned s) { Record(0x34, s); }
void __cdecl StubPartySelect(unsigned set, unsigned mode) {
    Record(0x35, set & 0xFF, mode);
    PutByte(at::kPartySet, Hash(0x35A));
}
int __cdecl StubLoadDone() {
    Record(0x36);
    if (g_load_done_after != 0) {
        --g_load_done_after;
        return 0;
    }
    return static_cast<int>(Hash(0x36A) | 1);
}
void __cdecl StubTaskSleep(int n) { Record(0x37, static_cast<U>(n)); }
void __cdecl StubTaskRestart(U entry) { Record(0x38, entry); }
int __cdecl StubListFiles() {
    Record(0x39);
    const U h = Hash(0x39A);
    return static_cast<int>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 + h % 16 : h);
}
int __cdecl StubReadFile(const char* path, unsigned size, int offset) {
    Record(0x3A, Addr(path), size, static_cast<U>(offset));
    const U h = Hash(0x3AA);
    if (h % 4 == 0) return -1;
    // Save_Staging as a file would leave it; the checksum right half the time.
    for (U i = 0; i < at::kBlockBytes; i += 4) PutLong(at::kStaging + i, Hash(0x1000 + i));
    U sum = 0;
    for (U i = 0; i < at::kBlockBytes; ++i)
        if (i != 0x70 && i != 0x71) sum += At(at::kStaging)[i];
    PutWord(at::kChecksum, (h & 2) ? sum : sum + 1 + (h >> 20));
    return (h & 4) ? 0 : static_cast<int>(h >> 8);
}
void __cdecl StubReadSummaries() { Record(0x3B); }
void __cdecl StubDrawFrame() {
    Record(0x3C);
    DisturbFlow();
}
void __cdecl StubDrawRows(unsigned glow) {
    Record(0x3D, glow);
    const U h = Hash(0x3DA);
    if (h % 4 == 0) PutByte(at::kCounter, h >> 8);
    if (h % 4 == 1) PutByte(at::kCursor, h >> 8);
    if (h % 4 == 2) PutByte(at::kRowBright + (h >> 8) % 3, h >> 16);
}
void __cdecl StubDrawRow(int y, unsigned row, unsigned glow) { Record(0x3E, static_cast<U>(y), row, glow); }
void __cdecl StubDrawPiece(int x, int y, unsigned index, unsigned slot) {
    Record(0x3F, static_cast<U>(x), static_cast<U>(y), index, slot);
    MovePacket();
}
void __cdecl StubDrawSlots(int x, int y, unsigned highlight) {
    Record(0x40, static_cast<U>(x), static_cast<U>(y), highlight);
}
void __cdecl StubRollGrowth(unsigned m) { Record(0x41, m & 0xFF); }
unsigned char __cdecl StubRollSum(unsigned lo, unsigned hi, unsigned n) {
    Record(0x42, lo & 0xFF, hi & 0xFF, n & 0xFF);
    return static_cast<unsigned char>(Hash(0x42A));
}

const Callees kStubs = {
    StubFileOpen, StubFileSize, StubFileSeek, StubFileRead, StubFileClose, StubMalloc, StubFree, StubSprintf,
    StubFindFirst, StubFindNext, StubRand,
    StubLoadBank, StubPlayEffect, StubMusicStop, StubMusicStart, StubMusicVolume, StubMusicPlaying,
    StubVoicePlay, StubVoicePlaying,
    StubClearStatus, StubRecalc, StubFlagsTest, StubPartyCount, StubItemPrice, StubShopFlag, StubInvRemove,
    StubInvAdd,
    StubWindowFrame, StubWindowBox, StubWindowBack, StubWindowFrame5, StubSlotDraw, StubSlotCursor, StubYesNo,
    StubMsgPtr, StubTextDraw, StubDrawHand, StubAutoRepeat,
    StubSetTile, StubSetSprt, StubSetSemi, StubSetDrawMode, StubGetTPage, StubGetClut, StubCommit,
    StubTransition, StubMemberSprite, StubSetAnimation, StubInitCharacters, StubScenarioStart, StubScenarioLoad,
    StubPartyLoad, StubPartySelect, StubLoadDone, StubTaskSleep, StubTaskRestart,
    StubListFiles, StubReadFile, StubReadSummaries,
    StubDrawFrame, StubDrawRows, StubDrawRow, StubDrawPiece, StubDrawSlots, StubRollGrowth, StubRollSum,
};

// The dispatch tables' recorders.
template <unsigned N> void __cdecl StubTarget() {
    Record(0x80 + N);
    DisturbFlow();
}
using Target = void (__cdecl*)();
template <unsigned... N> struct Targets {
    static constexpr Target kAll[sizeof...(N)] = {&StubTarget<N>...};
};
constexpr unsigned kTableEntries = 21;   // 0x6671F4..0x667247: six steps, then fifteen states
const Target* TargetTable() {
    return Targets<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20>::kAll;
}

// --- The fake DirectSound buffer ------------------------------------------------
struct Fake { const void* const* vtable; };
Fake g_buffer;
long __stdcall FakeGetStatus(void* self, U* status) {
    Record(0x50, self == &g_buffer ? 1 : 0);
    const U h = Hash(0x50A);
    if (h % 3) *status = Hash(0x50B);   // a failure leaves it as it was
    return static_cast<long>(h % 3 ? 0 : 0x88780096u);
}
template <int S> long __stdcall FakeUnexpected(void*) {
    bof3::Fatal("save_menu: self-test reached DirectSound slot %d, which nothing fakes", S);
}
const void* g_vtable[16];
template <int... S> void FillVtable(std::integer_sequence<int, S...>) {
    ((g_vtable[S] = reinterpret_cast<const void*>(&FakeUnexpected<S>)), ...);
}

// --- The kinds -------------------------------------------------------------------
enum Kind : unsigned {
    kLoadBankFile, kReadFile, kListFiles, kClutRow, kRestore, kChoices, kBlack, kPriceRate, kScale, kSell,
    kInitWin, kEquip, kDrawRows, kDrawRow, kRollGrowth, kRollSum, kDrawFrame, kDrawPiece, kDrawSlots,
    kSummaries, kLoadStream, kStreamDone, kMusicPlaying, kStep, kBegin, kMenu, kOpen, kOpenWait, kFrameUp,
    kRowsStart, kFadeIn, kChoose, kLeave, kNewGame, kLoad, kList, kLOpen, kLChoose, kConfirm, kRead, kError,
    kLoaded, kApply, kEnter, kKinds
};
struct Info {
    const char* name;
    U base, size;
    const void* ours;
    std::uint64_t regions;
};
constexpr std::uint64_t kCommon = Bit(rFlow) | Bit(rStep) | Bit(rWait) | Bit(rMisc) | Bit(rMenuFlags) |
                                  Bit(rSlot) | Bit(rPosition) | Bit(rPacketNext) | Bit(rPrim) | Bit(rInput);
const void* F(auto fn) { return reinterpret_cast<const void*>(fn); }
const Info kInfo[kKinds] = {
    {"Snd_LoadBankFile", 0x454770, 0x9A, F(&Snd_LoadBankFile), kCommon | Bit(rDatNames) | Bit(rHeap) | Bit(rFile)},
    {"Save_ReadFile", 0x454820, 0x45, F(&Save_ReadFile), kCommon},
    {"Save_ListFiles", 0x4548B0, 0xA1, F(&Save_ListFiles), kCommon | Bit(rDirectory) | Bit(rStaging)},
    {"Gfx_ClutStripCopyRow", 0x4549F0, 0x2E, F(&Gfx_ClutStripCopyRow), kCommon | Bit(rClut) | Bit(rSprite)},
    {"Party_RestoreAll", 0x580630, 0xB1, F(&Party_RestoreAll), kCommon | Bit(rBlock)},
    {"SaveMenu_DrawChoices", 0x5808E0, 0x88, F(&SaveMenu_DrawChoices), kCommon | Bit(rChoice) | Bit(rBlock)},
    {"Menu_DrawBlackScreen", 0x580970, 0x43, F(&Menu_DrawBlackScreen), kCommon},
    {"Shop_PriceRate", 0x583020, 0xA3, F(&Shop_PriceRate), kCommon | Bit(rObj) | Bit(rBlock) | Bit(rRate) | Bit(rMembers)},
    {"Shop_ScalePrice", 0x5830D0, 0x23, F(&Shop_ScalePrice), kCommon},
    {"Shop_SellPrice", 0x583100, 0x40, F(&Shop_SellPrice), kCommon},
    {"Shop_InitWindows", 0x583140, 0xCB, F(&Shop_InitWindows), kCommon | Bit(rShopWin) | Bit(rShopWin2)},
    {"Shop_Equip", 0x583210, 0x138, F(&Shop_Equip), kCommon | Bit(rBlock)},
    {"TitleMenu_DrawRows", 0x588880, 0x4D, F(&TitleMenu_DrawRows), kCommon},
    {"TitleMenu_DrawRow", 0x5888D0, 0x1E2, F(&TitleMenu_DrawRow), kCommon},
    {"NewGame_RollGrowth", 0x588AC0, 0xDE, F(&NewGame_RollGrowth), kCommon | Bit(rGrowth)},
    {"NewGame_RollSum", 0x588BA0, 0x56, F(&NewGame_RollSum), kCommon},
    {"TitleMenu_DrawFrame", 0x588C00, 0x83, F(&TitleMenu_DrawFrame), kCommon},
    {"TitleMenu_DrawPiece", 0x588C90, 0x8E, F(&TitleMenu_DrawPiece), kCommon},
    {"SaveMenu_DrawSlots", 0x588D20, 0x9D, F(&SaveMenu_DrawSlots), kCommon | Bit(rSummaries)},
    {"Save_ReadSummaries", 0x588DC0, 0xB0, F(&Save_ReadSummaries), kCommon | Bit(rDirectory) | Bit(rStaging) | Bit(rSummaries)},
    {"Sound_LoadStream", 0x587910, 0xE3, F(&Sound_LoadStream),
     kCommon | Bit(rStream) | Bit(rBlock) | Bit(rStreamTables) | Bit(rNames) | Bit(rHeap)},
    {"Sound_StreamDone", 0x587A00, 0x1B, F(&Sound_StreamDone), kCommon | Bit(rStream)},
    {"Music_IsPlaying", 0x5A7020, 0x2A, F(&Music_IsPlaying), kCommon | Bit(rMusicBuf)},
    {"TitleFlow_Step", 0x587DB0, 0xF, F(&TitleFlow_Step), kCommon},
    {"TitleFlow_Begin", 0x587DC0, 0xD4, F(&TitleFlow_Begin),
     kCommon | Bit(rBlock) | Bit(rSummaries) | Bit(rObj) | Bit(rSprite) | Bit(rSpriteBuf)},
    {"TitleFlow_Menu", 0x587EA0, 0xE, F(&TitleFlow_Menu), kCommon},
    {"TitleMenu_Open", 0x587EB0, 0x19, F(&TitleMenu_Open), kCommon},
    {"TitleMenu_OpenWait", 0x587ED0, 0x1A, F(&TitleMenu_OpenWait), kCommon},
    {"TitleMenu_FrameUp", 0x587EF0, 0x19, F(&TitleMenu_FrameUp), kCommon},
    {"TitleMenu_RowsStart", 0x587F10, 0x28, F(&TitleMenu_RowsStart), kCommon},
    {"TitleMenu_RowsFadeIn", 0x587F40, 0x59, F(&TitleMenu_RowsFadeIn), kCommon},
    {"TitleMenu_Choose", 0x587FA0, 0xF6, F(&TitleMenu_Choose), kCommon | Bit(rButtons)},
    {"TitleMenu_Leave", 0x5880A0, 0x31, F(&TitleMenu_Leave), kCommon},
    {"TitleFlow_NewGame", 0x5880E0, 0x48, F(&TitleFlow_NewGame), kCommon | Bit(rBlock) | Bit(rArea)},
    {"TitleFlow_Load", 0x588130, 0xE, F(&TitleFlow_Load), kCommon},
    {"LoadMenu_List", 0x588140, 0x4A, F(&LoadMenu_List), kCommon},
    {"LoadMenu_Open", 0x588190, 0x5C, F(&LoadMenu_Open), kCommon | Bit(rBlock)},
    {"LoadMenu_Choose", 0x5881F0, 0x149, F(&LoadMenu_Choose), kCommon | Bit(rBlock) | Bit(rButtons) | Bit(rSummaries)},
    {"LoadMenu_Confirm", 0x588340, 0x72, F(&LoadMenu_Confirm), kCommon | Bit(rBlock)},
    {"LoadMenu_Read", 0x5883C0, 0xD7, F(&LoadMenu_Read), kCommon | Bit(rBlock) | Bit(rStaging) | Bit(rPath)},
    {"LoadMenu_Error", 0x5884A0, 0xB3, F(&LoadMenu_Error), kCommon | Bit(rBlock) | Bit(rButtons)},
    {"LoadMenu_Loaded", 0x588560, 0x66, F(&LoadMenu_Loaded), kCommon | Bit(rBlock)},
    {"LoadMenu_Apply", 0x5885D0, 0x222, F(&LoadMenu_Apply), kCommon | Bit(rBlock) | Bit(rButtons) | Bit(rCounters)},
    {"TitleFlow_EnterGame", 0x588800, 0x79, F(&TitleFlow_EnterGame), kCommon | Bit(rGrowth) | Bit(rBlock)},
};

void* g_clones[kKinds];

struct Call { U offset, target; };
const void* StubFor(U target) {
    switch (target) {
    case 0x5A7380: return F(&StubFileOpen);
    case 0x5A74D0: return F(&StubFileSize);
    case 0x5A74F0: return F(&StubFileSeek);
    case 0x5A7470: return F(&StubFileRead);
    case 0x5A7510: return F(&StubFileClose);
    case 0x5B9660: return F(&StubMalloc);
    case 0x5B9577: return F(&StubFree);
    case 0x5B9380: return F(&StubSprintf);
    case kFindFirst: return F(&StubFindFirst);
    case kFindNext: return F(&StubFindNext);
    case 0x5B93D2: return F(&StubRand);
    case 0x587CD0: return F(&StubLoadBank);
    case 0x587740: return F(&StubPlayEffect);
    case 0x5A7050: return F(&StubMusicStop);
    case 0x5A6CC0: return F(&StubMusicStart);
    case 0x5A6FB0: return F(&StubMusicVolume);
    case 0x5A7020: return F(&StubMusicPlaying);
    case kVoicePlay: return F(&StubVoicePlay);
    case kVoiceIsPlaying: return F(&StubVoicePlaying);
    case 0x590F60: return F(&StubClearStatus);
    case kRecalcStats: return F(&StubRecalc);
    case 0x57C140: return F(&StubFlagsTest);
    case 0x531BB0: return F(&StubPartyCount);
    case kItemPrice: return F(&StubItemPrice);
    case kShopFlag: return F(&StubShopFlag);
    case kInventoryRemove: return F(&StubInvRemove);
    case kInventoryAdd: return F(&StubInvAdd);
    case kWindowFrame: return F(&StubWindowFrame);
    case kWindowBox: return F(&StubWindowBox);
    case kWindowBack: return F(&StubWindowBack);
    case kWindowFrame5: return F(&StubWindowFrame5);
    case kSlotDraw: return F(&StubSlotDraw);
    case kSlotCursor: return F(&StubSlotCursor);
    case kMenuYesNo: return F(&StubYesNo);
    case 0x497740: return F(&StubMsgPtr);
    case 0x516B30: return F(&StubTextDraw);
    case kDrawHand: return F(&StubDrawHand);
    case 0x461EB0: return F(&StubAutoRepeat);
    case 0x5A7740: return F(&StubSetTile);
    case 0x5A7710: return F(&StubSetSprt);
    case 0x5A7780: return F(&StubSetSemi);
    case 0x5A77C0: return F(&StubSetDrawMode);
    case 0x5A79A0: return F(&StubGetTPage);
    case 0x5A79E0: return F(&StubGetClut);
    case 0x461E50: return F(&StubCommit);
    case 0x495040: return F(&StubTransition);
    case 0x533BA0: return F(&StubMemberSprite);
    case 0x5891F0: return F(&StubSetAnimation);
    case 0x437820: return F(&StubInitCharacters);
    case 0x56D5E0: return F(&StubScenarioStart);
    case 0x56D670: return F(&StubScenarioLoad);
    case 0x533CE0: return F(&StubPartyLoad);
    case 0x536AC0: return F(&StubPartySelect);
    case 0x454810: return F(&StubLoadDone);
    case 0x5A9949: return F(&StubTaskSleep);
    case kTaskRestart: return F(&StubTaskRestart);
    case 0x4548B0: return F(&StubListFiles);
    case 0x454820: return F(&StubReadFile);
    case 0x588DC0: return F(&StubReadSummaries);
    case 0x588C00: return F(&StubDrawFrame);
    case 0x588880: return F(&StubDrawRows);
    case 0x5888D0: return F(&StubDrawRow);
    case 0x588C90: return F(&StubDrawPiece);
    case 0x588D20: return F(&StubDrawSlots);
    case 0x588AC0: return F(&StubRollGrowth);
    case 0x588BA0: return F(&StubRollSum);
    default: bof3::Fatal("save_menu: no stand-in for 0x%X", (unsigned)target);
    }
}

// The relative calls of each copy, by capstone 2026-09-23 (docs/save-menu.md
// section 4): the offset of the E8 byte and the callee.
const Call kCalls[] = {
    // kLoadBankFile
    {0x21, 0x5B9380}, {0x2D, 0x5A7380}, {0x3F, 0x5A74D0}, {0x47, 0x5B9660}, {0x51, 0x5A7470}, {0x57, 0x5A7510},
    {0x79, 0x587CD0}, {0x8A, 0x5B9577}, {0, 0},
    // kReadFile
    {0xA, 0x5A7380}, {0x23, 0x5A74F0}, {0x33, 0x5A7470}, {0x39, 0x5A7510}, {0, 0},
    // kListFiles
    {0x28, kFindFirst}, {0x86, kFindNext}, {0, 0},
    // kClutRow
    {0, 0},
    // kRestore
    {0x27, 0x590F60}, {0x3C, kRecalcStats}, {0x5D, 0x590F60}, {0x7D, 0x57C140}, {0x93, 0x57C140}, {0, 0},
    // kChoices
    {0x20, kWindowFrame}, {0x2B, kWindowBox}, {0x43, 0x497740}, {0x59, 0x516B30}, {0x7B, kDrawHand}, {0, 0},
    // kBlack
    {0x9, 0x5A7740}, {0x12, 0x5A7780}, {0x38, 0x461E50}, {0, 0},
    // kPriceRate
    {0x12, 0x531BB0}, {0x66, 0x531BB0}, {0x7F, kShopFlag}, {0, 0},
    // kScale
    {0, 0},
    // kSell
    {0xB, kItemPrice}, {0, 0},
    // kInitWin
    {0, 0},
    // kEquip
    {0x3D, kInventoryRemove}, {0x4C, kInventoryAdd}, {0x65, kInventoryRemove}, {0x74, kInventoryAdd},
    {0x8D, kInventoryRemove}, {0x9C, kInventoryAdd}, {0xB5, kInventoryRemove}, {0xC4, kInventoryAdd},
    {0xDD, kInventoryRemove}, {0xEC, kInventoryAdd}, {0x105, kInventoryRemove}, {0x114, kInventoryAdd}, {0, 0},
    // kDrawRows
    {0x13, 0x5888D0}, {0x1D, 0x5888D0}, {0x2A, 0x5888D0}, {0x36, 0x5888D0}, {0x43, 0x5888D0}, {0, 0},
    // kDrawRow
    {0x36, 0x5A79A0}, {0x4E, 0x5A77C0}, {0x57, 0x461E50}, {0x63, 0x5A7710}, {0xBF, 0x5A79E0}, {0xF5, 0x5A7780},
    {0xFE, 0x461E50}, {0x113, 0x5A79A0}, {0x12C, 0x5A77C0}, {0x135, 0x461E50}, {0x141, 0x5A7710}, {0x19C, 0x5A79E0},
    {0x1CB, 0x5A7780}, {0x1D4, 0x461E50}, {0, 0},
    // kRollGrowth
    {0x41, 0x588BA0}, {0x5B, 0x588BA0}, {0x75, 0x588BA0}, {0x8F, 0x588BA0}, {0xA9, 0x588BA0}, {0, 0},
    // kRollSum
    {0x34, 0x5B93D2}, {0, 0},
    // kDrawFrame
    {0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x27, 0x588C90}, {0x3A, 0x588C90}, {0x51, 0x5A77C0}, {0x5D, 0x461E50},
    {0x6A, 0x588C90}, {0x7A, 0x588C90}, {0, 0},
    // kDrawPiece
    {0x8, 0x5A7710}, {0x84, 0x461E50}, {0, 0},
    // kDrawSlots
    {0x4B, kSlotDraw}, {0x91, kSlotCursor}, {0, 0},
    // kSummaries
    {0x7C, 0x454820}, {0, 0},
    // kLoadStream
    {0x2E, 0x5B9380}, {0x3C, 0x5A7380}, {0x59, 0x5B9577}, {0x62, 0x5A74D0}, {0x6A, 0x5B9660}, {0x77, 0x5A7470},
    {0x7D, 0x5A7510}, {0x95, kVoicePlay}, {0xB6, 0x5A7050}, {0xCB, 0x5A6CC0}, {0xD5, 0x5A6FB0}, {0, 0},
    // kStreamDone
    {0x9, kVoiceIsPlaying}, {0x12, 0x5A7020}, {0, 0},
    // kMusicPlaying
    {0, 0},
    // kStep
    {0, 0},
    // kBegin
    {0x28, 0x533BA0}, {0x9C, 0x5891F0}, {0xAB, 0x4548B0}, {0, 0},
    // kMenu
    {0, 0},
    // kOpen, kOpenWait, kFrameUp
    {0x0, 0x588C00}, {0, 0},
    {0x0, 0x588C00}, {0, 0},
    {0x0, 0x588C00}, {0, 0},
    // kRowsStart
    {0xC, 0x588C00}, {0x13, 0x588880}, {0, 0},
    // kFadeIn
    {0x0, 0x588C00}, {0x7, 0x588880}, {0, 0},
    // kChoose
    {0x1, 0x588C00}, {0x8, 0x588880}, {0xD4, 0x495040}, {0xEC, 0x587740}, {0, 0},
    // kLeave
    {0x23, 0x588C00}, {0x2A, 0x588880}, {0, 0},
    // kNewGame
    {0x0, 0x437820}, {0x17, 0x56D5E0}, {0x36, 0x495040}, {0, 0},
    // kLoad
    {0, 0},
    // kList
    {0x2, 0x495040}, {0x16, 0x4548B0}, {0x1F, 0x588DC0}, {0x30, 0x587740}, {0, 0},
    // kLOpen
    {0x6, kWindowBack}, {0x1D, kWindowFrame5}, {0x27, 0x497740}, {0x38, 0x516B30}, {0x43, 0x588D20}, {0, 0},
    // kLChoose
    {0x7, kWindowBack}, {0x1E, kWindowFrame5}, {0x28, 0x497740}, {0x39, 0x516B30}, {0x44, 0x588D20},
    {0x57, 0x461EB0}, {0xC0, 0x587740}, {0xF5, 0x587740}, {0x117, 0x587740}, {0x12F, 0x587740}, {0, 0},
    // kConfirm
    {0x6, kWindowBack}, {0x1D, kWindowFrame5}, {0x27, 0x497740}, {0x38, 0x516B30}, {0x43, 0x588D20},
    {0x4B, kMenuYesNo}, {0, 0},
    // kRead
    {0x7, kWindowBack}, {0x1E, kWindowFrame5}, {0x29, 0x588D20}, {0x4D, 0x5B9380}, {0x5E, 0x454820},
    {0xB3, 0x587740}, {0, 0},
    // kError
    {0x8, kWindowBack}, {0x1F, kWindowFrame5}, {0x65, 0x497740}, {0x76, 0x516B30}, {0x92, 0x587740}, {0, 0},
    // kLoaded
    {0x6, kWindowBack}, {0x1D, kWindowFrame5}, {0x28, 0x588D20}, {0x32, 0x497740}, {0x43, 0x516B30},
    {0x4A, 0x495040}, {0, 0},
    // kApply
    {0xAB, 0x531BB0}, {0xC1, 0x536AC0}, {0xC9, 0x454810}, {0xD4, 0x5A9949}, {0xDC, 0x454810}, {0xF1, 0x536AC0},
    {0xF9, 0x454810}, {0x104, 0x5A9949}, {0x10C, 0x454810}, {0x117, 0x533CE0}, {0x18A, 0x56D670},
    {0x18F, 0x454810}, {0x19A, 0x5A9949}, {0x1A2, 0x454810}, {0x1DD, kWindowBack}, {0x1F3, kWindowFrame5},
    {0x1FE, 0x588D20}, {0x208, 0x497740}, {0x219, 0x516B30}, {0, 0},
    // kEnter
    {0x49, 0x588AC0}, {0x4F, kRecalcStats}, {0x6D, kTaskRestart}, {0, 0},
};

void CloneAll() {
    const Call* c = kCalls;
    for (unsigned k = 0; k < kKinds; ++k) {
        bof3::CloneCall calls[24];
        int n = 0;
        // A list ends at {0, 0}; an entry at offset 0 has a target.
        for (; c->target != 0; ++c) {
            if (n == 24) bof3::Fatal("save_menu: too many calls for %s", kInfo[k].name);
            calls[n++] = {c->offset, StubFor(c->target), c->target};
        }
        ++c;
        g_clones[k] = bof3::CloneOriginal(kInfo[k].name, kInfo[k].base, kInfo[k].size, calls, n);
    }
    if (c != kCalls + sizeof kCalls / sizeof kCalls[0]) bof3::Fatal("save_menu: the call lists do not match the kinds");
    // Shop_Equip's table: five entries at +0x124 and the jmp's operand at +0x2E.
    auto* code = static_cast<unsigned char*>(g_clones[kEquip]);
    const U base = 0x583210, moved = Addr(code) - base;
    for (U i = 0; i < 5; ++i) {
        U target;
        std::memcpy(&target, code + 0x124 + 4 * i, 4);
        if (target < base || target >= base + 0x124) bof3::Fatal("Shop_Equip: jump table entry %u is 0x%X", (unsigned)i, (unsigned)target);
        target += moved;
        std::memcpy(code + 0x124 + 4 * i, &target, 4);
    }
    U disp;
    std::memcpy(&disp, code + 0x2E, 4);
    if (disp != base + 0x124) bof3::Fatal("Shop_Equip: no jump table operand at +0x2E");
    disp += moved;
    std::memcpy(code + 0x2E, &disp, 4);
}

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

// --- State capture ---------------------------------------------------------------
constexpr unsigned kMaxBytes = 0x18000;
struct State {
    unsigned char bytes[kMaxBytes];
    unsigned n;
    Entry log[kLog];
    unsigned log_n;
    U result;
};
void Capture(State& s, std::uint64_t mask) {
    unsigned at = 0;
    for (unsigned r = 0; r < kRegions; ++r) {
        if (!(mask & Bit(r))) continue;
        if (at + g_regions[r].bytes > kMaxBytes) bof3::Fatal("save_menu: the self-test's regions exceed its state");
        std::memcpy(s.bytes + at, At(g_regions[r].at), g_regions[r].bytes);
        at += g_regions[r].bytes;
    }
    s.n = at;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
    s.result = 0;
}
void Apply(const State& s, std::uint64_t mask) {
    unsigned at = 0;
    for (unsigned r = 0; r < kRegions; ++r) {
        if (!(mask & Bit(r))) continue;
        std::memcpy(At(g_regions[r].at), s.bytes + at, g_regions[r].bytes);
        at += g_regions[r].bytes;
    }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
U FirstDifference(const State& a, const State& b, std::uint64_t mask) {
    unsigned at = 0;
    for (unsigned r = 0; r < kRegions; ++r) {
        if (!(mask & Bit(r))) continue;
        for (U k = 0; k < g_regions[r].bytes; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) return g_regions[r].at + k;
        at += g_regions[r].bytes;
    }
    return 0;
}
bool Same(const State& a, const State& b) {
    if (a.n != b.n || a.log_n != b.log_n || a.result != b.result) return false;
    if (std::memcmp(a.bytes, b.bytes, a.n) != 0) return false;
    const unsigned n = a.log_n < kLog ? a.log_n : kLog;
    return std::memcmp(a.log, b.log, n * sizeof(Entry)) == 0;
}

// --- A round's input -----------------------------------------------------------
struct Args { U a[4]; U ecx; };

void Fill(std::uint64_t mask) {
    for (unsigned r = 0; r < kRegions; ++r) {
        if (!(mask & Bit(r))) continue;
        for (U i = 0; i < g_regions[r].bytes; ++i) At(g_regions[r].at)[i] = static_cast<unsigned char>(Next());
    }
}
U Wide(U low, U bits_mask) { return (Next() & ~bits_mask) | (low & bits_mask); }   // noise above the bits read

// The pointers the originals dereference without a test, and the words that
// index tables, made valid.
void Sane() {
    PutLong(at::kPacketNext, Addr(g_prim) + (Next() % 0x30) * 4);
    PutWord(at::kWaitWord, Next() % 2 ? 0 : Next());
    PutLong(at::kSlot, Next() % 4 ? Next() % 17 : Next() % 0x100);
    // The first slot shown: its upper bytes reach 0x576960's slot argument.
    PutLong(at::kSlotTop, Next() % 4 ? Next() % 17 : Next() % 2 ? Next() % 0x1000 : 0u - Next() % 16);
}

Args Generate(unsigned k) {
    Fill(kInfo[k].regions);
    Sane();
    Args x{};
    for (U& v : x.a) v = Next();
    x.ecx = Next();
    g_load_done_after = Next() % 4 == 0 ? Next() % 4 : 0;
    g_find_left = Next() % 20;
    g_file_size = Next() % 5 == 0 ? static_cast<U>(-static_cast<int>(Next() % 8))
                  : Next() % 4 == 0 ? Next() % 0x30 : Next() % 0x1800;
    g_party = Next() % 5;
    g_equip_target = 0;
    // The step and the state index tables that hold recorders; keep them inside.
    PutWord(at::kGameStep, Next() % 6);
    PutByte(at::kFlowState, Next() % 15);
    PutByte(at::kCursor, Pick({0, 1, 2, 3, 0xFF, 0x7F, 0x80, Next()}));
    PutByte(at::kAnySave, Next() % 3 ? Next() % 2 : Next());
    switch (k) {
    case kLoadBankFile: {
        x.a[0] = Next() % 16;
        if (Next() % 6 == 0) PutLong(at::kDatNames + x.a[0] * 4, 0);
        // A chunk list: kinds around 2, every header the walk can reach
        // well-formed (the file is at most 0x1800 bytes, a chunk 0x20F).
        U off = 0;
        while (static_cast<int>(off) < static_cast<int>(g_file_size)) {
            const U size = Next() % 4 == 0 ? 0 : Next() % 0x200;
            g_file[off] = static_cast<unsigned char>(Pick({0, 1, 2, 2, 2, 3, 0x82, Next()}));
            PutLong(Addr(g_file) + off + 4, Next());
            PutLong(Addr(g_file) + off + 8, size);
            off += size + 0x10;
        }
        break;
    }
    case kReadFile:
        x.a[0] = at::kPath;
        break;
    case kListFiles:
        break;
    case kClutRow:
        x.a[0] = Wide(Next() % 64, 0xFF);
        break;
    case kRestore:
        x.a[0] = Pick({0, 1, 0x100, 0xFF, Next()});
        PutByte(at::kInnByte, Pick({0, 0x1D, 0x1E, 0x1F, Next()}));
        break;
    case kChoices:
        x.a[0] = Next() % 3 == 0 ? Next() : Next() % 0x140;
        x.a[1] = Next() % 3 == 0 ? Next() : Next() % 0xF0;
        break;
    case kPriceRate: {
        x.a[0] = Addr(g_rate) + 2;
        // Field_Members' record bytes and the two accessory slots, 0x1B sometimes.
        for (U i = 0; i < 5; ++i) {
            const U r = Next() % 4 ? Next() % 8 : Next() & 0xFF;
            PutByte(at::kMembers + i * at::kMemberStride, r);
            if (r < 16) {
                PutByte(at::kCharRecords + r * at::kCharStride + 0x16, Pick({0x1B, 0x1A, 0x1C, Next()}));
                PutByte(at::kCharRecords + r * at::kCharStride + 0x17, Pick({0x1B, 0x1A, 0x1C, Next()}));
            }
        }
        PutByte(at::kShopWindowByte, Pick({5, 6, 7, 8, Next()}));
        break;
    }
    case kScale:
        x.a[0] = Pick({0, 1, 99, 100, 101, 0xFFFFFFFF, Next() % 10000, Next()});
        x.a[1] = Wide(Pick({0, 1, 50, 70, 80, 100, 0xFFFF, Next()}), 0xFFFF);
        break;
    case kSell:
        x.a[0] = Next() % 2 ? Wide(0, 0xFF) : Next() % 3 == 0 ? 0 : Next();
        x.a[1] = Wide(Pick({0x2A, 0x2B, 0x30, 0x36, 0x37, Next()}), Next() % 2 ? 0xFFFFFFFF : 0xFF);
        x.a[2] = Pick({0, 1, 0x100, Next()});
        break;
    case kEquip: {
        x.a[0] = Wide(Next() % 16, 0xFF);
        x.a[1] = Wide(Next() % 8, 0xFF);
        const U slot = x.a[1] & 0xFF;
        g_equip_target = at::kCharRecords + (x.a[0] & 0xFF) * at::kCharStride + (slot >= 1 && slot <= 5 ? 0x12 + slot : 0x12);
        break;
    }
    case kDrawRows:
        x.a[0] = Pick({0, 1, 0x100, Next()});
        break;
    case kDrawRow:
        x.a[0] = Pick({0x50, 0x70, 0x90, 0x7FFF, 0x8000, 0xFFFF, Next()});
        x.a[1] = Wide(Next() % 3, 0xFF);
        x.a[2] = Pick({0, 1, 0x100, 0xFF, Next()});
        break;
    case kRollGrowth:
        x.a[0] = Wide(Next() % 4 ? Next() % 8 : Next() % 48, 0xFF);
        break;
    case kRollSum: {
        U lo = Next() & 0xFF, hi = Next() % 3 ? (lo + Next() % 12) & 0xFF : Next() & 0xFF;
        if (((hi + 1) & 0xFF) == lo || static_cast<int>(hi) - static_cast<int>(lo) + 1 == 0) hi = lo;
        x.a[0] = Wide(lo, 0xFF);
        x.a[1] = Wide(hi, 0xFF);
        x.a[2] = Wide(Pick({0, 1, 2, 4, 0xFF, Next()}), 0xFF);
        break;
    }
    case kDrawPiece:
        x.a[0] = Pick({0x1A, 0xFFFFFFFA, 0x7FFF, 0x8000, Next()});
        x.a[1] = Pick({0x18, 0x1C, 0x8000, Next()});
        x.a[2] = Next() % 2 ? Next() % 4 : Next();
        break;
    case kDrawSlots:
    case kSummaries:
    case kLChoose: {
        // Summaries present or not, directory names with hex digits and their neighbours at byte 7.
        for (U i = 0; i < 20; ++i) PutByte(at::kSummaries + i * at::kSummaryBytes + 0x15, Next() % 2 ? 0xFF : Next());
        for (U i = 0; i < 16; ++i)
            PutByte(at::kDirectory + i * at::kDirectoryBytes + 7,
                    Pick({'0', '9', 'A', 'F', 'a', 'f', '/', ':', '@', 'G', '`', 'g', Next(), '0' + Next() % 10,
                          'A' + Next() % 6}));
        x.a[2] = Pick({0, 1, Next()});
        if (k == kLChoose) {
            PutLong(at::kSlot, Pick({0, 1, 2, 14, 15, 16, Next() % 17, 0x100, 0xFFFFFFFF}));
            PutLong(at::kSlotTop, Pick({0, 1, 13, 14, Next() % 17, Long(at::kSlot), Long(at::kSlot) - 2}));
            const U pad = Next() & 0xFFFF;
            PutWord(at::kPressed, pad);
            PutWord(at::kConfirm, Next() % 3 ? (pad & 0x20) | 0x20 : Next());
            PutWord(at::kCancel, Next() % 3 ? (pad & 0x40) | 0x40 : Next());
        }
        break;
    }
    case kLoadStream: {
        // Kinds up to 19: ids above 0xFFFF, as 0x446E8B can push.
        const U kind = Next() % 3 ? Next() % 4 : Next() % 20;
        x.a[0] = (kind << 12) | (Next() % 0x100);
        for (U i = 0; i < 20; ++i) PutLong(at::kStreamTables + i * 4, Addr(g_names));
        PutLong(at::kStreamData, Next() % 3 == 0 ? 0 : Next());
        PutLong(at::kFadeCount, Next() % 2 ? 0 : Next());
        break;
    }
    case kStreamDone:
        PutLong(at::kStreamKind, Next() % 2 ? 0 : Next());
        break;
    case kMusicPlaying:
        PutLong(at::kMusicBuffer, Next() % 4 == 0 ? 0 : Addr(&g_buffer));
        x.ecx = Pick({0, 1, 0xFE, 0xFF, Next()});
        break;
    case kBegin:
        PutLong(at::kSpriteCurrent, at::kObjTrio);
        PutLong(at::kFieldState, at::kObjTrio);
        break;
    case kOpenWait:
        PutByte(at::kCounter, Pick({0, 1, 2, Next()}));
        break;
    case kFadeIn:
        PutByte(at::kCounter, Pick({0xF0, 0xE0, 0, Next()}));
        for (U i = 0; i < 3; ++i) PutByte(at::kRowBright + i, Pick({0x37, 0x38, 0x39, 0x40, 0xF8, 0xFF, Next()}));
        PutByte(at::kCursor, Pick({0, 1, 2, 3, 0xFF, Next()}));
        break;
    case kChoose: {
        for (U i = 0; i < 3; ++i)
            PutByte(at::kRowBright + i, Pick({0x40, 0x47, 0x48, 0x49, 0x78, 0x79, 0x80, 0x81, 0xF8, 0x05, Next()}));
        PutByte(at::kMenuFlags, Next() % 4 ? Next() | 2 : Next());
        U pad = Next() % 3 ? Pick({0x1000, 0x4000, 0x5000, 0x800, 0x20, 0x1020, 0}) : Next();
        pad |= Next() & 0xFFFF0000u;
        PutLong(at::kPressed, pad);
        PutWord(at::kConfirm, Next() % 2 ? 0x20 : Next());
        break;
    }
    case kLeave:
        PutByte(at::kCursor, Pick({0, 1, 2, 0xFF, 0x80, Next()}));
        break;
    case kList:
    case kLOpen:
    case kConfirm:
    case kLoaded:
        break;
    case kRead:
        PutLong(at::kSlot, Next());
        break;
    case kLoad:
        PutByte(at::kFlowState, Next() % 8);   // 0x667228 + 4 state stays inside the recorders
        break;
    case kError: {
        PutLong(at::kErrorKind, Pick({0, 1, 2, 3, Next()}));
        const U pad = Next() & 0xFFFF;
        PutWord(at::kPressed, pad);
        PutWord(at::kConfirm, Next() % 2 ? pad | 1 : Next());
        break;
    }
    case kApply:
        PutByte(at::kApplyDelay, Pick({0, 0, 1, 0x1E, Next()}));
        PutByte(at::kDefaultPad, Next() % 2 ? 0 : Next());
        PutLong(at::kHeld, Next());
        break;
    case kEnter:
        PutByte(at::kCursor, Pick({0, 1, 2, 3, Next()}));
        break;
    default:
        break;
    }
    return x;
}

U Run(unsigned k, const void* fn, const Args& x) {
    if (k == kMusicPlaying || k == kError) return SaveMenuFuzz_CallWithEcx(fn, x.ecx);
    using Fn4 = U (__cdecl*)(U, U, U, U);
    return reinterpret_cast<Fn4>(const_cast<void*>(fn))(x.a[0], x.a[1], x.a[2], x.a[3]);
}
U Result(unsigned k, U r) {
    switch (k) {
    case kReadFile: case kListFiles: case kScale: case kSell: case kStreamDone: case kMusicPlaying: return r;
    case kRollSum: return r & 0xFF;
    default: return 0;
    }
}

}  // namespace

void SelfTest() {
    DefineRegions();
    FillVtable(std::make_integer_sequence<int, 16>{});
    g_vtable[0x24 / 4] = reinterpret_cast<const void*>(&FakeGetStatus);
    g_buffer.vtable = g_vtable;
    CloneAll();

    // Everything any kind touches, saved once and put back at the end; the
    // dispatch tables filled with recorders meanwhile.
    std::uint64_t all = 0;
    for (unsigned k = 0; k < kKinds; ++k) all |= kInfo[k].regions;
    static State saved, input, theirs, ours;
    Capture(saved, all);
    U tables[kTableEntries];
    std::memcpy(tables, At(at::kStepTable), sizeof tables);
    for (unsigned i = 0; i < kTableEntries; ++i) PutLong(at::kStepTable + i * 4, Addr(reinterpret_cast<const void*>(TargetTable()[i])));

    const unsigned short cw_saved = ControlWord();
    const Callees g_saved = g;
    constexpr unsigned kPerKind = 1500;
    const unsigned short words[] = {0x027F, 0x007F, 0x037F};
    unsigned bad = 0, bad_per[kKinds] = {}, calls[0x100] = {};
    for (unsigned round = 0; round < kPerKind * kKinds; ++round) {
        const unsigned k = round % kKinds;
        g_kind = k;
        g_seed = round * 0x9E3779B9u + 0x7F4A7C15u;
        const std::uint64_t mask = kInfo[k].regions;
        // The per-round stand-in state is part of the input: saved with it.
        const Args x = Generate(k);
        const U load_done = g_load_done_after, find_left = g_find_left;
        Capture(input, mask);
        const unsigned short cw = words[(round / kKinds) % 3];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, mask);
            g_load_done_after = load_done;
            g_find_left = find_left;
            g = kStubs;
            SetControlWord(cw);
            State& out = pass ? ours : theirs;
            const U r = Run(k, pass ? kInfo[k].ours : g_clones[k], x);
            SetControlWord(cw_saved);
            Capture(out, mask);
            out.result = Result(k, r);
        }
        const unsigned n = theirs.log_n < kLog ? theirs.log_n : kLog;
        for (unsigned i = 0; i < n; ++i) ++calls[theirs.log[i].what & 0xFF];
        if (!Same(theirs, ours)) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < n && std::memcmp(&theirs.log[first], &ours.log[first], sizeof(Entry)) == 0) ++first;
                const Entry& t = theirs.log[first < kLog ? first : 0];
                const Entry& o = ours.log[first < kLog ? first : 0];
                bof3::Log("shadow      save_menu MISMATCH: round %u, %s, cw %04X, result %08X / %08X, log %u / %u, "
                          "memory at 0x%X, call %u: %X(%X %X %X %X %X) / %X(%X %X %X %X %X)",
                          round, kInfo[k].name, cw, theirs.result, ours.result, theirs.log_n, ours.log_n,
                          (unsigned)FirstDifference(theirs, ours, mask), first, t.what, t.a, t.b, t.c, t.d, t.e,
                          o.what, o.a, o.b, o.c, o.d, o.e);
            }
        }
    }
    g = g_saved;
    std::memcpy(At(at::kStepTable), tables, sizeof tables);
    Apply(saved, all);
    SetControlWord(cw_saved);
    bof3::Log("shadow      save_menu self-test: %u rounds (%u per kind, %u kinds), %u MISMATCHES; calls: file open %u, "
              "read %u, find next %u, rand %u, bank %u, effect %u, clear status %u, recalc %u, price %u, add %u, "
              "text %u, commit %u, load done %u, sleep %u, restart %u, read file %u, targets %u",
              kPerKind * kKinds, kPerKind, (unsigned)kKinds, bad, calls[0x01], calls[0x04], calls[0x0A], calls[0x0B],
              calls[0x0C], calls[0x0D], calls[0x14], calls[0x15], calls[0x18], calls[0x1B], calls[0x24], calls[0x2D],
              calls[0x36], calls[0x37], calls[0x38], calls[0x3A],
              calls[0x80] + calls[0x81] + calls[0x82] + calls[0x83] + calls[0x86] + calls[0x8D]);
    for (unsigned k = 0; k < kKinds; ++k)
        if (bad_per[k]) bof3::Log("shadow      save_menu self-test: %s %u of %u rounds differ", kInfo[k].name, bad_per[k], kPerKind);
    if (bad) bof3::Fatal("save, load and the stream differ from the original in %u of %u self-test rounds", bad,
                         kPerKind * kKinds);
}

}  // namespace save_menu
