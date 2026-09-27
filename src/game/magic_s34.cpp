// Five spell overlays compiled into the exe, round nine group S34
// (docs/magic_s34.md): the PSX's MAGIC158, MAGIC159, MAGIC161, MAGIC162 and
// MAGIC166.EMI, Magic_Rows rows 89, 120, 111, 116 and 90. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Charm, (no label),
// Timed Blow, Transfer and Monopolize; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC158 0x4ED670..0x4EDDC6: sixteen motes from a pool of 64 task-like
//     records of its own (0x6A9108), each waiting a random delay, then
//     starting above the owner in one of eight colours, closing on it and
//     spiralling down; each drawn as a fan of five flat quads;
//   - MAGIC159 0x4EDDD0..0x4EE2D7: CLUT row 26 back with its STP bits, the
//     target flagged, one child (kind 1, 0x58) that follows the owner and
//     draws a textured gouraud quad that grows, holds and fades;
//   - MAGIC161 0x4EE2E0..0x4EE6BC: the owner hidden (its +0 bit 0x40) while a
//     copy of the acting actor's record (kind 1, 0x54) plays its script,
//     strikes (a sound, the target flagged 0x40) and flashes the screen red
//     at top and bottom;
//   - MAGIC162 0x4EE6C0..0x4EF20E: sixty motes from a pool of 128 records of
//     0x2C bytes (0x6AB208, the current one at 0x6AC808) that rise from the
//     owner along a sine and fade, each drawn as a star of eight gouraud
//     triangles, with rays and forks of gouraud lines on some;
//   - MAGIC166 0x4EF210..0x4EF616: MAGIC158's motes again, 32 of them from a
//     pool of its own (0x6AC810), launched and spiralling in two steps, drawn
//     by MAGIC158's matrix and fan.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s34.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The three pools. MAGIC158's and MAGIC166's hold 64 task-like records of
// 0x84 bytes (+0x80 the owner); MAGIC162's 128 records of 0x2C bytes (+0x28
// the owner), run through a "current record" pointer that follows the pool.
constexpr std::uint32_t kCharmPool = 0x6A9108;
constexpr std::uint32_t kTransferPool = 0x6AB208;
constexpr std::uint32_t kTransferCurrent = 0x6AC808;
constexpr std::uint32_t kMonopolizePool = 0x6AC810;
constexpr unsigned kMotePools = 64;
constexpr unsigned kTransferRecords = 128;
constexpr unsigned kTransferStride = 0x2C;

// The overlays' .data, read in place.
constexpr std::uint32_t kCharmColours = 0x65BF00;        // 8 x (r, g, b)
constexpr std::uint32_t kTransferOffsets = 0x65BF40;     // 32 x (s16 dx, s16 dy)
constexpr std::uint32_t kTransferColours = 0x65BFE0;     // 5 kinds x 4 x (r, g, b)
constexpr std::uint32_t kTransferCounts = 0x65C01C;      // by kind: motes made
constexpr std::uint32_t kTransferDelays = 0x65C024;      // by kind: delay step per four motes
constexpr std::uint32_t kTransferRise = 0x65C02C;        // by kind: the rise's base length
constexpr std::uint32_t kMonopolizeColours = 0x65C038;   // 8 x (r, g, b)

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Cur() { return Pointer(kTransferCurrent); }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}
void AddWord(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` then `sar m` on a dword.
int ShlSar(int v, unsigned n, unsigned m) { return static_cast<int>(static_cast<std::uint32_t>(v) << n) >> m; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code), in the fuzz that
// address's recorder.
using Fn0 = void (__cdecl*)();
using Fn2 = void (__cdecl*)(unsigned, unsigned);
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_AT(Fn2, address)(a, b); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The callees with the arguments the originals push: Gte_RotTransPers4 gets
// the depth and flag pointers, Gte_RotTrans a flag pointer; ours reads
// neither flag.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
using RotTransFn = void (__cdecl*)(const short*, long*, long*);
#define S34_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four SVECTORs projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S34_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
// Linked at the sprite's (+0x34, +0x38), layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}
// Linked at the current Transfer record's (+0x14, +0x18), layer 2.
void LinkAtCur(unsigned size) {
    const unsigned char* const r = Cur();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(r + 0x14)), static_cast<unsigned long>(Long(r + 0x18)), 2,
                                size);
}
// A draw-mode packet (tpage 0x35, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// --- the mote pools (MAGIC158, MAGIC166) ----------------------------------------

unsigned char* MoteRecord(std::uint32_t pool, unsigned index) { return Mem(pool + index * 0x84u); }

// A pool walked: every record with bit 0 of +0 becomes Sprite_Current, its
// +0x80 the owner, for its task; both put back after each. The task and owner
// are read once, after the phase call that precedes the walk.
void WalkMotes(std::uint32_t pool, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kMotePools; ++i) {
        unsigned char* const rec = MoteRecord(pool, i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

// The pool's +0..+2 cleared; the task at the source sprite (its facing and
// point), +0xB 0, +1 on; `motes` records from the pool's allocator (a full
// pool skips one), each owned by this task, +0 or 0x41, +0xB its number, +9
// a delay of 1..0x20, counted in +0xB; sound 0x100.
void StartMotes(std::uint32_t pool, unsigned motes, std::uint32_t alloc) {
    for (unsigned i = 0; i < kMotePools; ++i) {
        unsigned char* const rec = MoteRecord(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    {
        const unsigned char* const src = Pointer(at::kSource);
        unsigned char* const s = Sc();
        s[8] = src[8];
        SetLong(s + 0x34, Long(src + 0x34));
        SetLong(s + 0x38, Long(src + 0x38));
        SetLong(s + 0x3C, Long(src + 0x3C));
        s[0xB] = 0;
        Inc(s[1]);
    }
    for (unsigned i = 0; i < motes; ++i) {
        const unsigned index = MH_AT(ByteFn, alloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = MoteRecord(pool, index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(Sc())));
        rec[0] = static_cast<unsigned char>(rec[0] | 0x41);
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 0x1F) + 1);
        Inc(Sc()[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// The first of `count` records of `stride` bytes without bit 0 of +0 gets it;
// its index in al, 0xFF when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned count, unsigned stride) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const rec = Mem(pool + i * stride);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// A mote's run: its step by +2; while +0 and +2 are set, MAGIC158's matrix,
// its fan, the matrix popped.
void RunMote(const std::uint32_t* steps, unsigned n, const char* who) {
    Dispatch(steps, n, Sc()[2], who);
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::CharmMote_PushMatrix);
    Call0(addr::CharmMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// A mote's launch: +9 down; at 0 the mote at the angle (+0xB & 0xF) << 8
// round the owner at radius 8 (the sine and cosine << 15 >> 12), `lift` above
// it; a colour of eight (Rand & 7) into +0x5D..+0x5F; +0x14 0x80, +0x20
// `fall`, +9 / +0xA as given, +2 on.
void LaunchMote(std::uint32_t colours, std::uint32_t lift, std::uint32_t fall, unsigned char count, unsigned char size) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned angle = (Sc()[0xB] & 0xFu) << 8;
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        const std::int32_t x = Long(Owner() + 0x34);
        SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(v, 15, 12)) + static_cast<std::uint32_t>(x)));
    }
    v = Cos(SS(4));
    {
        const std::int32_t z = Long(Owner() + 0x38);
        SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(v, 15, 12)) + static_cast<std::uint32_t>(z)));
    }
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + lift));
    const unsigned k = RandCall() & 7u;
    unsigned char* const s = Sc();
    const unsigned char* const c = Mem(colours + 3 * k);
    s[0x5D] = c[0];
    s[0x5E] = c[1];
    s[0x5F] = c[2];
    SetLong(s + 0x14, 0x80);
    SetLong(s + 0x20, static_cast<std::int32_t>(fall));
    s[9] = count;
    s[0xA] = size;
    Inc(s[2]);
}

// A mote's spiral: the point on by the sine and cosine of the angle (sign
// extended from 20 bits), each added through the pointer taken before its
// call; the fall +0x20 into the speed +0x14, the speed into the height +0x3E.
void SpiralMote(unsigned angle) {
    SetSW(4, angle);
    unsigned char* at = Sc() + 0x34;
    int v = Sin(static_cast<short>(angle));
    AddLong(at, static_cast<std::uint32_t>(ShlSar(v, 12, 12)));
    at = Sc() + 0x38;
    v = Cos(SS(4));
    AddLong(at, static_cast<std::uint32_t>(ShlSar(v, 12, 12)));
    unsigned char* const s = Sc();
    AddLong(s + 0x14, static_cast<std::uint32_t>(Long(s + 0x20)));
    AddWord(s + 0x3E, Word(s + 0x14));
}

// The mote freed: the owner's count +0xB down, +0..+4 cleared.
void FreeMote(unsigned char* s) {
    Dec(Owner()[0xB]);
    s[0] = 0;
    s[1] = 0;
    s[2] = 0;
    s[3] = 0;
    s[4] = 0;
}

struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

// --- MAGIC161 -------------------------------------------------------------------

// The raw floats of TimedBlow_DrawFlash's corners: 319.0, 120.0 and 359.0.
constexpr std::int32_t kRight = 0x439F8000;
constexpr std::int32_t kMiddle = 0x42F00000;
constexpr std::int32_t kBottom = 0x43B38000;

void Shade(unsigned char* p, unsigned k, unsigned char r, unsigned char g) {
    p[k] = r;
    p[k + 1] = g;
    p[k + 2] = g;
}

// --- MAGIC162 -------------------------------------------------------------------

unsigned char* TransferRecord(unsigned index) { return Mem(kTransferPool + index * kTransferStride); }
short OffsetX(unsigned k) { return static_cast<short>(Word(Mem(kTransferOffsets + 4 * (k & 0x1F)))); }
std::uint16_t OffsetXW(unsigned k) { return Word(Mem(kTransferOffsets + 4 * (k & 0x1F))); }
std::uint16_t OffsetYW(unsigned k) { return Word(Mem(kTransferOffsets + 4 * (k & 0x1F) + 2)); }

// The current record's +0xC up, its x +0x20 its base +8 plus the sine of
// (+0xC & 0x3F) << 6 x 24 >> 12, its y +0x22 on by +0xA.
void SwayMote() {
    AddWord(Cur() + 0xC, 1);
    const unsigned angle = (Cur()[0xC] & 0x3Fu) << 6;
    SetSW(6, angle);
    const int v = Sin(static_cast<short>(angle));
    unsigned char* const r = Cur();
    const int dx = static_cast<int>(static_cast<std::uint32_t>(v) * 24u) >> 12;
    SetWord(r + 0x20, (static_cast<unsigned>(dx) + Word(r + 8)) & 0xFFFF);
    AddWord(r + 0x22, Word(r + 0xA));
}

// Words 0x903850 / 52 the current record's point, 58 / 5A / 5C its +5 x 6;
// a draw-mode packet linked at the record.
void LinesHead() {
    {
        const unsigned char* const r = Cur();
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
    }
    DrawMode(0x35);
    LinkAtCur(0xC);
    const unsigned char* const r = Cur();
    const unsigned shade = r[5] * 6u;
    SetSW(8, shade);
    SetSW(0xA, shade);
    SetSW(0xC, shade);
}

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC158 (row 89, Charm read one id down)

// original 0x4ED670: the kind-2 task. A two-entry stack table by +1
// (Charm_Start, BattleFx_Finish), then Charm_Pool walked through
// CharmMote_Task.
S34_EXPORT void __cdecl Charm_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Charm_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Charm_Task");
    WalkMotes(kCharmPool, addr::CharmMote_Task);
}

// original 0x4ED6F0: the pool cleared, the task at the source sprite, sixteen
// motes (StartMotes), sound 0x100.
S34_EXPORT void __cdecl Charm_Start(void) { StartMotes(kCharmPool, 16, addr::Charm_PoolAlloc); }

// original 0x4ED7D0: a mote's task, a jmp through CharmMote_TaskTable (one
// entry) by +1, unchecked.
S34_EXPORT void __cdecl CharmMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::CharmMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "CharmMote_Task");
}

// original 0x4ED7F0: a call through CharmMote_Steps (three) by +2; while +0
// and +2 are set the matrix, the fan, the matrix popped.
S34_EXPORT void __cdecl CharmMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CharmMote_Launch, addr::CharmMote_ToOwner, addr::CharmMote_Fall};
    RunMote(kSteps, 3, "CharmMote_Run");
}

// original 0x4ED830: +9 down; at 0 round the owner, 0x480 units above it, a
// colour from Charm_Colours; +0x14 0x80, +0x20 -16, +9 8, +0xA 2, +2 on.
S34_EXPORT void __cdecl CharmMote_Launch(void) { LaunchMote(kCharmColours, 0x4800000, 0xFFFFFFF0u, 8, 2); }

// original 0x4ED940: the height word +0x3E down by 0x60; +9 up to 0x10, +0xA
// up by 2 to 0x10; +2 on once the height is the owner's.
S34_EXPORT void __cdecl CharmMote_ToOwner(void) {
    unsigned char* const s = Sc();
    AddWord(s + 0x3E, 0xFFA0);
    if (s[9] != 0x10) Inc(s[9]);
    if (s[0xA] != 0x10) s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (Word(s + 0x3E) == Word(Owner() + 0x3E)) Inc(s[2]);
}

// original 0x4ED990: the spiral at (+0xB & 0xF) << 8; +0xA down, at 0 the
// mote freed.
S34_EXPORT void __cdecl CharmMote_Fall(void) {
    SpiralMote((Sc()[0xB] & 0xFu) << 8);
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    FreeMote(s);
}

// original 0x4EDA60: the mote's matrix pushed: turned (0x400, 0xE00, the
// frame counter's low five bits << 7, inverted when bit 0 of +0xB is set),
// at (+0x34 >> 9) - 0x4000, (+0x38 >> 9) - 0x4000, -(s16 +0x3E / 2), after
// the camera's.
S34_EXPORT void __cdecl CharmMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    short rot[4];
    rot[0] = 0x400;
    rot[1] = 0xE00;
    const auto frame = static_cast<unsigned char>(Frame_Counter);
    if (s[0xB] & 1) rot[2] = static_cast<short>(((~static_cast<unsigned>(frame)) & 0x1Fu) << 7);
    else rot[2] = static_cast<short>((frame & 0x1Fu) << 7);
    rot[3] = 0;
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    S34_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4EDB30: a draw-mode packet (tpage 0x35); then a fan of five
// semi-transparent flat quads (Gpu_SetPolyF4) from the centre, each between
// the angles 0x199 i, 0x199 (i + 1) and 0x199 (i + 2) (0 for the last) at
// radii +9 x 2 and +9 x 4, its colour +0x5D..+0x5F (signed) x +0xA; linked
// at the mote, layer 2.
S34_EXPORT void __cdecl CharmMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[9] * 2u);
        SetSW(2, s[9] * 4u);
        SetSW(6, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[0xA])));
        SetSW(8, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[0xA])));
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[0xA])));
    }
    for (int i = 0; i < 10; i += 2) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyF4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int a = (i + 1) * 0x199;
        int v = Sin(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
        SetVW(4, 0);
        a = i * 0x199;
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0xC, 0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
        const unsigned last = i == 8 ? 0u : static_cast<unsigned>((i + 2) * 0x199);
        SetSW(4, last);
        v = Sin(static_cast<short>(last));
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(SS(4));
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(0x14, 0);
        SetVW(0x18, 0);
        SetVW(0x1A, 0);
        SetVW(0x1C, 0);
        Rtp4(p, 0xC);
        MH_CALL(Gte_PrimDepths4_0C)(p);
        p[4] = SB(6);
        p[5] = SB(8);
        p[6] = SB(0xA);
        LinkAtSprite(0x38);
    }
}

// original 0x4EDD70: the first of Charm_Pool's 64 records without bit 0 of
// +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Charm_PoolAlloc(void) { return PoolAlloc(kCharmPool, kMotePools, 0x84); }

// ===========================================================================
// MAGIC159 (row 120, no label one id down)

// original 0x4EDDD0: the kind-2 task. A two-entry stack table by +1
// (Magic159_Start, BattleFx_Finish).
S34_EXPORT void __cdecl Magic159_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Magic159_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Magic159_Task");
}

// original 0x4EDE00: the task at the source sprite's point, +0xB 0, +1 on;
// one child (kind 1, 0x58) owned by this task, +1 0, +9 8, counted in +0xB;
// row 26 of the CLUT strip back with its STP bits; the target flagged 0x10;
// sound 0x100.
S34_EXPORT void __cdecl Magic159_Start(void) {
    {
        const unsigned char* const src = Pointer(at::kSource);
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Long(src + 0x34));
        SetLong(s + 0x38, Long(src + 0x38));
        SetLong(s + 0x3C, Long(src + 0x3C));
        s[0xB] = 0;
        Inc(s[1]);
    }
    const unsigned slot = NewTask(0x58);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    child[9] = 8;
    Inc(self[0xB]);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    const unsigned char target = Mem(at::kTarget)[0];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Battle_SetTargetFlags)(target, 0x10);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4EDEC0: the child's kind-1 task, a jmp through
// Magic159Child_TaskTable (one entry) by +1, unchecked.
S34_EXPORT void __cdecl Magic159Child_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::Magic159Child_Run};
    Dispatch(kKinds, 1, Sc()[1], "Magic159Child_Task");
}

// original 0x4EDEE0: a call through Magic159Child_Steps (four) by +2; while
// +0 and +2 are set the screen point (BattleActor_UpdateScreenXY), x + 8,
// y - 0x10, and the quad (a tail jmp).
S34_EXPORT void __cdecl Magic159Child_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::Magic159Child_Wait, addr::Magic159Child_Grow,
                                                addr::Magic159Child_Hold, addr::Magic159Child_Fade};
    Dispatch(kSteps, 4, Sc()[2], "Magic159Child_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    AddWord(Sc() + 0x2E, 8);
    AddWord(Sc() + 0x30, 0xFFF0);
    Call0(addr::Magic159Child_Draw);
}

// original 0x4EDF30: +9 down; at 0 the child at the owner's point, +9 0x10,
// +0xA 0, +2 on.
S34_EXPORT void __cdecl Magic159Child_Wait(void) {
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    SetLong(s + 0x34, Long(Owner() + 0x34));
    SetLong(s + 0x38, Long(Owner() + 0x38));
    SetLong(s + 0x3C, Long(Owner() + 0x3C));
    s[9] = 0x10;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4EDFA0: +9 up; +0xA up by 2, at 0x10 +2 on.
S34_EXPORT void __cdecl Magic159Child_Grow(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (s[0xA] == 0x10) Inc(s[2]);
}

// original 0x4EDFD0: +9 up; past 0x20 +2 on.
S34_EXPORT void __cdecl Magic159Child_Hold(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] > 0x20) Inc(s[2]);
}

// original 0x4EDFF0: +9 up; +0xA down by 2, at 0 the owner's count +0xB down
// and the task freed (a tail jmp).
S34_EXPORT void __cdecl Magic159Child_Fade(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EE030: a draw-mode packet (tpage 0x35); one semi-transparent
// textured gouraud quad (Gpu_SetPolyGT4) round the screen point (+0x2E,
// +0x30) at radius +9 x 2, its corners at the angles 0xE00, 0xA00, 0x200,
// 0x600, page (0x340, 0x100), CLUT row 0x1FA, the texture's (8, 8)..(0x57,
// 0x48), shaded +0xA x 8 on the first two corners and +0xA on the last two;
// linked at the child, layer 2.
S34_EXPORT void __cdecl Magic159Child_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* p;
    {
        const unsigned char* const s = Sc();
        p = Gfx_PacketNext;
        SetSW(0, s[9] * 2u);
        SetSW(2, s[0xA] * 8u);
        SetSW(4, s[0xA]);
    }
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0xE00, 0xA00, 0x200, 0x600};
    static constexpr unsigned kAt[4] = {8, 0x1C, 0x30, 0x44};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        PutFloat(p + kAt[c], Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = Sin(kAngles[c]);
        PutFloat(p + kAt[c] + 4, Mul12(v, SS(0)) + S16(Sc() + 0x30));
    }
    SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100) & 0xFFFF);
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA) & 0xFFFF);
    p[0x14] = 8;
    p[0x15] = 8;
    p[0x29] = 8;
    p[0x3C] = 8;
    p[0x28] = 0x57;
    p[0x3D] = 0x48;
    p[0x50] = 0x57;
    p[0x51] = 0x48;
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = SB(2);
    for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(4);
    LinkAtSprite(0x54);
}

// ===========================================================================
// MAGIC161 (row 111, Timed Blow read one id down)

// original 0x4EE2E0: the kind-2 task. A three-entry stack table by +1
// (TimedBlow_Start, TimedBlow_Wait, MagicFx_DoneAndFree).
S34_EXPORT void __cdecl TimedBlow_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::TimedBlow_Start, addr::TimedBlow_Wait, addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "TimedBlow_Task");
}

// original 0x4EE310: the task at the owner (its facing and point), +0xB 0,
// +1 on; the actor animation 0xC (BattleActor_SetAnimation(0xC, 2)); a child
// (kind 1, 0x54) that is a copy of the acting actor's record's first 0x80
// bytes (a party member at 0..2, else the enemy by index - 3, unchecked),
// owned by this task, +1 / +2 0, +6 1, +5 0x54, counted in +0xB; the owner's
// +0 bit 0x40 set.
S34_EXPORT void __cdecl TimedBlow_Start(void) {
    {
        unsigned char* const s = Sc();
        s[8] = Owner()[8];
        SetLong(s + 0x34, Long(Owner() + 0x34));
        SetLong(s + 0x38, Long(Owner() + 0x38));
        SetLong(s + 0x3C, Long(Owner() + 0x3C));
        s[0xB] = 0;
        Inc(s[1]);
    }
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    const unsigned slot = NewTask(0x54);
    const unsigned actor = Mem(at::kActor)[0];
    const unsigned char* const rec = actor <= 2 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const child = TaskSlot(slot);
    // `rep movsd`: dword by dword, forward.
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(rec + k));
    unsigned char* const self = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x54;
    Inc(self[0xB]);
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
}

// original 0x4EE430: once the child count +0xB is 0, the owner's bit 0x40
// cleared, the actor animation 0x18 (BattleActor_SetAnimation(0x18, 0)), +1
// on.
S34_EXPORT void __cdecl TimedBlow_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    MH_CALL(BattleActor_SetAnimation)(0x18, 0);
    Inc(Sc()[1]);
}

// original 0x4EE460: the copy's kind-1 task, a jmp through
// TimedBlowCopy_TaskTable (one entry) by +1, unchecked.
S34_EXPORT void __cdecl TimedBlowCopy_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TimedBlowCopy_Run};
    Dispatch(kKinds, 1, Sc()[1], "TimedBlowCopy_Task");
}

// original 0x4EE480: a five-entry stack table by +2 (BattleFx_SetSize,
// TimedBlowCopy_Strike, TimedBlowCopy_Flash, BattleFx_ScriptToEnd,
// BattleFx_FreeTask); the screen update while +0 and +2 are set.
S34_EXPORT void __cdecl TimedBlowCopy_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::BattleFx_SetSize, addr::TimedBlowCopy_Strike,
                                                addr::TimedBlowCopy_Flash, addr::BattleFx_ScriptToEnd,
                                                addr::BattleFx_FreeTask};
    Dispatch(kSteps, 5, Sc()[2], "TimedBlowCopy_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4EE4E0: the script ticked once (its answer unread); +9 down, at
// 0 the actor sound (BattleActor_PlaySound(2, 4)), the target flagged 0x40,
// +2 on.
S34_EXPORT void __cdecl TimedBlowCopy_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 4);
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Inc(Sc()[2]);
}

// original 0x4EE520: at +9 0x10 sound 0x100 and +2 on, else +9 up by 4; the
// flash drawn, the script ticked once (a tail jmp).
S34_EXPORT void __cdecl TimedBlowCopy_Flash(void) {
    unsigned char* const s = Sc();
    const unsigned char n = s[9];
    if (n == 0x10) {
        MH_CALL(Sound_PlayById)(0x100);
        Inc(Sc()[2]);
    } else {
        s[9] = static_cast<unsigned char>(n + 4);
    }
    Call0(addr::TimedBlow_DrawFlash);
    MH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x4EE560, shared by eleven files: the script ticked once; when it
// reports its end, the owner's count +0xB down and +2 on.
S34_EXPORT void __cdecl BattleFx_ScriptToEnd(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4EE580: a draw-mode packet (tpage 0x35) on layer 1; two
// semi-transparent gouraud quads over the screen, (0, 0)..(319, 120) and
// (0, 120)..(319, 359), red (0xC0, 0x60, 0x60) at the top and bottom edges
// and +9 x (0xC, 6, 6) where they meet; a closing draw-mode packet (tpage
// 0x15). The corners are float constants.
S34_EXPORT void __cdecl TimedBlow_DrawFlash(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char n = Sc()[9];
    unsigned char* p = Gfx_PacketNext;
    const auto edge = static_cast<unsigned char>(n * 0xCu);
    const auto mid = static_cast<unsigned char>(n * 6u);
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetLong(p + 0x18, kRight);
    SetLong(p + 0x38, kRight);
    SetLong(p + 8, 0);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x1C, 0);
    SetLong(p + 0x28, 0);
    SetLong(p + 0x2C, kMiddle);
    SetLong(p + 0x3C, kMiddle);
    Shade(p, 4, 0xC0, 0x60);
    Shade(p, 0x14, 0xC0, 0x60);
    Shade(p, 0x24, edge, mid);
    Shade(p, 0x34, edge, mid);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetLong(p + 0x2C, kBottom);
    SetLong(p + 0x3C, kBottom);
    SetLong(p + 8, 0);
    SetLong(p + 0xC, kMiddle);
    SetLong(p + 0x18, kRight);
    SetLong(p + 0x1C, kMiddle);
    SetLong(p + 0x28, 0);
    SetLong(p + 0x38, kRight);
    Shade(p, 4, edge, mid);
    Shade(p, 0x14, edge, mid);
    Shade(p, 0x24, 0xC0, 0x60);
    Shade(p, 0x34, 0xC0, 0x60);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// ===========================================================================
// MAGIC162 (row 116, Transfer read one id down)

// original 0x4EE6C0: the kind-2 task. A six-entry stack table by +1
// (Transfer_Start, BattleFx_TintActor, BattleFx_Brighten, BattleFx_WaitStep4,
// Sparkle_End, BattleFx_Finish); then Transfer_Pool walked: every record with
// bit 0 of +0 becomes TransferMote_Current, its +0x28 the owner, for
// TransferMote_Task; the owner put back after each (the current pointer is
// not).
S34_EXPORT void __cdecl Transfer_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {addr::Transfer_Start,    addr::BattleFx_TintActor, addr::BattleFx_Brighten,
                                                 addr::BattleFx_WaitStep4, addr::Sparkle_End,        addr::BattleFx_Finish};
    Dispatch(kPhases, 6, Sc()[1], "Transfer_Task");
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kTransferRecords; ++i) {
        unsigned char* const rec = TransferRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x28);
        SetLong(Mem(kTransferCurrent), static_cast<std::int32_t>(Key(rec)));
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(addr::TransferMote_Task);
        SetLong(Mem(at::kOwner), owner);
    }
}

// original 0x4EE750: the pool's +0..+2 cleared; +4 4 (the kind); the task at
// the source sprite (its facing and point) and on the screen; +0xB 0, +9 8,
// +0xA 0, +1 on; sound 0x100; then Transfer_Counts[+4] records
// (Transfer_PoolAlloc; a full pool skips one), each owned by this task, +1 /
// +2 0, +3 Rand & 3, +4 the kind, +7 its number, +5 a delay of (number / 4)
// x Transfer_Delays[kind] + 1, counted in +0xB. Sprite_Current's +4 is read
// again at each use.
S34_EXPORT void __cdecl Transfer_Start(void) {
    for (unsigned i = 0; i < kTransferRecords; ++i) {
        unsigned char* const rec = TransferRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    Sc()[4] = 4;
    {
        const unsigned char* const src = Pointer(at::kSource);
        unsigned char* const s = Sc();
        s[8] = src[8];
        SetLong(s + 0x34, Long(src + 0x34));
        SetLong(s + 0x38, Long(src + 0x38));
        SetLong(s + 0x3C, Long(src + 0x3C));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        unsigned char* const s = Sc();
        s[0xB] = 0;
        s[9] = 8;
        s[0xA] = 0;
        Inc(s[1]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    if (Mem(kTransferCounts + Sc()[4])[0] == 0) return;
    unsigned char i = 0;
    do {
        const unsigned index = MH_AT(ByteFn, addr::Transfer_PoolAlloc)() & 0xFFu;
        if (index != 0xFF) {
            unsigned char* const rec = TransferRecord(index);
            SetLong(rec + 0x28, static_cast<std::int32_t>(Key(Sc())));
            rec[1] = 0;
            rec[2] = 0;
            const std::uint32_t r = RandCall();
            unsigned char* const self = Sc();
            rec[3] = static_cast<unsigned char>(r & 3);
            rec[4] = self[4];
            rec[7] = i;
            const unsigned char step = Mem(kTransferDelays + self[4])[0];
            rec[5] = static_cast<unsigned char>(static_cast<unsigned>(i >> 2) * step + 1);
            Inc(self[0xB]);
        }
        ++i;
    } while (i < Mem(kTransferCounts + Sc()[4])[0]);
}

// original 0x4EE8C0: a record's task, a jmp through TransferMote_TaskTable
// (one entry) by the current record's +1, unchecked.
S34_EXPORT void __cdecl TransferMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TransferMote_Run};
    Dispatch(kKinds, 1, Cur()[1], "TransferMote_Task");
}

// original 0x4EE8E0: a three-entry stack table by the current record's +2
// (TransferMote_Start, _Rise, _Fade); while its +0 bit 0 and +2 are set the
// star, and when +0xC & 3 is not 0 the rays (Frame_Counter >> 1, +6) and the
// forks (Frame_Counter >> 1 + 4, +6 >> 1 + +6).
S34_EXPORT void __cdecl TransferMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::TransferMote_Start, addr::TransferMote_Rise, addr::TransferMote_Fade};
    Dispatch(kSteps, 3, Cur()[2], "TransferMote_Run");
    {
        const unsigned char* const r = Cur();
        if ((r[0] & 1) == 0 || r[2] == 0) return;
    }
    Call0(addr::TransferMote_DrawStar);
    if ((Cur()[0xC] & 3) == 0) return;
    Call2(addr::TransferMote_DrawRays, Frame_Counter >> 1, Cur()[6]);
    const unsigned char b = Cur()[6];
    Call2(addr::TransferMote_DrawForks, (Frame_Counter >> 1) + 4, (static_cast<unsigned>(b >> 1) + b) & 0xFFFF);
}

// original 0x4EE970: +5 down; at 0 the record at the owner's point (+0x14..
// +0x1C) and screen point (+0x20, +0x22), moved by
// TransferMote_Offsets[+7 & 0x1F] and Rand & 7 (away from 0 in x, up in y);
// +8 the x, +0xA Rand & 1, +0xC Rand, +7 (Rand & 6) + Transfer_Rise[+4], +5
// 0x10, +6 0, +2 on.
S34_EXPORT void __cdecl TransferMote_Start(void) {
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    {
        SetLong(Cur() + 0x14, Long(Owner() + 0x34));
        SetLong(Cur() + 0x18, Long(Owner() + 0x38));
        SetLong(Cur() + 0x1C, Long(Owner() + 0x3C));
        SetWord(Cur() + 0x20, Word(Owner() + 0x2E));
        SetWord(Cur() + 0x22, Word(Owner() + 0x30));
    }
    unsigned char* at = Cur() + 0x20;
    if (OffsetX(Cur()[7]) < 0) {
        const std::uint32_t r = RandCall();
        const unsigned d = (OffsetXW(Cur()[7]) - (r & 7u)) & 0xFFFF;
        AddWord(at, d);
    } else {
        const std::uint32_t r = RandCall();
        const unsigned d = ((r & 7u) + OffsetXW(Cur()[7])) & 0xFFFF;
        AddWord(at, d);
    }
    at = Cur() + 0x22;
    {
        const std::uint32_t r = RandCall();
        const unsigned d = ((r & 7u) + OffsetYW(Cur()[7])) & 0xFFFF;
        SetWord(at, (Word(at) - d) & 0xFFFF);
    }
    SetWord(Cur() + 8, Word(Cur() + 0x20));
    {
        const std::uint32_t r = RandCall();
        SetWord(Cur() + 0xA, r & 1);
    }
    {
        const std::uint32_t r = RandCall();
        SetWord(Cur() + 0xC, r & 0xFFFF);
    }
    const std::uint32_t r = RandCall();
    unsigned char* const rec = Cur();
    rec[7] = static_cast<unsigned char>((r & 6) + Mem(kTransferRise + rec[4])[0]);
    rec[5] = 0x10;
    rec[6] = 0;
    Inc(rec[2]);
}

// original 0x4EEAE0: the sway (SwayMote); +6 up, at +7 +2 on.
S34_EXPORT void __cdecl TransferMote_Rise(void) {
    SwayMote();
    unsigned char* const r = Cur();
    Inc(r[6]);
    if (r[6] == r[7]) Inc(r[2]);
}

// original 0x4EEB50: the sway; every fourth frame +6 down; +5 down, at 0 the
// owner's count +0xB down and the record freed (a tail jmp).
S34_EXPORT void __cdecl TransferMote_Fade(void) {
    SwayMote();
    if ((static_cast<unsigned char>(Frame_Counter) & 3) == 0) Dec(Cur()[6]);
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::TransferMote_Free);
}

// original 0x4EEBE0: four semi-transparent gouraud lines (Gpu_SetLineG2)
// from the record's screen point at the angles (a1 + 8 i & 0x1F) << 7 for
// i < 4, length a2 (both u16), shaded +5 x 6 at the point and 1 at the end;
// after a draw-mode packet (tpage 0x35), all linked at the record.
S34_EXPORT void __cdecl TransferMote_DrawRays(unsigned a1, unsigned a2) {
    LinesHead();
    const unsigned from = a1 & 0xFFFF;
    const unsigned to = from + 0x20;
    const int length = static_cast<int>(a2 & 0xFFFF);
    for (unsigned k = from; k < to; k += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SS(0);
        SetSW(6, (k & 0x1Fu) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = Cos(SS(6));
        PutFloat(p + 0x18, Mul12(v, length) + SS(0));
        v = Sin(SS(6));
        PutFloat(p + 0x1C, Mul12(v, length) + SS(2));
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        LinkAtCur(0x34);
    }
}

// original 0x4EED70: four three-point gouraud lines (Gpu_SetLineG3) from the
// record's screen point at the angles (a1 + 8 i & 0x1F) << 7, to a2 / 2 and
// a2 along it (both u16), shaded 1, +5 x 6, 1; after a draw-mode packet
// (tpage 0x35), all linked at the record.
S34_EXPORT void __cdecl TransferMote_DrawForks(unsigned a1, unsigned a2) {
    LinesHead();
    const unsigned from = a1 & 0xFFFF;
    const unsigned to = from + 0x20;
    const int outer = static_cast<int>(a2 & 0xFFFF);
    const int inner = outer >> 1;
    for (unsigned k = from; k < to; k += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SS(0);
        SetSW(6, (k & 0x1Fu) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = Cos(SS(6));
        PutFloat(p + 0x18, Mul12(v, inner) + SS(0));
        v = Sin(SS(6));
        PutFloat(p + 0x1C, Mul12(v, inner) + SS(2));
        v = Cos(SS(6));
        PutFloat(p + 0x28, Mul12(v, outer) + SS(0));
        v = Sin(SS(6));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, Mul12(v, outer) + SS(2));
        p[0x14] = SB(8);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        LinkAtCur(0x34);
    }
}

// original 0x4EEF60: after a draw-mode packet (tpage 0x35), a star of eight
// semi-transparent gouraud triangles (Gpu_SetPolyG3) round the record's
// screen point at radius +6 + (Rand & 3), each 0x200 wide; the centre shaded
// TransferMote_Colours[+4 x 4 + +3] x +5, the rim +5; all linked at the
// record.
S34_EXPORT void __cdecl TransferMote_DrawStar(void) {
    DrawMode(0x35);
    LinkAtCur(0xC);
    const std::uint32_t rnd = RandCall();
    {
        const unsigned char* const r = Cur();
        SetSW(4, (rnd & 3u) + r[6]);
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
        const unsigned index = r[3] + r[4] * 4u;
        const unsigned char* const c = Mem(kTransferColours + index * 3u);
        SetSW(8, c[0] * static_cast<unsigned>(r[5]));
        SetSW(0xA, c[1] * static_cast<unsigned>(r[5]));
        SetSW(0xC, c[2] * static_cast<unsigned>(r[5]));
        SetSW(0xE, r[5]);
    }
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = Sin(a);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = Cos(a);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        a += 0x200;
        v = Sin(a);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = Cos(a);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xE);
        LinkAtCur(0x34);
    }
}

// original 0x4EF190: the first of Transfer_Pool's 128 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Transfer_PoolAlloc(void) {
    return PoolAlloc(kTransferPool, kTransferRecords, kTransferStride);
}

// original 0x4EF1E0: the current record's +0..+4 cleared.
S34_EXPORT void __cdecl TransferMote_Free(void) {
    unsigned char* const r = Cur();
    r[0] = 0;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
    r[4] = 0;
}

// ===========================================================================
// MAGIC166 (row 90, Monopolize read one id down)

// original 0x4EF210: the kind-2 task. A two-entry stack table by +1
// (Monopolize_Start, BattleFx_Finish), then Monopolize_Pool walked through
// MonopolizeMote_Task.
S34_EXPORT void __cdecl Monopolize_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Monopolize_Start, addr::BattleFx_Finish};
    Dispatch(kPhases, 2, Sc()[1], "Monopolize_Task");
    WalkMotes(kMonopolizePool, addr::MonopolizeMote_Task);
}

// original 0x4EF290: the pool cleared, the task at the source sprite, 32
// motes (StartMotes), sound 0x100.
S34_EXPORT void __cdecl Monopolize_Start(void) { StartMotes(kMonopolizePool, 32, addr::Monopolize_PoolAlloc); }

// original 0x4EF370: a mote's task, a jmp through MonopolizeMote_TaskTable
// (one entry) by +1, unchecked.
S34_EXPORT void __cdecl MonopolizeMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::MonopolizeMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "MonopolizeMote_Task");
}

// original 0x4EF390: a call through MonopolizeMote_Steps (two) by +2; while
// +0 and +2 are set MAGIC158's matrix and fan, the matrix popped.
S34_EXPORT void __cdecl MonopolizeMote_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {addr::MonopolizeMote_Launch, addr::MonopolizeMote_Fall};
    RunMote(kSteps, 2, "MonopolizeMote_Run");
}

// original 0x4EF3D0: +9 down; at 0 round the owner at its height, a colour
// from Monopolize_Colours; +0x14 0x80, +0x20 -8, +9 1, +0xA 0x10, +2 on.
S34_EXPORT void __cdecl MonopolizeMote_Launch(void) { LaunchMote(kMonopolizeColours, 0, 0xFFFFFFF8u, 1, 0x10); }

// original 0x4EF4E0: the spiral at (+0xB & 0x1F) << 7; +9 up to 0x10; once
// there +0xA down, at 0 the mote freed.
S34_EXPORT void __cdecl MonopolizeMote_Fall(void) {
    SpiralMote((Sc()[0xB] & 0x1Fu) << 7);
    unsigned char* const s = Sc();
    if (s[9] != 0x10) {
        Inc(s[9]);
        return;
    }
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    FreeMote(s);
}

// original 0x4EF5C0: the first of Monopolize_Pool's 64 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S34_EXPORT unsigned char __cdecl Monopolize_PoolAlloc(void) { return PoolAlloc(kMonopolizePool, kMotePools, 0x84); }

void MagicS34_Inject() {
    if (bof3::WantsShadow("magic_s34")) magic_s34::SelfTest();
    BOF3_INJECT(Charm_Task);
    BOF3_INJECT(Charm_Start);
    BOF3_INJECT(CharmMote_Task);
    BOF3_INJECT(CharmMote_Run);
    BOF3_INJECT(CharmMote_Launch);
    BOF3_INJECT(CharmMote_ToOwner);
    BOF3_INJECT(CharmMote_Fall);
    BOF3_INJECT(CharmMote_PushMatrix);
    BOF3_INJECT(CharmMote_Draw);
    BOF3_INJECT(Charm_PoolAlloc);
    BOF3_INJECT(Magic159_Task);
    BOF3_INJECT(Magic159_Start);
    BOF3_INJECT(Magic159Child_Task);
    BOF3_INJECT(Magic159Child_Run);
    BOF3_INJECT(Magic159Child_Wait);
    BOF3_INJECT(Magic159Child_Grow);
    BOF3_INJECT(Magic159Child_Hold);
    BOF3_INJECT(Magic159Child_Fade);
    BOF3_INJECT(Magic159Child_Draw);
    BOF3_INJECT(TimedBlow_Task);
    BOF3_INJECT(TimedBlow_Start);
    BOF3_INJECT(TimedBlow_Wait);
    BOF3_INJECT(TimedBlowCopy_Task);
    BOF3_INJECT(TimedBlowCopy_Run);
    BOF3_INJECT(TimedBlowCopy_Strike);
    BOF3_INJECT(TimedBlowCopy_Flash);
    BOF3_INJECT(BattleFx_ScriptToEnd);
    BOF3_INJECT(TimedBlow_DrawFlash);
    BOF3_INJECT(Transfer_Task);
    BOF3_INJECT(Transfer_Start);
    BOF3_INJECT(TransferMote_Task);
    BOF3_INJECT(TransferMote_Run);
    BOF3_INJECT(TransferMote_Start);
    BOF3_INJECT(TransferMote_Rise);
    BOF3_INJECT(TransferMote_Fade);
    BOF3_INJECT(TransferMote_DrawRays);
    BOF3_INJECT(TransferMote_DrawForks);
    BOF3_INJECT(TransferMote_DrawStar);
    BOF3_INJECT(Transfer_PoolAlloc);
    BOF3_INJECT(TransferMote_Free);
    BOF3_INJECT(Monopolize_Task);
    BOF3_INJECT(Monopolize_Start);
    BOF3_INJECT(MonopolizeMote_Task);
    BOF3_INJECT(MonopolizeMote_Run);
    BOF3_INJECT(MonopolizeMote_Launch);
    BOF3_INJECT(MonopolizeMote_Fall);
    BOF3_INJECT(Monopolize_PoolAlloc);
}
