// BOF3X_SHADOW=field_c2: group FC2's 44 functions through the scenario harness
// in field mode (scenario_harness.h, docs/scenario_harness.md section 7), once
// at start-up. docs/field_c2.md section 4. BOF3X_FC2_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group FC2 --clones (2026-09-29), each
// extent read again to its last instruction (capstone), names given, and the
// four starts the cut has no row for (0x46C730, 0x46C820, 0x46CEF0, 0x46D400)
// added with their call sites read by hand. Shapes: every state handler and
// helper without arguments kSprite (Sprite_Current one of the first four
// Sprite_Objects records; the state bytes each dispatcher reads seeded inside
// its table per function, sprite_span 0); the four with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c2.h"
#include "game/field_c2_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_c2 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group FC2 --clones, 2026-09-29; the four added by hand.
constexpr sh::CallSite kCalls46BBF0[] = {{0x14, 0x46C100}, {0x22, 0x57B830}, {0x95, 0x46C100}, {0x9C, 0x46BDF0}, {0xE4, 0x46C200}, {0xE9, 0x46C430}, {0xF3, 0x587740}, {0x10A, 0x46BD10}};
constexpr sh::CallSite kCalls46BD10[] = {{0x5C, 0x57C0F0}, {0x6A, 0x57C110}, {0x72, 0x46BF80}, {0x8A, 0x57B830}};
constexpr sh::CallSite kCalls46BDA0[] = {{0x2B, 0x57C0F0}, {0x33, 0x589840}, {0x38, 0x46C310}, {0x3D, 0x46C4B0}, {0x42, 0x57B830}};
constexpr sh::CallSite kCalls46BDF0[] = {{0xF, 0x531CF0}, {0x6C, 0x46BF40}, {0x8C, 0x5720C0}, {0x9F, 0x46BF40}, {0xC1, 0x5720C0}, {0xCE, 0x46BF40}, {0xEC, 0x5720C0}, {0xFC, 0x46BF40}, {0x118, 0x5720C0}};
constexpr sh::CallSite kCalls46BF40[] = {{0xA, 0x536700}};
constexpr sh::CallSite kCalls46BF80[] = {{0x12, 0x572620}, {0x26, 0x536700}, {0x4A, 0x579F00}, {0x6C, 0x572620}, {0x85, 0x536700}, {0xA7, 0x579F00}, {0xC9, 0x572620}, {0xE2, 0x536700}, {0x108, 0x579F00}, {0x133, 0x572620}, {0x14E, 0x536700}, {0x176, 0x579F00}};
constexpr sh::CallSite kCalls46C100[] = {{0x11, 0x572620}, {0x29, 0x579F00}, {0x4B, 0x572620}, {0x68, 0x579F00}, {0x8A, 0x572620}, {0xA7, 0x579F00}, {0xD2, 0x572620}, {0xF1, 0x579F00}};
constexpr sh::CallSite kCalls46C200[] = {{0xD1, 0x5A8C00}};
constexpr sh::CallSite kCalls46C310[] = {{0x35, 0x46C360}};
constexpr sh::CallSite kCalls46C360[] = {{0x7, 0x5A7B90}, {0x13, 0x5B93D2}, {0x20, 0x5B93D2}, {0x2E, 0x5B93D2}, {0x42, 0x5A8060}, {0x5A, 0x5A8E00}, {0x64, 0x5A8DE0}, {0x86, 0x5A8200}, {0xC0, 0x5A7BC0}};
constexpr sh::CallSite kCalls46C430[] = {{0x33, 0x5B93D2}, {0x44, 0x5B93D2}, {0x56, 0x5B93D2}, {0x63, 0x5A8B60}};
constexpr sh::CallSite kCalls46C4B0[] = {{0x3, 0x494060}, {0x24, 0x46C550}};
constexpr sh::CallSite kCalls46C550[] = {{0xC, 0x5A75D0}, {0x14, 0x5A7780}, {0x23, 0x494110}, {0x42, 0x4941E0}, {0x195, 0x5A79E0}, {0x1AC, 0x5A79A0}, {0x1CD, 0x572FA0}};
constexpr sh::CallSite kCalls46C770[] = {{0x4, 0x589810}, {0x64, 0x587740}, {0x8D, 0x589840}};
constexpr sh::CallSite kCalls46C810[] = {{0x9, 0x589840}};
constexpr sh::CallSite kCalls46C820[] = {{0x37, 0x589590}, {0x43, 0x589840}, {0x53, 0x5B93D2}, {0x98, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xCD, 0x5B93D2}, {0xE0, 0x5B93D2}, {0x115, 0x5891F0}};
constexpr sh::CallSite kCalls46C980[] = {{0x30, 0x5893A0}, {0x47, 0x588F00}, {0x61, 0x588F00}, {0x66, 0x589840}};
constexpr sh::CallSite kCalls46CA10[] = {{0x2, 0x589590}, {0x69, 0x5891F0}};
constexpr sh::CallSite kCalls46CA90[] = {{0x1D, 0x5893A0}, {0x2F, 0x5720C0}, {0x46, 0x589840}, {0x64, 0x588F00}, {0x69, 0x588F00}};
constexpr sh::CallSite kCalls46CB40[] = {{0x1D, 0x589810}, {0x87, 0x589840}};
constexpr sh::CallSite kCalls46CBD0[] = {{0x4, 0x589590}, {0x3D, 0x5B93D2}, {0x56, 0x5B93D2}, {0x7B, 0x5B93D2}, {0xAC, 0x5891F0}};
constexpr sh::CallSite kCalls46CCA0[] = {{0x0, 0x5893A0}, {0x2F, 0x5720C0}, {0x43, 0x589840}, {0x48, 0x588F00}};
constexpr sh::CallSite kCalls46CD10[] = {{0x2, 0x589590}, {0x65, 0x5891F0}};
constexpr sh::CallSite kCalls46CD90[] = {{0x1D, 0x5893A0}, {0x2F, 0x5720C0}, {0x43, 0x589840}, {0x48, 0x588F00}};
constexpr sh::CallSite kCalls46CE00[] = {{0x2, 0x589590}, {0xE, 0x589840}, {0x8B, 0x5891F0}};
constexpr sh::CallSite kCalls46CEA0[] = {{0x38, 0x589840}, {0x3D, 0x588F00}};
constexpr sh::CallSite kCalls46CFC0[] = {{0x55, 0x589840}, {0x5A, 0x46D180}, {0x63, 0x46D0E0}, {0x79, 0x5891F0}, {0x83, 0x587740}, {0xAD, 0x589840}};
constexpr sh::CallSite kCalls46D080[] = {{0x15, 0x589410}, {0x34, 0x588F00}, {0x39, 0x589410}, {0x4E, 0x588F00}, {0x53, 0x589840}};
constexpr sh::CallSite kCalls46D180[] = {{0x11, 0x5720C0}, {0x27, 0x531CF0}, {0xAC, 0x536700}, {0xCC, 0x536700}, {0xE7, 0x5728D0}, {0x10D, 0x536700}, {0x12A, 0x5728D0}, {0x150, 0x536700}, {0x16D, 0x5728D0}, {0x19C, 0x536700}, {0x1BF, 0x5728D0}, {0x1D1, 0x5B93D2}, {0x1F4, 0x5307C0}, {0x1FC, 0x589810}, {0x23F, 0x5720C0}};
constexpr sh::CallSite kCalls46D480[] = {{0x40, 0x46D5F0}};
constexpr sh::CallSite kCalls46D4E0[] = {{0x7D, 0x46D5F0}};
constexpr sh::CallSite kCalls46D570[] = {{0x6F, 0x46D5F0}, {0x78, 0x589840}};

#define FC2_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FC2_CALLS(a) a, FC2_N(a)
#define FC2_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite;
constexpr sh::Shape kCa = sh::Shape::kCall;
const sh::Clone kAll[] = {
    {"EffectKind30_Push", 0x46BBF0, 0x119, FC2_CALLS(kCalls46BBF0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_Push), 0, false, kSp},
    {"EffectKind30_Slide", 0x46BD10, 0x8F, FC2_CALLS(kCalls46BD10), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_Slide), 0, false, kSp},
    {"EffectKind30_Shatter", 0x46BDA0, 0x47, FC2_CALLS(kCalls46BDA0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_Shatter), 0, false, kSp},
    {"EffectKind30_WayBlocked", 0x46BDF0, 0x14B, FC2_CALLS(kCalls46BDF0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_WayBlocked), 0xFF, false, kCa},
    {"EffectKind30_CellSolid", 0x46BF40, 0x33, FC2_CALLS(kCalls46BF40), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_CellSolid), 0xFF, false, kCa},
    {"EffectKind30_ClaimCells", 0x46BF80, 0x180, FC2_CALLS(kCalls46BF80), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_ClaimCells), 0, false, kSp},
    {"EffectKind30_FreeCells", 0x46C100, 0xFA, FC2_CALLS(kCalls46C100), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_FreeCells), 0, false, kSp},
    {"EffectKind30_ShardsInit", 0x46C200, 0x110, FC2_CALLS(kCalls46C200), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_ShardsInit), 0, false, kSp},
    {"EffectKind30_ShardsStep", 0x46C310, 0x4A, FC2_CALLS(kCalls46C310), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_ShardsStep), 0, false, kSp},
    {"EffectKind30_ShardTumble", 0x46C360, 0xCD, FC2_CALLS(kCalls46C360), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_ShardTumble), 0, false, kCa},
    {"EffectKind30_SparksInit", 0x46C430, 0x7D, FC2_CALLS(kCalls46C430), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_SparksInit), 0, false, kSp},
    {"EffectKind30_SparksDraw", 0x46C4B0, 0x9C, FC2_CALLS(kCalls46C4B0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_SparksDraw), 0, false, kSp},
    {"EffectKind30_SparkQuad", 0x46C550, 0x1DB, FC2_CALLS(kCalls46C550), nullptr, 0, nullptr, 0, FC2_FN(EffectKind30_SparkQuad), 0, false, kCa},
    {"EffectKind34_Run", 0x46C730, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_Run), 0, false, kSp},
    {"EffectKind34_V0Run", 0x46C750, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V0Run), 0, false, kSp},
    {"EffectKind34_V0Burst", 0x46C770, 0x94, FC2_CALLS(kCalls46C770), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V0Burst), 0, false, kSp},
    {"EffectKind34_V0BurstEnd", 0x46C810, 0xE, FC2_CALLS(kCalls46C810), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V0BurstEnd), 0, false, kSp},
    {"EffectKind34_V0DebrisStart", 0x46C820, 0x154, FC2_CALLS(kCalls46C820), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V0DebrisStart), 0, false, kSp},
    {"EffectKind34_V0DebrisFly", 0x46C980, 0x6C, FC2_CALLS(kCalls46C980), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V0DebrisFly), 0, false, kSp},
    {"EffectKind34_V1Run", 0x46C9F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V1Run), 0, false, kSp},
    {"EffectKind34_V1Start", 0x46CA10, 0x7A, FC2_CALLS(kCalls46CA10), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V1Start), 0, false, kSp},
    {"EffectKind34_V1Fall", 0x46CA90, 0x6F, FC2_CALLS(kCalls46CA90), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V1Fall), 0, false, kSp},
    {"EffectKind34_V2Run", 0x46CB00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V2Run), 0, false, kSp},
    {"EffectKind34_V2Start", 0x46CB20, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V2Start), 0, false, kSp},
    {"EffectKind34_V2Spawn", 0x46CB40, 0x8E, FC2_CALLS(kCalls46CB40), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V2Spawn), 0, false, kSp},
    {"EffectKind34_V2PieceStart", 0x46CBD0, 0xC4, FC2_CALLS(kCalls46CBD0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V2PieceStart), 0, false, kSp},
    {"EffectKind34_V2PieceFall", 0x46CCA0, 0x4D, FC2_CALLS(kCalls46CCA0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V2PieceFall), 0, false, kSp},
    {"EffectKind34_V3Run", 0x46CCF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V3Run), 0, false, kSp},
    {"EffectKind34_V3Start", 0x46CD10, 0x76, FC2_CALLS(kCalls46CD10), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V3Start), 0, false, kSp},
    {"EffectKind34_V3Fall", 0x46CD90, 0x4D, FC2_CALLS(kCalls46CD90), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V3Fall), 0, false, kSp},
    {"EffectKind34_V4Run", 0x46CDE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V4Run), 0, false, kSp},
    {"EffectKind34_V4Start", 0x46CE00, 0x9C, FC2_CALLS(kCalls46CE00), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V4Start), 0, false, kSp},
    {"EffectKind34_V4Move", 0x46CEA0, 0x42, FC2_CALLS(kCalls46CEA0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind34_V4Move), 0, false, kSp},
    {"EffectKind3A_Run", 0x46CEF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_Run), 0, false, kSp},
    {"EffectKind3A_Start", 0x46CF10, 0xA5, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_Start), 0, false, kSp},
    {"EffectKind3A_Fly", 0x46CFC0, 0xB2, FC2_CALLS(kCalls46CFC0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_Fly), 0, false, kSp},
    {"EffectKind3A_Wait", 0x46D080, 0x59, FC2_CALLS(kCalls46D080), nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_Wait), 0, false, kSp},
    {"EffectKind3A_LeaderPose", 0x46D0E0, 0x98, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_LeaderPose), 0, false, kSp},
    {"EffectKind3A_Hit", 0x46D180, 0x274, FC2_CALLS(kCalls46D180), nullptr, 0, nullptr, 0, FC2_FN(EffectKind3A_Hit), 0xFF, false, kSp},
    {"EffectKind41_Run", 0x46D400, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind41_Run), 0, false, kSp},
    {"EffectKind41_Start", 0x46D420, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, FC2_FN(EffectKind41_Start), 0, false, kSp},
    {"EffectKind41_Hold", 0x46D480, 0x58, FC2_CALLS(kCalls46D480), nullptr, 0, nullptr, 0, FC2_FN(EffectKind41_Hold), 0, false, kSp},
    {"EffectKind41_Bounce", 0x46D4E0, 0x86, FC2_CALLS(kCalls46D4E0), nullptr, 0, nullptr, 0, FC2_FN(EffectKind41_Bounce), 0, false, kSp},
    {"EffectKind41_Fade", 0x46D570, 0x7D, FC2_CALLS(kCalls46D570), nullptr, 0, nullptr, 0, FC2_FN(EffectKind41_Fade), 0, false, kSp},
};
#undef FC2_FN
#undef FC2_CALLS
#undef FC2_N

enum : unsigned {
    kPush, kSlide, kShatter, kWayBlocked, kCellSolid, kClaim, kFree, kShardsInit, kShardsStep, kTumble, kSparksInit,
    kSparksDraw, kSparkQuad, k34Run, kV0Run, kV0Burst, kV0BurstEnd, kV0DebrisStart, kV0DebrisFly, kV1Run, kV1Start,
    kV1Fall, kV2Run, kV2Start, kV2Spawn, kV2PieceStart, kV2PieceFall, kV3Run, kV3Start, kV3Fall, kV4Run, kV4Start,
    kV4Move, k3ARun, k3AStart, k3AFly, k3AWait, k3APose, k3AHit, k41Run, k41Start, k41Hold, k41Bounce, k41Fade, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
// The same for an effect, by a draw of the recorders' stream (Noise): an
// effect never draws from Next(), whose stream the two passes do not share.
template <typename... T> U NoisePick(U draw, T... v) {
    const U values[] = {static_cast<U>(v)...};
    return values[draw % sizeof...(v)];
}

// The model the objects' +0x50 point at (read, copied and rebuilt), and the
// count bytes their +0x54 point at: buffers of the fuzz's own, compared.
alignas(16) unsigned char g_model[0x400];
alignas(16) unsigned char g_count[4];

// The ground AreaMap_Elevation's stand-in answers most of the time: drawn per
// round by the seed (both passes see the same), so the four corners of
// EffectKind30_WayBlocked often agree and the heights meet it exactly.
U g_ground;

// --- the effects -----------------------------------------------------------------------

// The caller's stack from just below this frame to its base (the TIB's
// StackBase): where the originals' and ours' locals are.
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
bool Writable(U at, unsigned n) { return sh::InRegions(P(at), n) || OnStack(P(at), n); }
void Fill(U at, unsigned n) {
    if (Writable(at, n)) sh::FillBytes(P(at), n);
}
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projection's screen words and depth are.
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// 0x494110: out[0..2] the projected x, y and depth (floats); the caller reads all three.
U FxProjectPoint(const U* a, U answer) {
    if (Writable(a[1], 12))
        for (unsigned i = 0; i < 3; ++i) FillFloat(P(a[1] + 4 * i));
    return answer;
}
// 0x4941E0: out's two s16.
U FxProjectSize(const U* a, U answer) {
    Fill(a[2], 4);
    return answer;
}
// Gte_RotMatrix: the nine s16 of the caller's MATRIX (its padding not), the matrix answered.
U FxRotMatrix(const U* a, U) {
    Fill(a[1], 18);
    return a[1];
}
// Gte_RotTrans: the three longs of out.
U FxRotTrans(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
// Gte_SetTransMatrix reads the MATRIX's translation, +0x14..+0x1F: noted.
U FxSetTrans(const U* a, U answer) {
    if (Writable(a[0], 0x20)) sh::NoteBytes(P(a[0] + 0x14), 12);
    return answer;
}
// AreaMap_ByteAt: the bytes its callers compare with, most of the time.
U FxByteAt(const U*, U answer) {
    const U n = sh::Noise();
    const U b = NoisePick(n >> 16, 0, 0xC0, 0x20, 0x80 | (n & 0xF), 0xA0 | (n & 0xF), 0x11, 0xFD, 0xFD, 0x10, n >> 8);
    return (answer & 0xFFFFFF00u) | (b & 0xFF);
}
// Sprite_ObjectAt: none (0xFF) half the time, else an object 0..0x1D or an extra
// 0x1E..0x21 - its whole range (docs/field-blocked.md), nothing past it.
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    return (answer & 0xFFFFFF00u) | ((n & 0x100) ? 0xFFu : (n >> 9) % 0x22u);
}
// AreaMap_Elevation: the round's ground two times in three, else a near or any height.
U FxElevation(const U*, U answer) {
    const U n = sh::Noise();
    const U h = n % 3 ? g_ground : NoisePick(n >> 16, g_ground + 1, g_ground - 1, 0, 0x100, n >> 8);
    return (answer & 0xFFFF0000u) | (h & 0xFFFF);
}

#define FC2_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {FC2_OURS(EffectKind30_FreeCells), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_ClaimCells), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_Slide), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_ShardsInit), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_ShardsStep), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_SparksInit), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind30_SparksDraw), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind3A_LeaderPose), 0, {}, kPh, 0, 0},
    {FC2_OURS(EffectKind3A_Hit), 0, {}, kF, 0, 0},
    {FC2_OURS(EffectKind30_WayBlocked), 2, {kW, kW}, kF, 0, 0},
    // x and z pushed as registers whose high words are the caller's leftovers;
    // AreaMap_ByteAt reads 16 bits of each
    {FC2_OURS(EffectKind30_CellSolid), 2, {k16, k16}, kF, 0, 0},
    {FC2_OURS(EffectKind30_ShardTumble), 2, {kW, kW}, kG, 0, 0},
    // the size read as a word (mov ax, [esp + 0x34]), the shade as a byte
    {FC2_OURS(EffectKind30_SparkQuad), 3, {kW, k16, k8}, kG, 0, 0},
    // nobody's, or another wave-two group's, by address
    {"0x494110", at::kProjectPoint, at::kProjectPoint, 2, {kW, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {"0x4941E0", at::kProjectSize, at::kProjectSize, 3, {kW, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
    // x, y read as s16 (movsx), the third argument never, the fourth's byte (and 0xFF)
    {"0x46D5F0", at::kDrawNumber, at::kDrawNumber, 4, {k16, k16, 0, k8}, kG, 0, 0},
    {"0x5728D0 (FE2)", at::kClearCell, at::kClearCell, 2, {k16, k16}, kG, 0, 0},
    {"0x5307C0 (FE1)", at::kZennyFind, at::kZennyFind, 1, {kW}, kG, 0, 0},
    // standard entries re-listed (docs/field_c2.md section 4): the answers the
    // callers compare, the bytes the callees read, the stack pointers unlogged
    {FC2_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    {FC2_OURS(AreaMap_SetHeight), 3, {k16, k16, k8}, kG, 0, 0},
    {FC2_OURS(AreaMap_SetByte), 3, {k16, k16, k8}, kG, 0, 0},
    {FC2_OURS(AreaMap_ByteAt), 2, {k16, k16}, kG, 0, 0, {}, &FxByteAt},
    {FC2_OURS(AreaMap_Elevation), 2, {kW, kW}, kG, 0, 0, {}, &FxElevation},
    {FC2_OURS(Gte_RotMatrix), 2, {kW, 0}, kG, 0, 0, {8, 0}, &FxRotMatrix, nullptr, true},
    {FC2_OURS(Gte_RotTrans), 2, {kW, 0}, kG, 0, 0, {8, 0}, &FxRotTrans, nullptr, true},
    {FC2_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &FxSetTrans, nullptr, true},
    {FC2_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}, nullptr, nullptr, true},
};
#undef FC2_OURS

// The tables the dispatchers jump through, read in place; lengths to the next
// table the code names (none of the eight is bounded by a compare).
const sh::DataTable kTables[] = {
    {0x653FF4, 6}, {0x65400C, 4}, {0x654028, 2}, {0x654030, 4}, {0x654040, 2}, {0x654048, 2}, {0x654050, 3}, {0x65405C, 4},
};

// Beyond field mode's standard regions: Game_Mode, the shards and sparks, the
// frame buffer the model is copied into, and the fuzz's model and counts.
const sh::Region kRegions[] = {
    {0, 4},                                 // Game_Mode (its word)
    {at::kShards, 0x644},
    {at::kModelCopy, 0x400},
    {0, sizeof g_model},
    {0, sizeof g_count},
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seed ------------------------------------------------------------------------

// What the dispatchers read: +1 (kinds) or +2 (variants) inside their table.
void State(unsigned char* s, unsigned at, unsigned entries) { s[at] = static_cast<unsigned char>(sh::Next() % entries); }

// Every one of the four records Sprite_Current may be moved among: the model
// and count pointers, the cell coordinates small (the tile word of
// EffectKind3A_Fly stays inside the area block), fractions 0 or not.
void Records() {
    for (unsigned r = 0; r < 4; ++r) {
        unsigned char* const s = sh::SpriteRecord(r);
        SetLong(s + 0x50, static_cast<std::int32_t>(sh::Half() ? Key(g_model) : at::kModelCopy));
        SetLong(s + 0x54, static_cast<std::int32_t>(Key(g_count + sh::Next() % 4)));
        SetWord(s + 0x34, PickOf(0, 0, 0x8000, sh::Next()));
        SetWord(s + 0x36, sh::Next() % 0x1E);
        SetWord(s + 0x38, PickOf(0, 0, 0x8000, sh::Next()));
        SetWord(s + 0x3A, sh::Next() % 0x1E);
        if (sh::Half()) {
            // a step inside a cell: the fly's tile read stays in the block
            SetLong(s + 0xC, static_cast<std::int32_t>(sh::Next() % 0x10000) - 0x8000);
            SetLong(s + 0x10, static_cast<std::int32_t>(sh::Next() % 0x10000) - 0x8000);
        }
    }
    for (unsigned i = 0; i < 4; ++i)
        g_count[i] = static_cast<unsigned char>(PickOf(0, 1, 0x18, 0x19, 0x80, 0xFF, sh::Next() % 0x1A, sh::Next() % 0x1A));
}

void Seed(unsigned k) {
    Records();
    unsigned char* const s = Sprite_Current;
    g_ground = PickOf(0, 0x40, 0x100, 0xFFC0, sh::Next() & 0xFFFF);
    Game_Mode = static_cast<unsigned short>(PickOf(1, 0, 2, sh::Next()));
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 3, 5, 2, sh::Next()));
    Game_AreaNumber = static_cast<unsigned short>(PickOf(0x54, 0x92, EffectKind34_V0Areas[sh::Next() % 9], sh::Next() % 0x100));
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xB, 0xC, 0xD, 0x10, sh::Next()));
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    // the height word against the ground: equal, one either side, 0x100 above and one either side
    SetWord(s + 0x3E, g_ground + PickOf(0, 1, 0xFFFF, 0x100, 0x101, 0xFF, 0x80, sh::Next()));
    SetLong(s + 0x14, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 0x40, sh::Next())));
    // the leader's height against the ground (EffectKind3A_Hit): 0x80 / 0x100 either side
    SetWord(Mem(at::kLeader + 0x3E), g_ground + PickOf(0, 0x80, 0x81, 0x7F, 0xFF00, 0xFEFF, 0xFF01, sh::Next()));
    if (sh::Half()) Mem(at::kLeader)[8] = static_cast<unsigned char>(sh::Next() % 8);
    sh::SetRandHint(PickOf(0xC, 0xD, 0xF, 0x1C, sh::Next()));
    switch (k) {
    case kSlide:
        // resting exactly at (0x118000, 0x4A8000) after the step
        if (sh::Half()) {
            SetLong(s + 0x34, static_cast<std::int32_t>(0x118000u - static_cast<U>(Long(s + 0xC))));
            SetLong(s + 0x38, static_cast<std::int32_t>(0x4A8000u - static_cast<U>(Long(s + 0x10))));
        }
        break;
    case kSparksDraw:
        for (unsigned r = 0; r < 4; ++r) sh::SpriteRecord(r)[9] = static_cast<unsigned char>(PickOf(0xB, 0xC, sh::Next()));
        break;
    case k34Run: State(s, 1, 6); break;
    case kV0Run:
    case kV2Run: State(s, 2, 4); break;
    case kV1Run:
    case kV3Run:
    case kV4Run: State(s, 2, 2); break;
    case k3ARun: State(s, 1, 3); break;
    case k41Run: State(s, 1, 4); break;
    case kV2Spawn: s[9] = static_cast<unsigned char>(PickOf(3, 3, 0, sh::Next())); break;
    case kV4Move: s[0] = static_cast<unsigned char>(sh::Half() ? s[0] | 0x80 : s[0] & 0x7F); break;
    case k3AFly: {
        // a step inside a cell, and the tile word at the cell it reaches 0 half the time
        SetLong(s + 0xC, static_cast<std::int32_t>(sh::Next() % 0x10000) - 0x8000);
        SetLong(s + 0x10, static_cast<std::int32_t>(sh::Next() % 0x10000) - 0x8000);
        const U x = static_cast<U>(Long(s + 0x34)) + static_cast<U>(Long(s + 0xC));
        const U z = static_cast<U>(Long(s + 0x38)) + static_cast<U>(Long(s + 0x10));
        const U width = AreaMap_Header[0];
        const U cell = static_cast<U>(static_cast<std::int32_t>(static_cast<short>(z >> 16))) * width +
                       static_cast<U>(static_cast<std::int32_t>(static_cast<short>(x >> 16))) +
                       Word(Mem(at::kAreaTileBase)) * 2u;
        if (cell * 2 < 0x2000 - 2) SetWord(AreaMap_Header + cell * 2, sh::Half() ? 0 : sh::Next() | 1);
        break;
    }
    case k41Hold:
    case k41Bounce:
    case k41Fade:
        s[0xB] = static_cast<unsigned char>(sh::Next() % 3);
        SetWord(s + 0x3A, PickOf(0, 0xFFFF, 0x7FFF, 0x8000, 0xFFF6, sh::Next()));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case kTumble:
        a[0] = at::kShards + (sh::Next() % at::kShardCount) * at::kShardStride;
        a[1] = Key(g_model) + (sh::Next() % at::kShardCount) * at::kFaceStride;
        break;
    case kSparkQuad: a[0] = at::kSparkRecords + (sh::Next() % at::kSparkCount) * at::kSparkStride; break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame and piece counters, the
// flag byte +0xB, the rise, the height, the sparks' shade and size, the leader's
// +0x27, Camera_Distance.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 8) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 0xC : v >> 1); break;
    case 1: s[0xA] = static_cast<unsigned char>(v % 3); break;
    case 2: s[0xB] = static_cast<unsigned char>(v % 3); break;
    case 3: SetLong(s + 0x14, static_cast<std::int32_t>(v)); break;
    case 4: SetWord(s + 0x3E, g_ground + (v & 0x1FF)); break;
    case 5:
        Mem(at::kSparkShade)[0] = static_cast<unsigned char>(v);
        SetWord(Mem(at::kSparkSize), v >> 8);
        break;
    case 6: Mem(at::kLeader)[0x27] = static_cast<unsigned char>(v); break;
    case 7: Camera_Distance = static_cast<short>(v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_FC2_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_FC2_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("field_c2: BOF3X_FC2_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[0].at = Key(&Game_Mode);
    g_regions[3].at = Key(g_model);
    g_regions[4].at = Key(g_count);
    sh::Group g = {"field_c2", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace field_c2
