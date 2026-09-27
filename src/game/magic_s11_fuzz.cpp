// BOF3X_SHADOW=magic_s11: group S11's two overlays (MAGIC058, MAGIC059)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s11.md section 4.
//
// The clone table is tools/magic_rows.py --unit MAGIC058 / MAGIC059 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// GTE and libgpu entry points, Math_Sin / Math_Cos, Gfx_CommitPrim,
// MapView_LinkPrimAt), Battle_ActorIsOut, the raw addresses of other units
// (MAGIC218's 0x4F5970, MAGIC219's 0x4F6290, the engine's 0x446770) and the
// functions of its own that its functions call directly. Everything the
// harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the GTE callees of the matrix push log what their pointers point at
//     (`deref`) and write a result where the real ones write;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what Tornado_Start reads back is compared;
//   - 0x4F5970 (the range test) answers a whole eax of 0 or 1 (its callers
//     test eax) and logs the radius word 0x903850 and Sprite_Current's point
//     it reads;
//   - Battle_ActorIsOut keeps one actor of each side in while
//     Tornado_AverageHeight is fuzzed (it divides by the count of those in).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s11.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s11 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC058 / MAGIC059 --clones, 2026-09-26, names
// given.
// MAGIC058
constexpr mh::CallSite kCalls4AF040[] = {{0x5B, 0x4AF4A0}};
constexpr mh::Imm kImms4AF040[] = {{0x16, 0x4AF0C0}, {0x1E, 0x4AF190}, {0x26, 0x4AF1C0}};
constexpr mh::CallSite kCalls4AF0C0[] = {{0x8E, 0x435180}, {0xC5, 0x587900}};
constexpr mh::CallSite kCalls4AF190[] = {{0xE, 0x497740}, {0x18, 0x44A880}};
constexpr mh::CallSite kCalls4AF1C0[] = {{0xF, 0x4530D0}, {0x1E, 0x4351F0}};
constexpr mh::CallSite kCalls4AF210[] = {{0x35, 0x4B7D40}, {0x3A, 0x4AF6A0}, {0x3F, 0x5A7BC0}};
constexpr mh::Imm kImms4AF210[] = {{0xF, 0x4AF260}, {0x17, 0x4AF2D0}, {0x22, 0x4AF490}};
constexpr mh::CallSite kCalls4AF2D0[] = {{0x47, 0x4456C0}, {0x58, 0x4F5970}, {0x77, 0x4AFB20},
                                         {0x109, 0x4456C0}, {0x116, 0x4F5970}, {0x12F, 0x4AFB20}};
constexpr mh::CallSite kCalls4AF490[] = {{0x8, 0x4351F0}};
constexpr mh::CallSite kCalls4AF4C0[] = {{0x3B, 0x4B7D40}, {0x40, 0x4AF8D0}, {0x45, 0x5A7BC0}};
constexpr mh::Imm kImms4AF4C0[] = {{0xF, 0x4AF510}, {0x17, 0x4AF620}, {0x22, 0x4AF650}};
constexpr mh::CallSite kCalls4AF650[] = {{0x47, 0x4F6290}};
constexpr mh::CallSite kCalls4AF6A0[] = {{0x2A, 0x5A7A00}, {0x43, 0x5A7A50}, {0x5C, 0x5A7A00}, {0x75, 0x5A7A50},
                                         {0x99, 0x5A77C0}, {0xA2, 0x461E50}, {0xB8, 0x5A7610}, {0xC0, 0x5A7780},
                                         {0xE9, 0x5A7A00}, {0x102, 0x5A7A50}, {0x143, 0x5A7A00}, {0x15C, 0x5A7A50},
                                         {0x1D3, 0x5A85F0}, {0x1DC, 0x5A9350}, {0x1E5, 0x461E50}, {0x209, 0x5A77C0},
                                         {0x212, 0x461E50}};
constexpr mh::CallSite kCalls4AF8D0[] = {{0x39, 0x5A7A00}, {0x52, 0x5A7A50}, {0x6B, 0x5A7A00}, {0x84, 0x5A7A50},
                                         {0xAC, 0x5A77C0}, {0xB5, 0x461E50}, {0xC9, 0x5A7610}, {0xD0, 0x5A7780},
                                         {0xF9, 0x5A7A00}, {0x112, 0x5A7A50}, {0x153, 0x5A7A00}, {0x16C, 0x5A7A50},
                                         {0x1FF, 0x5A85F0}, {0x208, 0x5A9350}, {0x211, 0x461E50}, {0x237, 0x5A77C0},
                                         {0x240, 0x461E50}};
// MAGIC059
constexpr mh::CallSite kCalls4AFB80[] = {{0x26, 0x4FBD10}, {0x58, 0x4B0800}};
constexpr mh::Imm kImms4AFB80[] = {{0x16, 0x4AFC00}, {0x1E, 0x4AFE70}};
constexpr mh::CallSite kCalls4AFC00[] = {{0x2D, 0x435180}, {0x6B, 0x4B0C50}, {0xCA, 0x4B0C50}, {0x129, 0x4B0C50},
                                         {0x1E3, 0x446770}, {0x20F, 0x5A7A00}, {0x227, 0x5A7A50}, {0x23C, 0x4B0CB0},
                                         {0x242, 0x587900}};
constexpr mh::CallSite kCalls4AFE70[] = {{0x24, 0x5A7A00}, {0x41, 0x5A7A50}, {0x6E, 0x4530D0}, {0x7D, 0x4351F0}};
constexpr mh::CallSite kCalls4AFF20[] = {{0x2B, 0x4B0170},  {0x41, 0x4B0250},  {0x5C, 0x4B0250},  {0x77, 0x4B0250},
                                         {0x92, 0x4B0250},  {0xAD, 0x4B0250},  {0xC8, 0x4B0250},  {0xE3, 0x4B0250},
                                         {0xFE, 0x4B0250},  {0x11E, 0x4B0250}, {0x13B, 0x4B0250}, {0x158, 0x4B0250},
                                         {0x175, 0x4B0250}, {0x192, 0x4B0250}, {0x1AF, 0x4B0250}, {0x1CC, 0x4B0250},
                                         {0x1E9, 0x4B0250}, {0x1F1, 0x5A7BC0}};
constexpr mh::CallSite kCalls4B0170[] = {{0x3, 0x5A7B90}, {0x94, 0x5A8200}, {0xA3, 0x5A8060},
                                         {0xB7, 0x5A7D70}, {0xC1, 0x5A8DE0}, {0xCB, 0x5A8E00}};
constexpr mh::CallSite kCalls4B0250[] = {
    {0x15, 0x5A77C0},  {0x2B, 0x572FA0},  {0x9D, 0x5A7A50},  {0xB2, 0x5A7A00},  {0xCE, 0x5A7A50},  {0xFF, 0x5A7A50},
    {0x114, 0x5A7A00}, {0x133, 0x5A7A50}, {0x161, 0x5A7A50}, {0x179, 0x5A7A00}, {0x198, 0x5A7A50}, {0x25D, 0x5A7A50},
    {0x272, 0x5A7A00}, {0x297, 0x5A7A50}, {0x2D7, 0x5A7A50}, {0x2EC, 0x5A7A00}, {0x311, 0x5A7A50}, {0x347, 0x5A7610},
    {0x34F, 0x5A7780}, {0x382, 0x5A85F0}, {0x38B, 0x5A9350}, {0x3ED, 0x572FA0}, {0x474, 0x5A7A50}, {0x489, 0x5A7A00},
    {0x4AE, 0x5A7A50}, {0x4E5, 0x5A7610}, {0x4ED, 0x5A7780}, {0x520, 0x5A85F0}, {0x529, 0x5A9350}, {0x58B, 0x572FA0}};
constexpr mh::CallSite kCalls4B0820[] = {{0x4B, 0x5A7A00}, {0x6E, 0x5A7A50}, {0x9C, 0x5A7A00},
                                         {0xD5, 0x5A7A50}, {0xE9, 0x4FBD10}, {0xEE, 0x4B0A00}};
constexpr mh::CallSite kCalls4B09C0[] = {{0x2E, 0x4F6290}};
constexpr mh::CallSite kCalls4B0A00[] = {{0x11, 0x5A77C0},  {0x27, 0x572FA0},  {0x33, 0x5A75D0}, {0x3B, 0x5A7780},
                                         {0x4E, 0x5A7A50},  {0x7C, 0x5A7A00},  {0xAA, 0x5A7A50}, {0xD8, 0x5A7A00},
                                         {0x109, 0x5A7A50}, {0x137, 0x5A7A00}, {0x165, 0x5A7A50}, {0x193, 0x5A7A00},
                                         {0x1C9, 0x5A79A0}, {0x1D8, 0x5A79E0}, {0x23E, 0x572FA0}};
constexpr mh::CallSite kCalls4B0CB0[] = {{0x1F, 0x4456C0}, {0x54, 0x4456C0}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S11_P(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
#define S11_K(name, base, size, calls) {#name, base, size, calls, MH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
#define S11_KI(name, base, size, calls, imms) {#name, base, size, calls, MH_N(calls), imms, MH_N(imms), nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
const mh::Clone kClones[] = {
    // MAGIC058
    S11_KI(Sanctuary_Task, 0x4AF040, 0x7D, kCalls4AF040, kImms4AF040),
    S11_K(Sanctuary_Start, 0x4AF0C0, 0xD0, kCalls4AF0C0),
    S11_K(Sanctuary_Message, 0x4AF190, 0x29, kCalls4AF190),
    S11_K(Sanctuary_End, 0x4AF1C0, 0x24, kCalls4AF1C0),
    S11_P(SanctuaryRing_Dispatch, 0x4AF1F0, 0x12),
    S11_KI(SanctuaryRing_Task, 0x4AF210, 0x48, kCalls4AF210, kImms4AF210),
    S11_P(SanctuaryRing_Start, 0x4AF260, 0x63),
    S11_K(SanctuaryRing_Spread, 0x4AF2D0, 0x1B2, kCalls4AF2D0),
    S11_K(MagicFx_UncountAndFree, 0x4AF490, 0xD, kCalls4AF490),
    S11_P(SanctuaryMote_Dispatch, 0x4AF4A0, 0x12),
    S11_KI(SanctuaryMote_Task, 0x4AF4C0, 0x4E, kCalls4AF4C0, kImms4AF4C0),
    S11_P(SanctuaryMote_Wait, 0x4AF510, 0x102),
    S11_P(SanctuaryMote_Slow, 0x4AF620, 0x2B),
    S11_K(SanctuaryMote_Fade, 0x4AF650, 0x4D, kCalls4AF650),
    S11_K(SanctuaryRing_Draw, 0x4AF6A0, 0x222, kCalls4AF6A0),
    S11_K(SanctuaryMote_Draw, 0x4AF8D0, 0x250, kCalls4AF8D0),
    {"SanctuaryMote_Alloc", 0x4AFB20, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SanctuaryMote_Alloc), 0xFF},
    // MAGIC059
    S11_KI(Tornado_Task, 0x4AFB80, 0x7A, kCalls4AFB80, kImms4AFB80),
    S11_K(Tornado_Start, 0x4AFC00, 0x26B, kCalls4AFC00),
    S11_K(Tornado_Spin, 0x4AFE70, 0x83, kCalls4AFE70),
    S11_P(TornadoFunnel_Dispatch, 0x4AFF00, 0x12),
    S11_K(TornadoFunnel_Task, 0x4AFF20, 0x1F7, kCalls4AFF20),
    S11_P(TornadoFunnel_Reset, 0x4B0120, 0x1D),
    S11_P(TornadoFunnel_Grow, 0x4B0140, 0x2A),
    S11_K(TornadoFunnel_PushMatrix, 0x4B0170, 0xD4, kCalls4B0170),
    S11_K(TornadoFunnel_DrawBand, 0x4B0250, 0x5A5, kCalls4B0250),
    S11_P(TornadoMote_Dispatch, 0x4B0800, 0x12),
    S11_K(TornadoMote_Task, 0x4B0820, 0xF5, kCalls4B0820),
    S11_P(TornadoMote_Place, 0x4B0920, 0x48),
    S11_P(TornadoMote_Rise, 0x4B0970, 0x29),
    S11_P(TornadoFx_Hold, 0x4B09A0, 0x1C),
    S11_K(TornadoMote_Fade, 0x4B09C0, 0x34, kCalls4B09C0),
    S11_K(TornadoMote_Draw, 0x4B0A00, 0x24A, kCalls4B0A00),
    {"TornadoMote_Alloc", 0x4B0C50, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TornadoMote_Alloc), 0xFF},
    S11_K(Tornado_AverageHeight, 0x4B0CB0, 0x93, kCalls4B0CB0),
};
#undef S11_P
#undef S11_K
#undef S11_KI
#undef MH_N

enum : unsigned {
    kSanctuary_Task, kSanctuary_Start, kSanctuary_Message, kSanctuary_End, kSanctuaryRing_Dispatch, kSanctuaryRing_Task,
    kSanctuaryRing_Start, kSanctuaryRing_Spread, kMagicFx_UncountAndFree, kSanctuaryMote_Dispatch, kSanctuaryMote_Task,
    kSanctuaryMote_Wait, kSanctuaryMote_Slow, kSanctuaryMote_Fade, kSanctuaryRing_Draw, kSanctuaryMote_Draw,
    kSanctuaryMote_Alloc,
    kTornado_Task, kTornado_Start, kTornado_Spin, kTornadoFunnel_Dispatch, kTornadoFunnel_Task, kTornadoFunnel_Reset,
    kTornadoFunnel_Grow, kTornadoFunnel_PushMatrix, kTornadoFunnel_DrawBand, kTornadoMote_Dispatch, kTornadoMote_Task,
    kTornadoMote_Place, kTornadoMote_Rise, kTornadoFx_Hold, kTornadoMote_Fade, kTornadoMote_Draw, kTornadoMote_Alloc,
    kTornado_AverageHeight, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kActorRecord = 0x904B3C, kKind2 = 0x905E60;
constexpr std::uint32_t kSanctuaryPool = 0x680780, kReached = 0x681A10, kTornadoPool = 0x681A20, kStride = 0x84;
constexpr unsigned kSanctuaryMotes = 36, kTornadoMotes = 24;

// Set by the seed for the one function it matters to.
bool g_keep_in = false;   // Battle_ActorIsOut keeps one actor of each side in (Tornado_AverageHeight)
unsigned g_keep_party, g_keep_enemy;

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x48 past it).
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[3]);
    return answer;
}
// 0x446770: the direction and pair it reads logged, a new pair written where
// the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}
// 0x4F5970: what it reads besides its argument - the radius word 0x903850 and
// Sprite_Current's point.
std::uint32_t NearEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Word(mh::Mem(kScratch)), static_cast<std::uint32_t>(Long(Sprite_Current + 0x34)),
             static_cast<std::uint32_t>(Long(Sprite_Current + 0x38)));
    return answer;
}
// Battle_ActorIsOut: the recorder's flag, except for the one actor of each
// side the seed keeps in while Tornado_AverageHeight is fuzzed.
std::uint32_t IsOutEffect(const std::uint32_t* a, std::uint32_t answer) {
    if (!g_keep_in) return answer;
    const unsigned actor = a[0] & 0xFF;
    return actor == g_keep_party || actor == g_keep_enemy ? answer & 0xFFFFFF00u : answer;
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

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kPh = mh::Answer::kPhase;
#define S11_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S11_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    {S11_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &IsOutEffect},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, battle_items:
    // all ours)
    {S11_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S11_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S11_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S11_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S11_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S11_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S11_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S11_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S11_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S11_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S11_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S11_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S11_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S11_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S11_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S11_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S11_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S11_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // other units', by address: MAGIC218's range test, MAGIC219's record free,
    // the engine's dx / dz turn
    {S11_RAW(0x4F5970), 1, {kAll}, mh::Answer::kBool, 0, 0, {}, &NearEffect},
    {S11_RAW(0x4F6290), 0, {}, kPh, 0, 0},
    {S11_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly: logged as the task they run for
    {S11_RAW(0x4AF4A0), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4AF6A0), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4AF8D0), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4B0800), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4B0170), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4B0A00), 0, {}, kPh, 0, 0},
    {S11_RAW(0x4B0CB0), 0, {}, kPh, 0, 0},
    // the funnel's band: its angle and radius
    {S11_RAW(0x4B0250), 2, {kAll, kAll}, kG, 0, 0},
    // the allocators: a record's index (their callers never test 0xFF, so the
    // recorders never answer past the pool)
    {S11_OURS(SanctuaryMote_Alloc), 0, {}, mh::Answer::kByte, 0, kSanctuaryMotes - 1},
    {S11_OURS(TornadoMote_Alloc), 0, {}, mh::Answer::kByte, 0, kTornadoMotes - 1},
};
#undef S11_OURS
#undef S11_RAW

// The .data handler tables the dispatchers read in place, adjacent ones as one
// run: SanctuaryRing_Types and SanctuaryMote_Types; TornadoFunnel_Types and
// _Phases; TornadoMote_Types and _Phases. Tornado_FacingPhase (0x65AA70) is
// not a handler table and stays.
const mh::DataTable kTables[] = {{0x65AA68, 2}, {0x65AA80, 5}, {0x65AA94, 5}};

mh::Region g_regions[] = {
    {0x7E0670, 4},                          // Gfx_PacketNext
    {0, kPrimBytes},                        // g_prims (filled in at start-up)
    {kVertex, 0x20},                        // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                       // 0x903850.., Scratch_Swap at +0xC
    {kKind2, 8},                            // Field_Kind2Z, Field_Kind2X
    {kSanctuaryPool, 0x1F00},               // SanctuaryMote_Pool, Sanctuary_Reached, TornadoMote_Pool
    {0x80E980, 0x200},                      // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                      // Gfx_ClutStrip row 26
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Record(std::uint32_t pool, unsigned i) { return mh::Mem(pool + i * kStride); }

// The group's cells a recorder may move: the packet pointer, a vertex or
// scratch word, the field point, the acting actor's record, a pool record's
// in-use bit (the pool runners test it after each call), a reached byte.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 5: {
        unsigned char* const r = Record(kSanctuaryPool, v % kSanctuaryMotes);
        r[0] = static_cast<unsigned char>(r[0] ^ 1);
        break;
    }
    case 6: {
        unsigned char* const r = Record(kTornadoPool, v % kTornadoMotes);
        r[0] = static_cast<unsigned char>(r[0] ^ 1);
        break;
    }
    case 7: mh::Mem(kReached + v % 11)[0] = Byte(h >> 24); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void NearLong(unsigned char* at, std::uint32_t before) {
    if (mh::Half()) SetLong(at, static_cast<std::int32_t>(before + (mh::Half() ? 1u : 0u)));
}
// A pool with its in-use bits random, or full to a random depth.
void PoolBits(std::uint32_t pool, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const r = Record(pool, i);
        r[0] = static_cast<unsigned char>(mh::Next() % 3 == 0 ? r[0] | 1 : r[0] & ~1u);
    }
}
void PoolDepth(std::uint32_t pool, unsigned n) {
    const unsigned used = mh::Next() % (n + 1);
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const r = Record(pool, i);
        if (i < used) r[0] = static_cast<unsigned char>(r[0] | 1);
        else if (i == used || mh::Half()) r[0] = static_cast<unsigned char>(r[0] & ~1u);
    }
}

unsigned char* SomeSprite() { return mh::Half() ? mh::TaskAt(mh::Next()) : mh::SpriteRecord(mh::Next()); }

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    // every pool record's owner a real sprite: the pool runners make it the
    // owner cell, which the disturbance writes through
    for (unsigned i = 0; i < kSanctuaryMotes; ++i) mh::SetPointer(kSanctuaryPool + i * kStride + 0x80, SomeSprite());
    for (unsigned i = 0; i < kTornadoMotes; ++i) mh::SetPointer(kTornadoPool + i * kStride + 0x80, SomeSprite());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    g_keep_in = k == kTornado_AverageHeight;
    g_keep_party = mh::Next() % 3;
    g_keep_enemy = 3 + mh::Next() % 8;
    if (mh::Often()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Next() % 11);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kSanctuary_Task: sc[1] = Byte(mh::Next() % 3); PoolBits(kSanctuaryPool, kSanctuaryMotes); break;
    case kTornado_Task: sc[1] = Byte(mh::Next() % 2); PoolBits(kTornadoPool, kTornadoMotes); break;
    case kSanctuaryRing_Dispatch: case kSanctuaryMote_Dispatch: case kTornadoFunnel_Dispatch: case kTornadoMote_Dispatch:
        sc[1] = 0;
        break;
    case kSanctuaryRing_Task: case kSanctuaryMote_Task: sc[2] = Byte(mh::Next() % 3); break;
    case kTornadoFunnel_Task: case kTornadoMote_Task: sc[2] = Byte(mh::Next() % 4); break;
    // the counters: at their thresholds
    case kSanctuary_Message: case kTornado_Spin: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0 : 1); break;
    case kSanctuary_End: if (mh::Half()) mh::Mem(mh::at::kMessageUp)[0] = 0; break;
    case kSanctuaryRing_Spread:
        NearLong(sc + 0xC, 0x5E0 - 1);
        for (unsigned i = 0; i < 11; ++i) {
            if (mh::Often()) mh::Mem(kReached + i)[0] = 0;
        }
        break;
    case kSanctuaryMote_Wait:
        Near(sc[9], 1);
        sc[0xB] = Byte(sc[0xB] % 11);
        break;
    case kSanctuaryMote_Slow: NearLong(sc + 0x18, 0x10); break;
    case kSanctuaryMote_Fade: case kTornadoMote_Fade: Near(sc[0xA], 1); break;
    case kTornadoFunnel_Grow: if (mh::Half()) sc[0xA] = Byte(0x1D + mh::Next() % 3); break;
    case kTornadoFx_Hold: Near(sc[9], 0x9E); break;
    case kTornadoMote_Rise: Near(sc[0xA], 0x1E); break;
    case kSanctuaryMote_Alloc: PoolDepth(kSanctuaryPool, kSanctuaryMotes); break;
    case kTornadoMote_Alloc: PoolDepth(kTornadoPool, kTornadoMotes); break;
    case kTornado_Start: if (mh::Often()) mh::Pointer(kActorRecord)[8] = Byte(mh::Next() % 4); break;
    case kTornado_AverageHeight:
        if (mh::Half()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Half() ? 2 : 3);
        break;
    default: break;
    }
}

// TornadoFunnel_DrawBand's angle and radius: its caller's most of the time.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kTornadoFunnel_DrawBand || !mh::Often()) return;
    a[0] = mh::Next() & 0x3F;
    a[1] = mh::Half() ? 0x20 : 0x50;
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s11", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s11
