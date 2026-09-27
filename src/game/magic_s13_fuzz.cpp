// BOF3X_SHADOW=magic_s13: group S13's overlay (MAGIC063) through the spell
// round's shared harness (magic_harness.h), once at start-up.
// docs/magic_s13.md section 4.
//
// The clone table is tools/magic_rows.py --unit MAGIC063 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// GTE and libgpu entry points, Math_Sin / Math_Cos, MapView_LinkPrimAt),
// Battle_ActorIsOut, and the functions of its own its functions call
// directly. Everything the harness lacks is built here, not in the harness:
//
//   - MapView_LinkPrimAt logs each primitive's bytes (every triangle of the
//     draw is built in the same buffer, so the state compare alone would see
//     only the last) and moves Gfx_PacketNext on through a packet buffer of the
//     fuzz's own, as the real one does;
//   - the GTE callees of the matrix push and the projection log what their
//     pointers point at (`deref`) and write a result where the real ones
//     write (group S01's / S22's);
//   - the mote pool's allocator answers 0xFF (full) or an index inside the
//     pool, as the real one does;
//   - the mote functions called directly (the dispatch, the matrix, the draw,
//     the free) log the current mote cell with the task.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s13.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s13 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --unit MAGIC063 --clones, 2026-09-26, names given.
// 0x4B2F40: 0x61 bytes  SuddenDeath_Task
constexpr mh::CallSite kCalls4B2F40[] = {{0x49, 0x4B3350}};
constexpr mh::Imm kImms4B2F40[] = {{0x15, 0x4B2FB0}, {0x1D, 0x4E5200}};
// 0x4B2FB0: 0x131 bytes  SuddenDeath_Spawn
constexpr mh::CallSite kCalls4B2FB0[] = {{0x43, 0x4456C0}, {0x63, 0x435180}, {0xBA, 0x4456C0}, {0xD2, 0x435180}, {0x123, 0x587900}};
// 0x4B30F0: 0x12 bytes; +0xB note: jmp through .data 0x65aba4, 1 code entries (a data_tables entry)  SuddenDeathChild_Task
// 0x4B3110: 0x3E bytes  SuddenDeathChild_Run
constexpr mh::Imm kImms4B3110[] = {{0xF, 0x4B3150}, {0x17, 0x4B3210}, {0x22, 0x4B3250}, {0x2A, 0x4B3270}, {0x32, 0x4B3320}};
// 0x4B3150: 0xB1 bytes  SuddenDeathChild_Start
constexpr mh::CallSite kCalls4B3150[] = {{0x5D, 0x5B93D2}, {0x80, 0x4B3E80}};
// 0x4B3210: 0x3A bytes  SuddenDeathChild_Delay
// 0x4B3250: 0x12 bytes  SuddenDeathChild_WaitMotes
constexpr mh::CallSite kCalls4B3250[] = {{0xC, 0x4351F0}};
// 0x4B3270: 0xA2 bytes  SuddenDeathChild_Burst
constexpr mh::CallSite kCalls4B3270[] = {{0x15, 0x4B3E80}, {0x48, 0x5B93D2}, {0x7F, 0x587900}, {0x89, 0x587900}};
// 0x4B3320: 0x26 bytes  SuddenDeathChild_End
constexpr mh::CallSite kCalls4B3320[] = {{0x10, 0x4530D0}, {0x20, 0x4351F0}};
// 0x4B3350: 0x12 bytes; +0xB note: jmp through .data 0x65aba8, 2 code entries (a data_tables entry)  SuddenDeathMote_Task
// 0x4B3370: 0x33 bytes; +0xB note: call through .data 0x65abb0, 4 code entries (a data_tables entry)  SuddenDeathBurst_Run
constexpr mh::CallSite kCalls4B3370[] = {{0x23, 0x4B3780}, {0x28, 0x4B3840}, {0x2D, 0x5A7BC0}};
// 0x4B33B0: 0x141 bytes  SuddenDeathBurst_Launch
constexpr mh::CallSite kCalls4B33B0[] = {{0x4B, 0x5A7A00}, {0x76, 0x5A7A50}, {0xBE, 0x5B93D2}, {0xD0, 0x5B93D2}, {0xE2, 0x5B93D2}, {0x107, 0x5B93D2}, {0x11A, 0x5B93D2}};
// 0x4B3500: 0xCA bytes  SuddenDeathBurst_Close
constexpr mh::CallSite kCalls4B3500[] = {{0x15, 0x5A7A00}, {0x3F, 0x5A7A50}, {0xC1, 0x452F70}};
// 0x4B35D0: 0xE3 bytes  SuddenDeathBurst_Spin
constexpr mh::CallSite kCalls4B35D0[] = {{0x23, 0x5A7A00}, {0x4C, 0x5A7A50}};
// 0x4B36C0: 0xBC bytes  SuddenDeathBurst_Rise
constexpr mh::CallSite kCalls4B36C0[] = {{0x23, 0x5A7A00}, {0x4C, 0x5A7A50}, {0xB6, 0x4B3ED0}};
// 0x4B3780: 0xBC bytes  SuddenDeathMote_PushMatrix
constexpr mh::CallSite kCalls4B3780[] = {{0x3, 0x5A7B90}, {0x7C, 0x5A8200}, {0x8B, 0x5A8060}, {0x9F, 0x5A7D70}, {0xA9, 0x5A8DE0}, {0xB3, 0x5A8E00}};
// 0x4B3840: 0x245 bytes  SuddenDeathMote_Draw
constexpr mh::CallSite kCalls4B3840[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0xEC, 0x5A75F0}, {0xF4, 0x5A7780}, {0x101, 0x5A7A00}, {0x121, 0x5A7A50}, {0x141, 0x5A7A00}, {0x161, 0x5A7A50}, {0x181, 0x5A7A00}, {0x1BD, 0x5A84A0}, {0x1C6, 0x5A9310}, {0x22A, 0x572FA0}};
// 0x4B3A90: 0x33 bytes; +0xB note: call through .data 0x65abe4, 5 code entries (a data_tables entry)  SuddenDeathOrbit_Run
constexpr mh::CallSite kCalls4B3A90[] = {{0x23, 0x4B3780}, {0x28, 0x4B3840}, {0x2D, 0x5A7BC0}};
// 0x4B3AD0: 0xC6 bytes  SuddenDeathOrbit_Start
constexpr mh::CallSite kCalls4B3AD0[] = {{0x2, 0x5A7A00}, {0x20, 0x5A7A50}, {0x50, 0x5B93D2}, {0x62, 0x5B93D2}, {0x74, 0x5B93D2}, {0xAF, 0x5B93D2}};
// 0x4B3BA0: 0x91 bytes  SuddenDeathOrbit_Brighten
constexpr mh::CallSite kCalls4B3BA0[] = {{0x22, 0x5A7A00}, {0x48, 0x5A7A50}};
// 0x4B3C40: 0xAB bytes  SuddenDeathOrbit_Circle
constexpr mh::CallSite kCalls4B3C40[] = {{0x22, 0x5A7A00}, {0x48, 0x5A7A50}};
// 0x4B3CF0: 0xC0 bytes  SuddenDeathOrbit_Spin
constexpr mh::CallSite kCalls4B3CF0[] = {{0x5D, 0x5A7A00}, {0x84, 0x5A7A50}};
// 0x4B3DB0: 0xCA bytes  SuddenDeathOrbit_Fade
constexpr mh::CallSite kCalls4B3DB0[] = {{0x5D, 0x5A7A00}, {0x84, 0x5A7A50}, {0xC4, 0x4B3ED0}};
// 0x4B3E80: 0x4F bytes  SuddenDeathMote_Alloc
// 0x4B3ED0: 0x2F bytes  SuddenDeathMote_Free
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S13_CLONE(name, base, size, calls, n_calls, imms, n_imms) \
    {#name, base, size, calls, n_calls, imms, n_imms, nullptr, 0, reinterpret_cast<const void*>(&::name)}
const mh::Clone kClones[] = {
    S13_CLONE(SuddenDeath_Task, 0x4B2F40, 0x61, kCalls4B2F40, MH_N(kCalls4B2F40), kImms4B2F40, MH_N(kImms4B2F40)),
    S13_CLONE(SuddenDeath_Spawn, 0x4B2FB0, 0x131, kCalls4B2FB0, MH_N(kCalls4B2FB0), nullptr, 0),
    S13_CLONE(SuddenDeathChild_Task, 0x4B30F0, 0x12, nullptr, 0, nullptr, 0),
    S13_CLONE(SuddenDeathChild_Run, 0x4B3110, 0x3E, nullptr, 0, kImms4B3110, MH_N(kImms4B3110)),
    S13_CLONE(SuddenDeathChild_Start, 0x4B3150, 0xB1, kCalls4B3150, MH_N(kCalls4B3150), nullptr, 0),
    S13_CLONE(SuddenDeathChild_Delay, 0x4B3210, 0x3A, nullptr, 0, nullptr, 0),
    S13_CLONE(SuddenDeathChild_WaitMotes, 0x4B3250, 0x12, kCalls4B3250, MH_N(kCalls4B3250), nullptr, 0),
    S13_CLONE(SuddenDeathChild_Burst, 0x4B3270, 0xA2, kCalls4B3270, MH_N(kCalls4B3270), nullptr, 0),
    S13_CLONE(SuddenDeathChild_End, 0x4B3320, 0x26, kCalls4B3320, MH_N(kCalls4B3320), nullptr, 0),
    S13_CLONE(SuddenDeathMote_Task, 0x4B3350, 0x12, nullptr, 0, nullptr, 0),
    S13_CLONE(SuddenDeathBurst_Run, 0x4B3370, 0x33, kCalls4B3370, MH_N(kCalls4B3370), nullptr, 0),
    S13_CLONE(SuddenDeathBurst_Launch, 0x4B33B0, 0x141, kCalls4B33B0, MH_N(kCalls4B33B0), nullptr, 0),
    S13_CLONE(SuddenDeathBurst_Close, 0x4B3500, 0xCA, kCalls4B3500, MH_N(kCalls4B3500), nullptr, 0),
    S13_CLONE(SuddenDeathBurst_Spin, 0x4B35D0, 0xE3, kCalls4B35D0, MH_N(kCalls4B35D0), nullptr, 0),
    S13_CLONE(SuddenDeathBurst_Rise, 0x4B36C0, 0xBC, kCalls4B36C0, MH_N(kCalls4B36C0), nullptr, 0),
    S13_CLONE(SuddenDeathMote_PushMatrix, 0x4B3780, 0xBC, kCalls4B3780, MH_N(kCalls4B3780), nullptr, 0),
    S13_CLONE(SuddenDeathMote_Draw, 0x4B3840, 0x245, kCalls4B3840, MH_N(kCalls4B3840), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Run, 0x4B3A90, 0x33, kCalls4B3A90, MH_N(kCalls4B3A90), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Start, 0x4B3AD0, 0xC6, kCalls4B3AD0, MH_N(kCalls4B3AD0), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Brighten, 0x4B3BA0, 0x91, kCalls4B3BA0, MH_N(kCalls4B3BA0), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Circle, 0x4B3C40, 0xAB, kCalls4B3C40, MH_N(kCalls4B3C40), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Spin, 0x4B3CF0, 0xC0, kCalls4B3CF0, MH_N(kCalls4B3CF0), nullptr, 0),
    S13_CLONE(SuddenDeathOrbit_Fade, 0x4B3DB0, 0xCA, kCalls4B3DB0, MH_N(kCalls4B3DB0), nullptr, 0),
    // answers in al: its index, or 0xFF
    {"SuddenDeathMote_Alloc", 0x4B3E80, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::SuddenDeathMote_Alloc), 0xFF},
    S13_CLONE(SuddenDeathMote_Free, 0x4B3ED0, 0x2F, nullptr, 0, nullptr, 0),
};
#undef S13_CLONE
#undef MH_N

enum : unsigned {
    kTask, kSpawn, kChildTask, kChildRun, kChildStart, kChildDelay, kChildWaitMotes, kChildBurst, kChildEnd, kMoteTask,
    kBurstRun, kBurstLaunch, kBurstClose, kBurstSpin, kBurstRise, kPushMatrix, kDraw, kOrbitRun, kOrbitStart,
    kOrbitBrighten, kOrbitCircle, kOrbitSpin, kOrbitFade, kAlloc, kFree, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850;
constexpr std::uint32_t kPool = 0x682680, kCell = 0x683280;
constexpr unsigned kMotes = 0x60, kStride = 0x20;
// SuddenDeathMote_Shape and _Turns, the exe's .data the draw reads.
constexpr std::uint32_t kShape = 0x65ABC0, kShapeBytes = 0x24;

// The .data as the exe holds it, read at start-up (not written down here).
unsigned char g_exe_shape[kShapeBytes];

unsigned char* Mote(unsigned i) { return mh::Mem(kPool + (i % kMotes) * kStride); }
unsigned char* Cur() { return mh::Pointer(kCell); }
unsigned char* ValidOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }

// --- the callees' effects (after the recorder's log and disturbance) ------------

// MapView_LinkPrimAt: the primitive at Gfx_PacketNext into the log (the real
// one links it), then Gfx_PacketNext on by its size, kept in the buffer.
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    const std::uint32_t size = a[3] & 0xFF;
    mh::NoteBytes(Gfx_PacketNext, size);
    unsigned char* p = Gfx_PacketNext + size;
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
    return answer;
}
// The GTE stand-ins of the matrix push: a result from the inputs, written
// where the real callee writes (group S22's).
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* t = reinterpret_cast<long*>(static_cast<std::uintptr_t>(a[1]));
    t[0] = v[0] * 3 + 1;
    t[1] = v[1] * 5 - 2;
    t[2] = v[2] * 7 + 3;
    return answer;
}
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* r = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* m = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(r[i % 3] * static_cast<int>(i + 1) + static_cast<int>(i));
    return answer;
}
std::uint32_t MulMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* b = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[1]));
    auto* out = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[2]));
    mh::Note(a[1] == a[2]);
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}
// Gte_RotTransPers3: the three screen points written where the real one
// writes them (floats of the vertices), so the primitive logged at the link
// carries them (group S01's).
std::uint32_t Rtp3Effect(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 0; i < 3; ++i) {
        const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[i]));
        auto* out = reinterpret_cast<float*>(static_cast<std::uintptr_t>(a[3 + i]));
        out[0] = static_cast<float>(v[0] * 2 + v[2]);
        out[1] = static_cast<float>(v[1] * 3 - v[2]);
    }
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S13_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S13_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // the engine's, ours: al tested
    {S13_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    // the draw library (psx_gpu, psx_gte*, world_map, the math): all ours
    {S13_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S13_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S13_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S13_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S13_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S13_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S13_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S13_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S13_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S13_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S13_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S13_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S13_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}, &Rtp3Effect},
    {S13_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    // this group's own, called directly: the task and the current mote logged
    {S13_RAW(0x4B3350), 0, {kCell}, mh::Answer::kPhase, 0, 0},
    {S13_RAW(0x4B3780), 0, {kCell}, mh::Answer::kPhase, 0, 0},
    {S13_RAW(0x4B3840), 0, {kCell}, mh::Answer::kPhase, 0, 0},
    {S13_RAW(0x4B3ED0), 0, {kCell}, mh::Answer::kPhase, 0, 0},
    // the pool's allocator: 0xFF (full) or an index inside the pool
    {S13_RAW(0x4B3E80), 0, {}, mh::Answer::kByte, 0xFF, kMotes - 1},
};
#undef S13_OURS
#undef S13_RAW

// The four .data handler tables the dispatchers read in place
// (SuddenDeathChild_Kinds, SuddenDeathMote_Kinds, SuddenDeathBurst_Steps,
// SuddenDeathOrbit_Steps).
const mh::DataTable kTables[] = {{0x65ABA4, 1}, {0x65ABA8, 2}, {0x65ABB0, 4}, {0x65ABE4, 5}};

mh::Region g_regions[] = {
    {kPool, kMotes * kStride + 4},   // SuddenDeath_Motes and SuddenDeath_CurrentMote
    {kShape, kShapeBytes},           // SuddenDeathMote_Shape, _Turns
    {0x7E0670, 4},                   // Gfx_PacketNext
    {0, kPrimBytes},                 // g_prims (filled in at start-up)
    {kVertex, 0x18},                 // Prim_VertexScratch, three SVECTORs
    {kScratch, 0x10},                // DamageScratch's words
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// The group's cells a recorder may move (the harness's case 14): the current
// mote cell to another mote, the packet pointer, a scratch or vertex word, a
// byte of the current mote below its owner +0x1C (which the walk hands on).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 5) {
    case 0: mh::SetPointer(kCell, Mote(v)); break;
    case 1: Gfx_PacketNext = PrimAt(v); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetWord(mh::Mem(kVertex + 2 * (v % 12)), h >> 16); break;
    case 4: {
        const std::uint32_t c = static_cast<std::uint32_t>(Long(mh::Mem(kCell)));
        if (c >= kPool && c < kPool + kMotes * kStride) Cur()[v % 0x1C] = Byte(h >> 24);
        break;
    }
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    // Every round: the packet pointer into the fuzz's buffer; every mote owned
    // by a real slot or record (the walk makes it the owner a recorder may
    // write through); the current cell a mote; the exe's .data two in three.
    Gfx_PacketNext = PrimAt(mh::Next());
    for (unsigned i = 0; i < kMotes; ++i) mh::SetPointer(kPool + i * kStride + 0x1C, ValidOwner(mh::Next()));
    mh::SetPointer(kCell, Mote(mh::Next()));
    if (mh::Often()) std::memcpy(mh::Mem(kShape), g_exe_shape, kShapeBytes);
    unsigned char* const m = Cur();
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    switch (k) {
    // the dispatchers: inside their tables
    case kTask: sc[1] = Byte(mh::Next() % 2); break;
    case kChildTask: sc[1] = 0; break;
    case kChildRun: sc[2] = Byte(mh::Next() % 5); break;
    case kMoteTask: m[1] = Byte(mh::Next() % 2); break;
    case kBurstRun:
        m[2] = Byte(mh::Next() % 4);
        if (mh::Half()) m[0] = mh::Half() ? 0 : 1;
        break;
    case kOrbitRun:
        m[2] = Byte(mh::Next() % 5);
        if (mh::Half()) m[0] = mh::Half() ? 0 : 1;
        break;
    // the spawn: any acting actor, on either side
    case kSpawn:
        if (mh::Often()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Next() % 11);
        break;
    // the children: their actor inside the records, their counters at 0 / 1
    case kChildStart: sc[4] = Byte(mh::Next() % 11); break;
    case kChildDelay:
        Near(sc[9], 0);
        if (mh::Often()) owner[0xB] = Byte(MH_PICK(0, 1, 2));
        break;
    case kChildWaitMotes: case kChildBurst: if (mh::Half()) sc[0xB] = 0; break;
    case kChildEnd:
        if (mh::Half()) sc[0xB] = 0;
        sc[4] = Byte(mh::Next() % 11);
        break;
    // the burst: each counter at its threshold
    case kBurstLaunch: Near(m[0xA], 0); break;
    case kBurstClose: {
        if (mh::Often()) m[9] = Byte(MH_PICK(0xE, 0xF, 0x10, 0x11));
        if (mh::Half()) m[4] = Byte(mh::Half() ? 0 : mh::Next() % 0x5C);
        if (mh::Often()) {
            const unsigned limit = (m[4] >> 5) * 8u + 0x20u;
            SetWord(m + 0xC, limit + 1 + mh::Next() % 3);
        }
        break;
    }
    case kBurstSpin:
        if (mh::Often()) m[5] = Byte(MH_PICK(0xB, 0xC, 0xD));
        if (mh::Often()) m[6] = Byte(MH_PICK(3, 4, 5));
        if (mh::Often()) m[7] = Byte(MH_PICK(3, 4, 5));
        if (mh::Often()) m[3] = Byte(MH_PICK(6, 7, 8));
        Near(m[8], 0);
        break;
    case kBurstRise: Near(m[9], 0); break;
    // the orbit: the owner at its burst or not, each counter at its threshold
    case kOrbitBrighten: if (mh::Often()) m[9] = Byte(MH_PICK(0xD, 0xE, 0xF, 0x10)); break;
    case kOrbitCircle: case kOrbitSpin:
        if (mh::Half()) owner[2] = 3;
        Near(m[0xA], 0);
        break;
    case kOrbitFade:
        if (mh::Half()) owner[2] = 3;
        Near(m[9], 0);
        break;
    // the allocator: a pool full, or filled to a point
    case kAlloc:
        if (mh::Half()) {
            const unsigned n = mh::Half() ? kMotes : mh::Next() % kMotes;
            for (unsigned i = 0; i < n; ++i) Mote(i)[0] |= 1;
        }
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    std::memcpy(g_exe_shape, mh::Mem(kShape), kShapeBytes);
    g_regions[3].at = Key(g_prims);
    const mh::Group group = {
        "magic_s13", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::Run(group);
}

}  // namespace magic_s13
