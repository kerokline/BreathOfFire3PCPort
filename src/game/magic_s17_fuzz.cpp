// BOF3X_SHADOW=magic_s17: MAGIC075, MAGIC077 and MAGIC078 through the spell
// round's shared harness (magic_harness.h), once at start-up.
// docs/magic_s17.md section 3.
//
// The clone table is tools/magic_rows.py --unit MAGIC075 / 077 / 078
// --clones, renamed. Beyond the harness's standard set this group lists the
// callees its functions reach - the PSX library layer's GPU and GTE setters,
// the draws' sorts, its own functions another of its functions calls, and
// the raw addresses other groups own - and gives custom stand-ins to those
// whose arguments point at the caller's stack (they log what is pointed at),
// that write through a pointer the caller reads again (the GTE's outputs, a
// depth), that move Gfx_PacketNext as the real ones do, or that take more
// than four arguments. LeechShell_Edge takes arguments: Group::args gives it
// the seed's.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s17.h"
#include "game/move_script_bytes.h"

namespace magic_s17 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the group's own memory ------------------------------------------------------

constexpr unsigned kPrimBytes = 0x3000;
constexpr unsigned kPrimReserve = 0x700;   // the most a draw writes past Gfx_PacketNext (LeechShell_Draw's lines)
struct Own {
    unsigned char prims[kPrimBytes];
    std::uint32_t edge_v[8];   // LeechShell_Edge's four SVECTORs, by value
    long edge_out;
    std::uint32_t pad;
};
alignas(16) Own g_own;

// What the seed asks of the stand-ins: the phase bytes a function indexes a
// table with after a call (0: no limit), and where Math_Ratan2 lands.
unsigned g_limit1, g_limit2;
std::uint32_t g_ratan_hint;

void Settle() {
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const t = mh::TaskAt(k);
        if (g_limit1) t[1] = static_cast<unsigned char>(t[1] % g_limit1);
        if (g_limit2) t[2] = static_cast<unsigned char>(t[2] % g_limit2);
    }
}

// Gfx_PacketNext on by `size`, as the real commit and link do three times in
// four, while the draw still fits.
void Advance(unsigned size) {
    size &= 0xFF;
    unsigned char* const next = Gfx_PacketNext;
    if (next < g_own.prims || next + size + kPrimReserve > g_own.prims + kPrimBytes) return;
    if (mh::Noise() % 4 != 0) Gfx_PacketNext = next + size;
}
void Fill(void* at, unsigned bytes, std::uint32_t salt) {
    auto* b = static_cast<unsigned char*>(at);
    for (unsigned i = 0; i < bytes; ++i) b[i] = static_cast<unsigned char>((mh::Noise() ^ salt) >> ((i & 3) * 8));
}
std::uint32_t H6(const void* v) { return mh::HashBytes(v, 6); }

// --- the custom stand-ins -----------------------------------------------------------

constexpr std::uint32_t kSortPrims = 0x4FB880, kSeek = 0x4FB9F0, kNear = 0x4FBBD0, kMoteGlow = 0x4F1BD0,
                        kMoteDone = 0x4F6290;

void __cdecl RecDrawMode(unsigned char* prim, std::uint32_t dfe, std::uint32_t dtd, std::uint32_t tpage, std::uint32_t tw) {
    mh::Record(bof3::addr::Gpu_SetDrawMode, Key(prim), dfe, dtd, tpage ^ (tw * 0x9E3779B1u));
    mh::Stir();
}
void __cdecl RecCommit(std::uint32_t slot, std::uint32_t size) {
    mh::Record(bof3::addr::Gfx_CommitPrim, slot & 0xFF, size & 0xFF, 0, 0);
    Advance(size);
    mh::Stir();
}
void __cdecl RecLink(std::uint32_t x, std::uint32_t z, std::uint32_t dy, std::uint32_t size) {
    mh::Record(bof3::addr::MapView_LinkPrimAt, x, z, dy & 0xFF, size & 0xFF);
    Advance(size);
    mh::Stir();
}
std::uint32_t __cdecl RecSetTint(unsigned char* sprite, std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t a) {
    mh::Record(bof3::addr::Sprite_SetTint, Key(sprite), (r & 0xFF) | (g & 0xFF) << 8 | (b & 0xFF) << 16, a & 0xFF, 0);
    mh::Stir();
    return mh::Noise();
}
// Gte_RotAverage4 / Gte_RotTransPers4: the vertices by content, the outputs
// by address (they are the primitive's); x, y (and the depth for the first)
// written at each output, the depth cue to *p.
std::uint32_t Project4(std::uint32_t address, const short* v0, const short* v1, const short* v2, const short* v3, float* o0,
                       float* o1, float* o2, float* o3, long* p, unsigned out_bytes) {
    mh::Record(address, H6(v0) ^ Key(o0), H6(v1) ^ Key(o1), H6(v2) ^ Key(o2), H6(v3) ^ Key(o3));
    Fill(o0, out_bytes, 1);
    Fill(o1, out_bytes, 2);
    Fill(o2, out_bytes, 3);
    Fill(o3, out_bytes, 4);
    *p = static_cast<long>(mh::Noise() ^ 5);
    mh::Stir();
    return mh::Noise();
}
long __cdecl RecRotAverage4(const short* v0, const short* v1, const short* v2, const short* v3, float* o0, float* o1, float* o2,
                            float* o3, long* p) {
    return static_cast<long>(Project4(bof3::addr::Gte_RotAverage4, v0, v1, v2, v3, o0, o1, o2, o3, p, 12));
}
long __cdecl RecRotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3, float* o0, float* o1, float* o2,
                              float* o3, long* p) {
    return static_cast<long>(Project4(bof3::addr::Gte_RotTransPers4, v0, v1, v2, v3, o0, o1, o2, o3, p, 8));
}
void __cdecl RecRotTrans(const short* v, long* out) {
    mh::Record(bof3::addr::Gte_RotTrans, H6(v), 0, 0, 0);
    Fill(out, 12, 6);
    mh::Stir();
}
short* __cdecl RecRotMatrix(const short* angles, short* m) {
    mh::Record(bof3::addr::Gte_RotMatrix, H6(angles), 0, 0, 0);
    Fill(m, 18, 7);
    mh::Stir();
    return m;
}
short* __cdecl RecMulMatrix0(const short* a, const short* b, short* out) {
    mh::Record(bof3::addr::Gte_MulMatrix0, Key(a), mh::HashBytes(a, 18), mh::HashBytes(b, 18), 0);
    Fill(out, 18, 8);
    mh::Stir();
    return out;
}
void __cdecl RecSetRot(const unsigned long* m) {
    mh::Record(bof3::addr::Gte_SetRotMatrix, mh::HashBytes(m, 18), 0, 0, 0);
    mh::Stir();
}
void __cdecl RecSetTrans(const unsigned long* m) {
    mh::Record(bof3::addr::Gte_SetTransMatrix, mh::HashBytes(reinterpret_cast<const unsigned char*>(m) + 0x14, 12), 0, 0, 0);
    mh::Stir();
}
int __cdecl RecRatan2(std::uint32_t y, std::uint32_t x) {
    mh::Record(bof3::addr::Math_Ratan2, y, x, 0, 0);
    mh::Stir();
    const std::uint32_t h = mh::Noise();
    return static_cast<int>(h % 2 ? g_ratan_hint ^ (h & ~0xFC0u & 0x7F03Fu) : h);
}
void __cdecl RecSort(std::uint32_t x, std::uint32_t z, long* depths, unsigned char* prims, std::uint32_t count,
                     std::uint32_t stride, std::uint32_t dy) {
    mh::Record(kSortPrims, x, z, Key(prims) ^ mh::HashBytes(depths, 4 * (count & 0xFF)),
               (count & 0xFF) | (stride & 0xFF) << 8 | (dy & 0xFF) << 16);
    mh::Stir();
}
int __cdecl RecNear(unsigned char* target, std::uint32_t range) {
    mh::Record(kNear, Key(target), range, 0, 0);
    mh::Stir();
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? 0 : static_cast<int>(h | 1);
}
std::uint32_t __cdecl RecAlloc() {
    mh::Record(bof3::addr::ReviveMote_Alloc, 0, 0, 0, 0);
    mh::Stir();
    const std::uint32_t h = mh::Noise();
    return (h & 0xFFFFFF00u) | (h % 4 == 0 ? 0xFFu : (h >> 8) % 64);
}
void __cdecl RecMoteDispatch() {
    mh::Record(bof3::addr::ReviveMote_Dispatch, Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(mh::at::kOwner))),
               Sprite_Current[1], 0);
    mh::Stir();
}
void __cdecl RecEdge(unsigned char* prim, long* out, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
                     std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7) {
    const std::uint32_t v[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
    mh::Record(bof3::addr::LeechShell_Edge, Key(prim), H6(v) ^ (H6(v + 2) * 3), H6(v + 4) ^ (H6(v + 6) * 5), 0);
    *out = static_cast<long>(mh::Noise() ^ 9);
    mh::Stir();
}

// --- the callees beyond the standard set ---------------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define S17_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S17_RAW(text, address) text, address, address
const mh::Callee kCallees[] = {
    {S17_OURS(Sprite_SetTint), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecSetTint)},
    {S17_OURS(Tint_Release), 1, {kU8}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Sprite_SetAnimation), 1, {kU8}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Math_Ratan2), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecRatan2)},
    {S17_OURS(Gpu_SetDrawMode), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecDrawMode)},
    {S17_OURS(Gfx_CommitPrim), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecCommit)},
    {S17_OURS(MapView_LinkPrimAt), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecLink)},
    {S17_OURS(Gpu_SetPolyFT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_SetPolyF4), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_SetLineF4), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gte_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(Gte_RotTrans), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecRotTrans)},
    {S17_OURS(Gte_RotMatrix), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecRotMatrix)},
    {S17_OURS(Gte_MulMatrix0), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecMulMatrix0)},
    {S17_OURS(Gte_SetRotMatrix), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecSetRot)},
    {S17_OURS(Gte_SetTransMatrix), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecSetTrans)},
    {S17_OURS(Gte_RotAverage4), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecRotAverage4)},
    {S17_OURS(Gte_RotTransPers4), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecRotTransPers4)},
    {S17_OURS(Gte_PrimDepths4_0C), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    // this group's own, called by its others
    {S17_OURS(Revive_IsStrong), 0, {}, mh::Answer::kByte, 0, 1, {}, nullptr, nullptr},
    {S17_OURS(ReviveHalo_Draw), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(ReviveMote_Dispatch), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecMoteDispatch)},
    {S17_OURS(ReviveMote_Draw), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(ReviveMote_Alloc), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecAlloc)},
    {S17_OURS(LeechShell_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(LeechShell_Draw), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_OURS(LeechShell_Edge), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecEdge)},
    {S17_OURS(LeechOrb_DrawRings), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    // other groups' (docs/magic_s17.md section 5)
    {S17_RAW("0x4FB880", kSortPrims), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecSort)},
    {S17_RAW("0x4FB9F0", kSeek), 2, {kAll, kU16}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_RAW("0x4FBBD0", kNear), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&RecNear)},
    {S17_RAW("0x4F1BD0", kMoteGlow), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
    {S17_RAW("0x4F6290", kMoteDone), 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, nullptr},
};
#undef S17_OURS
#undef S17_RAW

// The .data handler tables, swapped for recorders while the fuzz runs. Each
// run of adjacent tables is listed as one (PurifyMote_Types and _Phases;
// ReviveHalo_Types and _Phases; ReviveMote_Types and LeechOrb_Types).
const mh::DataTable kDataTables[] = {{0x65B1B4, 3}, {0x65B1C0, 5}, {0x65B1D8, 3}, {0x65B1E4, 7}, {0x65B228, 6}};

// --- the clones (tools/magic_rows.py --unit MAGIC075 / 077 / 078 --clones, 2026-09-25) ---

// MAGIC075
constexpr mh::Imm kImms4BCBF0[] = {{0xF, 0x4BCC20}, {0x17, 0x4BCD10}, {0x22, 0x4BCD60}};
constexpr mh::CallSite kCalls4BCC20[] = {{0x4A, 0x435180}, {0x9C, 0x454DC0}, {0xAA, 0x454CC0}, {0xE3, 0x587900}};
constexpr mh::CallSite kCalls4BCD60[] = {{0x6A, 0x454D60}, {0x76, 0x4530D0}, {0x85, 0x4351F0}};
constexpr mh::CallSite kCalls4BCE10[] = {{0x2D, 0x588F20}};
constexpr mh::CallSite kCalls4BCE50[] = {{0x2D, 0x5A7A00}, {0x55, 0x5A7A50}, {0x11F, 0x5891F0}};
constexpr mh::CallSite kCalls4BCF90[] = {{0x0, 0x589410}, {0x11, 0x4351F0}};
// MAGIC077
constexpr mh::CallSite kCalls4BCFB0[] = {{0x73, 0x4BD5C0}};
constexpr mh::Imm kImms4BCFB0[] = {{0x16, 0x4BD050}, {0x1E, 0x4BD180}, {0x26, 0x4B1ED0}, {0x2E, 0x4BD1E0}, {0x36, 0x4BD200}, {0x3E, 0x4F7350}};
constexpr mh::CallSite kCalls4BD050[] = {{0x1D, 0x4BD280}, {0x86, 0x435180}, {0xC4, 0x587900}};
constexpr mh::CallSite kCalls4BD180[] = {{0x20, 0x454DC0}, {0x34, 0x454CC0}};
constexpr mh::CallSite kCalls4BD200[] = {{0x68, 0x454DC0}};
constexpr mh::CallSite kCalls4BD2D0[] = {{0x23, 0x4FBD10}, {0x28, 0x4BD420}};
constexpr mh::CallSite kCalls4BD380[] = {{0x30, 0x4BDA60}, {0x69, 0x5B93D2}};
constexpr mh::CallSite kCalls4BD420[] = {{0x18, 0x5A77C0}, {0x2E, 0x572FA0}, {0x7C, 0x5A75D0}, {0x84, 0x5A7780},
                                         {0xED, 0x5A79A0}, {0xFD, 0x5A79E0}, {0x183, 0x572FA0}};
constexpr mh::CallSite kCalls4BD5E0[] = {{0x39, 0x5A77C0}, {0x4F, 0x572FA0}, {0x68, 0x4FBD10}, {0x6D, 0x4BD8A0},
                                         {0x72, 0x4F1BD0}, {0x85, 0x5A77C0}, {0x9B, 0x572FA0}};
constexpr mh::Imm kImms4BD5E0[] = {{0xF, 0x4BD690}, {0x17, 0x4F1A40}, {0x22, 0x4BD7D0}};
constexpr mh::CallSite kCalls4BD690[] = {{0x29, 0x587900}, {0x5A, 0x5A7A00}, {0x7E, 0x5A7A50}, {0xB4, 0x5B93D2},
                                         {0xE0, 0x5B93D2}, {0xF2, 0x5B93D2}, {0x104, 0x5B93D2}};
constexpr mh::CallSite kCalls4BD7D0[] = {{0x2A, 0x5A7A00}, {0x4E, 0x5A7A50}, {0xC1, 0x4F6290}};
constexpr mh::CallSite kCalls4BD8A0[] = {{0x99, 0x5A75F0}, {0xA1, 0x5A7780}, {0xB5, 0x5A7A00}, {0xD5, 0x5A7A50},
                                         {0xFB, 0x5A7A00}, {0x11B, 0x5A7A50}, {0x199, 0x572FA0}};
// MAGIC078
constexpr mh::CallSite kCalls4BDAC0[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x4C, 0x5A77C0}, {0x55, 0x461E50}};
constexpr mh::Imm kImms4BDAC0[] = {{0x15, 0x4BDB20}, {0x1D, 0x4BDC10}};
constexpr mh::CallSite kCalls4BDB20[] = {{0xD, 0x435180}, {0x63, 0x435180}, {0xC2, 0x587900}};
constexpr mh::CallSite kCalls4BDC10[] = {{0x12, 0x4530D0}, {0x21, 0x4351F0}};
constexpr mh::CallSite kCalls4BDC60[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}, {0x64, 0x4BE5B0}, {0x69, 0x4BDE50},
                                         {0x6E, 0x5A7BC0}, {0x81, 0x5A77C0}, {0x8A, 0x461E50}};
constexpr mh::CallSite kCalls4BDD40[] = {{0x35, 0x5A7A70}};
constexpr mh::CallSite kCalls4BDE20[] = {{0x23, 0x4351F0}};
constexpr mh::CallSite kCalls4BDE50[] = {
    {0x1B, 0x5A77C0},  {0x31, 0x572FA0},  {0x83, 0x5A7A50},  {0x9C, 0x5A7A00},  {0xB2, 0x5A7A50},  {0xC8, 0x5A7A00},
    {0xDE, 0x5A7A00},  {0xF3, 0x5A7A50},  {0x10B, 0x5A7A00}, {0x121, 0x5A7A50}, {0x136, 0x5A7A00}, {0x14C, 0x5A7A00},
    {0x179, 0x5A75B0}, {0x181, 0x5A7780}, {0x1B2, 0x5A7A50}, {0x1CB, 0x5A7A00}, {0x1E0, 0x5A7A50}, {0x1F6, 0x5A7A00},
    {0x20B, 0x5A7A00}, {0x248, 0x5A7A50}, {0x260, 0x5A7A00}, {0x275, 0x5A7A50}, {0x28A, 0x5A7A00}, {0x29F, 0x5A7A00},
    {0x2F7, 0x4BE550}, {0x338, 0x5A8950}, {0x411, 0x5A75B0}, {0x418, 0x5A7780}, {0x420, 0x5A7A50}, {0x438, 0x5A7A00},
    {0x44D, 0x5A7A50}, {0x462, 0x5A7A00}, {0x477, 0x5A7A00}, {0x48C, 0x5A7A50}, {0x4A4, 0x5A7A00}, {0x4B9, 0x5A7A50},
    {0x4CE, 0x5A7A00}, {0x4E3, 0x5A7A00}, {0x4F8, 0x5A7A50}, {0x510, 0x5A7A00}, {0x525, 0x5A7A50}, {0x53D, 0x5A7A00},
    {0x552, 0x5A7A00}, {0x567, 0x5A7A50}, {0x57F, 0x5A7A00}, {0x594, 0x5A7A50}, {0x5A9, 0x5A7A00}, {0x5BE, 0x5A7A00},
    {0x5FE, 0x5A8950}, {0x6B4, 0x4FB880}, {0x6D9, 0x4FB880}};
constexpr mh::CallSite kCalls4BE550[] = {{0x7, 0x5A7690}, {0xF, 0x5A7780}, {0x4E, 0x5A8950}};
constexpr mh::CallSite kCalls4BE5B0[] = {{0x3, 0x5A7B90}, {0x6E, 0x5A8200}, {0x7D, 0x5A8060}, {0x91, 0x5A7D70},
                                         {0x9B, 0x5A8DE0}, {0xA5, 0x5A8E00}};
constexpr mh::CallSite kCalls4BE660[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}, {0x2C, 0x4B7D40}, {0x31, 0x4BE810},
                                         {0x36, 0x5A7BC0}, {0x5B, 0x5A77C0}, {0x64, 0x461E50}};
constexpr mh::CallSite kCalls4BE6F0[] = {{0x12, 0x587900}};
constexpr mh::CallSite kCalls4BE720[] = {{0x8, 0x4FB9F0}, {0x23, 0x4FBBD0}};
constexpr mh::CallSite kCalls4BE770[] = {{0x1E, 0x587900}};
constexpr mh::CallSite kCalls4BE7A0[] = {{0x12, 0x4FB9F0}, {0x23, 0x4FBBD0}};
constexpr mh::CallSite kCalls4BE7F0[] = {{0x18, 0x4351F0}};
constexpr mh::CallSite kCalls4BE810[] = {
    {0x2A, 0x5A7A50},  {0x3D, 0x5A7A00},  {0x50, 0x5A7A50},  {0x63, 0x5A7A50},  {0x76, 0x5A7A00},  {0x96, 0x5A7A50},
    {0xA9, 0x5A7A00},  {0xBC, 0x5A7A50},  {0xCF, 0x5A7A50},  {0xE2, 0x5A7A00},  {0x132, 0x5A7A50}, {0x144, 0x5A7A00},
    {0x157, 0x5A7A50}, {0x169, 0x5A7A50}, {0x17C, 0x5A7A00}, {0x1BA, 0x5A7A50}, {0x1CC, 0x5A7A00}, {0x1DF, 0x5A7A50},
    {0x1F1, 0x5A7A50}, {0x204, 0x5A7A00}, {0x24E, 0x5A77C0}, {0x259, 0x572FA0}, {0x268, 0x5A75B0}, {0x270, 0x5A7780},
    {0x2A3, 0x5A85F0}, {0x2A9, 0x5A9240}, {0x301, 0x572FA0}};


#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S17_P(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
#define S17_K(name, base, size, calls) {#name, base, size, calls, MH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
#define S17_I(name, base, size, imms) {#name, base, size, nullptr, 0, imms, MH_N(imms), nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
#define S17_KI(name, base, size, calls, imms) {#name, base, size, calls, MH_N(calls), imms, MH_N(imms), nullptr, 0, reinterpret_cast<const void*>(&::name), 0}
const mh::Clone kClones[] = {
    // MAGIC075
    S17_I(Purify_Task, 0x4BCBF0, 0x2E, kImms4BCBF0),
    S17_K(Purify_Start, 0x4BCC20, 0xEC, kCalls4BCC20),
    S17_P(Purify_Glow, 0x4BCD10, 0x4D),
    S17_K(Purify_Fade, 0x4BCD60, 0x8B, kCalls4BCD60),
    S17_P(PurifyMote_Dispatch, 0x4BCDF0, 0x12),
    S17_K(PurifyMote_Task, 0x4BCE10, 0x3D, kCalls4BCE10),
    S17_K(PurifyMote_Place, 0x4BCE50, 0x131, kCalls4BCE50),
    S17_K(PurifyMote_End, 0x4BCF90, 0x17, kCalls4BCF90),
    // MAGIC077
    S17_KI(Revive_Task, 0x4BCFB0, 0x95, kCalls4BCFB0, kImms4BCFB0),
    S17_K(Revive_Start, 0x4BD050, 0x12B, kCalls4BD050),
    S17_K(Revive_TintSource, 0x4BD180, 0x58, kCalls4BD180),
    S17_P(Revive_WaitMotes, 0x4BD1E0, 0x19),
    S17_K(Revive_Fade, 0x4BD200, 0x79, kCalls4BD200),
    {"Revive_IsStrong", 0x4BD280, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Revive_IsStrong), 0xFF},
    S17_P(ReviveHalo_Dispatch, 0x4BD2B0, 0x12),
    S17_K(ReviveHalo_Task, 0x4BD2D0, 0x2E, kCalls4BD2D0),
    S17_P(ReviveHalo_Wait, 0x4BD300, 0x7A),
    S17_K(ReviveHalo_Spawn, 0x4BD380, 0xA0, kCalls4BD380),
    S17_K(ReviveHalo_Draw, 0x4BD420, 0x193, kCalls4BD420),
    S17_P(ReviveMote_Dispatch, 0x4BD5C0, 0x12),
    S17_KI(ReviveMote_Task, 0x4BD5E0, 0xA4, kCalls4BD5E0, kImms4BD5E0),
    S17_K(ReviveMote_Launch, 0x4BD690, 0x132, kCalls4BD690),
    S17_K(ReviveMote_Rise, 0x4BD7D0, 0xC7, kCalls4BD7D0),
    S17_K(ReviveMote_Draw, 0x4BD8A0, 0x1B8, kCalls4BD8A0),
    {"ReviveMote_Alloc", 0x4BDA60, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ReviveMote_Alloc), 0xFF},
    // MAGIC078
    S17_KI(Leech_Task, 0x4BDAC0, 0x5E, kCalls4BDAC0, kImms4BDAC0),
    S17_K(Leech_Start, 0x4BDB20, 0xE4, kCalls4BDB20),
    S17_K(Leech_WaitOrbs, 0x4BDC10, 0x27, kCalls4BDC10),
    S17_P(LeechOrb_Dispatch, 0x4BDC40, 0x12),
    S17_K(LeechShell_Task, 0x4BDC60, 0x93, kCalls4BDC60),
    S17_P(LeechShell_Grow, 0x4BDD00, 0x20),
    S17_P(MagicFx_WaitA, 0x4BDD20, 0x1D),
    S17_K(LeechShell_Aim, 0x4BDD40, 0x6A, kCalls4BDD40),
    S17_P(LeechShell_Hold, 0x4BDDB0, 0x2C),
    S17_P(LeechShell_WaitOrb, 0x4BDDE0, 0x1E),
    S17_P(LeechShell_Fade, 0x4BDE00, 0x1D),
    S17_K(LeechShell_End, 0x4BDE20, 0x29, kCalls4BDE20),
    S17_K(LeechShell_Draw, 0x4BDE50, 0x6F6, kCalls4BDE50),
    {"LeechShell_Edge", 0x4BE550, 0x5F, kCalls4BE550, MH_N(kCalls4BE550), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::LeechShell_Edge), 0},
    S17_K(LeechShell_PushMatrix, 0x4BE5B0, 0xAE, kCalls4BE5B0),
    S17_K(LeechOrb_Task, 0x4BE660, 0x6D, kCalls4BE660),
    S17_P(LeechOrb_WaitShell, 0x4BE6D0, 0x1B),
    S17_K(LeechOrb_Chime, 0x4BE6F0, 0x29, kCalls4BE6F0),
    S17_K(LeechOrb_Rise, 0x4BE720, 0x42, kCalls4BE720),
    S17_K(LeechOrb_Pause, 0x4BE770, 0x2F, kCalls4BE770),
    S17_K(LeechOrb_Merge, 0x4BE7A0, 0x46, kCalls4BE7A0),
    S17_K(LeechOrb_End, 0x4BE7F0, 0x1E, kCalls4BE7F0),
    S17_K(LeechOrb_DrawRings, 0x4BE810, 0x336, kCalls4BE810),
};
#undef S17_P
#undef S17_K
#undef S17_I
#undef S17_KI
#undef MH_N
enum : unsigned {
    kPurifyTask, kPurifyStart, kPurifyGlow, kPurifyFade, kPMoteDispatch, kPMoteTask, kPMotePlace, kPMoteEnd,
    kReviveTask, kReviveStart, kReviveTint, kReviveWait, kReviveFade, kReviveStrong, kHaloDispatch, kHaloTask, kHaloWait,
    kHaloSpawn, kHaloDraw, kMoteDispatch, kMoteTask, kMoteLaunch, kMoteRise, kMoteDraw, kMoteAlloc,
    kLeechTask, kLeechStart, kLeechWait, kOrbDispatch, kShellTask, kShellGrow, kWaitA, kShellAim, kShellHold, kShellWaitOrb,
    kShellFade, kShellEnd, kShellDraw, kShellEdge, kShellPush, kOrbTask, kOrbWaitShell, kOrbChime, kOrbRise, kOrbPause,
    kOrbMerge, kOrbEnd, kOrbRings, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "the enum follows the clone table");

// --- the regions beyond the standard ones -------------------------------------------

const mh::Region kRegions[] = {
    {0x7E0670, 4},        // Gfx_PacketNext
    {0x7E0700, 0xC00},    // MoveScript_TintRecords, all 256
    {0x80B980, 0x20},     // CLUT row 2's source
    {0x80E980, 0x200},    // CLUT row 26's source
    {0x80F980, 0x20},     // CLUT row 2
    {0x812980, 0x200},    // CLUT row 26
    {0x9037A0, 0x20},     // Prim_VertexScratch
    {0x903850, 0x10},     // DamageScratch: the radius, the angle, the shade and the rgb words
    {0x9039D8, 4},        // the frame-offset table pointer
    {0x904B80, 4},        // the ability id
    {0x68C0B8, 0x2100},   // ReviveMote_Pool
    {0, sizeof(Own)},     // g_own (the address filled in at start-up)
};
mh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

unsigned char* SomeSprite() { return mh::Half() ? mh::TaskAt(mh::Next()) : mh::SpriteRecord(mh::Next()); }

void Seed(unsigned k) {
    // every round: the packet pointer inside the group's buffer; each of the
    // four slots' +0x4C (the orb's effect task) and every pool record's owner
    // a real sprite; no limits unless the function asks.
    Gfx_PacketNext = g_own.prims + (mh::Next() & 0xFC);
    for (unsigned s = 0; s < 4; ++s) mh::SetPointer(Key(mh::TaskAt(s) + 0x4C), SomeSprite());
    for (unsigned i = 0; i < 0x40; ++i) mh::SetPointer(0x68C0B8 + i * 0x84 + 0x80, SomeSprite());
    g_limit1 = g_limit2 = 0;
    g_ratan_hint = mh::Next();
    unsigned char* const sc = Sprite_Current;
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    switch (k) {
    case kPurifyTask: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kPurifyGlow:
        if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Half() ? 7 : mh::Next() % 9);
        break;
    case kPurifyFade:
        if (mh::Often()) mh::Mem(0x7E0702 + sc[0xA] * 12u)[0] = static_cast<unsigned char>(MH_PICK(1, 2, 3, 0));
        break;
    case kPMoteDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kPMoteTask: sc[2] = static_cast<unsigned char>(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kPMotePlace: if (mh::Often()) sc[9] = 1; break;
    case kReviveTask: {
        sc[1] = static_cast<unsigned char>(mh::Next() % 6);
        for (unsigned i = 0; i < 0x40; ++i) {
            unsigned char* const m = mh::Mem(0x68C0B8 + i * 0x84);
            m[0] = static_cast<unsigned char>(mh::Next() % 4 == 0 ? m[0] | 1 : m[0] & ~1u);
        }
        break;
    }
    case kReviveTint: case kHaloWait: case kMoteLaunch: case kShellEnd:
        if (mh::Often()) sc[9] = 1;
        if (k == kMoteLaunch && mh::Half()) sc[0xB] = 0;
        break;
    case kReviveWait: if (mh::Half()) sc[0xB] = 0; break;
    case kReviveFade: if (mh::Often()) sc[9] = 1; break;
    case kReviveStrong:
        if (mh::Often()) mh::Mem(0x904B35)[0] = 4;
        SetWord(mh::Mem(0x904B80), MH_PICK(0x4C, 0xB4, 0x4D, 0xB5, 0x14C, 0x1B4, 0x4C00, mh::Next()));
        break;
    case kHaloDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 5); break;
    case kHaloTask: sc[2] = static_cast<unsigned char>(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kHaloSpawn:
        if (mh::Often()) sc[9] = 0xF;
        sc[4] = static_cast<unsigned char>(mh::Next() % 4);
        break;
    case kHaloDraw: if (mh::Half()) sc[4] = 0; break;
    case kMoteDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kMoteTask: sc[2] = static_cast<unsigned char>(mh::Next() % 3); if (mh::Half()) sc[0] = 0; break;
    case kMoteRise: {
        const std::int32_t rise = static_cast<std::int32_t>(mh::Next()) >> (mh::Next() % 24);
        SetLong(sc + 0x14, rise);
        SetLong(sc + 0x20, mh::Half() ? rise + static_cast<std::int32_t>(MH_PICK(0, 1, static_cast<std::uint32_t>(-1))) : Long(sc + 0x20));
        if (mh::Often()) sc[9] = 1;
        break;
    }
    case kMoteAlloc: {
        const unsigned used = mh::Next() % 0x41;
        for (unsigned i = 0; i < 0x40; ++i) {
            unsigned char* const m = mh::Mem(0x68C0B8 + i * 0x84);
            if (i < used) m[0] = static_cast<unsigned char>(m[0] | 1);
            else if (i == used || mh::Half()) m[0] = static_cast<unsigned char>(m[0] & ~1u);
        }
        break;
    }
    case kLeechTask: sc[1] = static_cast<unsigned char>(mh::Next() % 2); g_limit1 = 2; break;
    case kLeechWait: if (mh::Half()) sc[0xB] = 0xFF; break;
    case kOrbDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kShellTask:
        g_limit2 = 7;
        sc[2] = static_cast<unsigned char>(mh::Next() % 7);
        if (mh::Half()) owner[0xB] = 0xFF;
        if (mh::Half()) Frame_Counter = (Frame_Counter & ~0xFFu) | (mh::Half() ? 0x40u : 0x3Fu);
        break;
    case kShellGrow: if (mh::Half()) sc[9] = 0xC; break;
    case kWaitA: case kShellHold: case kOrbPause: if (mh::Half()) sc[0xA] = 1; break;
    case kShellAim: {
        // the spin at the answer the aim wants, or one step either side
        const std::int32_t spin = static_cast<std::int32_t>(mh::Next() % 0x1000) - (mh::Half() ? 0x800 : 0);
        SetLong(sc + 0xC, spin);
        const std::int32_t want = (spin / 64) + static_cast<std::int32_t>(MH_PICK(0, 0, 1, static_cast<std::uint32_t>(-1)));
        g_ratan_hint = static_cast<std::uint32_t>(want & 0x3F) << 6;
        break;
    }
    case kShellWaitOrb: if (mh::Half()) owner[0xB] = 2; break;
    case kShellFade: sc[0xA] = static_cast<unsigned char>(MH_PICK(0x77, 0x78, 0x79, 0x80, 0xF8, mh::Next())); break;
    case kShellDraw: case kOrbRings:
        sc[2] = static_cast<unsigned char>(MH_PICK(3, 4, 5, 6, mh::Next()));
        if (mh::Half()) Frame_Counter = (Frame_Counter & ~0x1Cu) | ((1 + mh::Next() % 6) << 2);
        break;
    case kShellPush: if (mh::Half()) sc[5] = 0xD; break;
    case kOrbTask: g_limit2 = 6; sc[2] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kOrbWaitShell: if (mh::Half()) mh::Pointer(Key(sc + 0x4C))[0xB] = 1; break;
    case kOrbChime: if (mh::Half()) sc[9] = 8; break;
    case kOrbEnd: if (mh::Half()) sc[0xA] = 7; break;
    default: break;
    }
}

// A group cell moved after a call: the packet pointer, or a word of the
// vertex scratch or of the radius words.
// LeechShell_Edge's arguments: the packet pointer, its out cell and the four
// SVECTORs the seed put in g_own (every other clone ignores its words).
void Args(unsigned k, std::uint32_t* a);

void Disturb(std::uint32_t h) {
    switch ((h >> 8) % 3) {
    case 0: Gfx_PacketNext = g_own.prims + ((h >> 12) & 0xFFC); break;
    case 1: SetWord(mh::Mem(0x9037A0 + ((h >> 12) & 0x1E)), h >> 16); break;
    default: SetWord(mh::Mem(0x903850 + ((h >> 12) & 0xE)), h >> 16); break;
    }
}

void Args(unsigned k, std::uint32_t* a) {
    if (k != kShellEdge) return;
    a[0] = Key(Gfx_PacketNext);
    a[1] = Key(&g_own.edge_out);
    for (unsigned i = 0; i < 8; ++i) a[2 + i] = g_own.edge_v[i];
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[sizeof kRegions / sizeof kRegions[0] - 1].at = Key(&g_own);
    const mh::Group group = {
        "magic_s17", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kDataTables, sizeof kDataTables / sizeof kDataTables[0], g_regions, sizeof g_regions / sizeof g_regions[0],
        &Seed, &Disturb, 2000, &Settle, 0, &Args,
    };
    mh::Run(group);
}

}  // namespace magic_s17
