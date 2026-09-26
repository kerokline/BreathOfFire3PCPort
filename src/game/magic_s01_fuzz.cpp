// BOF3X_SHADOW=magic_s01: group S01's overlay (MAGIC001, rows 1 and 105)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s01.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC001 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED
// line), names given. Beyond the standard set this group lists the CLUT
// helpers, the engine's 0x435A70, Math_Sin / Math_Cos / Math_Ratan2, the draw
// callees (the GTE and libgpu entry points, Gfx_CommitPrim) and its own two
// functions JumpChild_Run calls directly. Everything the harness lacks is
// built here, not in the harness:
//
//   - Gfx_CommitPrim logs each primitive's bytes (the shadow's sixteen
//     triangles are built one after another at Gfx_PacketNext, so the state
//     compare alone would see only the last) and moves Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real one does;
//   - the GTE callees of the shadow's matrix log what their pointers point at
//     (`deref`) and write a result where the real ones write;
//   - Sprite_ScriptTickOnce answers from its own stream, not the recorder's
//     flag (the kFlag blind spot, round9 doc section 9: a "no" came only with
//     no disturbance), and it and Sprite_UpdateScreen log which sprite they
//     act on.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s01.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s01 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC001 --clones, 2026-09-26, names given.
// 0x498FE0: 0x2E bytes  NueStomp_Task
constexpr mh::Imm kImms498FE0[] = {{0xF, 0x499010}, {0x17, 0x499750}, {0x22, 0x43FE80}};
// 0x499010: 0x157 bytes  NueStomp_Start
constexpr mh::CallSite kCalls499010[] = {{0x54, 0x435180}, {0xFE, 0x4FBF50}, {0x137, 0x4FBE30}};
// 0x499170: 0x12 bytes; +0xB jmp through .data 0x65A4F0 (a data_tables entry)  NueStompChild_Task
// 0x499190: 0x84 bytes  NueStompChild_Run
constexpr mh::CallSite kCalls499190[] = {{0x7B, 0x588F20}};
constexpr mh::Imm kImms499190[] = {{0xF, 0x499220}, {0x17, 0x499260}, {0x22, 0x4992B0}, {0x2A, 0x499330},
                                   {0x32, 0x4993B0}, {0x3A, 0x4993E0}, {0x42, 0x499440}, {0x4A, 0x499480},
                                   {0x52, 0x4994E0}, {0x5A, 0x499540}, {0x62, 0x4A0BA0}};
// 0x499220: 0x3E bytes  NueStompChild_Begin
constexpr mh::CallSite kCalls499220[] = {{0x8, 0x435A70}};
// 0x499260: 0x45 bytes  NueStompChild_Leap
constexpr mh::CallSite kCalls499260[] = {{0x0, 0x589410}, {0xD, 0x4FC030}, {0x1A, 0x435A70}};
// 0x4992B0: 0x7C bytes  NueStompChild_Rise
// 0x499330: 0x71 bytes  NueStompChild_Drop
constexpr mh::CallSite kCalls499330[] = {{0x3B, 0x587900}, {0x49, 0x435A70}, {0x57, 0x452F70}};
// 0x4993B0: 0x22 bytes  NueStompChild_Crouch
constexpr mh::CallSite kCalls4993B0[] = {{0x0, 0x589410}, {0x11, 0x435A70}};
// 0x4993E0: 0x58 bytes  NueStompChild_Bounce
constexpr mh::CallSite kCalls4993E0[] = {{0x0, 0x589410}, {0x5, 0x589410}, {0xA, 0x589410}, {0x17, 0x4FC030}, {0x24, 0x435A70}};
// 0x499440: 0x3A bytes  NueStompChild_Hop
// 0x499480: 0x56 bytes  NueStompChild_Stomp
constexpr mh::CallSite kCalls499480[] = {{0x0, 0x589410}, {0x5, 0x589410}, {0xA, 0x589410}, {0x18, 0x587900}, {0x25, 0x435A70}};
// 0x4994E0: 0x53 bytes  NueStompChild_Land
constexpr mh::CallSite kCalls4994E0[] = {{0x0, 0x589410}, {0xF, 0x4530D0}};
// 0x499540: 0x70 bytes  NueStompChild_Return
// 0x4995B0: 0x2E bytes  Jump_Task
constexpr mh::Imm kImms4995B0[] = {{0xF, 0x4995E0}, {0x17, 0x499750}, {0x22, 0x43FE80}};
// 0x4995E0: 0x16C bytes  Jump_Start
constexpr mh::CallSite kCalls4995E0[] = {{0x54, 0x4FB830}, {0x5D, 0x435180}, {0x108, 0x4FBF50}, {0x141, 0x4FBE30}, {0x14C, 0x4FBED0}};
// 0x499750: 0x26 bytes  Magic001_EndWhenChildDone
constexpr mh::CallSite kCalls499750[] = {{0xC, 0x4FC000}};
// 0x499780: 0x12 bytes; +0xB jmp through .data 0x65A4F4 (the same data_tables entry)  JumpChild_Task
// 0x4997A0: 0x7F bytes  JumpChild_Run
constexpr mh::CallSite kCalls4997A0[] = {{0x5B, 0x588F20}, {0x6C, 0x499B10}, {0x71, 0x499BD0}, {0x76, 0x5A7BC0}};
constexpr mh::Imm kImms4997A0[] = {{0xF, 0x499820}, {0x17, 0x499870}, {0x22, 0x499900}, {0x2A, 0x4999E0},
                                   {0x32, 0x4AE3C0}, {0x3A, 0x499A80}, {0x42, 0x4A0BA0}};
// 0x499820: 0x46 bytes  JumpChild_Begin
// 0x499870: 0x8E bytes  JumpChild_Rise
// 0x499900: 0xDE bytes  JumpChild_Hover
constexpr mh::CallSite kCalls499900[] = {{0x52, 0x4FC030}, {0x5C, 0x587900}, {0xAE, 0x5A7A70}, {0xC3, 0x4530D0}};
// 0x4999E0: 0x91 bytes  JumpChild_Return
constexpr mh::CallSite kCalls4999E0[] = {{0xD, 0x5A7A00}, {0x2A, 0x5A7A50}};
// 0x499A80: 0x82 bytes  JumpChild_Fade
// 0x499B10: 0xB8 bytes  JumpChild_PushMatrix
constexpr mh::CallSite kCalls499B10[] = {{0x3, 0x5A7B90}, {0x78, 0x5A8200}, {0x87, 0x5A8060}, {0x9B, 0x5A7D70}, {0xA5, 0x5A8DE0}, {0xAF, 0x5A8E00}};
// 0x499BD0: 0x1A5 bytes  JumpChild_DrawShadow
constexpr mh::CallSite kCalls499BD0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x4B, 0x5A7A00}, {0x64, 0x5A7A50},
                                         {0x8B, 0x5A75F0}, {0x92, 0x5A7780}, {0xC0, 0x5A7A00}, {0xD9, 0x5A7A50},
                                         {0x12B, 0x5A84A0}, {0x131, 0x5A9310}, {0x166, 0x461E50}, {0x18C, 0x5A77C0},
                                         {0x195, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S01_FN(name) reinterpret_cast<const void*>(&::name)
const mh::Clone kClones[] = {
    {"NueStomp_Task", 0x498FE0, 0x2E, nullptr, 0, kImms498FE0, MH_N(kImms498FE0), nullptr, 0, S01_FN(NueStomp_Task)},
    {"NueStomp_Start", 0x499010, 0x157, kCalls499010, MH_N(kCalls499010), nullptr, 0, nullptr, 0, S01_FN(NueStomp_Start)},
    {"NueStompChild_Task", 0x499170, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Task)},
    {"NueStompChild_Run", 0x499190, 0x84, kCalls499190, MH_N(kCalls499190), kImms499190, MH_N(kImms499190), nullptr, 0, S01_FN(NueStompChild_Run)},
    {"NueStompChild_Begin", 0x499220, 0x3E, kCalls499220, MH_N(kCalls499220), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Begin)},
    {"NueStompChild_Leap", 0x499260, 0x45, kCalls499260, MH_N(kCalls499260), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Leap)},
    {"NueStompChild_Rise", 0x4992B0, 0x7C, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Rise)},
    {"NueStompChild_Drop", 0x499330, 0x71, kCalls499330, MH_N(kCalls499330), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Drop)},
    {"NueStompChild_Crouch", 0x4993B0, 0x22, kCalls4993B0, MH_N(kCalls4993B0), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Crouch)},
    {"NueStompChild_Bounce", 0x4993E0, 0x58, kCalls4993E0, MH_N(kCalls4993E0), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Bounce)},
    {"NueStompChild_Hop", 0x499440, 0x3A, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Hop)},
    {"NueStompChild_Stomp", 0x499480, 0x56, kCalls499480, MH_N(kCalls499480), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Stomp)},
    {"NueStompChild_Land", 0x4994E0, 0x53, kCalls4994E0, MH_N(kCalls4994E0), nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Land)},
    {"NueStompChild_Return", 0x499540, 0x70, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(NueStompChild_Return)},
    {"Jump_Task", 0x4995B0, 0x2E, nullptr, 0, kImms4995B0, MH_N(kImms4995B0), nullptr, 0, S01_FN(Jump_Task)},
    {"Jump_Start", 0x4995E0, 0x16C, kCalls4995E0, MH_N(kCalls4995E0), nullptr, 0, nullptr, 0, S01_FN(Jump_Start)},
    {"Magic001_EndWhenChildDone", 0x499750, 0x26, kCalls499750, MH_N(kCalls499750), nullptr, 0, nullptr, 0, S01_FN(Magic001_EndWhenChildDone)},
    {"JumpChild_Task", 0x499780, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(JumpChild_Task)},
    {"JumpChild_Run", 0x4997A0, 0x7F, kCalls4997A0, MH_N(kCalls4997A0), kImms4997A0, MH_N(kImms4997A0), nullptr, 0, S01_FN(JumpChild_Run)},
    {"JumpChild_Begin", 0x499820, 0x46, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(JumpChild_Begin)},
    {"JumpChild_Rise", 0x499870, 0x8E, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(JumpChild_Rise)},
    {"JumpChild_Hover", 0x499900, 0xDE, kCalls499900, MH_N(kCalls499900), nullptr, 0, nullptr, 0, S01_FN(JumpChild_Hover)},
    {"JumpChild_Return", 0x4999E0, 0x91, kCalls4999E0, MH_N(kCalls4999E0), nullptr, 0, nullptr, 0, S01_FN(JumpChild_Return)},
    {"JumpChild_Fade", 0x499A80, 0x82, nullptr, 0, nullptr, 0, nullptr, 0, S01_FN(JumpChild_Fade)},
    {"JumpChild_PushMatrix", 0x499B10, 0xB8, kCalls499B10, MH_N(kCalls499B10), nullptr, 0, nullptr, 0, S01_FN(JumpChild_PushMatrix)},
    {"JumpChild_DrawShadow", 0x499BD0, 0x1A5, kCalls499BD0, MH_N(kCalls499BD0), nullptr, 0, nullptr, 0, S01_FN(JumpChild_DrawShadow)},
};
#undef S01_FN
#undef MH_N

enum : unsigned {
    kNueStomp_Task, kNueStomp_Start, kNueStompChild_Task, kNueStompChild_Run, kNueStompChild_Begin, kNueStompChild_Leap,
    kNueStompChild_Rise, kNueStompChild_Drop, kNueStompChild_Crouch, kNueStompChild_Bounce, kNueStompChild_Hop,
    kNueStompChild_Stomp, kNueStompChild_Land, kNueStompChild_Return,
    kJump_Task, kJump_Start, kMagic001_EndWhenChildDone, kJumpChild_Task, kJumpChild_Run, kJumpChild_Begin,
    kJumpChild_Rise, kJumpChild_Hover, kJumpChild_Return, kJumpChild_Fade, kJumpChild_PushMatrix, kJumpChild_DrawShadow,
    kCount
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

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim: the primitive at Gfx_PacketNext into the log (the real one
// links it), then Gfx_PacketNext on by its size, kept in the buffer.
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    const std::uint32_t size = a[1] & 0xFF;
    mh::NoteBytes(Gfx_PacketNext, size);
    unsigned char* p = Gfx_PacketNext + size;
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
    return answer;
}
// Sprite_UpdateScreen acts on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
// Sprite_ScriptTickOnce: which sprite, and an answer of its own stream (0 a
// third of the time), so a "no" also comes after a disturbance.
std::uint32_t TickAnswer(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? answer & 0xFFFFFF00u : (answer & 0xFFFFFF00u) | (1 + (h >> 8) % 0xFF);
}
// The GTE stand-ins of the shadow's matrix: a result from the inputs, written
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
// writes them (floats of the vertices), so the primitive logged at the commit
// carries them.
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
#define S01_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S01_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S01_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &TickAnswer},
    {S01_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSprite},
    // the effect library's CLUT helpers (group L)
    {S01_OURS(SpriteClut_CopyToFxRow), 1, {kAll}, kG, 0, 0},
    {S01_OURS(SpriteClut_SetStp), 1, {kAll}, kG, 0, 0},
    {S01_OURS(SpriteClut_ClearEntry31), 1, {kAll}, kG, 0, 0},
    {S01_OURS(SpriteClut_RestoreFxRow), 0, {}, kG, 0, 0},
    // the engine's, unnamed, in no group: the enemy of battle index a0's
    // animation a1 (0x93B960 + 0x128 (a0 - 3))
    {S01_RAW(0x435A70), 2, {kU8, kAll}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, the math): all ours
    {S01_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S01_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S01_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    {S01_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S01_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S01_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S01_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S01_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S01_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S01_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S01_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S01_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S01_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S01_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}, &Rtp3Effect},
    {S01_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    // this group's own, called directly by JumpChild_Run
    {S01_RAW(0x499B10), 0, {}, mh::Answer::kPhase, 0, 0},
    {S01_RAW(0x499BD0), 0, {}, mh::Answer::kPhase, 0, 0},
};
#undef S01_OURS
#undef S01_RAW

// The .data handler table both child tasks jmp through in place
// (Magic001_ChildTaskTable: NueStompChild_Task from its first entry,
// JumpChild_Task from its second).
const mh::DataTable kTables[] = {{0x65A4F0, 2}};

mh::Region g_regions[] = {
    {0x7E0670, 4},           // Gfx_PacketNext
    {0, kPrimBytes},         // g_prims (filled in at start-up)
    {kVertex, 0x20},         // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},        // DamageScratch: the shadow's radius words +0 / +2
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// The group's cells a recorder may move (the harness's case 14): the packet
// pointer, a vertex word, the radius words (kept small, the draw multiplies
// by them), the task fields the steps read back beyond the harness's own.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 6) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 2)), (h >> 16) % 0x100); break;
    case 3: {
        static const unsigned kFields[] = {9, 0xA, 0xB, 0x5D, 0x27};
        Sc()[kFields[v % 5]] = Byte(h >> 24);
        break;
    }
    case 4: {
        static const unsigned kFields[] = {0xC, 0x14, 0x20, 0x34, 0x38, 0x3C};
        SetLong(Sc() + kFields[v % 6], static_cast<std::int32_t>(h));
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

// The speed +0x14 one step (+0x20) from 0, or at it, or one past.
void NearStop(unsigned char* sc) {
    if (!mh::Often()) return;
    const std::int32_t step = static_cast<std::int32_t>(mh::Next() % 0x40) - 0x20;
    SetLong(sc + 0x20, step);
    SetLong(sc + 0x14, -step + static_cast<std::int32_t>(mh::Next() % 3) - 1);
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kNueStomp_Task: case kJump_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kNueStompChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kJumpChild_Task: sc[1] = 0; break;
    case kNueStompChild_Run: sc[2] = Byte(mh::Next() % 11); break;
    case kJumpChild_Run: sc[2] = Byte(mh::Next() % 7); break;
    // the child waits
    case kMagic001_EndWhenChildDone: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0 : 1); break;
    // the counters: at their thresholds
    case kNueStompChild_Rise: NearStop(sc); break;
    case kJumpChild_Rise:
        NearStop(sc);
        if (mh::Half()) sc[0xA] = Byte(mh::Next() % 2);
        break;
    case kNueStompChild_Drop: case kNueStompChild_Stomp: Near(sc[9], 1); break;
    case kNueStompChild_Hop: Near(sc[0xA], 1); break;
    case kJumpChild_Hover:
        Near(sc[9], 1);
        Near(sc[0xA], 7);
        break;
    case kJumpChild_Return:
        if (mh::Half()) sc[9] = Byte(mh::Half() ? 1 + mh::Next() % 2 : 7 + mh::Next() % 3);
        Near(sc[0xA], 0xF);
        break;
    case kNueStompChild_Return: case kJumpChild_Fade:
        if (mh::Often()) sc[0x5D] = Byte(MH_PICK(0x80, 0x90, 0x70, 0x81));
        if (k == kJumpChild_Fade && mh::Half()) sc[0xA] = Byte(mh::Next() % 3);
        break;
    // the matrix's height: the owner's below step 2, the source's after
    case kJumpChild_PushMatrix: sc[2] = Byte(1 + mh::Next() % 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    const mh::Group group = {
        "magic_s01", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::Run(group);
}

}  // namespace magic_s01
