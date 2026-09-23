// BOF3X_SHADOW=event_ops: the start-up differential fuzz of event_ops.cpp's
// 35 functions (docs/event-ops.md, "The fuzz").
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - the calls between this file's own functions included, so each
// is tested alone - and ours runs with the same stand-ins through
// event_ops::g. The three jump tables are moved into their copies; the two
// chapter hooks' table operand (0x662C80) is moved onto a table of the
// fuzz's own, whose records' +8 / +0xC are numbered stand-ins; Area_StepHook
// and Area_ArriveHook's 46 per-area calls are each re-aimed at a stand-in
// numbered by its case, so that a wrong case shows in the log. A round:
// random state with each branch's boundaries seeded, theirs, the same state
// again, ours; every byte of the state, the stand-ins' log (a count, a hash of
// every entry, the first 48 kept) and the result compared. The stand-ins give
// back what the real callee leaves for the caller to read - a found cell's
// words, a step target, an exit, a moved Sprite_Current - and now and then
// move Sprite_Current or Field_State, which the callers read again.
//
// Nothing here reaches the CRT of BOF3.exe: Rand is a stand-in like the rest.
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/event_ops_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_ops {
namespace {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }
template <class T, std::size_t N>
T Pick(const T (&a)[N]) { return a[Next() % N]; }

// --- the stand-ins' log ----------------------------------------------------------

constexpr unsigned kKeep = 48, kIds = 512;
struct Log {
    std::uint32_t n, hash, path;   // path: the callees' numbers alone, for the coverage line
    std::uint32_t keep[kKeep][6];
    unsigned counts[kIds];
};
Log g_log;
std::uint32_t g_seed;

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
        g_log.keep[g_log.n][4] = d;
        g_log.keep[g_log.n][5] = e;
    }
    for (const std::uint32_t v : {what, a, b, c, d, e}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    g_log.path = (g_log.path ^ what) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_log.counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash(unsigned salt = 0) {
    std::uint32_t h = (g_seed + g_log.n * 0x10001u + salt * 0x3C6EF372u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// --- the objects the fuzz owns --------------------------------------------------

constexpr int kFirstObject = -4, kObjects = 38;   // Sprite_Objects -4..33 (EventOp_5x / 9x may leave counts there)
constexpr int kExtras = 8;                         // Sprite_ObjectsExtra 0..7 (Field_LeaderTalkTo reaches past 4)
unsigned char* Object(int n) { return Sprite_Objects + n * 0xA4; }
unsigned char* Member(int n) { return At(at::kLeader) + n * at::kMemberStride; }

alignas(4) unsigned char g_script[0x40];     // an op's bytes
alignas(4) unsigned char g_zone[8];          // Area_ZoneAt's record
alignas(4) unsigned char g_passage[16];      // what Area_PassageAhead's stand-in returns
alignas(4) unsigned char g_descriptor[0x40]; // the area descriptor Area_PassageAhead reads
alignas(4) unsigned char g_list[10 * 8];     // its +0x2C list
alignas(4) std::uint32_t g_result;

// Where Sprite_Current may point: the leader, the other members, or objects.
unsigned char* PickSprite(bool objects) {
    if (!objects || OneIn(2)) return OneIn(4) ? Member(1 + static_cast<int>(Next() % 2)) : Member(0);
    return Object(static_cast<int>(Next() % 30));
}
bool g_objects_live;   // whether this test's regions hold Sprite_Objects (a stand-in may move Sprite_Current there)

// What a callee might move that the callers read again: Sprite_Current, and
// (less often) Field_State.
void Disturb(unsigned salt) {
    const std::uint32_t h = Hash(salt);
    if (h % 9 == 0) Sprite_Current = (h >> 8) % 3 == 0 && g_objects_live ? Object(static_cast<int>((h >> 12) % 30)) : Member(static_cast<int>((h >> 12) % 3));
    if (h % 23 == 0) Field_State = Member(static_cast<int>((h >> 16) % 3));
}
// 0 one time in `zero_in`, else a non-zero byte (never 0 with zero_in 0).
unsigned char Answer(unsigned salt, unsigned zero_in = 2) {
    const std::uint32_t h = Hash(salt);
    return zero_in != 0 && h % zero_in == 0 ? 0 : static_cast<unsigned char>(1 + (h >> 8) % 0xFF);
}

// --- the stand-ins ----------------------------------------------------------------

// This file's own functions, through g.
template <unsigned N>
unsigned char __cdecl StubTest() {
    Record(N);
    Disturb(N);
    const std::uint32_t h = Hash(N);
    return h % 12 == 0 ? static_cast<unsigned char>(1 + (h >> 8) % 0xFF) : 0;
}
unsigned char __cdecl StubDirection() {
    Record(0x10);
    const std::uint32_t h = Hash(0x10);
    if (h % 3 == 0) Sprite_Current[8] = static_cast<unsigned char>((h >> 4) % 8);
    Disturb(0x11);
    return h % 5 == 0 ? 0 : static_cast<unsigned char>(1 + (h >> 8) % 3);
}
unsigned char __cdecl StubStepTarget() {
    Record(0x12);
    Disturb(0x12);
    static const unsigned char kCodes[] = {0, 0, 1, 2, 3, 4, 5, 6, 7, 0xFF, 0xFF, 8, 0xFE, 0x80};
    const std::uint32_t h = Hash(0x12);
    return h % 16 == 0 ? static_cast<unsigned char>(h >> 8) : kCodes[(h >> 4) % (sizeof kCodes)];
}
unsigned char __cdecl StubPushObjects() {
    Record(0x13);
    Disturb(0x13);
    return Answer(0x13, 2);
}
unsigned char __cdecl StubPushCount() {
    Record(0x14);
    Disturb(0x14);
    return Answer(0x14);
}
void __cdecl StubSetPace() {
    Record(0x15);
    static const unsigned char kPace[] = {2, 3, 4};
    Field_State[0x128] = kPace[Hash(0x15) % 3];
    Disturb(0x16);
}
unsigned char __cdecl StubEncounterDue() {
    Record(0x17);
    Disturb(0x17);
    return Answer(0x17, 2);
}
unsigned char __cdecl StubStepTick() {
    Record(0x18);
    Disturb(0x18);
    return Answer(0x18);
}
unsigned char __cdecl StubEffectTest(unsigned pace) {
    Record(0x19, pace & 0xFF);
    Disturb(0x19);
    return Answer(0x19, 2);
}
void __cdecl StubStepCell() {
    Record(0x1A);
    const std::uint32_t h = Hash(0x1A);
    SetWord(At(at::kCellX), h);
    SetWord(At(at::kCellZ), h >> 16);
}
unsigned char __cdecl StubCanSwap() {
    Record(0x1B);
    return Answer(0x1B, 2);
}
unsigned char __cdecl StubEffectAhead() {
    Record(0x1C);
    const std::uint32_t h = Hash(0x1C);
    return h % 2 ? 0xFF : static_cast<unsigned char>((h >> 4) % 20);
}
const unsigned char* __cdecl StubPassageAhead() {
    Record(0x1D);
    const std::uint32_t h = Hash(0x1D);
    if (h % 3 == 0) return nullptr;
    g_passage[2] = static_cast<unsigned char>(h % 5 == 0 ? 8 : (h >> 8));
    return g_passage;
}
unsigned char __cdecl StubFacingObject() {
    Record(0x1E);
    const std::uint32_t h = Hash(0x1E);
    if (h % 4 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 8);
    Disturb(0x1F);
    return h % 3 == 0 ? static_cast<unsigned char>((h >> 16) % 34) : 0xFF;
}
unsigned char __cdecl StubObjectAhead(long x, long z, unsigned) {
    Record(0x20, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash(0x20);
    return h % 2 ? 0xFF : static_cast<unsigned char>((h >> 8) % 34);
}
// Field_CellAround's pair and 0x531120: they leave the found cell in the
// scratch words.
unsigned char CellBody(unsigned id, unsigned cell) {
    Record(id, cell & 0xFF);
    const std::uint32_t h = Hash(id);
    if (h % 3 == 0) {
        SetWord(At(at::kScratch), h >> 4);
        SetWord(At(at::kScratch2), h >> 16);
    }
    Disturb(id + 1);
    return h % 3 == 1 ? static_cast<unsigned char>(1 + (h >> 8) % 0xFF) : 0;
}
unsigned char __cdecl StubCellAround(unsigned cell) { return CellBody(0x21, cell); }
unsigned char __cdecl StubCellAroundNear(unsigned cell) { return CellBody(0x23, cell); }
unsigned char __cdecl StubCellAroundLarge(unsigned cell) { return CellBody(0x25, cell); }
void __cdecl StubTalkTo(unsigned index) {
    Record(0x27, index & 0xFF);
    Disturb(0x27);
}
// The hooks: the step target (0x903858 / 0x90385C) may be moved, which
// Field_LeaderStepTarget reads back.
int HookBody(unsigned id, long x, long z) {
    Record(id, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash(id);
    if (h % 5 == 0) {
        move_script::SetLong(At(at::kTargetX), static_cast<std::int32_t>(Hash(id + 1)));
        move_script::SetLong(At(at::kTargetZ), static_cast<std::int32_t>(Hash(id + 2)));
    }
    Disturb(id + 3);
    const std::uint32_t r = Hash(id + 4);
    return r % 2 ? 0 : static_cast<int>(r);
}
int __cdecl StubStepHook(long x, long z) { return HookBody(0x28, x, z); }
int __cdecl StubArriveHook(long x, long z) { return HookBody(0x2D, x, z); }
int __cdecl StubAreaStep(long x, long z) { return HookBody(0x32, x, z); }
int __cdecl StubAreaArrive(long x, long z) { return HookBody(0x37, x, z); }
unsigned char __cdecl StubReturnGate(long x, long z) {
    Record(0x3C, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    return Hash(0x3C) % 3 == 0 ? Answer(0x3D, 0) : 0;
}
// The chapter records: four of them, numbered, and a 256-entry table (-128 at
// the start) whose entry i is record i & 3.
template <unsigned N>
unsigned char __cdecl StubChapterStep(long x, long z) {
    Record(0x40 + N, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb(0x40 + N);
    const std::uint32_t h = Hash(0x40 + N);
    return h % 2 ? 0 : static_cast<unsigned char>(h >> 8);
}
template <unsigned N>
unsigned char __cdecl StubChapterArrive(long x, long z) {
    Record(0x48 + N, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb(0x48 + N);
    const std::uint32_t h = Hash(0x48 + N);
    return h % 2 ? 0 : static_cast<unsigned char>(h >> 8);
}
ChapterHooks g_records[4] = {
    {{nullptr, nullptr}, &StubChapterStep<0>, &StubChapterArrive<0>},
    {{nullptr, nullptr}, &StubChapterStep<1>, &StubChapterArrive<1>},
    {{nullptr, nullptr}, &StubChapterStep<2>, &StubChapterArrive<2>},
    {{nullptr, nullptr}, &StubChapterStep<3>, &StubChapterArrive<3>},
};
const ChapterHooks* g_chapters[256];

// Other files' functions.
unsigned char __cdecl StubLeaderAnimation(unsigned animation) {
    Record(0x60, animation & 0xFF);
    Disturb(0x60);
    return static_cast<unsigned char>(Hash(0x61));
}
void __cdecl StubClearSteps() {
    Record(0x62);
    Disturb(0x62);
}
unsigned char __cdecl StubEnsureAnimation(unsigned char animation) {
    Record(0x63, animation);
    Disturb(0x63);
    return static_cast<unsigned char>(Hash(0x64));
}
// AreaMap_ByteAt: the cells the callers test for, and now and then the one
// the test under way looks for (g_cell).
unsigned char g_cell;
unsigned char __cdecl StubByteAt(short x, short z) {
    Record(0x65, static_cast<unsigned short>(x), static_cast<unsigned short>(z));
    static const unsigned char kCells[] = {0, 0, 0x30, 0xA0, 0xA1, 0xAE, 0xA5, 0x50, 0x51, 0x52, 0x53, 0x55, 0xFF, 1};
    const std::uint32_t h = Hash(0x65);
    if (h % 4 == 0) return g_cell;
    return h % 7 == 1 ? static_cast<unsigned char>(h >> 8) : kCells[(h >> 12) % sizeof kCells];
}
void __cdecl StubZoneRoll(unsigned keep) { Record(0x66, keep); }
int __cdecl StubPartyCount(unsigned slot) {
    Record(0x67, slot);
    return static_cast<int>(Hash(0x67) % 5);
}
const unsigned char* __cdecl StubZoneAt(unsigned x, unsigned z) {
    Record(0x68, x & 0xFFFF, z & 0xFFFF);
    g_zone[4] = static_cast<unsigned char>(Hash(0x68) % 3 == 0 ? 0 : Hash(0x69));
    return g_zone;
}
int __cdecl StubRand() {
    Record(0x6A);
    return static_cast<int>(Hash(0x6A) & 0x7FFF);
}
void __cdecl StubPlayEffect(unsigned short id) {
    Record(0x6B, id);
    Disturb(0x6B);
}
unsigned char __cdecl StubLinkAt(unsigned x, unsigned z) {
    Record(0x6C, x & 0xFFFF, z & 0xFFFF);
    return Answer(0x6C, 2);
}
long __cdecl StubGroundAt(long x, long z) {
    Record(0x6D, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash(0x6D);
    // near the sprite's own height half the time: both sides of 0x80 / 0x100
    if (h % 2) return static_cast<long>(static_cast<short>(Word(Sprite_Current + 0x3E)) + static_cast<int>((h >> 4) % 0x281) - 0x140);
    return static_cast<long>(h);
}
unsigned char __cdecl StubScriptTick() {
    Record(0x6E);
    Disturb(0x6E);
    return static_cast<unsigned char>(Hash(0x6F));
}
unsigned char __cdecl StubObjectAt(long x, long y, unsigned margin) {
    Record(0x70, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), margin);
    static const unsigned char kIndex[] = {0xFF, 0xFF, 0, 29, 30, 33, 0x1D, 0x1E, 0x7F, 0x80, 0xFE, 5, 31};
    const std::uint32_t h = Hash(0x70);
    return h % 3 == 0 ? static_cast<unsigned char>((h >> 8) % 34) : kIndex[(h >> 12) % sizeof kIndex];
}
unsigned char __cdecl StubPointInReach(int x, int y, short z, int margin, const unsigned char* object) {
    Record(0x71, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<unsigned short>(z),
           static_cast<std::uint32_t>(margin), Address(object));
    Disturb(0x71);
    return Hash(0x72) % 3 == 0 ? Answer(0x73, 0) : 0;
}
void __cdecl StubChangeArea(unsigned area, int x, int z, unsigned flags) {
    Record(0x74, area & 0xFFFF, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags & 0xFF);
}
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(0x75, Address(bits), index);
    return Answer(0x75, 3);
}
void __cdecl StubFlagsClear(unsigned char* bits, unsigned index) { Record(0x76, Address(bits), index); }
// The placements' callees (event_script_fuzz.cpp's shape): the count and
// Sprite_Current may be moved, which the ops read again.
void SetCount(int v) { SetWord(At(at::kScratch), static_cast<unsigned>(v)); }
int PickCount(std::uint32_t h) {
    static const int kEdge[] = {29, 30, 31, 0, -1, 28};
    return h % 3 == 0 ? kEdge[(h >> 4) % 6] : static_cast<int>((h >> 8) % 34) - 2;
}
void DisturbPlacement(unsigned salt) {
    const std::uint32_t h = Hash(salt);
    if (h % 7 == 0) SetCount(PickCount(h >> 4));
    if (h % 11 == 0) Sprite_Current = Object(static_cast<int>((h >> 8) % 30));
}
void __cdecl StubReset() {
    Record(0x78, Address(Sprite_Current), Address(Field_ActiveMember));
    DisturbPlacement(0x78);
}
unsigned char __cdecl StubSetBank(unsigned short bank) {
    Record(0x79, bank);
    DisturbPlacement(0x79);
    return 0;
}
long __cdecl StubElevation(long x, long z) {
    Record(0x7A, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    DisturbPlacement(0x7A);
    return static_cast<long>(Hash(0x7B));
}
void __cdecl StubSetFlags(const unsigned char* flags) {
    Record(0x7C, Address(flags), *flags);
    DisturbPlacement(0x7C);
}
void __cdecl StubSetAnimation(unsigned char animation) {
    Record(0x7D, animation);
    DisturbPlacement(0x7D);
}
void __cdecl StubFace() {
    Record(0x7E, Address(Sprite_Current));
    DisturbPlacement(0x7E);
}
// Raw-address callees.
void __cdecl StubEncounterArea() { Record(0x80); }
// The two exits leave the four cells Field_LeaderCellEvent reads after.
void Exit(unsigned salt) {
    const std::uint32_t h = Hash(salt);
    if (h % 4 == 0) return;
    SetWord(At(at::kExitArea), h);
    move_script::SetLong(At(at::kExitX), static_cast<std::int32_t>(Hash(salt + 1)));
    move_script::SetLong(At(at::kExitZ), static_cast<std::int32_t>(Hash(salt + 2)));
    *At(at::kExitKind) = static_cast<unsigned char>(h >> 16);
}
unsigned char __cdecl StubExitGateway() {
    Record(0x81);
    Exit(0x81);
    Disturb(0x85);
    return Answer(0x86, 2);
}
void __cdecl StubExitFromCell() {
    Record(0x87);
    Exit(0x87);
    Disturb(0x8B);
}
int __cdecl StubCellHook(unsigned x, unsigned z) {
    Record(0x8C, x & 0xFFFF, z & 0xFFFF);
    Disturb(0x8C);
    const std::uint32_t h = Hash(0x8D);
    if (h % 4 == 0) return static_cast<int>(h & 0xFFFFFF00u) | 0x100;   // non-zero, al 0
    return h % 2 ? 0 : static_cast<int>(h | 0x100);
}
void __cdecl StubSetCell(unsigned x, unsigned z, unsigned v) { Record(0x8E, x & 0xFFFF, z & 0xFFFF, v & 0xFF); }
// 0x591F30 sets Sprite_Current to 0x6BDFF0 in the real game; here to another
// object, which the caller puts back.
unsigned char __cdecl StubPartyVisible(unsigned a, unsigned b) {
    Record(0x8F, a, b);
    Sprite_Current = Member(static_cast<int>(1 + Hash(0x8F) % 2));
    return Answer(0x90, 4);
}
unsigned char __cdecl StubMemberFits(long x, long z, unsigned slot, unsigned a, unsigned b) {
    Record(0x91, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), slot, a, b);
    const std::uint32_t h = Hash(0x92);
    if (h % 5 == 0) Field_MemberCount = static_cast<unsigned char>((h >> 8) % 5);   // read again by the loop
    return Answer(0x91, 5);
}
void __cdecl StubLeaderMove() {
    Record(0x92);
    Disturb(0x92);
}
void __cdecl StubLeaderFollow() {
    Record(0x93);
    Disturb(0x93);
}
void __cdecl StubLeaderGround() {
    Record(0x94);
    Disturb(0x94);
}
unsigned char __cdecl StubStepCode() {
    Record(0x95);
    Disturb(0x95);
    static const unsigned char kCodes[] = {0, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 0xFF};
    const std::uint32_t h = Hash(0x96);
    return h % 10 == 0 ? static_cast<unsigned char>(h >> 8) : kCodes[(h >> 4) % sizeof kCodes];
}
// 0x5725C0: AreaMap_Slope leaves the byte 0x903850 (flat 0, sloped 1); the
// answer is a height, both sides of 0x40 and of the sprite's own.
long __cdecl StubSlopeAt(long x, long z, std::uint32_t direction) {
    Record(0x97, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), direction);
    const std::uint32_t h = Hash(0x97);
    At(at::kScratch)[0] = static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : (h >> 8));
    Disturb(0x98);
    static const int kHeights[] = {0x40, 0x41, 0x3F, 0, -1, 0x7FFF, -0x8000};
    const std::uint32_t k = Hash(0x99);
    if (k % 3 == 0) {
        static const int kRise[] = {0x80, -0x80, 0x81, -0x81, 0x7F, -0x7F};
        const int rise = (k >> 4) % 2 ? kRise[(k >> 8) % 6] : static_cast<int>((k >> 4) % 0x181) - 0xC0;
        return static_cast<long>(static_cast<short>(Word(Sprite_Current + 0x3E)) - rise);
    }
    return static_cast<long>(k % 3 == 1 ? kHeights[(k >> 8) % 7] : static_cast<int>(k));
}
unsigned char __cdecl StubTestFB(unsigned x, unsigned z) {
    Record(0x9A, x & 0xFFFF, z & 0xFFFF);
    return Answer(0x9A, 2);
}
// The per-area handlers, numbered by case.
template <unsigned N>
unsigned char __cdecl StubAreaStepCase(long x, long z) {
    Record(0xA0 + N, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb(0xA0 + N);
    const std::uint32_t h = Hash(0xA0 + N);
    return h % 2 ? 0 : static_cast<unsigned char>(h >> 8);
}
unsigned char __cdecl StubAreaStepNoArgs() {   // case 6: nothing pushed, nothing read
    Record(0xA0 + kNoArgsCase);
    Disturb(0xA0 + kNoArgsCase);
    const std::uint32_t h = Hash(0xA0 + kNoArgsCase);
    return h % 2 ? 0 : static_cast<unsigned char>(h >> 8);
}
template <unsigned N>
unsigned char __cdecl StubAreaArriveCase(long x, long z) {
    Record(0xD0 + N, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb(0xD0 + N);
    const std::uint32_t h = Hash(0xD0 + N);
    return h % 2 ? 0 : static_cast<unsigned char>(h >> 8);
}
template <std::size_t... I>
constexpr std::array<Hook, sizeof...(I)> MakeStepCases(std::index_sequence<I...>) {
    return {{&StubAreaStepCase<I>...}};
}
template <std::size_t... I>
constexpr std::array<Hook, sizeof...(I)> MakeArriveCases(std::index_sequence<I...>) {
    return {{&StubAreaArriveCase<I>...}};
}
std::array<Hook, kStepCases> g_step_cases = MakeStepCases(std::make_index_sequence<kStepCases>());
const std::array<Hook, kArriveCases> kArriveCaseStubs = MakeArriveCases(std::make_index_sequence<kArriveCases>());

Callees MakeStubs() {
    Callees s = {};
    s.talk_test = StubTest<1>;
    s.swap_test = StubTest<2>;
    s.menu_test = StubTest<3>;
    s.check_test = StubTest<4>;
    s.request4_test = StubTest<5>;
    s.request9_test = StubTest<6>;
    s.direction = StubDirection;
    s.step_target = StubStepTarget;
    s.push_objects = StubPushObjects;
    s.push_count = StubPushCount;
    s.set_pace = StubSetPace;
    s.encounter_due = StubEncounterDue;
    s.step_tick = StubStepTick;
    s.effect_test = StubEffectTest;
    s.step_cell = StubStepCell;
    s.can_swap = StubCanSwap;
    s.effect_ahead = StubEffectAhead;
    s.passage_ahead = StubPassageAhead;
    s.facing_object = StubFacingObject;
    s.object_ahead = StubObjectAhead;
    s.cell_around = StubCellAround;
    s.cell_around_near = StubCellAroundNear;
    s.talk_to = StubTalkTo;
    s.step_hook = StubStepHook;
    s.arrive_hook = StubArriveHook;
    s.area_step = StubAreaStep;
    s.area_arrive = StubAreaArrive;
    s.return_gate = StubReturnGate;
    s.chapter_hooks = g_chapters + 128;
    s.leader_animation = StubLeaderAnimation;
    s.clear_steps = StubClearSteps;
    s.ensure_animation = StubEnsureAnimation;
    s.byte_at = StubByteAt;
    s.zone_roll = StubZoneRoll;
    s.party_count = StubPartyCount;
    s.zone_at = StubZoneAt;
    s.rand = StubRand;
    s.play_effect = StubPlayEffect;
    s.link_at = StubLinkAt;
    s.ground_at = StubGroundAt;
    s.script_tick = StubScriptTick;
    s.object_at = StubObjectAt;
    s.point_in_reach = StubPointInReach;
    s.change_area = StubChangeArea;
    s.flags_test = StubFlagsTest;
    s.flags_clear = StubFlagsClear;
    s.reset = StubReset;
    s.set_bank = StubSetBank;
    s.elevation = StubElevation;
    s.set_flags = StubSetFlags;
    s.set_animation = StubSetAnimation;
    s.face = StubFace;
    s.encounter_area = StubEncounterArea;
    s.exit_gateway = StubExitGateway;
    s.exit_from_cell = StubExitFromCell;
    s.cell_around_large = StubCellAroundLarge;
    s.cell_hook = StubCellHook;
    s.set_cell = StubSetCell;
    s.party_visible = StubPartyVisible;
    s.member_fits = StubMemberFits;
    s.leader_move = StubLeaderMove;
    s.leader_follow = StubLeaderFollow;
    s.leader_ground = StubLeaderGround;
    s.step_code = StubStepCode;
    s.slope_at = StubSlopeAt;
    s.test_fb = StubTestFB;
    for (unsigned c = 0; c < kStepCases; ++c) s.area_step_handlers[c] = g_step_cases[c];
    s.area_step_handlers[kNoArgsCase] = reinterpret_cast<Hook>(reinterpret_cast<void*>(&StubAreaStepNoArgs));
    for (unsigned c = 0; c < kArriveCases; ++c) s.area_arrive_handlers[c] = kArriveCaseStubs[c];
    return s;
}
Callees g_stubs;

// Where each call of a copy goes, by the original callee.
const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x530920: return f(g_stubs.talk_test);
    case 0x5302C0: return f(g_stubs.swap_test);
    case 0x530380: return f(g_stubs.menu_test);
    case 0x530800: return f(g_stubs.check_test);
    case 0x5303E0: return f(g_stubs.request4_test);
    case 0x530430: return f(g_stubs.request9_test);
    case 0x530480: return f(g_stubs.direction);
    case 0x52E160: return f(g_stubs.step_target);
    case 0x531DF0: return f(g_stubs.push_objects);
    case 0x5308D0: return f(g_stubs.push_count);
    case 0x52E060: return f(g_stubs.set_pace);
    case 0x530030: return f(g_stubs.encounter_due);
    case 0x52E140: return f(g_stubs.step_tick);
    case 0x530860: return f(g_stubs.effect_test);
    case 0x52E510: return f(g_stubs.step_cell);
    case 0x530320: return f(g_stubs.can_swap);
    case 0x530530: return f(g_stubs.effect_ahead);
    case 0x530600: return f(g_stubs.passage_ahead);
    case 0x530A50: return f(g_stubs.facing_object);
    case 0x530BF0: return f(g_stubs.object_ahead);
    case 0x530C90: return f(g_stubs.cell_around);
    case 0x530CC0: return f(g_stubs.cell_around_near);
    case 0x531660: return f(g_stubs.talk_to);
    case 0x56D700: return f(g_stubs.step_hook);
    case 0x56D750: return f(g_stubs.arrive_hook);
    case 0x56E050: return f(g_stubs.area_step);
    case 0x56E4E0: return f(g_stubs.area_arrive);
    case 0x56E440: return f(g_stubs.return_gate);
    case 0x5305B0: return f(g_stubs.leader_animation);
    case 0x536650: return f(g_stubs.clear_steps);
    case 0x589330: return f(g_stubs.ensure_animation);
    case 0x536700: return f(g_stubs.byte_at);
    case 0x52FEB0: return f(g_stubs.zone_roll);
    case 0x531BB0: return f(g_stubs.party_count);
    case 0x52FFD0: return f(g_stubs.zone_at);
    case 0x5B93D2: return f(g_stubs.rand);
    case 0x587740: return f(g_stubs.play_effect);
    case 0x5951D0: return f(g_stubs.link_at);
    case 0x572570: return f(g_stubs.ground_at);
    case 0x5893A0: return f(g_stubs.script_tick);
    case 0x531CF0: return f(g_stubs.object_at);
    case 0x531C70: return f(g_stubs.point_in_reach);
    case 0x594E00: return f(g_stubs.change_area);
    case 0x57C140: return f(g_stubs.flags_test);
    case 0x57C110: return f(g_stubs.flags_clear);
    case 0x579E30: return f(g_stubs.reset);
    case 0x589590: return f(g_stubs.set_bank);
    case 0x5720C0: return f(g_stubs.elevation);
    case 0x579DB0: return f(g_stubs.set_flags);
    case 0x5891F0: return f(g_stubs.set_animation);
    case fn::kFaceObject: return f(g_stubs.face);
    case fn::kEncounterArea: return f(g_stubs.encounter_area);
    case fn::kExitGateway: return f(g_stubs.exit_gateway);
    case fn::kExitFromCell: return f(g_stubs.exit_from_cell);
    case fn::kCellAroundLarge: return f(g_stubs.cell_around_large);
    case fn::kCellHook: return f(g_stubs.cell_hook);
    case fn::kSetCell: return f(g_stubs.set_cell);
    case fn::kPartyVisible: return f(g_stubs.party_visible);
    case fn::kMemberFits: return f(g_stubs.member_fits);
    case fn::kLeaderMove: return f(g_stubs.leader_move);
    case fn::kLeaderFollow: return f(g_stubs.leader_follow);
    case fn::kLeaderGround: return f(g_stubs.leader_ground);
    case fn::kStepCode: return f(g_stubs.step_code);
    case fn::kSlopeAt: return f(g_stubs.slope_at);
    case fn::kTestFB: return f(g_stubs.test_fb);
    default: bof3::Fatal("event_ops: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// --- the copies ------------------------------------------------------------------------

// Every function's range - its last instruction, and the jump table after it
// where it has one - and every call out, by capstone 2026-09-23; no jump
// leaves any of them. `table` / `entries` / `disp`: a jump table's offset, its
// entries and the offset of its `jmp [reg*4 + table]` operand. `chapter`: the
// offset of the operand 0x662C80. `step` / `arrive`: the per-area calls, by
// case, re-aimed at the case's stand-in.
struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[31];   // to the first empty entry
    std::uint32_t table, entries, disp, chapter;
};
enum Fn {
    kOp5, kOpD, kOp9, kWalk, kSetPace, kStepTick, kStepTarget, kStepCell, kEncounter, kIdle, kSwap, kCanSwap, kMenu,
    kReq4, kReq9, kDirection, kEffectAhead, kPassage, kCheck, kEffectTest, kPushCount, kTalk, kFacing, kObjectAhead,
    kCellAround, kCellNear, kTalkTo, kCellEvent, kPushObjects, kNoHook, kStepHook, kArriveHook, kAreaStep,
    kReturnGate, kAreaArrive, kFns
};
const Clone kClones[kFns] = {
    {"EventOp_5x", 0x57B310, 0x1C9, {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x18F, 0x579DB0}, {0x1BA, 0x579D70}}, 0, 0, 0, 0},
    {"EventOp_Dx", 0x57B500, 0x30, {}, 0, 0, 0, 0},
    {"EventOp_9x", 0x57B530, 0x24C, {{0x40, 0x579E30}, {0x4C, 0x589590}, {0xDF, 0x5720C0}, {0x13C, 0x579DB0}, {0x21B, 0x579F00}, {0x225, 0x5891F0}, {0x239, 0x5891F0}}, 0, 0, 0, 0},
    {"Field_LeaderWalk", 0x52DB90, 0x4C4,
     {{0x1F, 0x530920}, {0x2C, 0x5302C0}, {0x39, 0x530380}, {0x46, 0x530800}, {0x53, 0x5303E0}, {0x60, 0x530430},
      {0x84, 0x530480}, {0x96, 0x5305B0}, {0xB7, 0x536650}, {0xF6, 0x52E160}, {0x121, 0x531DF0}, {0x140, 0x5305B0},
      {0x152, 0x5308D0}, {0x165, 0x52E060}, {0x16A, 0x5345E0}, {0x16F, 0x535F50}, {0x1DD, 0x536700}, {0x1FD, 0x52FEB0},
      {0x207, 0x530030}, {0x210, 0x5317F0}, {0x247, 0x5305B0}, {0x257, 0x52E140}, {0x284, 0x530860}, {0x2C0, 0x5305B0},
      {0x2DF, 0x5308D0}, {0x2FF, 0x589330}, {0x379, 0x589330}, {0x3D4, 0x589330}, {0x417, 0x589330}, {0x48D, 0x5305B0}},
     0x4A8, 7, 0x11D, 0},
    {"Field_LeaderSetPace", 0x52E060, 0xA8, {{0x4, 0x531BB0}}, 0, 0, 0, 0},
    {"Field_LeaderStepTick", 0x52E140, 0x19, {{0x0, 0x536670}, {0x13, 0x5893A0}}, 0, 0, 0, 0},
    {"Field_LeaderStepTarget", 0x52E160, 0x3AC,
     {{0xF, 0x526DB0}, {0x113, 0x56D700}, {0x15E, 0x5725C0}, {0x168, 0x52E510}, {0x193, 0x5951D0}, {0x1B5, 0x572650},
      {0x1D1, 0x587740}, {0x1E4, 0x589330}, {0x201, 0x589330}, {0x22F, 0x5951D0}, {0x267, 0x5951D0}, {0x29A, 0x5951D0},
      {0x2C3, 0x5725C0}, {0x305, 0x572570}, {0x316, 0x52E510}, {0x32A, 0x5951D0}, {0x346, 0x52E510}, {0x35B, 0x5951D0}},
     0x38C, 8, 0x154, 0},
    {"Field_LeaderStepCell", 0x52E510, 0x65, {}, 0, 0, 0, 0},
    {"Field_EncounterDue", 0x530030, 0x1B3, {{0x26, 0x52FFD0}, {0x10C, 0x591F30}, {0x14B, 0x535C50}, {0x1A0, 0x536700}}, 0, 0, 0, 0},
    {"Field_LeaderIdleTest", 0x5301F0, 0xC6, {{0x7B, 0x5B93D2}, {0x9B, 0x589330}}, 0, 0, 0, 0},
    {"Field_LeaderSwapTest", 0x5302C0, 0x5E, {{0x2A, 0x530320}}, 0, 0, 0, 0},
    {"Party_CanSwap", 0x530320, 0x5D, {}, 0, 0, 0, 0},
    {"Field_LeaderMenuTest", 0x530380, 0x54, {{0x1D, 0x587740}, {0x33, 0x5305B0}}, 0, 0, 0, 0},
    {"Field_LeaderRequest4Test", 0x5303E0, 0x4B, {{0x2A, 0x5305B0}}, 0, 0, 0, 0},
    {"Field_LeaderRequest9Test", 0x530430, 0x4E, {{0x26, 0x5305B0}}, 0, 0, 0, 0},
    {"Field_LeaderDirection", 0x530480, 0xAE, {}, 0, 0, 0, 0},
    {"Field_EffectAhead", 0x530530, 0x73, {{0x44, 0x531C70}}, 0, 0, 0, 0},
    {"Area_PassageAhead", 0x530600, 0x1B2, {}, 0, 0, 0, 0},
    {"Field_LeaderCheckTest", 0x530800, 0x5C, {}, 0, 0, 0, 0},
    {"Field_LeaderEffectTest", 0x530860, 0x68, {{0x26, 0x530530}}, 0, 0, 0, 0},
    {"Field_LeaderPushCount", 0x5308D0, 0x4D, {}, 0, 0, 0, 0},
    {"Field_LeaderTalkTest", 0x530920, 0x12E,
     {{0x2D, 0x530C90}, {0x35, 0x530A50}, {0x4B, 0x530A50}, {0x5D, 0x531660}, {0x6C, 0x530C90}, {0x88, 0x56D7A0},
      {0x9D, 0x5305B0}, {0xBE, 0x530C90}, {0xCC, 0x530C90}, {0xDA, 0x530C90}, {0xE8, 0x530C90}, {0xF4, 0x530600}},
     0, 0, 0, 0},
    {"Field_FacingObject", 0x530A50, 0x19B, {{0x3F, 0x530BF0}, {0x94, 0x530BF0}, {0xE4, 0x530BF0}, {0x131, 0x536700}, {0x18C, 0x530BF0}}, 0, 0, 0, 0},
    {"Field_ObjectAhead", 0x530BF0, 0x98, {{0x16, 0x531CF0}}, 0, 0, 0, 0},
    {"Field_CellAround", 0x530C90, 0x28, {{0x11, 0x530CC0}, {0x1F, 0x531120}}, 0, 0, 0, 0},
    {"Field_CellAroundNear", 0x530CC0, 0x45E,
     {{0x6C, 0x536700}, {0xBE, 0x536700}, {0x14C, 0x536700}, {0x1B1, 0x536700}, {0x201, 0x536700}, {0x284, 0x536700},
      {0x2E6, 0x536700}, {0x348, 0x536700}, {0x3C7, 0x536700}, {0x42B, 0x536700}},
     0, 0, 0, 0},
    {"Field_LeaderTalkTo", 0x531660, 0x183, {{0x40, 0x5305B0}, {0xA1, 0x589330}, {0xFC, 0x5305B0}, {0x15A, 0x589330}}, 0, 0, 0, 0},
    {"Field_LeaderCellEvent", 0x531950, 0x198,
     {{0xF, 0x536700}, {0x57, 0x5305B0}, {0x5F, 0x531AF0}, {0x89, 0x594E00}, {0xCF, 0x531820}, {0xEB, 0x5305B0},
      {0x149, 0x594E00}, {0x183, 0x5305B0}},
     0, 0, 0, 0},
    {"Field_LeaderPushObjects", 0x531DF0, 0x118, {{0x3F, 0x572570}, {0x88, 0x531C70}, {0xCD, 0x531C70}}, 0, 0, 0, 0},
    {"Scenario_NoHook", 0x539AC0, 0x3, {}, 0, 0, 0, 0},
    {"Scenario_StepHook", 0x56D700, 0x42, {{0x30, 0x56E050}}, 0, 0, 0, 0xF},
    {"Scenario_ArriveHook", 0x56D750, 0x42, {{0x30, 0x56E4E0}}, 0, 0, 0, 0xF},
    {"Area_StepHook", 0x56E050, 0x3E6, {{0xC, 0x56E440}}, 0x2A8, 39, 0x41, 0},
    {"Area_ReturnGate", 0x56E440, 0x96, {{0x35, 0x536700}, {0x50, 0x57C140}, {0x6A, 0x57C110}, {0x86, 0x594E00}}, 0, 0, 0, 0},
    {"Area_ArriveHook", 0x56E4E0, 0x182, {}, 0xD8, 9, 0x21, 0},
};
// The per-area call sites: Area_StepHook's case c at kStepSites[c], Area_ArriveHook's at kArriveSites[c].
std::uint32_t StepSite(unsigned c) { return c < 7 ? 0x47 + 0x10 * c - (c == 6 ? 2 : 0) : 0xB2 + 0x10 * (c - 7); }
std::uint32_t ArriveSite(unsigned c) { return 0x2F + 0x16 * c; }

void* g_theirs[kFns];

void PatchDword(unsigned char* code, std::uint32_t at, std::uint32_t expected, std::uint32_t value, const char* what,
                const Clone& c) {
    std::uint32_t v;
    std::memcpy(&v, code + at, sizeof v);
    if (v != expected) bof3::Fatal("event_ops: %s's %s at +0x%X is 0x%X, not 0x%X", c.name, what, (unsigned)at, (unsigned)v, (unsigned)expected);
    std::memcpy(code + at, &value, sizeof value);
}

void MakeClones() {
    for (unsigned k = 0; k < kFns; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[48];
        int n = 0;
        for (const Call& call : c.calls)
            if (call.target) calls[n++] = {call.offset, StubFor(call.target), call.target};
        if (k == kAreaStep)
            for (unsigned i = 0; i < kStepCases; ++i)
                calls[n++] = {StepSite(i), reinterpret_cast<const void*>(g_stubs.area_step_handlers[i]), kStepHandlers[i]};
        if (k == kAreaArrive)
            for (unsigned i = 0; i < kArriveCases; ++i)
                calls[n++] = {ArriveSite(i), reinterpret_cast<const void*>(g_stubs.area_arrive_handlers[i]), kArriveHandlers[i]};
        void* const copy = bof3::CloneOriginal(c.name, c.base, c.size, calls, n);
        auto* code = static_cast<unsigned char*>(copy);
        if (c.entries) move_script::Relocate(copy, c.base, c.size, {c.disp, c.table, c.entries});
        if (c.chapter) PatchDword(code, c.chapter, at::kChapterHooks, Address(g_stubs.chapter_hooks), "chapter table operand", c);
        FlushInstructionCache(GetCurrentProcess(), copy, c.size);
        g_theirs[k] = copy;
    }
}

// --- the state and one round --------------------------------------------------------

struct Region { unsigned char* at; unsigned size; };
Region R(void* at, unsigned size) { return {static_cast<unsigned char*>(at), size}; }
Region R(std::uint32_t at, unsigned size) { return {At(at), size}; }

enum : unsigned { kObj = 1, kExt = 2, kEff = 4, kAct = 8, kFar = 16 };
constexpr unsigned kStateMax = 0x4000, kRegionsMax = 40;
unsigned char g_saved[kStateMax], g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];
Region g_r[kRegionsMax];
unsigned g_n;

void UseRegions(unsigned mask) {
    g_n = 0;
    const auto add = [](Region r) { g_r[g_n++] = r; };
    add(R(at::kLeader, 3 * at::kMemberStride));               // ObjTrio: the leader, Field_State
    add(R(0x903840, 0x30));                                   // the scratch words
    add(R(0x9039A0, 0x58));                                   // Field_ScriptFlags .. 0x9039F7
    add(R(0x905B80, 0x30));                                   // Field_EdgeBits .. Field_InputHeld (a dword)
    add(R(0x937F80, 0x10));                                   // 0x937F82, Sprite_Current, MoveScript_F3Divisor
    add(R(0x903580, 0x28));                                   // the buttons .. Field_ActiveMember
    add(R(&Input_Pressed, 4));
    add(R(&Field_MemberCount, 1));
    add(R(at::kScreenCounter, 4));
    add(R(&Game_AreaNumber, 4));                   // and MoveScript_FAWord
    add(R(&Field_Request, 1));
    add(R(0x904030, 0x130));                                  // story flags, party lists, the 9x bits, the return point
    add(R(at::kCellCycle, 1));
    add(R(at::kMemberPositions, 0x18));
    add(R(0x8034E0, 0x14));                                   // Cond_ByteFA .. Cond_ByteFD
    add(R(&Field_State, 4));
    add(R(g_script, sizeof g_script));
    add(R(g_zone, sizeof g_zone));
    add(R(g_passage, sizeof g_passage));
    add(R(g_descriptor, sizeof g_descriptor));
    add(R(g_list, sizeof g_list));
    add(R(&g_result, sizeof g_result));
    if (mask & kObj) add(R(Object(kFirstObject), kObjects * 0xA4));
    if (mask & kExt) add(R(Sprite_ObjectsExtra, kExtras * 0xA4));
    if (mask & kEff) add(R(Effect_Objects, 0xA00));
    if (mask & kAct) add(R(0x903A50, 0x5E0));                 // the run option byte and eight actor records
    if (mask & kFar)                                          // Field_ObjectAhead's answers read either way
        for (unsigned char* const o : {Object(-128), Sprite_ObjectsExtra + 97 * 0xA4, Sprite_ObjectsExtra + 98 * 0xA4,
                                       Sprite_ObjectsExtra + 224 * 0xA4})
            add(R(o, 0xA4));   // Object(-2), the other, lies in the objects' region
    unsigned total = 0;
    for (unsigned i = 0; i < g_n; ++i) total += g_r[i].size;
    if (total > kStateMax) bof3::Fatal("event_ops self-test: state of 0x%X bytes", total);
}
unsigned Total() {
    unsigned t = 0;
    for (unsigned i = 0; i < g_n; ++i) t += g_r[i].size;
    return t;
}
void Capture(unsigned char* out) {
    for (unsigned i = 0; i < g_n; out += g_r[i].size, ++i) std::memcpy(out, g_r[i].at, g_r[i].size);
}
void Apply(const unsigned char* in) {
    for (unsigned i = 0; i < g_n; in += g_r[i].size, ++i) std::memcpy(g_r[i].at, in, g_r[i].size);
}
void Randomize() {
    for (unsigned i = 0; i < g_n; ++i) {
        unsigned k = 0;
        for (; k + 4 <= g_r[i].size; k += 4) {
            const std::uint32_t v = Next();
            std::memcpy(g_r[i].at + k, &v, 4);
        }
        for (; k < g_r[i].size; ++k) g_r[i].at[k] = static_cast<unsigned char>(Next());
    }
}

unsigned g_total_bad, g_total_rounds, g_total_calls;

// Coverage: the number of distinct paths a test took, a path being the
// sequence of callees the original called with its answer's low byte.
std::uint32_t g_paths[4096];
unsigned g_n_paths;
void NotePath(std::uint32_t h) {
    h |= 1;
    for (unsigned i = h % 4096;; i = (i + 1) % 4096) {
        if (g_paths[i] == h) return;
        if (g_paths[i] == 0) {
            if (g_n_paths < 4000) {
                g_paths[i] = h;
                ++g_n_paths;
            }
            return;
        }
    }
}

// Runs `theirs` then `ours` from the state as it stands; true if the state,
// the log or the result differ.
template <class Theirs, class Ours>
bool Pair(const char* name, unsigned round, Theirs&& theirs, Ours&& ours, unsigned& bad) {
    const unsigned total = Total();
    Capture(g_input);
    g_seed = Next();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(g_input);
        std::memset(&g_log, 0, sizeof g_log);
        if (pass == 0) theirs();
        else ours();
        Capture(g_out[pass]);
        g_logs[pass] = g_log;
    }
    const bool log_differs = g_logs[0].n != g_logs[1].n || g_logs[0].hash != g_logs[1].hash;
    const bool state_differs = std::memcmp(g_out[0], g_out[1], total) != 0;
    if (!log_differs && !state_differs) return false;
    if (++bad <= 6) {
        if (state_differs) {
            unsigned at = 0;
            while (g_out[0][at] == g_out[1][at]) ++at;
            unsigned region = 0, base = 0;
            while (at >= base + g_r[region].size) base += g_r[region++].size;
            bof3::Log("shadow      event_ops %s MISMATCH: round %u, state region %u (0x%08X) +0x%X: %02X / %02X", name,
                      round, region, static_cast<unsigned>(Address(g_r[region].at)), at - base, g_out[0][at], g_out[1][at]);
        } else {
            unsigned at = 0;
            const unsigned kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], 24) == 0) ++at;
            const std::uint32_t* a = at < kKeep ? g_logs[0].keep[at] : g_logs[0].keep[0];
            const std::uint32_t* b = at < kKeep ? g_logs[1].keep[at] : g_logs[1].keep[0];
            bof3::Log("shadow      event_ops %s MISMATCH: round %u, log of %u / %u calls, first difference at entry "
                      "%u: %X(%X, %X, %X, %X) / %X(%X, %X, %X, %X)", name, round, g_logs[0].n, g_logs[1].n, at, a[0],
                      a[1], a[2], a[3], a[4], b[0], b[1], b[2], b[3], b[4]);
        }
    }
    return true;
}

// --- the seeding --------------------------------------------------------------------

// Field_MoveSpeeds' indices that do not divide by zero.
unsigned char g_paces[256];
unsigned g_n_paces;
unsigned char PickPace() {
    static const unsigned char kPace[] = {2, 3, 4, 3, 4};
    return OneIn(4) ? g_paces[Next() % g_n_paces] : Pick(kPace);
}
unsigned char PickDirection() {
    static const unsigned char kEdge[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xF, 0xFF};
    return OneIn(8) ? static_cast<unsigned char>(Next()) : Pick(kEdge);
}

// The state every test starts from: random, with the pointers valid and the
// fields each branch turns on seeded to their boundaries.
void Seed(bool objects) {
    Randomize();
    for (int i = 0; i < 3; ++i) {
        unsigned char* const m = Member(i);
        m[0x128] = PickPace();
        m[0x148] = static_cast<unsigned char>(Next() % 8);
        static const unsigned char kKinds[] = {2, 3, 4, 6, 9, 0xA, 0, 1};
        if (OneIn(2)) m[0x89] = Pick(kKinds);
        m[8] = PickDirection();
        if (OneIn(2)) m[0x70] = 0;
        if (OneIn(2)) SetWord(m + 0x34, 0);
        if (OneIn(2)) SetWord(m + 0x38, 0);
        if (OneIn(3)) m[0x136] = static_cast<unsigned char>(0xEF + Next() % 3);
        if (OneIn(3)) m[0x137] = 9;
        if (OneIn(3)) m[1] = 2;
    }
    if (objects)
        for (int i = kFirstObject; i < kFirstObject + kObjects; ++i) {
            unsigned char* const o = Object(i);
            o[8] = PickDirection();
            if (OneIn(2)) o[0x70] = 0;
            if (OneIn(2)) SetWord(o + 0x34, 0);
            if (OneIn(2)) SetWord(o + 0x38, 0);
            static const unsigned char kKinds[] = {8, 9, 9, 0, 1};
            if (OneIn(2)) o[6] = Pick(kKinds);
            if (OneIn(4)) SetWord(o + 0x88, 0xFFFF);
        }
    g_objects_live = objects;
    Sprite_Current = PickSprite(objects);
    Field_State = OneIn(6) ? Member(1 + static_cast<int>(Next() % 2)) : Member(0);
    Field_ActiveMember = objects ? Object(static_cast<int>(Next() % 30)) : Member(static_cast<int>(Next() % 3));
    static const unsigned char kCount[] = {0, 1, 2, 3, 3, 3};
    Field_MemberCount = OneIn(8) ? static_cast<unsigned char>(Next()) : Pick(kCount);
    for (unsigned i = 0; i < 6; ++i) At(at::kPartyLists)[i] = static_cast<unsigned char>(Next() % 24);
    // the input: the buttons the tests read overlapping what is pressed / held
    const std::uint32_t bits = Next();
    Input_Pressed = static_cast<unsigned short>(OneIn(3) ? 0 : bits);
    Field_InputHeld = static_cast<unsigned short>(OneIn(3) ? (Next() & 0xF000) | (Next() & 0xFFF) : Next());
    static const unsigned short kHeld[] = {0x1000, 0x2000, 0x3000, 0x4000, 0x6000, 0x8000, 0x9000, 0xC000, 0x5000, 0xF000, 0};
    if (OneIn(2)) Field_InputHeld = static_cast<unsigned short>(Pick(kHeld) | (Next() & 0xFFF));
    static const unsigned char kInput[] = {0, 1, 0x20, 0x21, 0x40, 0x41, 0x61, 0x69};
    Field_InputFlags = OneIn(4) ? static_cast<unsigned char>(Next()) : Pick(kInput);
    for (std::uint32_t b = at::kButtonConfirm; b <= at::kButtonCheck; b += 2)
        if (OneIn(3)) SetWord(At(b), 1u << (Next() % 16));
    static const unsigned char kFlagBytes[] = {0, 0, 0x10, 0x20, 0x40, 1, 2, 4, 0x30};
    for (const std::uint32_t f : {0x9039A2u, 0x9039A3u, 0x905BA4u, 0x905BA5u})
        if (OneIn(2)) *At(f) = Pick(kFlagBytes);
    static const unsigned char kRequest[] = {9, 5, 0, 4, 2};
    Field_Request = OneIn(2) ? Pick(kRequest) : static_cast<unsigned char>(Next());
    if (OneIn(2)) *At(at::kBlockByte) = 0;
    if (OneIn(3)) *At(at::kRunOption) = static_cast<unsigned char>(Next() % 3);
}

// --- the tests ----------------------------------------------------------------------

template <class F>
F Theirs(Fn k) { return reinterpret_cast<F>(g_theirs[k]); }

using V = void (__cdecl*)();
using U = unsigned char (__cdecl*)();
using UU = unsigned char (__cdecl*)(unsigned);
using VU = void (__cdecl*)(unsigned);
using VP = void (__cdecl*)(const unsigned char*);
using ULL = unsigned char (__cdecl*)(long, long, unsigned);
using ILL = int (__cdecl*)(long, long);
using BLL = unsigned char (__cdecl*)(long, long);
using PV = const unsigned char* (__cdecl*)();

// Each test: the function's name, its copy, the regions it needs, and ours.
struct Test {
    Fn fn;
    unsigned mask;
    void* ours;
};
const Test kTests[] = {
    {kOp5, kObj | kExt, reinterpret_cast<void*>(&EventOp_5x)},
    {kOpD, kObj, reinterpret_cast<void*>(&EventOp_Dx)},
    {kOp9, kObj, reinterpret_cast<void*>(&EventOp_9x)},
    {kWalk, kObj, reinterpret_cast<void*>(&Field_LeaderWalk)},
    {kSetPace, kAct, reinterpret_cast<void*>(&Field_LeaderSetPace)},
    {kStepTick, 0, reinterpret_cast<void*>(&Field_LeaderStepTick)},
    {kStepTarget, kObj, reinterpret_cast<void*>(&Field_LeaderStepTarget)},
    {kStepCell, 0, reinterpret_cast<void*>(&Field_LeaderStepCell)},
    {kEncounter, 0, reinterpret_cast<void*>(&Field_EncounterDue)},
    {kIdle, kAct, reinterpret_cast<void*>(&Field_LeaderIdleTest)},
    {kSwap, 0, reinterpret_cast<void*>(&Field_LeaderSwapTest)},
    {kCanSwap, 0, reinterpret_cast<void*>(&Party_CanSwap)},
    {kMenu, 0, reinterpret_cast<void*>(&Field_LeaderMenuTest)},
    {kReq4, 0, reinterpret_cast<void*>(&Field_LeaderRequest4Test)},
    {kReq9, 0, reinterpret_cast<void*>(&Field_LeaderRequest9Test)},
    {kDirection, 0, reinterpret_cast<void*>(&Field_LeaderDirection)},
    {kEffectAhead, kEff, reinterpret_cast<void*>(&Field_EffectAhead)},
    {kPassage, 0, reinterpret_cast<void*>(&Area_PassageAhead)},
    {kCheck, 0, reinterpret_cast<void*>(&Field_LeaderCheckTest)},
    {kEffectTest, 0, reinterpret_cast<void*>(&Field_LeaderEffectTest)},
    {kPushCount, 0, reinterpret_cast<void*>(&Field_LeaderPushCount)},
    {kTalk, 0, reinterpret_cast<void*>(&Field_LeaderTalkTest)},
    {kFacing, 0, reinterpret_cast<void*>(&Field_FacingObject)},
    {kObjectAhead, kObj | kExt | kFar, reinterpret_cast<void*>(&Field_ObjectAhead)},
    {kCellAround, 0, reinterpret_cast<void*>(&Field_CellAround)},
    {kCellNear, 0, reinterpret_cast<void*>(&Field_CellAroundNear)},
    {kTalkTo, kObj | kExt, reinterpret_cast<void*>(&Field_LeaderTalkTo)},
    {kCellEvent, 0, reinterpret_cast<void*>(&Field_LeaderCellEvent)},
    {kPushObjects, kObj | kExt, reinterpret_cast<void*>(&Field_LeaderPushObjects)},
    {kNoHook, 0, reinterpret_cast<void*>(&Scenario_NoHook)},
    {kStepHook, 0, reinterpret_cast<void*>(&Scenario_StepHook)},
    {kArriveHook, 0, reinterpret_cast<void*>(&Scenario_ArriveHook)},
    {kAreaStep, 0, reinterpret_cast<void*>(&Area_StepHook)},
    {kReturnGate, 0, reinterpret_cast<void*>(&Area_ReturnGate)},
    {kAreaArrive, 0, reinterpret_cast<void*>(&Area_ArriveHook)},
};

// The areas the hooks switch on, and the ones next to them.
unsigned short PickArea() {
    static const unsigned short kAreas[] = {0x24, 0x2A, 0x2E, 0x31, 0x3B, 0x4B, 0x4C, 0x61, 0x64, 0x69, 0x6A, 0x70, 0x74,
                                            0x87, 0x8C, 0x8F, 0x91, 0x92, 0x96, 0xAA, 0xAB, 0xAC, 0xAE, 0xAF, 0xB4, 0xB9,
                                            0xBF, 0xC0, 0xC1, 0xC5, 0x28, 0x30, 0x6F, 0x94, 0xA7, 0xA9, 0xAD, 0x23, 0x25,
                                            0xC6, 0x27, 0x29, 0xAE, 0xBB, 0, 0xFFFF, 0xC5, 0xC4};
    return OneIn(5) ? static_cast<unsigned short>(Next()) : Pick(kAreas);
}

// Area_PassageAhead's descriptor: area 0's pointer swapped for the fuzz's
// own, whose list holds runs that sometimes cover the cell ahead.
void SeedPassages() {
    const unsigned char* const sc = Sprite_Current;
    const unsigned n = Next() % 8;
    g_descriptor[0x31] = static_cast<unsigned char>(n);
    unsigned char* const list = g_list;
    std::memcpy(g_descriptor + 0x2C, &list, sizeof list);
    const unsigned char d = sc[8];
    const unsigned x = Word(sc + 0x36) + static_cast<unsigned>(static_cast<signed char>(At(at::kCellDelta)[d * 2u]));
    const unsigned z = Word(sc + 0x3A) + static_cast<unsigned>(static_cast<signed char>(At(at::kCellDelta)[d * 2u + 1]));
    for (unsigned i = 0; i <= n; ++i) {
        unsigned char* const e = g_list + i * 8;
        if (OneIn(3)) continue;   // random
        const unsigned len = 1 + Next() % 4;
        e[3] = static_cast<unsigned char>(OneIn(8) ? 0 : len);
        const unsigned k = Next() % len + (OneIn(4) ? 1 : 0);   // where along the run the cell falls
        const bool along_z = OneIn(2);
        e[2] = static_cast<unsigned char>((Next() & 0x7F) | (along_z ? 0x80 : 0));
        const unsigned dx = OneIn(3) ? Next() % 3 : 0, dz = OneIn(3) ? Next() % 3 : 0;
        e[0] = static_cast<unsigned char>((along_z ? x : x - k) + dx);
        e[1] = static_cast<unsigned char>((along_z ? z - k : z) + dz);
        if (OneIn(6)) e[along_z ? 1 : 0] = static_cast<unsigned char>(0xFF - Next() % 3);   // a run past 0xFF
    }
}

void RunTests(unsigned rounds, unsigned only) {
    for (const Test& t : kTests) {
        if (only != ~0u && only != static_cast<unsigned>(t.fn)) continue;
        UseRegions(t.mask);
        Capture(g_saved);
        g_rng = 0x5E0F0001u + t.fn * 0x9E3779B9u;
        const char* const name = kClones[t.fn].name;
        unsigned bad = 0, calls = 0, cover[4] = {};
        std::memset(g_paths, 0, sizeof g_paths);
        g_n_paths = 0;
        for (unsigned round = 0; round < rounds; ++round) {
            Seed((t.mask & kObj) != 0);
            static const unsigned char kCells[] = {0x30, 0x50, 0x51, 0x52, 0x53, 0x55, 0xA0, 0xA1, 0xAE};
            g_cell = Pick(kCells);
            unsigned arg = OneIn(2) ? g_cell : static_cast<unsigned>(Next());
            if (OneIn(2)) arg = (Next() & 0xFFFFFF00u) | (arg & 0xFF);
            long x = static_cast<long>(Next()), z = static_cast<long>(OneIn(2) ? 0x108000 - 1 + Next() % 3 : Next());
            unsigned char* const saved_area0 = Area_Descriptors[0];
            switch (t.fn) {
            case kOp5: case kOpD: case kOp9: {
                static const int kCount[] = {29, 30, 31, 0, 1, 0x7FFF, 100, -1};
                SetCount(OneIn(3) ? Pick(kCount) : static_cast<int>(Next() % 32));
                g_script[0] = static_cast<unsigned char>((t.fn == kOp5 ? 0x50 : t.fn == kOp9 ? 0x90 : 0xD0) | (Next() & 0xF));
                for (const unsigned at : {1u, 3u, 4u, 5u, 6u, 7u, 8u, 0xCu}) if (OneIn(3)) g_script[at] = OneIn(2) ? 0 : 0x80;
                if (OneIn(2)) g_script[0xC] = static_cast<unsigned char>(Next() & 1);
                Sprite_Current = Object(static_cast<int>(Next() % 30));
                break;
            }
            case kWalk: case kStepTarget: case kEncounter: case kStepTick: case kSetPace:
                if (OneIn(2)) Sprite_Current = Member(0);
                if (!OneIn(3)) Sprite_Current[0xA] = 0;
                if (t.fn == kWalk && !OneIn(4)) {   // past the two flag tests
                    *At(0x9039A3) &= 0xFE;
                    *At(0x905BA4) &= 0xBF;
                }
                if (t.fn == kEncounter) {   // each of the chain's tests passed more often than not
                    if (!OneIn(3)) SetWord(At(at::kZoneCounter), 1 + Next() % 0x40);
                    if (!OneIn(3)) Field_EdgeBits = static_cast<unsigned short>(OneIn(2) ? 0x40 + Next() % 0x40 : Next());
                    if (!OneIn(3)) *At(at::kLeaderFlags) &= 0xFE;
                    if (!OneIn(3)) for (int i = 0; i < 3; ++i) Member(i)[0] &= 0xBF;
                    if (OneIn(2)) move_script::SetLong(At(at::kScreenCounter), static_cast<std::int32_t>(0xEF + Next() % 3));
                    if (OneIn(2)) *At(at::kLeaderPace) = static_cast<unsigned char>(3 + Next() % 2);
                }
                break;
            case kIdle:
                if (OneIn(2)) Field_State[0x136] = static_cast<unsigned char>(0xF0 + Next() % 2);
                if (OneIn(2)) At(at::kActorRecords + 0x10)[Field_State[0x148] * at::kActorStride] &= 0xDF;
                break;
            case kEffectTest:
                if (OneIn(2)) arg = (arg & 0xFFFFFF00u) | 4;
                if (OneIn(2)) Field_State[0x89] = OneIn(2) ? 3 : 6;
                if (OneIn(2)) Sprite_Current[8] |= 1;
                break;
            case kPushCount:
                if (OneIn(2)) Field_State[0x89] = 2;
                if (OneIn(2)) Sprite_Current[0xB] = 0xF;
                if (OneIn(2)) Sprite_Current[8] |= 1;
                break;
            case kObjectAhead: {   // every object the answers reach within 0x100 of the sprite, often exactly
                const auto seed_object = [](unsigned char* o) {
                    if (OneIn(4)) return;
                    o[6] = static_cast<unsigned char>(OneIn(6) ? 8 : Next() % 8);
                    if (!OneIn(6)) SetWord(o + 0x88, Next() % 0xFFFF);
                    static const int kRise[] = {0x100, -0x100, 0x101, -0x101, 0xFF, 0};
                    SetWord(o + 0x3E, static_cast<unsigned>(static_cast<short>(Word(Sprite_Current + 0x3E)) + Pick(kRise)));
                };
                for (int i = 0; i < 30; ++i) seed_object(Object(i));
                for (int i = 0; i < kExtras; ++i) seed_object(Sprite_ObjectsExtra + i * 0xA4);
                seed_object(Object(-128));
                for (const int n : {97, 98, 224}) seed_object(Sprite_ObjectsExtra + n * 0xA4);
                break;
            }
            case kEffectAhead:
                for (unsigned i = 0; i < 20; ++i)
                    if (OneIn(3)) {
                        Effect_Objects[i * 0x80] = static_cast<unsigned char>(1 + Next() % 3);
                        Effect_Objects[i * 0x80 + 5] = OneIn(4) ? 0x16 : 0x17;
                    }
                break;
            case kCanSwap: case kSwap:
                for (int i = 1; i < 3; ++i) {
                    static const unsigned char kKinds[] = {3, 6, 8, 2, 9, 0};
                    if (!OneIn(3)) Member(i)[1] = 2;
                    if (!OneIn(4)) Member(i)[2] = Pick(kKinds);
                }
                if (OneIn(2)) { *At(0x905BA4) &= 0xE8; *At(0x9039A2) &= 0xE8; }
                break;
            case kCellEvent:
                if (OneIn(2)) *At(at::kCellCycle) = static_cast<unsigned char>(Next() % 4);
                if (OneIn(3)) Input_Pressed = static_cast<unsigned short>(OneIn(2) ? 0x100 : 0x800);
                break;
            case kReq9:
                if (OneIn(2)) Input_Pressed = static_cast<unsigned short>(Input_Pressed | 0x800);
                if (OneIn(2)) Field_InputFlags &= 0x96;
                break;
            case kPassage:
                Game_AreaNumber = 0;
                Area_Descriptors[0] = g_descriptor;
                SeedPassages();
                break;
            case kAreaStep: case kAreaArrive: case kReturnGate: case kStepHook: case kArriveHook: case kNoHook:
                Game_AreaNumber = PickArea();
                if (OneIn(2)) g_cell = 0xA1;
                if (OneIn(2)) Field_InputFlags = 0x40;
                if (OneIn(2)) Cond_ByteFD = 1;
                if (OneIn(3)) Cond_ByteFA = static_cast<signed char>(OneIn(2) ? -128 + static_cast<int>(Next() % 3) : 127 - static_cast<int>(Next() % 3));
                break;
            default: break;
            }
            // Field_LeaderTalkTo's object: 0..37 (the extras past the four
            // included), its register's other bits random.
            const unsigned talk = (Next() & 0xFFFFFF00u) | (OneIn(8) ? 26 + Next() % 12 : Next() % 38);
            void* const theirs = g_theirs[t.fn];
            void* const ours = t.ours;
            const auto run = [&](void* f) {
                switch (t.fn) {
                case kOp5: case kOpD: case kOp9: reinterpret_cast<VP>(f)(g_script); g_result = 0; break;
                case kWalk: case kSetPace: case kStepCell: reinterpret_cast<V>(f)(); g_result = 0; break;
                case kEffectTest: case kCellAround: case kCellNear: g_result = reinterpret_cast<UU>(f)(arg); break;
                case kTalkTo: reinterpret_cast<VU>(f)(talk); g_result = 0; break;
                case kObjectAhead: g_result = reinterpret_cast<ULL>(f)(x, z, arg); break;
                case kPassage: g_result = Address(reinterpret_cast<PV>(f)()); break;
                case kNoHook: case kReturnGate: g_result = reinterpret_cast<BLL>(f)(x, z); break;
                case kStepHook: case kArriveHook: case kAreaStep: case kAreaArrive:
                    g_result = static_cast<std::uint32_t>(reinterpret_cast<ILL>(f)(x, z));
                    break;
                default: g_result = reinterpret_cast<U>(f)(); break;
                }
            };
            // The unsigned-char answers are compared in al only: the rest of
            // the original's eax is whatever the last instruction left.
            const bool al_only = t.fn != kPassage && t.fn != kStepHook && t.fn != kArriveHook && t.fn != kAreaStep &&
                                 t.fn != kAreaArrive;
            Pair(name, round, [&] { run(theirs); if (al_only) g_result &= 0xFF; },
                 [&] { run(ours); if (al_only) g_result &= 0xFF; }, bad);
            Area_Descriptors[0] = saved_area0;
            calls += g_logs[0].n;
            cover[0] += (g_result & 0xFF) != 0;
            cover[1] += g_logs[0].n != 0;
            cover[2] += std::memcmp(g_input, g_out[0], Total()) != 0;
            NotePath(g_logs[0].path * 31u + (g_result & 0xFF));
        }
        Apply(g_saved);
        bof3::Log("shadow      event_ops %s self-test: %u rounds (%u calls to the stand-ins; %u answered non-zero, %u "
                  "called out, %u changed the state, %u%s distinct paths), %u MISMATCHES",
                  name, rounds, calls, cover[0], cover[1], cover[2], g_n_paths, g_n_paths >= 4000 ? "+" : "", bad);
        g_total_bad += bad;
        g_total_rounds += rounds;
        g_total_calls += calls;
    }
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < 256; ++i)
        if (Field_MoveSpeeds[i] != 0) g_paces[g_n_paces++] = static_cast<unsigned char>(i);
    g_stubs = MakeStubs();
    for (unsigned i = 0; i < 256; ++i) g_chapters[i] = &g_records[(i - 128) & 3];
    MakeClones();
    g = g_stubs;
    char only_name[64] = {};
    unsigned only = ~0u;
    if (GetEnvironmentVariableA("BOF3X_EVENT_OPS_ONLY", only_name, sizeof only_name))
        for (unsigned k = 0; k < kFns; ++k)
            if (std::strcmp(kClones[k].name, only_name) == 0) only = k;
    RunTests(10000, only);
    g = kOriginals;
    bof3::Log("shadow      event_ops self-test: %u functions, %u rounds, %u calls to the stand-ins, %u MISMATCHES",
              static_cast<unsigned>(kFns), g_total_rounds, g_total_calls, g_total_bad);
    if (g_total_bad) bof3::Fatal("the event script's ops differ from the original in %u self-test rounds", g_total_bad);
}

}  // namespace event_ops
