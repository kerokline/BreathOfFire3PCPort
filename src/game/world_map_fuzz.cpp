// BOF3X_SHADOW=world_map: a differential fuzz of the group's seven functions
// against byte-copies of Capcom's, once at start-up (docs/world-map.md section
// 7.4). Every call out of a copy is re-aimed at a recording stand-in - the
// calls among the seven included, so each function is tested alone - and ours
// is put on the same stand-ins through world_map::g. The log is the
// vertex-block harness's (d3d_fuzz.h; no device is needed here). Per round:
// the state a function reads, random with its boundaries seeded; Capcom's
// copy, then ours from the same state; the calls out with their arguments,
// the result and every region either could write compared.
//
// The stand-ins write what the real callee writes where the caller reads it
// again (Gfx_CommitPrim advances Gfx_PacketNext, the setters write their code
// and z, Gte_RotMatrix fills the matrix, Gte_RotTransPers4 the corners), and
// now and then change something the caller reads after the call - the
// packet cursor, Gfx_BufferIndex, the flag words, the button words, the
// party-set byte, the text offset, the vertex scratch - so a value read on
// the wrong side of a call shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/world_map_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace world_map {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U GetWord(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
U Pick(std::initializer_list<U> seeds) { return seeds.begin()[Next() % seeds.size()]; }

// --- addresses -------------------------------------------------------------------
const U kSpriteCurrent = at::Sprite_CurrentAt();     // 0x937F88, the pointer
const U kPacketNext = at::Gfx_PacketNextAt();         // 0x7E0670
const U kPassFlags = at::Draw_PassFlagsAt();          // 0x7E0918
const U kFlags = at::Field_ScriptFlagsAt();           // 0x9039A2
const U kFlags2 = at::Field_ScriptFlags2At();         // 0x905BA4
const U kAngles = at::Camera_AnglesAt();              // 0x929EC8, three words
const U kScratch = at::Prim_VertexScratchAt();        // 0x9037A0, 32 bytes
const U kOrigin = at::MapView_OriginAt();             // 0x7E0688, two words
const U kBufferIndex = at::Gfx_BufferIndexAt();       // 0x905B89
constexpr U kRowTailsBytes = kRows * kRowStride;      // 0x8022C4 .. the 0x38 records

// Buffers of the fuzz's own: the packet pool the draws write into, the map
// task's object.
unsigned char g_packets[0x200];
unsigned char g_object[0x40];

// --- regions ---------------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kMaxState = 0x1400;
struct State {
    unsigned char bytes[kMaxState];
};

U RegionBytes(const Region* r, int n) {
    U total = 0;
    for (int i = 0; i < n; ++i) total += r[i].size;
    return total;
}
void Capture(const Region* r, int n, State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(s.bytes + at, At(r[i].at), r[i].size);
        at += r[i].size;
    }
}
void Restore(const Region* r, int n, const State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(At(r[i].at), s.bytes + at, r[i].size);
        at += r[i].size;
    }
}
bool FirstDifference(const Region* r, int n, const State& a, const State& b, U* where) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        for (U k = 0; k < r[i].size; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) {
                *where = r[i].at + k;
                return true;
            }
        at += r[i].size;
    }
    return false;
}

// Every region any test touches, saved once and put back at the end.
Region g_all[] = {
    {kSpriteCurrent, 4}, {kPacketNext, 4},   {kPassFlags, 1},    {kFlags, 2},        {kFlags2, 2},
    {kAngles, 6},        {kScratch, 0x20},   {kOrigin, 4},       {kBufferIndex, 1},  {kButtonMap0, 4},
    {kButtonMap6, 4},    {kMapMode, 1},      {kPartySet, 1},     {kLeaderCellZ, 8},  {kAreaTextOffset, 4},
    {kRowTails - 4, kRowTailsBytes},
};

// --- the stand-ins -----------------------------------------------------------------

U g_round;
U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A disturbance of what the frame reads after its calls, one round in four:
// a flag word, the party-set byte, a button word, the text offset.
void DisturbFrame(U salt) {
    const U h = Mix(salt ^ 0x5BD1E995u);
    if (h % 4) return;
    switch ((h >> 2) % 7) {
    case 0: PutWord(At(kFlags2), GetWord(At(kFlags2)) ^ 0x1000); break;
    case 1: PutWord(At(kFlags), GetWord(At(kFlags)) ^ 0x4000); break;
    case 2: At(kPartySet)[0] = static_cast<unsigned char>(h >> 8); break;
    case 3: PutLong(At(kButtonMap0), h >> 8); break;
    case 4: PutLong(At(kButtonMap6), h >> 8); break;
    case 5: PutLong(At(kAreaTextOffset), h >> 8); break;
    default: PutWord(At(kLeaderCellZ + 2), h >> 8); PutWord(At(kLeaderCellZ + 6), h >> 12); break;
    }
}

void __cdecl StubState0() {
    Record(1);
    const U h = Mix(1);
    if (h % 3 == 0) At(kMapMode)[0] = static_cast<unsigned char>(h >> 8);   // read by no caller after; harmless
}
void __cdecl StubDrawFrame(U x, U y) { Record(2, x, y & 0xFFFF); }
void __cdecl StubSetDrawMode(unsigned char* prim, U dfe, U dtd, U tpage, U tw) {
    Record(3, Addr(prim), dfe, dtd, tpage, tw);
    PutLong(prim + 4, 0xE8000000u | (tpage & 0xFFFF));
    PutLong(prim + 8, tw);
}
// Gfx_CommitPrim advances the pool when there is room, which is always here;
// now and then it also moves the cursor on by 8 (read again by the sprite
// draw), or flips Gfx_BufferIndex, or disturbs what the frame reads later.
void __cdecl StubCommit(U slot, U size) {
    Record(4, slot & 0xFF, size & 0xFF);
    U next = GetLong(At(kPacketNext)) + (size & 0xFF);
    const U h = Mix(4);
    if (h % 5 == 0) next += 8;
    PutLong(At(kPacketNext), next);
    if (h % 7 == 0) At(kBufferIndex)[0] ^= 1;
    DisturbFrame(40);
}
void __cdecl StubDrawSprite(U x, U y, U index) {
    Record(5, x & 0xFFFF, y & 0xFFFF, index & 0xFF);
    DisturbFrame(50);
}
// AreaMap_ByteAt: the cell byte with stale upper bits (the caller reads al);
// the frame reads Field_ScriptFlags2 after it.
U __cdecl StubByteAt(U x, U y) {
    Record(6, x & 0xFFFF, y & 0xFFFF);
    const U h = Mix(6);
    U al;
    switch ((h >> 4) % 10) {
    case 0: al = 0xA0; break;
    case 1: al = 0xA1; break;
    case 2: al = 0xAE; break;
    case 3: al = 0x9F; break;
    case 4: al = 0xA2; break;
    case 5: al = 0xAF; break;
    case 6: al = 0; break;
    default: al = h & 0xFF; break;
    }
    DisturbFrame(60);
    return (h & 0xFFFFFF00u) | al;
}
U __cdecl StubCellEvent(U x, U y) {
    Record(7, x & 0xFFFF, y & 0xFFFF);
    const U h = Mix(7);
    DisturbFrame(70);
    return (h & 0xFFFFFF00u) | ((h >> 8) % 3 == 0 ? (h >> 12) & 0xFF : 0);
}
void __cdecl StubDrawNeedle(U x, U y) { Record(8, x & 0xFFFF, y & 0xFFFF); }
void __cdecl StubSetSprt(unsigned char* prim) {
    Record(9, Addr(prim));
    prim[7] = 0x64;
    PutLong(prim + 0x10, 0x3C23D70A);
}
void __cdecl StubSetSemiTrans(unsigned char* prim, U abe) {
    Record(10, Addr(prim), abe);
    if (abe & 1) prim[7] |= 2;
    else prim[7] &= 0xFD;
}
const unsigned char* __cdecl StubTextDrawAt(U x, U y, U colour, U count, const unsigned char* text) {
    Record(11, x, y, colour, count, Addr(text));
    return text + Mix(11) % 8;
}
void __cdecl StubPushMatrix() { Record(12); }
// Gte_RotMatrix fills the matrix (the caller zeroes its translation after,
// and hands it on: the two Set stand-ins record its contents).
short* __cdecl StubRotMatrix(const short* angles, short* matrix) {
    Record(13, Addr(angles), GetWord(reinterpret_cast<const unsigned char*>(angles)),
           GetWord(reinterpret_cast<const unsigned char*>(angles) + 2), GetWord(reinterpret_cast<const unsigned char*>(angles) + 4));
    auto* m = reinterpret_cast<unsigned char*>(matrix);
    for (U i = 0; i < 0x20; i += 4) PutLong(m + i, Mix(130 + i));
    return matrix;
}
void __cdecl StubSetRotMatrix(const unsigned long* matrix) {
    const auto* m = reinterpret_cast<const unsigned char*>(matrix);
    Record(14, GetLong(m), GetLong(m + 4), GetLong(m + 8), GetLong(m + 12), GetLong(m + 16));
}
void __cdecl StubSetTransMatrix(const unsigned long* matrix) {
    const auto* m = reinterpret_cast<const unsigned char*>(matrix);
    Record(15, GetLong(m + 0x14), GetLong(m + 0x18), GetLong(m + 0x1C));
}
// Gpu_SetPolyG4 writes the code and the z's; one round in four it also
// scrambles the vertex scratch, which the caller must have filled before it.
unsigned char* __cdecl StubSetPolyG4(unsigned char* prim) {
    Record(16, Addr(prim));
    prim[7] = 0x38;
    for (U z = 0x10; z <= 0x40; z += 0x10) PutLong(prim + z, 0x3C23D70A);
    const U h = Mix(16);
    if (h % 4 == 0)
        for (U i = 0; i < 0x20; i += 4) PutLong(At(kScratch) + i, Mix(160 + i));
    return prim;
}
const U kFloats[] = {
    0x00000000, 0x80000000, 0x3F800000, 0x3F000000, 0x43200000, 0xC2000000, 0x3C23D70A,  // 0 -0 1 .5 160 -32 .01
    0x7149F2CA, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000,  // 1e30 max min-normal denormals inf
    0x7FC00000, 0x7FA00000, 0xFF800001, 0x3F7D70A4, 0x4B7FFFFF, 0x3EAAAAAB, 0xBF800000,  // qNaN sNaN sNaN .99 2^24-1 1/3 -1
    0x42C40000, 0x42EC0000, 0x42C48000, 0x46FFFE00, 0xC6FFFE00, 0x4B800000, 0x33800000,  // 98 118 98.25 32767 -32767 2^24 2^-24
};
// A float for a stand-in to write: from the seeds, or any bits, or a plain
// value - keyed on the round and the log position (Mix), never on the shared
// stream, so both sides are handed the same one.
U MixFloat(U salt) {
    const U h = Mix(salt);
    switch (h % 4) {
    case 0: return kFloats[(h >> 2) % (sizeof kFloats / sizeof kFloats[0])];
    case 1: return Mix(salt ^ 0xA5A5A5A5u);
    default: return 0x40000000u + (h >> 4) % 0x08000000u;   // 2 .. 4096 or so
    }
}
// Gte_RotTransPers4 with the ten arguments the original pushes: records the
// vectors' words and the corner pointers, writes float corners (the seeds'
// values, so the subtraction after it is exercised at the edges), and the
// depth and flag through the two locals.
long __cdecl StubRotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0,
                               float* s1, float* s2, float* s3, long* p, long* flag) {
    const auto w = [](const short* v, U i) { return GetWord(reinterpret_cast<const unsigned char*>(v) + i * 2); };
    Record(17, Addr(v0), w(v0, 0), w(v0, 1), w(v0, 2), Addr(v1), w(v1, 0) | (w(v1, 1) << 16), w(v1, 2));
    Record(18, Addr(v2), w(v2, 0), w(v2, 1), w(v2, 2), Addr(v3), w(v3, 0) | (w(v3, 1) << 16), w(v3, 2));
    Record(19, Addr(s0), Addr(s1), Addr(s2), Addr(s3), p != nullptr, flag != nullptr, Addr(p) != Addr(flag));
    float* corners[] = {s0, s1, s2, s3};
    U salt = 173;
    for (float* c : corners) {
        PutLong(reinterpret_cast<unsigned char*>(c), MixFloat(salt++));
        PutLong(reinterpret_cast<unsigned char*>(c) + 4, MixFloat(salt++));
    }
    *p = static_cast<long>(Mix(170));
    *flag = static_cast<long>(Mix(171));
    return static_cast<long>(Mix(172));
}
void __cdecl StubPrimDepths(void* prim) {
    Record(20, Addr(prim));
    auto* p = static_cast<unsigned char*>(prim);
    for (U z = 0x10; z <= 0x40; z += 0x10) PutLong(p + z, Mix(200 + z));
}
void __cdecl StubPopMatrix() { Record(21); }
// MapView_ItemAt: 0 a third of the time, else a whole random dword (the
// caller uses all of eax); Gfx_BufferIndex is read after it.
U __cdecl StubItemAt(U x, U y) {
    Record(22, x, y);
    const U h = Mix(22);
    if (h % 5 == 0) At(kBufferIndex)[0] ^= 1;
    if (h % 3 == 0) return 0;
    return (h >> 4) % 4 ? (h >> 8) & 0xFFF : Mix(220);
}
// Gpu_LinkPrim: records the tail and the item; one round in four it moves
// Gfx_PacketNext or flips Gfx_BufferIndex, both read again by the caller.
void __cdecl StubLinkPrim(unsigned long* tail, U item) {
    Record(23, Addr(tail), item);
    const U h = Mix(23);
    switch (h % 8) {
    case 0: PutLong(At(kPacketNext), GetLong(At(kPacketNext)) + 0x10); break;
    case 1: At(kBufferIndex)[0] ^= 1; break;
    default: break;
    }
}

const Callees kStandIns = {
    StubState0,
    As<void (__cdecl*)(int, int)>(&StubDrawFrame),
    As<void (__cdecl*)(unsigned char*, int, int, unsigned, unsigned long)>(&StubSetDrawMode),
    As<void (__cdecl*)(unsigned, unsigned)>(&StubCommit),
    As<void (__cdecl*)(int, int, unsigned)>(&StubDrawSprite),
    As<unsigned char (__cdecl*)(short, short)>(&StubByteAt),
    As<unsigned char (__cdecl*)(short, short)>(&StubCellEvent),
    As<void (__cdecl*)(int, int)>(&StubDrawNeedle),
    StubSetSprt,
    As<void (__cdecl*)(unsigned char*, unsigned)>(&StubSetSemiTrans),
    As<const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*)>(&StubTextDrawAt),
    StubPushMatrix,
    StubRotMatrix,
    StubSetRotMatrix,
    StubSetTransMatrix,
    StubSetPolyG4,
    StubRotTransPers4,
    StubPrimDepths,
    StubPopMatrix,
    As<unsigned long (__cdecl*)(long, long)>(&StubItemAt),
    As<void (__cdecl*)(unsigned long*, unsigned long)>(&StubLinkPrim),
};

// --- the copies --------------------------------------------------------------------
// Every E8 of each body, by capstone 2026-09-24 (docs/world-map.md section
// 7); every jump stays inside. WorldMap_FrameStep's jump table is rebuilt for
// its copy (entry 0 re-aimed at the state-0 stand-in).

const void* StubFor(U target) {
    switch (target) {
    case 0x404390: return As<const void*>(&StubDrawFrame);
    case 0x5A77C0: return As<const void*>(&StubSetDrawMode);
    case 0x461E50: return As<const void*>(&StubCommit);
    case 0x404560: return As<const void*>(&StubDrawSprite);
    case 0x536700: return As<const void*>(&StubByteAt);
    case 0x531920: return As<const void*>(&StubCellEvent);
    case 0x408530: return As<const void*>(&StubDrawNeedle);
    case 0x5A7710: return As<const void*>(&StubSetSprt);
    case 0x5A7780: return As<const void*>(&StubSetSemiTrans);
    case 0x516B30: return As<const void*>(&StubTextDrawAt);
    case 0x5A7B90: return As<const void*>(&StubPushMatrix);
    case 0x5A8060: return As<const void*>(&StubRotMatrix);
    case 0x5A8DE0: return As<const void*>(&StubSetRotMatrix);
    case 0x5A8E00: return As<const void*>(&StubSetTransMatrix);
    case 0x5A7610: return As<const void*>(&StubSetPolyG4);
    case 0x5A85F0: return As<const void*>(&StubRotTransPers4);
    case 0x5A9350: return As<const void*>(&StubPrimDepths);
    case 0x5A7BC0: return As<const void*>(&StubPopMatrix);
    case 0x572ED0: return As<const void*>(&StubItemAt);
    case 0x5A7560: return As<const void*>(&StubLinkPrim);
    default: bof3::Fatal("world_map: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Site {
    U offset, target;
};
constexpr Site kStepSites[] = {{0x6F, 0x404390}, {0xB8, 0x404390}};
constexpr Site kFrameSites[] = {{0x22, 0x5A77C0},  {0x2B, 0x461E50},  {0x3C, 0x404560},  {0x51, 0x536700},
                                {0x7B, 0x404560},  {0xDC, 0x404560},  {0xF8, 0x404560},  {0x15B, 0x404560},
                                {0x173, 0x531920}, {0x1A8, 0x404560}, {0x1B8, 0x408530}};
constexpr Site kSpriteSites[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr Site kHudSites[] = {{0x17, 0x404560}, {0x26, 0x404560}, {0x4D, 0x516B30}};
constexpr Site kNeedleSites[] = {{0x7, 0x5A7B90},  {0x16, 0x5A8060}, {0x2E, 0x5A8DE0}, {0x38, 0x5A8E00}, {0xA0, 0x5A7610},
                                 {0xD3, 0x5A85F0}, {0xD9, 0x5A9350}, {0x181, 0x461E50}, {0x189, 0x5A7BC0}};
constexpr Site kHalfSites[] = {{0xA, 0x572ED0}};
constexpr Site kLinkSites[] = {{0x87, 0x5A7560}};

void* Clone(const char* name, U base, U size, const Site* sites, int n) {
    bof3::CloneCall calls[12];
    if (n > 12) bof3::Fatal("world_map: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, StubFor(sites[i].target), sites[i].target};
    void* code = bof3::CloneOriginal(name, base, size, calls, n);
    if (!code) bof3::Fatal("world_map: CloneOriginal(%s) returned null", name);
    return code;
}

// WorldMap_FrameStep's copy: its `jmp [eax*4 + 0x5EF5F8]` (the disp32 at
// +0xE) re-aimed at a table of the fuzz's own whose entry 0 is the state-0
// stand-in and entries 1..3 the copy's own case blocks.
U g_step_table[4];
void RelocateStepTable(void* copy) {
    auto* code = static_cast<unsigned char*>(copy);
    constexpr U kBase = 0x404160, kSize = 0xC1;
    U disp;
    std::memcpy(&disp, code + 0xE, sizeof disp);
    if (disp != kStateTable || code[0xB] != 0xFF || code[0xC] != 0x24 || code[0xD] != 0x85)
        bof3::Fatal("world_map: WorldMap_FrameStep's copy has no jump table operand at +0xE");
    for (U i = 0; i < 4; ++i) {
        const U target = GetLong(At(kStateTable + i * 4));
        if (i == 0) {
            if (target != kState0) bof3::Fatal("world_map: state table entry 0 is 0x%X, not 0x411310", (unsigned)target);
            g_step_table[0] = Addr(As<const void*>(&StubState0));
        } else {
            if (target < kBase || target >= kBase + kSize)
                bof3::Fatal("world_map: state table entry %u is 0x%X, outside the copy", (unsigned)i, (unsigned)target);
            g_step_table[i] = Addr(code) + (target - kBase);
        }
    }
    const U table = Addr(g_step_table);
    std::memcpy(code + 0xE, &table, sizeof table);
}

// --- the comparison ------------------------------------------------------------------

d3d_fuzz::Log g_theirs, g_ours;
State g_start, g_after_theirs, g_after_ours;

struct Tally {
    const char* name;
    unsigned rounds, bad, calls, hits;   // hits: a coverage count each test defines
};

template <typename Theirs, typename Ours>
void Pass(Tally& t, const Region* rs, int nr, U mask, Theirs theirs, Ours ours) {
    Capture(rs, nr, g_start);
    g_theirs.Clear();
    d3d_fuzz::g_log = &g_theirs;
    const U ret_theirs = theirs();
    Capture(rs, nr, g_after_theirs);

    Restore(rs, nr, g_start);
    g_ours.Clear();
    d3d_fuzz::g_log = &g_ours;
    const U ret_ours = ours();
    Capture(rs, nr, g_after_ours);
    d3d_fuzz::g_log = nullptr;

    ++t.rounds;
    t.calls += g_theirs.n;
    char why[200] = "";
    bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && (ret_ours & mask) != (ret_theirs & mask)) {
        std::snprintf(why, sizeof why, "the result %08X, the original %08X", (unsigned)ret_ours,
                      (unsigned)ret_theirs);
        same = false;
    }
    U where = 0;
    if (same && FirstDifference(rs, nr, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) {
        if (t.bad < 4) bof3::Log("shadow      world_map MISMATCH: %s round %u: %s", t.name, t.rounds - 1, why);
        ++t.bad;
    }
}

void FillRandom(U at, U bytes) {
    for (U i = 0; i < bytes; i += 4) PutLong(At(at + i), Next());
}
void FillRandom(unsigned char* p, U bytes) {
    for (U i = 0; i < bytes; ++i) p[i] = static_cast<unsigned char>(Next());
}
// A word argument with a stale upper half, now and then.
U WordArg(U low) { return (low & 0xFFFF) | (Next() % 3 ? 0 : Next() << 16); }
U Coordinate() {
    return Next() % 2 ? Next() % 0x140 : Pick({0, 0x10, 0x5C, 0x9E, 0x76, 0xFFFF, 0x8000, 0x7FFF, 0xFFD0, 0x2A0});
}

// --- WorldMap_FrameStep ------------------------------------------------------------------

void FuzzStep(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)();
    const Region regions[] = {{Addr(g_object), sizeof g_object}, {kMapMode, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x100000u + r;
        FillRandom(g_object, sizeof g_object);
        PutLong(At(kSpriteCurrent), Addr(g_object));
        g_object[2] = static_cast<unsigned char>(Next() % 4);
        PutWord(g_object + 0x2E, Next() % 2 ? Next() : Pick({0xFFC0, 0xFFD0, 0xFFCF, 0xFFD1, 0xFFE0, 0, 0xF, 0x10, 0x11, 0x20,
                                                             0xFFC1, 0xFFBF}));
        At(kMapMode)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 4 : Next());
        Pass(t, regions, 2, 0, [&] { reinterpret_cast<Fn>(clone)(); return 0u; },
             [&] { WorldMap_FrameStep(); return 0u; });
        if (g_theirs.n && g_theirs.calls[0].what == 2) ++t.hits;
    }
}

// --- WorldMap_DrawFrame ------------------------------------------------------------------

U ButtonWord() {
    switch (Next() % 6) {
    case 0: return Pick({0x10, 0x80, 0x20, 0x40, 0x08, 0x04});          // one of the six
    case 1: return 0;                                                   // none
    case 2: return Pick({0x100, 0x200, 0x400, 0x1000, 0x4000, 0x5300, 0x4600, 0xFF00});   // the state table's words
    case 3: return Next() & 0xFF;                                       // several
    case 4: return Next();                                              // any, with an upper half
    default: return Pick({0xFFFF, 0x23, 0x43, 0x1, 0x2, 0x8000});
    }
}

void FuzzFrame(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U);
    const Region regions[] = {{Addr(g_packets), sizeof g_packets}, {kPacketNext, 4},  {kPassFlags, 1},
                              {kFlags, 2},                          {kFlags2, 2},     {kPartySet, 1},
                              {kButtonMap0, 4},                     {kButtonMap6, 4}, {kLeaderCellZ, 8},
                              {kBufferIndex, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x200000u + r;
        FillRandom(g_packets, sizeof g_packets);
        PutLong(At(kPacketNext), Addr(g_packets) + (Next() % 2 ? 0 : 4));
        At(kPassFlags)[0] = static_cast<unsigned char>(Next() % 4 ? Next() : Pick({0, 0x1B, 0xE4, 1, 2, 8, 0x10, 0x20}));
        PutWord(At(kFlags), Next() % 2 ? Next() : Pick({0, 0x4000, 0xBFFF, 0xFFFF}));
        PutWord(At(kFlags2), Next() % 2 ? Next() : Pick({0, 0x1000, 0xEFFF, 0xFFFF}));
        At(kPartySet)[0] = static_cast<unsigned char>(Next() % 2 ? Next() : Pick({0xC, 0x8C, 0xFF, 0, 0xB, 0xD}));
        PutLong(At(kButtonMap0), ButtonWord());
        PutLong(At(kButtonMap6), ButtonWord());
        FillRandom(kLeaderCellZ, 8);
        At(kBufferIndex)[0] = static_cast<unsigned char>(Next() % 2);
        const U x = WordArg(Coordinate()), y = WordArg(Coordinate());
        Pass(t, regions, 10, 0, [&] { reinterpret_cast<Fn>(clone)(x, y); return 0u; },
             [&] { As<Fn>(&WorldMap_DrawFrame)(x, y); return 0u; });
        if (g_theirs.n > 4) ++t.hits;
    }
}

// --- WorldMap_DrawSprite -----------------------------------------------------------------

void FuzzSprite(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U, U);
    const Region regions[] = {{Addr(g_packets), sizeof g_packets}, {kPacketNext, 4}, {kBufferIndex, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x300000u + r;
        FillRandom(g_packets, sizeof g_packets);
        PutLong(At(kPacketNext), Addr(g_packets) + (Next() % 2 ? 0 : 4));
        At(kBufferIndex)[0] = static_cast<unsigned char>(Next() % 2);
        const U x = WordArg(Coordinate()), y = WordArg(Coordinate());
        U index;
        switch (Next() % 4) {
        case 0: index = Next() % 22; break;
        case 1: index = Pick({0, 1, 2, 3, 4, 5, 6, 7, 21, 22, 0x40, 0x42, 0x43, 0xFF}); break;
        case 2: index = Pick({0x100, 0x1FF, 0x122, 0x10000, 0xFFFFFF00u, 0xFFFFFFFFu, 0x80000000u}); break;
        default: index = Next(); break;
        }
        Pass(t, regions, 3, 0, [&] { reinterpret_cast<Fn>(clone)(x, y, index); return 0u; },
             [&] { As<Fn>(&WorldMap_DrawSprite)(x, y, index); return 0u; });
        if ((index & 0xFF) == 0) ++t.hits;
    }
}

// --- WorldMap_DrawHud --------------------------------------------------------------------

void FuzzHud(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U);
    const Region regions[] = {{kPassFlags, 1}, {kAreaTextOffset, 4}, {kFlags2, 2}, {kFlags, 2}, {kPartySet, 1},
                              {kButtonMap0, 4}, {kButtonMap6, 4}, {kLeaderCellZ, 8}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x400000u + r;
        At(kPassFlags)[0] = static_cast<unsigned char>(Next() % 4 ? Next() : Pick({0, 0x1B, 0xE4, 1, 2, 8, 0x10, 0x20}));
        PutLong(At(kAreaTextOffset), Next() % 2 ? Next() : Pick({0, 0xFFFF, 0x10000, 0x8, 0x1FFFF, 0xFFFFFFFFu}));
        const U x = WordArg(Coordinate()), y = WordArg(Coordinate());
        Pass(t, regions, 8, 0, [&] { reinterpret_cast<Fn>(clone)(x, y); return 0u; },
             [&] { As<Fn>(&WorldMap_DrawHud)(x, y); return 0u; });
        if (g_theirs.n) ++t.hits;
    }
}

// --- WorldMap_DrawNeedle -----------------------------------------------------------------

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

void FuzzNeedle(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U);
    const Region regions[] = {{Addr(g_packets), sizeof g_packets}, {kPacketNext, 4}, {kScratch, 0x20}, {kAngles, 6}};
    const unsigned short saved = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x500000u + r;
        FillRandom(g_packets, sizeof g_packets);
        PutLong(At(kPacketNext), Addr(g_packets) + (Next() % 2 ? 0 : 4));
        FillRandom(kScratch, 0x20);
        FillRandom(kAngles, 6);
        const U x = WordArg(Coordinate()), y = WordArg(Coordinate());
        const unsigned short cw = kControlWords[Next() % 3];
        Pass(t, regions, 4, 0,
             [&] {
                 SetControlWord(cw);
                 reinterpret_cast<Fn>(clone)(x, y);
                 SetControlWord(saved);
                 return 0u;
             },
             [&] {
                 SetControlWord(cw);
                 As<Fn>(&WorldMap_DrawNeedle)(x, y);
                 SetControlWord(saved);
                 return 0u;
             });
        ++t.hits;
    }
}

// --- MapView_ItemHalfAt -------------------------------------------------------------------

void FuzzItemHalf(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U);
    const Region regions[] = {{kBufferIndex, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x600000u + r;
        At(kBufferIndex)[0] = static_cast<unsigned char>(Next() % 4 ? Next() % 2 : Next());
        const U x = Next(), y = Next();
        U ret = 0;
        Pass(t, regions, 1, 0xFFFFFFFFu,
             [&] {
                 ret = reinterpret_cast<Fn>(clone)(x, y);
                 return ret;
             },
             [&] { return As<Fn>(&MapView_ItemHalfAt)(x, y); });
        if (ret) ++t.hits;
    }
}

// --- MapView_LinkPrimAt -------------------------------------------------------------------

void FuzzLinkPrim(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(U, U, U, U);
    const Region regions[] = {{kRowTails - 4, kRowTailsBytes}, {kPacketNext, 4}, {kBufferIndex, 1}, {kOrigin, 4}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x700000u + r;
        if (r % 8 == 0) FillRandom(kRowTails - 4, kRowTailsBytes);
        // Origins small mostly, so the row's 16-bit halves can reach it; the
        // edges one round in four (the sum then leaves the row's range).
        PutWord(At(kOrigin), Next() % 4 ? Next() % 0x40 : Pick({0, 0xFFFF, 0x8000, 0x7FFF, 0x10, Next()}));
        PutWord(At(kOrigin + 2), Next() % 4 ? Next() % 0x40 : Pick({0, 0xFFFF, 0x8000, 0x7FFF, 0x10, Next()}));
        // The buffer index stays 0 or 1, as the game keeps it: the slot store
        // reaches (row * 6 + index) * 8 past Gfx_FrameNodes, and a larger index
        // would write beyond the records into the game's data.
        const U buffer = Next() % 2;
        At(kBufferIndex)[0] = static_cast<unsigned char>(buffer);
        // A row aimed at the edges: -2 .. 0x39 mostly, from the origin, the
        // fractions and dy.
        const auto ox = static_cast<std::int32_t>(static_cast<std::int16_t>(GetWord(At(kOrigin))));
        const auto oz = static_cast<std::int32_t>(static_cast<std::int16_t>(GetWord(At(kOrigin + 2))));
        const std::int32_t want = Next() % 4 ? static_cast<std::int32_t>(Next() % 0x3C) - 2
                                             : static_cast<std::int32_t>(Pick({0, 0x37, 0x38, 0xFFFFFFFFu, 0x39, 0x7FFF, 0x8000}));
        const U dy = Next() % 3 ? Pick({0, 1, 0xFF, 0x7F, 0x80, 2, 0xFE}) : Next();
        const U xlow = Next() % 2 ? 0 : (Next() & 0xFFFF), zlow = Next() % 2 ? 0 : (Next() & 0xFFFF);
        const std::int32_t sum = want - static_cast<signed char>(dy) - 2 + (xlow == 0) + (zlow == 0) + ox + oz;
        const std::int32_t xh = static_cast<std::int32_t>(Next() % 0x40) - 0x20;
        const std::int32_t zh = sum - xh;
        const U x = (static_cast<U>(xh) << 16) | xlow, z = (static_cast<U>(zh) << 16) | zlow;
        const U size = Next() % 2 ? Next() % 0x100 : Pick({0, 1, 0xFF, 0x1C, 0x44, 0x100, 0x1FF, 0xFFFFFF00u, Next()});
        // The cursor against the pool's bound: (buffer << 16) + 0x7F1BAC.
        const U bound = (buffer << 16) + kPoolBound;
        U next;
        switch (Next() % 8) {
        case 0: next = bound - (size & 0xFF); break;         // no room, by nothing
        case 1: next = bound - (size & 0xFF) - 1; break;     // room, by one byte
        case 2: next = bound - (size & 0xFF) + 1; break;     // no room
        case 3: next = bound + 0x1000; break;                // far past
        case 4: next = Next(); break;                        // anywhere
        default: next = bound - (size & 0xFF) - 1 - Next() % 0x1000; break;   // room
        }
        PutLong(At(kPacketNext), next);
        const unsigned bad_before = t.bad;
        Pass(t, regions, 4, 0, [&] { reinterpret_cast<Fn>(clone)(x, z, dy, size); return 0u; },
             [&] { As<Fn>(&MapView_LinkPrimAt)(x, z, dy, size); return 0u; });
        if (t.bad != bad_before && t.bad < 4)
            bof3::Log("shadow      world_map LinkPrimAt round %u: x %08X z %08X dy %02X size %02X buffer %u origin %04X %04X want row %d next %08X",
                      r, (unsigned)x, (unsigned)z, (unsigned)(dy & 0xFF), (unsigned)(size & 0xFF), (unsigned)buffer,
                      (unsigned)GetWord(At(kOrigin)), (unsigned)GetWord(At(kOrigin + 2)), (int)want, (unsigned)next);
        if (g_theirs.n) ++t.hits;
    }
}

}  // namespace

void SelfTest() {
    // The copies, before WorldMap_Inject patches anything.
    void* step = Clone("WorldMap_FrameStep", 0x404160, 0xC1, kStepSites, 2);
    RelocateStepTable(step);
    void* frame = Clone("WorldMap_DrawFrame", 0x404390, 0x1C5, kFrameSites, 11);
    void* sprite = Clone("WorldMap_DrawSprite", 0x404560, 0xBC, kSpriteSites, 5);
    void* hud = Clone("WorldMap_DrawHud", 0x404620, 0x58, kHudSites, 3);
    void* needle = Clone("WorldMap_DrawNeedle", 0x408530, 0x196, kNeedleSites, 9);
    void* half = Clone("MapView_ItemHalfAt", 0x572F70, 0x2F, kHalfSites, 1);
    void* link = Clone("MapView_LinkPrimAt", 0x572FA0, 0xAF, kLinkSites, 1);

    const int n_all = static_cast<int>(sizeof g_all / sizeof g_all[0]);
    static State saved;
    if (RegionBytes(g_all, n_all) > kMaxState) bof3::Fatal("world_map: the saved regions outgrow the state buffer");
    Capture(g_all, n_all, saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x574D4150u);

    Tally tallies[] = {
        {"WorldMap_FrameStep", 0, 0, 0, 0}, {"WorldMap_DrawFrame", 0, 0, 0, 0},  {"WorldMap_DrawSprite", 0, 0, 0, 0},
        {"WorldMap_DrawHud", 0, 0, 0, 0},   {"WorldMap_DrawNeedle", 0, 0, 0, 0}, {"MapView_ItemHalfAt", 0, 0, 0, 0},
        {"MapView_LinkPrimAt", 0, 0, 0, 0},
    };
    FuzzStep(tallies[0], step, 10000);
    FuzzFrame(tallies[1], frame, 20000);
    FuzzSprite(tallies[2], sprite, 10000);
    FuzzHud(tallies[3], hud, 5000);
    FuzzNeedle(tallies[4], needle, 20000);
    FuzzItemHalf(tallies[5], half, 5000);
    FuzzLinkPrim(tallies[6], link, 20000);

    g = saved_callees;
    Restore(g_all, n_all, saved);

    unsigned bad = 0, rounds = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        bof3::Log("shadow      world_map self-test: %s %u rounds, %u calls out, %u covered, %u MISMATCHES", t.name,
                  t.rounds, t.calls, t.hits, t.bad);
    }
    bof3::Log("shadow      world_map self-test: %u rounds over 7 functions, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("the world map's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace world_map
