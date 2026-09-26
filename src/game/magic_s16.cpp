// Four sparkle overlays: the PSX's MAGIC071..074.EMI (Magic_Rows rows 45, 113,
// 47, 114; read one id down, Healing Herb / Rejuvenate, Restore, Vitalize and
// Vigor - names that are hypotheses, cut-content section 2), compiled into the
// exe at 0x4B9930..0x4BCBEE. docs/magic_s16.md.
//
//   - Each overlay holds a copy of the Healing Herb's sparkle code (MAGIC070,
//     battle_items.cpp and magic_fx_reached.cpp): the sparkle's type dispatch,
//     its update and three phases, its three draws, the pool's alloc and free
//     - instruction for instruction the same but for the addresses of its own
//     pool (0x2C-byte records), its "current sparkle" cell and its .data
//     tables. Written once here, over a table per overlay (kPools).
//   - MAGIC071 and 072 are MAGIC070 whole: the task (a six-entry stack table,
//     then the pool walk) and the spawn, with the effect's kind 1 and 2 where
//     MAGIC070's is 0; the task's other phases are shared bodies (round
//     eight's BattleFx_* and Sparkle_End).
//   - MAGIC073 and 074 put a child task (kind 1, parameter 0x26 / 0x24) on
//     every actor of the target side that is not out; each child tints its
//     actor, sparkles on it, and fades; the parent waits for the children
//     (073 also for every actor to leave state 6).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent). The dispatches through
// .data read their table in place, unchecked, as round seven's
// Sparkle_Dispatch does.
#include "game/magic_s16.h"

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
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr std::uint32_t kSparkleStride = 0x2C;
constexpr std::uint32_t kScratch = 0x903850;       // s16 words +0 .. +0xE (DamageScratch's neighbourhood)
constexpr std::uint32_t kTints = 0x7E0700;         // MoveScript_TintRecords, 12-byte records
constexpr std::uint32_t kEnemyState = 0x93BCD9;    // what 0x4BBCE0 reads: 0x93B961 + 0x128 * 3 (docs/magic_s16.md section 6)
constexpr std::uint32_t kPartyState = 0x802D41;    // a member's +1

// A sparkle's fields (battle_items_callees.h names them for MAGIC070's pool).
constexpr unsigned kFlags = 0, kType = 1, kPhase = 2, kShade = 3, kKind = 4, kTimer = 5, kCount = 6, kLimit = 7;
constexpr unsigned kBaseX = 8, kDriftY = 0xA, kWave = 0xC, kX = 0x20, kY = 0x22, kOwnerOf = 0x28;

using Fn0 = void (__cdecl*)();
using Draw = void (__cdecl*)(unsigned, unsigned);
using AllocFn = unsigned char (__cdecl*)();

// One overlay's copy of the sparkle code: where its pool, its current-sparkle
// cell and its tables are, and its own functions' addresses (the phases its
// update's stack table holds) and ours (what its calls reach).
struct Pool {
    std::uint32_t pool;          // count records of 0x2C
    unsigned count;
    std::uint32_t current;       // unsigned char *: the sparkle being updated
    std::uint32_t offsets;       // 32 x (s16 dx, s16 dy), by the launch row +7 & 0x1F
    std::uint32_t colours;       // rgb rows, row = shade +3 + 4 * kind +4
    std::uint32_t life;          // u8 by kind: the rise length
    std::uint32_t counts;        // u8 by kind: sparkles spawned
    std::uint32_t delays;        // u8 by kind: launch delay per four
    std::uint32_t types;         // the sparkle dispatch's table, read in place by +1
    std::uint32_t launch, rise, fade;   // the update's stack table (their original addresses)
    Fn0 disc;
    Draw rays_g2, rays_g3;
    AllocFn alloc;
    Fn0 free;
};

const Pool kPools[4] = {
    {0x685D98, 0x80, 0x687398, 0x65AE2C, 0x65AECC, 0x65AF18, 0x65AF08, 0x65AF10, 0x65AF20, 0x4B9BC0, 0x4B9D30, 0x4B9DA0,
     Magic071_SparkleDisc, Magic071_SparkleRaysG2, Magic071_SparkleRaysG3, Magic071_SparkleAlloc, Magic071_SparkleFree},
    {0x6873A0, 0x80, 0x6889A0, 0x65AF24, 0x65AFC4, 0x65B010, 0x65B000, 0x65B008, 0x65B018, 0x4BA6F0, 0x4BA860, 0x4BA8D0,
     Magic072_SparkleDisc, Magic072_SparkleRaysG2, Magic072_SparkleRaysG3, Magic072_SparkleAlloc, Magic072_SparkleFree},
    {0x6889A8, 0xA0, 0x68A528, 0x65B01C, 0x65B0BC, 0x65B0DC, 0x65B0D4, 0x65B0D8, 0x65B0E4, 0x4BB440, 0x4BB5B0, 0x4BB620,
     Magic073_SparkleDisc, Magic073_SparkleRaysG2, Magic073_SparkleRaysG3, Magic073_SparkleAlloc, Magic073_SparkleFree},
    {0x68A530, 0xA0, 0x68C0B0, 0x65B0E8, 0x65B188, 0x65B1A8, 0x65B1A0, 0x65B1A4, 0x65B1B0, 0x4BC350, 0x4BC4C0, 0x4BC530,
     Magic074_SparkleDisc, Magic074_SparkleRaysG2, Magic074_SparkleRaysG3, Magic074_SparkleAlloc, Magic074_SparkleFree},
};
const Pool& k071 = kPools[0];
const Pool& k072 = kPools[1];
const Pool& k073 = kPools[2];
const Pool& k074 = kPools[3];

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }
std::int32_t PtrValue(const void* p) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p))); }
unsigned char* Record(const Pool& p, unsigned index) { return Mem(p.pool + index * kSparkleStride); }
unsigned char* Cur(const Pool& p) { return Pointer(p.current); }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Scr(unsigned k) { return Mem(kScratch + k); }
short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `fild dword` then `fstp dword`: the integer as a float.
void PutFloat(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}

// A battle actor's record by index, as the originals compute it: a member
// (0x802D40, stride 0x14C) below 3, else the enemy index - 3 (0x93B960,
// stride 0x128), unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(at::kParty + actor * at::kPartyStride);
    return Mem(at::kEnemies + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}

// ===========================================================================
// The sparkle code, one copy per overlay (MAGIC070's, battle_items.cpp)

// original (e.g. 0x4B9B10): the sparkle's type +1 through the overlay's table,
// a jmp read in place, the type unchecked (the spawns store 0).
void Dispatch(const Pool& p) {
    const unsigned type = Cur(p)[kType];
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(p.types + 4 * type)))))();
}

// original (e.g. 0x4B9B30): the sparkle's phase +2 through a three-entry
// stack table (launch, rise, fade; 3..255 would call through the stack, ours
// aborts); then while it lives (+0 bit 0) and is launched (+2 not 0) its
// disc, and when its sway +0xC & 3 is not 0 its rays: two-point ones of
// radius +6 at Frame_Counter / 2, three-point ones of radius +6 * 1.5 at
// Frame_Counter / 2 + 4 (pushed with stale upper halves; the draws read the
// low words).
void Update(const Pool& p, const char* who) {
    const unsigned phase = Cur(p)[kPhase];
    if (phase >= 3) bof3::Fatal("%s: phase %u, past the three-entry table", who, phase);
    const std::uint32_t table[3] = {p.launch, p.rise, p.fade};
    magic_harness::Phase(table[phase])();
    const unsigned char* c = Cur(p);
    if ((c[kFlags] & 1) == 0 || c[kPhase] == 0) return;
    MH_CALL(p.disc)();
    c = Cur(p);
    if ((c[kWave] & 3) == 0) return;
    MH_CALL(p.rays_g2)(Frame_Counter >> 1, c[kCount]);
    c = Cur(p);
    const unsigned radius = c[kCount];
    MH_CALL(p.rays_g3)((Frame_Counter >> 1) + 4, (radius >> 1) + radius);
}

// original (e.g. 0x4B9BC0): the launch. The delay +5 down; at 0 the owner's
// position (+0x34..+0x3C) and screen point (+0x2E / +0x30) to the sparkle;
// thrown by the offset row +7 & 0x1F (x by dx pushed 0..7 further from 0, y
// up by dy + 0..7), a base x, a drift Rand & 1, a sway phase Rand, a rise
// length (Rand & 6) + life[kind], brightness 0x10, count 0, phase on. The
// sparkle is read again after every call, the x pointer taken before its Rand.
void Launch(const Pool& p) {
    unsigned char* c = Cur(p);
    Bump(c[kTimer], -1);
    c = Cur(p);
    if (c[kTimer] != 0) return;
    SetLong(c + 0x14, Long(Owner() + 0x34));
    SetLong(Cur(p) + 0x18, Long(Owner() + 0x38));
    SetLong(Cur(p) + 0x1C, Long(Owner() + 0x3C));
    SetWord(Cur(p) + kX, Word(Owner() + 0x2E));
    SetWord(Cur(p) + kY, Word(Owner() + 0x30));
    c = Cur(p);
    unsigned char* const x = c + kX;
    if (S16(Mem(p.offsets + (c[kLimit] & 0x1F) * 4)) < 0) {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        SetWord(x, Word(x) + static_cast<std::uint16_t>(Word(Mem(p.offsets + (c2[kLimit] & 0x1F) * 4)) - (r & 7)));
    } else {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        SetWord(x, Word(x) + static_cast<std::uint16_t>((r & 7) + Word(Mem(p.offsets + (c2[kLimit] & 0x1F) * 4))));
    }
    unsigned char* const y = Cur(p) + kY;
    {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        SetWord(y, Word(y) - static_cast<std::uint16_t>((r & 7) + Word(Mem(p.offsets + 2 + (c2[kLimit] & 0x1F) * 4))));
    }
    c = Cur(p);
    SetWord(c + kBaseX, Word(c + kX));
    {
        const int r = MH_CALL(Rand)();
        SetWord(Cur(p) + kDriftY, static_cast<unsigned>(r & 1));
    }
    {
        const int r = MH_CALL(Rand)();
        SetWord(Cur(p) + kWave, static_cast<unsigned>(r));
    }
    {
        const int r = MH_CALL(Rand)();
        c = Cur(p);
        c[kLimit] = static_cast<unsigned char>((r & 6) + Mem(p.life)[c[kKind]]);
    }
    Cur(p)[kTimer] = 0x10;
    Cur(p)[kCount] = 0;
    Bump(Cur(p)[kPhase]);
}

// The sway the rise and the fade share: +0xC up; x = base + sin((+0xC & 0x3F)
// << 6) * 24 >> 12 (the angle kept in scratch +6); y += the drift +0xA.
void Sway(const Pool& p) {
    unsigned char* c = Cur(p);
    SetWord(c + kWave, Word(c + kWave) + 1);
    const int angle = (Cur(p)[kWave] & 0x3F) << 6;
    SetWord(Scr(6), static_cast<unsigned>(angle));
    const int s = MH_CALL(Math_Sin)(angle);
    c = Cur(p);
    SetWord(c + kX, static_cast<unsigned>(Mul12(s, 24)) + Word(c + kBaseX));
    c = Cur(p);
    SetWord(c + kY, Word(c + kY) + Word(c + kDriftY));
}

// original (e.g. 0x4B9D30): the rise. The sway; the count +6 up; at the rise
// length +7 the phase on.
void Rise(const Pool& p) {
    Sway(p);
    Bump(Cur(p)[kCount]);
    unsigned char* const c = Cur(p);
    if (c[kCount] == c[kLimit]) Bump(c[kPhase]);
}

// original (e.g. 0x4B9DA0): the fade. The sway; the count down on every
// fourth frame; the brightness +5 down, and at 0 the owner's sparkle count
// +0xB down and the sparkle freed (a tail jmp in the original).
void Fade(const Pool& p) {
    Sway(p);
    if ((Frame_Counter & 3) == 0) Bump(Cur(p)[kCount], -1);
    Bump(Cur(p)[kTimer], -1);
    if (Cur(p)[kTimer] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(p.free)();
}

// The draws' common start: the draw mode at Gfx_PacketNext and a sorted
// commit at the sparkle's position; then each primitive's commit.
void DrawMode(const Pool& p) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    const unsigned char* const c = Cur(p);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(c + 0x14)), static_cast<unsigned long>(Long(c + 0x18)), 2, 0xC);
}
void Commit(const Pool& p) {
    const unsigned char* const c = Cur(p);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(c + 0x14)), static_cast<unsigned long>(Long(c + 0x18)), 2, 0x34);
}
// Scratch +0 / +2 the sparkle's screen point (read before the draw mode).
void PointToScratch(const Pool& p) {
    const unsigned char* const c = Cur(p);
    SetWord(Scr(0), Word(c + kX));
    SetWord(Scr(2), Word(c + kY));
}
// Scratch +8 / +0xA / +0xC: the brightness +5 times 6.
void RayShade(const Pool& p) {
    const unsigned char* const c = Cur(p);
    SetWord(Scr(8), c[kTimer] * 6u);
    SetWord(Scr(0xA), c[kTimer] * 6u);
    SetWord(Scr(0xC), c[kTimer] * 6u);
}

// original (e.g. 0x4B9E30): four gouraud lines from the sparkle's point,
// `radius` long, at ((start + 8k) & 0x1F) << 7, bright (+5 * 6) at the centre,
// dark at the tip. Both arguments are read as their low words.
void RaysG2(const Pool& p, unsigned start, unsigned radius) {
    PointToScratch(p);
    DrawMode(p);
    RayShade(p);
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int r = static_cast<int>(radius & 0xFFFF);
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloat(q + 8, S16(Scr(0)));
        SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(q + 0xC, S16(Scr(2)));
        const int co = MH_CALL(Math_Cos)(S16(Scr(6)));
        PutFloat(q + 0x18, Mul12(co, r) + S16(Scr(0)));
        const int si = MH_CALL(Math_Sin)(S16(Scr(6)));
        PutFloat(q + 0x1C, Mul12(si, r) + S16(Scr(2)));
        q[4] = Scr(8)[0];
        q[5] = Scr(0xA)[0];
        q[6] = Scr(0xC)[0];
        q[0x14] = 1;
        q[0x15] = 1;
        q[0x16] = 1;
        Commit(p);
        n += 8;
    } while (n < end);
}

// original (e.g. 0x4B9FC0): the same four rays as three-point lines - dark at
// the centre, bright (+5 * 6) at half the radius, dark at the tip.
void RaysG3(const Pool& p, unsigned start, unsigned radius) {
    PointToScratch(p);
    DrawMode(p);
    RayShade(p);
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int full = static_cast<int>(radius & 0xFFFF);
    const int half = full >> 1;
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloat(q + 8, S16(Scr(0)));
        SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(q + 0xC, S16(Scr(2)));
        const int co = MH_CALL(Math_Cos)(S16(Scr(6)));
        PutFloat(q + 0x18, Mul12(co, half) + S16(Scr(0)));
        const int si = MH_CALL(Math_Sin)(S16(Scr(6)));
        PutFloat(q + 0x1C, Mul12(si, half) + S16(Scr(2)));
        const int co2 = MH_CALL(Math_Cos)(S16(Scr(6)));
        PutFloat(q + 0x28, Mul12(co2, full) + S16(Scr(0)));
        const int si2 = MH_CALL(Math_Sin)(S16(Scr(6)));
        const int y2 = Mul12(si2, full) + S16(Scr(2));
        q[4] = 1;
        q[5] = 1;
        q[6] = 1;
        PutFloat(q + 0x2C, y2);
        q[0x14] = Scr(8)[0];
        q[0x15] = Scr(0xA)[0];
        q[0x16] = Scr(0xC)[0];
        q[0x24] = 1;
        q[0x25] = 1;
        q[0x26] = 1;
        Commit(p);
        n += 8;
    } while (n < end);
}

// original (e.g. 0x4BA1B0): the sparkle's body - eight gouraud triangles
// round its point, radius +6 + (Rand & 3), the centre the colour row (+3 +
// 4 * +4) times the brightness +5, the rim +5 grey.
void Disc(const Pool& p) {
    DrawMode(p);
    const int r = MH_CALL(Rand)();
    const unsigned char* const c = Cur(p);
    SetWord(Scr(4), static_cast<unsigned>((r & 3) + c[kCount]));
    SetWord(Scr(0), Word(c + kX));
    SetWord(Scr(2), Word(c + kY));
    const unsigned char* const rgb = Mem(p.colours + (c[kShade] + c[kKind] * 4u) * 3u);
    SetWord(Scr(8), rgb[0] * static_cast<unsigned>(c[kTimer]));
    SetWord(Scr(0xA), rgb[1] * static_cast<unsigned>(c[kTimer]));
    SetWord(Scr(0xC), rgb[2] * static_cast<unsigned>(c[kTimer]));
    SetWord(Scr(0xE), c[kTimer]);
    int a = 0;
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloat(q + 8, S16(Scr(0)));
        PutFloat(q + 0xC, S16(Scr(2)));
        int s = MH_CALL(Math_Sin)(a);
        PutFloat(q + 0x18, Mul12(s, S16(Scr(4))) + S16(Scr(0)));
        int co = MH_CALL(Math_Cos)(a);
        PutFloat(q + 0x1C, Mul12(co, S16(Scr(4))) + S16(Scr(2)));
        a += 0x200;
        s = MH_CALL(Math_Sin)(a);
        PutFloat(q + 0x28, Mul12(s, S16(Scr(4))) + S16(Scr(0)));
        co = MH_CALL(Math_Cos)(a);
        PutFloat(q + 0x2C, Mul12(co, S16(Scr(4))) + S16(Scr(2)));
        q[4] = Scr(8)[0];
        q[5] = Scr(0xA)[0];
        q[6] = Scr(0xC)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) q[k] = Scr(0xE)[0];
        Commit(p);
    } while (a < 0x1000);
}

// original (e.g. 0x4BA3E0): the first record of the pool without bit 0,
// marked and its index answered; 0xFF when all are in use. Only al is the
// original's answer; every caller keeps al.
unsigned char Alloc(const Pool& p) {
    unsigned char i = 0;
    do {
        unsigned char* const s = Record(p, i);
        if ((s[kFlags] & 1) == 0) {
            s[kFlags] = static_cast<unsigned char>(s[kFlags] | 1);
            return i;
        }
        ++i;
    } while (i < p.count);
    return 0xFF;
}

// original (e.g. 0x4BA430): bytes +0..+4 of the sparkle cleared, the pointer
// read again for each.
void Free(const Pool& p) {
    for (unsigned k = 0; k < 5; ++k) Cur(p)[k] = 0;
}

// ===========================================================================
// The tasks

// The pool walk that follows each task's phase: every record in use (bit 0)
// becomes the current sparkle, its +0x28 the owner 0x93B940 for the dispatch,
// and the owner is put back after each (saved once, after the phase).
void Walk(const Pool& p, Fn0 dispatch) {
    const std::int32_t saved = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < p.count; ++i) {
        unsigned char* const s = Record(p, i);
        if ((s[kFlags] & 1) == 0) continue;
        SetLong(Mem(p.current), PtrValue(s));
        SetLong(Mem(at::kOwner), Long(s + kOwnerOf));
        MH_CALL(dispatch)();
        SetLong(Mem(at::kOwner), saved);
    }
}

// The phase +1 through a stack table of `n` (unchecked by the original).
void Step(const std::uint32_t* table, unsigned n, const char* who) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(table[phase])();
}
// A child's phase +2 through its six-entry stack table (unchecked by the
// original).
void Step2(const std::uint32_t* table, const char* who) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 6) bof3::Fatal("%s: phase %u, past the six-entry table", who, phase);
    magic_harness::Phase(table[phase])();
}

// The sparkles of one spawn: for n from 0 while n < counts[kind], a record
// from the pool (0xFF skips one) owned by Sprite_Current, type and phase 0,
// shade Rand & 3, the kind, offset row n, delay delays[kind] * (n >> 2) + 1
// (a byte), counted in Sprite_Current +0xB. `kind` reads the kind again where
// the original does (the task's own +4, or the owner's).
template <typename Kind> void MakeSparkles(const Pool& p, Kind kind) {
    unsigned char n = 0;
    if (Mem(p.counts)[kind()] == 0) return;
    do {
        const unsigned char a = MH_CALL(p.alloc)();
        if (a != 0xFF) {
            unsigned char* const s = Record(p, a);
            SetLong(s + kOwnerOf, PtrValue(Sprite_Current));
            s[kType] = 0;
            s[kPhase] = 0;
            const int r = MH_CALL(Rand)();
            s[kShade] = static_cast<unsigned char>(r & 3);
            const unsigned k = kind();
            s[kKind] = static_cast<unsigned char>(k);
            s[kLimit] = n;
            s[kTimer] = static_cast<unsigned char>(Mem(p.delays)[k] * static_cast<unsigned>(n >> 2) + 1);
            Bump(Sprite_Current[0xB]);
        }
        ++n;
    } while (n < Mem(p.counts)[kind()]);
}

void ClearPool(const Pool& p) {
    for (unsigned i = 0; i < p.count; ++i) {
        unsigned char* const s = Record(p, i);
        s[0] = 0;
        s[1] = 0;
        s[2] = 0;
    }
}

// original 0x4B99C0 / 0x4BA4F0 (MAGIC070's Sparkle_Spawn with its kind): the
// pool's +0..+2 cleared; the task's kind +4; the source sprite's (0x904B4C,
// read once) +8 and position to the task; its screen point; +0xB 0, +9 = 8,
// +0xA 0, the phase on; the sparkles (the kind the task's +4, read again);
// sound 0x100.
void Spawn(const Pool& p, unsigned char kind) {
    ClearPool(p);
    Sprite_Current[4] = kind;
    const unsigned char* const src = Pointer(at::kSource);
    Sprite_Current[8] = src[8];
    SetLong(Sprite_Current + 0x34, Long(src + 0x34));
    SetLong(Sprite_Current + 0x38, Long(src + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(src + 0x3C));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
    MakeSparkles(p, [] { return static_cast<unsigned>(Sprite_Current[4]); });
    MH_CALL(Sound_PlayEffect)(0x100);
}

// original 0x4BB000 / 0x4BBDD0: the pool's +0..+2 cleared; the task's kind
// +4; +0xB 0; the phase on. Then for every actor of the target's side - the
// enemies 3..10 when the target 0x904B44 has bit 0x40, else the party 0..2 -
// that is not out, a child task (kind 1, `parameter`): its owner the task,
// phase 0, its actor +4, its delay +9 (1, then 0x1C more for each child), and
// the task's +0xB counts it. Sound 0x100 by id.
void SpawnOnActors(const Pool& p, unsigned char kind, unsigned parameter) {
    ClearPool(p);
    Sprite_Current[4] = kind;
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
    const bool enemies = (Mem(at::kTarget)[0] & 0x40) != 0;
    unsigned char delay = 1;
    unsigned char i = enemies ? 3 : 0;
    do {
        if (MH_CALL(Battle_ActorIsOut)(i) == 0) {
            const unsigned char slot = MH_CALL(BattleTask_Create)(1, parameter);
            unsigned char* const t = Mem(at::kTasks + slot * at::kTaskStride);
            unsigned char* const sc = Sprite_Current;
            SetLong(t + 0x80, PtrValue(sc));
            t[1] = 0;
            t[4] = i;
            t[9] = delay;
            delay = static_cast<unsigned char>(delay + 0x1C);
            Bump(sc[0xB]);
        }
        ++i;
    } while (enemies ? static_cast<unsigned char>(i - 3) < 8 : i < 3);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4BB210 / 0x4BBFB0: the child's start. Its delay +9 down; at 0 it
// takes its actor's (+4) byte +8 and position, the actor's screen point
// (BattleActor_UpdateScreenXY), +0xB 0, +9 = 8, +0xA 0, its phase +2 on; then
// the sparkles, their kind the parent's (the owner 0x93B940) +4.
void ActorStart(const Pool& p) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    const unsigned char* const rec = ActorRecord(sc[4]);
    sc[8] = rec[8];
    SetLong(Sprite_Current + 0x34, Long(rec + 0x34));
    SetLong(Sprite_Current + 0x38, Long(rec + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(rec + 0x3C));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
    MakeSparkles(p, [] { return static_cast<unsigned>(Owner()[4]); });
}

}  // namespace

#define MS16_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC071 (row 45) and MAGIC072 (row 113): MAGIC070 whole

namespace {
// The task's six phases: its spawn, then the shared tint, brighten and
// wait (round eight's BattleFx_*), MAGIC070's Sparkle_End, and the finish.
constexpr std::uint32_t kTask071[6] = {0x4B99C0, bof3::addr::BattleFx_TintActor, bof3::addr::BattleFx_Brighten,
                                       bof3::addr::BattleFx_WaitStep4, bof3::addr::Sparkle_End, bof3::addr::BattleFx_Finish};
constexpr std::uint32_t kTask072[6] = {0x4BA4F0, bof3::addr::BattleFx_TintActor, bof3::addr::BattleFx_Brighten,
                                       bof3::addr::BattleFx_WaitStep4, bof3::addr::Sparkle_End, bof3::addr::BattleFx_Finish};
}  // namespace

// original 0x4B9930 (Magic_Rows row 45): the phase +1 through its six-entry
// stack table, then the pool walk.
MS16_EXPORT void __cdecl Magic071_Task(void) {
    Step(kTask071, 6, "Magic071_Task");
    Walk(k071, Magic071_SparkleDispatch);
}
// original 0x4B99C0: the spawn, kind 1.
MS16_EXPORT void __cdecl Magic071_Spawn(void) { Spawn(k071, 1); }

// original 0x4BA460 (Magic_Rows row 113): as Magic071_Task.
MS16_EXPORT void __cdecl Magic072_Task(void) {
    Step(kTask072, 6, "Magic072_Task");
    Walk(k072, Magic072_SparkleDispatch);
}
// original 0x4BA4F0: the spawn, kind 2.
MS16_EXPORT void __cdecl Magic072_Spawn(void) { Spawn(k072, 2); }

// ===========================================================================
// MAGIC073 (row 47) and MAGIC074 (row 114): a child on every actor

namespace {
constexpr std::uint32_t kTask073[2] = {0x4BB000, 0x4BB170};
constexpr std::uint32_t kTask074[2] = {0x4BBDD0, 0x4E5200};   // 0x4E5200: MAGIC131's wait-and-end (group S30's)
// The child's six phases: its start, the actor's tint, 0x4F4DA0 (MAGIC213's
// brighten, group C1's), the wait for four sparkles, the fade, the end.
constexpr std::uint32_t kActor073[6] = {0x4BB210, 0x4BC110, 0x4F4DA0, 0x4BB370, 0x4BC1A0, 0x4BC260};
constexpr std::uint32_t kActor074[6] = {0x4BBFB0, 0x4BC110, 0x4F4DA0, 0x4BB370, 0x4BC1A0, 0x4BC260};
constexpr std::uint32_t kActorPhases073 = 0x65B0E0;   // the child's dispatch table, read in place by +1
constexpr std::uint32_t kActorPhases074 = 0x65B1AC;

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}
}  // namespace

// original 0x4BAF90 (Magic_Rows row 47): the phase +1 through a two-entry
// stack table (the spawn, the wait), then the pool walk (160 records).
MS16_EXPORT void __cdecl Magic073_Task(void) {
    Step(kTask073, 2, "Magic073_Task");
    Walk(k073, Magic073_SparkleDispatch);
}
// original 0x4BB000: a child (kind 1, parameter 0x26) per actor, kind 0.
MS16_EXPORT void __cdecl Magic073_Spawn(void) { SpawnOnActors(k073, 0, 0x26); }

// original 0x4BB170: the parent's wait. While children live (+0xB) or any
// actor reacts (Magic073_CountReacting), nothing; then the effect-done flag
// (0x904AA8 bit 2) and the task freed (a tail jmp).
MS16_EXPORT void __cdecl Magic073_Wait(void) {
    if (Sprite_Current[0xB] != 0) return;
    if (MH_CALL(Magic073_CountReacting)() != 0) return;
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BB1A0: the child (kind 1, parameter 0x26): a jmp through
// 0x65B0E0 by +1, read in place, unchecked (+1 is 0 as the spawn sets it;
// entry 1 is the sparkle table's).
MS16_EXPORT void __cdecl Magic073_ActorDispatch(void) { CallCell(kActorPhases073, Sprite_Current[1]); }

// original 0x4BB1C0: the child's phase +2 through a six-entry stack table.
MS16_EXPORT void __cdecl Magic073_ActorTask(void) { Step2(kActor073, "Magic073_ActorTask"); }

// original 0x4BB210: the child's start (sparkles from 0x6889A8's pool).
MS16_EXPORT void __cdecl Magic073_ActorStart(void) { ActorStart(k073); }

// original 0x4BB370 (shared: reached from MAGIC073, 074, 168, 169): with the
// task's +0xB (its live sparkles) 4 or fewer, +9 = 8 and the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_WaitStep4(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xB] > 4) return;
    sc[9] = 8;
    Bump(Sprite_Current[2]);
}

// original 0x4BBCE0: how many actors are in the reaction state 6 and not out:
// the enemies (Battle_ActorIsOut(i + 3), the state byte at 0x93BCD9 +
// 0x128 i - three records past actor i + 3's own, as the original indexes
// it) for i 0..7, then the members (+1 of 0x802D40 + 0x14C i) for i 0..2.
MS16_EXPORT unsigned char __cdecl Magic073_CountReacting(void) {
    unsigned char n = 0;
    for (unsigned char i = 0; i < 8; ++i)
        if (MH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i + 3)) == 0 && Mem(kEnemyState + i * at::kEnemyStride)[0] == 6)
            ++n;
    for (unsigned char i = 0; i < 3; ++i)
        if (MH_CALL(Battle_ActorIsOut)(i) == 0 && Mem(kPartyState + i * at::kPartyStride)[0] == 6) ++n;
    return n;
}

// original 0x4BBD60 (Magic_Rows row 114): the phase +1 through a two-entry
// stack table (the spawn, 0x4E5200), then the pool walk (160 records).
MS16_EXPORT void __cdecl Magic074_Task(void) {
    Step(kTask074, 2, "Magic074_Task");
    Walk(k074, Magic074_SparkleDispatch);
}
// original 0x4BBDD0: a child (kind 1, parameter 0x24) per actor, kind 1.
MS16_EXPORT void __cdecl Magic074_Spawn(void) { SpawnOnActors(k074, 1, 0x24); }
// original 0x4BBF40: the child (parameter 0x24): a jmp through 0x65B1AC by +1.
MS16_EXPORT void __cdecl Magic074_ActorDispatch(void) { CallCell(kActorPhases074, Sprite_Current[1]); }
// original 0x4BBF60: the child's phase +2 through a six-entry stack table.
MS16_EXPORT void __cdecl Magic074_ActorTask(void) { Step2(kActor074, "Magic074_ActorTask"); }
// original 0x4BBFB0: the child's start (sparkles from 0x68A530's pool).
MS16_EXPORT void __cdecl Magic074_ActorStart(void) { ActorStart(k074); }

// original 0x4BC110 (shared by MAGIC073 and 074): the delay +9 down; at 0 the
// actor's (+4) tints released, a tint (0, 0, 0, 1) set on it and its record
// kept in +0xA, +9 = 8, the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_Tint(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const rec = ActorRecord(sc[4]);
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(rec);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(rec, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[2]);
}

// original 0x4BC1A0 (shared by MAGIC073 and 074): the tint record +0xA's
// colour (+2..+4 of the 12-byte MoveScript_TintRecords, the index unbounded)
// down one; +9 down, and at 0 the actor's tints released, the actor flashed
// and the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_Untint(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const rec = ActorRecord(sc[4]);
    for (unsigned k = 2; k <= 4; ++k) Bump(Mem(kTints + sc[0xA] * 12u + k)[0], -1);
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(rec);
    MH_CALL(BattleActor_Flash)(Sprite_Current[4]);
    Bump(Sprite_Current[2]);
}

// original 0x4BC260 (shared by MAGIC073 and 074): once the child's sparkles
// are gone (+0xB 0), the parent's count +0xB down, the actor's flag 0x40, and
// the child freed (a tail jmp).
MS16_EXPORT void __cdecl ActorFx_End(void) {
    if (Sprite_Current[0xB] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(Battle_SetTargetFlag40)(Sprite_Current[4]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// The sparkle code's named copies

#define MS16_SPARKLES(N, P)                                                                                        \
    MS16_EXPORT void __cdecl Magic##N##_SparkleDispatch(void) { Dispatch(P); }                                     \
    MS16_EXPORT void __cdecl Magic##N##_SparkleUpdate(void) { Update(P, "Magic" #N "_SparkleUpdate"); }            \
    MS16_EXPORT void __cdecl Magic##N##_SparkleLaunch(void) { Launch(P); }                                         \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRise(void) { Rise(P); }                                             \
    MS16_EXPORT void __cdecl Magic##N##_SparkleFade(void) { Fade(P); }                                             \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRaysG2(unsigned start, unsigned radius) { RaysG2(P, start, radius); } \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRaysG3(unsigned start, unsigned radius) { RaysG3(P, start, radius); } \
    MS16_EXPORT void __cdecl Magic##N##_SparkleDisc(void) { Disc(P); }                                             \
    MS16_EXPORT unsigned char __cdecl Magic##N##_SparkleAlloc(void) { return Alloc(P); }                           \
    MS16_EXPORT void __cdecl Magic##N##_SparkleFree(void) { Free(P); }

MS16_SPARKLES(071, k071)
MS16_SPARKLES(072, k072)
MS16_SPARKLES(073, k073)
MS16_SPARKLES(074, k074)
#undef MS16_SPARKLES

void MagicS16_Inject() {
    if (bof3::WantsShadow("magic_s16")) magic_s16::SelfTest();
    BOF3_INJECT(Magic071_Task);
    BOF3_INJECT(Magic071_Spawn);
    BOF3_INJECT(Magic071_SparkleDispatch);
    BOF3_INJECT(Magic071_SparkleUpdate);
    BOF3_INJECT(Magic071_SparkleLaunch);
    BOF3_INJECT(Magic071_SparkleRise);
    BOF3_INJECT(Magic071_SparkleFade);
    BOF3_INJECT(Magic071_SparkleRaysG2);
    BOF3_INJECT(Magic071_SparkleRaysG3);
    BOF3_INJECT(Magic071_SparkleDisc);
    BOF3_INJECT(Magic071_SparkleAlloc);
    BOF3_INJECT(Magic071_SparkleFree);
    BOF3_INJECT(Magic072_Task);
    BOF3_INJECT(Magic072_Spawn);
    BOF3_INJECT(Magic072_SparkleDispatch);
    BOF3_INJECT(Magic072_SparkleUpdate);
    BOF3_INJECT(Magic072_SparkleLaunch);
    BOF3_INJECT(Magic072_SparkleRise);
    BOF3_INJECT(Magic072_SparkleFade);
    BOF3_INJECT(Magic072_SparkleRaysG2);
    BOF3_INJECT(Magic072_SparkleRaysG3);
    BOF3_INJECT(Magic072_SparkleDisc);
    BOF3_INJECT(Magic072_SparkleAlloc);
    BOF3_INJECT(Magic072_SparkleFree);
    BOF3_INJECT(Magic073_Task);
    BOF3_INJECT(Magic073_Spawn);
    BOF3_INJECT(Magic073_Wait);
    BOF3_INJECT(Magic073_ActorDispatch);
    BOF3_INJECT(Magic073_ActorTask);
    BOF3_INJECT(Magic073_ActorStart);
    BOF3_INJECT(ActorFx_WaitStep4);
    BOF3_INJECT(Magic073_SparkleDispatch);
    BOF3_INJECT(Magic073_SparkleUpdate);
    BOF3_INJECT(Magic073_SparkleLaunch);
    BOF3_INJECT(Magic073_SparkleRise);
    BOF3_INJECT(Magic073_SparkleFade);
    BOF3_INJECT(Magic073_SparkleRaysG2);
    BOF3_INJECT(Magic073_SparkleRaysG3);
    BOF3_INJECT(Magic073_SparkleDisc);
    BOF3_INJECT(Magic073_SparkleAlloc);
    BOF3_INJECT(Magic073_SparkleFree);
    BOF3_INJECT(Magic073_CountReacting);
    BOF3_INJECT(Magic074_Task);
    BOF3_INJECT(Magic074_Spawn);
    BOF3_INJECT(Magic074_ActorDispatch);
    BOF3_INJECT(Magic074_ActorTask);
    BOF3_INJECT(Magic074_ActorStart);
    BOF3_INJECT(ActorFx_Tint);
    BOF3_INJECT(ActorFx_Untint);
    BOF3_INJECT(ActorFx_End);
    BOF3_INJECT(Magic074_SparkleDispatch);
    BOF3_INJECT(Magic074_SparkleUpdate);
    BOF3_INJECT(Magic074_SparkleLaunch);
    BOF3_INJECT(Magic074_SparkleRise);
    BOF3_INJECT(Magic074_SparkleFade);
    BOF3_INJECT(Magic074_SparkleRaysG2);
    BOF3_INJECT(Magic074_SparkleRaysG3);
    BOF3_INJECT(Magic074_SparkleDisc);
    BOF3_INJECT(Magic074_SparkleAlloc);
    BOF3_INJECT(Magic074_SparkleFree);
}
